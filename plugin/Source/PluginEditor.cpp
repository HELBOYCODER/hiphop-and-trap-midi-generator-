#include "PluginEditor.h"

using namespace juce;

namespace
{
const Colour bg (0xff101114), panel (0xff1a1c21), accent (0xffffb020), accent2 (0xff39c5bb), textDim (0xff9aa0aa);

Colour trackColour (int ch) { return ch == hhmg::kChords ? Colour (0xff5b8def) : ch == hhmg::kMelody ? accent : Colour (0xffe0526b); }
}

// ---- piano roll ------------------------------------------------------------------------------------------------
void PianoRoll::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (b, 6.0f);
    auto clip = proc.getClip();
    if (clip == nullptr || clip->result.notes.empty())
    {
        g.setColour (textDim);
        g.drawText ("Press GENERATE, drop a .mid file, or ask the AI", getLocalBounds(), Justification::centred);
        return;
    }
    const auto& r = clip->result;
    int lo = 127, hi = 0;
    for (auto& n : r.notes) { lo = jmin (lo, n.pitch); hi = jmax (hi, n.pitch); }
    lo -= 2; hi += 2;
    const float total = (float) (r.request.bars * hhmg::kTicksPerBar);
    auto area = b.reduced (6.0f);
    const float rowH = area.getHeight() / (float) (hi - lo + 1);

    for (int bar = 0; bar < r.request.bars; ++bar)
    {
        float x = area.getX() + area.getWidth() * (float) (bar * hhmg::kTicksPerBar) / total;
        g.setColour (Colours::white.withAlpha (bar % 4 == 0 ? 0.18f : 0.07f));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }
    // rows that belong to the scale are tinted, so "no wrong notes" is visible
    for (int p = lo; p <= hi; ++p)
    {
        if (! r.scale.contains (p)) continue;
        float y = area.getBottom() - (float) (p - lo + 1) * rowH;
        g.setColour (Colours::white.withAlpha (r.scale.chordContains (p) ? 0.05f : 0.025f));
        g.fillRect (area.getX(), y, area.getWidth(), rowH);
    }
    for (auto& n : r.notes)
    {
        float x = area.getX() + area.getWidth() * (float) n.start / total;
        float w = jmax (2.0f, area.getWidth() * (float) n.length / total - 1.0f);
        float y = area.getBottom() - (float) (n.pitch - lo + 1) * rowH;
        g.setColour (trackColour (n.channel).withAlpha (0.35f + 0.65f * (float) n.velocity / 127.0f));
        g.fillRoundedRectangle (x, y + 0.5f, w, jmax (2.0f, rowH - 1.0f), 2.0f);
    }
}

// ---- drag handle ------------------------------------------------------------------------------------------------------
void DragHandle::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (accent2.withAlpha (dragging ? 0.5f : 0.22f));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (accent2);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.2f);
    g.setColour (Colours::white);
    g.setFont (FontOptions (14.0f, Font::bold));
    g.drawText ("DRAG MIDI TO DAW", getLocalBounds(), Justification::centred);
}

void DragHandle::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    auto f = proc.exportTempMidi();
    if (! f.existsAsFile()) return;
    dragging = true;
    repaint();
    DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
        [safe = Component::SafePointer<DragHandle> (this)] { if (safe != nullptr) { safe->dragging = false; safe->repaint(); } });
}

