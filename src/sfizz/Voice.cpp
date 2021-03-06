// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "Voice.h"
#include "AudioSpan.h"
#include "Config.h"
#include "Defaults.h"
#include "MathHelpers.h"
#include "SIMDHelpers.h"
#include "SfzHelpers.h"
#include "absl/algorithm/container.h"
#include <memory>

sfz::Voice::Voice(const sfz::MidiState& midiState, sfz::Resources& resources)
    : midiState(midiState), resources(resources)
{
}

void sfz::Voice::startVoice(Region* region, int delay, int number, uint8_t value, sfz::Voice::TriggerType triggerType) noexcept
{
    this->triggerType = triggerType;
    triggerNumber = number;
    triggerValue = value;

    this->region = region;
    state = State::playing;

    ASSERT(delay >= 0);
    if (delay < 0)
        delay = 0;

    if (!region->isGenerator()) {
        currentPromise = resources.filePool.getFilePromise(region->sample);
        if (currentPromise == nullptr) {
            reset();
            return;
        }
        speedRatio = static_cast<float>(currentPromise->sampleRate / this->sampleRate);
    }
    pitchRatio = region->getBasePitchVariation(number, value);

    baseVolumedB = region->getBaseVolumedB(number);
    auto volumedB { baseVolumedB };
    if (region->volumeCC)
        volumedB += normalizeCC(midiState.getCCValue(region->volumeCC->first)) * region->volumeCC->second;
    volumeEnvelope.reset(db2mag(Default::volumeRange.clamp(volumedB)));

    baseGain = region->getBaseGain();
    if (triggerType != TriggerType::CC)
        baseGain *= region->getNoteGain(number, value);

    float gain { baseGain };
    if (region->amplitudeCC)
        gain *= normalizeCC(midiState.getCCValue(region->amplitudeCC->first)) * normalizePercents(region->amplitudeCC->second);
    amplitudeEnvelope.reset(Default::normalizedRange.clamp(gain));

    float crossfadeGain { region->getCrossfadeGain(midiState.getCCArray()) };
    crossfadeEnvelope.reset(Default::normalizedRange.clamp(crossfadeGain));

    basePan = normalizeNegativePercents(region->pan);
    auto pan { basePan };
    if (region->panCC)
        pan += normalizeCC(midiState.getCCValue(region->panCC->first)) * normalizeNegativePercents(region->panCC->second);
    panEnvelope.reset(Default::symmetricNormalizedRange.clamp(pan));

    basePosition = normalizeNegativePercents(region->position);
    auto position { basePosition };
    if (region->positionCC)
        position += normalizeCC(midiState.getCCValue(region->positionCC->first)) * normalizeNegativePercents(region->positionCC->second);
    positionEnvelope.reset(Default::symmetricNormalizedRange.clamp(position));

    baseWidth = normalizeNegativePercents(region->width);
    auto width { baseWidth };
    if (region->widthCC)
        width += normalizeCC(midiState.getCCValue(region->widthCC->first)) * normalizeNegativePercents(region->widthCC->second);
    widthEnvelope.reset(Default::symmetricNormalizedRange.clamp(width));

    pitchBendEnvelope.setFunction([region](float pitchValue){
        const auto normalizedBend = normalizeBend(pitchValue);
        const auto bendInCents = normalizedBend > 0.0f ? normalizedBend * region->bendUp : -normalizedBend * region->bendDown;
        return centsFactor(bendInCents);
    });
    pitchBendEnvelope.reset(static_cast<float>(midiState.getPitchBend()));

    sourcePosition = region->getOffset();
    triggerDelay = delay;
    initialDelay = delay + static_cast<uint32_t>(region->getDelay() * sampleRate);
    baseFrequency = midiNoteFrequency(number);
    bendStepFactor = centsFactor(region->bendStep);
    prepareEGEnvelope(initialDelay, value);
}

