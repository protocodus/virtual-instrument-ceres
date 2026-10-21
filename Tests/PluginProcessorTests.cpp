// Processor contracts: parameters built from the engine's descriptors,
// programs, state, MIDI and EXT IN rendering against a directly driven
// ceres::Synth, block-partition invariance, deferred voice and quality
// changes, an allocation-free audio callback, buses and the editor.
// CERES_EDITOR_SNAPSHOT=<file.png> also writes the editor screenshot the
// render tier checks and the main build commits, tagged with the editor
// source it was rendered from (CERES_EDITOR_SOURCE_ID, see EditorRenderTests.py).

#include "PluginControls.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <vector>

#ifndef CERES_EDITOR_SOURCE_ID
 #define CERES_EDITOR_SOURCE_ID "unknown"
#endif

// The audio callback must not allocate: count what this thread allocates
// while `counting` is set.
namespace
{
thread_local bool counting = false;
thread_local std::size_t counted = 0;

void* allocate (std::size_t size)
{
    if (counting)
        ++counted;
    if (void* p = std::malloc (size != 0 ? size : 1))
        return p;
    throw std::bad_alloc();
}
} // namespace

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

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

constexpr double kRate = 48000.0;

juce::RangedAudioParameter& parameter (CeresAudioProcessor& p, const char* id)
{
    auto* found = p.state().getParameter (id);
    if (found == nullptr)
    {
        std::fprintf (stderr, "FAIL no parameter %s\n", id);
        std::abort();
    }
    return *found;
}

bool isChoice (juce::RangedAudioParameter& ranged)
{
    return dynamic_cast<juce::AudioParameterChoice*> (&ranged) != nullptr;
}

// The engine value a parameter stands for: a choice's index i is minimum + i.
double engineValue (CeresAudioProcessor& p, const ceres::ParameterInfo& info)
{
    auto& ranged = parameter (p, info.key);
    const double plain = ranged.convertFrom0to1 (ranged.getValue());
    return isChoice (ranged) ? info.minimum + plain : plain;
}

void setEngineValue (CeresAudioProcessor& p, const char* key, double value)
{
    for (std::size_t i = 0; i < ceres::parameterCount(); ++i)
    {
        const auto info = ceres::parameterInfo (i);
        if (std::strcmp (info.key, key) != 0)
            continue;
        auto& ranged = parameter (p, key);
        const double plain = isChoice (ranged) ? value - info.minimum : value;
        ranged.setValueNotifyingHost (ranged.convertTo0to1 (static_cast<float> (plain)));
        return;
    }
    check (false, juce::String ("no engine key ") + key);
}

void setSetting (CeresAudioProcessor& p, const char* id, float plain)
{
    auto& ranged = parameter (p, id);
    ranged.setValueNotifyingHost (ranged.convertTo0to1 (plain));
}

struct Timed
{
    int at;
    std::vector<juce::uint8> bytes;
};

Timed noteOn (int at, int note, int velocity) { return { at, { 0x90, (juce::uint8) note, (juce::uint8) velocity } }; }
Timed noteOff (int at, int note) { return { at, { 0x80, (juce::uint8) note, 0 } }; }
Timed controller (int at, int number, int value) { return { at, { 0xb0, (juce::uint8) number, (juce::uint8) value } }; }
Timed pressure (int at, int value) { return { at, { 0xd0, (juce::uint8) value } }; }
Timed bend (int at, int value) { return { at, { 0xe0, (juce::uint8) (value & 0x7f), (juce::uint8) (value >> 7) } }; }

struct Played
{
    std::vector<float> left, right;
    std::size_t allocations = 0;
};

