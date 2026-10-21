#include "PluginEditor.h"
#include "PluginControls.h"

#include <algorithm>

namespace
{
const juce::Colour kPanelTop { 0xff2e3138 }, kPanelBottom { 0xff1b1c20 }, kInk { 0xffe9e4d4 },
    kDim { 0xff9c988d }, kSection { 0xff26282e }, kEdge { 0xff3d4048 }, kSlot { 0xff0e0f12 },
    kAccent { 0xffc8a85a };

constexpr int kMargin = 12, kGap = 8, kHeaderHeight = 30, kToneHeight = 46, kTitleHeight = 18,
              kCaptionHeight = 16, kTextBoxHeight = 15, kPadding = 8;

// Rows of panel sections by group ID. The engine's calibration controls fill
// the last row as knobs, with any parameter whose group no row names.
struct RowPlan
{
    std::vector<const char*> groups;
    int height;
};
const std::vector<RowPlan>& rowPlans()
{
    static const std::vector<RowPlan> plans {
        { { "vco", "vcf", "vca" }, 190 },                                          // Panel 1
        { { "pitch", "ring", "tone", "transposition", "touch", "volume" }, 160 },   // Panel 2
        { { "sub", "sustain", "portamento", "rear", "plugin" }, 160 },              // Panel 3, rear
    };
    return plans;
}
constexpr int kEngineRowHeight = 100;
constexpr int kPlannedHeight = 2 * kMargin + kHeaderHeight + kGap + kToneHeight + 4 * kGap + 190 + 160 + 160
                             + kEngineRowHeight;
static_assert (kPlannedHeight <= CeresAudioProcessorEditor::kHeight, "the rows must fit the editor");

bool planned (const juce::String& group)
{
    return std::any_of (rowPlans().begin(), rowPlans().end(), [&group] (const RowPlan& plan)
    {
        return std::any_of (plan.groups.begin(), plan.groups.end(), [&group] (const char* id) { return group == id; });
    });
}

class PanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, kPanelBottom);
        setColour (juce::Label::textColourId, kInk);
        setColour (juce::Slider::textBoxTextColourId, kInk);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        setColour (juce::Slider::rotarySliderOutlineColourId, kSlot);
        setColour (juce::Slider::thumbColourId, kInk);
        setColour (juce::ComboBox::backgroundColourId, kSlot);
        setColour (juce::ComboBox::outlineColourId, kEdge);
        setColour (juce::ComboBox::textColourId, kInk);
        setColour (juce::ComboBox::arrowColourId, kAccent);
        setColour (juce::PopupMenu::backgroundColourId, kSection);
        setColour (juce::PopupMenu::textColourId, kInk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3d45));
        setColour (juce::TextButton::buttonOnColourId, kAccent);
        setColour (juce::TextButton::textColourOffId, kInk);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::TooltipWindow::backgroundColourId, kSection);
        setColour (juce::TooltipWindow::textColourId, kInk);
        setColour (juce::TooltipWindow::outlineColourId, kEdge);
    }

    // A CS-50 lever: a dark slot and a square cap with a marker line.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float position, float minimum,
                           float maximum, juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, position, minimum, maximum, style, slider);
            return;
        }
        const float centre = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        g.setColour (kSlot);
        g.fillRoundedRectangle (centre - 2.5f, static_cast<float> (y), 5.0f, static_cast<float> (height), 2.5f);
        const float capWidth = juce::jmin (26.0f, static_cast<float> (width) - 6.0f), capHeight = 13.0f;
        const juce::Rectangle<float> cap (centre - capWidth * 0.5f, position - capHeight * 0.5f, capWidth, capHeight);
        g.setColour (slider.isEnabled() ? kInk : kDim);
        g.fillRoundedRectangle (cap, 2.0f);
        g.setColour (kSlot);
        g.drawLine (cap.getX() + 3.0f, position, cap.getRight() - 3.0f, position, 1.6f);
    }

    int getSliderThumbRadius (juce::Slider& slider) override
    {
        return slider.getSliderStyle() == juce::Slider::LinearVertical ? 8 : LookAndFeel_V4::getSliderThumbRadius (slider);
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (juce::FontOptions (10.5f));
        label->setMinimumHorizontalScale (0.7f);
        return label;
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (12.0f); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (13.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (11.5f); }
};

