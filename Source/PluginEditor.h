#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

// A plain panel with the product name; controls arrive with the instrument.
class CeresAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CeresAudioProcessorEditor (CeresAudioProcessor&);
    ~CeresAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::String getBuildIdentity() { return CERES_BUILD_IDENTITY; }

private:
    juce::Label title;
};
