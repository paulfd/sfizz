// SPDX-License-Identifier: BSD-2-Clause

// This code is part of the sfizz library and is licensed under a BSD 2-clause
// license. You should have receive a LICENSE.md file along with the code.
// If not, contact the sfizz maintainers at https://github.com/sfztools/sfizz

#include "sfizz/Synth.h"
#include "sfizz/SfzHelpers.h"
#include "sfizz/NumericId.h"
#include "catch2/catch.hpp"
using namespace Catch::literals;
using namespace sfz::literals;

constexpr int blockSize { 256 };

TEST_CASE("[Synth] Play and check active voices")
{
    sfz::Synth synth;
    synth.setSamplesPerBlock(blockSize);
    sfz::AudioBuffer<float> buffer { 2, blockSize };
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/groups_avl.sfz");

    synth.noteOn(0, 36, 24);
    synth.noteOn(0, 36, 89);
    REQUIRE(synth.getNumActiveVoices() == 2);
    // Render for a while
    for (int i = 0; i < 200; ++i)
        synth.renderBlock(buffer);
    REQUIRE(synth.getNumActiveVoices() == 0);
}

TEST_CASE("[Synth] All sound off")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/groups_avl.sfz");
    synth.noteOn(0, 36, 24);
    synth.noteOn(0, 36, 89);
    REQUIRE(synth.getNumActiveVoices() == 2);
    synth.allSoundOff();
    REQUIRE(synth.getNumActiveVoices() == 0);
}

TEST_CASE("[Synth] Change the number of voice while playing")
{
    sfz::Synth synth;
    synth.setSamplesPerBlock(blockSize);
    sfz::AudioBuffer<float> buffer { 2, blockSize };
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/groups_avl.sfz");

    synth.noteOn(0, 36, 24);
    synth.noteOn(0, 36, 89);
    synth.renderBlock(buffer);
    REQUIRE(synth.getNumActiveVoices() == 2);
    synth.setNumVoices(8);
    REQUIRE(synth.getNumActiveVoices() == 0);
    REQUIRE(synth.getNumVoices() == 8);
}

TEST_CASE("[Synth] Check that the sample per block and sample rate are actually propagated to all voices even on recreation")
{
    sfz::Synth synth;
    synth.setSamplesPerBlock(256);
    synth.setSampleRate(96000);
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        REQUIRE( synth.getVoiceView(i)->getSamplesPerBlock() == 256 );
        REQUIRE( synth.getVoiceView(i)->getSampleRate() == 96000.0f );
    }
    synth.setNumVoices(8);
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        REQUIRE( synth.getVoiceView(i)->getSamplesPerBlock() == 256 );
        REQUIRE( synth.getVoiceView(i)->getSampleRate() == 96000.0f );
    }
    synth.setSamplesPerBlock(128);
    synth.setSampleRate(48000);
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        REQUIRE( synth.getVoiceView(i)->getSamplesPerBlock() == 128 );
        REQUIRE( synth.getVoiceView(i)->getSampleRate() == 48000.0f );
    }
    synth.setNumVoices(64);
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        REQUIRE( synth.getVoiceView(i)->getSamplesPerBlock() == 128 );
        REQUIRE( synth.getVoiceView(i)->getSampleRate() == 48000.0f );
    }
}

TEST_CASE("[Synth] Check that we can change the size of the preload before and after loading")
{
    sfz::Synth synth;
    synth.setPreloadSize(512);
    synth.setSamplesPerBlock(blockSize);
    sfz::AudioBuffer<float> buffer { 2, blockSize };
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/groups_avl.sfz");
    synth.setPreloadSize(1024);

    synth.noteOn(0, 36, 24);
    synth.noteOn(0, 36, 89);
    synth.renderBlock(buffer);
    synth.setPreloadSize(2048);
    synth.renderBlock(buffer);
}

