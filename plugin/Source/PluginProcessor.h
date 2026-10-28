#pragma once
#include "AiBridge.h"
#include "PresetManager.h"
#include <hhmg/Analyze.h>
#include <hhmg/Tools.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

// A generated clip, pre-flattened into note on/off events so the audio thread only walks a sorted vector.
struct Clip
{
    struct Event { double ppq; bool on; int pitch; int velocity; int channel; }; // channel 0-15
    hhmg::Result result;
    std::vector<Event> events;
    double lengthPpq = 16.0;

    static std::shared_ptr<const Clip> make (const hhmg::Result& r);
};

class HipHopMidiProcessor : public juce::AudioProcessor, public juce::ChangeBroadcaster
{
public:
    HipHopMidiProcessor();
    ~HipHopMidiProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "HipHop MIDI Generator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- generation (message thread) -----------------------------------------------------------
    hhmg::Request buildRequest() const;
    void generate (bool newSeed);
    void setResult (const hhmg::Result& r);
    void applyRequestToParameters (const hhmg::Request& r);
    bool importMidiFile (const juce::File& file);
    // style recognition
    hhmg::Json analyzeClipJson() const;                   // thread-safe; analyses the loaded clip
    bool hasImportAnalysis() const { return importAnalysis.has_value(); }
    juce::String importAnalysisSummary() const;           // one human-readable line (key, BPM, style, reasons)
    void matchImportedStyle();                            // apply detected settings + its progression / rhythm
    juce::File exportTempMidi() const; // empty File if there is no clip
    std::shared_ptr<const Clip> getClip() const { return std::atomic_load (&clip); }

    // ---- AI -----------------------------------------------------------------------------------------
    void askAi (const juce::String& prompt, std::function<void (const AiBridge::Outcome&)> done);
    void cancelAi();
    bool isAiBusy() const;
    AiBridge::Settings loadAiSettings() const;
    void saveAiSettings (const AiBridge::Settings&);

    // ---- dataset ---------------------------------------------------------------------------------------
    bool loadDataset (const juce::File& patternsJson, juce::String& message);
    juce::File defaultDatasetFile() const;
    juce::String libraryInfo() const;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    juce::String lastPrompt, lastAiMessage;
    std::vector<int> refProgression;                      // set by matchImportedStyle(); cleared on preset load
    std::vector<hhmg::RhythmNote> refHookRhythm;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct AiThread;
    std::unique_ptr<AiThread> aiThread;

    std::shared_ptr<const hhmg::PatternLibrary> library;
    std::shared_ptr<const Clip> clip;
    std::optional<hhmg::Analysis> importAnalysis;

    std::atomic<double> hostBpm { 120.0 };
    double sampleRate = 44100.0;
    bool wasPlaying = false;
    double expectedPpq = -1.0;
    bool sounding[16][128] = {};
    int heldMap[128];

    void silenceAll (juce::MidiBuffer&, int samplePos);
    juce::File settingsFile() const;

    JUCE_DECLARE_WEAK_REFERENCEABLE (HipHopMidiProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HipHopMidiProcessor)
};
