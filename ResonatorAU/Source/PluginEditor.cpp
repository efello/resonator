#include "PluginEditor.h"

using namespace resonator;

static const juce::Colour kBg      { 0xff17191c };
static const juce::Colour kPanel   { 0xff22262b };
static const juce::Colour kAccent  { 0xffe8a33d };
static const juce::Colour kText    { 0xffd9dde2 };

// ------------------------------------------------------------------------------------
void ResponseDisplay::timerCallback()
{
    auto& a = proc.apvts;
    std::array<float, 7> now {
        a.getRawParameterValue (ids::lowFreq)->load(),  a.getRawParameterValue (ids::midFreq)->load(),
        a.getRawParameterValue (ids::highFreq)->load(), a.getRawParameterValue (ids::emphasis)->load(),
        a.getRawParameterValue (ids::mode)->load(),     a.getRawParameterValue (ids::freqLaw)->load(),
        a.getRawParameterValue (ids::output)->load() };
    if (now != last) { last = now; repaint(); }
}

void ResponseDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (kPanel);  g.fillRoundedRectangle (r, 6.f);
    auto plot = r.reduced (34.f, 14.f);
    plot.removeFromBottom (8.f);

    constexpr float fLo = 20.f, fHi = 20000.f, dbLo = -30.f, dbHi = 20.f;
    auto xOf = [&] (float f) { return plot.getX() + plot.getWidth() * std::log10 (f / fLo) / std::log10 (fHi / fLo); };
    auto yOf = [&] (float db) { return plot.getBottom() - plot.getHeight() * (db - dbLo) / (dbHi - dbLo); };

    g.setFont (11.f);
    for (float db = -30.f; db <= 20.f; db += 10.f)
    {
        g.setColour (db == 0.f ? juce::Colour (0x55ffffff) : juce::Colour (0x22ffffff));
        g.drawHorizontalLine ((int) yOf (db), plot.getX(), plot.getRight());
        g.setColour (juce::Colour (0x99ffffff));
        g.drawText (juce::String ((int) db), (int) 2, (int) yOf (db) - 7, 30, 14, juce::Justification::centredRight);
    }
    for (float f : { 20.f, 30.f, 40.f, 50.f, 60.f, 80.f, 100.f, 200.f, 300.f, 400.f, 500.f, 600.f, 800.f, 1000.f,
                     2000.f, 3000.f, 4000.f, 5000.f, 6000.f, 8000.f, 10000.f, 20000.f })
    {
        const bool major = f == 100.f || f == 1000.f || f == 10000.f;
        g.setColour (juce::Colour (major ? 0x33ffffff : 0x14ffffff));
        g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());
        if (major || f == 20.f || f == 20000.f)
        {
            g.setColour (juce::Colour (0x99ffffff));
            g.drawText (f >= 1000.f ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f),
                        (int) xOf (f) - 20, (int) plot.getBottom() + 1, 40, 12, juce::Justification::centred);
        }
    }

    auto& a = proc.apvts;
    const std::array<double, 3> knobs { (double) a.getRawParameterValue (ids::lowFreq)->load(),
                                        (double) a.getRawParameterValue (ids::midFreq)->load(),
                                        (double) a.getRawParameterValue (ids::highFreq)->load() };
    const double emph = a.getRawParameterValue (ids::emphasis)->load();
    const auto mode = (Mode) juce::jlimit (0, 3, (int) a.getRawParameterValue (ids::mode)->load());
    const auto law = proc.currentLaw();
    const float outDb = a.getRawParameterValue (ids::output)->load();

    juce::Path curve;
    const int N = (int) plot.getWidth();
    for (int i = 0; i <= N; ++i)
    {
        const float f = fLo * std::pow (fHi / fLo, (float) i / (float) N);
        const float db = juce::jlimit (dbLo, dbHi, (float) analogMagnitudeDb (f, knobs, emph, mode, law) + outDb);
        const float x = plot.getX() + (float) i, y = yOf (db);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    juce::Path fill (curve);
    fill.lineTo (plot.getRight(), plot.getBottom()); fill.lineTo (plot.getX(), plot.getBottom()); fill.closeSubPath();
    g.setColour (kAccent.withAlpha (0.12f)); g.fillPath (fill);
    g.setColour (kAccent); g.strokePath (curve, juce::PathStrokeType (2.f));
}

// ------------------------------------------------------------------------------------
void ResonatorEditor::addKnob (Knob& k, const juce::String& text, const char* paramId)
{
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    k.slider.setColour (juce::Slider::thumbColourId, kText);
    k.slider.setColour (juce::Slider::textBoxTextColourId, kText);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (k.slider); addAndMakeVisible (k.label);
    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

ResonatorEditor::ResonatorEditor (ResonatorAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    addAndMakeVisible (display);
    addKnob (low,  "LOW",      ids::lowFreq);
    addKnob (mid,  "MID",      ids::midFreq);
    addKnob (high, "HIGH",     ids::highFreq);
    addKnob (emph, "EMPHASIS", ids::emphasis);
    addKnob (mix,  "MIX",      ids::mix);
    addKnob (out,  "OUTPUT",   ids::output);

    auto setupBox = [this] (juce::ComboBox& b, juce::Label& l, const juce::String& t, const char* id,
                            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
    {
        b.addItemList (dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))->choices, 1);
        l.setText (t, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, kText);
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (b); addAndMakeVisible (l);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, b);
    };
    setupBox (modeBox,    modeLabel,    "Mode",    ids::mode,    modeAtt);
    setupBox (lawBox,     lawLabel,     "Freq law", ids::freqLaw, lawAtt);
    setupBox (qualityBox, qualityLabel, "Quality", ids::quality, qualityAtt);

    bypass.setColour (juce::ToggleButton::textColourId, kText);
    bypass.setColour (juce::ToggleButton::tickColourId, kAccent);
    addAndMakeVisible (bypass);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::bypass, bypass);

    setSize (760, 470);
}

void ResonatorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kText);
    g.setFont (juce::FontOptions (22.f, juce::Font::bold));
    g.drawText ("RESONATOR", 16, 8, 300, 28, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.f));
    g.setColour (juce::Colour (0x99ffffff));
    g.drawText ("3-band resonant filter", 16, 32, 300, 16, juce::Justification::centredLeft);
}

void ResonatorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (44);

    auto top = area.removeFromTop (40);
    bypass.setBounds (top.removeFromRight (90));
    auto place = [&top] (juce::Label& l, juce::ComboBox& b, int labelW, int boxW)
    {
        l.setBounds (top.removeFromLeft (labelW)); b.setBounds (top.removeFromLeft (boxW).reduced (0, 6)); top.removeFromLeft (14);
    };
    place (modeLabel, modeBox, 40, 100);
    place (lawLabel, lawBox, 62, 130);
    place (qualityLabel, qualityBox, 56, 130);

    display.setBounds (area.removeFromTop (230));
    area.removeFromTop (10);

    const int w = area.getWidth() / 6;
    for (auto* k : { &low, &mid, &high, &emph, &mix, &out })
    {
        auto cell = area.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (18));
        k->slider.setBounds (cell);
    }
}
