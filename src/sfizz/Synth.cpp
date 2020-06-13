// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "Synth.h"
#include "Config.h"
#include "Debug.h"
#include "Macros.h"
#include "MidiState.h"
#include "SisterVoiceRing.h"
#include "ScopedFTZ.h"
#include "StringViewHelpers.h"
#include "pugixml.hpp"
#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_replace.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>

sfz::Synth::Synth()
    : Synth(config::numVoices)
{
}

sfz::Synth::Synth(int numVoices)
{
    const std::lock_guard<std::mutex> disableCallback { callbackGuard };
    parser.setListener(this);
    effectFactory.registerStandardEffectTypes();
    effectBuses.reserve(5); // sufficient room for main and fx1-4
    resetVoices(numVoices);
}

sfz::Synth::~Synth()
{
    const std::lock_guard<std::mutex> disableCallback { callbackGuard };
    applyToAllVoices([](VoicePtr& voice){
        voice->reset();
    });
    resources.filePool.emptyFileLoadingQueues();
}

void sfz::Synth::onParseFullBlock(const std::string& header, const std::vector<Opcode>& members)
{
    const auto newRegionSet = [&](RegionSet* parentSet) {
        ASSERT(parentSet != nullptr);
        sets.emplace_back(new RegionSet);
        auto newSet = sets.back().get();
        parentSet->addSubset(newSet);
        newSet->setParent(parentSet);
        currentSet = newSet;
    };

    switch (hash(header)) {
    case hash("global"):
        globalOpcodes = members;
        currentSet = sets.front().get();
        lastHeader = Header::Global;
        groupOpcodes.clear();
        masterOpcodes.clear();
        handleGlobalOpcodes(members);
        break;
    case hash("control"):
        defaultPath = ""; // Always reset on a new control header
        handleControlOpcodes(members);
        break;
    case hash("master"):
        masterOpcodes = members;
        newRegionSet(sets.front().get());
        groupOpcodes.clear();
        lastHeader = Header::Master;
        handleMasterOpcodes(members);
        numMasters++;
        break;
    case hash("group"):
        groupOpcodes = members;
        if (lastHeader == Header::Group)
            newRegionSet(currentSet->getParent());
        else
            newRegionSet(currentSet);
        lastHeader = Header::Group;
        handleGroupOpcodes(members, masterOpcodes);
        numGroups++;
        break;
    case hash("region"):
        buildRegion(members);
        break;
    case hash("curve"):
        resources.curves.addCurveFromHeader(members);
        break;
    case hash("effect"):
        handleEffectOpcodes(members);
        break;
    default:
        std::cerr << "Unknown header: " << header << '\n';
    }
}

void sfz::Synth::onParseError(const SourceRange& range, const std::string& message)
{
    const auto relativePath = range.start.filePath->lexically_relative(parser.originalDirectory());
    std::cerr << "Parse error in " << relativePath << " at line " << range.start.lineNumber + 1 << ": " << message << '\n';
}

void sfz::Synth::onParseWarning(const SourceRange& range, const std::string& message)
{
    const auto relativePath = range.start.filePath->lexically_relative(parser.originalDirectory());
    std::cerr << "Parse warning in " << relativePath << " at line " << range.start.lineNumber + 1 << ": " << message << '\n';
}

void sfz::Synth::buildRegion(const std::vector<Opcode>& regionOpcodes)
{
    ASSERT(currentSet != nullptr);
    auto lastRegion = absl::make_unique<Region>(resources.midiState, defaultPath);

    auto parseOpcodes = [&](const std::vector<Opcode>& opcodes) {
        for (auto& opcode : opcodes) {
            const auto unknown = absl::c_find_if(unknownOpcodes, [&](absl::string_view sv) { return sv.compare(opcode.opcode) == 0; });
            if (unknown != unknownOpcodes.end()) {
                continue;
            }

            if (!lastRegion->parseOpcode(opcode))
                unknownOpcodes.emplace_back(opcode.opcode);
        }
    };

    parseOpcodes(globalOpcodes);
    parseOpcodes(masterOpcodes);
    parseOpcodes(groupOpcodes);
    parseOpcodes(regionOpcodes);

    if (octaveOffset != 0 || noteOffset != 0)
        lastRegion->offsetAllKeys(octaveOffset * 12 + noteOffset);

    // There was a combination of group= and polyphony= on a region, so set the group polyphony
    if (lastRegion->group != Default::group && lastRegion->polyphony != config::maxVoices)
        setGroupPolyphony(lastRegion->group, lastRegion->polyphony);

    lastRegion->parent = currentSet;
    currentSet->addRegion(lastRegion.get());
    regions.push_back(std::move(lastRegion));
}

void sfz::Synth::clear()
{
    const std::lock_guard<std::mutex> disableCallback { callbackGuard };

    applyToAllVoices([](VoicePtr& voice){
        voice->reset();
    });
    for (auto& list : noteActivationLists)
        list.clear();
    for (auto& list : ccActivationLists)
        list.clear();

    sets.clear();
    sets.emplace_back(new RegionSet);
    currentSet = sets.front().get();
    regions.clear();
    effectBuses.clear();
    effectBuses.emplace_back(new EffectBus);
    effectBuses[0]->setGainToMain(1.0);
    effectBuses[0]->setSamplesPerBlock(samplesPerBlock);
    effectBuses[0]->setSampleRate(sampleRate);
    effectBuses[0]->clearInputs(samplesPerBlock);
    resources.clear();
    numGroups = 0;
    numMasters = 0;
    defaultSwitch = absl::nullopt;
    defaultPath = "";
    resources.midiState.reset();
    ccLabels.clear();
    keyLabels.clear();
    keyswitchLabels.clear();
    globalOpcodes.clear();
    masterOpcodes.clear();
    groupOpcodes.clear();
    unknownOpcodes.clear();
    polyphonyGroups.clear();
    polyphonyGroups.emplace_back();
    polyphonyGroups.back().setPolyphonyLimit(config::maxVoices);
    modificationTime = fs::file_time_type::min();
}

