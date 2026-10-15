// Headless test: instantiates the real processor, measures impulse responses, prints CSV-ish results.
#include "../Source/PluginProcessor.h"
#include <cstdio>

static void setP (ResonatorAudioProcessor& p, const char* id, float real)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

static std::vector<float> impulse (ResonatorAudioProcessor& p, int N, int block)
{
    std::vector<float> out ((size_t) N);
    juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m;
    for (int s = 0; s < N; s += block)
    {
        b.clear(); if (s == 0) { b.setSample (0, 0, 1.f); b.setSample (1, 0, 1.f); }
        p.processBlock (b, m);
        for (int i = 0; i < block && s + i < N; ++i) out[(size_t) (s + i)] = b.getSample (0, i);
    }
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double fs = argc > 1 ? atof (argv[1]) : 44100.0;
    const int block = 512;
    // mode, quality are choice indices; mix 1.0
    for (int quality = 0; quality < 3; ++quality)
    {
        for (int mode = 0; mode < 4; ++mode)
        {
            ResonatorAudioProcessor p;
            p.setPlayConfigDetails (2, 2, fs, block);
            setP (p, "mode", (float) mode); setP (p, "quality", (float) quality);
            setP (p, "emphasis", 1.f); setP (p, "lowFreq", 1.f); setP (p, "midFreq", 1.f); setP (p, "highFreq", 1.f);
            setP (p, "mix", 1.f); setP (p, "output", 0.f);
            p.prepareToPlay (fs, block);
            auto ir = impulse (p, 1 << 16, block);
            printf ("IR %d %d %d", quality, mode, p.getLatencySamples());
            for (float v : ir) printf (" %.7g", v);
            printf ("\n");
        }
    }
    // dry/wet alignment: mix = 0 should return the input delayed by exactly `latency`
    for (int quality = 0; quality < 3; ++quality)
    {
        ResonatorAudioProcessor p;
        p.setPlayConfigDetails (2, 2, fs, block);
        setP (p, "quality", (float) quality); setP (p, "mix", 0.f);
        p.prepareToPlay (fs, block);
        auto ir = impulse (p, 2048, block);
        int peak = 0; for (int i = 0; i < 2048; ++i) if (std::abs (ir[(size_t) i]) > std::abs (ir[(size_t) peak])) peak = i;
        printf ("DRY %d latency=%d peakAt=%d peakVal=%.4f\n", quality, p.getLatencySamples(), peak, ir[(size_t) peak]);
    }
    // stress: noise in, random automation + quality/mode switching every block, check finite & bounded
    {
        juce::Random rnd (1234);
        for (double rate : { 44100.0, 96000.0 })
        {
            ResonatorAudioProcessor p; p.setPlayConfigDetails (2, 2, rate, block);
            p.prepareToPlay (rate, block);
            juce::AudioBuffer<float> b (2, block); juce::MidiBuffer m; float peak = 0; bool bad = false;
            for (int blk = 0; blk < 4000; ++blk)
            {
                if (blk % 3 == 0)
                {
                    setP (p, "lowFreq", rnd.nextFloat()); setP (p, "midFreq", rnd.nextFloat()); setP (p, "highFreq", rnd.nextFloat());
                    setP (p, "emphasis", rnd.nextFloat()); setP (p, "mode", (float) rnd.nextInt (4));
                    setP (p, "mix", rnd.nextFloat()); setP (p, "output", rnd.nextFloat() * 36.f - 24.f);
                    setP (p, "freqLaw", (float) rnd.nextInt (2));
                }
                if (blk % 50 == 0) setP (p, "quality", (float) rnd.nextInt (3));
                for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) b.setSample (c, i, rnd.nextFloat() * 0.5f - 0.25f);
                p.processBlock (b, m);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i)
                { const float v = b.getSample (c, i); if (! std::isfinite (v)) bad = true; peak = std::max (peak, std::abs (v)); }
            }
            printf ("STRESS fs=%.0f finite=%s peak=%.2f\n", rate, bad ? "NO" : "yes", peak);
        }
    }
    return 0;
}