// Plays `events` (sorted by frame) and EXT IN (`ext` on its first channel,
// `extRight` on a second) `block` frames at a time. With `garbage` every
// channel starts each block full of noise, as a host's buffers may.
Played play (CeresAudioProcessor& p, int total, int block, const std::vector<Timed>& events,
             const std::vector<float>& ext = {}, const std::vector<float>& extRight = {}, bool garbage = false)
{
    const int inputs = p.getTotalNumInputChannels(), outputs = p.getTotalNumOutputChannels();
    const int channels = std::max (inputs, outputs);
    juce::AudioBuffer<float> buffer (channels, block);
    juce::MidiBuffer midi;
    midi.ensureSize (4096);
    juce::Random random (11);
    Played played;
    std::size_t next = 0;
    for (int start = 0; start < total; start += block)
    {
        const int frames = std::min (block, total - start);
        buffer.setSize (channels, frames, false, false, true);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < frames; ++i)
                buffer.setSample (c, i, garbage ? random.nextFloat() * 2.0f - 1.0f : 0.0f);
        for (int i = 0; i < frames && inputs > 0; ++i)
        {
            const auto at = static_cast<std::size_t> (start + i);
            buffer.setSample (0, i, at < ext.size() ? ext[at] : 0.0f);
            if (inputs > 1)
                buffer.setSample (1, i, at < extRight.size() ? extRight[at] : 0.0f);
        }
        midi.clear();
        for (; next < events.size() && events[next].at < start + frames; ++next)
            midi.addEvent (events[next].bytes.data(), static_cast<int> (events[next].bytes.size()),
                           events[next].at - start);
        counting = true;
        counted = 0;
        p.processBlock (buffer, midi);
        counting = false;
        played.allocations += counted;
        for (int i = 0; i < frames; ++i)
        {
            played.left.push_back (buffer.getSample (0, i));
            played.right.push_back (buffer.getSample (std::min (1, outputs - 1), i));
        }
    }
    return played;
}

void prepare (CeresAudioProcessor& p, int block, double rate = kRate)
{
    p.setRateAndBufferSizeDetails (rate, block);
    p.prepareToPlay (rate, block);
}

// The engine driven directly with the documented mapping: MIDI velocity and
// pressure /127, bend +/-2 semitones over +/-8192, CC64 at 64 and above, CC11
// scaling the expression parameter, CC120/121/123 as MIDI defines them, the
// same seed, configuration, patch and EXT IN samples. An independent
// statement of what the plug-in promises, not a call into it.
std::vector<float> reference (CeresAudioProcessor& p, int total, const std::vector<Timed>& events,
                              const std::vector<float>& ext = {}, double rate = kRate)
{
    ceres::Patch patch;
    double lever = 1.0;
    for (std::size_t i = 0; i < ceres::parameterCount(); ++i)
    {
        const auto info = ceres::parameterInfo (i);
        const double value = engineValue (p, info);
        if (std::strcmp (info.key, "expression") == 0)
            lever = value;
        else
            ceres::setParameter (patch, info.key, value);
    }
    int expression = 127;
    ceres::setParameter (patch, "expression", lever * expression / 127.0);
    const auto voices = static_cast<unsigned> (std::lround (parameter (p, ceres_plugin::kVoiceCountId).convertFrom0to1 (
        parameter (p, ceres_plugin::kVoiceCountId).getValue())));
    const auto quality = static_cast<std::size_t> (std::lround (parameter (p, ceres_plugin::kOversamplingId).convertFrom0to1 (
        parameter (p, ceres_plugin::kOversamplingId).getValue())));
    const auto factor = static_cast<unsigned> (ceres_plugin::kOversamplingFactors[quality]);

    ceres::Synth synth (rate, CeresAudioProcessor::kSeed);
    synth.reset();
    check (synth.configure (voices, factor), "the reference engine takes the plug-in's configuration");
    synth.setPatch (patch);
    synth.pitchBend (0.0);
    synth.pressure (0.0);
    synth.sustain (false);

    std::vector<float> out;
    std::size_t next = 0;
    for (int frame = 0; frame < total; ++frame)
    {
        for (; next < events.size() && events[next].at <= frame; ++next)
        {
            const auto& b = events[next].bytes;
            const int kind = b[0] & 0xf0;
            if (kind == 0x90 && b[2] > 0)
                synth.noteOn (b[1], b[2] / 127.0);
            else if (kind == 0x90 || kind == 0x80)
                synth.noteOff (b[1]);
            else if (kind == 0xd0)
                synth.pressure (b[1] / 127.0);
            else if (kind == 0xe0)
                synth.pitchBend (((b[2] << 7 | b[1]) - 8192) * (2.0 / 8192.0));
            else if (kind == 0xb0 && b[1] == 64)
                synth.sustain (b[2] >= 64);
            else if (kind == 0xb0 && b[1] == 11 && b[2] != expression)
            {
                expression = b[2];
                ceres::setParameter (patch, "expression", lever * expression / 127.0);
                synth.setPatch (patch);
            }
            else if (kind == 0xb0 && b[1] == 120)
                synth.allSoundOff();
            else if (kind == 0xb0 && b[1] == 121)
            {
                synth.pitchBend (0.0);
                synth.pressure (0.0);
                synth.sustain (false);
                if (expression != 127)
                {
                    expression = 127;
                    ceres::setParameter (patch, "expression", lever * expression / 127.0);
                    synth.setPatch (patch);
                }
            }
            else if (kind == 0xb0 && b[1] == 123)
                synth.allNotesOff();
        }
        synth.externalInput (static_cast<std::size_t> (frame) < ext.size() ? static_cast<double> (ext[static_cast<std::size_t> (frame)]) : 0.0);
        out.push_back (synth.process());
    }
    return out;
}

