// Headless test: instantiates the real processor, measures impulse responses, prints CSV-ish results.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/Presets.h"
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
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        ResonatorAudioProcessor p;
        // pose similar to the hardware photo
        const std::pair<const char*, float> pose[] = { { "dry", 0.f }, { "res", 0.f }, { "lowCF", 0.9f }, { "lowEmph", 0.55f },
            { "lowGain", 0.5f }, { "midCF", 0.28f }, { "midEmph", 0.45f }, { "midGain", 0.8f }, { "highCF", 0.85f },
            { "highEmph", 0.65f }, { "highGain", 0.4f }, { "mode", 1.f } };
        for (auto& kv : pose) setP (p, kv.first, kv.second);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (1280, 651);
        if (argc > 3) dynamic_cast<ResonatorEditor*> (ed.get())->applySkin (atoi (argv[3]));
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (argv[2]); f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--presets")
    {
        int fails = 0;
        auto check = [&fails] (bool ok, const char* what) { printf ("%s: %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++fails; };
        ResonatorAudioProcessor p;
        auto getReal = [&p] (const char* id) { auto* r = p.apvts.getParameter (id); return r->convertFrom0to1 (r->getValue()); };
        // 1. defaults: band gains are -6 dB, double-click reset value == default
        for (auto* id : { "lowGain", "midGain", "highGain" })
        {
            const float db = juce::Decibels::gainToDecibels (getReal (id));
            check (std::abs (db + 6.f) < 0.01f, "band gain default is -6 dB");
        }
        check (getReal ("dry") == 0.f && getReal ("res") == 1.f, "dry/res defaults unchanged");
        // 2. gain reset: move away then back to default
        { auto* r = p.apvts.getParameter ("midGain"); r->setValueNotifyingHost (1.f);
          r->setValueNotifyingHost (r->getDefaultValue());
          check (std::abs (juce::Decibels::gainToDecibels (getReal ("midGain")) + 6.f) < 0.01f, "reset to default gives -6 dB"); }
        // 3. factory preset applies
        auto fac = presets::factory();
        check (fac.size() >= 5, "factory presets exist");
        presets::apply (p.apvts, fac[3].values);                       // Measured MAX - Band 2
        check (getReal ("mode") == 2.f && getReal ("lowCF") == 1.f && getReal ("highGain") == 1.f, "factory preset values applied");
        // 4. save -> change -> load round trip
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("rt_test.resopreset");
        check (presets::saveToFile (p.apvts, file, "RT Test"), "preset file written");
        p.apvts.getParameter ("lowCF")->setValueNotifyingHost (0.1f);
        p.apvts.getParameter ("mode")->setValueNotifyingHost (0.f);
        juce::String nm; std::vector<std::pair<juce::String, float>> v;
        check (presets::loadFromFile (file, nm, v) && nm == "RT Test", "preset file parsed");
        presets::apply (p.apvts, v);
        check (std::abs (getReal ("lowCF") - 1.f) < 1e-5f && getReal ("mode") == 2.f, "round trip restores values");
        // 5. quality / bypass are not part of presets
        p.apvts.getParameter ("quality")->setValueNotifyingHost (1.f);
        presets::apply (p.apvts, fac[0].values);
        check (getReal ("quality") == 2.f, "quality untouched by presets");
        // 6. state round trip keeps preset name + skin
        p.setPresetName ("My Sound"); p.setSkin (3);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        ResonatorAudioProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (q.getPresetName() == "My Sound" && q.getSkin() == 3, "preset name + skin saved in state");
        printf ("%s\n", fails == 0 ? "ALL PRESET TESTS PASSED" : "SOME TESTS FAILED");
        return fails;
    }
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
            for (auto* id : { "lowEmph", "midEmph", "highEmph", "lowCF", "midCF", "highCF", "lowGain", "midGain", "highGain" }) setP (p, id, 1.f);
            setP (p, "dry", 0.f); setP (p, "res", 1.f); setP (p, "output", 0.f);
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
        setP (p, "quality", (float) quality); setP (p, "dry", 1.f); setP (p, "res", 0.f);
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
                    for (auto* id : { "lowCF", "midCF", "highCF", "lowEmph", "midEmph", "highEmph", "lowGain", "midGain", "highGain", "dry", "res" })
                        setP (p, id, rnd.nextFloat());
                    setP (p, "mode", (float) rnd.nextInt (4)); setP (p, "bypass", (float) rnd.nextInt (2));
                    setP (p, "output", rnd.nextFloat() * 36.f - 24.f);
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
