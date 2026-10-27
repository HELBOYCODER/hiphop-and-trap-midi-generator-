#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Factory presets (parameter snapshots) + user presets stored as XML in
// ~/Documents/HipHopMidiGenerator/Presets.
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts);

    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;

    void loadFactory (int index);
    bool saveUser (const juce::String& name, const juce::String& extra = {});
    bool loadUser (const juce::String& name, juce::String* extraOut = nullptr);
    bool deleteUser (const juce::String& name);

    static juce::File getPresetDirectory();

private:
    struct Factory { juce::String name; std::vector<std::pair<juce::String, float>> values; };
    juce::AudioProcessorValueTreeState& state;
    std::vector<Factory> factory;

    void setReal (const juce::String& id, float realValue);
};