// RESONANCE reads with its minimum at the top of the lever, as on the panel.
class InvertedSlider final : public juce::Slider
{
public:
    double proportionOfLengthToValue (double proportion) override
    {
        return Slider::proportionOfLengthToValue (1.0 - proportion);
    }
    double valueToProportionOfLength (double value) override
    {
        return 1.0 - Slider::valueToProportionOfLength (value);
    }
};

const ceres_plugin::Group* findGroup (const juce::String& id)
{
    for (const auto& group : ceres_plugin::kGroups)
        if (id == group.id)
            return &group;
    return nullptr;
}
} // namespace

CeresAudioProcessorEditor::CeresAudioProcessorEditor (CeresAudioProcessor& p)
    : AudioProcessorEditor (p), audioProcessor (p), lookAndFeel (std::make_unique<PanelLookAndFeel>())
{
    setLookAndFeel (lookAndFeel.get());
    setOpaque (true);

    title.setText ("CERES", juce::dontSendNotification);
    title.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    title.setTooltip ("About Ceres " + getBuildIdentity());
    subtitle.setText ("CS-50 circuit model", juce::dontSendNotification);
    subtitle.setFont (juce::FontOptions (13.0f));
    subtitle.setColour (juce::Label::textColourId, kDim);
    status.setFont (juce::FontOptions (12.0f));
    status.setJustificationType (juce::Justification::centredRight);
    status.setColour (juce::Label::textColourId, kDim);
    toneCaption.setText ("TONE SELECTOR  -  research presets from the circuit study, not calibrated Yamaha voices",
                         juce::dontSendNotification);
    toneCaption.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    toneCaption.setColour (juce::Label::textColourId, kDim);
    for (auto* label : { &title, &subtitle, &status, &toneCaption })
        addAndMakeVisible (*label);

    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i)
    {
        auto* tablet = tablets.add (std::make_unique<juce::TextButton> (audioProcessor.getProgramName (i)));
        tablet->setClickingTogglesState (false);
        tablet->setRadioGroupId (1);
        tablet->setTooltip ("Program " + juce::String (i + 1) + ": " + audioProcessor.getProgramName (i));
        tablet->onClick = [this, i]
        {
            audioProcessor.setCurrentProgram (i);
            audioProcessor.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
            refreshStatus();
        };
        addAndMakeVisible (tablet);
    }

    // Sections from the processor's own parameter groups, so the editor
    // shows exactly the parameters the host sees.
    std::vector<Section> panel;
    Section engine;
    engine.group = ceres_plugin::kUnlistedGroup;
    engine.title = findGroup (ceres_plugin::kUnlistedGroup)->caption;
    for (const auto* group : audioProcessor.getParameterTree().getSubgroups (false))
    {
        const bool onPanel = planned (group->getID());
        Section section;
        section.group = group->getID();
        const auto* known = findGroup (group->getID());
        section.title = known != nullptr ? juce::String (known->caption) : group->getName().toUpperCase();
        for (auto* parameter : group->getParameters (true))
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                (onPanel ? section : engine).controls.push_back (
                    makeControl (*ranged, ! onPanel || ranged->paramID == "tune_cents"));
        if (! section.controls.empty())
            panel.push_back (std::move (section));
    }

    for (const auto& plan : rowPlans())
    {
        Row row;
        row.height = plan.height;
        for (const char* id : plan.groups)
            for (auto& section : panel)
                if (section.group == id && ! section.controls.empty())
                    row.sections.push_back (std::move (section));
        rows.push_back (std::move (row));
    }
    if (! engine.controls.empty())
    {
        Row row;
        row.height = kEngineRowHeight;
        row.sections.push_back (std::move (engine));
        rows.push_back (std::move (row));
    }

    // A one-control section already names it.
    for (auto& row : rows)
        for (auto& section : row.sections)
            if (section.controls.size() == 1 && section.controls.front()->caption.getText() == section.title)
                section.controls.front()->caption.setText ({}, juce::dontSendNotification);

    // Each control built its value box before it joined the editor, so with
    // the default look; joining a parent does not tell it, so tell it now.
    sendLookAndFeelChange();
    setSize (kWidth, kHeight);
    refreshStatus();
    startTimerHz (10);
}

