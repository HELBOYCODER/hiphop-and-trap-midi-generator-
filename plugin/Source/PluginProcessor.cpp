#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include <hhmg/MidiFile.h>
#include <cmath>
#include <map>

using namespace juce;

// ---------------------------------------------------------------------------------------------------
std::shared_ptr<const Clip> Clip::make (const hhmg::Result& r)
{
    auto c = std::make_shared<Clip>();
    c->result = r;
    c->lengthPpq = r.request.bars * 4.0;
    for (const auto& n : r.notes)
    {
        double on = n.start / hhmg::kPPQ;
        double off = (n.start + n.length) / hhmg::kPPQ;
        if (on >= c->lengthPpq - 0.02) continue;                    // would start after the loop point
        off = std::min (off, c->lengthPpq - 0.01);                  // never hang across the loop
        off = std::max (off, on + 0.01);
        c->events.push_back ({ on, true, n.pitch, n.velocity, n.channel });
        c->events.push_back ({ off, false, n.pitch, 0, n.channel });
    }
    std::stable_sort (c->events.begin(), c->events.end(), [] (const Event& a, const Event& b)
                      { return a.ppq != b.ppq ? a.ppq < b.ppq : (int) a.on < (int) b.on; }); // offs first
    return c;
}

// ---------------------------------------------------------------------------------------------------
static StringArray noteNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

AudioProcessorValueTreeState::ParameterLayout HipHopMidiProcessor::createLayout()
{
    using P = AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    StringArray scales;
    for (auto& s : hhmg::allScales()) scales.add (s.label);

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "root", 1 }, "Key", noteNames(), 9));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale / Mode", scales, hhmg::findScale ("minor")));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "bars", 1 }, "Bars", 1, 32, 4));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "bpm", 1 }, "BPM", 40, 220, 140));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "syncBpm", 1 }, "Use Host BPM", false));
    auto pct = [] { return NormalisableRange<float> (0.0f, 1.0f, 0.001f); };
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rnb", 1 }, "Style R&B", pct(), 0.15f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "trap", 1 }, "Style Trap", pct(), 1.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "hiphop", 1 }, "Style Hip-Hop", pct(), 0.3f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "complexity", 1 }, "Complexity", pct(), 0.5f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize", pct(), 0.3f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", pct(), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "velVar", 1 }, "Velocity Variation", pct(), 0.3f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "scaleLock", 1 }, "Scale Lock", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "chords", 1 }, "Chords", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "melody", 1 }, "Melody", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bass", 1 }, "808 Bass", true));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Melody Octave", 3, 7, 5));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "seed", 1 }, "Seed", 0, 99999, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "playback", 1 }, "Playback", true));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "voiceMode", 1 }, "Voice Mode",
                                                         StringArray { "My melody", "Melody + chords/808", "Similar idea" }, 2));
    return { p.begin(), p.end() };
}

// ---------------------------------------------------------------------------------------------------
HipHopMidiProcessor::HipHopMidiProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), false).withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      presets (apvts)
{
    for (int& h : heldMap) h = -1;
    auto lib = std::make_shared<hhmg::PatternLibrary> (hhmg::PatternLibrary::builtin());
    try { lib->mergeJson (hhmg::Json::parse (std::string (BinaryData::patterns_pop909_json, (size_t) BinaryData::patterns_pop909_jsonSize))); }
    catch (const std::exception&) {}
    try { lib->mergeJson (hhmg::Json::parse (std::string (BinaryData::patterns_cymatics_json, (size_t) BinaryData::patterns_cymatics_jsonSize))); }
    catch (const std::exception&) {}
    try { lib->mergeJson (hhmg::Json::parse (std::string (BinaryData::typebeat_styles_json, (size_t) BinaryData::typebeat_styles_jsonSize))); }
    catch (const std::exception&) {}
    auto def = defaultDatasetFile();
    if (def.existsAsFile())
    {
        try { lib->mergeJson (hhmg::Json::parse (def.loadFileAsString().toStdString())); }
        catch (const std::exception&) {}
    }
    try   // a bank you built yourself with tools/typebeats/harvest.py
    {
        auto tf = defaultDatasetFile().getSiblingFile ("typebeat_styles.json");
        if (tf.existsAsFile()) lib->mergeJson (hhmg::Json::parse (tf.loadFileAsString().toStdString()));
    }
    catch (const std::exception&) {}
    try
    {
        auto uf = userLibraryFile();
        if (uf.existsAsFile())
        {
            auto j = hhmg::Json::parse (uf.loadFileAsString().toStdString());
            lib->mergeJson (j);
            userLibrary->mergeJson (j);
        }
    }
    catch (const std::exception&) {}
    library = lib;
    generate (false);
}

