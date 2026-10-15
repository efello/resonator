#pragma once
#include "PluginProcessor.h"

// Live magnitude-response display (analytic model of the current settings)
class ResponseDisplay : public juce::Component, private juce::Timer
{
public:
    explicit ResponseDisplay (ResonatorAudioProcessor& p) : proc (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    ResonatorAudioProcessor& proc;
    std::array<float, 7> last {};
};

class ResonatorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ResonatorEditor (ResonatorAudioProcessor&);
    ~ResonatorEditor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    void addKnob (Knob&, const juce::String& text, const char* paramId);

    ResonatorAudioProcessor& proc;
    ResponseDisplay display;
    Knob low, mid, high, emph, mix, out;
    juce::ComboBox modeBox, lawBox, qualityBox;
    juce::Label modeLabel, lawLabel, qualityLabel;
    juce::ToggleButton bypass { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt, lawAtt, qualityAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorEditor)
};
