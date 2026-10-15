#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace resonator;

// default band GAIN = -6 dB (also the double-click reset value)
static constexpr float kGainDefault = 0.50118723f;

static juce::String hzText (double hz)
{
    return hz >= 1000.0 ? juce::String (hz / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
}

juce::AudioProcessorValueTreeState::ParameterLayout ResonatorAudioProcessor::createLayout()
{
    using namespace juce;
    using P  = AudioParameterFloat;
    using PC = AudioParameterChoice;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    const char* bandName[3] = { "Low", "Mid", "High" };

    auto freqParam = [this] (const char* id, const String& name, int band)
    {
        return std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 1.f), 0.5f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([this, band] (float v, int)
                    { return hzText (knobToFreq (band, v, currentLaw())); })
                .withValueFromStringFunction ([this, band] (const String& s)
                    {
                        // accept "300", "300 Hz", "1.2k", "1.2 kHz"
                        auto t = s.trim().toLowerCase();
                        double hz = t.getDoubleValue();
                        if (t.contains ("k")) hz *= 1000.0;
                        hz = jlimit (fit::fMin[(size_t) band], fit::fMax[(size_t) band], hz);
                        double lo = 0.0, hi = 1.0;                       // invert mapping by bisection
                        for (int i = 0; i < 40; ++i)
                        {
                            const double m = 0.5 * (lo + hi);
                            (knobToFreq (band, m, currentLaw()) < hz ? lo : hi) = m;
                        }
                        return (float) (0.5 * (lo + hi));
                    }));
    };
    auto levelText = [] (float v, int) { return v <= 0.0001f ? String ("-inf dB") : String (Decibels::gainToDecibels (v), 1) + " dB"; };

    for (int b = 0; b < 3; ++b)
    {
        p.push_back (freqParam (ids::cf[b], String (bandName[b]) + " CF", b));

        p.push_back (std::make_unique<P> (ParameterID { ids::emph[b], 1 }, String (bandName[b]) + " Emphasis",
                        NormalisableRange<float> (0.f, 1.f), 0.5f,
                        AudioParameterFloatAttributes().withStringFromValueFunction (
                            [b] (float v, int) { return "Q " + String (emphasisToQ (b, v), 2); })));

        p.push_back (std::make_unique<P> (ParameterID { ids::gain[b], 1 }, String (bandName[b]) + " Gain",
                        NormalisableRange<float> (0.f, 1.f), kGainDefault,       // -6 dB
                        AudioParameterFloatAttributes().withStringFromValueFunction (levelText)));
    }

    p.push_back (std::make_unique<P> (ParameterID { ids::dry, 1 }, "Dry", NormalisableRange<float> (0.f, 1.f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (levelText)));
    p.push_back (std::make_unique<P> (ParameterID { ids::res, 1 }, "Res", NormalisableRange<float> (0.f, 1.f), 1.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (levelText)));

    p.push_back (std::make_unique<PC> (ParameterID { ids::mode, 1 }, "Pass Mode",
                    StringArray { "Low", "Band 1", "Band 2", "High" }, 1));
    p.push_back (std::make_unique<PC> (ParameterID { ids::freqLaw, 1 }, "Freq Law",
                    StringArray { "Analog (pot)", "Exponential" }, 0));
    p.push_back (std::make_unique<PC> (ParameterID { ids::quality, 1 }, "Quality",
                    StringArray { "Eco (1x)", "Standard (2x)", "High (4x)" }, 1));
    p.push_back (std::make_unique<P> (ParameterID { ids::output, 1 }, "Output",
                    NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass (OFF)", false));

    return { p.begin(), p.end() };
}

ResonatorAudioProcessor::ResonatorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (size_t i = 0; i < 3; ++i)
    {
        cfP[i]   = apvts.getRawParameterValue (ids::cf[i]);
        emphP[i] = apvts.getRawParameterValue (ids::emph[i]);
        gainP[i] = apvts.getRawParameterValue (ids::gain[i]);
    }
    dryP     = apvts.getRawParameterValue (ids::dry);
    resP     = apvts.getRawParameterValue (ids::res);
    modeP    = apvts.getRawParameterValue (ids::mode);
    lawParam = apvts.getRawParameterValue (ids::freqLaw);
    qualityP = apvts.getRawParameterValue (ids::quality);
    outP     = apvts.getRawParameterValue (ids::output);
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ids::bypass));
}