HipHopMidiProcessor::~HipHopMidiProcessor() { cancelAi(); }

bool HipHopMidiProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto in = l.getMainInputChannelSet();
    const bool inOk = in.isDisabled() || in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
    return inOk && (l.getMainOutputChannelSet() == AudioChannelSet::stereo() || l.getMainOutputChannelSet().isDisabled());
}

void HipHopMidiProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    recRate = sr;
    recBuffer.assign ((size_t) (sr * 90.0), 0.0f);          // preallocated: the audio thread never allocates
    recCount.store (0);
    wasPlaying = false;
    expectedPpq = -1.0;
    std::memset (sounding, 0, sizeof sounding);
    for (int& h : heldMap) h = -1;
}

// ---- audio thread --------------------------------------------------------------------------------------
void HipHopMidiProcessor::silenceAll (MidiBuffer& out, int pos)
{
    for (int ch = 0; ch < 16; ++ch)
        for (int n = 0; n < 128; ++n)
            if (sounding[ch][n]) { out.addEvent (MidiMessage::noteOff (ch + 1, n), pos); sounding[ch][n] = false; }
}

void HipHopMidiProcessor::processBlock (AudioBuffer<float>& audio, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = audio.getNumSamples();
    if (recordingInput.load() && getTotalNumInputChannels() > 0 && ! recBuffer.empty())
    {
        const int c = recCount.load();
        const int take = jmin (numSamples, (int) recBuffer.size() - c);
        const int chans = jmin (2, audio.getNumChannels());
        for (int i = 0; i < take; ++i)
        {
            float v = 0.0f;
            for (int ch = 0; ch < chans; ++ch) v += audio.getSample (ch, i);
            recBuffer[(size_t) (c + i)] = v / (float) jmax (1, chans);
        }
        recCount.store (c + take);
    }
    audio.clear();

    // 1. live MIDI input -> optional scale lock (note-offs follow the note-on's snapped pitch)
    MidiBuffer out;
    const bool lock = *apvts.getRawParameterValue ("scaleLock") > 0.5f;
    const hhmg::Scale scale { (int) *apvts.getRawParameterValue ("root"), (int) *apvts.getRawParameterValue ("scale") };
    for (const auto meta : midi)
    {
        auto m = meta.getMessage();
        if (lock && m.isNoteOnOrOff())
        {
            int n = m.getNoteNumber();
            if (m.isNoteOn()) { int q = scale.snap (n); heldMap[n] = q; m.setNoteNumber (q); }
            else { int q = heldMap[n] >= 0 ? heldMap[n] : scale.snap (n); heldMap[n] = -1; m.setNoteNumber (q); }
        }
        out.addEvent (m, meta.samplePosition);
    }

    // 2. clip playback locked to the host transport
    bool playing = false;
    double p0 = 0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) hostBpm.store (*b);
            if (auto q = pos->getPpqPosition()) p0 = *q; else playing = false;
        }

    auto c = std::atomic_load (&clip);
    const bool enabled = *apvts.getRawParameterValue ("playback") > 0.5f;
    if (! playing || ! enabled || c == nullptr || c->events.empty())
    {
        if (wasPlaying) silenceAll (out, 0);
        wasPlaying = false;
        expectedPpq = -1.0;
    }
    else
    {
        const double bpm = std::max (1.0, hostBpm.load());
        const double spb = sampleRate * 60.0 / bpm;           // samples per beat
        const double span = numSamples / spb;

        if (! wasPlaying || std::fabs (p0 - expectedPpq) > 0.02) silenceAll (out, 0); // start or transport jump
        wasPlaying = true;

        double p = p0, remaining = span;
        const double L = c->lengthPpq;
        while (remaining > 1e-9)
        {
            double loc = std::fmod (p, L);
            if (loc < 0) loc += L;
            double seg = std::min (remaining, L - loc);
            auto first = std::lower_bound (c->events.begin(), c->events.end(), loc,
                                           [] (const Clip::Event& e, double v) { return e.ppq < v; });
            for (auto it = first; it != c->events.end() && it->ppq < loc + seg; ++it)
            {
                int offset = (int) std::lround ((p - p0 + (it->ppq - loc)) * spb);
                offset = jlimit (0, std::max (0, numSamples - 1), offset);
                int ch = jlimit (0, 15, it->channel);
                if (it->on) { out.addEvent (MidiMessage::noteOn (ch + 1, it->pitch, (uint8) it->velocity), offset); sounding[ch][it->pitch] = true; }
                else if (sounding[ch][it->pitch]) { out.addEvent (MidiMessage::noteOff (ch + 1, it->pitch), offset); sounding[ch][it->pitch] = false; }
            }
            p += seg;
            remaining -= seg;
        }
        expectedPpq = p0 + span;
    }
    midi.swapWith (out);
}