TEST_CASE("[Synth] Check that we can change the oversampling factor before and after loading")
{
    sfz::Synth synth;
    synth.setOversamplingFactor(sfz::Oversampling::x2);
    synth.setSamplesPerBlock(blockSize);
    sfz::AudioBuffer<float> buffer { 2, blockSize };
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/groups_avl.sfz");
    synth.setOversamplingFactor(sfz::Oversampling::x4);

    synth.noteOn(0, 36, 24);
    synth.noteOn(0, 36, 89);
    synth.renderBlock(buffer);
    synth.setOversamplingFactor(sfz::Oversampling::x2);
    synth.renderBlock(buffer);
}


TEST_CASE("[Synth] All notes offs/all sounds off")
{
    sfz::Synth synth;
    synth.setNumVoices(8);
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/sound_off.sfz");
    synth.noteOn(0, 60, 63);
    synth.noteOn(0, 62, 63);
    REQUIRE( synth.getNumActiveVoices() == 2 );
    synth.cc(0, 120, 63);
    REQUIRE( synth.getNumActiveVoices() == 0 );

    synth.noteOn(0, 62, 63);
    synth.noteOn(0, 60, 63);
    REQUIRE( synth.getNumActiveVoices() == 2 );
    synth.cc(0, 123, 63);
    REQUIRE( synth.getNumActiveVoices() == 0 );
}

TEST_CASE("[Synth] Reset all controllers")
{
    sfz::Synth synth;
    const auto& midiState = synth.getResources().midiState;
    synth.cc(0, 12, 64);
    REQUIRE(midiState.getCCValue(12) == 64_norm);
    synth.cc(0, 121, 64);
    REQUIRE(midiState.getCCValue(12) == 0_norm);
}

TEST_CASE("[Synth] Releasing before the EG started smoothing (initial delay) kills the voice")
{
    sfz::Synth synth;
    synth.setSamplesPerBlock(1024);
    synth.setNumVoices(1);
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/delay_release.sfz");
    synth.noteOn(0, 60, 63);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
    synth.noteOff(100, 60, 63);
    REQUIRE( synth.getVoiceView(0)->isFree() );
    synth.noteOn(200, 60, 63);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
    synth.noteOff(1000, 60, 63);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
}

TEST_CASE("[Synth] Releasing after the initial and normal mode does not trigger a fast release")
{
    sfz::Synth synth;
    synth.setSamplesPerBlock(1024);
    sfz::AudioBuffer<float> buffer(2, 1024);
    synth.setNumVoices(1);
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/delay_release.sfz");
    synth.noteOn(200, 60, 63);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
    synth.renderBlock(buffer);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
    synth.noteOff(0, 60, 63);
    synth.renderBlock(buffer);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
}

TEST_CASE("[Synth] Trigger=release and an envelope properly kills the voice at the end of the envelope")
{
    sfz::Synth synth;
    synth.setSampleRate(48000);
    synth.setSamplesPerBlock(480);
    sfz::AudioBuffer<float> buffer(2, 480);
    synth.setNumVoices(1);
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/envelope_trigger_release.sfz");
    synth.noteOn(0, 60, 63);
    synth.noteOff(0, 60, 63);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
    synth.renderBlock(buffer); // Attack (0.02)
    synth.renderBlock(buffer);
    synth.renderBlock(buffer); // Decay (0.02)
    synth.renderBlock(buffer);
    synth.renderBlock(buffer); // Release (0.1)
    REQUIRE(synth.getVoiceView(0)->releasedOrFree());
    // Release is 0.1s
    for (int i = 0; i < 10; ++i)
        synth.renderBlock(buffer);
    REQUIRE( synth.getVoiceView(0)->isFree() );
}

