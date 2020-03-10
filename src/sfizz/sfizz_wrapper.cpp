// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "Config.h"
#include "Macros.h"
#include "Synth.h"
#include "sfizz.h"

#ifdef __cplusplus
extern "C" {
#endif

sfizz_synth_t* sfizz_create_synth()
{
    return reinterpret_cast<sfizz_synth_t*>(new sfz::Synth());
}

bool sfizz_load_file(sfizz_synth_t* synth, const char* path)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->loadSfzFile(path);
}

void sfizz_free(sfizz_synth_t* synth)
{
    delete reinterpret_cast<sfz::Synth*>(synth);
}

int sfizz_get_num_regions(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumRegions();
}
int sfizz_get_num_groups(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumGroups();
}
int sfizz_get_num_masters(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumMasters();
}
int sfizz_get_num_curves(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumCurves();
}
char* sfizz_export_midnam(sfizz_synth_t* synth, const char* model)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return strdup(self->exportMidnam(model ? model : "").c_str());
}
size_t sfizz_get_num_preloaded_samples(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumPreloadedSamples();
}
int sfizz_get_num_active_voices(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumActiveVoices();
}

void sfizz_set_samples_per_block(sfizz_synth_t* synth, int samples_per_block)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->setSamplesPerBlock(samples_per_block);
}
void sfizz_set_sample_rate(sfizz_synth_t* synth, float sample_rate)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->setSampleRate(sample_rate);
}

void sfizz_send_note_on(sfizz_synth_t* synth, int delay, int note_number, char velocity)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->noteOn(delay, note_number, velocity);
}
void sfizz_send_note_off(sfizz_synth_t* synth, int delay, int note_number, char velocity)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->noteOff(delay, note_number, velocity);
}
void sfizz_send_cc(sfizz_synth_t* synth, int delay, int cc_number, char cc_value)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->cc(delay, cc_number, cc_value);
}
void sfizz_send_pitch_wheel(sfizz_synth_t* synth, int delay, int pitch)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->pitchWheel(delay, pitch);
}
void sfizz_send_aftertouch(sfizz_synth_t* synth, int delay, char aftertouch)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->aftertouch(delay, aftertouch);
}
void sfizz_send_tempo(sfizz_synth_t* synth, int delay, float seconds_per_quarter)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->tempo(delay, seconds_per_quarter);
}

void sfizz_render_block(sfizz_synth_t* synth, float** channels, int num_channels, int num_frames)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    // Only stereo output is supported for now
    ASSERT(num_channels == 2);
    UNUSED(num_channels);
    self->renderBlock({{channels[0], channels[1]}, static_cast<size_t>(num_frames)});
}

unsigned int sfizz_get_preload_size(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getPreloadSize();
}
void sfizz_set_preload_size(sfizz_synth_t* synth, unsigned int preload_size)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->setPreloadSize(preload_size);
}

sfizz_oversampling_factor_t sfizz_get_oversampling_factor(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return static_cast<sfizz_oversampling_factor_t>(self->getOversamplingFactor());
}

bool sfizz_set_oversampling_factor(sfizz_synth_t* synth, sfizz_oversampling_factor_t oversampling)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    using sfz::Oversampling;
    switch(oversampling)
    {
        case SFIZZ_OVERSAMPLING_X1:
            self->setOversamplingFactor(sfz::Oversampling::x1);
            return true;
        case SFIZZ_OVERSAMPLING_X2:
            self->setOversamplingFactor(sfz::Oversampling::x2);
            return true;
        case SFIZZ_OVERSAMPLING_X4:
            self->setOversamplingFactor(sfz::Oversampling::x4);
            return true;
        case SFIZZ_OVERSAMPLING_X8:
            self->setOversamplingFactor(sfz::Oversampling::x8);
            return true;
        default:
            return false;
    }
}

void sfizz_set_volume(sfizz_synth_t* synth, float volume)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->setVolume(volume);
}

float sfizz_get_volume(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getVolume();
}

void sfizz_set_num_voices(sfizz_synth_t* synth, int num_voices)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->setNumVoices(num_voices);
}

int sfizz_get_num_voices(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumVoices();
}

int sfizz_get_num_buffers(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getAllocatedBuffers();
}

int sfizz_get_num_bytes(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getAllocatedBytes();
}

void sfizz_enable_freewheeling(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->enableFreeWheeling();
}

void sfizz_disable_freewheeling(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    self->disableFreeWheeling();
}

char* sfizz_get_unknown_opcodes(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    const auto unknownOpcodes = self->getUnknownOpcodes();
    size_t totalLength = 0;
    for (auto& opcode: unknownOpcodes)
        totalLength += opcode.length() + 1;

    if (totalLength == 0)
        return nullptr;

    auto opcodeList = (char *)std::malloc(totalLength);

    auto listIterator = opcodeList;
    for (auto& opcode : unknownOpcodes) {
        std::copy(opcode.begin(), opcode.end(), listIterator);
        listIterator += opcode.length();
        *listIterator++ = (opcode == *unknownOpcodes.rbegin()) ? '\0' : ',';
    }
    return opcodeList;
}

bool sfizz_should_reload_file(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->shouldReloadFile();
}

void sfizz_enable_logging(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->enableLogging();
}
void sfizz_disable_logging(sfizz_synth_t* synth)
{
    auto self = reinterpret_cast<sfz::Synth*>(synth);
    return self->disableLogging();
}

#ifdef __cplusplus
}
#endif
