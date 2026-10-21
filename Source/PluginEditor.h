#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <memory>
#include <vector>

// The CS-50's three panels as rows of levers, in the panel's own order
// (Servicing Guide printed pp.2-7), the TONE SELECTOR as tablets for the
// research presets, then the rear panel, the engine's calibration controls
// and the plug-in's settings. Every parameter has exactly one control, whose
// component ID is the parameter ID. The size is fixed and set here alone, so
// the render tier can tell when the screenshot's size is due to change: only
// when this editor's source does.
class CeresAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kWidth = 1080, kHeight = 750;

    explicit CeresAudioProcessorEditor (CeresAudioProcessor&);
    ~CeresAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Defined in the .cpp: CERES_BUILD_IDENTITY is private to the plug-in target.
    static juce::String getBuildIdentity();

private:
    // Follows program changes the host makes and the engine's settings.
    void timerCallback() override;
    void refreshStatus();

    enum class Kind { lever, knob, choice };
    struct Control
    {
        juce::String id;
        Kind kind = Kind::lever;
        float cells = 1.0f; // width in the row's cells
        std::unique_ptr<juce::Component> widget;
        juce::Label caption;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> choice;
    };
    struct Section
    {
        juce::String group, title;
        std::vector<std::unique_ptr<Control>> controls;
        juce::Rectangle<int> bounds;
    };
    struct Row
    {
        std::vector<Section> sections;
        int height = 0;
    };

    std::unique_ptr<Control> makeControl (juce::RangedAudioParameter&, bool knob);

    CeresAudioProcessor& audioProcessor;
    std::unique_ptr<juce::LookAndFeel_V4> lookAndFeel;
    juce::Label title, subtitle, status, toneCaption;
    juce::OwnedArray<juce::TextButton> tablets;
    std::vector<Row> rows;
    juce::TooltipWindow tooltips { this, 700 };
};