double peak (const std::vector<float>& x)
{
    double result = 0.0;
    for (const float v : x)
        result = std::max (result, static_cast<double> (std::abs (v)));
    return result;
}

bool finite (const std::vector<float>& x)
{
    return std::all_of (x.begin(), x.end(), [] (float v) { return std::isfinite (v); });
}

// A phrase that touches every MIDI message the plug-in handles.
const std::vector<Timed>& phrase()
{
    static const std::vector<Timed> events {
        noteOn (0, 60, 100),
        controller (0, 11, 90),
        pressure (1000, 64),
        noteOn (2000, 64, 80),
        noteOn (2000, 67, 127),
        bend (3001, 8192 + 4096),
        controller (5000, 64, 127),
        noteOff (6000, 60),
        controller (7000, 64, 0),
        { 8000, { 0x90, 64, 0 } }, // note-on at velocity 0 releases 64
        controller (9000, 121, 0),
        noteOn (10000, 48, 110),
        controller (12000, 123, 0),
        noteOn (13000, 50, 90),
        controller (15000, 120, 0),
        noteOn (16000, 55, 64),
        bend (17000, 0),
        noteOff (20000, 55),
    };
    return events;
}

void parameterContract()
{
    CeresAudioProcessor p;
    std::set<std::string> names, engineKeys;
    for (std::size_t i = 0; i < ceres::parameterCount(); ++i)
    {
        const auto info = ceres::parameterInfo (i);
        engineKeys.insert (info.key);
        auto* found = p.state().getParameter (info.key);
        check (found != nullptr, juce::String ("engine key ") + info.key + " is a parameter");
        if (found == nullptr)
            continue;
        check (found->paramID == info.key, juce::String (info.key) + ": the parameter ID is the engine key");
        check (found->getVersionHint() == 1, juce::String (info.key) + ": version hint 1");
        check (found->isAutomatable(), juce::String (info.key) + " is automatable");
        const auto* presentation = ceres_plugin::findControl (info.key);
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (found))
        {
            check (info.integer || (presentation != nullptr && presentation->positions != nullptr),
                   juce::String (info.key) + " is a choice only where the engine or panel has positions");
            check (choice->choices.size() == static_cast<int> (std::lround (info.maximum - info.minimum)) + 1,
                   juce::String (info.key) + " offers every position from minimum to maximum");
            check (juce::exactlyEqual (static_cast<double> (std::lround (found->convertFrom0to1 (found->getDefaultValue())))
                                           + info.minimum, info.defaultValue),
                   juce::String (info.key) + ": the default position is the engine default");
        }
        else if (auto* number = dynamic_cast<juce::AudioParameterFloat*> (found))
        {
            check (! info.integer, juce::String (info.key) + " rounds in the engine, so it must be a choice");
            check (juce::exactlyEqual (number->range.start, static_cast<float> (info.minimum))
                       && juce::exactlyEqual (number->range.end, static_cast<float> (info.maximum)),
                   juce::String (info.key) + ": the range is the engine's");
            // Hosts reset to the normalised default; a logarithmic lever maps
            // it back to the engine default within float precision.
            const double reset = number->convertFrom0to1 (found->getDefaultValue());
            check (std::abs (reset - info.defaultValue) <= 1.0e-6 * std::max (1.0, std::abs (info.defaultValue)),
                   juce::String (info.key) + ": the default is the engine default");
            check (juce::exactlyEqual (number->convertFrom0to1 (0.0f), number->range.start)
                       && juce::exactlyEqual (number->convertFrom0to1 (1.0f), number->range.end),
                   juce::String (info.key) + ": the ends of travel are the ends of the range");
            // Displayed text reads back to (nearly) the same lever position.
            for (const float position : { 0.0f, 0.37f, 1.0f })
            {
                const auto text = found->getText (position, 64);
                const float back = found->getValueForText (text);
                check (std::abs (back - position) < 0.02f,
                       juce::String (info.key) + ": \"" + text + "\" reads back near the same position");
            }
        }
        else
            check (false, juce::String (info.key) + " is a float or a choice");
    }

    for (const auto& control : ceres_plugin::kControls)
        check (engineKeys.count (control.key) == 1,
               juce::String ("PluginControls.h presents ") + control.key + ", which the engine does not have");

    auto& voices = parameter (p, ceres_plugin::kVoiceCountId);
    auto& quality = parameter (p, ceres_plugin::kOversamplingId);
    auto* voiceCount = dynamic_cast<juce::AudioParameterInt*> (&voices);
    check (voiceCount != nullptr && voiceCount->getRange().getStart() == 1
               && voiceCount->getRange().getEnd() == static_cast<int> (ceres::Synth::maxVoiceCount)
               && voiceCount->get() == ceres_plugin::kDefaultVoices,
           "Voices offers 1-16, default 4");
    auto* factors = dynamic_cast<juce::AudioParameterChoice*> (&quality);
    check (factors != nullptr && factors->choices == juce::StringArray ("1x", "2x", "4x") && factors->getIndex() == 2,
           "Oversampling offers 1x, 2x and 4x, default 4x");
    check (! voices.isAutomatable() && ! quality.isAutomatable(), "the voice and quality settings are not automatable");

    check (p.getParameters().size() == static_cast<int> (ceres::parameterCount()) + 2,
           "the parameters are the engine's keys and the two settings");
    for (auto* each : p.getParameters())
        check (names.insert (each->getName (64).toStdString()).second, "parameter name " + each->getName (64) + " is unique");
}

