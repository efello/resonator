// Presets.h – preset save/load (XML files) + factory presets + tiny global settings (skin).
#pragma once
#include "PluginProcessor.h"

namespace presets
{
    struct Entry
    {
        juce::String name;
        bool factory = true;
        std::vector<std::pair<juce::String, float>> values;   // real (denormalised) parameter values
        juce::File file;                                       // user presets only
    };

    inline juce::File rootDirectoryFor (const juce::String& brand)
    {
       #if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                   .getChildFile ("Library/Audio/Presets/" + brand + "/Resonator");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile (brand + "/Resonator");
       #endif
    }

    inline juce::File rootDirectory() { return rootDirectoryFor ("Flo"); }

    inline juce::File presetDirectory()
    {
        auto d = rootDirectory().getChildFile ("Presets");
        if (! d.isDirectory())
        {
            d.createDirectory();
            // one-time migration of presets saved under the former brand name
            auto old = rootDirectoryFor ("ResonatorDIY").getChildFile ("Presets");
            if (old.isDirectory())
            {
                juce::Array<juce::File> files;
                old.findChildFiles (files, juce::File::findFiles, false, "*.resopreset");
                for (auto& f : files) f.copyFileTo (d.getChildFile (f.getFileName()));
            }
        }
        return d;
    }

    // parameters that are NOT part of a preset (system / host related)
    inline bool isExcluded (const juce::String& id) { return id == ids::quality || id == ids::bypass; }

    inline std::vector<std::pair<juce::String, float>> capture (juce::AudioProcessorValueTreeState& apvts)
    {
        std::vector<std::pair<juce::String, float>> v;
        for (auto* p : apvts.processor.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! isExcluded (r->paramID))
                    v.emplace_back (r->paramID, r->convertFrom0to1 (r->getValue()));
        return v;
    }

    inline void setReal (juce::RangedAudioParameter& p, float real)
    {
        p.beginChangeGesture();
        p.setValueNotifyingHost (p.convertTo0to1 (real));
        p.endChangeGesture();
    }

    // Reset every preset parameter to its default, then apply the given values.
    inline void apply (juce::AudioProcessorValueTreeState& apvts,
                       const std::vector<std::pair<juce::String, float>>& values)
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! isExcluded (r->paramID))
                    setReal (*r, r->convertFrom0to1 (r->getDefaultValue()));

        for (auto& kv : values)
            if (auto* r = apvts.getParameter (kv.first))
                if (! isExcluded (kv.first))
                    setReal (*r, kv.second);
    }

    inline bool saveToFile (juce::AudioProcessorValueTreeState& apvts, const juce::File& file, const juce::String& name)
    {
        juce::XmlElement xml ("ResonatorPreset");
        xml.setAttribute ("version", 1);
        xml.setAttribute ("name", name);
        for (auto& kv : capture (apvts))
        {
            auto* e = xml.createNewChildElement ("P");
            e->setAttribute ("id", kv.first);
            e->setAttribute ("v", (double) kv.second);
        }
        file.getParentDirectory().createDirectory();
        return file.replaceWithText (xml.toString());
    }

    inline bool loadFromFile (const juce::File& file, juce::String& nameOut,
                              std::vector<std::pair<juce::String, float>>& valuesOut)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("ResonatorPreset")) return false;
        nameOut = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
        valuesOut.clear();
        for (auto* e : xml->getChildWithTagNameIterator ("P"))
            valuesOut.emplace_back (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("v"));
        return true;
    }

    // ---- factory presets -----------------------------------------------------------------
    inline std::vector<Entry> factory()
    {
        using V = std::vector<std::pair<juce::String, float>>;
        auto make = [] (const char* name, V v) { Entry e; e.name = name; e.values = std::move (v); return e; };
        auto all = [] (float cf, float em, float ga, float mode)
        {
            V v;
            for (int b = 0; b < 3; ++b) { v.emplace_back (ids::cf[b], cf); v.emplace_back (ids::emph[b], em); v.emplace_back (ids::gain[b], ga); }
            v.emplace_back (ids::mode, mode);
            v.emplace_back (ids::dry, 0.f);
            v.emplace_back (ids::res, 1.f);
            return v;
        };
        std::vector<Entry> out;
        out.push_back (make ("Init", {}));
        // These reproduce the measured captures (all CF / EMPH / GAIN at maximum)
        out.push_back (make ("Measured MAX - Low",    all (1.f, 1.f, 1.f, 0.f)));
        out.push_back (make ("Measured MAX - Band 1", all (1.f, 1.f, 1.f, 1.f)));
        out.push_back (make ("Measured MAX - Band 2", all (1.f, 1.f, 1.f, 2.f)));
        out.push_back (make ("Measured MAX - High",   all (1.f, 1.f, 1.f, 3.f)));
        {
            auto v = all (0.6f, 0.35f, 0.7f, 1.f);  v.emplace_back (ids::cf[1], 0.5f);  v.emplace_back (ids::cf[2], 0.55f);
            out.push_back (make ("Soft Formant", v));
        }
        {
            auto v = all (0.5f, 0.75f, 0.6f, 2.f);  v.emplace_back (ids::cf[0], 0.45f);
            out.push_back (make ("Vowel Notches", v));
        }
        out.push_back (make ("Dark Lowpass",  all (0.3f, 0.5f, 0.7f, 0.f)));
        out.push_back (make ("Air Highpass",  all (0.8f, 0.6f, 0.6f, 3.f)));
        return out;
    }

    inline std::vector<Entry> listAll()
    {
        auto list = factory();
        juce::Array<juce::File> files;
        presetDirectory().findChildFiles (files, juce::File::findFiles, false, "*.resopreset");
        files.sort();
        for (auto& f : files)
        {
            Entry e; e.factory = false; e.file = f;
            e.name = f.getFileNameWithoutExtension();
            list.push_back (std::move (e));
        }
        return list;
    }

    // ---- global (all instances) settings: last chosen skin -------------------------------
    inline juce::File settingsFile() { return rootDirectory().getChildFile ("skin.txt"); }
    inline int readGlobalSkin()
    {
        auto f = settingsFile();
        return f.existsAsFile() ? f.loadFileAsString().trim().getIntValue() : 0;
    }
    inline void writeGlobalSkin (int i)
    {
        auto f = settingsFile();
        f.getParentDirectory().createDirectory();
        f.replaceWithText (juce::String (i));
    }
}
