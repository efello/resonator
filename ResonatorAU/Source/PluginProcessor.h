#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include "ResonatorDSP.h"

namespace ids
{
    // per band: 0 = LOW, 1 = MED, 2 = HIGH
    inline constexpr const char* cf[3]   = { "lowCF",   "midCF",   "highCF"   };
    inline constexpr const char* emph[3] = { "lowEmph", "midEmph", "highEmph" };
    inline constexpr const char* gain[3] = { "lowGain", "midGain", "highGain" };
    inline constexpr const char* dry     = "dry";
    inline constexpr const char* res     = "res";
    inline constexpr const char* mode    = "mode";
    inline constexpr const char* freqLaw = "freqLaw";
    inline constexpr const char* quality = "quality";
    inline constexpr const char* output  = "output";
    inline constexpr const char* bypass  = "bypass";    // true = OFF (bypassed)
}

class ResonatorAudioProcessor : public juce::AudioProcessor
{
public:
    ResonatorAudioProcessor();
    ~ResonatorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }   // high-Q ringing

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    // Used by the editor's response display
    resonator::FreqLaw currentLaw() const noexcept
    {
        return lawParam != nullptr && lawParam->load() > 0.5f ? resonator::FreqLaw::Exponential
                                                              : resonator::FreqLaw::Analog;
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyQuality (int qualityIndex);

    std::array<std::atomic<float>*, 3> cfP {}, emphP {}, gainP {};
    std::atomic<float> *dryP {}, *resP {}, *modeP {}, *lawParam {}, *qualityP {}, *outP {};
    juce::AudioParameterBool* bypassParam {};

    std::array<resonator::ResonatorChannel, 2> filters;

    // oversampling: [0] = 2x, [1] = 4x (linear-phase FIR half-band => integer latency)
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> oversamplers;
    int activeQuality = -1;                 // 0 = Eco(1x), 1 = Standard(2x), 2 = High(4x)
    int osFactor = 1;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };
    juce::AudioBuffer<float> dryBuffer;

    juce::SmoothedValue<float> drySmooth, resSmooth, outSmooth, bypassSmooth;
    double hostRate = 44100.0;
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorAudioProcessor)
};