void programContract()
{
    CeresAudioProcessor p;
    const auto presetNames = ceres::presetNames();
    check (p.getNumPrograms() == static_cast<int> (presetNames.size()), "every research preset is a program");
    check (p.getCurrentProgram() == 0 && p.getProgramName (0) == "Init", "a new instance opens on Init");
    check (p.getProgramName (1) == "String 1" && p.getProgramName (6) == "Electric Piano", "program names read as words");

    // Performance and connection settings survive a program change.
    setEngineValue (p, "expression", 0.4);
    setEngineValue (p, "ext_level", 0.3);
    setEngineValue (p, "output_trim_db", -6.0);
    setSetting (p, ceres_plugin::kVoiceCountId, 7.0f);
    setSetting (p, ceres_plugin::kOversamplingId, 1.0f);
    for (int index = 0; index < p.getNumPrograms(); ++index)
    {
        p.setCurrentProgram (index);
        check (p.getCurrentProgram() == index, "program " + juce::String (index) + " becomes current");
        const auto preset = ceres::preset (presetNames[static_cast<std::size_t> (index)]);
        for (std::size_t i = 0; i < ceres::parameterCount(); ++i)
        {
            const auto info = ceres::parameterInfo (i);
            if (! CeresAudioProcessor::programSetsKey (info.key))
                continue;
            double expected = 0.0;
            ceres::getParameter (preset, info.key, expected);
            const double actual = engineValue (p, info);
            // Parameters hold floats: a preset value lands within float precision.
            check (std::abs (actual - expected) <= 1.0e-6 * std::max (1.0, std::abs (expected)),
                   p.getProgramName (index) + " sets " + info.key + " to " + juce::String (expected) + ", got "
                       + juce::String (actual));
        }
    }
    for (const char* key : { "expression", "ext_level", "output_trim_db" })
        check (! CeresAudioProcessor::programSetsKey (key), juce::String ("programs leave ") + key + " alone");
    check (std::abs (parameter (p, "expression").convertFrom0to1 (parameter (p, "expression").getValue()) - 0.4f) < 1.0e-6f
               && std::abs (parameter (p, "ext_level").convertFrom0to1 (parameter (p, "ext_level").getValue()) - 0.3f) < 1.0e-6f
               && std::abs (parameter (p, "output_trim_db").convertFrom0to1 (parameter (p, "output_trim_db").getValue()) + 6.0f) < 1.0e-5f,
           "programs keep the pedal, EXT LEVEL and the output trim");
    check (std::lround (parameter (p, ceres_plugin::kVoiceCountId).convertFrom0to1 (parameter (p, ceres_plugin::kVoiceCountId).getValue())) == 7
               && std::lround (parameter (p, ceres_plugin::kOversamplingId).convertFrom0to1 (parameter (p, ceres_plugin::kOversamplingId).getValue())) == 1,
           "programs keep the voice and quality settings");
    p.setCurrentProgram (-1);
    p.setCurrentProgram (p.getNumPrograms());
    check (p.getCurrentProgram() == p.getNumPrograms() - 1, "an out-of-range program is ignored");
}