TEST_CASE("[Synth] Trigger=release_key and an envelope properly kills the voice at the end of the envelope")
{
    sfz::Synth synth;
    synth.setSampleRate(48000);
    synth.setSamplesPerBlock(480);
    sfz::AudioBuffer<float> buffer(2, 480);
    synth.setNumVoices(1);
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/envelope_trigger_release_key.sfz");
    synth.noteOn(0, 60, 63);
    synth.noteOff(0, 60, 63);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
    synth.renderBlock(buffer); // Attack (0.02)
    synth.renderBlock(buffer);
    synth.renderBlock(buffer); // Decay (0.02)
    synth.renderBlock(buffer);
    synth.renderBlock(buffer); // Release (0.1)
    REQUIRE(synth.getVoiceView(0)->releasedOrFree());
    // Release is 0.1s
    for (int i = 0; i < 10; ++i)
        synth.renderBlock(buffer);
    REQUIRE( synth.getVoiceView(0)->isFree() );
}

TEST_CASE("[Synth] loopmode=one_shot and an envelope properly kills the voice at the end of the envelope")
{
    sfz::Synth synth;
    synth.setSampleRate(48000);
    synth.setSamplesPerBlock(480);
    sfz::AudioBuffer<float> buffer(2, 480);
    synth.setNumVoices(1);
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/envelope_one_shot.sfz");
    synth.noteOn(0, 60, 63);
    synth.noteOff(0, 60, 63);
    REQUIRE( !synth.getVoiceView(0)->isFree() );
    synth.renderBlock(buffer); // Attack (0.02)
    synth.renderBlock(buffer);
    synth.renderBlock(buffer); // Decay (0.02)
    synth.renderBlock(buffer);
    synth.renderBlock(buffer); // Release (0.1)
    REQUIRE(synth.getVoiceView(0)->releasedOrFree());
    // Release is 0.1s
    for (int i = 0; i < 10; ++i)
        synth.renderBlock(buffer);
    REQUIRE( synth.getVoiceView(0)->isFree() );
}

TEST_CASE("[Synth] Number of effect buses and resetting behavior")
{
    sfz::Synth synth;
    synth.setSamplesPerBlock(blockSize);
    sfz::AudioBuffer<float> buffer { 2, blockSize };

    REQUIRE( synth.getEffectBusView(0) == nullptr); // No effects at first
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/base.sfz");
    REQUIRE( synth.getEffectBusView(0) != nullptr); // We have a main bus
    // Check that we can render blocks
    for (int i = 0; i < 100; ++i)
        synth.renderBlock(buffer);

    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/bitcrusher_2.sfz");
    REQUIRE( synth.getEffectBusView(0) != nullptr); // We have a main bus
    REQUIRE( synth.getEffectBusView(1) != nullptr); // and an FX bus
    // Check that we can render blocks
    for (int i = 0; i < 100; ++i)
        synth.renderBlock(buffer);

    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/base.sfz");
    REQUIRE( synth.getEffectBusView(0) != nullptr); // We have a main bus
    REQUIRE( synth.getEffectBusView(1) == nullptr); // and no FX bus
    // Check that we can render blocks
    for (int i = 0; i < 100; ++i)
        synth.renderBlock(buffer);

    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/bitcrusher_3.sfz");
    REQUIRE( synth.getEffectBusView(0) != nullptr); // We have a main bus
    REQUIRE( synth.getEffectBusView(1) == nullptr); // empty/uninitialized fx bus
    REQUIRE( synth.getEffectBusView(2) == nullptr); // empty/uninitialized fx bus
    REQUIRE( synth.getEffectBusView(3) != nullptr); // and an FX bus (because we built up to fx3)
    REQUIRE( synth.getEffectBusView(3)->numEffects() == 1);
    // Check that we can render blocks
    for (int i = 0; i < 100; ++i)
        synth.renderBlock(buffer);
}

TEST_CASE("[Synth] No effect in the main bus")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/base.sfz");
    auto bus = synth.getEffectBusView(0);
    REQUIRE( bus != nullptr); // We have a main bus
    REQUIRE( bus->numEffects() == 0 );
    REQUIRE( bus->gainToMain() == 1 );
    REQUIRE( bus->gainToMix() == 0 );
}

TEST_CASE("[Synth] One effect")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/bitcrusher_1.sfz");
    auto bus = synth.getEffectBusView(0);
    REQUIRE( bus != nullptr); // We have a main bus
    REQUIRE( bus->numEffects() == 1 );
    REQUIRE( bus->gainToMain() == 1 );
    REQUIRE( bus->gainToMix() == 0 );
}