// ---- generation -------------------------------------------------------------------------------------------
hhmg::Request HipHopMidiProcessor::buildRequest() const
{
    auto f = [this] (const char* id) -> float { return apvts.getRawParameterValue (id)->load(); };
    hhmg::Request r;
    r.root = (int) f ("root");
    r.scale = (int) f ("scale");
    r.bars = (int) f ("bars");
    r.bpm = f ("syncBpm") > 0.5f ? (int) std::lround (hostBpm.load()) : (int) f ("bpm");
    r.mix = { f ("rnb"), f ("trap"), f ("hiphop") };
    r.complexity = f ("complexity");
    r.humanize = f ("humanize");
    r.swing = f ("swing");
    r.velocityVariation = f ("velVar");
    r.chords = f ("chords") > 0.5f;
    r.melody = f ("melody") > 0.5f;
    r.bass = f ("bass") > 0.5f;
    r.melodyOctave = (int) f ("octave");
    r.seed = (uint32_t) f ("seed");
    r.progression = refProgression;
    r.hookRhythm = refHookRhythm;
    return r;
}

void HipHopMidiProcessor::generate (bool newSeed)
{
    if (newSeed)
        if (auto* p = apvts.getParameter ("seed"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) Random::getSystemRandom().nextInt (100000)));
    auto lib = std::atomic_load (&library);
    setResult (hhmg::generate (buildRequest(), *lib));
}

void HipHopMidiProcessor::setResult (const hhmg::Result& r)
{
    std::atomic_store (&clip, Clip::make (r));
    sendChangeMessage();
}

void HipHopMidiProcessor::applyRequestToParameters (const hhmg::Request& r)
{
    refProgression.clear();
    refHookRhythm.clear();
    auto setReal = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    setReal ("root", (float) r.root);
    setReal ("scale", (float) r.scale);
    setReal ("bars", (float) r.bars);
    setReal ("bpm", (float) r.bpm);
    setReal ("syncBpm", 0.0f);
    setReal ("rnb", (float) r.mix.rnb);
    setReal ("trap", (float) r.mix.trap);
    setReal ("hiphop", (float) r.mix.hiphop);
    setReal ("complexity", (float) r.complexity);
    setReal ("humanize", (float) r.humanize);
    setReal ("swing", (float) r.swing);
    setReal ("velVar", (float) r.velocityVariation);
    setReal ("chords", r.chords ? 1.0f : 0.0f);
    setReal ("melody", r.melody ? 1.0f : 0.0f);
    setReal ("bass", r.bass ? 1.0f : 0.0f);
    setReal ("octave", (float) r.melodyOctave);
    setReal ("seed", (float) (r.seed % 100000));
}

