// Renders one README demo per factory preset (README G3) from the committed
// phrases in Docs/audio/midi, through the JUCE-free engine. The contract and
// the fixed rate, block size and seed live in the shared PresetDemoRenderer.h.
//
// Ceres has no presets yet, so the list below is empty and the audio tier
// fails by design until the first preset and its phrase are added.

#include "CeresEngine.h"
#include "PresetDemoRenderer.h"

#include <memory>

namespace
{
class CeresVoice final : public protocodus::demos::Voice
{
public:
    explicit CeresVoice (double sampleRate) { engine.prepare (sampleRate); }

    void Midi (const std::uint8_t* bytes, int size) override
    {
        const int kind = bytes[0] & 0xf0;
        const int data1 = size > 1 ? bytes[1] : 0, data2 = size > 2 ? bytes[2] : 0;
        if (kind == 0x80 || (kind == 0x90 && data2 == 0)) engine.noteOff (data1);
        else if (kind == 0x90) engine.noteOn (data1, data2);
    }

    void Render (float* left, float* right, int frames) override
    {
        engine.process (left, right, static_cast<std::size_t> (frames));
    }

private:
    ceres::CeresEngine engine;
};
} // namespace

int main (int argc, char** argv)
{
    protocodus::demos::Instrument instrument;
    instrument.name = "Ceres";
    instrument.ceiling = 1.0f;
    instrument.open = [] (std::size_t, double sampleRate, int, std::uint32_t)
    {
        return std::make_unique<CeresVoice> (sampleRate);
    };
    return protocodus::demos::Main (argc, argv, instrument);
}
