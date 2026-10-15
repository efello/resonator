#include "PluginEditor.h"

// ---------------------------------------------------------------------------------------
// All geometry below is in "panel units": the hardware front plate is 1920 x 977 units.
// ---------------------------------------------------------------------------------------
namespace
{
    constexpr float kW = 1920.f, kH = 977.f;

    constexpr float kSlotTop = 335.f, kSlotBottom = 830.f;       // fader travel
    constexpr float kCapW = 100.f;                               // cap width; height = 0.72 * width

    // x centre of each fader slot (same order as ResonatorEditor::faders)
    constexpr float kFaderX[11] = { 178, 298,  647, 770, 890,  1060, 1185, 1305,  1480, 1605, 1725 };
}

juce::Font PanelLookAndFeel::panelFont (float h)
{
   #if JUCE_WINDOWS
    return juce::Font (juce::FontOptions ("Segoe UI", h, juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions ("Gill Sans", h, juce::Font::plain));
   #endif
}

const std::array<Skin, 6>& getSkins()
{
    //                    name         plateTop    plateBottom  ink         capTop      capBottom   capEdge     capLine     slot        slotEdge    holo
    static const std::array<Skin, 6> s {{
        { "Black",         0xff1a1a1c, 0xff0b0b0c, 0xfff2f2ee, 0xff343437, 0xff0d0d0e, 0xff4a4a50, 0xfff2f2ee, 0xff060607, 0xff34353a, false },
        { "Cream",         0xffe6dcc3, 0xffd2c6a8, 0xff22201c, 0xff3a3631, 0xff14120f, 0xff5a544b, 0xffeee6d0, 0xff2a2620, 0xff6d6455, false },
        { "Midnight Blue", 0xff0e1a2e, 0xff070d19, 0xffa8d8ff, 0xff2a3d5c, 0xff0b1426, 0xff4f7099, 0xffcfeaff, 0xff040810, 0xff22385a, false },
        // holographic: plate colours below are only used for menus / combo boxes; the plate itself is painted as animated foil
        { "Holographic",   0xfff1e8ff, 0xffd9ccff, 0xff231a45, 0xff3a3358, 0xff0f0b22, 0xffc7b0ff, 0xffeafcff, 0xff120d26, 0xff8a78e6, true  },
        { "Silver",        0xffc9ccd0, 0xffa9adb3, 0xff15161a, 0xff3a3b3f, 0xff111214, 0xff5d6066, 0xfff4f4f4, 0xff1b1c20, 0xff6b6e75, false },
        { "Phosphor",      0xff0a0f0a, 0xff040704, 0xff58ff8a, 0xff1d2a1f, 0xff060a07, 0xff2f5a3a, 0xff58ff8a, 0xff020402, 0xff1c3a24, false }
    }};
    return s;
}

void PanelLookAndFeel::applySkin (const Skin& k)
{
    setColour (inkId, juce::Colour (k.ink));           setColour (plateTopId, juce::Colour (k.plateTop));
    setColour (plateBottomId, juce::Colour (k.plateBottom));
    setColour (capTopId, juce::Colour (k.capTop));     setColour (capBottomId, juce::Colour (k.capBottom));
    setColour (capEdgeId, juce::Colour (k.capEdge));   setColour (capLineId, juce::Colour (k.capLine));
    setColour (slotId, juce::Colour (k.slot));         setColour (slotEdgeId, juce::Colour (k.slotEdge));

    const juce::Colour ink (k.ink), top (k.plateTop), bottom (k.plateBottom);
    setColour (juce::ComboBox::backgroundColourId, bottom.withAlpha (0.9f));
    setColour (juce::ComboBox::outlineColourId,    ink.withAlpha (0.85f));
    setColour (juce::ComboBox::textColourId,       ink);
    setColour (juce::ComboBox::arrowColourId,      ink);
    setColour (juce::PopupMenu::backgroundColourId,            top.withAlpha (1.0f));
    setColour (juce::PopupMenu::textColourId,                  ink);
    setColour (juce::PopupMenu::headerTextColourId,            ink.withAlpha (0.6f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ink.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId,       ink);
    setColour (juce::BubbleComponent::backgroundColourId,      bottom.withAlpha (0.95f));
    setColour (juce::BubbleComponent::outlineColourId,         ink.withAlpha (0.6f));
    setColour (juce::TooltipWindow::textColourId,              ink);
    setColour (juce::TextButton::textColourOffId,              ink);
    setColour (juce::TextButton::textColourOnId,               ink);
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const auto ink = findColour (inkId);
    if (down || over) { g.setColour (ink.withAlpha (down ? 0.30f : 0.14f)); g.fillRoundedRectangle (r, 4.f); }
    g.setColour (ink.withAlpha (0.9f));
    g.drawRoundedRectangle (r, 4.f, 1.6f);
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
        g.setColour (findColour (slotId));                 g.fillRoundedRectangle (slot, slotW * 0.25f);
        g.setColour (findColour (slotEdgeId));             g.drawRoundedRectangle (slot, slotW * 0.25f, 1.2f);

        juce::Rectangle<float> cap (full.getX(), sliderPos - capH * 0.5f, capW, capH);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (cap.translated (capW * 0.02f, capH * 0.07f), capW * 0.05f);
        g.setGradientFill (juce::ColourGradient (findColour (capTopId), cap.getX(), cap.getY(),
                                                 findColour (capBottomId), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, capW * 0.05f);
        g.setColour (findColour (capEdgeId));              g.drawRoundedRectangle (cap, capW * 0.05f, 1.2f);
        g.setColour (findColour (capLineId));
        g.fillRect (juce::Rectangle<float> (cap.getX() + capW * 0.07f, sliderPos - capH * 0.07f, capW * 0.86f,
                                            juce::jmax (2.f, capH * 0.07f)));
    }
    else
    {
        const float capH = full.getHeight(), capW = capH * 1.4f;
        g.setColour (findColour (inkId).withAlpha (0.55f));
        g.fillRect (juce::Rectangle<float> ((float) x, full.getCentreY() - 1.f, (float) w, 2.f));
        juce::Rectangle<float> cap (sliderPos - capW * 0.5f, full.getY() + capH * 0.12f, capW, capH * 0.76f);
        g.setGradientFill (juce::ColourGradient (findColour (capTopId), cap.getX(), cap.getY(),
                                                 findColour (capBottomId), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.f);
        g.setColour (findColour (capEdgeId));             g.drawRoundedRectangle (cap, 3.f, 1.f);
        g.setColour (findColour (capLineId));
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
    const auto ink = findColour (PanelLookAndFeel::inkId);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 115.f));
    g.setColour (ink);
    g.setFont (PanelLookAndFeel::panelFont (28.f));
    g.drawText ("OFF", juce::Rectangle<float> (0.f, 18.f, 60.f, 30.f), juce::Justification::centredRight);
    g.drawText ("ON",  juce::Rectangle<float> (0.f, 65.f, 60.f, 30.f), juce::Justification::centredRight);

    juce::Rectangle<float> body (68.f, 17.f, 34.f, 78.f);
    g.setColour (findColour (PanelLookAndFeel::slotEdgeId)); g.fillRoundedRectangle (body.expanded (3.f), 3.f);
    g.setColour (findColour (PanelLookAndFeel::slotId));     g.fillRoundedRectangle (body, 2.f);
    juce::Rectangle<float> lever (body.getX() + 2.f, on ? body.getBottom() - 36.f : body.getY() + 2.f, body.getWidth() - 4.f, 34.f);
    g.setGradientFill (juce::ColourGradient (findColour (PanelLookAndFeel::capTopId), lever.getX(), lever.getY(),
                                             findColour (PanelLookAndFeel::capBottomId), lever.getX(), lever.getBottom(), false));
    g.fillRoundedRectangle (lever, 3.f);
    g.setColour (findColour (PanelLookAndFeel::capLineId).withAlpha (0.9f));
    g.fillRect (lever.reduced (4.f, 0.f).withHeight (3.f).withY (lever.getCentreY() - 1.5f));
}

void PowerSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
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
    g.setColour (findColour (PanelLookAndFeel::inkId));
    g.setFont (PanelLookAndFeel::panelFont (34.f));
    g.drawText ("PASS", juce::Rectangle<float> (40.f, 0.f, 130.f, 34.f), juce::Justification::centredLeft, false);
    g.drawText ("MODE", juce::Rectangle<float> (40.f, 30.f, 130.f, 34.f), juce::Justification::centredLeft, false);

    const char* names[4] = { "LOW", "BAND1", "BAND2", "HIGH" };
    g.setFont (PanelLookAndFeel::panelFont (25.f));
    for (int i = 0; i < 4; ++i)
        g.drawText (names[i], juce::Rectangle<float> (0.f, kModeRowY[i] - 13.f, 82.f, 26.f), juce::Justification::centredLeft);

    juce::Rectangle<float> body (86.f, 70.f, 24.f, 114.f);
    g.setColour (findColour (PanelLookAndFeel::slotEdgeId)); g.fillRoundedRectangle (body.expanded (2.f), 3.f);
    g.setColour (findColour (PanelLookAndFeel::slotId));     g.fillRoundedRectangle (body, 2.f);
    juce::Rectangle<float> lever (body.getX() + 1.5f, kModeRowY[index] - 11.f, body.getWidth() - 3.f, 22.f);
    g.setGradientFill (juce::ColourGradient (findColour (PanelLookAndFeel::capTopId), lever.getX(), lever.getY(),
                                             findColour (PanelLookAndFeel::capBottomId), lever.getX(), lever.getBottom(), false));
    g.fillRoundedRectangle (lever, 3.f);
    g.setColour (findColour (PanelLookAndFeel::capLineId).withAlpha (0.9f));
    g.fillRect (lever.reduced (3.f, 0.f).withHeight (2.5f).withY (lever.getCentreY() - 1.f));
}

void PassModeSwitch::select (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
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
    skinIndex = juce::jlimit (0, (int) getSkins().size() - 1, proc.getSkin());
    laf.applySkin (getSkins()[(size_t) skinIndex]);

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
    outputSlider.setPopupMenuEnabled (false);
    addAndMakeVisible (outputSlider);
    outputAtt = std::make_unique<SliderAtt> (proc.apvts, ids::output, outputSlider);

    // presets
    addAndMakeVisible (presetBox);  addAndMakeVisible (prevBtn);  addAndMakeVisible (nextBtn);
    addAndMakeVisible (saveBtn);    addAndMakeVisible (loadBtn);
    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0) loadEntry (id - 1); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    saveBtn.onClick = [this] { savePreset(); };
    loadBtn.onClick = [this] { loadPresetFile(); };
    refreshPresetList();
    applySkin (skinIndex);              // also starts the timer at the right rate

    addMouseListener (this, true);       // right-click anywhere -> skin menu

    const int w = 1100;
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setResizeLimits (770, (int) (770 * kH / kW), 1920, 977);
    setSize (w, juce::roundToInt (w * kH / kW));
}