bool HipHopMidiProcessor::importMidiFile (const File& file)
{
    FileInputStream in (file);
    MidiFile mf;
    if (! in.openedOk() || ! mf.readFrom (in)) return false;
    const int tf = mf.getTimeFormat();
    if (tf <= 0) return false; // SMPTE not supported

    hhmg::Result r;
    r.request = buildRequest();
    r.scale = hhmg::Scale { r.request.root, r.request.scale };
    const bool lock = *apvts.getRawParameterValue ("scaleLock") > 0.5f;
    double maxEnd = 0;
    const double k = (double) hhmg::kPPQ / tf;

    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        MidiMessageSequence seq (*mf.getTrack (t));
        seq.updateMatchedPairs();
        std::vector<hhmg::Note> notes;
        std::map<long long, int> onsets;
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* e = seq.getEventPointer (i);
            if (! e->message.isNoteOn() || e->message.getChannel() == 10) continue;
            double off = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : e->message.getTimeStamp() + tf / 4;
            hhmg::Note n;
            n.pitch = e->message.getNoteNumber();
            n.start = e->message.getTimeStamp() * k;
            n.length = std::max (20.0, (off - e->message.getTimeStamp()) * k);
            n.velocity = e->message.getVelocity();
            notes.push_back (n);
            onsets[(long long) (n.start / (hhmg::kPPQ / 8))]++;
        }
        if (notes.empty()) continue;
        const bool chordTrack = (double) notes.size() / (double) onsets.size() >= 2.3;
        for (auto& n : notes)
        {
            n.channel = chordTrack ? hhmg::kChords : hhmg::kMelody;
            if (lock) n.pitch = chordTrack ? r.scale.snapChord (n.pitch) : r.scale.snap (n.pitch);
            maxEnd = std::max (maxEnd, n.start + n.length);
            r.notes.push_back (n);
        }
    }
    if (r.notes.empty()) return false;
    r.request.bars = jlimit (1, 32, (int) std::ceil (maxEnd / hhmg::kTicksPerBar - 0.001));
    r.description = "Imported: " + file.getFileName().toStdString();
    if (auto* p = apvts.getParameter ("bars")) p->setValueNotifyingHost (p->convertTo0to1 ((float) r.request.bars));
    {   // recognise the style of what was dropped (on the original notes, before any scale snapping changed them)
        hhmg::MidiData md;
        std::string err;
        if (hhmg::readMidiFile (file.getFullPathName().toStdString(), md, err))
            importAnalysis = hhmg::analyzeMidi (md, *std::atomic_load (&library));
    }
    setResult (r);
    return true;
}

hhmg::Json HipHopMidiProcessor::analyzeClipJson() const
{
    auto c = getClip();
    if (c == nullptr || c->result.notes.empty())
    {
        hhmg::Json e = hhmg::Json::object(); e.set ("ok", false); e.set ("error", "no clip loaded"); return e;
    }
    std::vector<hhmg::Note> chords, melody;
    for (auto& n : c->result.notes)
    {
        if (n.channel == hhmg::kChords) chords.push_back (n);
        else if (n.channel == hhmg::kMelody) melody.push_back (n);
    }
    auto lib = std::atomic_load (&library);
    hhmg::Json j = hhmg::analysisToJson (hhmg::analyzeNotes (chords, melody, c->result.request.bpm, *lib));
    j.set ("ok", true);
    return j;
}

String HipHopMidiProcessor::importAnalysisSummary() const
{
    if (! importAnalysis) return {};
    const auto& a = *importAnalysis;
    static const char* names[] = { "R&B", "Trap", "Hip-Hop" };
    int top = a.topStyle == "rnb" ? 0 : a.topStyle == "trap" ? 1 : 2;
    String s = String (hhmg::noteName (a.root)) + " " + hhmg::allScales()[(size_t) a.scale].label + ", " + (a.bpmKnown ? String (a.bpm, 0) + " BPM" : String ("no tempo in file")) + "  -  looks like "
               + names[top] + " (R&B " + String ((int) std::lround (a.style.rnb * 100)) + "%, Trap " + String ((int) std::lround (a.style.trap * 100))
               + "%, Hip-Hop " + String ((int) std::lround (a.style.hiphop * 100)) + "%)";
    if (! a.reasons.empty()) s += ".  " + String (a.reasons.front());
    return s;
}

void HipHopMidiProcessor::matchImportedStyle()
{
    if (! importAnalysis) return;
    applyRequestToParameters (importAnalysis->suggested);
    refProgression = importAnalysis->suggested.progression;
    refHookRhythm = importAnalysis->suggested.hookRhythm;
    generate (false);
}

