// U4 input fuzzing of the JUCE-free circuit engine, ceres::Synth, through the
// shared portfolio harness (Tests/FuzzHarness.h is a byte-identical copy of
// the master; change it in reason-rack-extensions). Every input starts from a
// fresh engine, so a saved crash replays alone; commands and rendered frames
// are bounded whatever the input length.
//
// Controls arrive the way a front end sends them: any key the parameter
// descriptors list, or a whole Patch, with any float including NaN, the
// infinities and values far outside each range, which the engine must bound.
// A sample rate outside 8-384 kHz must be refused with std::invalid_argument.
#include "DSP/ceres.hpp"
#include "FuzzHarness.h"

#include <memory>
#include <stdexcept>

extern "C" int LLVMFuzzerTestOneInput (const std::uint8_t* data, std::size_t size)
{
    using namespace protocodus::fuzz;
    Input input { data, size };

    const double rate = input.Rate();
    const bool supported = rate >= 8000.0 && rate <= 384000.0;
    std::unique_ptr<ceres::Synth> engine;
    try
    {
        engine = std::make_unique<ceres::Synth> (rate, input.Word());
    }
    catch (const std::invalid_argument&)
    {
        Require (! supported);
        return 0;
    }
    Require (supported);

    const auto anyInt = [&] { return static_cast<int> (input.Word()); };
    // Mostly a position inside the key's range, otherwise any float scaled
    // up to well past every range.
    const auto anyValue = [&] (const ceres::ParameterInfo& info)
    {
        const double x = static_cast<double> (input.Scalar());
        return input.Byte() & 1 ? info.minimum + (info.maximum - info.minimum) * x : x * 1.0e6;
    };
    const auto anyKey = [&] { return ceres::parameterInfo (input.Pick (ceres::parameterCount())); };

    // Quality costs CPU: start from the lightest configuration and let the
    // input ask for others through configure().
    Require (engine->configure (1 + input.Pick (4), 1u << input.Pick (3)));
    ceres::Patch patch;

    constexpr std::size_t maxFrames = 256;
    Sentinels<maxFrames> block;
    for (int command = 0; command < 32 && ! input.Empty(); ++command)
    {
        switch (input.Pick (13))
        {
            case 0: engine->noteOn (anyInt() % 300 - 100, static_cast<double> (input.Scalar())); break;
            case 1: engine->noteOff (anyInt() % 300 - 100); break;
            case 2: engine->pressure (static_cast<double> (input.Scalar()) * (input.Byte() & 1 ? 1.0 : 100.0)); break;
            case 3: engine->pitchBend (static_cast<double> (input.Scalar()) * 200.0 - 100.0); break;
            case 4: engine->sustain (input.Byte() & 1); break;
            case 5: engine->allNotesOff(); break;
            case 6: engine->allSoundOff(); break;
            case 7: engine->reset(); break;
            case 8:
            {
                // Valid only while idle and before any sample; otherwise refused unchanged.
                const unsigned voices = static_cast<unsigned> (input.Pick (20)), factor = static_cast<unsigned> (input.Pick (6));
                const unsigned before = engine->polyphony(), beforeFactor = engine->oversamplingFactor();
                if (! engine->configure (voices, factor))
                    Require (engine->polyphony() == before && engine->oversamplingFactor() == beforeFactor);
                else
                    Require (engine->polyphony() == voices && engine->oversamplingFactor() == factor);
                break;
            }
            case 9:
            {
                const auto info = anyKey();
                Require (ceres::setParameter (patch, info.key, anyValue (info)));
                double stored = 0.0;
                Require (ceres::getParameter (patch, info.key, stored));
                Require (stored >= info.minimum && stored <= info.maximum);
                engine->setPatch (patch);
                break;
            }
            case 10:
            {
                // A Patch with members written directly, as a corrupt preset
                // might deliver them: setPatch must bound every one.
                ceres::Patch raw = patch;
                for (double* member : { &raw.saw, &raw.pulseWidth, &raw.hpCutoff, &raw.lpCutoff, &raw.lpResonance,
                                        &raw.filterAttack, &raw.ampRelease, &raw.globalRelease, &raw.sustainMode,
                                        &raw.volume, &raw.outputTrimDb, &raw.octave, &raw.driftCents,
                                        &raw.boardTemperatureRise, &raw.lfoRate, &raw.lfoWaveform, &raw.extLevel,
                                        &raw.ringRate, &raw.ringDepth, &raw.glideMode, &raw.glideRate, &raw.drive,
                                        &raw.resetMicroseconds })
                    if (input.Byte() & 1)
                        *member = static_cast<double> (input.Scalar()) * 1.0e6;
                engine->setPatch (raw);
                break;
            }
            case 11: engine->externalInput (static_cast<double> (input.Scalar()) * (input.Byte() & 1 ? 1.0 : 1.0e3)); break;
            default:
            {
                const auto info = anyKey();
                ceres::setParameter (patch, info.key, static_cast<double> (input.Scalar()));
                engine->setPatch (patch);
                break;
            }
        }
        const std::size_t frames = 1 + input.Word() % maxFrames;
        block.Arm();
        const bool external = input.Byte() & 1;
        for (std::size_t i = 0; i < frames; ++i)
        {
            if (external)
                engine->externalInput (static_cast<double> (input.Scalar()));
            block.Left()[i] = block.Right()[i] = engine->process();
        }
        block.Check (frames);
        Require (engine->activeVoices() <= engine->polyphony());
    }
    return 0;
}