// ---- editor ---------------------------------------------------------------------------------------------------------------
HipHopMidiEditor::HipHopMidiEditor (HipHopMidiProcessor& p)
    : AudioProcessorEditor (&p), proc (p), roll (p), drag (p)
{
    setSize (940, 700);
    setResizable (false, false);

    auto label = [this] (Label& l, const String& t, Justification j = Justification::centredLeft)
    {
        l.setText (t, dontSendNotification); l.setJustificationType (j); l.setColour (Label::textColourId, textDim);
        addAndMakeVisible (l);
    };

    for (auto& s : hhmg::allScales()) scaleBox.addItem (s.label, scaleBox.getNumItems() + 1);
    for (auto* n : { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }) rootBox.addItem (n, rootBox.getNumItems() + 1);
    addAndMakeVisible (rootBox); addAndMakeVisible (scaleBox);
    rootAtt = std::make_unique<Choice> (proc.apvts, "root", rootBox);
    scaleAtt = std::make_unique<Choice> (proc.apvts, "scale", scaleBox);
    label (rootL, "KEY"); label (scaleL, "SCALE / MODE");

    auto intSlider = [this, &label] (Slider& s, Label& l, const String& t, const String& id, std::unique_ptr<Slide>& att)
    {
        s.setTextBoxStyle (Slider::TextBoxLeft, false, 52, 24);
        addAndMakeVisible (s); label (l, t);
        att = std::make_unique<Slide> (proc.apvts, id, s);
    };
    intSlider (bars, barsL, "BARS", "bars", barsAtt);
    intSlider (bpm, bpmL, "BPM", "bpm", bpmAtt);
    intSlider (seed, seedL, "SEED", "seed", seedAtt);
    intSlider (octave, octaveL, "MELODY OCT", "octave", octaveAtt);

    addKnob (rnb, "rnb", "R&B"); addKnob (trap, "trap", "TRAP"); addKnob (hiphop, "hiphop", "HIP-HOP");
    addKnob (complexity, "complexity", "COMPLEXITY"); addKnob (humanize, "humanize", "HUMANIZE");
    addKnob (swing, "swing", "SWING"); addKnob (velVar, "velVar", "VEL VAR");

    addToggle (syncBpm, "syncBpm", "Host BPM"); addToggle (chords, "chords", "Chords"); addToggle (melody, "melody", "Melody");
    addToggle (bass, "bass", "808 Bass"); addToggle (scaleLock, "scaleLock", "Scale Lock"); addToggle (playback, "playback", "Play with host");

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        int id = presetBox.getSelectedId();
        if (id <= 0) return;
        int nFactory = proc.presets.getFactoryNames().size();
        if (id <= nFactory) proc.presets.loadFactory (id - 1);
        else
        {
            String extra;
            if (proc.presets.loadUser (presetBox.getText(), &extra) && extra.isNotEmpty()) prompt.setText (extra, dontSendNotification);
        }
        proc.refProgression.clear();
        proc.refHookRhythm.clear();
        proc.generate (false);
    };
    addAndMakeVisible (presetBox);
    savePreset.onClick = [this]
    {
        auto* w = new AlertWindow ("Save preset", "Name:", MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", presetBox.getText());
        w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r == 1 && proc.presets.saveUser (w->getTextEditorContents ("name"), prompt.getText())) refreshPresetList();
            delete w;
        }), false);
    };
    deletePreset.onClick = [this]
    {
        if (presetBox.getSelectedId() > proc.presets.getFactoryNames().size()) { proc.presets.deleteUser (presetBox.getText()); refreshPresetList(); }
    };
    addAndMakeVisible (savePreset); addAndMakeVisible (deletePreset);

    generateButton.setColour (TextButton::buttonColourId, accent);
    generateButton.setColour (TextButton::textColourOffId, Colours::black);
    generateButton.onClick = [this] { proc.generate (true); };
    addAndMakeVisible (generateButton);
    dice.onClick = [this] { proc.generate (true); };
    loadDataset.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Pattern library (patterns.json from tools/dataset/ingest.py)",
                                                 proc.defaultDatasetFile().getParentDirectory(), "*.json");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
        {
            auto f = fc.getResult();
            if (! f.existsAsFile()) return;
            String msg;
            bool ok = proc.loadDataset (f, msg);
            setStatus (msg, ! ok);
            infoLabel.setText ("Library: " + proc.libraryInfo(), dontSendNotification);
            if (ok) proc.generate (false);
        });
    };
    addAndMakeVisible (loadDataset);
    matchButton.setEnabled (proc.hasImportAnalysis());
    matchButton.setTooltip ("Apply the key, tempo, style mix, swing, chord progression and rhythm detected in the MIDI you dropped, then generate in that style");
    matchButton.onClick = [this] { proc.matchImportedStyle(); setStatus ("Generating in the detected style (progression + rhythm taken from your file).", false); };
    addAndMakeVisible (matchButton);
    addAndMakeVisible (roll); addAndMakeVisible (drag);

    // AI panel
    providerBox.addItem ("Gemini", 1);
    providerBox.addItem ("OpenAI-compatible (Ollama, LM Studio, ...)", 2);
    providerBox.onChange = [this] { persistAiFields(); };
    addAndMakeVisible (providerBox);
    prompt.setMultiLine (true); prompt.setReturnKeyStartsNewLine (true);
    prompt.setTextToShowWhenEmpty ("Describe it: \"dark trap, F# phrygian, 142 BPM, eerie bell melody, sliding 808\"", textDim);
    prompt.setText (proc.lastPrompt, dontSendNotification);
    addAndMakeVisible (prompt);
    model.setTextToShowWhenEmpty ("model", textDim); endpoint.setTextToShowWhenEmpty ("http://localhost:11434/v1", textDim);
    apiKey.setPasswordCharacter ('*'); apiKey.setTextToShowWhenEmpty ("API key", textDim);
    for (auto* t : { &model, &endpoint, &apiKey }) { t->onFocusLost = [this] { persistAiFields(); }; addAndMakeVisible (t); }
    label (modelL, "MODEL"); label (endpointL, "ENDPOINT (OpenAI-compatible)"); label (keyL, "API KEY (stored outside DAW projects)");
    askButton.setColour (TextButton::buttonColourId, accent2);
    askButton.setColour (TextButton::textColourOffId, Colours::black);
    askButton.onClick = [this]
    {
        persistAiFields();
        askButton.setEnabled (false);
        setStatus ("Asking the model...");
        WeakReference<HipHopMidiEditor> safe (this); // a local, not an init-capture: MSVC reads `this` there as the enclosing lambda
        proc.askAi (prompt.getText(), [safe] (const AiBridge::Outcome& o)
        {
            if (safe == nullptr) return;
            safe->askButton.setEnabled (true);
            safe->setStatus (o.message.isEmpty() ? (o.ok ? "Done." : "Failed.") : o.message, ! o.ok);
        });
    };
    addAndMakeVisible (askButton);
    statusLabel.setJustificationType (Justification::topLeft);
    statusLabel.setColour (Label::textColourId, textDim);
    addAndMakeVisible (statusLabel);
    infoLabel.setColour (Label::textColourId, textDim);
    infoLabel.setText ("Library: " + proc.libraryInfo(), dontSendNotification);
    addAndMakeVisible (infoLabel);
    (void) dice;

    refreshPresetList();
    refreshAiFields();
    proc.addChangeListener (this);
    startTimerHz (4);
}