CeresAudioProcessorEditor::~CeresAudioProcessorEditor()
{
    stopTimer();
    for (auto& row : rows)
        for (auto& section : row.sections)
            for (auto& control : section.controls)
            {
                control->slider.reset();
                control->choice.reset();
            }
    setLookAndFeel (nullptr);
}

std::unique_ptr<CeresAudioProcessorEditor::Control> CeresAudioProcessorEditor::makeControl (
    juce::RangedAudioParameter& parameter, bool knob)
{
    auto control = std::make_unique<Control>();
    control->id = parameter.paramID;
    const auto* presentation = ceres_plugin::findControl (parameter.paramID.toRawUTF8());
    const juce::String name = parameter.getName (64);
    if (presentation != nullptr)
        control->caption.setText (presentation->caption, juce::dontSendNotification);
    else if (parameter.paramID == ceres_plugin::kVoiceCountId)
        control->caption.setText ("VOICES", juce::dontSendNotification);
    else if (parameter.paramID == ceres_plugin::kOversamplingId)
        control->caption.setText ("QUALITY", juce::dontSendNotification);
    else
        control->caption.setText (name.toUpperCase(), juce::dontSendNotification);
    control->caption.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    control->caption.setJustificationType (juce::Justification::centred);
    control->caption.setMinimumHorizontalScale (0.6f);
    control->caption.setTooltip (name);
    addAndMakeVisible (control->caption);

    auto& state = audioProcessor.state();
    if (dynamic_cast<juce::AudioParameterChoice*> (&parameter) != nullptr
        || dynamic_cast<juce::AudioParameterInt*> (&parameter) != nullptr)
    {
        auto box = std::make_unique<juce::ComboBox>();
        const int steps = parameter.getNumSteps();
        for (int i = 0; i < steps; ++i)
            box->addItem (parameter.getText (steps > 1 ? static_cast<float> (i) / static_cast<float> (steps - 1) : 0.0f, 32),
                          i + 1);
        box->setTooltip (name);
        box->setComponentID (parameter.paramID);
        control->choice = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, parameter.paramID, *box);
        control->kind = Kind::choice;
        control->cells = knob ? 1.4f : 2.0f;
        control->widget = std::move (box);
    }
    else
    {
        auto slider = presentation != nullptr && presentation->inverted ? std::make_unique<InvertedSlider>()
                                                                         : std::make_unique<juce::Slider>();
        slider->setSliderStyle (knob ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearVertical);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, kTextBoxHeight);
        slider->setTooltip (name);
        slider->setComponentID (parameter.paramID);
        slider->setPopupDisplayEnabled (false, false, nullptr);
        control->slider = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, parameter.paramID, *slider);
        control->kind = knob ? Kind::knob : Kind::lever;
        control->cells = parameter.paramID == "tune_cents" ? 1.6f : 1.0f;
        control->widget = std::move (slider);
    }
    addAndMakeVisible (*control->widget);
    return control;
}

void CeresAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient::vertical (kPanelTop, 0.0f, kPanelBottom, static_cast<float> (getHeight())));
    g.fillAll();
    for (const auto& row : rows)
        for (const auto& section : row.sections)
        {
            const auto box = section.bounds.toFloat();
            g.setColour (kSection);
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (kEdge);
            g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.0f);
            if (section.title.isNotEmpty())
            {
                const auto strip = section.bounds.withHeight (kTitleHeight).reduced (6, 0);
                g.setColour (kAccent);
                g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
                g.drawFittedText (section.title, strip, juce::Justification::centredLeft, 1, 0.7f);
            }
        }
    g.setColour (kEdge);
    g.drawHorizontalLine (kMargin + kHeaderHeight, static_cast<float> (kMargin), static_cast<float> (getWidth() - kMargin));
}

void CeresAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin);
    auto header = area.removeFromTop (kHeaderHeight);
    title.setBounds (header.removeFromLeft (96));
    subtitle.setBounds (header.removeFromLeft (220));
    status.setBounds (header);
    area.removeFromTop (kGap);

    auto tone = area.removeFromTop (kToneHeight);
    toneCaption.setBounds (tone.removeFromTop (18));
    const int count = tablets.size();
    for (int i = 0; i < count; ++i)
    {
        const int left = tone.getX() + tone.getWidth() * i / count, right = tone.getX() + tone.getWidth() * (i + 1) / count;
        tablets[i]->setBounds (juce::Rectangle<int> (left, tone.getY(), right - left, tone.getHeight()).reduced (2, 1));
    }

    for (auto& row : rows)
    {
        area.removeFromTop (kGap);
        auto band = area.removeFromTop (row.height);
        float cells = 0.0f;
        for (const auto& section : row.sections)
            for (const auto& control : section.controls)
                cells += control->cells;
        const int sections = static_cast<int> (row.sections.size());
        const float cell = (static_cast<float> (band.getWidth()) - static_cast<float> (kGap * (sections - 1))
                            - static_cast<float> (kPadding * sections))
                         / juce::jmax (1.0f, cells);
        float x = static_cast<float> (band.getX());
        for (auto& section : row.sections)
        {
            float width = static_cast<float> (kPadding);
            for (const auto& control : section.controls)
                width += control->cells * cell;
            section.bounds = juce::Rectangle<int> (juce::roundToInt (x), band.getY(), juce::roundToInt (width), band.getHeight());
            auto inner = section.bounds.reduced (kPadding / 2, 0);
            inner.removeFromTop (kTitleHeight);
            inner.removeFromBottom (4);
            float left = static_cast<float> (inner.getX());
            for (auto& control : section.controls)
            {
                const float right = left + control->cells * cell;
                auto slot = juce::Rectangle<int> (juce::roundToInt (left), inner.getY(),
                                                  juce::roundToInt (right) - juce::roundToInt (left), inner.getHeight());
                control->caption.setBounds (slot.removeFromBottom (kCaptionHeight));
                if (control->kind == Kind::choice)
                    control->widget->setBounds (slot.withSizeKeepingCentre (slot.getWidth() - 4, 22));
                else
                    control->widget->setBounds (slot);
                left = right;
            }
            x += width + static_cast<float> (kGap);
        }
    }
}

void CeresAudioProcessorEditor::timerCallback()
{
    refreshStatus();
}

void CeresAudioProcessorEditor::refreshStatus()
{
    const int current = audioProcessor.getCurrentProgram();
    for (int i = 0; i < tablets.size(); ++i)
        tablets[i]->setToggleState (i == current, juce::dontSendNotification);

    juce::String text;
    if (audioProcessor.sampleRateUnsupported())
        text = "Silent: the engine runs at 8 to 384 kHz";
    else if (audioProcessor.engineRunning())
    {
        text << audioProcessor.appliedVoices() << (audioProcessor.appliedVoices() == 1 ? " voice, " : " voices, ")
             << audioProcessor.appliedOversampling() << "x oversampling";
        if (audioProcessor.configurationPending())
            text << "  -  the new setting waits for silence";
    }
    status.setText (text, juce::dontSendNotification);
}

juce::String CeresAudioProcessorEditor::getBuildIdentity()
{
    return CERES_BUILD_IDENTITY;
}
