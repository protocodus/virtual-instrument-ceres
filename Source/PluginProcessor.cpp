#include "PluginProcessor.h"
#include "PluginControls.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace
{
using ceres_plugin::Control;
using ceres_plugin::Scale;
using ceres_plugin::Unit;

// 1 was the silent scaffold's empty chunk; 2 holds every parameter.
constexpr int kStateVersion = 2;
const juce::Identifier kStateType { "Ceres" }, kStateVersionProperty { "stateVersion" },
    kProgramProperty { "program" }, kPluginVersionProperty { "pluginVersion" };

// "string1" -> "String 1", "electric_piano" -> "Electric Piano".
juce::String programName (const std::string& key)
{
    juce::String name;
    bool wordStart = true;
    for (std::size_t i = 0; i < key.size(); ++i)
    {
        const auto c = static_cast<juce::juce_wchar> (static_cast<unsigned char> (key[i]));
        if (c == '_' || c == '-')
        {
            name << ' ';
            wordStart = true;
            continue;
        }
        if (juce::CharacterFunctions::isDigit (c) && i > 0 && ! juce::CharacterFunctions::isDigit (
                static_cast<juce::juce_wchar> (static_cast<unsigned char> (key[i - 1]))))
            name << ' ';
        name << juce::String::charToString (wordStart ? juce::CharacterFunctions::toUpperCase (c) : c);
        wordStart = false;
    }
    return name;
}

// An engine key the presentation table does not know: "board_x" -> "Board x".
juce::String keyName (const char* key)
{
    auto name = juce::String (key).replaceCharacter ('_', ' ');
    return name.substring (0, 1).toUpperCase() + name.substring (1);
}

// juce::String (v, 0) prints every significant digit; 0 decimals here means none.
juce::String fixed (double value, int decimals)
{
    return decimals > 0 ? juce::String (value, decimals) : juce::String (juce::roundToInt (value));
}

juce::String significant (double value, int digits)
{
    if (juce::exactlyEqual (value, 0.0) || ! std::isfinite (value))
        return "0";
    const int magnitude = static_cast<int> (std::floor (std::log10 (std::abs (value))));
    return fixed (value, juce::jlimit (0, 6, digits - 1 - magnitude));
}

juce::String signedText (double value, int decimals)
{
    return (value > 0.0 ? "+" : "") + fixed (value, decimals);
}

juce::String formatValue (Unit unit, double v)
{
    switch (unit)
    {
        case Unit::Fraction: return juce::String (juce::roundToInt (v * 100.0)) + "%";
        case Unit::Duty: return juce::String (v * 100.0, 1) + "%";
        case Unit::DutySwing: return juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (v * 100.0, 1) + "%";
        case Unit::Hertz: return v < 1000.0 ? significant (v, 3) + " Hz" : significant (v / 1000.0, 3) + " kHz";
        case Unit::Seconds: return v < 1.0 ? significant (v * 1000.0, 3) + " ms" : significant (v, 3) + " s";
        case Unit::Cents: return fixed (v, std::abs (v) >= 10.0 ? 0 : 1) + " cents";
        case Unit::Decibels: return signedText (v, 1) + " dB";
        case Unit::Octaves: return juce::String (v, 2) + " oct";
        case Unit::EgUnits: return signedText (v, 2);
        case Unit::Volts: return signedText (v, 2) + " V";
        case Unit::Degrees: return juce::String (juce::roundToInt (v * 360.0)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
        case Unit::Microseconds: return significant (v, 3) + juce::String (juce::CharPointer_UTF8 (" \xc2\xb5s"));
        case Unit::SemitonesPerSecond: return significant (v, 3) + " st/s";
        case Unit::Ratio: return juce::String (v, 2) + "x";
        case Unit::Kelvin: return signedText (v, 1) + " K";
        case Unit::Plain: break;
    }
    return significant (v, 3);
}

// Reads what formatValue writes, so a typed "1.2 kHz" or "350 ms" lands.
double parseValue (Unit unit, const juce::String& text)
{
    const auto trimmed = text.trim();
    const int start = trimmed.indexOfAnyOf ("0123456789.-+");
    if (start < 0)
        return 0.0;
    double v = trimmed.substring (start).getDoubleValue();
    switch (unit)
    {
        case Unit::Fraction:
        case Unit::Duty:
        case Unit::DutySwing: return v / 100.0;
        case Unit::Hertz: return trimmed.containsIgnoreCase ("khz") ? v * 1000.0 : v;
        case Unit::Seconds: return trimmed.containsIgnoreCase ("ms") ? v / 1000.0 : v;
        case Unit::Degrees: return v / 360.0;
        case Unit::Plain:
        case Unit::Cents:
        case Unit::Decibels:
        case Unit::Octaves:
        case Unit::EgUnits:
        case Unit::Volts:
        case Unit::Microseconds:
        case Unit::SemitonesPerSecond:
        case Unit::Ratio:
        case Unit::Kelvin: break;
    }
    return v;
}

juce::NormalisableRange<float> rangeFor (const ceres::ParameterInfo& info, const Control* control)
{
    const auto low = static_cast<float> (info.minimum), high = static_cast<float> (info.maximum);
    if (control != nullptr && control->scale == Scale::Logarithmic && info.minimum > 0.0)
    {
        const double bottom = info.minimum, octaves = std::log2 (info.maximum / info.minimum);
        // Exact at both ends, so a saved end value restores bit for bit.
        return { low, high,
                 [bottom, octaves, low, high] (float, float, float x)
                 {
                     return x <= 0.0f ? low : x >= 1.0f ? high
                                        : juce::jlimit (low, high, static_cast<float> (bottom * std::exp2 (octaves * x)));
                 },
                 [bottom, octaves, low, high] (float, float, float v)
                 {
                     return v <= low ? 0.0f : v >= high ? 1.0f
                                         : static_cast<float> (std::log2 (v / bottom) / octaves);
                 },
                 [low, high] (float, float, float v) { return juce::jlimit (low, high, v); } };
    }
    juce::NormalisableRange<float> range (low, high);
    if (control != nullptr && control->scale == Scale::Centred && control->centre > info.minimum
        && control->centre < info.maximum)
        range.setSkewForCentre (static_cast<float> (control->centre));
    return range;
}

std::unique_ptr<juce::RangedAudioParameter> makeParameter (const ceres::ParameterInfo& info, const Control* control)
{
    const juce::ParameterID id { info.key, 1 };
    const juce::String name = control != nullptr ? juce::String (control->name) : keyName (info.key);
    if (info.integer || (control != nullptr && control->positions != nullptr))
    {
        const auto names = control != nullptr && control->positions != nullptr
                               ? juce::StringArray::fromTokens (control->positions, "|", {})
                               : juce::StringArray();
        const int count = static_cast<int> (std::lround (info.maximum - info.minimum)) + 1;
        juce::StringArray choices;
        for (int i = 0; i < count; ++i)
            choices.add (i < names.size() ? names[i] : juce::String (info.minimum + i));
        return std::make_unique<juce::AudioParameterChoice> (
            id, name, choices, static_cast<int> (std::lround (info.defaultValue - info.minimum)));
    }
    const auto unit = control != nullptr ? control->unit : Unit::Plain;
    return std::make_unique<juce::AudioParameterFloat> (
        id, name, rangeFor (info, control), static_cast<float> (info.defaultValue),
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([unit] (float v, int) { return formatValue (unit, v); })
            .withValueFromStringFunction ([unit] (const juce::String& text)
                                          { return static_cast<float> (parseValue (unit, text)); }));
}
} // namespace

// EXT IN is off until a host or the user turns it on, so an instrument track
// asks for no audio input; while it is off the jack reads silence.
CeresAudioProcessor::CeresAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("EXT IN", juce::AudioChannelSet::mono(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, kStateType, createLayout())
{
    for (std::size_t i = 0; i < ceres::parameterCount(); ++i)
    {
        const auto info = ceres::parameterInfo (i);
        Binding binding;
        binding.key = info.key;
        binding.raw = parameters.getRawParameterValue (info.key);
        if (dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter (info.key)) != nullptr)
            binding.offset = info.minimum;
        if (std::strcmp (info.key, "expression") == 0)
            expressionBinding = bindings.size();
        bindings.push_back (binding);
    }
    voiceCountValue = parameters.getRawParameterValue (ceres_plugin::kVoiceCountId);
    oversamplingValue = parameters.getRawParameterValue (ceres_plugin::kOversamplingId);
    ampReleaseValue = parameters.getRawParameterValue ("amp_release");
    globalReleaseValue = parameters.getRawParameterValue ("global_release");

    for (const auto& name : ceres::presetNames())
        programs.push_back ({ programName (name), ceres::preset (name) });
    // A new instance opens on the first program, Init.
    setCurrentProgram (0);
    readControls (true);
}

CeresAudioProcessor::~CeresAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout CeresAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::AudioProcessorParameterGroup>> groups;
    for (const auto& group : ceres_plugin::kGroups)
        groups.push_back (std::make_unique<juce::AudioProcessorParameterGroup> (group.id, group.name, " | "));
    const auto groupFor = [&groups] (const char* id) -> juce::AudioProcessorParameterGroup&
    {
        for (std::size_t i = 0; i < groups.size(); ++i)
            if (std::strcmp (ceres_plugin::kGroups[i].id, id) == 0)
                return *groups[i];
        for (std::size_t i = 0; i < groups.size(); ++i)
            if (std::strcmp (ceres_plugin::kGroups[i].id, ceres_plugin::kUnlistedGroup) == 0)
                return *groups[i];
        return *groups.back();
    };

    // The engine's keys in panel order, then any key the table does not list.
    const std::size_t count = ceres::parameterCount();
    std::vector<bool> placed (count, false);
    for (const auto& control : ceres_plugin::kControls)
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto info = ceres::parameterInfo (i);
            if (! placed[i] && std::strcmp (info.key, control.key) == 0)
            {
                placed[i] = true;
                groupFor (control.group).addChild (makeParameter (info, &control));
            }
        }
    for (std::size_t i = 0; i < count; ++i)
        if (! placed[i])
            groupFor (ceres_plugin::kUnlistedGroup).addChild (makeParameter (ceres::parameterInfo (i), nullptr));

    // Settings, not sound: hosts must not automate them, since each change
    // waits for the engine to fall idle and then resets it.
    auto& plugin = groupFor ("plugin");
    plugin.addChild (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ceres_plugin::kVoiceCountId, 1 }, "Voices", 1,
        static_cast<int> (ceres::Synth::maxVoiceCount), ceres_plugin::kDefaultVoices,
        juce::AudioParameterIntAttributes().withAutomatable (false)));
    juce::StringArray factors;
    for (const int factor : ceres_plugin::kOversamplingFactors)
        factors.add (juce::String (factor) + "x");
    plugin.addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ceres_plugin::kOversamplingId, 1 }, "Oversampling", factors,
        ceres_plugin::kDefaultOversamplingIndex, juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& group : groups)
        if (! group->getParameters (false).isEmpty())
            layout.add (std::move (group));
    return layout;
}

