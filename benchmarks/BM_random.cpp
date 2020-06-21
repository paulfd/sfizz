// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "SIMDHelpers.h"
#include <benchmark/benchmark.h>
#include <random>
#include <numeric>
#include <vector>
#include <cmath>
#include <iostream>
#include <immintrin.h>

constexpr uintptr_t ByteAlignmentMask(unsigned N) { return N - 1; }

template<unsigned N, class T>
T* prevAligned(const T* ptr)
{
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(ptr) & (~ByteAlignmentMask(N)));
}

template<unsigned N, class T>
bool unaligned(const T* ptr)
{
    return (reinterpret_cast<uintptr_t>(ptr) & ByteAlignmentMask(N) )!= 0;
}

template<unsigned N, class T, class... Args>
bool unaligned(const T* ptr1, Args... rest)
{
    return unaligned<N>(ptr1) || unaligned<N>(rest...);
}

class Fixture : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) {
        ints.resize(state.range());
        floats.resize(state.range());
        sfz::fill<uint32_t>(absl::MakeSpan(ints), 0u);
        sfz::fill<float>(absl::MakeSpan(floats), 0.0f);
    }

    void TearDown(const ::benchmark::State& /* state */) {

    }

    inline uint32_t nextInt()
    {
        seed *= 1664525u + 1013904223u;
        return seed;
    }

    inline void randomIntsSSE(uint32_t* output, unsigned size) noexcept
    {
        const auto sentinel = output + size;
        const auto* lastAligned = prevAligned<4>(sentinel);
        while (unaligned<4>(output) && output < lastAligned) {
            seed *= 1664525u + 1013904223u;
            *output++ = seed;
        }

        // Original Intel code from
        // https://software.intel.com/content/www/us/en/develop/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor.html
        // I don't really get the shuffling as it seems it will basically not be any different from just
        // applying the operations, thereby avoiding the and/or masking? ....
        //
        // auto mmSeed = _mm_set_epi32( seed, seed + 1, seed , seed + 1 );
        // static const unsigned int mult[4] = { 214013, 17405, 214013, 69069 };
        // static const unsigned int gadd[4] = { 2531011, 10395331, 13737667, 1 };
        // static const unsigned int mask[4] = { 0xFFFFFFFF, 0, 0xFFFFFFFF, 0 };
        // auto mmAdd = _mm_load_si128( (__m128i*) gadd);
        // auto mmMult = _mm_load_si128( (__m128i*) mult);
        // auto mmMask = _mm_load_si128( (__m128i*) mask);

        // while (output < lastAligned) {
        //     auto mmShuffledSeed = _mm_shuffle_epi32( mmSeed, _MM_SHUFFLE( 2, 3, 0, 1 ) );
        //     mmSeed = _mm_mul_epu32( mmSeed, mmMult );
        //     mmMult = _mm_shuffle_epi32( mmMult, _MM_SHUFFLE( 2, 3, 0, 1 ) );
        //     mmShuffledSeed = _mm_mul_epu32( mmShuffledSeed, mmMult );
        //     // Added to reset mult to the original
        //     mmMult = _mm_shuffle_epi32( mmMult, _MM_SHUFFLE( 2, 3, 0, 1 ) );

        //     mmSeed = _mm_and_si128( mmSeed, mmMask);
        //     mmShuffledSeed = _mm_and_si128( mmShuffledSeed, mmMask );
        //     mmShuffledSeed = _mm_shuffle_epi32( mmShuffledSeed, _MM_SHUFFLE( 2, 3, 0, 1 ) );
        //     mmSeed = _mm_or_si128( mmSeed, mmShuffledSeed );
        //     mmSeed = _mm_add_epi32( mmSeed, mmAdd);
        // }

        const uint32_t _seed [4] { seed, seed + 1, seed + 2, seed + 3 };
        static const uint32_t _mult[4] { 214013, 17405, 214013, 69069 };
        static const uint32_t _add[4] { 2531011, 10395331, 13737667, 1 };
        auto mmAdd = _mm_load_si128( (__m128i*) _add);
        auto mmMult = _mm_load_si128( (__m128i*) _mult);
        auto mmSeed = _mm_load_si128( (__m128i*) _seed);

        while (output < lastAligned) {
            mmSeed = _mm_mul_epu32( mmSeed, mmMult );
            mmSeed = _mm_add_epi32( mmSeed, mmAdd);
            _mm_store_si128((__m128i*) output, mmSeed);
            incrementAll<4>(output);
        }
        // Replace _mm_store_si32 which doesn't exist in all *mmintrin.h headers apparently
        _mm_store_ss((float*)&seed, _mm_castsi128_ps(mmSeed));

        while (output < sentinel) {
            seed *= 1664525u + 1013904223u;
            *output++ = seed;
        }
    }

    inline void randomIntsAVX(uint32_t* output, unsigned size) noexcept
    {
        const auto sentinel = output + size;
        const auto* lastAligned = prevAligned<8>(sentinel);
        while (unaligned<8>(output) && output < lastAligned) {
            seed *= 1664525u + 1013904223u;
            *output++ = seed;
        }

        const uint32_t _seed [8] __attribute__((aligned(32))) { seed, seed + 1, seed + 2, seed + 3, seed + 4, seed + 5, seed + 6, seed + 7 };
        static const uint32_t _mult[8] __attribute__((aligned(32))) { 214013, 17405, 214013, 69069, 214013, 17405, 214013, 69069 };
        static const uint32_t _add[8]  __attribute__((aligned(32))){ 2531011, 10395331, 13737667, 1, 2531011, 10395331, 13737667, 1 };
        auto mmAdd __attribute__((aligned(32))) = _mm256_loadu_si256( (__m256i*) _add);
        auto mmMult __attribute__((aligned(32))) = _mm256_loadu_si256( (__m256i*) _mult);
        auto mmSeed __attribute__((aligned(32))) = _mm256_loadu_si256( (__m256i*) _seed);

        while (output < lastAligned) {
            mmSeed = _mm256_mul_epu32( mmSeed, mmMult );
            mmSeed = _mm256_add_epi32( mmSeed, mmAdd);
            _mm256_store_si256((__m256i*) output, mmSeed);
            incrementAll<8>(output);
        }
        // Replace _mm_store_si32 which doesn't exist in all *mmintrin.h headers apparently
        _mm_store_ss((float*)&seed, _mm256_extractf128_ps(_mm256_castsi256_ps(mmSeed), 0));

        while (output < sentinel) {
            seed *= 1664525u + 1013904223u;
            *output++ = seed;
        }
    }

    inline float nextFloat()
    {
        return (static_cast<int32_t>(nextInt()) / (1 << 16)) * (1.0f / 32768);
    }

    inline float nextPositiveFloat()
    {
        return (nextInt() >> 16) * (1.0f / 65536);
    }

  std::minstd_rand stdRand { };
  uint32_t seed { 42u };
  std::vector<uint32_t> ints;
  std::vector<float> floats;
};