File HipHopMidiProcessor::exportTempMidi() const
{
    auto c = getClip();
    if (c == nullptr || c->result.notes.empty()) return {};
    auto dir = File::getSpecialLocation (File::tempDirectory).getChildFile ("HipHopMidiGenerator");
    dir.createDirectory();
    String name = String (hhmg::noteName (c->result.scale.root)) + "_" + c->result.scale.def().id + "_" + String (c->result.request.bpm)
                  + "bpm_seed" + String ((int) c->result.request.seed);
    auto f = dir.getChildFile (File::createLegalFileName (name) + ".mid");
    return hhmg::writeMidiFile (c->result, f.getFullPathName().toStdString()) ? f : File();
}

// ---- dataset ----------------------------------------------------------------------------------------------------
File HipHopMidiProcessor::defaultDatasetFile() const
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("HipHopMidiGenerator").getChildFile ("patterns.json");
}

bool HipHopMidiProcessor::loadDataset (const File& f, String& message)
{
    try
    {
        auto lib = std::make_shared<hhmg::PatternLibrary> (*std::atomic_load (&library)); // keep embedded + builtin
        auto before = *lib;
        lib->mergeJson (hhmg::Json::parse (f.loadFileAsString().toStdString()));
        message = "Loaded " + f.getFileName() + ": +" + String ((int) (lib->progressions.size() - before.progressions.size())) + " progressions, +"
                  + String ((int) (lib->rhythms.size() - before.rhythms.size())) + " rhythms, +"
                  + String ((int) (lib->motifs.size() - before.motifs.size())) + " motifs";
        std::atomic_store (&library, std::static_pointer_cast<const hhmg::PatternLibrary> (lib));
        return true;
    }
    catch (const std::exception& e)
    {
        message = String ("Dataset error: ") + e.what();
        return false;
    }
}

String HipHopMidiProcessor::libraryInfo() const
{
    auto l = std::atomic_load (&library);
    return String ((int) l->progressions.size()) + " progressions, " + String ((int) l->rhythms.size()) + " rhythms, "
           + String ((int) l->motifs.size()) + " motifs";
}


// ---- voice / audio -> MIDI ----------------------------------------------------------------------------------
File HipHopMidiProcessor::userLibraryFile() const
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("HipHopMidiGenerator").getChildFile ("user_library.json");
}

void HipHopMidiProcessor::processVoiceSamples (std::vector<float> mono, int sr, String name, std::function<void (String)> done)
{
    WeakReference<HipHopMidiProcessor> ref (this);
    Thread::launch ([ref, mono = std::move (mono), sr, name, done]
    {
        hhmg::TranscribeOptions to;
        auto tr = std::make_shared<hhmg::Transcription> (hhmg::transcribeAudio (mono, sr, to));
        MessageManager::callAsync ([ref, tr, name, done]
        {
            if (ref == nullptr) return;
            String msg;
            if (! tr->error.empty()) msg = "Could not use " + name + ": " + String (tr->error);
            else
            {
                std::atomic_store (&ref->voice, std::static_pointer_cast<const hhmg::Transcription> (tr));
                msg = "Heard " + String ((int) tr->notes.size()) + " notes from " + name + ". " + ref->makeFromVoice (false);
            }
            if (done) done (msg);
        });
    });
}

void HipHopMidiProcessor::importAudioAsync (const File& file, std::function<void (String)> done)
{
    WeakReference<HipHopMidiProcessor> ref (this);
    Thread::launch ([ref, file, done]
    {
        AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr)
        {
            MessageManager::callAsync ([done, file] { if (done) done ("Cannot read " + file.getFileName() + " (use WAV, AIFF, FLAC, OGG or MP3)."); });
            return;
        }
        const int sr = (int) reader->sampleRate;
        const int len = (int) jmin<int64> (reader->lengthInSamples, (int64) sr * 90);
        AudioBuffer<float> buf ((int) jmax<unsigned int> (1u, reader->numChannels), len);
        reader->read (&buf, 0, len, 0, true, true);
        std::vector<float> mono ((size_t) len, 0.0f);
        for (int i = 0; i < len; ++i)
        {
            float v = 0;
            for (int c = 0; c < buf.getNumChannels(); ++c) v += buf.getSample (c, i);
            mono[(size_t) i] = v / (float) buf.getNumChannels();
        }
        MessageManager::callAsync ([ref, mono = std::move (mono), sr, file, done]() mutable
        {
            if (ref == nullptr) return;
            ref->processVoiceSamples (std::move (mono), sr, file.getFileName(), done);
        });
    });
}

