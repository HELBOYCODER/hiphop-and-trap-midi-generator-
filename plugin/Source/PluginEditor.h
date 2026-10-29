#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>

class PianoRoll : public juce::Component
{
public:
    explicit PianoRoll (HipHopMidiProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    HipHopMidiProcessor& proc;
};

class DragHandle : public juce::Component
{
public:
    explicit DragHandle (HipHopMidiProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    HipHopMidiProcessor& proc;
    bool dragging = false;
};

class HipHopMidiEditor : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    explicit HipHopMidiEditor (HipHopMidiProcessor&);
    ~HipHopMidiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    struct Toggle
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void addKnob (Knob&, const juce::String& id, const juce::String& text);
    void addToggle (Toggle&, const juce::String& id, const juce::String& text);
    void refreshPresetList();
    void refreshAiFields();
    void persistAiFields();
    void setStatus (const juce::String& s, bool error = false);

    HipHopMidiProcessor& proc;

    juce::ComboBox presetBox, rootBox, scaleBox, providerBox;
    juce::TextButton savePreset { "Save" }, deletePreset { "Delete" }, generateButton { "GENERATE" }, dice { "New seed" },
        loadDataset { "Load dataset..." }, askButton { "Ask AI" }, matchButton { "Match dropped style" },
        importAudioButton { "Import audio..." }, recordButton { "Record input" }, makeVoiceButton { "Make from voice" }, addFolderButton { "Add my MIDI folder..." };
    juce::ComboBox voiceModeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceModeAtt;
    static bool isAudioFile (const juce::String& f);
    juce::Slider bars { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 bpm { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 seed { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft },
                 octave { juce::Slider::IncDecButtons, juce::Slider::TextBoxLeft };
    juce::Label barsL, bpmL, seedL, octaveL, rootL, scaleL, statusLabel, infoLabel, modelL, endpointL, keyL;
    Knob rnb, trap, hiphop, complexity, humanize, swing, velVar;
    Toggle syncBpm, chords, melody, bass, scaleLock, playback;
    juce::TextEditor prompt, model, endpoint, apiKey;
    PianoRoll roll;
    DragHandle drag;
    std::unique_ptr<juce::FileChooser> chooser;

    using Choice = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using Slide = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Choice> rootAtt, scaleAtt;
    std::unique_ptr<Slide> barsAtt, bpmAtt, seedAtt, octaveAtt;

    JUCE_DECLARE_WEAK_REFERENCEABLE (HipHopMidiEditor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HipHopMidiEditor)
};