juce::MemoryBlock save (CeresAudioProcessor& p)
{
    juce::MemoryBlock block;
    p.getStateInformation (block);
    return block;
}

void stateContract()
{
    CeresAudioProcessor a;
    a.setCurrentProgram (3);
    setEngineValue (a, "lp_cutoff", 1234.5);
    setEngineValue (a, "lfo_waveform", 5.0);
    setEngineValue (a, "expression", 0.25);
    setSetting (a, ceres_plugin::kVoiceCountId, 9.0f);
    const auto saved = save (a);
    check (saved.getSize() > 0, "state is written");

    CeresAudioProcessor b;
    b.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
    check (save (b) == saved, "a restored state saves the same bytes");
    check (b.getCurrentProgram() == 3, "the program number is restored");
    bool same = true;
    for (int i = 0; i < a.getParameters().size(); ++i)
        same = same && juce::exactlyEqual (a.getParameters()[i]->getValue(), b.getParameters()[i]->getValue());
    check (same, "every parameter is restored");

    const char garbage[] = "not a Ceres state";
    b.setStateInformation (garbage, sizeof garbage);
    b.setStateInformation (nullptr, 0);
    check (save (b) == saved, "a foreign or empty chunk changes nothing");

    // The silent scaffold saved <Ceres stateVersion="1"/>: it opens as a new instance.
    juce::MemoryBlock scaffold;
    juce::AudioProcessor::copyXmlToBinary (*juce::parseXML ("<Ceres stateVersion=\"1\"/>"), scaffold);
    b.setStateInformation (scaffold.getData(), static_cast<int> (scaffold.getSize()));
    CeresAudioProcessor fresh;
    check (save (b) == save (fresh), "a scaffold chunk restores a new instance's state");

    // A parameter the chunk leaves out takes its default, whatever was there.
    auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
    auto* cutoff = xml != nullptr ? xml->getChildByAttribute ("id", "lp_cutoff") : nullptr;
    check (cutoff != nullptr, "the state holds lp_cutoff");
    if (cutoff != nullptr)
        xml->removeChildElement (cutoff, true);
    juce::MemoryBlock partial;
    juce::AudioProcessor::copyXmlToBinary (*xml, partial);
    CeresAudioProcessor c;
    setEngineValue (c, "lp_cutoff", 300.0);
    c.setStateInformation (partial.getData(), static_cast<int> (partial.getSize()));
    auto& lp = parameter (c, "lp_cutoff");
    check (juce::exactlyEqual (lp.getValue(), lp.getDefaultValue()), "a parameter missing from the chunk takes its default");
    bool rest = c.getCurrentProgram() == 3;
    for (int i = 0; i < a.getParameters().size(); ++i)
        if (a.getParameters()[i] != &parameter (a, "lp_cutoff"))
            rest = rest && juce::exactlyEqual (a.getParameters()[i]->getValue(), c.getParameters()[i]->getValue());
    check (rest, "the rest of a partial chunk is restored");
}