void sfz::Synth::handleMasterOpcodes(const std::vector<Opcode>& members)
{
    for (auto& rawMember : members) {
        const Opcode member = rawMember.cleanUp(kOpcodeScopeGlobal);

        switch (member.lettersOnlyHash) {
        case hash("polyphony"):
            ASSERT(currentSet != nullptr);
            if (auto value = readOpcode(member.value, Default::polyphonyRange))
                currentSet->setPolyphonyLimit(*value);
            break;
        }
    }
}

void sfz::Synth::handleGlobalOpcodes(const std::vector<Opcode>& members)
{
    for (auto& rawMember : members) {
        const Opcode member = rawMember.cleanUp(kOpcodeScopeGlobal);

        switch (member.lettersOnlyHash) {
        case hash("polyphony"):
            ASSERT(currentSet != nullptr);
            if (auto value = readOpcode(member.value, Default::polyphonyRange))
                currentSet->setPolyphonyLimit(*value);
            break;
        case hash("sw_default"):
            setValueFromOpcode(member, defaultSwitch, Default::keyRange);
            break;
        case hash("volume"):
            // FIXME : Probably best not to mess with this and let the host control the volume
            // setValueFromOpcode(member, volume, Default::volumeRange);
            break;
        }
    }
}

void sfz::Synth::handleGroupOpcodes(const std::vector<Opcode>& members, const std::vector<Opcode>& masterMembers)
{
    absl::optional<unsigned> groupIdx;
    absl::optional<unsigned> maxPolyphony;

    const auto parseOpcode = [&](const Opcode& rawMember) {
        const Opcode member = rawMember.cleanUp(kOpcodeScopeGroup);

        switch (member.lettersOnlyHash) {
        case hash("group"):
            setValueFromOpcode(member, groupIdx, Default::groupRange);
            break;
        case hash("polyphony"):
            setValueFromOpcode(member, maxPolyphony, Default::groupRange);
            break;
        }
    };

    for (auto& member : masterMembers)
        parseOpcode(member);

    for (auto& member : members)
        parseOpcode(member);

    if (groupIdx && maxPolyphony) {
        setGroupPolyphony(*groupIdx, *maxPolyphony);
    } else if (maxPolyphony) {
        ASSERT(currentSet != nullptr);
        currentSet->setPolyphonyLimit(*maxPolyphony);
    } else if (groupIdx && *groupIdx > polyphonyGroups.size()) {
        setGroupPolyphony(*groupIdx, config::maxVoices);
    }
}

void sfz::Synth::handleControlOpcodes(const std::vector<Opcode>& members)
{
    for (auto& rawMember : members) {
        const Opcode member = rawMember.cleanUp(kOpcodeScopeControl);

        switch (member.lettersOnlyHash) {
        case hash("set_cc&"):
            if (Default::ccNumberRange.containsWithEnd(member.parameters.back())) {
                const auto ccValue = readOpcode(member.value, Default::midi7Range);
                if (ccValue)
                    resources.midiState.ccEvent(0, member.parameters.back(), normalizeCC(*ccValue));
            }
            break;
        case hash("set_hdcc&"):
            if (Default::ccNumberRange.containsWithEnd(member.parameters.back())) {
                const auto ccValue = readOpcode(member.value, Default::normalizedRange);
                if (ccValue)
                    resources.midiState.ccEvent(0, member.parameters.back(), *ccValue);
            }
            break;
        case hash("label_cc&"):
            if (Default::ccNumberRange.containsWithEnd(member.parameters.back()))
                ccLabels.emplace_back(member.parameters.back(), std::string(member.value));
            break;
        case hash("label_key&"):
            if (Default::keyRange.containsWithEnd(member.parameters.back()))
                keyLabels.emplace_back(member.parameters.back(), std::string(member.value));
            break;
        case hash("default_path"):
            defaultPath = absl::StrReplaceAll(trim(member.value), { { "\\", "/" } });
            DBG("Changing default sample path to " << defaultPath);
            break;
        case hash("note_offset"):
            setValueFromOpcode(member, noteOffset, Default::noteOffsetRange);
            break;
        case hash("octave_offset"):
            setValueFromOpcode(member, octaveOffset, Default::octaveOffsetRange);
            break;
        default:
            // Unsupported control opcode
            DBG("Unsupported control opcode: " << member.opcode);
        }
    }
}

