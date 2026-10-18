#include "PluginEditor.h"

namespace
{
const juce::Colour kPanelTop { 0xff2e3138 }, kPanelBottom { 0xff1b1c20 }, kInk { 0xffe9e4d4 };
} // namespace

CeresAudioProcessorEditor::CeresAudioProcessorEditor (CeresAudioProcessor& p) : AudioProcessorEditor (p)
{
    title.setText ("CERES", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    title.setColour (juce::Label::textColourId, kInk);
    title.setTooltip ("About Ceres " + getBuildIdentity());
    addAndMakeVisible (title);
    setSize (480, 240);
}

void CeresAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient::vertical (kPanelTop, 0.0f, kPanelBottom, static_cast<float> (getHeight())));
    g.fillAll();
}

void CeresAudioProcessorEditor::resized()
{
    title.setBounds (12, 8, 200, 28);
}
