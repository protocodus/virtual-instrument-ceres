// Processor contracts: state, MIDI rendering and the editor.
// CERES_EDITOR_SNAPSHOT=<file.png> also writes the editor screenshot the
// render tier checks and the main build commits.

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    if (! condition)
    {
        ++failures;
        std::fprintf (stderr, "FAIL %s\n", what.toRawUTF8());
    }
}

// Plays a two-note phrase `block` frames at a time.
std::vector<float> render (CeresAudioProcessor& p, int block, double rate = 48000.0)
{
    constexpr int total = 24000;
    p.setRateAndBufferSizeDetails (rate, block);
    p.prepareToPlay (rate, block);
    const std::vector<std::pair<int, juce::MidiMessage>> events {
        { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) },
        { 3001, juce::MidiMessage::noteOn (1, 67, (juce::uint8) 127) },
        { 15013, juce::MidiMessage::noteOff (1, 60) },
        { 15013, juce::MidiMessage::noteOff (1, 67) },
    };
    std::vector<float> out;
    juce::AudioBuffer<float> buffer (2, block);
    for (int start = 0; start < total; start += block)
    {
        const int frames = std::min (block, total - start);
        buffer.setSize (2, frames, false, false, true);
        buffer.clear();
        juce::MidiBuffer midi;
        for (const auto& [at, message] : events)
            if (at >= start && at < start + frames)
                midi.addEvent (message, at - start);
        p.processBlock (buffer, midi);
        for (int i = 0; i < frames; ++i)
        {
            out.push_back (buffer.getSample (0, i));
            out.push_back (buffer.getSample (1, i));
        }
    }
    return out;
}

void stateContract()
{
    CeresAudioProcessor a, b;
    juce::MemoryBlock saved;
    a.getStateInformation (saved);
    check (saved.getSize() > 0, "state is written");
    b.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    const char garbage[] = "not a Ceres state";
    b.setStateInformation (garbage, sizeof garbage);
    juce::MemoryBlock again;
    b.getStateInformation (again);
    check (again == saved, "state survives a round trip and a foreign chunk");
}

void renderContract()
{
    CeresAudioProcessor whole, pieces;
    const auto a = render (whole, 512), b = render (pieces, 64);
    bool finite = true;
    for (float x : a)
        finite = finite && std::isfinite (x);
    check (finite, "rendered samples are finite");
    check (a == b, "512- and 64-frame blocks render identical samples");
    check (whole.getLatencySamples() >= 0, "latency is reported");
}

void editorContract()
{
    CeresAudioProcessor p;
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    check (image.getPixelAt (4, 4).getAlpha() == 255, "the editor paints an opaque panel");

    // The main build asks for the committed screenshot through this variable,
    // so the README image tracks the real editor.
    const auto path = juce::SystemStats::getEnvironmentVariable ("CERES_EDITOR_SNAPSHOT", {});
    if (path.isNotEmpty())
    {
        const juce::File file (path);
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::FileOutputStream output (file);
        juce::PNGImageFormat png;
        check (output.openedOk() && png.writeImageToStream (image, output), "write the editor snapshot");
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    stateContract();
    renderContract();
    editorContract();
    std::printf ("Ceres processor contracts: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