void sfz::Voice::prepareEGEnvelope(int delay, uint8_t velocity) noexcept
{
    auto secondsToSamples = [this](auto timeInSeconds) {
        return static_cast<int>(timeInSeconds * sampleRate);
    };
    const auto& ccArray = midiState.getCCArray();
    egEnvelope.reset(
        secondsToSamples(region->amplitudeEG.getAttack(ccArray, velocity)),
        secondsToSamples(region->amplitudeEG.getRelease(ccArray, velocity)),
        normalizePercents(region->amplitudeEG.getSustain(ccArray, velocity)),
        delay + secondsToSamples(region->amplitudeEG.getDelay(ccArray, velocity)),
        secondsToSamples(region->amplitudeEG.getDecay(ccArray, velocity)),
        secondsToSamples(region->amplitudeEG.getHold(ccArray, velocity)),
        normalizePercents(region->amplitudeEG.getStart(ccArray, velocity)));
}

bool sfz::Voice::isFree() const noexcept
{
    return (state == State::idle);
}

void sfz::Voice::release(int delay, bool fastRelease) noexcept
{
    if (state != State::playing)
        return;

    if (egEnvelope.getRemainingDelay() > std::max(0, delay - initialDelay)) {
        reset();
    } else {
        state = State::release;
        egEnvelope.startRelease(delay, fastRelease);
    }
}

void sfz::Voice::registerNoteOff(int delay, int noteNumber, uint8_t velocity [[maybe_unused]]) noexcept
{
    if (region == nullptr)
        return;

    if (state != State::playing)
        return;

    if (triggerNumber == noteNumber) {
        noteIsOff = true;

        if (region->loopMode == SfzLoopMode::one_shot)
            return;

        if (!region->checkSustain || midiState.getCCValue(config::sustainCC) < config::halfCCThreshold)
            release(delay);
    }
}

void sfz::Voice::registerCC(int delay, int ccNumber, uint8_t ccValue) noexcept
{
    if (region == nullptr)
        return;

    if (state ==  State::idle)
        return;

    if (ccNumber == config::allNotesOffCC || ccNumber == config::allSoundOffCC) {
        reset();
        return;
    }

    if (region->checkSustain && noteIsOff && ccNumber == config::sustainCC && ccValue < config::halfCCThreshold)
        release(delay);

    // Add a minimum delay for smoothing the envelopes
    // TODO: this feels like a hack, revisit this along with the smoothed envelopes...
    delay = max(delay, minEnvelopeDelay);

    if (region->amplitudeCC && ccNumber == region->amplitudeCC->first) {
        const float newGain { baseGain * normalizeCC(ccValue) * normalizePercents(region->amplitudeCC->second) };
        amplitudeEnvelope.registerEvent(delay, Default::normalizedRange.clamp(newGain));
    }

    if (region->volumeCC && ccNumber == region->volumeCC->first) {
        const float newVolumedB { baseVolumedB + normalizeCC(ccValue) * region->volumeCC->second };
        volumeEnvelope.registerEvent(delay, db2mag(Default::volumeRange.clamp(newVolumedB)));
    }

    if (region->panCC && ccNumber == region->panCC->first) {
        const float newPan { basePan + normalizeCC(ccValue) * normalizeNegativePercents(region->panCC->second) };
        panEnvelope.registerEvent(delay, Default::symmetricNormalizedRange.clamp(newPan));
    }

    if (region->positionCC && ccNumber == region->positionCC->first) {
        const float newPosition { basePosition + normalizeCC(ccValue) * normalizeNegativePercents(region->positionCC->second) };
        positionEnvelope.registerEvent(delay, Default::symmetricNormalizedRange.clamp(newPosition));
    }

    if (region->widthCC && ccNumber == region->widthCC->first) {
        const float newWidth { baseWidth + normalizeCC(ccValue) * normalizeNegativePercents(region->widthCC->second) };
        widthEnvelope.registerEvent(delay, Default::symmetricNormalizedRange.clamp(newWidth));
    }

    if (region->crossfadeCCInRange.contains(ccNumber) || region->crossfadeCCOutRange.contains(ccNumber)) {
        const float crossfadeGain = region->getCrossfadeGain(midiState.getCCArray());
        crossfadeEnvelope.registerEvent(delay, Default::normalizedRange.clamp(crossfadeGain));
    }
}

void sfz::Voice::registerPitchWheel(int delay, int pitch) noexcept
{
    if (state == State::idle)
        return;

    pitchBendEnvelope.registerEvent(delay, static_cast<float>(pitch));
}