void HipHopMidiProcessor::startInputRecording()
{
    recCount.store (0);
    recordingInput.store (true);
}

void HipHopMidiProcessor::stopInputRecording (std::function<void (String)> done)
{
    recordingInput.store (false);
    const int n = recCount.load();
    if (n < (int) recRate / 2)
    {
        if (done) done ("Nothing was recorded. The plugin hears only what the DAW sends to its audio input: route a vocal track to it (enable the plugin's input / sidechain), or use Import audio.");
        return;
    }
    std::vector<float> mono (recBuffer.begin(), recBuffer.begin() + n);
    processVoiceSamples (std::move (mono), (int) recRate, "your recording", std::move (done));
}

String HipHopMidiProcessor::makeFromVoice (bool newSeed)
{
    auto tr = std::atomic_load (&voice);
    if (tr == nullptr) return "Import or record some audio first.";
    if (newSeed)
        if (auto* p = apvts.getParameter ("seed"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) Random::getSystemRandom().nextInt (100000)));
    static const char* modes[] = { "melody", "melody_plus", "similar" };
    const int mi = jlimit (0, 2, (int) apvts.getRawParameterValue ("voiceMode")->load());
    hhmg::Json opt = hhmg::Json::object();
    opt.set ("seed", (int) apvts.getRawParameterValue ("seed")->load());
    opt.set ("bass", apvts.getRawParameterValue ("bass")->load() > 0.5f);
    opt.set ("scale_lock", apvts.getRawParameterValue ("scaleLock")->load() > 0.5f);
    opt.set ("keep_bpm", mi != 2 || ! tr->bpmEstimated);
    opt.set ("use_dataset", true);
    // the style knobs steer "similar": a clear winner among the three is passed on
    const float r = apvts.getRawParameterValue ("rnb")->load(), t = apvts.getRawParameterValue ("trap")->load(), h = apvts.getRawParameterValue ("hiphop")->load();
    if (mi == 2) opt.set ("style", t >= r && t >= h ? "trap" : h >= r ? "hiphop" : "rnb");
    auto res = hhmg::studioProcess (modes[mi], tr->notes, tr->bpm, ! tr->bpmEstimated, opt, *std::atomic_load (&library));
    if (! res["ok"].boolean (false)) return String (res["error"].str ("could not build MIDI"));
    applyStudioResult (res);
    return voiceSummary();
}

void HipHopMidiProcessor::applyStudioResult (const hhmg::Json& r)
{
    hhmg::Result res;
    res.request = buildRequest();
    std::vector<hhmg::Note> notes;
    int bpm = 120, bars = 4;
    if (! hhmg::clipFromJson (r["clip"], notes, bpm, bars)) return;
    res.notes = notes;
    res.request.bpm = bpm;
    res.request.bars = bars;
    res.request.root = r["root"].integer (res.request.root);
    res.request.scale = r["scale_index"].integer (res.request.scale);
    res.scale = hhmg::Scale { res.request.root, res.request.scale };
    res.description = r["clip"]["description"].str ("");
    for (const auto& c : r["chords"].items()) res.chordNames.push_back (c.str (""));
    voiceAnalysis = r["analysis"];
    voiceInspiredBy = r["inspired_by"];
    applyRequestToParameters (res.request);
    setResult (res);
}

String HipHopMidiProcessor::voiceSummary() const
{
    if (! voiceAnalysis.isObject()) return {};
    String s = String (voiceAnalysis["key"].str ("")) + ", " + String (voiceAnalysis["detected_style"].str ("")) + " feel";
    if (voiceInspiredBy.isObject()) s += ". Inspired by \"" + String (voiceInspiredBy["name"].str ("")) + "\" (" + String (voiceInspiredBy["pack"].str ("")) + ")";
    return s;
}

