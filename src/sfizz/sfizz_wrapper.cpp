// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "Config.h"
#include "Macros.h"
#include "Synth.h"
#include "Messaging.h"
#include "sfizz.h"
#include <limits>

#ifdef __cplusplus
extern "C" {
#endif

sfizz_synth_t* sfizz_create_synth()
{
    return reinterpret_cast<sfizz_synth_t*>(new sfz::Synth());
}

bool sfizz_load_file(sfizz_synth_t* synth, const char* path)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->loadSfzFile(path);
}

bool sfizz_load_string(sfizz_synth_t* synth, const char* path, const char* text)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->loadSfzString(path, text);
}

bool sfizz_load_scala_file(sfizz_synth_t* synth, const char* path)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->loadScalaFile(path);
}

bool sfizz_load_scala_string(sfizz_synth_t* synth, const char* text)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->loadScalaString(text);
}

void sfizz_set_scala_root_key(sfizz_synth_t* synth, int root_key)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->setScalaRootKey(root_key);
}

int sfizz_get_scala_root_key(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getScalaRootKey();
}

void sfizz_set_tuning_frequency(sfizz_synth_t* synth, float frequency)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->setTuningFrequency(frequency);
}

float sfizz_get_tuning_frequency(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getTuningFrequency();
}

void sfizz_load_stretch_tuning_by_ratio(sfizz_synth_t* synth, float ratio)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->loadStretchTuningByRatio(ratio);
}

void sfizz_free(sfizz_synth_t* synth)
{
    delete reinterpret_cast<sfz::Synth*>(synth);
}

int sfizz_get_num_regions(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumRegions();
}
int sfizz_get_num_groups(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumGroups();
}
int sfizz_get_num_masters(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumMasters();
}
int sfizz_get_num_curves(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumCurves();
}
char* sfizz_export_midnam(sfizz_synth_t* synth, const char* model)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return strdup(self->exportMidnam(model ? model : "").c_str());
}
size_t sfizz_get_num_preloaded_samples(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumPreloadedSamples();
}
int sfizz_get_num_active_voices(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumActiveVoices();
}

void sfizz_set_samples_per_block(sfizz_synth_t* synth, int samples_per_block)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->setSamplesPerBlock(samples_per_block);
}
void sfizz_set_sample_rate(sfizz_synth_t* synth, float sample_rate)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->setSampleRate(sample_rate);
}

void sfizz_send_note_on(sfizz_synth_t* synth, int delay, int note_number, char velocity)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->noteOn(delay, note_number, velocity);
}
void sfizz_send_note_off(sfizz_synth_t* synth, int delay, int note_number, char velocity)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->noteOff(delay, note_number, velocity);
}
void sfizz_send_cc(sfizz_synth_t* synth, int delay, int cc_number, char cc_value)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->cc(delay, cc_number, cc_value);
}
void sfizz_send_hdcc(sfizz_synth_t* synth, int delay, int cc_number, float norm_value)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->hdcc(delay, cc_number, norm_value);
}
void sfizz_automate_hdcc(sfizz_synth_t* synth, int delay, int cc_number, float norm_value)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->automateHdcc(delay, cc_number, norm_value);
}
void sfizz_send_pitch_wheel(sfizz_synth_t* synth, int delay, int pitch)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->pitchWheel(delay, pitch);
}
void sfizz_send_aftertouch(sfizz_synth_t* synth, int delay, char aftertouch)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->aftertouch(delay, aftertouch);
}
void sfizz_send_tempo(sfizz_synth_t* synth, int delay, float seconds_per_quarter)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->tempo(delay, seconds_per_quarter);
}
void sfizz_send_time_signature(sfizz_synth_t* synth, int delay, int beats_per_bar, int beat_unit)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->timeSignature(delay, beats_per_bar, beat_unit);
}
void sfizz_send_time_position(sfizz_synth_t* synth, int delay, int bar, double bar_beat)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->timePosition(delay, bar, bar_beat);
}
void sfizz_send_playback_state(sfizz_synth_t* synth, int delay, int playback_state)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->playbackState(delay, playback_state);
}

void sfizz_render_block(sfizz_synth_t* synth, float** channels, int num_channels, int num_frames)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    // Only stereo output is supported for now
    ASSERT(num_channels == 2);
    UNUSED(num_channels);
    self->renderBlock({{channels[0], channels[1]}, static_cast<size_t>(num_frames)});
}

unsigned int sfizz_get_preload_size(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getPreloadSize();
}
void sfizz_set_preload_size(sfizz_synth_t* synth, unsigned int preload_size)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->setPreloadSize(preload_size);
}

sfizz_oversampling_factor_t sfizz_get_oversampling_factor(sfizz_synth_t*)
{
    return SFIZZ_OVERSAMPLING_X1;
}

bool sfizz_set_oversampling_factor(sfizz_synth_t*, sfizz_oversampling_factor_t)
{
    return true;
}

int sfizz_get_sample_quality(sfizz_synth_t* synth, sfizz_process_mode_t mode)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getSampleQuality(static_cast<sfz::Synth::ProcessMode>(mode));
}