BENCHMARK_DEFINE_F(Fixture, ScalarInt)(benchmark::State& state) {
    for (auto _ : state)
    {
        for (auto& out : ints)
            out = nextInt();
    }
}

BENCHMARK_DEFINE_F(Fixture, ScalarFloat)(benchmark::State& state) {
    for (auto _ : state)
    {
        for (auto& out : floats)
            out = nextFloat();
    }
}

BENCHMARK_DEFINE_F(Fixture, ScalarPositiveFloat)(benchmark::State& state) {
    for (auto _ : state)
    {
        for (auto& out : floats)
            out = nextPositiveFloat();
    }
}

BENCHMARK_DEFINE_F(Fixture, SSEInt)(benchmark::State& state) {
    for (auto _ : state)
    {
        randomIntsSSE(ints.data(), ints.size());
    }
}

BENCHMARK_DEFINE_F(Fixture, AVXInt)(benchmark::State& state) {
    for (auto _ : state)
    {
        randomIntsAVX(ints.data(), ints.size());
    }
}


BENCHMARK_REGISTER_F(Fixture, ScalarInt)->RangeMultiplier(4)->Range(1 << 2, 1 << 12);
BENCHMARK_REGISTER_F(Fixture, ScalarFloat)->RangeMultiplier(4)->Range(1 << 2, 1 << 12);
BENCHMARK_REGISTER_F(Fixture, ScalarPositiveFloat)->RangeMultiplier(4)->Range(1 << 2, 1 << 12);
BENCHMARK_REGISTER_F(Fixture, SSEInt)->RangeMultiplier(4)->Range(1 << 2, 1 << 12);
BENCHMARK_REGISTER_F(Fixture, AVXInt)->RangeMultiplier(4)->Range(1 << 2, 1 << 12);
BENCHMARK_MAIN();