void renderContract()
{
    constexpr int total = 24000;
    const auto& events = phrase();

    // Program 4 with the SUB on the VCO and VCF, so the controllers reach it.
    const auto configure = [] (CeresAudioProcessor& p)
    {
        p.setCurrentProgram (4);
        setEngineValue (p, "vibrato_cents", 40.0);
        setEngineValue (p, "filter_lfo", 0.5);
        setEngineValue (p, "pressure_vibrato", 30.0);
    };
    CeresAudioProcessor whole;
    configure (whole);
    prepare (whole, 512);
    check (whole.getLatencySamples() == static_cast<int> (ceres::Synth::outputLatency), "latency is the engine's 31 samples");
    check (whole.getTotalNumInputChannels() == 0, "EXT IN is off by default");
    const auto played = play (whole, total, 512, events);
    check (finite (played.left), "rendered samples are finite");
    check (peak (played.left) > 1.0e-3, "the phrase is audible");
    check (played.left == played.right, "both output channels carry the mono engine");
    check (played.allocations == 0, "the audio callback allocates nothing (" + juce::String ((int) played.allocations) + ")");
    check (played.left == reference (whole, total, events),
           "the plug-in renders what the engine renders from the same MIDI, parameters and seed");

    for (const int block : { 64, 37, 1 })
    {
        CeresAudioProcessor pieces;
        configure (pieces);
        prepare (pieces, block);
        check (play (pieces, total, block, events).left == played.left,
               juce::String (block) + "-frame blocks render the same samples as 512");
    }

    CeresAudioProcessor mono;
    configure (mono);
    auto layout = mono.getBusesLayout();
    layout.getChannelSet (false, 0) = juce::AudioChannelSet::mono();
    check (mono.setBusesLayout (layout), "a mono output is accepted");
    prepare (mono, 512);
    check (play (mono, total, 512, events).left == played.left, "a mono output carries the same samples");

    // Garbage left in the host's buffers is not EXT IN while the bus is off.
    CeresAudioProcessor noisy;
    configure (noisy);
    prepare (noisy, 512);
    check (play (noisy, total, 512, events, {}, {}, true).left == played.left,
           "with EXT IN off, the buffer's previous contents are ignored");
}

void extInContract()
{
    constexpr int total = 24000;
    const std::vector<Timed> events { noteOn (0, 57, 100), noteOn (5000, 64, 90), noteOff (20000, 57), noteOff (20000, 64) };
    std::vector<float> sine (total), other (total);
    for (int i = 0; i < total; ++i)
    {
        sine[static_cast<std::size_t> (i)] = 0.8f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 6.5 * i / kRate));
        other[static_cast<std::size_t> (i)] = 0.5f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 1.3 * i / kRate));
    }
    const auto configure = [] (CeresAudioProcessor& p, const juce::AudioChannelSet& input)
    {
        auto layout = p.getBusesLayout();
        layout.getChannelSet (true, 0) = input;
        check (p.setBusesLayout (layout), "EXT IN takes the layout " + input.getDescription());
        setEngineValue (p, "lfo_waveform", 5.0); // FUNCTION EXT
        setEngineValue (p, "vibrato_cents", 60.0);
        setEngineValue (p, "filter_lfo", 1.0);
        setEngineValue (p, "ext_level", 0.7);
    };

    CeresAudioProcessor p;
    check (p.getBus (true, 0) != nullptr && p.getBus (true, 0)->getName() == "EXT IN"
               && ! p.getBus (true, 0)->isEnabledByDefault(),
           "the processor has an EXT IN input bus, off by default");
    configure (p, juce::AudioChannelSet::mono());
    prepare (p, 256);
    const auto played = play (p, total, 256, events, sine);
    check (played.allocations == 0, "the audio callback allocates nothing with EXT IN on");
    check (played.left == reference (p, total, events, sine), "EXT IN reaches the engine sample for sample");
    check (played.left != reference (p, total, events), "the EXT IN signal changes the sound under FUNCTION EXT");

    for (const int block : { 61, 1 })
    {
        CeresAudioProcessor pieces;
        configure (pieces, juce::AudioChannelSet::mono());
        prepare (pieces, block);
        check (play (pieces, total, block, events, sine).left == played.left,
               "EXT IN renders the same in " + juce::String (block) + "-frame blocks");
    }

    CeresAudioProcessor stereo;
    configure (stereo, juce::AudioChannelSet::stereo());
    prepare (stereo, 256);
    check (play (stereo, total, 256, events, sine, other).left == played.left,
           "a stereo EXT IN feeds the jack its first channel");

    CeresAudioProcessor off;
    configure (off, juce::AudioChannelSet::disabled());
    prepare (off, 256);
    check (play (off, total, 256, events, sine, {}, true).left == reference (off, total, events),
           "a disabled EXT IN feeds the jack silence");

    // Internal SUB waveforms ignore the jack bit for bit.
    CeresAudioProcessor internal;
    configure (internal, juce::AudioChannelSet::mono());
    setEngineValue (internal, "lfo_waveform", 1.0);
    prepare (internal, 256);
    check (play (internal, total, 256, events, sine).left == reference (internal, total, events),
           "FUNCTION's internal waveforms ignore EXT IN");
}