HipHopMidiEditor::~HipHopMidiEditor() { proc.removeChangeListener (this); }

void HipHopMidiEditor::addKnob (Knob& k, const String& id, const String& text)
{
    k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 56, 18);
    k.slider.setColour (Slider::rotarySliderFillColourId, accent);
    k.slider.setNumDecimalPlacesToDisplay (2);
    k.label.setText (text, dontSendNotification);
    k.label.setJustificationType (Justification::centred);
    k.label.setColour (Label::textColourId, textDim);
    addAndMakeVisible (k.slider); addAndMakeVisible (k.label);
    k.attach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void HipHopMidiEditor::addToggle (Toggle& t, const String& id, const String& text)
{
    t.button.setButtonText (text);
    addAndMakeVisible (t.button);
    t.attach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t.button);
}

void HipHopMidiEditor::refreshPresetList()
{
    presetBox.clear (dontSendNotification);
    int id = 1;
    for (auto& n : proc.presets.getFactoryNames()) presetBox.addItem (n, id++);
    presetBox.addSeparator();
    for (auto& n : proc.presets.getUserNames()) presetBox.addItem (n, id++);
}

void HipHopMidiEditor::refreshAiFields()
{
    auto s = proc.loadAiSettings();
    providerBox.setSelectedId ((int) s.provider + 1, dontSendNotification);
    model.setText (s.model, false);
    endpoint.setText (s.endpoint, false);
    apiKey.setText (s.apiKey, false);
}

void HipHopMidiEditor::persistAiFields()
{
    AiBridge::Settings s;
    s.provider = (AiBridge::Provider) jmax (0, providerBox.getSelectedId() - 1);
    s.model = model.getText();
    s.endpoint = endpoint.getText();
    s.apiKey = apiKey.getText();
    proc.saveAiSettings (s);
}

void HipHopMidiEditor::setStatus (const String& s, bool error)
{
    statusLabel.setColour (Label::textColourId, error ? Colour (0xffff6b6b) : textDim);
    statusLabel.setText (s, dontSendNotification);
}

void HipHopMidiEditor::changeListenerCallback (ChangeBroadcaster*)
{
    roll.repaint();
    if (auto c = proc.getClip())
        infoLabel.setText (String (c->result.description) + "   |   " + proc.libraryInfo(), dontSendNotification);
}

void HipHopMidiEditor::timerCallback() { bpm.setEnabled (! proc.apvts.getRawParameterValue ("syncBpm")->load()); }

bool HipHopMidiEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi")) return true;
    return false;
}

