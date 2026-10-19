#pragma once

#include <JuceHeader.h>

#include "DSP/CeresEngine.h"

// JUCE front end over the canonical engine. It owns MIDI, state and latency;
// synthesis stays in Source/DSP. Parameters and programs arrive with the
// instrument's controls and presets.
class CeresAudioProcessor final : public juce::AudioProcessor
{
public:
    CeresAudioProcessor();
    ~CeresAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    void handleMidi (const juce::MidiMessage&) noexcept;

    ceres::CeresEngine engine;
};