void busContract()
{
    CeresAudioProcessor p;
    using Set = juce::AudioChannelSet;
    const auto layout = [] (Set input, Set output)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (input);
        l.outputBuses.add (output);
        return l;
    };
    for (const auto& input : { Set::disabled(), Set::mono(), Set::stereo() })
        for (const auto& output : { Set::mono(), Set::stereo() })
            check (p.checkBusesLayoutSupported (layout (input, output)),
                   "EXT IN " + input.getDescription() + " with output " + output.getDescription() + " is supported");
    check (! p.checkBusesLayoutSupported (layout (Set::mono(), Set::disabled())), "the output cannot be turned off");
    check (! p.checkBusesLayoutSupported (layout (Set::create5point1(), Set::stereo())), "EXT IN is mono or stereo");
    check (! p.checkBusesLayoutSupported (layout (Set::disabled(), Set::create5point1())), "the output is mono or stereo");
    auto* extensions = p.getVST3ClientExtensions();
    check (extensions != nullptr && ! extensions->getPluginHasMainInput(), "VST3 sees EXT IN as an auxiliary input");
#if CERES_CLAP_EXTENSIONS
    check (! p.isInputMain (0), "CLAP sees EXT IN as a non-main port");
#endif
}

void sampleRateContract()
{
    CeresAudioProcessor p;
    prepare (p, 512, 4000.0);
    check (! p.engineRunning() && p.sampleRateUnsupported(), "4 kHz is outside the engine's range");
    const auto silent = play (p, 4800, 512, { noteOn (0, 60, 100) });
    check (juce::exactlyEqual (peak (silent.left), 0.0), "an unsupported sample rate renders silence");
    prepare (p, 512, 96000.0);
    check (p.engineRunning() && ! p.sampleRateUnsupported(), "the engine starts again at 96 kHz");
    check (peak (play (p, 9600, 512, { noteOn (0, 60, 100) }).left) > 1.0e-3, "and plays");
}

void configurationContract()
{
    // Applied at once while the engine is stopped.
    {
        CeresAudioProcessor p;
        setSetting (p, ceres_plugin::kVoiceCountId, 8.0f);
        setSetting (p, ceres_plugin::kOversamplingId, 0.0f);
        prepare (p, 256);
        check (p.appliedVoices() == 8 && p.appliedOversampling() == 1 && ! p.configurationPending(),
               "prepareToPlay applies the voice and quality settings");
    }

    // Deferred while a key, the pedal or an envelope is active.
    CeresAudioProcessor p;
    prepare (p, 256);
    play (p, 2560, 256, { noteOn (0, 60, 100), controller (100, 64, 127) });
    setSetting (p, ceres_plugin::kVoiceCountId, 2.0f);
    play (p, 2560, 256, { noteOff (0, 60) });
    check (p.configurationPending() && p.appliedVoices() == 4, "a voice change waits while the pedal holds a note");
    play (p, 256, 256, { controller (0, 64, 0) });
    bool applied = false;
    int blocks = 0;
    for (; blocks < 400 && ! applied; ++blocks)
    {
        play (p, 256, 256, {});
        applied = p.appliedVoices() == 2 && ! p.configurationPending();
    }
    check (applied, "the voice change applies once everything is idle");
    check (peak (play (p, 9600, 256, { noteOn (0, 62, 100) }).left) > 1.0e-3, "the reconfigured engine plays");

    // A key during the fade-out cancels it: the note plays at full level.
    CeresAudioProcessor q;
    prepare (q, 64);
    setSetting (q, ceres_plugin::kOversamplingId, 1.0f);
    play (q, 64 * 4, 64, {});
    check (q.configurationPending(), "the quality change is pending during its fade-out");
    CeresAudioProcessor r;
    prepare (r, 64);
    const auto withFade = play (q, 9600, 64, { noteOn (0, 60, 100) });
    const auto plain = play (r, 9600 + 64 * 4, 64, { noteOn (64 * 4, 60, 100) });
    check (q.appliedOversampling() == 4, "a note during the fade-out keeps the running quality");
    check (peak (withFade.left) > 0.5 * peak (plain.left), "a note during the fade-out sounds");
}

