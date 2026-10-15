#pragma once
#include "PluginProcessor.h"
#include "Presets.h"

// ---- skins ----------------------------------------------------------------------------
struct Skin
{
    const char* name;
    juce::uint32 plateTop, plateBottom, ink, capTop, capBottom, capEdge, capLine, slot, slotEdge;
};
const std::array<Skin, 6>& getSkins();

// Look of the hardware front panel; colours come from the current skin.
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum ColourIds { inkId = 0x7100001, plateTopId, plateBottomId, capTopId, capBottomId,
                     capEdgeId, capLineId, slotId, slotEdgeId };

    PanelLookAndFeel() { applySkin (getSkins()[0]); }
    void applySkin (const Skin&);

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return panelFont (26.f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return panelFont (22.f); }
    juce::Font getPopupMenuFont() override { return panelFont (18.f); }
    static juce::Font panelFont (float height);
};

// Slider / button that ignore right-clicks (right-click is reserved for the skin menu)
struct PanelSlider : juce::Slider
{
    using juce::Slider::Slider;
    void mouseDown (const juce::MouseEvent& e) override        { rightClick = e.mods.isPopupMenu(); if (! rightClick) Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override        { if (! rightClick) Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override          { if (! rightClick) Slider::mouseUp (e); rightClick = false; }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDoubleClick (e); }
    bool rightClick = false;
};

struct PanelButton : juce::TextButton
{
    using juce::TextButton::TextButton;
    void mouseDown (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) TextButton::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) TextButton::mouseUp (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) TextButton::mouseDrag (e); }
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

class ResonatorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ResonatorEditor (ResonatorAudioProcessor&);
    ~ResonatorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void applySkin (int index);      // also used by the offline snapshot test

private:
    void timerCallback() override;
    void showSkinMenu();
    void refreshPresetList();
    void loadEntry (int index);
    void stepPreset (int delta);
    void savePreset();
    void loadPresetFile();

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Fader
    {
        PanelSlider slider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
        std::unique_ptr<SliderAtt> attach;
    };

    ResonatorAudioProcessor& proc;
    PanelLookAndFeel laf;
    int skinIndex = 0;
    bool menuOpen = false;

    std::array<Fader, 11> faders;      // 0 DRY, 1 RES, 2-4 LOW (CF, EMPH, GAIN), 5-7 MED, 8-10 HIGH
    PowerSwitch powerSwitch;
    PassModeSwitch passMode;

    juce::ComboBox presetBox, lawBox, qualityBox;
    PanelButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, loadBtn { "LOAD" };
    PanelSlider outputSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<ComboAtt> lawAtt, qualityAtt;
    std::unique_ptr<SliderAtt> outputAtt;

    std::vector<presets::Entry> entries;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorEditor)
};