void sfz::Synth::handleEffectOpcodes(const std::vector<Opcode>& rawMembers)
{
    absl::string_view busName = "main";

    auto getOrCreateBus = [this](unsigned index) -> EffectBus& {
        if (index + 1 > effectBuses.size())
            effectBuses.resize(index + 1);
        EffectBusPtr& bus = effectBuses[index];
        if (!bus) {
            bus.reset(new EffectBus);
            bus->setSampleRate(sampleRate);
            bus->setSamplesPerBlock(samplesPerBlock);
            bus->clearInputs(samplesPerBlock);
        }
        return *bus;
    };

    std::vector<Opcode> members;
    members.reserve(rawMembers.size());
    for (const Opcode& opcode : rawMembers)
        members.push_back(opcode.cleanUp(kOpcodeScopeEffect));

    for (const Opcode& opcode : members) {
        switch (opcode.lettersOnlyHash) {
        case hash("bus"):
            busName = opcode.value;
            break;

            // note(jpc): gain opcodes are linear volumes in % units

        case hash("directtomain"):
            if (auto valueOpt = readOpcode<float>(opcode.value, { 0, 100 }))
                getOrCreateBus(0).setGainToMain(*valueOpt / 100);
            break;

        case hash("fx&tomain"): // fx&tomain
            if (opcode.parameters.front() < 1 || opcode.parameters.front() > config::maxEffectBuses)
                break;
            if (auto valueOpt = readOpcode<float>(opcode.value, { 0, 100 }))
                getOrCreateBus(opcode.parameters.front()).setGainToMain(*valueOpt / 100);
            break;

        case hash("fx&tomix"): // fx&tomix
            if (opcode.parameters.front() < 1 || opcode.parameters.front() > config::maxEffectBuses)
                break;
            if (auto valueOpt = readOpcode<float>(opcode.value, { 0, 100 }))
                getOrCreateBus(opcode.parameters.front()).setGainToMix(*valueOpt / 100);
            break;
        }
    }

    unsigned busIndex;
    if (busName.empty() || busName == "main")
        busIndex = 0;
    else if (busName.size() > 2 && busName.substr(0, 2) == "fx" && absl::SimpleAtoi(busName.substr(2), &busIndex) && busIndex >= 1 && busIndex <= config::maxEffectBuses) {
        // an effect bus fxN, with N usually in [1,4]
    } else {
        DBG("Unsupported effect bus: " << busName);
        return;
    }

    // create the effect and add it
    EffectBus& bus = getOrCreateBus(busIndex);
    auto fx = effectFactory.makeEffect(members);
    fx->setSampleRate(sampleRate);
    fx->setSamplesPerBlock(samplesPerBlock);
    bus.addEffect(std::move(fx));
}

bool sfz::Synth::loadSfzFile(const fs::path& file)
{
    clear();

    const std::lock_guard<std::mutex> disableCallback { callbackGuard };
    parser.parseFile(file);
    if (parser.getErrorCount() > 0)
        return false;

    if (regions.empty())
        return false;

    finalizeSfzLoad();

    return true;
}

bool sfz::Synth::loadSfzString(const fs::path& path, absl::string_view text)
{
    clear();

    const std::lock_guard<std::mutex> disableCallback { callbackGuard };
    parser.parseString(path, text);
    if (parser.getErrorCount() > 0)
        return false;

    if (regions.empty())
        return false;

    finalizeSfzLoad();

    return true;
}

void sfz::Synth::finalizeSfzLoad()
{
    resources.filePool.setRootDirectory(parser.originalDirectory());

    auto currentRegion = regions.begin();
    auto lastRegion = regions.rbegin();
    auto removeCurrentRegion = [&currentRegion, &lastRegion]() {
        if (currentRegion->get() == nullptr)
            return;

        DBG("Removing the region with sample " << currentRegion->get()->sampleId);
        std::iter_swap(currentRegion, lastRegion);
        ++lastRegion;
    };

    size_t maxFilters { 0 };
    size_t maxEQs { 0 };

    while (currentRegion < lastRegion.base()) {
        auto region = currentRegion->get();

        if (!region->oscillator && !region->isGenerator()) {
            if (!resources.filePool.checkSampleId(region->sampleId)) {
                removeCurrentRegion();
                continue;
            }

            const auto fileInformation = resources.filePool.getFileInformation(region->sampleId);
            if (!fileInformation) {
                removeCurrentRegion();
                continue;
            }

            region->sampleEnd = std::min(region->sampleEnd, fileInformation->end);

            if (fileInformation->loopBegin != Default::loopRange.getStart() && fileInformation->loopEnd != Default::loopRange.getEnd()) {
                if (region->loopRange.getStart() == Default::loopRange.getStart())
                    region->loopRange.setStart(fileInformation->loopBegin);

                if (region->loopRange.getEnd() == Default::loopRange.getEnd())
                    region->loopRange.setEnd(fileInformation->loopEnd);

                if (!region->loopMode)
                    region->loopMode = SfzLoopMode::loop_continuous;
            }

            if (region->loopRange.getEnd() == Default::loopRange.getEnd())
                region->loopRange.setEnd(region->sampleEnd);

            if (fileInformation->numChannels == 2)
                region->hasStereoSample = true;

            // TODO: adjust with LFO targets
            const auto maxOffset = [region]() {
                uint64_t sumOffsetCC = region->offset + region->offsetRandom;
                for (const auto& offsets : region->offsetCC)
                    sumOffsetCC += offsets.data;
                return Default::offsetCCRange.clamp(sumOffsetCC);
            }();

            if (!resources.filePool.preloadFile(region->sampleId, maxOffset))
                removeCurrentRegion();
        } else if (region->oscillator && !region->isGenerator()) {
            if (!resources.filePool.checkSampleId(region->sampleId)) {
                removeCurrentRegion();
                continue;
            }

            if (!resources.wavePool.createFileWave(resources.filePool, std::string(region->sampleId.filename()))) {
                removeCurrentRegion();
                continue;
            }
        }

        if (region->keyswitchLabel && region->keyswitch)
            keyswitchLabels.push_back({ *region->keyswitch, *region->keyswitchLabel });

        // Some regions had group number but no "group-level" opcodes handled the polyphony
        while (polyphonyGroups.size() <= region->group) {
            polyphonyGroups.emplace_back();
            polyphonyGroups.back().setPolyphonyLimit(config::maxVoices);
        }

        for (auto note = 0; note < 128; note++) {
            if (region->keyRange.containsWithEnd(note) || (region->hasKeyswitches() && region->keyswitchRange.containsWithEnd(note)))
                noteActivationLists[note].push_back(region);
        }

        for (int cc = 0; cc < config::numCCs; cc++) {
            if (region->ccTriggers.contains(cc) || region->ccConditions.contains(cc))
                ccActivationLists[cc].push_back(region);
        }

        // Defaults
        for (int cc = 0; cc < config::numCCs; cc++) {
            region->registerCC(cc, resources.midiState.getCCValue(cc));
        }

        if (defaultSwitch) {
            region->registerNoteOn(*defaultSwitch, 1.0f, 1.0f);
            region->registerNoteOff(*defaultSwitch, 0.0f, 1.0f);
        }

        // Set the default frequencies on equalizers if needed
        if (region->equalizers.size() > 0
            && region->equalizers[0].frequency == Default::eqFrequencyUnset) {
            region->equalizers[0].frequency = Default::eqFrequency1;
            if (region->equalizers.size() > 1
                && region->equalizers[1].frequency == Default::eqFrequencyUnset) {
                region->equalizers[1].frequency = Default::eqFrequency2;
                if (region->equalizers.size() > 2
                    && region->equalizers[2].frequency == Default::eqFrequencyUnset) {
                    region->equalizers[2].frequency = Default::eqFrequency3;
                }
            }
        }

        if (!region->velocityPoints.empty())
            region->velCurve = Curve::buildFromVelcurvePoints(
                region->velocityPoints, Curve::Interpolator::Linear, region->ampVeltrack < 0.0f);
        region->registerPitchWheel(0);
        region->registerAftertouch(0);
        region->registerTempo(2.0f);
        maxFilters = max(maxFilters, region->filters.size());
        maxEQs = max(maxEQs, region->equalizers.size());

        ++currentRegion;
    }
    const auto remainingRegions = std::distance(regions.begin(), lastRegion.base());
    DBG("Removing " << (regions.size() - remainingRegions) << " out of " << regions.size() << " regions");
    regions.resize(remainingRegions);
    modificationTime = checkModificationTime();


    applyToAllVoices([&](std::unique_ptr<Voice>& voice) {
        voice->setMaxFiltersPerVoice(maxFilters);
        voice->setMaxEQsPerVoice(maxEQs);
    });
}

