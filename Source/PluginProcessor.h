#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include "ResonatorDSP.h"

namespace ids
{
    inline constexpr const char* lowFreq  = "lowFreq";
    inline constexpr const char* midFreq  = "midFreq";
    inline constexpr const char* highFreq = "highFreq";
    inline constexpr const char* emphasis = "emphasis";
    inline constexpr const char* mode     = "mode";
    inline constexpr const char* freqLaw  = "freqLaw";
    inline constexpr const char* quality  = "quality";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* output   = "output";
    inline constexpr const char* bypass   = "bypass";
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

    std::atomic<float> *lowP {}, *midP {}, *highP {}, *emphP {}, *modeP {}, *lawParam {},
                       *qualityP {}, *mixP {}, *outP {};
    juce::AudioParameterBool* bypassParam {};

    std::array<resonator::ResonatorChannel, 2> filters;

    // oversampling: [0] = 2x, [1] = 4x (linear-phase FIR half-band => integer latency)
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> oversamplers;
    int activeQuality = -1;                 // 0 = Eco(1x), 1 = Standard(2x), 2 = High(4x)
    int osFactor = 1;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };
    juce::AudioBuffer<float> dryBuffer;

    juce::SmoothedValue<float> mixSmooth, outSmooth;
    double hostRate = 44100.0;
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorAudioProcessor)
};