bool CeresAudioProcessor::programSetsKey (const char* key) noexcept
{
    return std::strcmp (key, "expression") != 0 && std::strcmp (key, "ext_level") != 0
        && std::strcmp (key, "output_trim_db") != 0;
}

bool CeresAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // The engine is mono; a stereo output carries it on both channels, and a
    // stereo EXT IN feeds the jack its first channel.
    const auto monoOrStereo = [] (const juce::AudioChannelSet& set)
    { return set == juce::AudioChannelSet::mono() || set == juce::AudioChannelSet::stereo(); };
    if (layouts.outputBuses.size() != 1 || ! monoOrStereo (layouts.outputBuses.getReference (0)))
        return false;
    for (const auto& input : layouts.inputBuses)
        if (! input.isDisabled() && ! monoOrStereo (input))
            return false;
    return layouts.inputBuses.size() <= 1;
}

bool CeresAudioProcessor::readControls (bool force) noexcept
{
    bool changed = force;
    for (std::size_t i = 0; i < bindings.size(); ++i)
    {
        auto& binding = bindings[i];
        const float raw = binding.raw->load (std::memory_order_relaxed);
        if (! force && juce::exactlyEqual (raw, binding.last))
            continue;
        binding.last = raw;
        changed = true;
        if (i == expressionBinding)
            expressionLever = raw;
        else
            ceres::setParameter (patch, binding.key, binding.offset + raw);
    }
    if (changed)
        applyExpression();

    requestedVoices = static_cast<unsigned> (juce::jlimit (1, static_cast<int> (ceres::Synth::maxVoiceCount),
                                                           juce::roundToInt (voiceCountValue->load())));
    const auto quality = static_cast<std::size_t> (
        juce::jlimit (0, static_cast<int> (std::size (ceres_plugin::kOversamplingFactors)) - 1,
                      juce::roundToInt (oversamplingValue->load())));
    requestedFactor = static_cast<unsigned> (ceres_plugin::kOversamplingFactors[quality]);
    return changed;
}