sfz::Voice* sfz::Synth::stealVoice(sfz::Synth::VoiceViewVector& candidates, VoiceStealingPolicy policy)
{
    UNUSED(policy);
    // Start of the voice stealing algorithm
    absl::c_sort(candidates, voiceOrdering);

    const auto sumEnvelope = absl::c_accumulate(candidates, 0.0f, [](float sum, const Voice* v) {
        return sum + v->getAverageEnvelope();
    });

    const auto envThreshold = sumEnvelope
        / static_cast<float>(candidates.size()) * config::stealingEnvelopeCoeff;
    const auto ageThreshold = candidates.front()->getAge() * config::stealingAgeCoeff;


    Voice* returnedVoice = candidates.front();
    unsigned idx = 0;
    while (idx < candidates.size()) {
        const auto ref = candidates[idx];

        if (ref->getAge() < ageThreshold) {
            // Went too far, we'll kill the oldest note.
            break;
        }

        float maxEnvelope = ref->getAverageEnvelope();
        returnedVoice->applyToSisterRing([&](Voice* v) {
            maxEnvelope = max(maxEnvelope, v->getAverageEnvelope());
        });

        if (maxEnvelope < envThreshold) {
            returnedVoice = ref;
            break;
        }

        // Jump over the sister voices in the set
        do { idx++; }
        while (idx < candidates.size() && sisterVoices(ref, candidates[idx]));
    }

    auto tempSpan = resources.bufferPool.getStereoBuffer(samplesPerBlock);
    returnedVoice->applyToSisterRing([&] (Voice* v) {
        std::cout << "Killing a voice" << '\n';
        renderVoiceToOutputs(*v, *tempSpan);
        v->reset();
    });
    ASSERT(returnedVoice->isFree());
    return returnedVoice;
}

sfz::Voice* sfz::Synth::findFreeVoice() noexcept
{
    auto freeVoice = absl::c_find_if(voices, [](const std::unique_ptr<Voice>& voice) {
        return voice->isFree();
    });
    if (freeVoice != voices.end())
        return freeVoice->get();

    return {};
}

int sfz::Synth::getNumActiveVoices() const noexcept
{
    auto activeVoices = 0;
    for (const auto& voice : voices) {
        if (!voice->isFree())
            activeVoices++;
    }
    return activeVoices;
}

void sfz::Synth::garbageCollect() noexcept
{
}

void sfz::Synth::setSamplesPerBlock(int samplesPerBlock) noexcept
{
    ASSERT(samplesPerBlock < config::maxBlockSize);

    const std::lock_guard<std::mutex> disableCallback { callbackGuard };

    this->samplesPerBlock = samplesPerBlock;
    applyToAllVoices([&](VoicePtr& voice) {
        voice->setSamplesPerBlock(samplesPerBlock);
    });

    resources.setSamplesPerBlock(samplesPerBlock);

    for (auto& bus : effectBuses) {
        if (bus)
            bus->setSamplesPerBlock(samplesPerBlock);
    }
}

void sfz::Synth::setSampleRate(float sampleRate) noexcept
{
    const std::lock_guard<std::mutex> disableCallback { callbackGuard };

    this->sampleRate = sampleRate;
    applyToAllVoices([&](VoicePtr& voice) {
        voice->setSampleRate(sampleRate);
    });

    resources.setSampleRate(sampleRate);

    for (auto& bus : effectBuses) {
        if (bus)
            bus->setSampleRate(sampleRate);
    }
}