TEST_CASE("[Synth] Effect on a second bus")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/bitcrusher_2.sfz");
    auto bus = synth.getEffectBusView(0);
    REQUIRE( bus != nullptr); // We have a main bus
    REQUIRE( bus->numEffects() == 0 );
    REQUIRE( bus->gainToMain() == 0.5 );
    REQUIRE( bus->gainToMix() == 0 );
    bus = synth.getEffectBusView(1);
    REQUIRE( bus != nullptr);
    REQUIRE( bus->numEffects() == 1 );
    REQUIRE( bus->gainToMain() == 0.5 );
    REQUIRE( bus->gainToMix() == 0 );
}


TEST_CASE("[Synth] Effect on a third bus")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/bitcrusher_3.sfz");
    auto bus = synth.getEffectBusView(0);
    REQUIRE( bus != nullptr); // We have a main bus
    REQUIRE( bus->numEffects() == 0 );
    REQUIRE( bus->gainToMain() == 0.5 );
    REQUIRE( bus->gainToMix() == 0 );
    bus = synth.getEffectBusView(3);
    REQUIRE( bus != nullptr);
    REQUIRE( bus->numEffects() == 1 );
    REQUIRE( bus->gainToMain() == 0.5 );
    REQUIRE( bus->gainToMix() == 0 );
}

TEST_CASE("[Synth] Gain to mix")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/Effects/to_mix.sfz");
    auto bus = synth.getEffectBusView(0);
    REQUIRE( bus != nullptr); // We have a main bus
    REQUIRE( bus->numEffects() == 0 );
    REQUIRE( bus->gainToMain() == 1 );
    REQUIRE( bus->gainToMix() == 0 );
    bus = synth.getEffectBusView(1);
    REQUIRE( bus != nullptr);
    REQUIRE( bus->numEffects() == 1 );
    REQUIRE( bus->gainToMain() == 0 );
    REQUIRE( bus->gainToMix() == 0.5 );
}

TEST_CASE("[Synth] group polyphony limits")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/polyphony.sfz");
    synth.noteOn(0, 65, 64);
    synth.noteOn(0, 65, 64);
    synth.noteOn(0, 65, 64);
    REQUIRE(synth.getNumActiveVoices() == 2); // group polyphony should block the last note
}

TEST_CASE("[Synth] Self-masking")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/polyphony.sfz");
    synth.noteOn(0, 64, 63);
    synth.noteOn(0, 64, 62);
    synth.noteOn(0, 64, 64);
    REQUIRE(synth.getNumActiveVoices() == 3); // One of these is releasing
    REQUIRE(synth.getVoiceView(0)->getTriggerValue() == 63_norm);
    REQUIRE(!synth.getVoiceView(0)->releasedOrFree());
    REQUIRE(synth.getVoiceView(1)->getTriggerValue() == 62_norm);
    REQUIRE(synth.getVoiceView(1)->releasedOrFree()); // The lowest velocity voice is the masking candidate
    REQUIRE(synth.getVoiceView(2)->getTriggerValue() == 64_norm);
    REQUIRE(!synth.getVoiceView(2)->releasedOrFree());
}

TEST_CASE("[Synth] Not self-masking")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/polyphony.sfz");
    synth.noteOn(0, 66, 63);
    synth.noteOn(0, 66, 62);
    synth.noteOn(0, 66, 64);
    REQUIRE(synth.getNumActiveVoices() == 3); // One of these is releasing
    REQUIRE(synth.getVoiceView(0)->getTriggerValue() == 63_norm);
    REQUIRE(synth.getVoiceView(0)->releasedOrFree()); // The first encountered voice is the masking candidate
    REQUIRE(synth.getVoiceView(1)->getTriggerValue() == 62_norm);
    REQUIRE(!synth.getVoiceView(1)->releasedOrFree());
    REQUIRE(synth.getVoiceView(2)->getTriggerValue() == 64_norm);
    REQUIRE(!synth.getVoiceView(2)->releasedOrFree());
}