// MIDI CC11 scales the EXPRESSION pedal parameter, as General MIDI's
// expression scales channel volume: the parameter sets where a fully raised
// MIDI pedal sits, and without CC11 (127) it alone sets the pedal. Neither
// writes the other, so host automation and a played pedal never fight.
void CeresAudioProcessor::applyExpression() noexcept
{
    ceres::setParameter (patch, "expression", expressionLever * expressionController / 127.0);
}

void CeresAudioProcessor::prepareToPlay (double sampleRate, int)
{
    readControls (true);
    startEngine (sampleRate);
    // Every oversampling factor has the same 31-sample group delay.
    setLatencySamples (static_cast<int> (ceres::Synth::outputLatency));
}

void CeresAudioProcessor::startEngine (double sampleRate)
{
    engine.reset();
    running = false;
    unsupportedRate = false;
    try
    {
        engine = std::make_unique<ceres::Synth> (sampleRate, kSeed);
    }
    catch (const std::invalid_argument&)
    {
        // Outside 8-384 kHz the circuit is undefined; stay silent.
        unsupportedRate = true;
        return;
    }
    running = true;
    fadeFrames = std::max (1, static_cast<int> (std::ceil (sampleRate * 0.020)));
    restartEngine();
}

void CeresAudioProcessor::reset()
{
    const juce::ScopedLock lock (getCallbackLock());
    if (engine == nullptr)
        return;
    readControls (false);
    restartEngine();
}

