#include "PresetManager.h"
#include <hhmg/Theory.h>

namespace
{
int scaleIndex (const char* id) { return hhmg::findScale (id); }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
{
    auto add = [this] (const char* name, std::vector<std::pair<juce::String, float>> v) { factory.push_back ({ name, std::move (v) }); };

    add ("Dark Trap - Minor", { { "scale", (float) scaleIndex ("minor") }, { "bpm", 140 }, { "rnb", 0.1f }, { "trap", 1.0f }, { "hiphop", 0.2f },
                                { "complexity", 0.35f }, { "humanize", 0.15f }, { "swing", 0.0f }, { "bass", 1 }, { "bars", 4 } });
    add ("Smooth R&B - 9ths", { { "scale", (float) scaleIndex ("dorian") }, { "bpm", 78 }, { "rnb", 1.0f }, { "trap", 0.0f }, { "hiphop", 0.1f },
                                { "complexity", 0.85f }, { "humanize", 0.5f }, { "swing", 0.25f }, { "bass", 0 }, { "bars", 8 } });
    add ("Boom Bap - Dusty", { { "scale", (float) scaleIndex ("pentatonic_minor") }, { "bpm", 90 }, { "rnb", 0.1f }, { "trap", 0.0f }, { "hiphop", 1.0f },
                               { "complexity", 0.4f }, { "humanize", 0.6f }, { "swing", 0.5f }, { "bass", 1 }, { "bars", 4 } });
    add ("Trap Soul - Phrygian", { { "scale", (float) scaleIndex ("phrygian") }, { "bpm", 132 }, { "rnb", 0.6f }, { "trap", 0.7f }, { "hiphop", 0.0f },
                                   { "complexity", 0.6f }, { "humanize", 0.35f }, { "swing", 0.1f }, { "bass", 1 }, { "bars", 8 } });
    add ("Neo Soul - Major 7ths", { { "scale", (float) scaleIndex ("major") }, { "bpm", 86 }, { "rnb", 1.0f }, { "trap", 0.0f }, { "hiphop", 0.3f },
                                    { "complexity", 1.0f }, { "humanize", 0.7f }, { "swing", 0.35f }, { "bass", 0 }, { "bars", 4 } });
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray n;
    for (auto& f : factory) n.add (f.name);
    return n;
}

void PresetManager::setReal (const juce::String& id, float value)
{
    if (auto* p = state.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void PresetManager::loadFactory (int index)
{
    if (index < 0 || index >= (int) factory.size()) return;
    for (auto& kv : factory[(size_t) index].values) setReal (kv.first, kv.second);
}

juce::File PresetManager::getPresetDirectory()
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("HipHopMidiGenerator").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray n;
    for (auto& f : getPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.hhpreset"))
        n.add (f.getFileNameWithoutExtension());
    n.sort (true);
    return n;
}

static juce::String safeName (const juce::String& s)
{
    return juce::File::createLegalFileName (s.trim()).replaceCharacters ("/\\", "__");
}

bool PresetManager::saveUser (const juce::String& name, const juce::String& extra)
{
    auto clean = safeName (name);
    if (clean.isEmpty()) return false;
    auto xml = state.copyState().createXml();
    if (xml == nullptr) return false;
    xml->setAttribute ("extra", extra);
    return xml->writeTo (getPresetDirectory().getChildFile (clean + ".hhpreset"));
}

bool PresetManager::loadUser (const juce::String& name, juce::String* extraOut)
{
    auto xml = juce::XmlDocument::parse (getPresetDirectory().getChildFile (safeName (name) + ".hhpreset"));
    if (xml == nullptr) return false;
    if (extraOut != nullptr) *extraOut = xml->getStringAttribute ("extra");
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid() || tree.getType() != state.state.getType()) return false;
    state.replaceState (tree);
    return true;
}

bool PresetManager::deleteUser (const juce::String& name)
{
    return getPresetDirectory().getChildFile (safeName (name) + ".hhpreset").deleteFile();
}