void sfz::Voice::registerAftertouch(int delay [[maybe_unused]], uint8_t aftertouch [[maybe_unused]]) noexcept
{
    // TODO
}

void sfz::Voice::registerTempo(int delay [[maybe_unused]], float secondsPerQuarter [[maybe_unused]]) noexcept
{
    // TODO
}

void sfz::Voice::setSampleRate(float sampleRate) noexcept
{
    this->sampleRate = sampleRate;
}

void sfz::Voice::setSamplesPerBlock(int samplesPerBlock) noexcept
{
    this->samplesPerBlock = samplesPerBlock;
    this->minEnvelopeDelay = samplesPerBlock / 2;
    tempBuffer1.resize(samplesPerBlock);
    tempBuffer2.resize(samplesPerBlock);
    tempBuffer3.resize(samplesPerBlock);
    indexBuffer.resize(samplesPerBlock);
    tempSpan1 = absl::MakeSpan(tempBuffer1);
    tempSpan2 = absl::MakeSpan(tempBuffer2);
    tempSpan3 = absl::MakeSpan(tempBuffer3);
    indexSpan = absl::MakeSpan(indexBuffer);
}

void sfz::Voice::renderBlock(AudioSpan<float> buffer) noexcept
{
    ASSERT(static_cast<int>(buffer.getNumFrames()) <= samplesPerBlock);
    buffer.fill(0.0f);

    if (state == State::idle || region == nullptr) {
        powerHistory.push(0.0f);
        return;
    }

    const auto delay = min(static_cast<size_t>(initialDelay), buffer.getNumFrames());
    auto delayed_buffer = buffer.subspan(delay);
    initialDelay -= static_cast<int>(delay);

    if (region->isGenerator())
        fillWithGenerator(delayed_buffer);
    else
        fillWithData(delayed_buffer);

    if (region->isStereo)
        processStereo(buffer);
    else
        processMono(buffer);

    if (!egEnvelope.isSmoothing())
        reset();

    powerHistory.push(buffer.meanSquared());
    this->triggerDelay = absl::nullopt;
}

void sfz::Voice::processMono(AudioSpan<float> buffer) noexcept
{
    const auto numSamples = buffer.getNumFrames();
    auto leftBuffer = buffer.getSpan(0);
    auto rightBuffer = buffer.getSpan(1);

    auto span1 = tempSpan1.first(numSamples);
    auto span2 = tempSpan2.first(numSamples);

    // Amplitude envelope
    amplitudeEnvelope.getBlock(span1);
    applyGain<float>(span1, leftBuffer);

    // Crossfade envelope
    crossfadeEnvelope.getBlock(span1);
    applyGain<float>(span1, leftBuffer);

    // Volume envelope
    volumeEnvelope.getBlock(span1);
    applyGain<float>(span1, leftBuffer);

    // AmpEG envelope
    egEnvelope.getBlock(span1);
    applyGain<float>(span1, leftBuffer);

    // Prepare for stereo output
    copy<float>(leftBuffer, rightBuffer);

    panEnvelope.getBlock(span1);
    // We assume that the pan envelope is already normalized between -1 and 1
    // Check bm_pan for your architecture to check if it's interesting to use the pan helper instead
    fill<float>(span2, 1.0f);
    add<float>(span1, span2);
    applyGain<float>(piFour<float>, span2);
    cos<float>(span2, span1);
    sin<float>(span2, span2);
    applyGain<float>(span1, leftBuffer);
    applyGain<float>(span2, rightBuffer);
}

