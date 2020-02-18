// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#pragma once
#include "Config.h"
#include "ADSREnvelope.h"
#include "EventEnvelopes.h"
#include "HistoricalBuffer.h"
#include "Region.h"
#include "AudioBuffer.h"
#include "MidiState.h"
#include "Resources.h"
#include "AudioSpan.h"
#include "LeakDetector.h"
#include <absl/types/span.h>
#include <atomic>
#include <memory>
#include <random>

namespace sfz {
/**
 * @brief The SFZ voice are the polyphony holders. They get activated by the synth
 * and tasked to play a given region until the end, stopping on note-offs, off-groups
 * or natural sample decay.
 *
 */
class Voice {
public:
    Voice() = delete;
    /**
     * @brief Construct a new voice with the midistate singleton
     *
     * @param midiState
     */
    Voice(Resources& resources);
    enum class TriggerType {
        NoteOn,
        NoteOff,
        CC
    };
    /**
     * @brief Change the sample rate of the voice. This is used to compute all
     * pitch related transformations so it needs to be propagated from the synth
     * at all times.
     *
     * @param sampleRate
     */
    void setSampleRate(float sampleRate) noexcept;
    /**
     * @brief Set the expected block size. If the block size is not fixed, set an
     * upper bound. The voice will adapt at each callback to the actual number of
     * samples requested but this function will allocate temporary buffers that are
     * needed for proper functioning.
     *
     * @param samplesPerBlock
     */
    void setSamplesPerBlock(int samplesPerBlock) noexcept;
    /**
     * @brief Get the sample rate of the voice.
     *
     * @return float
     */
    float getSampleRate() const noexcept { return sampleRate; }
    /**
     * @brief Get the expected block size.
     *
     * @return int
     */
    int getSamplesPerBlock() const noexcept { return samplesPerBlock; }

    /**
     * @brief Start playing a region after a short delay for different triggers (note on, off, cc)
     *
     * @param region
     * @param delay
     * @param number
     * @param value
     * @param triggerType
     */
    void startVoice(Region* region, int delay, int number, uint8_t value, TriggerType triggerType) noexcept;

    /**
     * @brief Register a note-off event; this may trigger a release.
     *
     * @param delay
     * @param noteNumber
     * @param velocity
     */
    void registerNoteOff(int delay, int noteNumber, uint8_t velocity) noexcept;
    /**
     * @brief Register a CC event; this may trigger a release. If the voice is playing and its
     * region has CC modifiers, it will use this value to compute the CC envelope to apply to the
     * parameter.
     *
     * @param delay
     * @param ccNumber
     * @param ccValue
     */
    void registerCC(int delay, int ccNumber, uint8_t ccValue) noexcept;
    /**
     * @brief Register a pitch wheel event; for now this does nothing
     *
     * @param delay
     * @param pitch
     */
    void registerPitchWheel(int delay, int pitch) noexcept;
    /**
     * @brief Register an aftertouch event; for now this does nothing
     *
     * @param delay
     * @param aftertouch
     */
    void registerAftertouch(int delay, uint8_t aftertouch) noexcept;
    /**
     * @brief Register a tempo event; for now this does nothing
     *
     * @param delay
     * @param pitch
     */
    void registerTempo(int delay, float secondsPerQuarter) noexcept;
    /**
     * @brief Checks if the voice should be offed by another starting in the group specified.
     * This will trigger the release if true.
     *
     * @param delay
     * @param group
     * @return true
     * @return false
     */
    bool checkOffGroup(int delay, uint32_t group) noexcept;

    /**
     * @brief Render a block of data for this voice into the span
     *
     * @param buffer
     */
    void renderBlock(AudioSpan<float, 2> buffer) noexcept;

    /**
     * @brief Is the voice free?
     *
     * @return true
     * @return false
     */
    bool isFree() const noexcept;
    /**
     * @brief Can the voice be "stolen" and reused (i.e. is it releasing)
     *
     * @return true
     * @return false
     */
    bool canBeStolen() const noexcept;
    /**
     * @brief Get the number that triggered the voice (note number or cc number)
     *
     * @return int
     */
    int getTriggerNumber() const noexcept;
    /**
     * @brief Get the value that triggered the voice (note velocity or cc value)
     *
     * @return uint8_t
     */
    uint8_t getTriggerValue() const noexcept;
    /**
     * @brief Get the type of trigger
     *
     * @return TriggerType
     */
    TriggerType getTriggerType() const noexcept;