// Clears keys, pedal and every circuit state, applies the requested voices and
// quality, then the patch and the controller positions.
void CeresAudioProcessor::restartEngine() noexcept
{
    heldKeys.fill (0);
    heldCount = 0;
    pedal = false;
    fade = Fade::none;
    fadeGain = 1.0;
    fadeRemaining = 0;
    engine->reset();
    engine->configure (requestedVoices, requestedFactor);
    engine->setPatch (patch);
    restoreControllers();
    publishConfiguration();
}

void CeresAudioProcessor::restoreControllers() noexcept
{
    engine->pitchBend (bend);
    engine->pressure (touch);
    engine->sustain (pedal);
}

void CeresAudioProcessor::publishConfiguration() noexcept
{
    appliedVoiceCount = static_cast<int> (engine->polyphony());
    appliedFactor = static_cast<int> (engine->oversamplingFactor());
    pendingConfiguration = engine->polyphony() != requestedVoices || engine->oversamplingFactor() != requestedFactor;
}

bool CeresAudioProcessor::idle() const noexcept
{
    // Keys whose board was stolen still count, and so does the pedal:
    // activeVoices() alone misses both, and it says nothing about the
    // coupling-capacitor tails the fade covers.
    return heldCount == 0 && ! pedal && engine->activeVoices() == 0;
}

