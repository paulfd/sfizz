// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "MidiState.h"
#include "Macros.h"
#include "Debug.h"

sfz::MidiState::MidiState()
{
    reset(0);
}

void sfz::MidiState::noteOnEvent(int delay, int noteNumber, uint8_t velocity) noexcept
{
    ASSERT(noteNumber >= 0 && noteNumber <= 127);
    ASSERT(velocity >= 0 && velocity <= 127);

    if (noteNumber >= 0 && noteNumber < 128) {
        lastNoteVelocities[noteNumber] = velocity;
        noteOnTimes[noteNumber] = std::chrono::steady_clock::now();
        activeNotes++;
    }

}

void sfz::MidiState::noteOffEvent(int delay, int noteNumber, uint8_t velocity) noexcept
{
    ASSERT(noteNumber >= 0 && noteNumber <= 127);
    ASSERT(velocity >= 0 && velocity <= 127);
    UNUSED(velocity);
    if (noteNumber >= 0 && noteNumber < 128) {
        if (activeNotes > 0)
            activeNotes--;
    }

}

float sfz::MidiState::getNoteDuration(int noteNumber) const
{
    ASSERT(noteNumber >= 0 && noteNumber <= 127);

    if (noteNumber >= 0 && noteNumber < 128) {
        const auto noteOffTime = std::chrono::steady_clock::now();
        const auto duration = std::chrono::duration_cast<std::chrono::duration<float>>(noteOffTime - noteOnTimes[noteNumber]);
        return duration.count();
    }

    return 0.0f;
}

uint8_t sfz::MidiState::getNoteVelocity(int noteNumber) const noexcept
{
    ASSERT(noteNumber >= 0 && noteNumber <= 127);

    return lastNoteVelocities[noteNumber];
}

void sfz::MidiState::pitchBendEvent(int delay, int pitchBendValue) noexcept
{
    ASSERT(pitchBendValue >= -8192 && pitchBendValue <= 8192);

    pitchBend = pitchBendValue;
}

int sfz::MidiState::getPitchBend() const noexcept
{
    return pitchBend;
}

void sfz::MidiState::ccEvent(int delay, int ccNumber, uint8_t ccValue) noexcept
{
    ASSERT(ccNumber >= 0 && ccNumber < config::numCCs);
    ASSERT(ccValue >= 0 && ccValue <= 127);

    cc[ccNumber] = ccValue;
}

uint8_t sfz::MidiState::getCCValue(int ccNumber) const noexcept
{
    ASSERT(ccNumber >= 0 && ccNumber < config::numCCs);

    return cc[ccNumber];
}

const sfz::SfzCCArray& sfz::MidiState::getCCArray() const noexcept
{
    return cc;
}

void sfz::MidiState::reset(int delay) noexcept
{
    for (auto& velocity: lastNoteVelocities)
        velocity = 0;

    for (auto& ccValue: cc)
        ccValue = 0;

    pitchBend = 0;
    activeNotes = 0;
}

void sfz::MidiState::resetAllControllers(int delay) noexcept
{
    for (int idx = 0; idx < config::numCCs; idx++)
        cc[idx] = 0;

    pitchBend = 0;
}