ResonatorEditor::~ResonatorEditor()
{
    stopTimer();
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

// ---- skins --------------------------------------------------------------------------------
void ResonatorEditor::applySkin (int index)
{
    skinIndex = juce::jlimit (0, (int) getSkins().size() - 1, index);
    laf.applySkin (getSkins()[(size_t) skinIndex]);
    startTimerHz (getSkins()[(size_t) skinIndex].holographic ? 24 : 3);      // animate only the hologram
    sendLookAndFeelChange();
    repaint();
}

void ResonatorEditor::mouseMove (const juce::MouseEvent& e)
{
    if (! getSkins()[(size_t) skinIndex].holographic) return;
    const auto p = e.getEventRelativeTo (this).position;
    tiltTarget = { p.x / (float) juce::jmax (1, getWidth()) - 0.5f, p.y / (float) juce::jmax (1, getHeight()) - 0.5f };
}

// Iridescent foil: a repeating pastel rainbow gradient whose position drifts slowly and
// shifts with the mouse (as if tilting a hologram), plus a few moving specular bands.
void ResonatorEditor::paintHologram (juce::Graphics& g)
{
    const juce::Colour cyc[6] = { juce::Colour (0xffffb3de), juce::Colour (0xffb8a8ff), juce::Colour (0xffa6e6ff),
                                  juce::Colour (0xffb5ffda), juce::Colour (0xfffff0a8), juce::Colour (0xffffc4a8) };
    const float Lc = 1100.f;  const int cycles = 5;  const float span = Lc * (float) cycles;
    const float angle = 0.62f + tilt.y * 0.5f;
    const juce::Point<float> dir (std::cos (angle), std::sin (angle)), centre (kW * 0.5f, kH * 0.5f);
    const float shift = std::fmod (holoPhase + tilt.x * 0.9f + 4.f, 1.f) * Lc;
    const auto start = centre - dir * (span * 0.5f) + dir * shift, end = start + dir * span;

    juce::ColourGradient grad (cyc[0], start, cyc[0], end, false);
    const int stops = cycles * 6;
    for (int i = 1; i <= stops; ++i) grad.addColour ((double) i / (double) stops, cyc[i % 6]);
    g.setGradientFill (grad);
    g.fillRect (0.f, 0.f, kW, kH);

    // soft specular bands
    for (int k = 0; k < 3; ++k)
    {
        const float w = k == 1 ? 120.f : 280.f;
        const float cx = std::fmod (holoPhase * (1.7f + 0.4f * (float) k) + 0.37f * (float) k + tilt.x * 0.6f + 4.f, 1.f) * (kW + 800.f) - 400.f;
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (0.5f + 0.05f * (float) k, cx, kH * 0.5f));
        juce::ColourGradient band (juce::Colours::white.withAlpha (0.0f), cx - w, 0.f, juce::Colours::white.withAlpha (0.0f), cx + w, 0.f, false);
        band.addColour (0.5, juce::Colours::white.withAlpha (k == 1 ? 0.30f : 0.22f));
        g.setGradientFill (band);
        g.fillRect (cx - w, -1500.f, 2.f * w, 4000.f);
    }
}

void ResonatorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showSkinMenu();
}

void ResonatorEditor::showSkinMenu()
{
    if (menuOpen) return;
    menuOpen = true;
    juce::PopupMenu m;
    m.addSectionHeader ("Skin");
    for (size_t i = 0; i < getSkins().size(); ++i)
        m.addItem ((int) i + 1, juce::String (getSkins()[i].name), true, (int) i == skinIndex);

    juce::Component::SafePointer<ResonatorEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe] (int result)
    {
        if (safe == nullptr) return;
        safe->menuOpen = false;
        if (result > 0) { safe->applySkin (result - 1); safe->proc.setSkin (result - 1); }
    });
}

// ---- presets ------------------------------------------------------------------------------
void ResonatorEditor::refreshPresetList()
{
    entries = presets::listAll();
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    bool userHeader = false;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (! entries[i].factory && ! userHeader)
        {
            presetBox.addSeparator();
            presetBox.addSectionHeading ("User");
            userHeader = true;
        }
        presetBox.addItem (entries[i].name, (int) i + 1);
    }
    presetBox.setText (proc.getPresetName(), juce::dontSendNotification);
}

void ResonatorEditor::loadEntry (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) entries.size())) return;
    auto& e = entries[(size_t) index];
    if (e.factory)
        presets::apply (proc.apvts, e.values);
    else
    {
        juce::String name; std::vector<std::pair<juce::String, float>> v;
        if (! presets::loadFromFile (e.file, name, v)) return;
        presets::apply (proc.apvts, v);
    }
    proc.setPresetName (e.name);
}