void sfz::Voice::processStereo(AudioSpan<float> buffer) noexcept
{
    const auto numSamples = buffer.getNumFrames();
    auto span1 = tempSpan1.first(numSamples);
    auto span2 = tempSpan2.first(numSamples);
    auto span3 = tempSpan3.first(numSamples);
    auto leftBuffer = buffer.getSpan(0);
    auto rightBuffer = buffer.getSpan(1);

    // Amplitude envelope
    amplitudeEnvelope.getBlock(span1);
    buffer.applyGain(span1);

    // Crossfade envelope
    crossfadeEnvelope.getBlock(span1);
    buffer.applyGain(span1);

    // Volume envelope
    volumeEnvelope.getBlock(span1);
    buffer.applyGain(span1);

    // AmpEG envelope
    egEnvelope.getBlock(span1);
    buffer.applyGain(span1);

    // Create mid/side from left/right in the output buffer
    copy<float>(rightBuffer, span1);
    add<float>(leftBuffer, rightBuffer);
    subtract<float>(span1, leftBuffer);
    applyGain<float>(sqrtTwoInv<float>, leftBuffer);
    applyGain<float>(sqrtTwoInv<float>, rightBuffer);

    // Apply the width process
    widthEnvelope.getBlock(span1);
    fill<float>(span2, 1.0f);
    add<float>(span1, span2);
    applyGain<float>(piFour<float>, span2);
    cos<float>(span2, span1);
    sin<float>(span2, span2);
    applyGain<float>(span1, leftBuffer);
    applyGain<float>(span2, rightBuffer);

    // Apply a position to the "left" channel which is supposed to be our mid channel
    // TODO: add panning here too?
    positionEnvelope.getBlock(span1);
    fill<float>(span2, 1.0f);
    add<float>(span1, span2);
    applyGain<float>(piFour<float>, span2);
    cos<float>(span2, span1);
    sin<float>(span2, span2);
    copy<float>(leftBuffer, span3);
    copy<float>(rightBuffer, leftBuffer);
    multiplyAdd<float>(span1, span3, leftBuffer);
    multiplyAdd<float>(span2, span3, rightBuffer);
    applyGain<float>(sqrtTwoInv<float>, leftBuffer);
    applyGain<float>(sqrtTwoInv<float>, rightBuffer);
}

void sfz::Voice::fillWithData(AudioSpan<float> buffer) noexcept
{
    if (buffer.getNumFrames() == 0)
        return;

    if (currentPromise == nullptr) {
        DBG("[Voice] Missing promise during fillWithData");
        return;
    }

    auto source = currentPromise->getData();
    auto indices = indexSpan.first(buffer.getNumFrames());
    auto jumps = tempSpan1.first(buffer.getNumFrames());
    auto bends = tempSpan2.first(buffer.getNumFrames());
    auto leftCoeffs = tempSpan1.first(buffer.getNumFrames());
    auto rightCoeffs = tempSpan2.first(buffer.getNumFrames());

    fill<float>(jumps, pitchRatio * speedRatio);
    if (region->bendStep > 1)
        pitchBendEnvelope.getQuantizedBlock(bends, bendStepFactor);
    else
        pitchBendEnvelope.getBlock(bends);

    applyGain<float>(bends, jumps);
    jumps[0] += floatPositionOffset;
    cumsum<float>(jumps, jumps);
    sfzInterpolationCast<float>(jumps, indices, leftCoeffs, rightCoeffs);
    add<int>(sourcePosition, indices);

    absl::optional<int> releaseAt {};

    if (region->shouldLoop() && region->loopEnd(currentPromise->oversamplingFactor) <= source.getNumFrames()) {
        const auto loopEnd = static_cast<int>(region->loopEnd(currentPromise->oversamplingFactor));
        const auto offset = loopEnd - static_cast<int>(region->loopStart(currentPromise->oversamplingFactor)) + 1;
        for (auto* index = indices.begin(); index < indices.end(); ++index) {
            if (*index > loopEnd) {
                const auto remainingElements = static_cast<size_t>(std::distance(index, indices.end()));
                subtract<int>(offset, { index, remainingElements });
            }
        }
    } else {
        const auto sampleEnd = min(
            static_cast<int>(region->trueSampleEnd(currentPromise->oversamplingFactor)),
            static_cast<int>(source.getNumFrames())
        ) - 2;
        for (auto* index = indices.begin(); index < indices.end(); ++index) {
            if (*index >= sampleEnd) {
                releaseAt = static_cast<int>(std::distance(indices.begin(), index));
                const auto remainingElements = static_cast<size_t>(std::distance(index, indices.end()));
                if (source.getNumFrames() != region->trueSampleEnd(currentPromise->oversamplingFactor)) {
                    DBG("[sfizz] Underflow: source available samples "
                        << source.getNumFrames() << "/"
                        << region->trueSampleEnd(currentPromise->oversamplingFactor)
                        << " for sample " << region->sample);
                }
                fill<int>(indices.last(remainingElements), sampleEnd);
                fill<float>(leftCoeffs.last(remainingElements), 0.0f);
                fill<float>(rightCoeffs.last(remainingElements), 1.0f);
                break;
            }
        }
    }

    auto ind = indices.data();
    auto leftCoeff = leftCoeffs.data();
    auto rightCoeff = rightCoeffs.data();
    auto leftSource = source.getConstSpan(0);
    auto left = buffer.getChannel(0);
    if (source.getNumChannels() == 1) {
        while (ind < indices.end()) {
            *left = linearInterpolation(leftSource[*ind], leftSource[*ind + 1], *leftCoeff, *rightCoeff);
            incrementAll(ind, left, leftCoeff, rightCoeff);
        }
    } else {
        auto right = buffer.getChannel(1);
        auto rightSource = source.getConstSpan(1);
        while (ind < indices.end()) {
            *left = linearInterpolation(leftSource[*ind], leftSource[*ind + 1], *leftCoeff, *rightCoeff);
            *right = linearInterpolation(rightSource[*ind], rightSource[*ind + 1], *leftCoeff, *rightCoeff);
            incrementAll(ind, left, right, leftCoeff, rightCoeff);
        }
    }

    sourcePosition = indices.back();
    floatPositionOffset = rightCoeffs.back();

    if (state != State::release && releaseAt) {
        release(*releaseAt);
        // TODO: not needed probably
        buffer.subspan(*releaseAt).fill(0.0f);
    }
}

