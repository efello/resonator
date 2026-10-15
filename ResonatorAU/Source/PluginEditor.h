#pragma once
#include "PluginProcessor.h"

// Look of the hardware front panel: black plate, white hairline graphics, black fader caps.
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return panelFont (24.f); }
    juce::Font getPopupMenuFont() override { return panelFont (18.f); }
    static juce::Font panelFont (float height);
};

// 2-position lever (ON / OFF).  Bound to the "bypass" parameter (true = OFF).
class PowerSwitch : public juce::Component
{
public:
    explicit PowerSwitch (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    bool on = true;
    juce::ParameterAttachment attachment;
};

// 4-position lever (LOW / BAND1 / BAND2 / HIGH).  Bound to the "mode" choice parameter.
class PassModeSwitch : public juce::Component
{
public:
    explicit PassModeSwitch (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { select (e); }
    void mouseDrag (const juce::MouseEvent& e) override { select (e); }
private:
    void select (const juce::MouseEvent&);
    int index = 1;
    juce::ParameterAttachment attachment;
};

class ResonatorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ResonatorEditor (ResonatorAudioProcessor&);
    ~ResonatorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Fader
    {
        juce::Slider slider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
        std::unique_ptr<SliderAtt> attach;
    };

    ResonatorAudioProcessor& proc;
    PanelLookAndFeel laf;

    std::array<Fader, 11> faders;      // 0 DRY, 1 RES, 2-4 LOW (CF, EMPH, GAIN), 5-7 MED, 8-10 HIGH
    PowerSwitch powerSwitch;
    PassModeSwitch passMode;

    juce::ComboBox lawBox, qualityBox;
    juce::Slider outputSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<ComboAtt> lawAtt, qualityAtt;
    std::unique_ptr<SliderAtt> outputAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorEditor)
};