void sfz::Synth::renderVoiceToOutputs(Voice& voice, AudioSpan<float>& tempSpan) noexcept
{
    const Region* region = voice.getRegion();
    voice.renderBlock(tempSpan);
    for (size_t i = 0, n = effectBuses.size(); i < n; ++i) {
        if (auto& bus = effectBuses[i]) {
            float addGain = region->getGainToEffectBus(i);
            bus->addToInputs(tempSpan, addGain, tempSpan.getNumFrames());
        }
    }
}

void sfz::Synth::renderBlock(AudioSpan<float> buffer) noexcept
{
    ScopedFTZ ftz;
    CallbackBreakdown callbackBreakdown;

    { // Silence buffer
        ScopedTiming logger { callbackBreakdown.renderMethod };
        buffer.fill(0.0f);
    }

    if (freeWheeling)
        resources.filePool.waitForBackgroundLoading();

    const std::unique_lock<std::mutex> lock { callbackGuard, std::try_to_lock };
    if (!lock.owns_lock())
        return;

    size_t numFrames = buffer.getNumFrames();
    auto tempSpan = resources.bufferPool.getStereoBuffer(numFrames);
    auto tempMixSpan = resources.bufferPool.getStereoBuffer(numFrames);
    auto rampSpan = resources.bufferPool.getBuffer(numFrames);
    if (!tempSpan || !tempMixSpan || !rampSpan) {
        DBG("[sfizz] Could not get a temporary buffer; exiting callback... ");
        return;
    }

    int numActiveVoices { 0 };
    { // Main render block
        ScopedTiming logger { callbackBreakdown.renderMethod, ScopedTiming::Operation::addToDuration };
        tempSpan->fill(0.0f);
        tempMixSpan->fill(0.0f);
        resources.filePool.cleanupPromises();

        // Ramp out whatever is in the buffer at this point; should only be killed voice data
        linearRamp<float>(*rampSpan, 1.0f, -1.0f / static_cast<float>(numFrames));
        for (size_t i = 0, n = effectBuses.size(); i < n; ++i) {
            if (auto& bus = effectBuses[i]) {
                bus->applyGain(rampSpan->data(), numFrames);
            }
        }

        applyToAllVoices([&](VoicePtr& voice) {
            if (!voice->isFree()) {
                numActiveVoices++;
                renderVoiceToOutputs(*voice, *tempSpan);
                callbackBreakdown.data += voice->getLastDataDuration();
                callbackBreakdown.amplitude += voice->getLastAmplitudeDuration();
                callbackBreakdown.filters += voice->getLastFilterDuration();
                callbackBreakdown.panning += voice->getLastPanningDuration();
            }
        });
    }

    { // Apply effect buses
        // -- note(jpc) there is always a "main" bus which is initially empty.
        //    without any <effect>, the signal is just going to flow through it.
        ScopedTiming logger { callbackBreakdown.effects, ScopedTiming::Operation::addToDuration };

        for (auto& bus : effectBuses) {
            if (bus) {
                bus->process(numFrames);
                bus->mixOutputsTo(buffer, *tempMixSpan, numFrames);
            }
        }
    }

    // Add the Mix output (fxNtomix opcodes)
    // -- note(jpc) the purpose of the Mix output is not known.
    //    perhaps it's designed as extension point for custom processing?
    //    as default behavior, it adds itself to the Main signal.
    buffer.add(*tempMixSpan);

    // Apply the master volume
    buffer.applyGain(db2mag(volume));

    { // Clear events and advance midi time
        ScopedTiming logger { dispatchDuration, ScopedTiming::Operation::addToDuration };
        resources.midiState.advanceTime(buffer.getNumFrames());
    }

    callbackBreakdown.dispatch = dispatchDuration;
    resources.logger.logCallbackTime(callbackBreakdown, numActiveVoices, numFrames);

    // Reset the dispatch counter
    dispatchDuration = Duration(0);

    { // Clear for the next run
        ScopedTiming logger { callbackBreakdown.effects };
        for (auto& bus : effectBuses) {
            if (bus)
                bus->clearInputs(numFrames);
        }
    }

    ASSERT(!hasNanInf(buffer.getConstSpan(0)));
    ASSERT(!hasNanInf(buffer.getConstSpan(1)));
    CHECK(isReasonableAudio(buffer.getConstSpan(0)));
    CHECK(isReasonableAudio(buffer.getConstSpan(1)));
}

void sfz::Synth::noteOn(int delay, int noteNumber, uint8_t velocity) noexcept
{
    ASSERT(noteNumber < 128);
    ASSERT(noteNumber >= 0);
    const auto normalizedVelocity = normalizeVelocity(velocity);
    ScopedTiming logger { dispatchDuration, ScopedTiming::Operation::addToDuration };
    resources.midiState.noteOnEvent(delay, noteNumber, normalizedVelocity);

    const std::unique_lock<std::mutex> lock { callbackGuard, std::try_to_lock };
    if (!lock.owns_lock())
        return;

    noteOnDispatch(delay, noteNumber, normalizedVelocity);
}

void sfz::Synth::noteOff(int delay, int noteNumber, uint8_t velocity) noexcept
{
    ASSERT(noteNumber < 128);
    ASSERT(noteNumber >= 0);
    UNUSED(velocity);
    const auto normalizedVelocity = normalizeVelocity(velocity);
    ScopedTiming logger { dispatchDuration, ScopedTiming::Operation::addToDuration };
    resources.midiState.noteOffEvent(delay, noteNumber, normalizedVelocity);

    const std::unique_lock<std::mutex> lock { callbackGuard, std::try_to_lock };
    if (!lock.owns_lock())
        return;

    // FIXME: Some keyboards (e.g. Casio PX5S) can send a real note-off velocity. In this case, do we have a
    // way in sfz to specify that a release trigger should NOT use the note-on velocity?
    // auto replacedVelocity = (velocity == 0 ? sfz::getNoteVelocity(noteNumber) : velocity);
    const auto replacedVelocity = resources.midiState.getNoteVelocity(noteNumber);

    for (auto& voice : voices)
        voice->registerNoteOff(delay, noteNumber, replacedVelocity);

    noteOffDispatch(delay, noteNumber, replacedVelocity);
}

