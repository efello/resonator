#include "PluginEditor.h"

// ---------------------------------------------------------------------------------------
// All geometry below is in "panel units": the hardware front plate is 1920 x 977 units.
// ---------------------------------------------------------------------------------------
namespace
{
    constexpr float kW = 1920.f, kH = 977.f;

    const juce::Colour kPlate  { 0xff0f0f10 };
    const juce::Colour kInk    { 0xfff2f2ee };    // white panel printing
    const juce::Colour kGold   { 0xffc9a227 };

    constexpr float kSlotTop = 335.f, kSlotBottom = 830.f;       // fader travel
    constexpr float kCapW = 100.f;                               // cap width; height = 0.72 * width
    constexpr float kCapHalfH = kCapW * 0.72f * 0.5f;

    // x centre of each fader slot (same order as ResonatorEditor::faders)
    constexpr float kFaderX[11] = { 178, 298,  647, 770, 890,  1060, 1185, 1305,  1480, 1605, 1725 };
}

juce::Font PanelLookAndFeel::panelFont (float h)
{
    return juce::Font (juce::FontOptions ("Gill Sans", h, juce::Font::plain));
}

PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff111112));
    setColour (juce::ComboBox::outlineColourId,    kInk.withAlpha (0.8f));
    setColour (juce::ComboBox::textColourId,       kInk);
    setColour (juce::ComboBox::arrowColourId,      kInk);
    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff17171a));
    setColour (juce::PopupMenu::textColourId,                  kInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3a3f));
    setColour (juce::PopupMenu::highlightedTextColourId,       kInk);
    setColour (juce::BubbleComponent::backgroundColourId,      juce::Colour (0xee202024));
    setColour (juce::BubbleComponent::outlineColourId,         kInk.withAlpha (0.6f));
    setColour (juce::TooltipWindow::textColourId,              kInk);
}

juce::Slider::SliderLayout PanelLookAndFeel::getSliderLayout (juce::Slider& s)
{
    juce::Slider::SliderLayout l;
    auto b = s.getLocalBounds();
    // keep the cap fully inside the component at both ends of the travel
    b = s.isVertical() ? b.reduced (0, juce::roundToInt ((float) b.getWidth() * 0.36f))
                       : b.reduced (juce::roundToInt ((float) b.getHeight() * 0.7f), 0);
    l.sliderBounds = b;
    return l;
}

void PanelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                                         float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    auto full = s.getLocalBounds().toFloat();
    const bool vertical = s.isVertical();

    if (vertical)
    {
        const float capW = full.getWidth(), capH = capW * 0.72f, slotW = capW * 0.28f;
        juce::Rectangle<float> slot (full.getCentreX() - slotW * 0.5f, (float) y - 6.f, slotW, (float) h + 12.f);
        g.setColour (juce::Colour (0xff060607));          g.fillRoundedRectangle (slot, slotW * 0.25f);
        g.setColour (juce::Colour (0xff34353a));          g.drawRoundedRectangle (slot, slotW * 0.25f, 1.2f);

        juce::Rectangle<float> cap (full.getX(), sliderPos - capH * 0.5f, capW, capH);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (cap.translated (capW * 0.02f, capH * 0.07f), capW * 0.05f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343437), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff0d0d0e), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, capW * 0.05f);
        g.setColour (juce::Colour (0xff4a4a50));          g.drawRoundedRectangle (cap, capW * 0.05f, 1.2f);
        g.setColour (kInk);
        g.fillRect (juce::Rectangle<float> (cap.getX() + capW * 0.07f, sliderPos - capH * 0.07f, capW * 0.86f,
                                            juce::jmax (2.f, capH * 0.07f)));
    }
    else
    {
        const float capH = full.getHeight(), capW = capH * 1.4f;
        g.setColour (kInk.withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> ((float) x, full.getCentreY() - 1.f, (float) w, 2.f));
        juce::Rectangle<float> cap (sliderPos - capW * 0.5f, full.getY() + capH * 0.12f, capW, capH * 0.76f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff343437), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff0d0d0e), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.f);
        g.setColour (juce::Colour (0xff4a4a50));          g.drawRoundedRectangle (cap, 3.f, 1.f);
        g.setColour (kInk);
        g.fillRect (juce::Rectangle<float> (sliderPos - 1.5f, cap.getY() + 3.f, 3.f, cap.getHeight() - 6.f));
    }
}