TEST_CASE("[Synth] Polyphony in master")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/master_polyphony.sfz");
    synth.noteOn(0, 65, 64);
    synth.noteOn(0, 65, 64);
    synth.noteOn(0, 65, 64);
    REQUIRE(synth.getNumActiveVoices() == 2); // group polyphony should block the last note
    synth.allSoundOff();
    REQUIRE(synth.getNumActiveVoices() == 0);
    synth.noteOn(0, 63, 64);
    synth.noteOn(0, 63, 64);
    synth.noteOn(0, 63, 64);
    REQUIRE(synth.getNumActiveVoices() == 2); // group polyphony should block the last note
    synth.allSoundOff();
    REQUIRE(synth.getNumActiveVoices() == 0);
    synth.noteOn(0, 61, 64);
    synth.noteOn(0, 61, 64);
    synth.noteOn(0, 61, 64);
    REQUIRE(synth.getNumActiveVoices() == 3);
}

TEST_CASE("[Synth] Basic curves")
{
    sfz::Synth synth;
    const auto& curves = synth.getResources().curves;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/curves.sfz");
    REQUIRE( synth.getNumCurves() == 19 );
    REQUIRE( curves.getCurve(18).evalCC7(127) == 1.0f );
    REQUIRE( curves.getCurve(18).evalCC7(95) == 0.5f );
    REQUIRE( curves.getCurve(17).evalCC7(100) == 1.0f );
    REQUIRE( curves.getCurve(17).evalCC7(95) == 0.5f );
    // Default linear
    REQUIRE( curves.getCurve(16).evalCC7(63) == Approx(63_norm) );
}

TEST_CASE("[Synth] Velocity points")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/velocity_endpoints.sfz");
    REQUIRE( !synth.getRegionView(0)->velocityPoints.empty());
    REQUIRE( synth.getRegionView(0)->velocityPoints[0].first == 64 );
    REQUIRE( synth.getRegionView(0)->velocityPoints[0].second == 1.0_a );
    REQUIRE( !synth.getRegionView(1)->velocityPoints.empty());
    REQUIRE( synth.getRegionView(1)->velocityPoints[0].first == 64 );
    REQUIRE( synth.getRegionView(1)->velocityPoints[0].second == 1.0_a );
}

TEST_CASE("[Synth] velcurve")
{
    sfz::Synth synth;
    synth.loadSfzFile(fs::current_path() / "tests/TestFiles/velocity_endpoints.sfz");
    REQUIRE( synth.getRegionView(0)->velocityCurve(0_norm) == 0.0_a );
    REQUIRE( synth.getRegionView(0)->velocityCurve(32_norm) == Approx(0.5f).margin(1e-2) );
    REQUIRE( synth.getRegionView(0)->velocityCurve(64_norm) == 1.0_a );
    REQUIRE( synth.getRegionView(0)->velocityCurve(96_norm) == 1.0_a );
    REQUIRE( synth.getRegionView(0)->velocityCurve(127_norm) == 1.0_a );
    REQUIRE( synth.getRegionView(1)->velocityCurve(0_norm) == 1.0_a );
    REQUIRE( synth.getRegionView(1)->velocityCurve(32_norm) == 1.0_a );
    REQUIRE( synth.getRegionView(1)->velocityCurve(64_norm) == 1.0_a );
    REQUIRE( synth.getRegionView(1)->velocityCurve(96_norm) == Approx(0.5f).margin(1e-2) );
    REQUIRE( synth.getRegionView(1)->velocityCurve(127_norm) == 0.0_a );
}