void sfz::Synth::noteOffDispatch(int delay, int noteNumber, float velocity) noexcept
{
    const auto randValue = randNoteDistribution(Random::randomGenerator);
    SisterVoiceRingBuilder ring;

    for (auto& region : noteActivationLists[noteNumber]) {
        if (region->registerNoteOff(noteNumber, velocity, randValue)) {
            auto voice = findFreeVoice();
            if (voice == nullptr)
                continue;

            voice->startVoice(region, delay, noteNumber, velocity, Voice::TriggerType::NoteOff);
            ring.addVoiceToRing(voice);
        }
    }
}

void sfz::Synth::noteOnDispatch(int delay, int noteNumber, float velocity) noexcept
{
    const auto randValue = randNoteDistribution(Random::randomGenerator);
    SisterVoiceRingBuilder ring;
    VoiceViewVector idRegionVoices;
    auto& regionPolyphonyViews = tempVoiceViews;

    for (auto& region : noteActivationLists[noteNumber]) {
        if (region->registerNoteOn(noteNumber, velocity, randValue)) {
            unsigned notePolyphonyCounter { 0 };
            Voice* selfMaskCandidate { nullptr };
            regionPolyphonyViews.clear();

            for (auto& voice : voices) {
                const auto voiceRegion = voice->getRegion();
                if (voice->isFree() || voiceRegion == nullptr)
                    continue;

                if (voiceRegion == region)
                    regionPolyphonyViews.push_back(voice.get());

                if (region->notePolyphony) {
                    if (voice->getTriggerNumber() == noteNumber && voice->getTriggerType() == Voice::TriggerType::NoteOn) {
                        notePolyphonyCounter += 1;
                        switch (region->selfMask) {
                        case SfzSelfMask::mask:
                            if (voice->getTriggerValue() < velocity) {
                                if (!selfMaskCandidate || selfMaskCandidate->getTriggerValue() > voice->getTriggerValue())
                                    selfMaskCandidate = voice.get();
                            }
                            break;
                        case SfzSelfMask::dontMask:
                            if (!selfMaskCandidate || selfMaskCandidate->getSourcePosition() < voice->getSourcePosition())
                                selfMaskCandidate = voice.get();
                            break;
                        }
                    }
                }

                if (voice->checkOffGroup(delay, region->group))
                    noteOffDispatch(delay, voice->getTriggerNumber(), voice->getTriggerValue());
            }

            // Check all the polyphony limits
            Voice* voice { nullptr };
            auto polyphonyGroupVoices = polyphonyGroups[region->group].getActiveVoices();

            if (polyphonyGroupVoices.size()
                == polyphonyGroups[region->group].getPolyphonyLimit()) {
                DBG("Stealing a voice due to polyphony groups");
                voice = stealVoice(polyphonyGroupVoices);
            } else if (regionPolyphonyViews.size() >= region->polyphony) {
                DBG("Stealing a voice due to region polyphony");
                voice = stealVoice(regionPolyphonyViews);
            } else if (region->notePolyphony && notePolyphonyCounter >= *region->notePolyphony) {
                if (selfMaskCandidate != nullptr) {
                    DBG("Stealing the self-mask candidate");
                    selfMaskCandidate->release(delay);
                } else {
                    DBG("Not starting the voice; masked...");
                    // We're the lowest velocity guy here, so we just don't start
                    continue;
                }
            } else {
                auto parent = region->parent;
                while (parent != nullptr && voice == nullptr) {
                    auto activeVoices = parent->getActiveVoices();
                    if (activeVoices.size() >= parent->getPolyphonyLimit()) {
                        DBG("Stealing due to a parent polyphony");
                        voice = stealVoice(activeVoices);
                    }
                    parent = parent->getParent();
                }
            }

            if (voice == nullptr)
                voice = findFreeVoice();

            if (voice == nullptr)
                voice = stealVoice(allVoiceViews);

            voice->startVoice(region, delay, noteNumber, velocity, Voice::TriggerType::NoteOn);
            ring.addVoiceToRing(voice);
        }
    }
}

void sfz::Synth::cc(int delay, int ccNumber, uint8_t ccValue) noexcept
{
    ASSERT(ccNumber < config::numCCs);
    ASSERT(ccNumber >= 0);
    const auto normalizedCC = normalizeCC(ccValue);

    ScopedTiming logger { dispatchDuration, ScopedTiming::Operation::addToDuration };
    resources.midiState.ccEvent(delay, ccNumber, normalizedCC);

    const std::unique_lock<std::mutex> lock { callbackGuard, std::try_to_lock };
    if (!lock.owns_lock())
        return;

    if (ccNumber == config::resetCC) {
        resetAllControllers(delay);
        return;
    }

    for (auto& voice : voices)
        voice->registerCC(delay, ccNumber, normalizedCC);

    SisterVoiceRingBuilder ring;

    for (auto& region : ccActivationLists[ccNumber]) {
        if (region->registerCC(ccNumber, normalizedCC)) {
            auto voice = findFreeVoice();
            if (voice == nullptr)
                continue;

            voice->startVoice(region, delay, ccNumber, normalizedCC, Voice::TriggerType::CC);
            ring.addVoiceToRing(voice);
        }
    }
}