void HipHopMidiEditor::filesDropped (const StringArray& files, int, int)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".mid") || f.endsWithIgnoreCase (".midi"))
        {
            const bool ok = proc.importMidiFile (File (f));
            matchButton.setEnabled (proc.hasImportAnalysis());
            if (! ok) { setStatus ("Could not read " + File (f).getFileName(), true); return; }
            setStatus ("Imported " + File (f).getFileName() + (proc.apvts.getRawParameterValue ("scaleLock")->load() > 0.5f ? " (snapped to scale)" : "")
                       + "\n" + proc.importAnalysisSummary() + "\nPress \"Match dropped style\" to generate in this style, or ask the AI to combine it.");
            return;
        }
}

void HipHopMidiEditor::paint (Graphics& g)
{
    g.fillAll (bg);
    g.setColour (accent);
    g.setFont (FontOptions (22.0f, Font::bold));
    g.drawText ("HIPHOP MIDI GENERATOR", 20, 12, 380, 30, Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (FontOptions (12.0f));
    g.drawText ("R&B  /  Trap  /  Hip-Hop   -   every note stays in key", 20, 40, 380, 16, Justification::centredLeft);
    g.setColour (panel);
    g.fillRoundedRectangle (Rectangle<float> (14, 448, 912, 238), 8.0f);
}

void HipHopMidiEditor::resized()
{
    auto area = getLocalBounds().reduced (14);
    auto top = area.removeFromTop (46);
    deletePreset.setBounds (top.removeFromRight (70).reduced (0, 8));
    savePreset.setBounds (top.removeFromRight (70).reduced (4, 8));
    presetBox.setBounds (top.removeFromRight (240).reduced (4, 8));

    area.removeFromTop (6);
    auto row1 = area.removeFromTop (54);
    auto cell = [&row1] (int w) { auto r = row1.removeFromLeft (w); row1.removeFromLeft (10); return r; };
    auto place = [] (Label& l, Component& c, Rectangle<int> r) { l.setBounds (r.removeFromTop (16)); c.setBounds (r.removeFromTop (28)); };
    place (rootL, rootBox, cell (90));
    place (scaleL, scaleBox, cell (230));
    place (barsL, bars, cell (120));
    place (bpmL, bpm, cell (120));
    place (octaveL, octave, cell (130));
    place (seedL, seed, cell (150));

    area.removeFromTop (6);
    auto knobs = area.removeFromTop (118);
    int kw = knobs.getWidth() / 7;
    for (auto* k : { &rnb, &trap, &hiphop, &complexity, &humanize, &swing, &velVar })
    {
        auto r = knobs.removeFromLeft (kw);
        k->label.setBounds (r.removeFromTop (16));
        k->slider.setBounds (r);
    }

    auto toggles = area.removeFromTop (28);
    int tw = toggles.getWidth() / 6;
    for (auto* t : { &syncBpm, &chords, &melody, &bass, &scaleLock, &playback }) t->button.setBounds (toggles.removeFromLeft (tw));

    area.removeFromTop (6);
    auto actions = area.removeFromTop (36);
    generateButton.setBounds (actions.removeFromLeft (170));
    actions.removeFromLeft (10);
    drag.setBounds (actions.removeFromLeft (200));
    actions.removeFromLeft (10);
    loadDataset.setBounds (actions.removeFromLeft (130));
    actions.removeFromLeft (8);
    matchButton.setBounds (actions.removeFromLeft (150));
    infoLabel.setBounds (actions.reduced (8, 0));

    area.removeFromTop (6);
    roll.setBounds (area.removeFromTop (96));

    // AI panel (drawn on panel at y=448)
    auto ai = Rectangle<int> (14, 448, 912, 238).reduced (12, 10);
    auto head = ai.removeFromTop (44);
    providerBox.setBounds (head.removeFromLeft (300).withTrimmedTop (16));
    head.removeFromLeft (10);
    auto mcol = head.removeFromLeft (170); modelL.setBounds (mcol.removeFromTop (16)); model.setBounds (mcol);
    head.removeFromLeft (10);
    auto ecol = head.removeFromLeft (190); endpointL.setBounds (ecol.removeFromTop (16)); endpoint.setBounds (ecol);
    head.removeFromLeft (10);
    keyL.setBounds (head.removeFromTop (16)); apiKey.setBounds (head);
    ai.removeFromTop (8);
    auto bottom = ai;
    askButton.setBounds (bottom.removeFromRight (120).withHeight (40));
    bottom.removeFromRight (10);
    prompt.setBounds (bottom.removeFromTop (bottom.getHeight() - 56));
    statusLabel.setBounds (bottom.reduced (2, 4));
}