void ResonatorEditor::stepPreset (int delta)
{
    if (entries.empty()) return;
    int cur = 0;
    for (size_t i = 0; i < entries.size(); ++i) if (entries[i].name == proc.getPresetName()) { cur = (int) i; break; }
    const int n = (int) entries.size();
    presetBox.setSelectedId (((cur + delta) % n + n) % n + 1);      // triggers onChange -> loadEntry
}

void ResonatorEditor::savePreset()
{
    auto suggested = presets::presetDirectory().getChildFile (
        juce::File::createLegalFileName (proc.getPresetName()) + ".resopreset");
    chooser = std::make_unique<juce::FileChooser> ("Save preset", suggested, "*.resopreset", true);
    juce::Component::SafePointer<ResonatorEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (safe == nullptr || f == juce::File()) return;
        f = f.withFileExtension (".resopreset");
        const auto name = f.getFileNameWithoutExtension();
        if (presets::saveToFile (safe->proc.apvts, f, name))
        {
            safe->proc.setPresetName (name);
            safe->refreshPresetList();
        }
    });
}

void ResonatorEditor::loadPresetFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load preset", presets::presetDirectory(), "*.resopreset", true);
    juce::Component::SafePointer<ResonatorEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (safe == nullptr || ! f.existsAsFile()) return;
        juce::String name; std::vector<std::pair<juce::String, float>> v;
        if (! presets::loadFromFile (f, name, v)) return;
        presets::apply (safe->proc.apvts, v);
        safe->proc.setPresetName (name);
        safe->refreshPresetList();
    });
}