void sfz::Synth::pitchWheel(int delay, int pitch) noexcept
{
    ASSERT(pitch <= 8192);
    ASSERT(pitch >= -8192);
    const auto normalizedPitch = normalizeBend(float(pitch));

    ScopedTiming logger { dispatchDuration, ScopedTiming::Operation::addToDuration };
    resources.midiState.pitchBendEvent(delay, normalizedPitch);

    for (auto& region : regions) {
        region->registerPitchWheel(normalizedPitch);
    }

    for (auto& voice : voices) {
        voice->registerPitchWheel(delay, normalizedPitch);
    }
}
void sfz::Synth::aftertouch(int /* delay */, uint8_t /* aftertouch */) noexcept
{
    ScopedTiming logger { dispatchDuration, ScopedTiming::Operation::addToDuration };
}
void sfz::Synth::tempo(int /* delay */, float /* secondsPerQuarter */) noexcept
{
    ScopedTiming logger { dispatchDuration, ScopedTiming::Operation::addToDuration };
}

int sfz::Synth::getNumRegions() const noexcept
{
    return static_cast<int>(regions.size());
}
int sfz::Synth::getNumGroups() const noexcept
{
    return numGroups;
}
int sfz::Synth::getNumMasters() const noexcept
{
    return numMasters;
}
int sfz::Synth::getNumCurves() const noexcept
{
    return static_cast<int>(resources.curves.getNumCurves());
}

std::string sfz::Synth::exportMidnam(absl::string_view model) const
{
    pugi::xml_document doc;
    absl::string_view manufacturer = config::midnamManufacturer;

    if (model.empty())
        model = config::midnamModel;

    doc.append_child(pugi::node_doctype).set_value("MIDINameDocument PUBLIC"
                                                   " \"-//MIDI Manufacturers Association//DTD MIDINameDocument 1.0//EN\""
                                                   " \"http://www.midi.org/dtds/MIDINameDocument10.dtd\"");

    pugi::xml_node root = doc.append_child("MIDINameDocument");

    root.append_child(pugi::node_comment)
        .set_value("Generated by Sfizz for the current instrument");

    root.append_child("Author");

    pugi::xml_node device = root.append_child("MasterDeviceNames");
    device.append_child("Manufacturer")
        .append_child(pugi::node_pcdata)
        .set_value(std::string(manufacturer).c_str());
    device.append_child("Model")
        .append_child(pugi::node_pcdata)
        .set_value(std::string(model).c_str());

    {
        pugi::xml_node devmode = device.append_child("CustomDeviceMode");
        devmode.append_attribute("Name").set_value("Default");

        pugi::xml_node nsas = devmode.append_child("ChannelNameSetAssignments");
        for (unsigned c = 0; c < 16; ++c) {
            pugi::xml_node nsa = nsas.append_child("ChannelNameSetAssign");
            nsa.append_attribute("Channel").set_value(std::to_string(c + 1).c_str());
            nsa.append_attribute("NameSet").set_value("Play");
        }
    }

    {
        pugi::xml_node chns = device.append_child("ChannelNameSet");
        chns.append_attribute("Name").set_value("Play");

        pugi::xml_node acs = chns.append_child("AvailableForChannels");
        for (unsigned c = 0; c < 16; ++c) {
            pugi::xml_node ac = acs.append_child("AvailableChannel");
            ac.append_attribute("Channel").set_value(std::to_string(c + 1).c_str());
            ac.append_attribute("Available").set_value("true");
        }

        chns.append_child("UsesControlNameList")
            .append_attribute("Name")
            .set_value("Controls");
        chns.append_child("UsesNoteNameList")
            .append_attribute("Name")
            .set_value("Notes");
    }

    {
        pugi::xml_node cns = device.append_child("ControlNameList");
        cns.append_attribute("Name").set_value("Controls");
        for (const auto& pair : ccLabels) {
            pugi::xml_node cn = cns.append_child("Control");
            cn.append_attribute("Type").set_value("7bit");
            cn.append_attribute("Number").set_value(std::to_string(pair.first).c_str());
            cn.append_attribute("Name").set_value(pair.second.c_str());
        }
    }

    {
        pugi::xml_node nnl = device.append_child("NoteNameList");
        nnl.append_attribute("Name").set_value("Notes");
        for (const auto& pair : keyswitchLabels) {
            pugi::xml_node nn = nnl.append_child("Note");
            nn.append_attribute("Number").set_value(std::to_string(pair.first).c_str());
            nn.append_attribute("Name").set_value(pair.second.c_str());
        }
        for (const auto& pair : keyLabels) {
            pugi::xml_node nn = nnl.append_child("Note");
            nn.append_attribute("Number").set_value(std::to_string(pair.first).c_str());
            nn.append_attribute("Name").set_value(pair.second.c_str());
        }
    }

    ///
    struct string_writer : pugi::xml_writer {
        std::string result;

        string_writer()
        {
            result.reserve(8192);
        }

        void write(const void* data, size_t size) override
        {
            result.append(static_cast<const char*>(data), size);
        }
    };

    ///
    string_writer writer;
    doc.save(writer);
    return std::move(writer.result);
}

const sfz::Region* sfz::Synth::getRegionView(int idx) const noexcept
{
    return (size_t)idx < regions.size() ? regions[idx].get() : nullptr;
}

const sfz::RegionSet* sfz::Synth::getRegionSetView(int idx) const noexcept
{
    return (size_t)idx < sets.size() ? sets[idx].get() : nullptr;
}

const sfz::PolyphonyGroup* sfz::Synth::getPolyphonyGroupView(int idx) const noexcept
{
    return (size_t)idx < polyphonyGroups.size() ? &polyphonyGroups[idx] : nullptr;
}

