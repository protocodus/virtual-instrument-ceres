#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

namespace
{
constexpr int kStateVersion = 1;
const juce::Identifier kStateType { "Ceres" }, kStateVersionProperty { "stateVersion" };
} // namespace

CeresAudioProcessor::CeresAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool CeresAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void CeresAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.latencySamples());
}

void CeresAudioProcessor::handleMidi (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

void CeresAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int frames = buffer.getNumSamples();
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    // Events start at their own sample.
    int position = 0;
    for (const auto metadata : midi)
    {
        const int at = std::clamp (metadata.samplePosition, position, frames);
        if (at > position)
            engine.process (left + position, right + position, static_cast<std::size_t> (at - position));
        position = at;
        handleMidi (metadata.getMessage());
    }
    if (frames > position)
        engine.process (left + position, right + position, static_cast<std::size_t> (frames - position));
}

juce::AudioProcessorEditor* CeresAudioProcessor::createEditor()
{
    return new CeresAudioProcessorEditor (*this);
}

void CeresAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state (kStateType);
    state.setProperty (kStateVersionProperty, kStateVersion, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CeresAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Nothing is stored yet beyond the version; a foreign or newer chunk is ignored.
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (kStateType.toString()))
        return;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CeresAudioProcessor();
}
