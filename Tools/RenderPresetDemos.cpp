// Renders the CIRCUIT RESEARCH preset table, unchanged, from original
// committed MIDI phrases, through ceres::Synth directly: the engine and seed
// the JUCE plug-in plays, whose programs are these presets.
// Shared PresetDemoRenderer.h fixes sample rate, block size, seed, tail and
// finite/audible/ceiling/determinism checks. No gain trim or normalization.

#include "ceres.hpp"
#include "PresetDemoRenderer.h"

#include <memory>

namespace
{
class CeresVoice final : public protocodus::demos::Voice
{
public:
    CeresVoice (const std::string& presetName, double sampleRate, std::uint32_t seed)
        : engine (sampleRate, seed)
    {
        engine.setPatch (ceres::preset (presetName));
    }

    void Midi (const std::uint8_t* bytes, int size) override
    {
        if (bytes == nullptr || size < 1) return;
        const int kind = bytes[0] & 0xf0;
        const int data1 = size > 1 ? bytes[1] : 0, data2 = size > 2 ? bytes[2] : 0;
        if (size >= 3 && (kind == 0x80 || (kind == 0x90 && data2 == 0))) engine.noteOff (data1);
        else if (size >= 3 && kind == 0x90) engine.noteOn (data1, data2 / 127.0);
        else if (size >= 3 && kind == 0xb0)
        {
            if (data1 == 64) engine.sustain (data2 >= 64);
            else if (data1 == 123) engine.allNotesOff();
            else if (data1 == 120) engine.allSoundOff();
        }
        else if (size >= 2 && kind == 0xd0) engine.pressure (data1 / 127.0);
        else if (size >= 3 && kind == 0xe0)
            engine.pitchBend (((data2 << 7 | data1) - 8192) * (2.0 / 8192.0));
    }

    void Render (float* left, float* right, int frames) override
    {
        // The real engine is mono. Identical copies expose it without an
        // invented pan/spread stage or an amplitude change.
        for (int frame = 0; frame < frames; ++frame)
            left[frame] = right[frame] = engine.process();
    }

private:
    ceres::Synth engine;
};
} // namespace

int main (int argc, char** argv)
{
    protocodus::demos::Instrument instrument;
    instrument.name = "Ceres standalone circuit research presets";
    instrument.presets = ceres::presetNames();
    instrument.ceiling = 1.0f;
    // U7 perf budget: 0 reports without failing until the CI runner's
    // median is measured.
    instrument.cpuBudget = 0.0;
    instrument.open = [names = instrument.presets] (std::size_t preset, double sampleRate, int, std::uint32_t seed)
    {
        return std::make_unique<CeresVoice> (names.at (preset), sampleRate, seed);
    };
    return protocodus::demos::Main (argc, argv, instrument);
}
