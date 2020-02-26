// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#pragma once
#ifdef _WIN32
// There's a spurious min/max function in MSVC that makes everything go badly...
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
#endif
#include "absl/strings/string_view.h"
#include <cstddef>
#include <cstdint>

namespace sfz {
enum class Oversampling: int {
    x1 = 1,
    x2 = 2,
    x4 = 4,
    x8 = 8
};

namespace config {
    constexpr float defaultSampleRate { 48000 };
    constexpr int defaultSamplesPerBlock { 1024 };
    constexpr int maxBlockSize { 8192 };
    constexpr int preloadSize { 8192 };
    constexpr int loggerQueueSize { 16 };
    constexpr bool loggingEnabled { false };
    constexpr size_t numChannels { 2 };
    constexpr int numBackgroundThreads { 4 };
    constexpr int numVoices { 64 };
    constexpr int maxVoices { 256 };
    constexpr int maxFilePromises { maxVoices * 2 };
    constexpr int sustainCC { 64 };
    constexpr int allSoundOffCC { 120 };
    constexpr int resetCC { 121 };
    constexpr int allNotesOffCC { 123 };
    constexpr int omniOffCC { 124 };
    constexpr int omniOnCC { 125 };
    constexpr int halfCCThreshold { 64 };
    constexpr int centPerSemitone { 100 };
    constexpr float virtuallyZero { 0.00005f };
    constexpr float fastReleaseDuration { 0.01f };
    constexpr char defineCharacter { '$' };
    constexpr Oversampling defaultOversamplingFactor { Oversampling::x1 };
    constexpr float A440 { 440.0 };
    constexpr size_t powerHistoryLength { 16 };
    constexpr float voiceStealingThreshold { 0.00001f };
    constexpr uint8_t numCCs { 143 };
    constexpr int chunkSize { 1024 };
    constexpr float defaultAmpEGRelease { 0.02f };
    constexpr int filtersInPool { maxVoices * 2 };
    constexpr int filtersPerVoice { 2 };
    constexpr int eqsPerVoice { 3 };
    constexpr float noiseVariance { 0.25f };
    /**
       Minimum interval in frames between recomputations of coefficients of the
       modulated filter. The lower, the more CPU resources are consumed.
    */
    constexpr int filterControlInterval { 16 };
    /**
       Default metadata for MIDIName documents
     */
    const absl::string_view midnamManufacturer { "The Sfizz authors" };
    const absl::string_view midnamModel { "Sfizz" };
} // namespace config

// Enable or disable SIMD accelerators by default
namespace SIMDConfig {
    constexpr unsigned int defaultAlignment { 16 };
    constexpr bool writeInterleaved { true };
    constexpr bool readInterleaved { true };
    constexpr bool fill { true };
    constexpr bool gain { false };
    constexpr bool divide { false };
    constexpr bool mathfuns { false };
    constexpr bool loopingSFZIndex { true };
    constexpr bool saturatingSFZIndex { true };
    constexpr bool linearRamp { false };
    constexpr bool multiplicativeRamp { true };
    constexpr bool add { false };
    constexpr bool subtract { false };
    constexpr bool multiplyAdd { false };
    constexpr bool copy { false };
    constexpr bool pan { false };
    constexpr bool cumsum { true };
    constexpr bool diff { false };
    constexpr bool sfzInterpolationCast { true };
    constexpr bool mean { false };
    constexpr bool meanSquared { false };
    constexpr bool upsampling { true };
}
} // namespace sfz
