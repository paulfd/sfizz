// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#pragma once
#include "Config.h"
#include "MathHelpers.h"
#include <absl/types/span.h>
#include <cmath>

namespace sfz {
/**
 * @brief An implementation of a one pole filter. This is a scalar
 * implementation.
 *
 * @tparam Type the underlying type of the filter.
 */
template <class Type = float>
class OnePoleFilter {
public:
    OnePoleFilter() = default;
    // Normalized cutoff with respect to the sampling rate
    template <class C>
    static Type normalizedGain(Type cutoff, C sampleRate)
    {
        return std::tan(cutoff / static_cast<Type>(sampleRate) * pi<float>());
    }

    OnePoleFilter(Type gain)
    {
        setGain(gain);
    }

    void setGain(Type gain)
    {
        G = gain / (1 + gain);
    }

    size_t processLowpass(absl::Span<const Type> input, absl::Span<Type> lowpass)
    {
        auto in = input.begin();
        auto out = lowpass.begin();
        auto size = std::min(input.size(), lowpass.size());
        auto sentinel = in + size;
        while (in < sentinel) {
            oneLowpass(in, out);
            in++;
            out++;
        }
        return size;
    }

    size_t processHighpass(absl::Span<const Type> input, absl::Span<Type> highpass)
    {
        auto in = input.begin();
        auto out = highpass.begin();
        auto size = std::min(input.size(), highpass.size());
        auto sentinel = in + size;
        while (in < sentinel) {
            oneHighpass(in, out);
            in++;
            out++;
        }
        return size;
    }

    size_t processLowpassVariableGain(absl::Span<const Type> input, absl::Span<Type> lowpass, absl::Span<const Type> gain)
    {
        auto in = input.begin();
        auto out = lowpass.begin();
        auto g = gain.begin();
        auto size = min(input.size(), lowpass.size(), gain.size());
        auto sentinel = in + size;
        while (in < sentinel) {
            setGain(*g);
            oneLowpass(in, out);
            in++;
            out++;
            g++;
        }
        return size;
    }

    size_t processHighpassVariableGain(absl::Span<const Type> input, absl::Span<Type> highpass, absl::Span<const Type> gain)
    {
        auto in = input.begin();
        auto out = highpass.begin();
        auto g = gain.begin();
        auto size = min(input.size(), highpass.size(), gain.size());
        auto sentinel = in + size;
        while (in < sentinel) {
            setGain(*g);
            oneHighpass(in, out);
            in++;
            out++;
            g++;
        }
        return size;
    }

    void reset() { state = 0.0; }

private:
    Type state { 0.0 };
    Type G { 0.2 };

    void oneLowpass(const Type* in, Type* out)
    {
        const auto intermediate = G * (*in - state);
        *out = intermediate + state;
        state = *out + intermediate;
    }

    void oneHighpass(const Type* in, Type* out)
    {
        const auto intermediate = G * (*in - state);
        *out = *in - intermediate - state;
        state += 2 * intermediate;
    }
};

constexpr unsigned int mask(int x)
{
	return (1U << x) - 1;
}


class PowGenerator {
    enum { N = 11 };
    float tbl0_[256];
    struct
    {
        float app;
        float rev;
    } tbl1_[1 << N];
    union fi {
        float f;
        unsigned int i;
    };

public:
    PowGenerator(float y)
    {
        for (int i = 0; i < 256; i++) {
            tbl0_[i] = ::powf(2, (i - 127) * y);
        }
        const double e = 1 / double(1 << 24);
        const double h = 1 / double(1 << N);
        const size_t n = 1U << N;
        for (size_t i = 0; i < n; i++) {
            double x = 1 + double(i) / n;
            double a = ::pow(x, (double)y);
            tbl1_[i].app = (float)a;
            double b = ::pow(x + h - e, (double)y);
            tbl1_[i].rev = (float)((b - a) / (h - e) / (1 << 23));
        }
    }

    float get(float x) const
    {
        fi fi;
        fi.f = x;
        int a = (fi.i >> 23) & mask(8);
        unsigned int b = fi.i & mask(23);
        unsigned int b1 = b & (mask(N) << (23 - N));
        unsigned int b2 = b & mask(23 - N);
        float f;
        int idx = b1 >> (23 - N);
        f = tbl0_[a] * (tbl1_[idx].app + float(b2) * tbl1_[idx].rev);
        return f;
    }
};

template <class Type = float>
class OnePoleFilterMul {
public:
    OnePoleFilterMul() = default;
    // Normalized cutoff with respect to the sampling rate
    template <class C>
    static Type normalizedGain(Type cutoff, C sampleRate)
    {
        return std::tan(cutoff / static_cast<Type>(sampleRate) * pi<float>());
    }

    OnePoleFilterMul(Type gain)
    {
        setGain(gain);
    }

    void setGain(Type gain)
    {
        G = gain / (1 + gain);
    }

    size_t processLowpass(absl::Span<const Type> input, absl::Span<Type> lowpass)
    {
        auto in = input.begin();
        auto out = lowpass.begin();
        auto size = std::min(input.size(), lowpass.size());
        auto sentinel = in + size;
        while (in < sentinel) {
            oneLowpass(in, out);
            in++;
            out++;
        }
        return size;
    }

    void reset() { state = 1.0; }

private:
    Type state { 1.0 };
    Type G { 0.2 };
    PowGenerator gen { G };

    void oneLowpass(const Type* in, Type* out)
    {
        const auto intermediate = gen.get(*in / state);
        *out = intermediate * state;
        state = *out * intermediate;
    }
};
}