TEST_CASE("[Synth] Region by identifier")
{
    sfz::Synth synth;
    synth.loadSfzString("regionByIdentifier.sfz", R"(
<region>sample=*sine
<region>sample=*sine
<region>sample=doesNotExist.wav
<region>sample=*sine
<region>sample=doesNotExist.wav
<region>sample=*sine
)");

    REQUIRE(synth.getNumRegions() == 4);
    REQUIRE(synth.getRegionView(0) == synth.getRegionById(NumericId<sfz::Region>{0}));
    REQUIRE(synth.getRegionView(1) == synth.getRegionById(NumericId<sfz::Region>{1}));
    REQUIRE(nullptr == synth.getRegionById(NumericId<sfz::Region>{2}));
    REQUIRE(synth.getRegionView(2) == synth.getRegionById(NumericId<sfz::Region>{3}));
    REQUIRE(nullptr == synth.getRegionById(NumericId<sfz::Region>{4}));
    REQUIRE(synth.getRegionView(3) == synth.getRegionById(NumericId<sfz::Region>{5}));
    REQUIRE(nullptr == synth.getRegionById(NumericId<sfz::Region>{6}));
    REQUIRE(nullptr == synth.getRegionById(NumericId<sfz::Region>{}));
}

TEST_CASE("[Synth] Sister voices")
{
    sfz::Synth synth;
    synth.loadSfzString(fs::current_path(),
R"(<region> key=61 sample=*sine
<region> key=62 sample=*sine
<region> key=62 sample=*sine
<region> key=63 sample=*saw
<region> key=63 sample=*saw
<region> key=63 sample=*saw)");
    synth.noteOn(0, 61, 85);
    REQUIRE( synth.getVoiceView(0)->countSisterVoices() == 1 );
    REQUIRE( synth.getVoiceView(0)->getNextSisterVoice() == synth.getVoiceView(0) );
    REQUIRE( synth.getVoiceView(0)->getPreviousSisterVoice() == synth.getVoiceView(0) );
    synth.noteOn(0, 62, 85);
    REQUIRE( synth.getNumActiveVoices() == 3 );
    REQUIRE( synth.getVoiceView(1)->countSisterVoices() == 2 );
    REQUIRE( synth.getVoiceView(1)->getNextSisterVoice() == synth.getVoiceView(2) );
    REQUIRE( synth.getVoiceView(1)->getPreviousSisterVoice() == synth.getVoiceView(2) );
    REQUIRE( synth.getVoiceView(2)->countSisterVoices() == 2 );
    REQUIRE( synth.getVoiceView(2)->getNextSisterVoice() == synth.getVoiceView(1) );
    REQUIRE( synth.getVoiceView(2)->getPreviousSisterVoice() == synth.getVoiceView(1) );
    synth.noteOn(0, 63, 85);
    REQUIRE( synth.getNumActiveVoices() == 6 );
    REQUIRE( synth.getVoiceView(3)->countSisterVoices() == 3 );
    REQUIRE( synth.getVoiceView(3)->getNextSisterVoice() == synth.getVoiceView(4) );
    REQUIRE( synth.getVoiceView(3)->getPreviousSisterVoice() == synth.getVoiceView(5) );
    REQUIRE( synth.getVoiceView(4)->countSisterVoices() == 3 );
    REQUIRE( synth.getVoiceView(4)->getNextSisterVoice() == synth.getVoiceView(5) );
    REQUIRE( synth.getVoiceView(4)->getPreviousSisterVoice() == synth.getVoiceView(3) );
    REQUIRE( synth.getVoiceView(5)->countSisterVoices() == 3 );
    REQUIRE( synth.getVoiceView(5)->getNextSisterVoice() == synth.getVoiceView(3) );
    REQUIRE( synth.getVoiceView(5)->getPreviousSisterVoice() == synth.getVoiceView(4) );
}

TEST_CASE("[Synth] Apply function on sisters")
{
    sfz::Synth synth;
    sfz::AudioBuffer<float> buffer { 2, 256 };
    synth.loadSfzString(fs::current_path(),
R"(<region> key=63 sample=*saw
<region> key=63 sample=*saw
<region> key=63 sample=*saw)");
    synth.noteOn(0, 63, 85);
    REQUIRE( synth.getVoiceView(0)->countSisterVoices() == 3 );
    float start = 1.0f;
    synth.getVoiceView(0)->applyToSisterRing([&](const sfz::Voice* v) {
        start += static_cast<float>(v->getTriggerNumber());
    });
    REQUIRE( start == 1.0f + 3.0f * 63.0f );
}