// CRC-32 (ISO-HDLC), for the PNG chunk the screenshot's tag goes in.
juce::uint32 crc32 (const juce::uint8* data, std::size_t size)
{
    juce::uint32 crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

// The PNG with a tEXt chunk "Ceres-Editor-Source" before IEND, naming the
// editor source the image was rendered from.
juce::MemoryBlock taggedPng (const juce::Image& image)
{
    juce::MemoryOutputStream encoded;
    juce::PNGImageFormat png;
    check (png.writeImageToStream (image, encoded), "encode the editor snapshot");
    const auto* bytes = static_cast<const juce::uint8*> (encoded.getData());
    const auto size = encoded.getDataSize();
    check (size > 12 && std::memcmp (bytes + size - 8, "IEND", 4) == 0, "the snapshot ends with IEND");

    std::vector<juce::uint8> chunk;
    const std::string payload = std::string ("tEXt") + "Ceres-Editor-Source" + '\0' + CERES_EDITOR_SOURCE_ID;
    const auto length = static_cast<juce::uint32> (payload.size() - 4);
    for (int shift = 24; shift >= 0; shift -= 8)
        chunk.push_back (static_cast<juce::uint8> (length >> shift));
    chunk.insert (chunk.end(), payload.begin(), payload.end());
    const auto crc = crc32 (chunk.data() + 4, payload.size());
    for (int shift = 24; shift >= 0; shift -= 8)
        chunk.push_back (static_cast<juce::uint8> (crc >> shift));

    juce::MemoryBlock tagged;
    tagged.append (bytes, size - 12);
    tagged.append (chunk.data(), chunk.size());
    tagged.append (bytes + size - 12, 12);
    return tagged;
}

void collectIds (juce::Component& component, juce::Component& editor, std::map<juce::String, std::vector<juce::Rectangle<int>>>& ids)
{
    for (auto* child : component.getChildren())
    {
        if (child->getComponentID().isNotEmpty())
            ids[child->getComponentID()].push_back (editor.getLocalArea (child, child->getLocalBounds()));
        collectIds (*child, editor, ids);
    }
}

void editorContract()
{
    CeresAudioProcessor p;
    prepare (p, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    check (editor->getWidth() == CeresAudioProcessorEditor::kWidth && editor->getHeight() == CeresAudioProcessorEditor::kHeight,
           "the editor opens at its fixed size");
    check (editor->isOpaque(), "the editor is opaque");

    // Every parameter has exactly one control, inside the window, and no
    // two controls overlap.
    std::map<juce::String, std::vector<juce::Rectangle<int>>> ids;
    collectIds (*editor, *editor, ids);
    std::vector<juce::Rectangle<int>> placed;
    for (auto* each : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (each))
        {
            const auto& found = ids[ranged->paramID];
            check (found.size() == 1, ranged->paramID + " has exactly one control");
            for (const auto& bounds : found)
            {
                check (! bounds.isEmpty() && editor->getLocalBounds().contains (bounds), ranged->paramID + "'s control is on screen");
                for (const auto& other : placed)
                    check (! other.intersects (bounds), ranged->paramID + "'s control overlaps another");
                placed.push_back (bounds);
            }
        }
    check (ids.size() == static_cast<std::size_t> (p.getParameters().size()), "no control lacks a parameter");

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    check (image.getPixelAt (4, 4).getAlpha() == 255, "the editor paints an opaque panel");

    // A tone selector tablet selects its program.
    for (auto* child : editor->getChildren())
        if (auto* tablet = dynamic_cast<juce::TextButton*> (child);
            tablet != nullptr && tablet->getButtonText() == "Flute" && tablet->onClick != nullptr)
            tablet->onClick();
    check (p.getProgramName (p.getCurrentProgram()) == "Flute", "a tone selector tablet selects its program");
    p.setCurrentProgram (0);

    // The main build asks for the committed screenshot through this variable,
    // so the README image tracks the real editor.
    const auto path = juce::SystemStats::getEnvironmentVariable ("CERES_EDITOR_SNAPSHOT", {});
    if (path.isNotEmpty())
    {
        const juce::File file (path);
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        const auto png = taggedPng (image);
        check (file.replaceWithData (png.getData(), png.getSize()), "write the editor snapshot");
    }
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    parameterContract();
    programContract();
    stateContract();
    renderContract();
    extInContract();
    busContract();
    sampleRateContract();
    configurationContract();
    editorContract();
    std::printf ("Ceres processor contracts: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