bool ResonatorAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& in = l.getMainInputChannelSet();
    const auto& out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void ResonatorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;
    maxBlock = samplesPerBlock;
    const auto numCh = (size_t) juce::jmax (1, getTotalNumInputChannels());

    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>> (
            numCh, (size_t) (i + 1),
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
        oversamplers[i]->initProcessing ((size_t) samplesPerBlock);
    }

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };
    dryDelay.prepare (spec);
    dryBuffer.setSize ((int) numCh, samplesPerBlock);

    for (auto* s : { &drySmooth, &resSmooth, &outSmooth, &bypassSmooth }) s->reset (sampleRate, 0.02);
    drySmooth.setCurrentAndTargetValue (dryP->load());
    resSmooth.setCurrentAndTargetValue (resP->load());
    outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outP->load()));
    bypassSmooth.setCurrentAndTargetValue (bypassParam != nullptr && bypassParam->get() ? 1.f : 0.f);

    activeQuality = -1;                           // force (re)configuration
    applyQuality ((int) qualityP->load());
}

void ResonatorAudioProcessor::applyQuality (int q)
{
    q = juce::jlimit (0, 2, q);
    activeQuality = q;
    osFactor = q == 0 ? 1 : (q == 1 ? 2 : 4);

    for (auto& f : filters) { f.prepare (hostRate * osFactor); }

    int latency = 0;
    if (q > 0)
    {
        oversamplers[(size_t) (q - 1)]->reset();
        latency = juce::roundToInt (oversamplers[(size_t) (q - 1)]->getLatencyInSamples());
    }
    dryDelay.reset();
    dryDelay.setDelay ((float) latency);
    setLatencySamples (latency);
}

void ResonatorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples > maxBlock)          // host exceeded promised block size: process in chunks
    {
        for (int start = 0; start < numSamples; start += maxBlock)
        {
            const int n = juce::jmin (maxBlock, numSamples - start);
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, n);
            juce::MidiBuffer dummy;
            processBlock (sub, dummy);
        }
        return;
    }

    const int q = (int) qualityP->load();
    if (q != activeQuality) applyQuality (q);

    // ---- parameters -------------------------------------------------------------
    const auto mode = (Mode) juce::jlimit (0, 3, (int) modeP->load());
    const auto law  = currentLaw();
    std::array<double, 3> cf, em, ga;
    for (size_t i = 0; i < 3; ++i) { cf[i] = cfP[i]->load(); em[i] = emphP[i]->load(); ga[i] = gainP[i]->load(); }
    for (auto& f : filters) f.setParams (cf, em, ga, mode, law);

    drySmooth.setTargetValue (dryP->load());
    resSmooth.setTargetValue (resP->load());
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (outP->load()));
    bypassSmooth.setTargetValue (bypassParam != nullptr && bypassParam->get() ? 1.f : 0.f);

    // ---- keep a latency-aligned dry copy ---------------------------------------
    for (int ch = 0; ch < numCh; ++ch)
    {
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        auto* d = dryBuffer.getWritePointer (ch);
        for (int n = 0; n < numSamples; ++n)
        {
            dryDelay.pushSample (ch, d[n]);
            d[n] = dryDelay.popSample (ch);
        }
    }

    // ---- wet path (oversampled) ------------------------------------------------
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) numSamples);
    if (osFactor == 1)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            for (int n = 0; n < numSamples; ++n) d[n] = filters[(size_t) ch].process (d[n]);
        }
    }
    else
    {
        auto& os = *oversamplers[(size_t) (activeQuality - 1)];
        auto up = os.processSamplesUp (block);
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* d = up.getChannelPointer (ch);
            for (size_t n = 0; n < up.getNumSamples(); ++n) d[n] = filters[ch].process (d[n]);
        }
        os.processSamplesDown (block);
    }

    // ---- DRY / RES faders, output trim, OFF (= clean bypass of the filter) -------
    for (int n = 0; n < numSamples; ++n)
    {
        const float dg = drySmooth.getNextValue(), rg = resSmooth.getNextValue() * outSmooth.getNextValue();
        const float byp = bypassSmooth.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            const float dry = dryBuffer.getReadPointer (ch)[n];
            const float on = dry * dg + w[n] * rg;
            w[n] = on + byp * (dry - on);
        }
    }
}

int ResonatorAudioProcessor::getSkin() const
{
    return apvts.state.hasProperty ("skin") ? (int) apvts.state.getProperty ("skin") : presets::readGlobalSkin();
}

void ResonatorAudioProcessor::setSkin (int index)
{
    apvts.state.setProperty ("skin", index, nullptr);
    presets::writeGlobalSkin (index);
}

juce::AudioProcessorEditor* ResonatorAudioProcessor::createEditor() { return new ResonatorEditor (*this); }

void ResonatorAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void ResonatorAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ResonatorAudioProcessor(); }