TEST_CASE("[Synth] Sisters and off-by")
{
    sfz::Synth synth;
    sfz::AudioBuffer<float> buffer { 2, 256 };
    synth.loadSfzString(fs::current_path(),
R"(<region> key=62 sample=*sine
<group> group=1 off_by=2 <region> key=62 sample=*sine
<group> group=2 <region> key=63 sample=*saw)");
    synth.noteOn(0, 62, 85);
    REQUIRE( synth.getNumActiveVoices() == 2 );
    REQUIRE( synth.getVoiceView(0)->countSisterVoices() == 2 );
    synth.renderBlock(buffer);
    REQUIRE( synth.getNumActiveVoices() == 2 );
    synth.noteOn(0, 63, 85);
    REQUIRE( synth.getNumActiveVoices() == 3 );
    synth.renderBlock(buffer);
    REQUIRE( synth.getNumActiveVoices() == 2 );
    REQUIRE( synth.getVoiceView(0)->countSisterVoices() == 1 );
}

TEST_CASE("[Synth] Hierarchy 1")
{
    sfz::Synth synth;
    synth.loadSfzString(fs::current_path(),
R"(
<region> key=61 sample=*sine
<region> key=62 sample=*sine
<region> key=63 sample=*sine
)");
    REQUIRE( synth.getRegionSetView(0)->getParent() == nullptr );
    auto regions = synth.getRegionSetView(0)->getRegions();
    REQUIRE( regions.size() == 3 );
    for (auto* region : regions)
        REQUIRE( region->parent == synth.getRegionSetView(0) );
}

TEST_CASE("[Synth] Hierarchy 2")
{
    sfz::Synth synth;
    synth.loadSfzString(fs::current_path(),
R"(
<region> key=61 sample=*sine
<group>
<region> key=62 sample=*sine
<region> key=62 sample=*sine
<group>
<region> key=63 sample=*sine
<region> key=63 sample=*sine
<region> key=63 sample=*sine
)");
    const auto* rootSet = synth.getRegionSetView(0);
    REQUIRE( rootSet->getRegions().size() == 1 );
    REQUIRE( rootSet->getSubsets().size() == 2 );
    REQUIRE( rootSet->getSubsets()[0] == synth.getRegionSetView(1) );
    REQUIRE( rootSet->getSubsets()[0]->getParent() == rootSet );
    REQUIRE( rootSet->getSubsets()[1] == synth.getRegionSetView(2) );
    REQUIRE( rootSet->getSubsets()[1]->getParent() == rootSet );
    REQUIRE( rootSet->getSubsets()[0]->getRegions().size() == 2 );
    REQUIRE( rootSet->getSubsets()[1]->getRegions().size() == 3 );
    for (const auto* region: rootSet->getSubsets()[0]->getRegions())
        REQUIRE(region->parent == rootSet->getSubsets()[0]);
    for (const auto* region: rootSet->getSubsets()[1]->getRegions())
        REQUIRE(region->parent == rootSet->getSubsets()[1]);
}