hhmg::Json HipHopMidiProcessor::similarForAi (const hhmg::Json& options)
{
    auto c = getClip();
    hhmg::Json e = hhmg::Json::object();
    if (c == nullptr || c->result.notes.empty()) { e.set ("ok", false); e.set ("error", "no clip loaded"); return e; }
    auto clipJson = hhmg::clipToJson (c->result.notes, c->result.request.bpm, c->result.request.bars, c->result.description);
    hhmg::Json opt = options;
    if (! opt.has ("seed")) opt.set ("seed", (int) Random::getSystemRandom().nextInt (1000));
    auto res = hhmg::similarToClip (clipJson, opt, *std::atomic_load (&library));
    if (! res["ok"].boolean (false)) return res;
    WeakReference<HipHopMidiProcessor> ref (this);
    MessageManager::callAsync ([ref, res] { if (ref != nullptr) ref->applyStudioResult (res); });
    hhmg::Json out = hhmg::Json::object();
    for (size_t i = 0; i < res.keys().size(); ++i) if (res.keys()[i] != "clip") out.set (res.keys()[i], res.valueAt (i));
    out.set ("applied", true);
    return out;
}

void HipHopMidiProcessor::addMidiFolderAsync (const File& dir, std::function<void (String)> done)
{
    WeakReference<HipHopMidiProcessor> ref (this);
    auto baseLib = std::atomic_load (&library);
    auto baseUser = std::make_shared<hhmg::PatternLibrary> (*userLibrary);
    Thread::launch ([ref, dir, done, baseLib, baseUser]
    {
        auto lib2 = std::make_shared<hhmg::PatternLibrary> (*baseLib);
        std::string err;
        const int n = hhmg::indexMidiFolder (dir.getFullPathName().toStdString(), *lib2, baseUser.get(), 5000, err);
        MessageManager::callAsync ([ref, dir, done, lib2, baseUser, n, err]
        {
            if (ref == nullptr) return;
            String msg;
            if (n > 0)
            {
                std::atomic_store (&ref->library, std::static_pointer_cast<const hhmg::PatternLibrary> (lib2));
                ref->userLibrary = baseUser;
                auto f = ref->userLibraryFile();
                f.getParentDirectory().createDirectory();
                f.replaceWithText (String (baseUser->toJson().dump()));
                msg = "Indexed " + String (n) + " MIDI files from " + dir.getFileName() + " (profiles only, notes are not stored). Library: " + ref->libraryInfo();
            }
            else msg = "No new MIDI files indexed from " + dir.getFileName() + (err.empty() ? "" : " (" + String (err) + ")");
            if (done) done (msg);
        });
    });
}

// ---- AI ------------------------------------------------------------------------------------------------------------
struct HipHopMidiProcessor::AiThread : public Thread
{
    AiThread (HipHopMidiProcessor& o) : Thread ("HHMG AI"), owner (o) {}
    void run() override
    {
        auto lib = std::atomic_load (&owner.library);
        AiBridge::ClipTools tools;
        tools.analyze = [this] { return owner.analyzeClipJson(); };
        tools.similar = [this] (const hhmg::Json& o) { return owner.similarForAi (o); };
        auto outcome = AiBridge::run (settings, prompt, ctx, *lib, [this] { return threadShouldExit(); }, tools);
        if (threadShouldExit()) return;
        WeakReference<HipHopMidiProcessor> ref (&owner);
        auto cb = done;
        MessageManager::callAsync ([ref, cb, outcome]
        {
            if (ref == nullptr) return;
            if (outcome.result) { ref->applyRequestToParameters (outcome.result->request); ref->setResult (*outcome.result); }
            ref->lastAiMessage = outcome.message;
            if (cb) cb (outcome);
        });
    }
    HipHopMidiProcessor& owner;
    AiBridge::Settings settings;
    String prompt, ctx;
    std::function<void (const AiBridge::Outcome&)> done;
};

File HipHopMidiProcessor::settingsFile() const
{
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("HipHopMidiGenerator").getChildFile ("settings.xml");
}

