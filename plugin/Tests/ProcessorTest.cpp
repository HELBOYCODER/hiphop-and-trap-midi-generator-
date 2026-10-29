// Headless check of the plugin processor: transport-locked MIDI playback, scale lock, state round trip,
// MIDI export/import and (optionally) an editor snapshot.   Usage: HipHopProcessorTest [snapshot.png]
#include "../Source/PluginEditor.h"
#include <cmath>
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

    // --- voice / audio import and the user's MIDI dataset (async: spin the message loop until the callbacks fire)
    {
        auto pump = [] (const std::function<bool()>& until, int maxMs)
        {
            for (int waited = 0; waited < maxMs && ! until(); waited += 20) MessageManager::getInstance()->runDispatchLoopUntil (20);
        };
        // a sung phrase as a WAV file
        std::vector<float> audio ((size_t) 4800, 0.0f);
        const int phrase[] = { 57, 60, 64, 62, 60, 57, 55, 57 };
        for (int p : phrase)
        {
            double f0 = 440.0 * std::pow (2.0, (p - 69) / 12.0), ph = 0;
            for (int i = 0; i < 7360; ++i) { ph += 6.283185307 * f0 / 16000.0; double t = i / 16000.0; double env = std::min (1.0, t / 0.03) * std::min (1.0, (0.46 - t) / 0.03); audio.push_back ((float) (0.3 * env * (std::sin (ph) + 0.5 * std::sin (2 * ph)))); }
            audio.resize (audio.size() + 640, 0.0f);
        }
        auto wavBytes = hhmg::writeWav16 (audio, 16000);
        auto wav = File::getSpecialLocation (File::tempDirectory).getChildFile ("hhmg_voice_test.wav");
        wav.replaceWithData (wavBytes.data(), wavBytes.size());
        bool done = false; String msg;
        HipHopMidiProcessor voiceProc;
        voiceProc.importAudioAsync (wav, [&] (String m) { msg = m; done = true; });
        pump ([&] { return done; }, 15000);
        check (done && voiceProc.hasVoice(), "audio import produced a transcription: " + msg);
        check (voiceProc.getClip() != nullptr && hhmg::countOutOfScale (voiceProc.getClip()->result) == 0, "voice clip is in scale");
        for (int mode = 0; mode < 3; ++mode)
        {
            if (auto* p = voiceProc.apvts.getParameter ("voiceMode")) p->setValueNotifyingHost (p->convertTo0to1 ((float) mode));
            auto m2 = voiceProc.makeFromVoice (true);
            check (voiceProc.getClip() != nullptr && ! voiceProc.getClip()->result.notes.empty() && hhmg::countOutOfScale (voiceProc.getClip()->result) == 0, "voice mode " + String (mode) + " " + m2);
        }
        check (voiceProc.analyzeClipJson()["ok"].boolean(), "analyze_current_clip after voice");
        auto sim = voiceProc.similarForAi (hhmg::Json::object());
        check (sim["ok"].boolean (false), "similar_to_current_clip tool");
        // the user's own MIDI folder
        auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("hhmg_midi_dir_test");
        dir.deleteRecursively(); dir.createDirectory();
        hhmg::Request rq; rq.root = 7; rq.scale = hhmg::findScale ("minor"); rq.progression = { 1, 6, 3, 7 }; rq.bars = 8; rq.melody = false;
        hhmg::writeMidiFile (hhmg::generate (rq, hhmg::PatternLibrary::builtin()), dir.getChildFile ("Loop - G Min.mid").getFullPathName().toStdString());
        auto before = voiceProc.libraryInfo();
        bool done2 = false; String msg2;
        voiceProc.addMidiFolderAsync (dir, [&] (String m) { msg2 = m; done2 = true; });
        pump ([&] { return done2; }, 15000);
        check (done2 && voiceProc.libraryInfo() != before, "folder indexed: " + msg2);
        voiceProc.userLibraryFileForTest().deleteFile();
        wav.deleteFile(); dir.deleteRecursively();
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