const sfz::EffectBus* sfz::Synth::getEffectBusView(int idx) const noexcept
{
    return (size_t)idx < effectBuses.size() ? effectBuses[idx].get() : nullptr;
}

const sfz::Voice* sfz::Synth::getVoiceView(int idx) const noexcept
{
    return (size_t)idx < voices.size() ? voices[idx].get() : nullptr;
}

unsigned sfz::Synth::getNumPolyphonyGroups() const noexcept
{
    return polyphonyGroups.size();
}

const std::vector<std::string>& sfz::Synth::getUnknownOpcodes() const noexcept
{
    return unknownOpcodes;
}
size_t sfz::Synth::getNumPreloadedSamples() const noexcept
{
    return resources.filePool.getNumPreloadedSamples();
}

float sfz::Synth::getVolume() const noexcept
{
    return volume;
}
void sfz::Synth::setVolume(float volume) noexcept
{
    this->volume = Default::volumeRange.clamp(volume);
}

int sfz::Synth::getNumVoices() const noexcept
{
    return numVoices;
}

void sfz::Synth::setNumVoices(int numVoices) noexcept
{
    ASSERT(numVoices > 0);
    const std::lock_guard<std::mutex> disableCallback { callbackGuard };

    // fast path
    if (numVoices == this->numVoices)
        return;

    resetVoices(numVoices);
}

void sfz::Synth::resetVoices(int numVoices)
{
    voices.clear();
    voices.reserve(numVoices);

    for (int i = 0; i < numVoices; ++i)
        voices.push_back(absl::make_unique<Voice>(resources));

    allVoiceViews.clear();
    allVoiceViews.reserve(numVoices);
    tempVoiceViews.clear();
    tempVoiceViews.reserve(numVoices);

    for (auto& voice : voices) {
        voice->setSampleRate(this->sampleRate);
        voice->setSamplesPerBlock(this->samplesPerBlock);
        allVoiceViews.push_back(voice.get());
    }

    overflowVoices.clear();
    overflowVoices.reserve(numVoices);
    for (int i = 0; i < numVoices; ++i)
        overflowVoices.push_back(absl::make_unique<Voice>(resources));

    for (auto& voice : overflowVoices) {
        voice->setSampleRate(this->sampleRate);
        voice->setSamplesPerBlock(this->samplesPerBlock);
    }

    this->numVoices = numVoices;
}

void sfz::Synth::setOversamplingFactor(sfz::Oversampling factor) noexcept
{
    const std::lock_guard<std::mutex> disableCallback { callbackGuard };

    // fast path
    if (factor == oversamplingFactor)
        return;

    for (auto& voice : voices)
        voice->reset();

    resources.filePool.emptyFileLoadingQueues();
    resources.filePool.setOversamplingFactor(factor);
    oversamplingFactor = factor;
}

sfz::Oversampling sfz::Synth::getOversamplingFactor() const noexcept
{
    return oversamplingFactor;
}

void sfz::Synth::setPreloadSize(uint32_t preloadSize) noexcept
{
    const std::lock_guard<std::mutex> disableCallback { callbackGuard };

    // fast path
    if (preloadSize == resources.filePool.getPreloadSize())
        return;

    resources.filePool.setPreloadSize(preloadSize);
}

uint32_t sfz::Synth::getPreloadSize() const noexcept
{
    return resources.filePool.getPreloadSize();
}

void sfz::Synth::enableFreeWheeling() noexcept
{
    if (!freeWheeling) {
        freeWheeling = true;
        DBG("Enabling freewheeling");
    }
}
void sfz::Synth::disableFreeWheeling() noexcept
{
    if (freeWheeling) {
        freeWheeling = false;
        DBG("Disabling freewheeling");
    }
}

void sfz::Synth::resetAllControllers(int delay) noexcept
{
    resources.midiState.resetAllControllers(delay);

    const std::unique_lock<std::mutex> lock { callbackGuard, std::try_to_lock };
    if (!lock.owns_lock())
        return;

    for (auto& voice : voices) {
        voice->registerPitchWheel(delay, 0);
        for (int cc = 0; cc < config::numCCs; ++cc)
            voice->registerCC(delay, cc, 0.0f);
    }

    for (auto& region : regions) {
        for (int cc = 0; cc < config::numCCs; ++cc)
            region->registerCC(cc, 0.0f);
    }
}

fs::file_time_type sfz::Synth::checkModificationTime()
{
    auto returnedTime = modificationTime;
    for (const auto& file : parser.getIncludedFiles()) {
        std::error_code ec;
        const auto fileTime = fs::last_write_time(file, ec);
        if (!ec && returnedTime < fileTime)
            returnedTime = fileTime;
    }
    return returnedTime;
}

bool sfz::Synth::shouldReloadFile()
{
    return (checkModificationTime() > modificationTime);
}

void sfz::Synth::enableLogging(absl::string_view prefix) noexcept
{
    resources.logger.enableLogging(prefix);
}

void sfz::Synth::setLoggingPrefix(absl::string_view prefix) noexcept
{
    resources.logger.setPrefix(prefix);
}

void sfz::Synth::disableLogging() noexcept
{
    resources.logger.disableLogging();
}

void sfz::Synth::allSoundOff() noexcept
{
    const std::lock_guard<std::mutex> disableCallback { callbackGuard };

    for (auto& voice : voices)
        voice->reset();
    for (auto& effectBus : effectBuses)
        effectBus->clear();
}

void sfz::Synth::setGroupPolyphony(unsigned groupIdx, unsigned polyphony) noexcept
{
    while (polyphonyGroups.size() <= groupIdx)
        polyphonyGroups.emplace_back();

    polyphonyGroups[groupIdx].setPolyphonyLimit(polyphony);
}
