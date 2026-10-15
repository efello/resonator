#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace resonator;

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

    auto freqParam = [this] (const char* id, const char* name, int band)
    {
        return std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 1.f), 1.0f,
            AudioParameterFloatAttributes()
                .withStringFromValueFunction ([this, band] (float v, int)
                    { return hzText (knobToFreq (band, v, currentLaw())); })
                .withValueFromStringFunction ([this, band] (const String& s)
                    {
                        // accept "300", "300 Hz", "1.2k", "1.2 kHz"
                        auto t = s.trim().toLowerCase();
                        double hz = t.getDoubleValue();
                        if (t.contains ("k")) hz *= 1000.0;
                        const double lo = fit::fMin[(size_t) band], hi = fit::fMax[(size_t) band];
                        hz = jlimit (lo, hi, hz);
                        // invert the mapping by bisection (works for either law)
                        double a = 0.0, b = 1.0;
                        for (int i = 0; i < 40; ++i)
                        {
                            const double m = 0.5 * (a + b);
                            (knobToFreq (band, m, currentLaw()) < hz ? a : b) = m;
                        }
                        return (float) (0.5 * (a + b));
                    }));
    };
    p.push_back (freqParam (ids::lowFreq,  "Low Freq",  0));
    p.push_back (freqParam (ids::midFreq,  "Mid Freq",  1));
    p.push_back (freqParam (ids::highFreq, "High Freq", 2));

    p.push_back (std::make_unique<P> (ParameterID { ids::emphasis, 1 }, "Emphasis",
                    NormalisableRange<float> (0.f, 1.f), 0.6f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return String (roundToInt (v * 100.f)) + " %"; })));

    p.push_back (std::make_unique<PC> (ParameterID { ids::mode, 1 }, "Mode",
                    StringArray { "LPF", "BPF 1", "BPF 2", "HPF" }, 1));
    p.push_back (std::make_unique<PC> (ParameterID { ids::freqLaw, 1 }, "Freq Law",
                    StringArray { "Analog (pot)", "Exponential" }, 0));
    p.push_back (std::make_unique<PC> (ParameterID { ids::quality, 1 }, "Quality",
                    StringArray { "Eco (1x)", "Standard (2x)", "High (4x)" }, 1));

    p.push_back (std::make_unique<P> (ParameterID { ids::mix, 1 }, "Mix",
                    NormalisableRange<float> (0.f, 1.f), 1.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return String (roundToInt (v * 100.f)) + " %"; })));
    p.push_back (std::make_unique<P> (ParameterID { ids::output, 1 }, "Output",
                    NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return String (v, 1) + " dB"; })));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

ResonatorAudioProcessor::ResonatorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    lowP     = apvts.getRawParameterValue (ids::lowFreq);
    midP     = apvts.getRawParameterValue (ids::midFreq);
    highP    = apvts.getRawParameterValue (ids::highFreq);
    emphP    = apvts.getRawParameterValue (ids::emphasis);
    modeP    = apvts.getRawParameterValue (ids::mode);
    lawParam = apvts.getRawParameterValue (ids::freqLaw);
    qualityP = apvts.getRawParameterValue (ids::quality);
    mixP     = apvts.getRawParameterValue (ids::mix);
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

    mixSmooth.reset (sampleRate, 0.02);
    outSmooth.reset (sampleRate, 0.02);
    mixSmooth.setCurrentAndTargetValue (bypassParam != nullptr && bypassParam->get() ? 0.f : mixP->load());
    outSmooth.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outP->load()));

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
    const std::array<double, 3> knobs { (double) lowP->load(), (double) midP->load(), (double) highP->load() };
    for (auto& f : filters) f.setParams (knobs, (double) emphP->load(), mode, law);

    const bool bypassed = bypassParam != nullptr && bypassParam->get();
    mixSmooth.setTargetValue (bypassed ? 0.f : mixP->load());
    outSmooth.setTargetValue (juce::Decibels::decibelsToGain (outP->load()));

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

    // ---- mix + output gain -----------------------------------------------------
    for (int n = 0; n < numSamples; ++n)
    {
        const float m = mixSmooth.getNextValue(), g = outSmooth.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* w = buffer.getWritePointer (ch);
            const float dry = dryBuffer.getReadPointer (ch)[n];
            w[n] = (dry + m * (w[n] * g - dry)) ;   // bypass/mix keep unity on the dry path
        }
    }
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