// ---------------------------------------------------------------------------------------
PowerSwitch::PowerSwitch (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v) { on = v < 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
}

void PowerSwitch::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 115.f));
    g.setColour (kInk);
    g.setFont (PanelLookAndFeel::panelFont (28.f));
    g.drawText ("OFF", juce::Rectangle<float> (0.f, 18.f, 60.f, 30.f), juce::Justification::centredRight);
    g.drawText ("ON",  juce::Rectangle<float> (0.f, 65.f, 60.f, 30.f), juce::Justification::centredRight);

    juce::Rectangle<float> body (68.f, 17.f, 34.f, 78.f);
    g.setColour (juce::Colour (0xff3a3a3f)); g.fillRoundedRectangle (body.expanded (3.f), 3.f);
    g.setColour (juce::Colour (0xff060607)); g.fillRoundedRectangle (body, 2.f);
    juce::Rectangle<float> lever (body.getX() + 2.f, on ? body.getBottom() - 36.f : body.getY() + 2.f, body.getWidth() - 4.f, 34.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b3f), lever.getX(), lever.getY(),
                                             juce::Colour (0xff131314), lever.getX(), lever.getBottom(), false));
    g.fillRoundedRectangle (lever, 3.f);
    g.setColour (kInk.withAlpha (0.85f));
    g.fillRect (lever.reduced (4.f, 0.f).withHeight (3.f).withY (lever.getCentreY() - 1.5f));
}

void PowerSwitch::mouseDown (const juce::MouseEvent& e)
{
    const bool wantOn = e.position.y / ((float) getWidth() / 115.f) > 56.f;
    attachment.setValueAsCompleteGesture (wantOn ? 0.f : 1.f);
}

// ---------------------------------------------------------------------------------------
namespace { constexpr float kModeRowY[4] = { 84.f, 112.5f, 141.f, 169.5f }; }

PassModeSwitch::PassModeSwitch (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v) { index = juce::jlimit (0, 3, juce::roundToInt (v)); repaint(); })
{
    attachment.sendInitialUpdate();
}

void PassModeSwitch::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 130.f));
    g.setColour (kInk);
    g.setFont (PanelLookAndFeel::panelFont (34.f));
    g.drawText ("PASS", juce::Rectangle<float> (40.f, 0.f, 130.f, 34.f), juce::Justification::centredLeft, false);
    g.drawText ("MODE", juce::Rectangle<float> (40.f, 30.f, 130.f, 34.f), juce::Justification::centredLeft, false);

    const char* names[4] = { "LOW", "BAND1", "BAND2", "HIGH" };
    g.setFont (PanelLookAndFeel::panelFont (25.f));
    for (int i = 0; i < 4; ++i)
        g.drawText (names[i], juce::Rectangle<float> (0.f, kModeRowY[i] - 13.f, 82.f, 26.f), juce::Justification::centredLeft);

    juce::Rectangle<float> body (86.f, 70.f, 24.f, 114.f);
    g.setColour (juce::Colour (0xff3a3a3f)); g.fillRoundedRectangle (body.expanded (2.f), 3.f);
    g.setColour (juce::Colour (0xff060607)); g.fillRoundedRectangle (body, 2.f);
    juce::Rectangle<float> lever (body.getX() + 1.5f, kModeRowY[index] - 11.f, body.getWidth() - 3.f, 22.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b3f), lever.getX(), lever.getY(),
                                             juce::Colour (0xff131314), lever.getX(), lever.getBottom(), false));
    g.fillRoundedRectangle (lever, 3.f);
    g.setColour (kInk.withAlpha (0.85f));
    g.fillRect (lever.reduced (3.f, 0.f).withHeight (2.5f).withY (lever.getCentreY() - 1.f));
}

void PassModeSwitch::select (const juce::MouseEvent& e)
{
    const float y = e.position.y / ((float) getWidth() / 130.f);
    int best = 0;
    for (int i = 1; i < 4; ++i)
        if (std::abs (y - kModeRowY[i]) < std::abs (y - kModeRowY[best])) best = i;
    if (best != index) attachment.setValueAsCompleteGesture ((float) best);
}