AiBridge::Settings HipHopMidiProcessor::loadAiSettings() const
{
    AiBridge::Settings s;
    if (auto xml = XmlDocument::parse (settingsFile()))
    {
        s.provider = (AiBridge::Provider) xml->getIntAttribute ("provider", 0);
        s.model = xml->getStringAttribute ("model", s.model);
        s.endpoint = xml->getStringAttribute ("endpoint");
        s.apiKey = xml->getStringAttribute ("apiKey");
    }
    if (s.apiKey.isEmpty() && s.provider == AiBridge::Provider::gemini)
        s.apiKey = SystemStats::getEnvironmentVariable ("GEMINI_API_KEY", {});
    return s;
}

void HipHopMidiProcessor::saveAiSettings (const AiBridge::Settings& s)
{
    XmlElement x ("Settings");
    x.setAttribute ("provider", (int) s.provider);
    x.setAttribute ("model", s.model);
    x.setAttribute ("endpoint", s.endpoint);
    x.setAttribute ("apiKey", s.apiKey);
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    x.writeTo (f);
}

void HipHopMidiProcessor::askAi (const String& prompt, std::function<void (const AiBridge::Outcome&)> done)
{
    cancelAi();
    lastPrompt = prompt;
    auto r = buildRequest();
    hhmg::Json ctx = hhmg::Json::object();
    ctx.set ("root", hhmg::noteName (r.root));
    ctx.set ("scale", hhmg::allScales()[(size_t) r.scale].id);
    ctx.set ("bpm", r.bpm); ctx.set ("bars", r.bars);
    ctx.set ("complexity", r.complexity); ctx.set ("humanize", r.humanize); ctx.set ("swing", r.swing);
    if (importAnalysis) ctx.set ("imported_clip", hhmg::analysisToJson (*importAnalysis));
    if (voiceAnalysis.isObject()) ctx.set ("recorded_voice", voiceAnalysis);
    aiThread = std::make_unique<AiThread> (*this);
    aiThread->settings = loadAiSettings();
    aiThread->prompt = prompt;
    aiThread->ctx = String (ctx.dump());
    aiThread->done = std::move (done);
    aiThread->startThread();
}

bool HipHopMidiProcessor::isAiBusy() const { return aiThread != nullptr && aiThread->isThreadRunning(); }

void HipHopMidiProcessor::cancelAi()
{
    if (aiThread != nullptr) { aiThread->stopThread (3000); aiThread.reset(); }
}

// ---- state ----------------------------------------------------------------------------------------------------------
void HipHopMidiProcessor::getStateInformation (MemoryBlock& dest)
{
    auto xml = apvts.copyState().createXml();
    xml->setAttribute ("prompt", lastPrompt);
    if (auto c = getClip())
    {
        String s;
        for (auto& n : c->result.notes)
            s << n.pitch << ',' << String (n.start, 1) << ',' << String (n.length, 1) << ',' << n.velocity << ',' << n.channel << ';';
        auto* ce = xml->createNewChildElement ("Clip");
        ce->setAttribute ("notes", s);
        ce->setAttribute ("desc", String (c->result.description));
    }
    copyXmlToBinary (*xml, dest);
}

void HipHopMidiProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    lastPrompt = xml->getStringAttribute ("prompt");
    auto tree = ValueTree::fromXml (*xml);
    tree.removeProperty ("prompt", nullptr);
    tree.removeChild (tree.getChildWithName ("Clip"), nullptr);
    apvts.replaceState (tree);
    if (auto* ce = xml->getChildByName ("Clip"))
    {
        hhmg::Result r;
        r.request = buildRequest();
        r.scale = hhmg::Scale { r.request.root, r.request.scale };
        r.description = ce->getStringAttribute ("desc").toStdString();
        for (auto& tok : StringArray::fromTokens (ce->getStringAttribute ("notes"), ";", ""))
        {
            auto f = StringArray::fromTokens (tok, ",", "");
            if (f.size() != 5) continue;
            hhmg::Note n;
            n.pitch = f[0].getIntValue(); n.start = f[1].getDoubleValue(); n.length = f[2].getDoubleValue();
            n.velocity = f[3].getIntValue(); n.channel = jlimit (0, 2, f[4].getIntValue());
            r.notes.push_back (n);
        }
        if (! r.notes.empty()) setResult (r);
    }
}

AudioProcessorEditor* HipHopMidiProcessor::createEditor() { return new HipHopMidiEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HipHopMidiProcessor(); }