    /**
     * @brief Reset the voice to its initial values
     *
     */
    void reset() noexcept;

    /**
     * @brief Get the mean squared power of the last rendered block. This is used
     * to determine which voice to steal if there are too many notes flying around.
     *
     * @return float
     */
    float getMeanSquaredAverage() const noexcept;
    /**
     * @brief Get the position of the voice in the source, in samples
     *
     * @return uint32_t
     */
    uint32_t getSourcePosition() const noexcept;
    /**
     * Returns the region that is currently playing. May be null if the voice is not active!
     *
     * @return
     */
    const Region* getRegion() const noexcept { return region; }
    /**
     * @brief Set the max number of filters per voice
     *
     * @param numFilters
     */
    void setMaxFiltersPerVoice(size_t numFilters);
    /**
     * @brief Set the max number of EQs per voice
     *
     * @param numFilters
     */
    void setMaxEQsPerVoice(size_t numEQs);
private:
    /**
     * @brief Fill a span with data from a file source. This is the first step
     * in rendering each block of data.
     *
     * @param buffer
     */
    void fillWithData(AudioSpan<float> buffer) noexcept;
    /**
     * @brief Fill a span with data from a generator source. This is the first step
     * in rendering each block of data.
     *
     * @param buffer
     */
    void fillWithGenerator(AudioSpan<float> buffer) noexcept;
    /**
     * @brief The function processing a mono sample source
     *
     * @param buffer
     */
    void processMono(AudioSpan<float> buffer) noexcept;
    /**
     * @brief The function processing a stereo sample source
     *
     * @param buffer
     */
    void processStereo(AudioSpan<float> buffer) noexcept;
    /**
     * @brief Release the voice after a given delay
     *
     * @param delay
     * @param fastRelease whether to do a normal release or cut the voice abruptly
     */
    void release(int delay, bool fastRelease = false) noexcept;
    Region* region { nullptr };

    enum class State {
        idle,
        playing
    };
    State state { State::idle };
    bool noteIsOff { false };

    TriggerType triggerType;
    int triggerNumber;
    uint8_t triggerValue;
    absl::optional<int> triggerDelay;

    float speedRatio { 1.0 };
    float pitchRatio { 1.0 };
    float baseVolumedB{ 0.0 };
    float baseGain { 1.0 };
    float basePan { 0.0 };
    float basePosition { 0.0 };
    float baseWidth { 0.0 };
    float baseFrequency { 440.0 };
    float phase { 0.0f };

    float floatPositionOffset { 0.0f };
    int sourcePosition { 0 };
    int initialDelay { 0 };

    FilePromisePtr currentPromise { nullptr };

    Buffer<float> tempBuffer1;
    Buffer<float> tempBuffer2;
    Buffer<float> tempBuffer3;
    Buffer<int> indexBuffer;
    absl::Span<float> tempSpan1 { absl::MakeSpan(tempBuffer1) };
    absl::Span<float> tempSpan2 { absl::MakeSpan(tempBuffer2) };
    absl::Span<float> tempSpan3 { absl::MakeSpan(tempBuffer3) };
    absl::Span<int> indexSpan { absl::MakeSpan(indexBuffer) };

    int samplesPerBlock { config::defaultSamplesPerBlock };
    int minEnvelopeDelay { config::defaultSamplesPerBlock / 2 };
    float sampleRate { config::defaultSampleRate };

    Resources& resources;

    std::vector<FilterHolderPtr> filters;
    std::vector<EQHolderPtr> equalizers;

    ADSREnvelope<float> egEnvelope;
    LinearEnvelope<float> amplitudeEnvelope; // linear events
    LinearEnvelope<float> crossfadeEnvelope;
    LinearEnvelope<float> panEnvelope;
    LinearEnvelope<float> positionEnvelope;
    LinearEnvelope<float> widthEnvelope;
    MultiplicativeEnvelope<float> pitchBendEnvelope;
    MultiplicativeEnvelope<float> volumeEnvelope;
    float bendStepFactor { centsFactor(1) };

    std::normal_distribution<float> noiseDist { 0, config::noiseVariance };

    HistoricalBuffer<float> powerHistory { config::powerHistoryLength };
    LEAK_DETECTOR(Voice);
};

} // namespace sfz