void CeresAudioProcessor::advanceConfiguration() noexcept
{
    const bool wanted = engine->polyphony() != requestedVoices || engine->oversamplingFactor() != requestedFactor;
    const auto startFade = [this] (Fade kind, double target)
    {
        fade = kind;
        fadeRemaining = fadeFrames;
        fadeStep = (target - fadeGain) / fadeFrames;
    };
    if (wanted && idle())
    {
        if (fade != Fade::out)
            startFade (Fade::out, 0.0);
        else if (fadeRemaining == 0)
        {
            // The remaining tail is muted; nothing that sounds is cut off.
            restartEngine();
            fadeGain = 0.0;
            startFade (Fade::in, 1.0);
        }
    }
    else if (fade == Fade::out)
        startFade (Fade::in, 1.0); // a key or the pedal came back first

    if (fadeRemaining > 0 && --fadeRemaining == 0)
    {
        fadeGain = fade == Fade::out ? 0.0 : 1.0;
        if (fade == Fade::in)
            fade = Fade::none;
    }
    else if (fadeRemaining > 0)
        fadeGain += fadeStep;
}

void CeresAudioProcessor::render (float* out, const float* ext, int begin, int end) noexcept
{
    for (int i = begin; i < end; ++i)
    {
        // The jack is fed every frame, whatever FUNCTION selects: the EXT
        // preamp's coupling capacitor keeps charging, and the internal SUB
        // waveforms ignore it.
        engine->externalInput (ext != nullptr ? static_cast<double> (ext[i]) : 0.0);
        if (fade != Fade::none || pendingConfiguration.load (std::memory_order_relaxed))
        {
            advanceConfiguration();
            const float sample = engine->process();
            out[i] = fade == Fade::none ? sample : static_cast<float> (sample * fadeGain);
            continue;
        }
        out[i] = engine->process();
    }
}

void CeresAudioProcessor::handleMidi (const juce::uint8* data, int size) noexcept
{
    if (size < 1 || data[0] < 0x80 || data[0] >= 0xf0)
        return;
    const int kind = data[0] & 0xf0;
    const int first = size > 1 ? data[1] & 0x7f : 0, second = size > 2 ? data[2] & 0x7f : 0;
    const auto key = static_cast<std::size_t> (first);
    switch (kind)
    {
        case 0x90:
            if (size < 3)
                return;
            if (second > 0)
            {
                engine->noteOn (first, second / 127.0);
                if (heldKeys[key] < std::numeric_limits<std::uint32_t>::max())
                {
                    ++heldKeys[key];
                    ++heldCount;
                }
                return;
            }
            [[fallthrough]]; // note-on at velocity 0 is a note-off
        case 0x80:
            if (size < 3)
                return;
            engine->noteOff (first);
            if (heldKeys[key] > 0)
            {
                --heldKeys[key];
                --heldCount;
            }
            return;
        case 0xd0: // channel pressure is the CS-50's touch response
            if (size < 2)
                return;
            touch = first / 127.0;
            engine->pressure (touch);
            return;
        case 0xe0:
            if (size < 3)
                return;
            bend = ((second << 7 | first) - 8192) * (kBendSemitones / 8192.0);
            engine->pitchBend (bend);
            return;
        case 0xb0:
            break;
        default:
            return;
    }
    if (size < 3)
        return;
    switch (first)
    {
        case 64: // damper pedal
            pedal = second >= 64;
            engine->sustain (pedal);
            break;
        case 11: // expression pedal
            if (second != expressionController)
            {
                expressionController = second;
                applyExpression();
                engine->setPatch (patch);
            }
            break;
        case 120: // all sound off
            engine->allSoundOff();
            heldKeys.fill (0);
            heldCount = 0;
            break;
        case 121: // reset all controllers (MIDI RP-015)
            bend = touch = 0.0;
            pedal = false;
            restoreControllers();
            if (expressionController != 127)
            {
                expressionController = 127;
                applyExpression();
                engine->setPatch (patch);
            }
            break;
        case 123: // all notes off; the pedal still holds what it holds
            engine->allNotesOff();
            heldKeys.fill (0);
            heldCount = 0;
            break;
        default:
            break;
    }
}

void CeresAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int frames = buffer.getNumSamples(), channels = buffer.getNumChannels();
    if (engine == nullptr || channels == 0)
    {
        buffer.clear();
        return;
    }
    if (readControls (false))
        engine->setPatch (patch);
    pendingConfiguration = engine->polyphony() != requestedVoices || engine->oversamplingFactor() != requestedFactor;

    // EXT IN's first channel shares the buffer with the output, so render()
    // reads each of its samples before writing that frame's output.
    const float* ext = nullptr;
    if (getTotalNumInputChannels() > 0)
        ext = buffer.getReadPointer (getChannelIndexInProcessBlockBuffer (true, 0, 0));
    auto* out = buffer.getWritePointer (getChannelIndexInProcessBlockBuffer (false, 0, 0));

    // Events start at their own sample.
    int position = 0;
    for (const auto metadata : midi)
    {
        const int at = std::clamp (metadata.samplePosition, position, frames);
        render (out, ext, position, at);
        position = at;
        handleMidi (metadata.data, metadata.numBytes);
    }
    render (out, ext, position, frames);
    const int outputs = std::min (channels, getTotalNumOutputChannels());
    for (int channel = 1; channel < outputs; ++channel)
        buffer.copyFrom (channel, 0, buffer, 0, 0, frames);
    publishConfiguration();
}

double CeresAudioProcessor::getTailLengthSeconds() const
{
    // After key-off a board's VCA EG falls from its peak toward a floor below
    // zero and closes where it crosses zero. At the widest board tolerance
    // (CS-60 TP2: peak 10 V +/-0.2 V, floor -200 mV +/-20 mV) that takes
    // ln (1 + 10.2 / 0.18) / ln (1000) = 0.587 of the release T60, and the EG
    // time trims (+/-5 % at the 4 ms and 125 ms attack trims, extrapolated in
    // log time over the whole 0.1 ms to 30 s range) stretch that by at most
    // 1.22, to 0.72 T60. The slowest coupling capacitor sounding after that,
    // the bus (4.7 uF into 189 kohm, tau 0.888 s), falls 60 dB in
    // ln (1000) tau = 6.14 s. Both figures are rounded up below.
    const auto seconds = [] (const std::atomic<float>* value)
    { return value != nullptr ? static_cast<double> (value->load()) : 30.0; };
    return 0.73 * std::max (seconds (ampReleaseValue), seconds (globalReleaseValue)) + 6.2;
}

void CeresAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    const auto& program = programs[static_cast<std::size_t> (index)].patch;
    for (std::size_t i = 0; i < ceres::parameterCount(); ++i)
    {
        const auto info = ceres::parameterInfo (i);
        if (! programSetsKey (info.key))
            continue;
        double value = info.defaultValue;
        ceres::getParameter (program, info.key, value);
        auto* parameter = parameters.getParameter (info.key);
        if (dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr)
            value -= info.minimum;
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (value)));
    }
}

const juce::String CeresAudioProcessor::getProgramName (int index)
{
    return index >= 0 && index < getNumPrograms() ? programs[static_cast<std::size_t> (index)].name : juce::String();
}

void CeresAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty (kStateVersionProperty, kStateVersion, nullptr);
    state.setProperty (kProgramProperty, currentProgram.load(), nullptr);
    state.setProperty (kPluginVersionProperty, JucePlugin_VersionString, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CeresAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (kStateType.toString()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid())
        return;
    if (static_cast<int> (state.getProperty (kStateVersionProperty, 1)) < 2)
    {
        // The silent scaffold saved no sound: open as a new instance does.
        for (auto* parameter : getParameters())
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
        setCurrentProgram (0);
        return;
    }
    // A parameter the chunk does not mention takes its default, so a state
    // restores the same sound whatever the instance played before.
    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
            if (! state.getChildWithProperty ("id", ranged->paramID).isValid())
            {
                juce::ValueTree child ("PARAM");
                child.setProperty ("id", ranged->paramID, nullptr);
                child.setProperty ("value", ranged->convertFrom0to1 (ranged->getDefaultValue()), nullptr);
                state.appendChild (child, nullptr);
            }
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, static_cast<int> (state.getProperty (kProgramProperty, 0)));
    parameters.replaceState (state);
}

juce::AudioProcessorEditor* CeresAudioProcessor::createEditor()
{
    return new CeresAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CeresAudioProcessor();
}
