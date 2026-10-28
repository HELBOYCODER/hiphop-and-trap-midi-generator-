// Headless check of the plugin processor: transport-locked MIDI playback, scale lock, state round trip,
// MIDI export/import and (optionally) an editor snapshot.   Usage: HipHopProcessorTest [snapshot.png]
#include "../Source/PluginEditor.h"
#include <cstdio>

using namespace juce;

namespace
{
int failures = 0;
void check (bool ok, const String& what)
{
    if (! ok) { ++failures; std::printf ("FAIL: %s\n", what.toRawUTF8()); }
}

struct FakeHost : AudioPlayHead
{
    double ppq = 0, bpm = 120;
    bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing); p.setPpqPosition (ppq); p.setBpm (bpm);
        return p;
    }
};
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    HipHopMidiProcessor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    const double sr = 48000;
    const int block = 512;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    // --- a 4-bar clip loops sample-accurately and never leaves a note hanging
    proc.generate (true);
    auto clip = proc.getClip();
    check (clip != nullptr && ! clip->events.empty(), "clip generated");
    const hhmg::Scale scale = clip->result.scale;

    int ons = 0, offs = 0, outOfScale = 0;
    std::set<std::pair<int, int>> held;
    AudioBuffer<float> audio (2, block);
    const double beatsPerBlock = block / (sr * 60.0 / host.bpm);
    const int loops = 3;
    const int blocks = (int) std::ceil (clip->lengthPpq * loops / beatsPerBlock);
    for (int b = 0; b < blocks; ++b)
    {
        MidiBuffer m;
        host.ppq = b * beatsPerBlock;
        proc.processBlock (audio, m);
        for (const auto meta : m)
        {
            auto msg = meta.getMessage();
            if (msg.isNoteOn())
            {
                ++ons; held.insert ({ msg.getChannel(), msg.getNoteNumber() });
                bool ok = msg.getChannel() == 2 ? scale.contains (msg.getNoteNumber()) : scale.chordContains (msg.getNoteNumber());
                if (! ok) ++outOfScale;
            }
            else if (msg.isNoteOff()) { ++offs; held.erase ({ msg.getChannel(), msg.getNoteNumber() }); }
        }
    }
    int expectedOns = 0;
    for (auto& e : clip->events) if (e.on) ++expectedOns;
    std::printf ("playback: %d note-ons over %d loops (clip has %d), %d offs, %d hanging, %d out of scale\n", ons, loops, expectedOns, offs, (int) held.size(), outOfScale);
    check (std::abs (ons - expectedOns * loops) <= expectedOns / 20 + 1, "note-on count follows the loop");
    check (outOfScale == 0, "no out-of-scale notes reach the output");
    // stop transport -> everything released
    host.playing = false;
    { MidiBuffer m; proc.processBlock (audio, m); for (const auto meta : m) if (meta.getMessage().isNoteOff()) held.erase ({ meta.getMessage().getChannel(), meta.getMessage().getNoteNumber() }); }
    check (held.empty(), "transport stop releases notes");

    // --- scale lock on live input
    host.playing = false;
    for (const char* id : { "root", "scale" })
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (id[0] == 'r' ? 0.0f : (float) hhmg::findScale ("major")));
    {
        MidiBuffer in, out;
        in.addEvent (MidiMessage::noteOn (1, 61, (uint8) 100), 0);   // C# is not in C major
        in.addEvent (MidiMessage::noteOff (1, 61), 10);
        proc.processBlock (audio, in);
        int pitchOn = -1, pitchOff = -1;
        for (const auto meta : in) { auto msg = meta.getMessage(); if (msg.isNoteOn()) pitchOn = msg.getNoteNumber(); if (msg.isNoteOff()) pitchOff = msg.getNoteNumber(); }
        check (pitchOn == 60 && pitchOff == 60, "scale lock snaps C# -> C and the matching note-off");
    }

    // --- state round trip
    proc.generate (true);
    auto before = proc.getClip();
    MemoryBlock state;
    proc.getStateInformation (state);
    HipHopMidiProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    auto after = other.getClip();
    check (after != nullptr && after->result.notes.size() == before->result.notes.size(), "state restores the clip");
    check (other.apvts.getRawParameterValue ("seed")->load() == proc.apvts.getRawParameterValue ("seed")->load(), "state restores parameters");

    // --- export -> import
    auto f = proc.exportTempMidi();
    check (f.existsAsFile(), "midi export");
    check (other.importMidiFile (f), "midi import");
    auto imp = other.getClip();
    check (imp != nullptr && hhmg::countOutOfScale (imp->result) == 0, "imported notes are in scale");

    // --- style recognition on import + "match style"
    check (other.hasImportAnalysis(), "import produced an analysis");
    check (other.importAnalysisSummary().contains ("looks like"), "analysis summary: " + other.importAnalysisSummary());
    {
        auto j = other.analyzeClipJson();
        check (j["ok"].boolean() && j["detected_style"].isString(), "analyze_current_clip json");
        other.matchImportedStyle();
        check (other.getClip() != nullptr && hhmg::countOutOfScale (other.getClip()->result) == 0, "match style generates in scale");
        check (! other.refProgression.empty(), "match style carries the progression");
    }

    // --- factory presets
    for (int i = 0; i < proc.presets.getFactoryNames().size(); ++i) { proc.presets.loadFactory (i); proc.generate (true); check (hhmg::countOutOfScale (proc.getClip()->result) == 0, "preset " + String (i)); }

    // --- snapshot of the editor
    if (argc > 1)
    {
        proc.presets.loadFactory (0);
        proc.generate (false);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        File out (argv[1]);
        out.deleteFile();
        FileOutputStream os (out);
        PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", argv[1]);
    }

    if (failures) { std::printf ("%d failure(s)\n", failures); return 1; }
    std::printf ("processor tests passed\n");
    return 0;
}