// ---------------------------------------------------------------------------------------
static const char* faderParamId (int i)
{
    if (i == 0) return ids::dry;
    if (i == 1) return ids::res;
    const int band = (i - 2) / 3, kind = (i - 2) % 3;
    return kind == 0 ? ids::cf[band] : (kind == 1 ? ids::emph[band] : ids::gain[band]);
}

ResonatorEditor::ResonatorEditor (ResonatorAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      powerSwitch (*p.apvts.getParameter (ids::bypass)),
      passMode (*p.apvts.getParameter (ids::mode))
{
    setLookAndFeel (&laf);

    for (int i = 0; i < 11; ++i)
    {
        auto& f = faders[(size_t) i];
        f.slider.setPopupDisplayEnabled (true, false, this);
        f.slider.setPopupMenuEnabled (false);
        addAndMakeVisible (f.slider);
        f.attach = std::make_unique<SliderAtt> (proc.apvts, faderParamId (i), f.slider);
    }
    addAndMakeVisible (powerSwitch);
    addAndMakeVisible (passMode);

    auto setupBox = [this] (juce::ComboBox& b, const char* id, std::unique_ptr<ComboAtt>& att)
    {
        b.addItemList (dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))->choices, 1);
        addAndMakeVisible (b);
        att = std::make_unique<ComboAtt> (proc.apvts, id, b);
    };
    setupBox (lawBox, ids::freqLaw, lawAtt);
    setupBox (qualityBox, ids::quality, qualityAtt);

    outputSlider.setPopupDisplayEnabled (true, false, this);
    addAndMakeVisible (outputSlider);
    outputAtt = std::make_unique<SliderAtt> (proc.apvts, ids::output, outputSlider);

    const int w = 1100;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setResizeLimits (770, (int) (770 * kH / kW), 1920, 977);
    setSize (w, juce::roundToInt (w * kH / kW));
}

ResonatorEditor::~ResonatorEditor() { setLookAndFeel (nullptr); }

void ResonatorEditor::resized()
{
    const float s = (float) getWidth() / kW;
    auto R = [s] (float x, float y, float w, float h)
    { return juce::Rectangle<int> (juce::roundToInt (x * s), juce::roundToInt (y * s),
                                   juce::roundToInt (w * s), juce::roundToInt (h * s)); };

    for (size_t i = 0; i < 11; ++i)
        faders[i].slider.setBounds (R (kFaderX[i] - kCapW * 0.5f, kSlotTop - kCapW * 0.36f,
                                       kCapW, (kSlotBottom - kSlotTop) + kCapW * 0.72f));

    powerSwitch.setBounds (R (385, 445, 115, 110));
    passMode.setBounds (R (370, 610, 130, 190));

    lawBox.setBounds (R (295, 38, 235, 52));
    qualityBox.setBounds (R (735, 38, 235, 52));
    outputSlider.setBounds (R (1395, 40, 310, 46));
}

void ResonatorEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / kW;
    g.fillAll (kPlate);
    g.addTransform (juce::AffineTransform::scale (s));

    // subtle plate shading
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1a1c), 0.f, 0.f, juce::Colour (0xff0b0b0c), 0.f, kH, false));
    g.fillRect (0.f, 0.f, kW, kH);

    // frames
    g.setColour (kInk);
    g.drawRect (juce::Rectangle<float> (90.f, 27.f, 1737.f, 76.f), 3.f);
    g.drawRect (juce::Rectangle<float> (85.f, 112.f, 1740.f, 840.f), 3.f);

    // header strip text
    g.setFont (PanelLookAndFeel::panelFont (34.f));
    g.drawText ("FREQ LAW", juce::Rectangle<float> (110.f, 38.f, 180.f, 52.f), juce::Justification::centredLeft);
    g.drawText ("QUALITY",  juce::Rectangle<float> (565.f, 38.f, 165.f, 52.f), juce::Justification::centredLeft);
    g.drawText ("OUTPUT",   juce::Rectangle<float> (1245.f, 38.f, 145.f, 52.f), juce::Justification::centredLeft);

    // title
    g.setFont (PanelLookAndFeel::panelFont (92.f));
    g.drawText ("RESONATOR", juce::Rectangle<float> (560.f, 140.f, 730.f, 100.f), juce::Justification::centred);

    // screws / holes
    auto screw = [&g] (float x, float y, float r)
    {
        g.setColour (kGold.darker (0.5f)); g.fillEllipse (x - r - 2, y - r - 2, 2 * r + 4, 2 * r + 4);
        g.setColour (kGold);               g.fillEllipse (x - r, y - r, 2 * r, 2 * r);
        g.setColour (juce::Colour (0xff3b2f08));
        g.drawLine (x - r * 0.6f, y + r * 0.3f, x + r * 0.6f, y - r * 0.3f, 4.f);
    };
    for (auto pt : { juce::Point<float> (42, 237), { 35, 714 }, { 1875, 257 }, { 1862, 744 },
                     { 975, 429 }, { 555, 745 }, { 1395, 437 }, { 1390, 762 } })
        screw (pt.x, pt.y, 21.f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (562 - 15, 424 - 15, 30, 30); g.fillEllipse (970 - 15, 752 - 15, 30, 30);

    // fader ticks
    const float tickFrac[5] = { 0.107f, 0.293f, 0.485f, 0.677f, 0.873f };
    g.setColour (kInk);
    for (int i = 0; i < 11; ++i)
        for (float f : tickFrac)
            g.fillRect (juce::Rectangle<float> (kFaderX[i] - 66.f, kSlotTop + (kSlotBottom - kSlotTop) * f - 1.5f, 132.f, 3.f));

    // column headings
    g.setFont (PanelLookAndFeel::panelFont (46.f));
    auto head = [&g] (const char* t, float x, float y)
    { g.drawText (t, juce::Rectangle<float> (x - 100.f, y - 28.f, 200.f, 56.f), juce::Justification::centred); };
    head ("DRY", kFaderX[0], 288); head ("RES", kFaderX[1], 288);
    const char* sub[3] = { "CF", "EMPH", "GAIN" };
    for (int b = 0; b < 3; ++b)
        for (int k = 0; k < 3; ++k) head (sub[k], kFaderX[2 + b * 3 + k], 290.f + (float) b * 5.f);

    // scale labels (left of each slot)
    g.setFont (PanelLookAndFeel::panelFont (28.f));
    auto left = [&g] (const juce::String& t, float slotX, float y)
    { g.drawText (t, juce::Rectangle<float> (slotX - 130.f, y - 15.f, 108.f, 30.f), juce::Justification::centredRight); };
    left ("10", kFaderX[0], kSlotTop + 12); left ("10", kFaderX[1], kSlotTop + 12);
    const char* cfTop[3] = { "300", "1.5", "7.5" };  const char* cfTopU[3] = { "Hz", "KHz", "KHz" };
    const char* cfBot[3] = { "60", "300", "1.5" };   const char* cfBotU[3] = { "Hz", "Hz", "KHz" };
    for (int b = 0; b < 3; ++b)
    {
        const float cfX = kFaderX[2 + b * 3], emX = kFaderX[3 + b * 3];
        left (cfTop[b],  cfX, kSlotTop + 20);   left (cfTopU[b], cfX, kSlotTop + 46);
        left (cfBot[b],  cfX, kSlotBottom - 34); left (cfBotU[b], cfX, kSlotBottom - 8);
        left ("10",  emX, kSlotTop + 20);
        left ("0.5", emX, kSlotBottom - 20);
    }

    // group brackets
    g.setFont (PanelLookAndFeel::panelFont (46.f));
    struct Br { float x0, x1; const char* t; float w; };
    for (auto br : { Br { 640, 885, "LOW", 92 }, Br { 1055, 1300, "MED", 100 }, Br { 1470, 1715, "HIGH", 110 } })
    {
        const float y = 884.f, cx = 0.5f * (br.x0 + br.x1);
        g.fillRect (br.x0, y - 24.f, 3.f, 26.f);               g.fillRect (br.x1 - 3.f, y - 24.f, 3.f, 26.f);
        g.fillRect (br.x0, y - 1.f, cx - br.w * 0.5f - br.x0 - 6.f, 3.f);
        g.fillRect (cx + br.w * 0.5f + 6.f, y - 1.f, br.x1 - (cx + br.w * 0.5f + 6.f), 3.f);
        g.drawText (br.t, juce::Rectangle<float> (cx - 80.f, y - 28.f, 160.f, 56.f), juce::Justification::centred);
    }
}