void sfz::Voice::fillWithGenerator(AudioSpan<float> buffer) noexcept
{
    if (region->sample != "*sine")
        return;

    if (buffer.getNumFrames() == 0)
        return;

    auto jumps = tempSpan1.first(buffer.getNumFrames());
    auto bends = tempSpan2.first(buffer.getNumFrames());
    auto phases = tempSpan2.first(buffer.getNumFrames());

    const float step = baseFrequency * twoPi<float> / sampleRate;
    fill<float>(jumps, step);

    if (region->bendStep > 1)
        pitchBendEnvelope.getQuantizedBlock(bends, bendStepFactor);
    else
        pitchBendEnvelope.getBlock(bends);

    applyGain<float>(bends, jumps);
    jumps[0] += phase;
    cumsum<float>(jumps, phases);
    phase = phases.back();

    sin<float>(phases, buffer.getSpan(0));
    copy<float>(buffer.getSpan(0), buffer.getSpan(1));

    // Wrap the phase so we don't loose too much precision on longer notes
    const auto numTwoPiWraps = static_cast<int>(phase / twoPi<float>);
    phase -= twoPi<float> * static_cast<float>(numTwoPiWraps);
}

bool sfz::Voice::checkOffGroup(int delay, uint32_t group) noexcept
{
    if (region == nullptr)
        return false;

    if (delay <= this->triggerDelay)
        return false;

    if (triggerType == TriggerType::NoteOn && region->offBy == group) {
        release(delay, region->offMode == SfzOffMode::fast);
        return true;
    }

    return false;
}

int sfz::Voice::getTriggerNumber() const noexcept
{
    return triggerNumber;
}

uint8_t sfz::Voice::getTriggerValue() const noexcept
{
    return triggerValue;
}

sfz::Voice::TriggerType sfz::Voice::getTriggerType() const noexcept
{
    return triggerType;
}

void sfz::Voice::reset() noexcept
{
    state = State::idle;
    region = nullptr;
    currentPromise.reset();
    sourcePosition = 0;
    floatPositionOffset = 0.0f;
    noteIsOff = false;
}

float sfz::Voice::getMeanSquaredAverage() const noexcept
{
    return powerHistory.getAverage();
}

bool sfz::Voice::canBeStolen() const noexcept
{
    return state == State::release;
}

uint32_t sfz::Voice::getSourcePosition() const noexcept
{
    return sourcePosition;
}