void sfizz_set_sample_quality(sfizz_synth_t* synth, sfizz_process_mode_t mode, int quality)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->setSampleQuality(static_cast<sfz::Synth::ProcessMode>(mode), quality);
}

void sfizz_set_volume(sfizz_synth_t* synth, float volume)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->setVolume(volume);
}

float sfizz_get_volume(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getVolume();
}

void sfizz_set_num_voices(sfizz_synth_t* synth, int num_voices)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->setNumVoices(num_voices);
}

int sfizz_get_num_voices(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getNumVoices();
}

int sfizz_get_num_buffers(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getAllocatedBuffers();
}

int sfizz_get_num_bytes(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getAllocatedBytes();
}

void sfizz_enable_freewheeling(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->enableFreeWheeling();
}

void sfizz_disable_freewheeling(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->disableFreeWheeling();
}

char* sfizz_get_unknown_opcodes(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
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
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->shouldReloadFile();
}

bool sfizz_should_reload_scala(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->shouldReloadScala();
}

void sfizz_enable_logging(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->enableLogging();
}

void sfizz_set_logging_prefix(sfizz_synth_t* synth, const char* prefix)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->setLoggingPrefix(prefix);
}

void sfizz_disable_logging(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->disableLogging();
}

void sfizz_all_sound_off(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->allSoundOff();
}

void sfizz_add_external_definitions(sfizz_synth_t* synth, const char* id, const char* value)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->getParser().addExternalDefinition(id, value);
}

void sfizz_clear_external_definitions(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->getParser().clearExternalDefinitions();
}

unsigned int sfizz_get_num_key_labels(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getKeyLabels().size();
}

int sfizz_get_key_label_number(sfizz_synth_t* synth, int label_index)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    const auto keyLabels = self->getKeyLabels();
    if (label_index < 0)
        return SFIZZ_OUT_OF_BOUNDS_LABEL_INDEX;

    if (static_cast<unsigned int>(label_index) >= keyLabels.size())
        return SFIZZ_OUT_OF_BOUNDS_LABEL_INDEX;

    // Sanity checks for the future or platforms
    static_assert(
        std::numeric_limits<sfz::NoteNamePair::first_type>::max() < std::numeric_limits<int>::max(),
        "The C API sends back an int but the note index in NoteNamePair can overflow it on this platform"
    );
    return static_cast<int>(keyLabels[label_index].first);
}

const char * sfizz_get_key_label_text(sfizz_synth_t* synth, int label_index)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    const auto keyLabels = self->getKeyLabels();
    if (label_index < 0)
        return NULL;

    if (static_cast<unsigned int>(label_index) >= keyLabels.size())
        return NULL;

    return keyLabels[label_index].second.c_str();
}

unsigned int sfizz_get_num_cc_labels(sfizz_synth_t* synth)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    return self->getCCLabels().size();
}

int sfizz_get_cc_label_number(sfizz_synth_t* synth, int label_index)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    const auto ccLabels = self->getCCLabels();
    if (label_index < 0)
        return SFIZZ_OUT_OF_BOUNDS_LABEL_INDEX;

    if (static_cast<unsigned int>(label_index) >= ccLabels.size())
        return SFIZZ_OUT_OF_BOUNDS_LABEL_INDEX;

    // Sanity checks for the future or platforms
    static_assert(
        std::numeric_limits<sfz::CCNamePair::first_type>::max() < std::numeric_limits<int>::max(),
        "The C API sends back an int but the cc index in CCNamePair can overflow it on this platform"
    );
    return static_cast<int>(ccLabels[label_index].first);
}

const char * sfizz_get_cc_label_text(sfizz_synth_t* synth, int label_index)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    const auto ccLabels = self->getCCLabels();
    if (label_index < 0)
        return NULL;

    if (static_cast<unsigned int>(label_index) >= ccLabels.size())
        return NULL;

    return ccLabels[label_index].second.c_str();
}

sfizz_client_t* sfizz_create_client(void* data)
{
    return reinterpret_cast<sfizz_client_t*>(new sfz::Client(data));
}

void sfizz_delete_client(sfizz_client_t* client)
{
    delete reinterpret_cast<sfz::Client*>(client);
}

void* sfizz_get_client_data(sfizz_client_t* client)
{
    return reinterpret_cast<sfz::Client*>(client)->getClientData();
}

void sfizz_set_receive_callback(sfizz_client_t* client, sfizz_receive_t* receive)
{
    reinterpret_cast<sfz::Client*>(client)->setReceiveCallback(receive);
}

void sfizz_send_message(sfizz_synth_t* synth, sfizz_client_t* client, int delay, const char* path, const char* sig, const sfizz_arg_t* args)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->dispatchMessage(*reinterpret_cast<sfz::Client*>(client), delay, path, sig, args);
}

void sfizz_set_broadcast_callback(sfizz_synth_t* synth, sfizz_receive_t* broadcast, void* data)
{
    auto* self = reinterpret_cast<sfz::Synth*>(synth);
    self->setBroadcastCallback(broadcast, data);
}

#ifdef __cplusplus
}
#endif