TEST_CASE("[Synth] Hierarchy 3")
{
    sfz::Synth synth;
    synth.loadSfzString(fs::current_path(),
R"(
<region> key=61 sample=*sine
<group>
<region> key=62 sample=*sine
<region> key=62 sample=*sine
<master>
<region> key=63 sample=*sine
<region> key=63 sample=*sine
<region> key=63 sample=*sine
<group>
<region> key=64 sample=*sine
<region> key=64 sample=*sine
<region> key=64 sample=*sine
<region> key=64 sample=*sine
)");
    const auto* rootSet = synth.getRegionSetView(0);
    REQUIRE( rootSet->getRegions().size() == 1 );
    REQUIRE( rootSet->getSubsets().size() == 2 );
    REQUIRE( rootSet->getSubsets()[0] == synth.getRegionSetView(1) );
    REQUIRE( rootSet->getSubsets()[0]->getParent() == rootSet );
    REQUIRE( rootSet->getSubsets()[0]->getRegions().size() == 2 );
    REQUIRE( rootSet->getSubsets()[1] == synth.getRegionSetView(2) );
    REQUIRE( rootSet->getSubsets()[1]->getParent() == rootSet );
    REQUIRE( rootSet->getSubsets()[1]->getRegions().size() == 3 );
    REQUIRE( rootSet->getSubsets()[1]->getSubsets().size() == 1 );
    REQUIRE( rootSet->getSubsets()[1]->getSubsets()[0] == synth.getRegionSetView(3) );
    REQUIRE( rootSet->getSubsets()[1]->getSubsets()[0]->getParent() == synth.getRegionSetView(2) );
    REQUIRE( rootSet->getSubsets()[1]->getSubsets()[0]->getRegions().size() == 4 );
    REQUIRE( rootSet->getSubsets()[1]->getSubsets()[0]->getSubsets().size() == 0 );
}

TEST_CASE("[Synth] Polyphony in hierarchy")
{
    sfz::Synth synth;
    synth.loadSfzString(fs::current_path(),
R"(
<region> key=61 sample=*sine polyphony=2
<group> polyphony=2
<region> key=62 sample=*sine
<master> polyphony=3
<region> key=63 sample=*sine
<region> key=63 sample=*sine
<region> key=63 sample=*sine
<group> polyphony=4
<region> key=64 sample=*sine polyphony=5
<region> key=64 sample=*sine
<region> key=64 sample=*sine
<region> key=64 sample=*sine
)");
    REQUIRE( synth.getRegionView(0)->polyphony == 2 );
    REQUIRE( synth.getRegionSetView(1)->getPolyphonyLimit() == 2 );
    REQUIRE( synth.getRegionView(1)->polyphony == 2 );
    REQUIRE( synth.getRegionSetView(2)->getPolyphonyLimit() == 3 );
    REQUIRE( synth.getRegionSetView(2)->getRegions()[0]->polyphony == 3 );
    REQUIRE( synth.getRegionSetView(3)->getPolyphonyLimit() == 4 );
    REQUIRE( synth.getRegionSetView(3)->getRegions()[0]->polyphony == 5 );
    REQUIRE( synth.getRegionSetView(3)->getRegions()[1]->polyphony == 4 );
}

TEST_CASE("[Synth] Polyphony groups")
{
    sfz::Synth synth;
    synth.loadSfzString(fs::current_path(),
R"(
<group> polyphony=2
<region> key=62 sample=*sine
<group> group=1 polyphony=3
<region> key=63 sample=*sine
<region> key=63 sample=*sine group=2 polyphony=4
<region> key=63 sample=*sine group=4 polyphony=5
<group> group=4
<region> key=62 sample=*sine
)");
    REQUIRE( synth.getNumPolyphonyGroups() == 5 );
    REQUIRE( synth.getNumRegions() == 5 );
    REQUIRE( synth.getRegionView(0)->group == 0 );
    REQUIRE( synth.getRegionView(1)->group == 1 );
    REQUIRE( synth.getRegionView(2)->group == 2 );
    REQUIRE( synth.getRegionView(3)->group == 4 );
    REQUIRE( synth.getRegionView(3)->polyphony == 5 );
    REQUIRE( synth.getRegionView(4)->group == 4 );
    REQUIRE( synth.getPolyphonyGroupView(1)->getPolyphonyLimit() == 3 );
    REQUIRE( synth.getPolyphonyGroupView(2)->getPolyphonyLimit() == 4 );
    REQUIRE( synth.getPolyphonyGroupView(3)->getPolyphonyLimit() == sfz::config::maxVoices );
    REQUIRE( synth.getPolyphonyGroupView(4)->getPolyphonyLimit() == 5 );
}
