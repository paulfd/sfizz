// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

/*
   Note(jpc): implementation status

- [x] comp_gain           Gain (dB)
- [x] comp_attack         Attack time (s)
- [x] comp_release        Release time (s)
- [x] comp_ratio          Ratio (linear gain)
- [x] comp_threshold      Threshold (dB)
- [x] comp_stlink         Stereo link (boolean)

*/

#include "Compressor.h"
#include "Opcode.h"
#include "AudioSpan.h"
#include "MathHelpers.h"
#include "absl/memory/memory.h"

static constexpr int _oversampling = 2;
#define FAUST_UIMACROS 1
#include "gen/compressor.cxx"

namespace sfz {
namespace fx {

    struct Compressor::Impl {
        faustCompressor _compressor[2];
        bool _stlink = false;
        float _inputGain = 1.0;
        AudioBuffer<float, 2> _tempBuffer2x { 2, _oversampling * config::defaultSamplesPerBlock };
        AudioBuffer<float, 2> _gain2x { 2, _oversampling * config::defaultSamplesPerBlock };
        hiir::Downsampler2xFpu<12> _downsampler2x[EffectChannels];
        hiir::Upsampler2xFpu<12> _upsampler2x[EffectChannels];

        #define DEFINE_SET_GET(type, ident, name, var, def, min, max, step) \
            float get_##ident(size_t i) const noexcept { return _compressor[i].var; } \
            void set_##ident(size_t i, float value) noexcept { _compressor[i].var = value; }
        FAUST_LIST_ACTIVES(DEFINE_SET_GET);
        #undef DEFINE_SET_GET
    };

    Compressor::Compressor()
        : _impl(new Impl)
    {
        Impl& impl = *_impl;
        for (faustCompressor& comp : impl._compressor)
            comp.instanceResetUserInterface();
    }

    Compressor::~Compressor()
    {
    }

    void Compressor::setSampleRate(double sampleRate)
    {
        Impl& impl = *_impl;
        for (faustCompressor& comp : impl._compressor) {
            comp.classInit(sampleRate);
            comp.instanceConstants(sampleRate);
        }

        static constexpr double coefs2x[12] = { 0.036681502163648017, 0.13654762463195794, 0.27463175937945444, 0.42313861743656711, 0.56109869787919531, 0.67754004997416184, 0.76974183386322703, 0.83988962484963892, 0.89226081800387902, 0.9315419599631839, 0.96209454837808417, 0.98781637073289585 };

        for (unsigned c = 0; c < EffectChannels; ++c) {
            impl._downsampler2x[c].set_coefs(coefs2x);
            impl._upsampler2x[c].set_coefs(coefs2x);
        }

        clear();
    }

    void Compressor::setSamplesPerBlock(int samplesPerBlock)
    {
        Impl& impl = *_impl;
        impl._tempBuffer2x.resize(_oversampling * samplesPerBlock);
        impl._gain2x.resize(_oversampling * samplesPerBlock);
    }

    void Compressor::clear()
    {
        Impl& impl = *_impl;
        for (faustCompressor& comp : impl._compressor)
            comp.instanceClear();
    }

    void Compressor::process(const float* const inputs[], float* const outputs[], unsigned nframes)
    {
        Impl& impl = *_impl;
        auto inOut2x = AudioSpan<float>(impl._tempBuffer2x).first(_oversampling * nframes);

        absl::Span<float> left2x = inOut2x.getSpan(0);
        absl::Span<float> right2x = inOut2x.getSpan(1);

        impl._upsampler2x[0].process_block(left2x.data(), inputs[0], nframes);
        impl._upsampler2x[1].process_block(right2x.data(), inputs[1], nframes);

        const float inputGain = impl._inputGain;
        for (unsigned i = 0; i < _oversampling * nframes; ++i) {
            left2x[i] *= inputGain;
            right2x[i] *= inputGain;
        }

        if (!impl._stlink) {
            absl::Span<float> leftGain2x = impl._gain2x.getSpan(0);
            absl::Span<float> rightGain2x = impl._gain2x.getSpan(1);

            {
                faustCompressor& comp = impl._compressor[0];
                float* inputs[] = { left2x.data() };
                float* outputs[] = { leftGain2x.data() };
                comp.compute(_oversampling * nframes, inputs, outputs);
            }

            {
                faustCompressor& comp = impl._compressor[1];
                float* inputs[] = { right2x.data() };
                float* outputs[] = { rightGain2x.data() };
                comp.compute(_oversampling * nframes, inputs, outputs);
            }

            for (unsigned i = 0; i < _oversampling * nframes; ++i) {
                left2x[i] *= leftGain2x[i];
                right2x[i] *= rightGain2x[i];
            }
        }
        else {
            absl::Span<float> compIn2x = impl._gain2x.getSpan(0);
            for (unsigned i = 0; i < _oversampling * nframes; ++i)
                compIn2x[i] = std::abs(left2x[i]) + std::abs(right2x[1]);

            absl::Span<float> gain2x = impl._gain2x.getSpan(1);

            {
                faustCompressor& comp = impl._compressor[0];
                float* inputs[] = { compIn2x.data() };
                float* outputs[] = { gain2x.data() };
                comp.compute(_oversampling * nframes, inputs, outputs);
            }

            for (unsigned i = 0; i < _oversampling * nframes; ++i) {
                left2x[i] *= gain2x[i];
                right2x[i] *= gain2x[i];
            }
        }

        impl._downsampler2x[0].process_block(outputs[0], left2x.data(), nframes);
        impl._downsampler2x[1].process_block(outputs[1], right2x.data(), nframes);
    }

    std::unique_ptr<Effect> Compressor::makeInstance(absl::Span<const Opcode> members)
    {
        Compressor* compressor = new Compressor;
        std::unique_ptr<Effect> fx { compressor };

        Impl& impl = *compressor->_impl;

        for (const Opcode& opc : members) {
            switch (opc.lettersOnlyHash) {
            case hash("comp_attack"):
                if (auto value = readOpcode<float>(opc.value, {0.0, 10.0})) {
                    for (size_t c = 0; c < 2; ++c)
                        impl.set_Attack(c, *value);
                }
                break;
            case hash("comp_release"):
                if (auto value = readOpcode<float>(opc.value, {0.0, 10.0})) {
                    for (size_t c = 0; c < 2; ++c)
                        impl.set_Release(c, *value);
                }
                break;
            case hash("comp_threshold"):
                if (auto value = readOpcode<float>(opc.value, {-100.0, 0.0})) {
                    for (size_t c = 0; c < 2; ++c)
                        impl.set_Threshold(c, *value);
                }
                break;
            case hash("comp_ratio"):
                if (auto value = readOpcode<float>(opc.value, {1.0, 50.0})) {
                    for (size_t c = 0; c < 2; ++c)
                        impl.set_Ratio(c, *value);
                }
                break;
            case hash("comp_gain"):
                if (auto value = readOpcode<float>(opc.value, {-100.0, 100.0}))
                    impl._inputGain = db2mag(*value);
                break;
            case hash("comp_stlink"):
                if (auto value = readBooleanOpcode(opc))
                    impl._stlink = *value;
                break;
            }
        }

        return fx;
    }

} // namespace fx
} // namespace sfz
