// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "Region.h"
#include "Defaults.h"
#include "MathHelpers.h"
#include "Macros.h"
#include "Debug.h"
#include "Opcode.h"
#include "StringViewHelpers.h"
#include "MidiState.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_cat.h"
#include "absl/algorithm/container.h"
#include <random>

template<class T>
bool extendIfNecessary(std::vector<T>& vec, unsigned size, unsigned defaultCapacity)
{
    if (size == 0)
        return false;

    if (vec.capacity() == 0)
        vec.reserve(defaultCapacity);

    if (vec.size() < size)
        vec.resize(size);

    return true;
}

bool sfz::Region::parseOpcode(const Opcode& opcode)
{
    switch (opcode.lettersOnlyHash) {
    // Sound source: sample playback
    case hash("sample"):
        {
            const auto trimmedSample = trim(opcode.value);
            if (trimmedSample.empty())
                break;

            if (trimmedSample[0] == '*')
                sample = std::string(trimmedSample);
            else
                sample = absl::StrCat(defaultPath, absl::StrReplaceAll(trimmedSample, { { "\\", "/" } }));
        }
        break;
    case hash("delay"):
        setValueFromOpcode(opcode, delay, Default::delayRange);
        break;
    case hash("delay_random"):
        setValueFromOpcode(opcode, delayRandom, Default::delayRange);
        break;
    case hash("offset"):
        setValueFromOpcode(opcode, offset, Default::offsetRange);
        break;
    case hash("offset_random"):
        setValueFromOpcode(opcode, offsetRandom, Default::offsetRange);
        break;
    case hash("end"):
        setValueFromOpcode(opcode, sampleEnd, Default::sampleEndRange);
        break;
    case hash("count"):
        setValueFromOpcode(opcode, sampleCount, Default::sampleCountRange);
        break;
    case hash("loopmode"): // fallthrough
    case hash("loop_mode"):
        switch (hash(opcode.value)) {
        case hash("no_loop"):
            loopMode = SfzLoopMode::no_loop;
            break;
        case hash("one_shot"):
            loopMode = SfzLoopMode::one_shot;
            break;
        case hash("loop_continuous"):
            loopMode = SfzLoopMode::loop_continuous;
            break;
        case hash("loop_sustain"):
            loopMode = SfzLoopMode::loop_sustain;
            break;
        default:
            DBG("Unkown loop mode:" << std::string(opcode.value));
        }
        break;
    case hash("loopend"): // fallthrough
    case hash("loop_end"):
        setRangeEndFromOpcode(opcode, loopRange, Default::loopRange);
        break;
    case hash("loopstart"): // fallthrough
    case hash("loop_start"):
        setRangeStartFromOpcode(opcode, loopRange, Default::loopRange);
        break;

    // Wavetable oscillator
    case hash("oscillator_phase"):
        setValueFromOpcode(opcode, oscillatorPhase, Default::oscillatorPhaseRange);
        break;
    case hash("oscillator"):
        if (auto value = readBooleanFromOpcode(opcode))
            oscillator = *value;
        break;
    case hash("oscillator_multi"):
        setValueFromOpcode(opcode, oscillatorMulti, Default::oscillatorMultiRange);
        break;
    case hash("oscillator_detune"):
        setValueFromOpcode(opcode, oscillatorDetune, Default::oscillatorDetuneRange);
        break;

    // Instrument settings: voice lifecycle
    case hash("group"): // fallthrough
    case hash("polyphony_group"):
        setValueFromOpcode(opcode, group, Default::groupRange);
        break;
    case hash("offby"): // fallthrough
    case hash("off_by"):
        setValueFromOpcode(opcode, offBy, Default::groupRange);
        break;
    case hash("off_mode"):
        switch (hash(opcode.value)) {
        case hash("fast"):
            offMode = SfzOffMode::fast;
            break;
        case hash("normal"):
            offMode = SfzOffMode::normal;
            break;
        default:
            DBG("Unkown off mode:" << std::string(opcode.value));
        }
        break;
    case hash("note_polyphony"):
        if (auto value = readOpcode(opcode.value, Default::polyphonyRange))
            notePolyphony = *value;
        break;
    case hash("note_selfmask"):
        switch (hash(opcode.value)) {
        case hash("on"):
            selfMask = SfzSelfMask::mask;
            break;
        case hash("off"):
            selfMask = SfzSelfMask::dontMask;
            break;
        default:
            DBG("Unkown self mask value:" << std::string(opcode.value));
        }
        break;
    // Region logic: key mapping
    case hash("lokey"):
        setRangeStartFromOpcode(opcode, keyRange, Default::keyRange);
        break;
    case hash("hikey"):
        triggerOnCC = (opcode.value == "-1");
        setRangeEndFromOpcode(opcode, keyRange, Default::keyRange);
        break;
    case hash("key"):
        triggerOnCC = (opcode.value == "-1");
        setRangeStartFromOpcode(opcode, keyRange, Default::keyRange);
        setRangeEndFromOpcode(opcode, keyRange, Default::keyRange);
        setValueFromOpcode(opcode, pitchKeycenter, Default::keyRange);
        break;
    case hash("lovel"):
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            velocityRange.setStart(normalizeVelocity(*value));
        break;
    case hash("hivel"):
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            velocityRange.setEnd(normalizeVelocity(*value));
        break;

    // Region logic: MIDI conditions
    case hash("lobend"):
        if (auto value = readOpcode(opcode.value, Default::bendRange))
            bendRange.setStart(normalizeBend(*value));
        break;
    case hash("hibend"):
        if (auto value = readOpcode(opcode.value, Default::bendRange))
            bendRange.setEnd(normalizeBend(*value));
        break;
    case hash("locc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            ccConditions[opcode.parameters.back()].setStart(normalizeCC(*value));
        break;
    case hash("hicc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            ccConditions[opcode.parameters.back()].setEnd(normalizeCC(*value));
        break;
    case hash("sw_lokey"):
        setRangeStartFromOpcode(opcode, keyswitchRange, Default::keyRange);
        break;
    case hash("sw_hikey"):
        setRangeEndFromOpcode(opcode, keyswitchRange, Default::keyRange);
        break;
    case hash("sw_last"):
        setValueFromOpcode(opcode, keyswitch, Default::keyRange);
        keySwitched = false;
        break;
    case hash("sw_down"):
        setValueFromOpcode(opcode, keyswitchDown, Default::keyRange);
        keySwitched = false;
        break;
    case hash("sw_up"):
        setValueFromOpcode(opcode, keyswitchUp, Default::keyRange);
        break;
    case hash("sw_previous"):
        setValueFromOpcode(opcode, previousNote, Default::keyRange);
        previousKeySwitched = false;
        break;
    case hash("sw_vel"):
        switch (hash(opcode.value)) {
        case hash("current"):
            velocityOverride = SfzVelocityOverride::current;
            break;
        case hash("previous"):
            velocityOverride = SfzVelocityOverride::previous;
            break;
        default:
            DBG("Unknown velocity mode: " << std::string(opcode.value));
        }
        break;

    case hash("sustain_sw"):
        checkSustain = readBooleanFromOpcode(opcode).value_or(Default::checkSustain);
        break;
    case hash("sostenuto_sw"):
        checkSostenuto = readBooleanFromOpcode(opcode).value_or(Default::checkSostenuto);
        break;
    // Region logic: internal conditions
    case hash("lochanaft"):
        setRangeStartFromOpcode(opcode, aftertouchRange, Default::aftertouchRange);
        break;
    case hash("hichanaft"):
        setRangeEndFromOpcode(opcode, aftertouchRange, Default::aftertouchRange);
        break;
    case hash("lobpm"):
        setRangeStartFromOpcode(opcode, bpmRange, Default::bpmRange);
        break;
    case hash("hibpm"):
        setRangeEndFromOpcode(opcode, bpmRange, Default::bpmRange);
        break;
    case hash("lorand"):
        setRangeStartFromOpcode(opcode, randRange, Default::randRange);
        break;
    case hash("hirand"):
        setRangeEndFromOpcode(opcode, randRange, Default::randRange);
        break;
    case hash("seq_length"):
        setValueFromOpcode(opcode, sequenceLength, Default::sequenceRange);
        break;
    case hash("seq_position"):
        setValueFromOpcode(opcode, sequencePosition, Default::sequenceRange);
        sequenceSwitched = (opcode.value == "1");
        break;
    // Region logic: triggers
    case hash("trigger"):
        switch (hash(opcode.value)) {
        case hash("attack"):
            trigger = SfzTrigger::attack;
            break;
        case hash("first"):
            trigger = SfzTrigger::first;
            break;
        case hash("legato"):
            trigger = SfzTrigger::legato;
            break;
        case hash("release"):
            trigger = SfzTrigger::release;
            break;
        case hash("release_key"):
            trigger = SfzTrigger::release_key;
            break;
        default:
            DBG("Unknown trigger mode: " << std::string(opcode.value));
        }
        break;
    case hash("on_locc&"): // fallthrough
    case hash("start_locc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            ccTriggers[opcode.parameters.back()].setStart(normalizeCC(*value));
        break;
    case hash("on_hicc&"): // fallthrough
    case hash("start_hicc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            ccTriggers[opcode.parameters.back()].setEnd(normalizeCC(*value));
        break;

    // Performance parameters: amplifier
    case hash("volume"):
        setValueFromOpcode(opcode, volume, Default::volumeRange);
        break;
    case hash("volume_curvecc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::curveCCRange))
            volumeCC[opcode.parameters.back()].curve = *value;
        break;
    case hash("volume_stepcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::stepCCRange))
            volumeCC[opcode.parameters.back()].steps = *value;
        break;
    case hash("volume_smoothcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::smoothCCRange))
            volumeCC[opcode.parameters.back()].smooth = *value;
        break;
    case hash("gain_cc&"):
    case hash("gain_oncc&"): // fallthrough
    case hash("volume_oncc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::volumeCCRange))
            volumeCC[opcode.parameters.back()].value = *value;
        break;
    case hash("amplitude"):
        if (auto value = readOpcode(opcode.value, Default::amplitudeRange))
            amplitude = normalizePercents(*value);
        break;
    case hash("amplitude_curvecc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::curveCCRange))
            amplitudeCC[opcode.parameters.back()].curve = *value;
        break;
    case hash("amplitude_stepcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::stepCCRange))
            amplitudeCC[opcode.parameters.back()].steps = *value;
        break;
    case hash("amplitude_smoothcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::smoothCCRange))
            amplitudeCC[opcode.parameters.back()].smooth = *value;
        break;
    case hash("amplitude_cc&"): // fallthrough
    case hash("amplitude_oncc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::amplitudeRange))
            amplitudeCC[opcode.parameters.back()].value = normalizePercents(*value);
        break;
    case hash("pan"):
        if (auto value = readOpcode(opcode.value, Default::panRange))
            pan = normalizePercents(*value);
        break;
    case hash("pan_curvecc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::curveCCRange))
            panCC[opcode.parameters.back()].curve = *value;
        break;
    case hash("pan_stepcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::stepCCRange))
            panCC[opcode.parameters.back()].steps = *value;
        break;
    case hash("pan_smoothcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::smoothCCRange))
            panCC[opcode.parameters.back()].smooth = *value;
        break;
    case hash("pan_cc&"):
    case hash("pan_oncc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::panCCRange))
            panCC[opcode.parameters.back()].value = normalizePercents(*value);
        break;
    case hash("position"):
        if (auto value = readOpcode(opcode.value, Default::positionRange))
            position = normalizePercents(*value);
        break;
    case hash("position_curvecc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::curveCCRange))
            positionCC[opcode.parameters.back()].curve = *value;
        break;
    case hash("position_stepcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::stepCCRange))
            positionCC[opcode.parameters.back()].steps = *value;
        break;
    case hash("position_smoothcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::smoothCCRange))
            positionCC[opcode.parameters.back()].smooth = *value;
        break;
    case hash("position_cc&"): // fallthrough
    case hash("position_oncc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::positionCCRange))
            positionCC[opcode.parameters.back()].value = normalizePercents(*value);
        break;
    case hash("width"):
        if (auto value = readOpcode(opcode.value, Default::widthRange))
            width = normalizePercents(*value);
        break;
    case hash("width_curvecc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::curveCCRange))
            widthCC[opcode.parameters.back()].curve = *value;
        break;
    case hash("width_stepcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::stepCCRange))
            widthCC[opcode.parameters.back()].steps = *value;
        break;
    case hash("width_smoothcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::smoothCCRange))
            widthCC[opcode.parameters.back()].smooth = *value;
        break;
    case hash("width_oncc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::widthCCRange))
            widthCC[opcode.parameters.back()].value = normalizePercents(*value);
        break;
    case hash("amp_keycenter"):
        setValueFromOpcode(opcode, ampKeycenter, Default::keyRange);
        break;
    case hash("amp_keytrack"):
        setValueFromOpcode(opcode, ampKeytrack, Default::ampKeytrackRange);
        break;
    case hash("amp_veltrack"):
        setValueFromOpcode(opcode, ampVeltrack, Default::ampVeltrackRange);
        break;
    case hash("amp_random"):
        setValueFromOpcode(opcode, ampRandom, Default::ampRandomRange);
        break;
    case hash("amp_velcurve_&"):
        {
            auto value = readOpcode(opcode.value, Default::ampVelcurveRange);
            if (opcode.parameters.back() > 127)
                return false;

            if (value)
                velocityPoints.emplace_back(normalizeVelocity(opcode.parameters.back()), *value);
        }
        break;
    case hash("xfin_lokey"):
        setRangeStartFromOpcode(opcode, crossfadeKeyInRange, Default::keyRange);
        break;
    case hash("xfin_hikey"):
        setRangeEndFromOpcode(opcode, crossfadeKeyInRange, Default::keyRange);
        break;
    case hash("xfout_lokey"):
        setRangeStartFromOpcode(opcode, crossfadeKeyOutRange, Default::keyRange);
        break;
    case hash("xfout_hikey"):
        setRangeEndFromOpcode(opcode, crossfadeKeyOutRange, Default::keyRange);
        break;
    case hash("xfin_lovel"):
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            crossfadeVelInRange.setStart(normalizeVelocity(*value));
        break;
    case hash("xfin_hivel"):
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            crossfadeVelInRange.setEnd(normalizeVelocity(*value));
        break;
    case hash("xfout_lovel"):
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            crossfadeVelOutRange.setStart(normalizeVelocity(*value));
        break;
    case hash("xfout_hivel"):
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            crossfadeVelOutRange.setEnd(normalizeVelocity(*value));
        break;
    case hash("xf_keycurve"):
        switch (hash(opcode.value)) {
        case hash("power"):
            crossfadeKeyCurve = SfzCrossfadeCurve::power;
            break;
        case hash("gain"):
            crossfadeKeyCurve = SfzCrossfadeCurve::gain;
            break;
        default:
            DBG("Unknown crossfade power curve: " << std::string(opcode.value));
        }
        break;
    case hash("xf_velcurve"):
        switch (hash(opcode.value)) {
        case hash("power"):
            crossfadeVelCurve = SfzCrossfadeCurve::power;
            break;
        case hash("gain"):
            crossfadeVelCurve = SfzCrossfadeCurve::gain;
            break;
        default:
            DBG("Unknown crossfade power curve: " << std::string(opcode.value));
        }
        break;
    case hash("xfin_locc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            crossfadeCCInRange[opcode.parameters.back()].setStart(normalizeCC(*value));
        break;
    case hash("xfin_hicc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            crossfadeCCInRange[opcode.parameters.back()].setEnd(normalizeCC(*value));
        break;
    case hash("xfout_locc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            crossfadeCCOutRange[opcode.parameters.back()].setStart(normalizeCC(*value));
        break;
    case hash("xfout_hicc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::midi7Range))
            crossfadeCCOutRange[opcode.parameters.back()].setEnd(normalizeCC(*value));
        break;
    case hash("xf_cccurve"):
        switch (hash(opcode.value)) {
        case hash("power"):
            crossfadeCCCurve = SfzCrossfadeCurve::power;
            break;
        case hash("gain"):
            crossfadeCCCurve = SfzCrossfadeCurve::gain;
            break;
        default:
            DBG("Unknown crossfade power curve: " << std::string(opcode.value));
        }
        break;
    case hash("rt_decay"):
        setValueFromOpcode(opcode, rtDecay, Default::rtDecayRange);
        break;

    // Performance parameters: filters
    case hash("cutoff"): // fallthrough
    case hash("cutoff&"):
        {
            const auto filterIndex = opcode.parameters.empty() ? 0 : (opcode.parameters.back() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;
            setValueFromOpcode(opcode, filters[filterIndex].cutoff, Default::filterCutoffRange);
        }
        break;
    case hash("resonance"): // fallthrough
    case hash("resonance&"):
        {
            const auto filterIndex = opcode.parameters.empty() ? 0 : (opcode.parameters.back() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;
            setValueFromOpcode(opcode, filters[filterIndex].resonance, Default::filterResonanceRange);
        }
        break;
    case hash("cutoff_oncc&"):
    case hash("cutoff_cc&"):
    case hash("cutoff&_oncc&"): // fallthrough
    case hash("cutoff&_cc&"):
        {
            const auto filterIndex = opcode.parameters.size() == 1 ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            setValueFromOpcode(
                opcode,
                filters[filterIndex].cutoffCC[opcode.parameters.back()],
                Default::filterCutoffModRange
            );
        }
        break;
    case hash("resonance&_oncc&"):
    case hash("resonance&_cc&"):
    case hash("resonance_oncc&"): // fallthrough
    case hash("resonance_cc&"):
        {
            const auto filterIndex = opcode.parameters.size() == 1 ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            setValueFromOpcode(
                opcode,
                filters[filterIndex].resonanceCC[opcode.parameters.back()],
                Default::filterResonanceModRange
            );
        }
        break;
    case hash("fil_keytrack"): // fallthrough
    case hash("fil&_keytrack"):
        {
            const auto filterIndex = opcode.parameters.empty() ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            setValueFromOpcode(opcode, filters[filterIndex].keytrack, Default::filterKeytrackRange);
        }
        break;
    case hash("fil_keycenter"): // fallthrough
    case hash("fil&_keycenter"):
        {
            const auto filterIndex = opcode.parameters.empty() ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            setValueFromOpcode(opcode, filters[filterIndex].keycenter, Default::keyRange);
        }
        break;
    case hash("fil_veltrack"): // fallthrough
    case hash("fil&_veltrack"):
        {
            const auto filterIndex = opcode.parameters.empty() ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            setValueFromOpcode(opcode, filters[filterIndex].veltrack, Default::filterVeltrackRange);
        }
        break;
    case hash("fil_random"): // fallthrough
    case hash("fil&_random"):
        {
            const auto filterIndex = opcode.parameters.empty() ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            setValueFromOpcode(opcode, filters[filterIndex].random, Default::filterRandomRange);
        }
        break;
    case hash("fil_gain"): // fallthrough
    case hash("fil&_gain"):
        {
            const auto filterIndex = opcode.parameters.empty() ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            setValueFromOpcode(opcode, filters[filterIndex].gain, Default::filterGainRange);
        }
        break;
    case hash("fil_gaincc&"): // fallthrough
    case hash("fil&_gaincc&"):
        {
            const auto filterIndex = opcode.parameters.size() == 1 ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            setValueFromOpcode(
                opcode,
                filters[filterIndex].gainCC[opcode.parameters.back()],
                Default::filterGainModRange
            );
        }
        break;
    case hash("fil_type"): // fallthrough
    case hash("fil&_type"):
        {
            const auto filterIndex = opcode.parameters.empty() ? 0 : (opcode.parameters.front() - 1);
            if (!extendIfNecessary(filters, filterIndex + 1, Default::numFilters))
                return false;

            absl::optional<FilterType> ftype = Filter::typeFromName(opcode.value);

            if (ftype)
                filters[filterIndex].type = *ftype;
            else {
                filters[filterIndex].type = FilterType::kFilterNone;
                DBG("Unknown filter type: " << std::string(opcode.value));
            }
        }
        break;

    // Performance parameters: EQ
    case hash("eq&_bw"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;
            setValueFromOpcode(opcode, equalizers[eqNumber - 1].bandwidth, Default::eqBandwidthRange);
        }
        break;
    case hash("eq&_bw_oncc&"): // fallthrough
    case hash("eq&_bwcc&"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;

            setValueFromOpcode(opcode, equalizers[eqNumber - 1].bandwidthCC[opcode.parameters.back()], Default::eqBandwidthModRange);
        }
        break;
    case hash("eq&_freq"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;
            setValueFromOpcode(opcode, equalizers[eqNumber - 1].frequency, Default::eqFrequencyRange);
        }
        break;
    case hash("eq&_freq_oncc&"): // fallthrough
    case hash("eq&_freqcc&"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;

            setValueFromOpcode(opcode, equalizers[eqNumber - 1].frequencyCC[opcode.parameters.back()], Default::eqFrequencyModRange);
        }
        break;
    case hash("eq&_vel&freq"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (opcode.parameters[1] != 2)
                return false; // was eqN_vel3freq or something else than eqN_vel2freq
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;

            setValueFromOpcode(opcode, equalizers[eqNumber - 1].vel2frequency, Default::eqFrequencyModRange);
        }
        break;
    case hash("eq&_gain"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;
            setValueFromOpcode(opcode, equalizers[eqNumber - 1].gain, Default::eqGainRange);
        }
        break;
    case hash("eq&_gain_oncc&"): // fallthrough
    case hash("eq&_gaincc&"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;

            setValueFromOpcode(opcode, equalizers[eqNumber - 1].gainCC[opcode.parameters.back()], Default::eqGainModRange);
        }
        break;
    case hash("eq&_vel&gain"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (opcode.parameters[1] != 2)
                return false;  // was eqN_vel3gain or something else than eqN_vel2gain
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;

            setValueFromOpcode(opcode, equalizers[eqNumber - 1].vel2gain, Default::eqGainModRange);
        }
        break;
    case hash("eq&_type"):
        {
            const auto eqNumber = opcode.parameters.front();
            if (eqNumber == 0)
                return false;
            if (!extendIfNecessary(equalizers, eqNumber, Default::numEQs))
                return false;

            absl::optional<EqType> ftype = FilterEq::typeFromName(opcode.value);

            if (ftype)
                equalizers[eqNumber - 1].type = *ftype;
            else {
                equalizers[eqNumber - 1].type = EqType::kEqNone;
                DBG("Unknown EQ type: " << std::string(opcode.value));
            }
        }
        break;

    // Performance parameters: pitch
    case hash("pitch_keycenter"):
        setValueFromOpcode(opcode, pitchKeycenter, Default::keyRange);
        break;
    case hash("pitch_keytrack"):
        setValueFromOpcode(opcode, pitchKeytrack, Default::pitchKeytrackRange);
        break;
    case hash("pitch_veltrack"):
        setValueFromOpcode(opcode, pitchVeltrack, Default::pitchVeltrackRange);
        break;
    case hash("pitch_random"):
        setValueFromOpcode(opcode, pitchRandom, Default::pitchRandomRange);
        break;
    case hash("transpose"):
        setValueFromOpcode(opcode, transpose, Default::transposeRange);
        break;
    case hash("tune"): // fallthrough
    case hash("pitch"):
        setValueFromOpcode(opcode, tune, Default::tuneRange);
        break;
    case hash("pitch_curvecc&"): // fallthrough
    case hash("tune_curvecc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::curveCCRange))
            tuneCC[opcode.parameters.back()].curve = *value;
        break;
    case hash("pitch_stepcc&"): // fallthrough
    case hash("tune_stepcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::stepCCRange))
            tuneCC[opcode.parameters.back()].steps = *value;
        break;
    case hash("pitch_smoothcc&"): // fallthrough
    case hash("tune_smoothcc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::smoothCCRange))
            tuneCC[opcode.parameters.back()].smooth = *value;
        break;
    case hash("tune_cc&"):
    case hash("tune_oncc&"):
    case hash("pitch_cc&"):
    case hash("pitch_oncc&"):
        if (opcode.parameters.back() > config::numCCs)
            return false;
        if (auto value = readOpcode(opcode.value, Default::tuneCCRange))
            tuneCC[opcode.parameters.back()].value = *value;
        break;
    case hash("bend_up"):
        setValueFromOpcode(opcode, bendUp, Default::bendBoundRange);
        break;
    case hash("bend_down"):
        setValueFromOpcode(opcode, bendDown, Default::bendBoundRange);
        break;
    case hash("bend_step"):
        setValueFromOpcode(opcode, bendStep, Default::bendStepRange);
        break;

    // Amplitude Envelope
    case hash("ampeg_attack"):
        setValueFromOpcode(opcode, amplitudeEG.attack, Default::egTimeRange);
        break;
    case hash("ampeg_decay"):
        setValueFromOpcode(opcode, amplitudeEG.decay, Default::egTimeRange);
        break;
    case hash("ampeg_delay"):
        setValueFromOpcode(opcode, amplitudeEG.delay, Default::egTimeRange);
        break;
    case hash("ampeg_hold"):
        setValueFromOpcode(opcode, amplitudeEG.hold, Default::egTimeRange);
        break;
    case hash("ampeg_release"):
        setValueFromOpcode(opcode, amplitudeEG.release, Default::egTimeRange);
        break;
    case hash("ampeg_start"):
        setValueFromOpcode(opcode, amplitudeEG.start, Default::egPercentRange);
        break;
    case hash("ampeg_sustain"):
        setValueFromOpcode(opcode, amplitudeEG.sustain, Default::egPercentRange);
        break;
    case hash("ampeg_vel&attack"):
        if (opcode.parameters.front() != 2)
            return false; // Was not vel2...
        setValueFromOpcode(opcode, amplitudeEG.vel2attack, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_vel&decay"):
        if (opcode.parameters.front() != 2)
            return false; // Was not vel2...
        setValueFromOpcode(opcode, amplitudeEG.vel2decay, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_vel&delay"):
        if (opcode.parameters.front() != 2)
            return false; // Was not vel2...
        setValueFromOpcode(opcode, amplitudeEG.vel2delay, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_vel&hold"):
        if (opcode.parameters.front() != 2)
            return false; // Was not vel2...
        setValueFromOpcode(opcode, amplitudeEG.vel2hold, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_vel&release"):
        if (opcode.parameters.front() != 2)
            return false; // Was not vel2...
        setValueFromOpcode(opcode, amplitudeEG.vel2release, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_vel&sustain"):
        if (opcode.parameters.front() != 2)
            return false; // Was not vel2...
        setValueFromOpcode(opcode, amplitudeEG.vel2sustain, Default::egOnCCPercentRange);
        break;
    case hash("ampeg_attackcc&"): // fallthrough
    case hash("ampeg_attack_oncc&"):
        setCCPairFromOpcode(opcode, amplitudeEG.ccAttack, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_decaycc&"): // fallthrough
    case hash("ampeg_decay_oncc&"):
        setCCPairFromOpcode(opcode, amplitudeEG.ccDecay, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_delaycc&"): // fallthrough
    case hash("ampeg_delay_oncc&"):
        setCCPairFromOpcode(opcode, amplitudeEG.ccDelay, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_holdcc&"): // fallthrough
    case hash("ampeg_hold_oncc&"):
        setCCPairFromOpcode(opcode, amplitudeEG.ccHold, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_releasecc&"): // fallthrough
    case hash("ampeg_release_oncc&"):
        setCCPairFromOpcode(opcode, amplitudeEG.ccRelease, Default::egOnCCTimeRange);
        break;
    case hash("ampeg_startcc&"): // fallthrough
    case hash("ampeg_start_oncc&"):
        setCCPairFromOpcode(opcode, amplitudeEG.ccStart, Default::egOnCCPercentRange);
        break;
    case hash("ampeg_sustaincc&"): // fallthrough
    case hash("ampeg_sustain_oncc&"):
        setCCPairFromOpcode(opcode, amplitudeEG.ccSustain, Default::egOnCCPercentRange);
        break;

    case hash("effect&"):
    {
        const auto effectNumber = opcode.parameters.back();
        if (!effectNumber || effectNumber < 1 || effectNumber > config::maxEffectBuses)
            break;
        auto value = readOpcode<float>(opcode.value, { 0, 100 });
        if (!value)
            break;
        if (static_cast<size_t>(effectNumber + 1) > gainToEffect.size())
            gainToEffect.resize(effectNumber + 1);
        gainToEffect[effectNumber] = *value / 100;
        break;
    }

    // Ignored opcodes
    case hash("hichan"):
    case hash("lochan"):
    case hash("ampeg_depth"):
    case hash("ampeg_vel&depth"):
        break;
    default:
        return false;
    }

    return true;
}

bool sfz::Region::isSwitchedOn() const noexcept
{
    return keySwitched && previousKeySwitched && sequenceSwitched && pitchSwitched && bpmSwitched && aftertouchSwitched && ccSwitched.all();
}

bool sfz::Region::registerNoteOn(int noteNumber, float velocity, float randValue) noexcept
{
    ASSERT(velocity >= 0.0f && velocity <= 1.0f);

    if (keyswitchRange.containsWithEnd(noteNumber)) {
        if (keyswitch) {
            if (*keyswitch == noteNumber)
                keySwitched = true;
            else
                keySwitched = false;
        }

        if (keyswitchDown && *keyswitchDown == noteNumber)
            keySwitched = true;

        if (keyswitchUp && *keyswitchUp == noteNumber)
            keySwitched = false;
    }

    const bool keyOk = keyRange.containsWithEnd(noteNumber);
    if (keyOk) {
        // Sequence activation
        sequenceCounter += 1;
        if ((sequenceCounter % sequenceLength) == sequencePosition - 1)
            sequenceSwitched = true;
        else
            sequenceSwitched = false;

        if (previousNote) {
            if (*previousNote == noteNumber)
                previousKeySwitched = true;
            else
                previousKeySwitched = false;
        }
    }

    if (!isSwitchedOn())
        return false;

    if (triggerOnCC)
        return false;

    if (previousNote && !(previousKeySwitched && noteNumber != *previousNote))
        return false;

    const bool velOk = velocityRange.containsWithEnd(velocity);
    const bool randOk = randRange.contains(randValue) || (randValue == 1.0f && randRange.getEnd() == 1.0f);
    const bool firstLegatoNote = (trigger == SfzTrigger::first && midiState.getActiveNotes() == 1);
    const bool attackTrigger = (trigger == SfzTrigger::attack);
    const bool notFirstLegatoNote = (trigger == SfzTrigger::legato && midiState.getActiveNotes() > 1);

    return keyOk && velOk && randOk && (attackTrigger || firstLegatoNote || notFirstLegatoNote);
}

bool sfz::Region::registerNoteOff(int noteNumber, float velocity, float randValue) noexcept
{
    ASSERT(velocity >= 0.0f && velocity <= 1.0f);

    if (keyswitchRange.containsWithEnd(noteNumber)) {
        if (keyswitchDown && *keyswitchDown == noteNumber)
            keySwitched = false;

        if (keyswitchUp && *keyswitchUp == noteNumber)
            keySwitched = true;
    }

    const bool keyOk = keyRange.containsWithEnd(noteNumber);

    if (!isSwitchedOn())
        return false;

    if (triggerOnCC)
        return false;

    const bool velOk = velocityRange.containsWithEnd(velocity);
    const bool randOk = randRange.contains(randValue);
    const bool releaseTrigger = (trigger == SfzTrigger::release || trigger == SfzTrigger::release_key);
    return keyOk && velOk && randOk && releaseTrigger;
}

bool sfz::Region::registerCC(int ccNumber, float ccValue) noexcept
{
    ASSERT(ccValue >= 0.0f && ccValue <= 1.0f);
    if (ccConditions.getWithDefault(ccNumber).containsWithEnd(ccValue))
        ccSwitched.set(ccNumber, true);
    else
        ccSwitched.set(ccNumber, false);

    if (!isSwitchedOn())
        return false;

    if (!triggerOnCC)
        return false;

    if (ccTriggers.contains(ccNumber) && ccTriggers[ccNumber].containsWithEnd(ccValue))
        return true;
    else
        return false;
}

void sfz::Region::registerPitchWheel(float pitch) noexcept
{
    if (bendRange.containsWithEnd(pitch))
        pitchSwitched = true;
    else
        pitchSwitched = false;
}

void sfz::Region::registerAftertouch(uint8_t aftertouch) noexcept
{
    if (aftertouchRange.containsWithEnd(aftertouch))
        aftertouchSwitched = true;
    else
        aftertouchSwitched = false;
}

void sfz::Region::registerTempo(float secondsPerQuarter) noexcept
{
    const float bpm = 60.0f / secondsPerQuarter;
    if (bpmRange.containsWithEnd(bpm))
        bpmSwitched = true;
    else
        bpmSwitched = false;
}

float sfz::Region::getBasePitchVariation(int noteNumber, float velocity) const noexcept
{
    ASSERT(velocity >= 0.0f && velocity <= 1.0f);

    std::uniform_int_distribution<int> pitchDistribution { -pitchRandom, pitchRandom };
    auto pitchVariationInCents = pitchKeytrack * (noteNumber - (int)pitchKeycenter); // note difference with pitch center
    pitchVariationInCents += tune; // sample tuning
    pitchVariationInCents += config::centPerSemitone * transpose; // sample transpose
    pitchVariationInCents += static_cast<int>(velocity) * pitchVeltrack; // track velocity
    pitchVariationInCents += pitchDistribution(Random::randomGenerator); // random pitch changes
    return centsFactor(pitchVariationInCents);
}

float sfz::Region::getBaseVolumedB(int noteNumber) const noexcept
{
    std::uniform_real_distribution<float> volumeDistribution { -ampRandom, ampRandom };
    auto baseVolumedB = volume + volumeDistribution(Random::randomGenerator);
    if (trigger == SfzTrigger::release || trigger == SfzTrigger::release_key)
        baseVolumedB -= rtDecay * midiState.getNoteDuration(noteNumber);
    return baseVolumedB;
}

float sfz::Region::getBaseGain() const noexcept
{
    return amplitude;
}

float sfz::Region::getPhase() const noexcept
{
    float phase;
    if (oscillatorPhase >= 0) {
        phase = oscillatorPhase * (1.0f / 360.0f);
        phase -= static_cast<float>(static_cast<int>(phase));
    } else {
        std::uniform_real_distribution<float> phaseDist { 0.0001f, 0.9999f };
        phase = phaseDist(Random::randomGenerator);
    }
    return phase;
}

uint32_t sfz::Region::getOffset(Oversampling factor) const noexcept
{
    std::uniform_int_distribution<uint32_t> offsetDistribution { 0, offsetRandom };
    return (offset + offsetDistribution(Random::randomGenerator)) * static_cast<uint32_t>(factor);
}

float sfz::Region::getDelay() const noexcept
{
    std::uniform_real_distribution<float> delayDistribution { 0, delayRandom };
    return delay + delayDistribution(Random::randomGenerator);
}

uint32_t sfz::Region::trueSampleEnd(Oversampling factor) const noexcept
{
    return min(sampleEnd, loopRange.getEnd()) * static_cast<uint32_t>(factor);
}

uint32_t sfz::Region::loopStart(Oversampling factor) const noexcept
{
    return loopRange.getStart() * static_cast<uint32_t>(factor);
}

uint32_t sfz::Region::loopEnd(Oversampling factor) const noexcept
{
    return loopRange.getEnd() * static_cast<uint32_t>(factor);
}

float sfz::Region::getNoteGain(int noteNumber, float velocity) const noexcept
{
    ASSERT(velocity >= 0.0f && velocity <= 1.0f);

    float baseGain { 1.0f };

    // Amplitude key tracking
    baseGain *= db2mag(ampKeytrack * static_cast<float>(noteNumber - ampKeycenter));

    // Crossfades related to the note number
    baseGain *= crossfadeIn(crossfadeKeyInRange, noteNumber, crossfadeKeyCurve);
    baseGain *= crossfadeOut(crossfadeKeyOutRange, noteNumber, crossfadeKeyCurve);

    // Amplitude velocity tracking
    baseGain *= velocityCurve(velocity);

    // Crossfades related to velocity
    baseGain *= crossfadeIn(crossfadeVelInRange, velocity, crossfadeVelCurve);
    baseGain *= crossfadeOut(crossfadeVelOutRange, velocity, crossfadeVelCurve);

    return baseGain;
}

float sfz::Region::getCrossfadeGain() const noexcept
{
    float gain { 1.0f };

    // Crossfades due to CC states
    for (const auto& ccData : crossfadeCCInRange) {
        const auto ccValue = midiState.getCCValue(ccData.cc);
        const auto crossfadeRange = ccData.data;
        gain *= crossfadeIn(crossfadeRange, ccValue, crossfadeCCCurve);
    }

    for (const auto& ccData : crossfadeCCOutRange) {
        const auto ccValue = midiState.getCCValue(ccData.cc);
        const auto crossfadeRange = ccData.data;
        gain *= crossfadeOut(crossfadeRange, ccValue, crossfadeCCCurve);
    }

    return gain;
}

float sfz::Region::velocityCurve(float velocity) const noexcept
{
    ASSERT(velocity >= 0.0f && velocity <= 1.0f);

    float gain { 1.0f };
    if (velocityPoints.size() > 0) { // Custom velocity curve
        auto after = absl::c_find_if(velocityPoints, [velocity](const std::pair<float, float>& val) { return val.first >= velocity; });
        auto before = after == velocityPoints.begin() ? velocityPoints.begin() : after - 1;
        // Linear interpolation
        float relativePositionInSegment {
            (velocity - before->first) / (after->first - before->first)
        };
        float segmentEndpoints { after->second - before->second };
        gain *= relativePositionInSegment * segmentEndpoints;
    } else { // Standard velocity curve
        // FIXME: Maybe there's a prettier way to check the boundaries?
        const float gaindB = [&]() {
            if (ampVeltrack >= 0)
                return velocity == 0.0f ? -90.0f : 40 * std::log(velocity) / std::log(10.0f);
            else
                return velocity == 1.0f ? -90.0f : 40 * std::log(1 - velocity) / std::log(10.0f);
        }();
        gain *= db2mag( gaindB * std::abs(ampVeltrack) / sfz::Default::ampVeltrackRange.getEnd());
    }

    return gain;
}

uint8_t offsetAndClamp(uint8_t key, int offset, sfz::Range<uint8_t> range)
{
    const int offsetKey { key + offset };
    if (offsetKey > std::numeric_limits<uint8_t>::max())
        return range.getEnd();
    if (offsetKey < std::numeric_limits<uint8_t>::min())
        return range.getStart();

    return range.clamp(static_cast<uint8_t>(offsetKey));
}

void sfz::Region::offsetAllKeys(int offset) noexcept
{
    // Offset key range
    if (keyRange != Default::keyRange) {
        const auto start = keyRange.getStart();
        const auto end = keyRange.getEnd();
        keyRange.setStart(offsetAndClamp(start, offset, Default::keyRange));
        keyRange.setEnd(offsetAndClamp(end, offset, Default::keyRange));
    }
    pitchKeycenter = offsetAndClamp(pitchKeycenter, offset, Default::keyRange);

    // Offset key switches
    if (keyswitchRange != Default::keyRange) {
        const auto start = keyswitchRange.getStart();
        const auto end = keyswitchRange.getEnd();
        keyswitchRange.setStart(offsetAndClamp(start, offset, Default::keyRange));
        keyswitchRange.setEnd(offsetAndClamp(end, offset, Default::keyRange));
    }
    if (keyswitchUp)
        keyswitchUp = offsetAndClamp(*keyswitchUp, offset, Default::keyRange);
    if (keyswitch)
        keyswitch = offsetAndClamp(*keyswitch, offset, Default::keyRange);
    if (keyswitchDown)
        keyswitchDown = offsetAndClamp(*keyswitchDown, offset, Default::keyRange);
    if (previousNote)
        previousNote = offsetAndClamp(*previousNote, offset, Default::keyRange);

    // Offset crossfade ranges
    if (crossfadeKeyInRange != Default::crossfadeKeyInRange) {
        const auto start = crossfadeKeyInRange.getStart();
        const auto end = crossfadeKeyInRange.getEnd();
        crossfadeKeyInRange.setStart(offsetAndClamp(start, offset, Default::keyRange));
        crossfadeKeyInRange.setEnd(offsetAndClamp(end, offset, Default::keyRange));
    }

    if (crossfadeKeyOutRange != Default::crossfadeKeyOutRange) {
        const auto start = crossfadeKeyOutRange.getStart();
        const auto end = crossfadeKeyOutRange.getEnd();
        crossfadeKeyOutRange.setStart(offsetAndClamp(start, offset, Default::keyRange));
        crossfadeKeyOutRange.setEnd(offsetAndClamp(end, offset, Default::keyRange));
    }
}

float sfz::Region::getGainToEffectBus(unsigned number) const noexcept
{
    if (number >= gainToEffect.size())
        return 0.0;

    return gainToEffect[number];
}