void ResonatorEditor::timerCallback()
{
    const bool holo = getSkins()[(size_t) skinIndex].holographic;
    if (holo)
    {
        holoPhase = std::fmod (holoPhase + 0.0015f, 1.f);
        tilt += (tiltTarget - tilt) * 0.12f;
        repaint();
    }
    if (holo && (++tick % 8) != 0) return;

    // keep the name in sync when the host restores a project / state
    const auto n = proc.getPresetName();
    if (presetBox.getText() != n && ! presetBox.isPopupActive()) presetBox.setText (n, juce::dontSendNotification);
}

// ---- layout / painting ---------------------------------------------------------------------
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

    // header strip
    prevBtn.setBounds (R (222, 40, 44, 48));
    presetBox.setBounds (R (272, 38, 360, 52));
    nextBtn.setBounds (R (638, 40, 44, 48));
    saveBtn.setBounds (R (700, 40, 100, 48));
    loadBtn.setBounds (R (808, 40, 100, 48));
    lawBox.setBounds (R (1018, 38, 205, 52));
    qualityBox.setBounds (R (1352, 38, 228, 52));
    outputSlider.setBounds (R (1660, 40, 150, 46));
}

void ResonatorEditor::paint (juce::Graphics& g)
{
    using L = PanelLookAndFeel;
    const auto ink = findColour (L::inkId);
    const float s = (float) getWidth() / kW;
    g.fillAll (findColour (L::plateBottomId));
    g.addTransform (juce::AffineTransform::scale (s));

    // plate
    if (getSkins()[(size_t) skinIndex].holographic)
        paintHologram (g);
    else
    {
        g.setGradientFill (juce::ColourGradient (findColour (L::plateTopId), 0.f, 0.f, findColour (L::plateBottomId), 0.f, kH, false));
        g.fillRect (0.f, 0.f, kW, kH);
    }

    // frames
    g.setColour (ink);
    g.drawRect (juce::Rectangle<float> (90.f, 27.f, 1737.f, 76.f), 3.f);
    g.drawRect (juce::Rectangle<float> (85.f, 112.f, 1740.f, 840.f), 3.f);

    // header strip labels
    g.setFont (L::panelFont (30.f));
    auto lab = [&g] (const char* t, float x, float w)
    { g.drawText (t, juce::Rectangle<float> (x, 38.f, w, 52.f), juce::Justification::centredLeft); };
    lab ("PRESET", 108.f, 112.f);  lab ("LAW", 945.f, 70.f);  lab ("QUALITY", 1232.f, 120.f);  lab ("OUT", 1592.f, 66.f);

    // title
    g.setFont (L::panelFont (92.f));
    g.drawText ("RESONATOR", juce::Rectangle<float> (560.f, 140.f, 730.f, 100.f), juce::Justification::centred);

    // fader ticks
    const float tickFrac[5] = { 0.107f, 0.293f, 0.485f, 0.677f, 0.873f };
    g.setColour (ink);
    for (int i = 0; i < 11; ++i)
        for (float f : tickFrac)
            g.fillRect (juce::Rectangle<float> (kFaderX[i] - 66.f, kSlotTop + (kSlotBottom - kSlotTop) * f - 1.5f, 132.f, 3.f));

    // column headings
    g.setFont (L::panelFont (46.f));
    auto head = [&g] (const char* t, float x, float y)
    { g.drawText (t, juce::Rectangle<float> (x - 100.f, y - 28.f, 200.f, 56.f), juce::Justification::centred); };
    head ("DRY", kFaderX[0], 288); head ("RES", kFaderX[1], 288);
    const char* sub[3] = { "CF", "EMPH", "GAIN" };
    for (int b = 0; b < 3; ++b)
        for (int k = 0; k < 3; ++k) head (sub[k], kFaderX[2 + b * 3 + k], 290.f + (float) b * 5.f);

    // scale labels (left of each slot)
    g.setFont (L::panelFont (28.f));
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
    g.setFont (L::panelFont (46.f));
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
