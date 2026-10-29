#include "hhmg/Analyze.h"
#include "hhmg/Guide.h"
#include "hhmg/AudioRead.h"
#include "hhmg/Library.h"
#include "hhmg/Studio.h"
#include "hhmg/Tools.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace hhmg;

static int failures = 0;
static std::string tmpFile(const char* name) { return (std::filesystem::temp_directory_path() / name).string(); }
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

int main() {
    PatternLibrary lib = PatternLibrary::builtin();
    long totalNotes = 0, runs = 0;

    // The hard rule: no out-of-scale note, for every scale, root, style blend, complexity and seed.
    const double mixes[][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 1}, {0.7, 0.2, 0.1}};
    for (int scale = 0; scale < (int) allScales().size(); ++scale)
        for (int root = 0; root < 12; ++root)
            for (const auto& m : mixes)
                for (double cx : {0.0, 0.5, 1.0})
                    for (uint32_t seed = 1; seed <= 3; ++seed) {
                        Request rq;
                        rq.root = root; rq.scale = scale; rq.mix = {m[0], m[1], m[2]};
                        rq.complexity = cx; rq.seed = seed; rq.bars = 1 + (int) (seed * 3 % 8);
                        rq.bass = true; rq.swing = 0.4; rq.humanize = 0.8; rq.velocityVariation = 1.0;
                        Result r = generate(rq, lib);
                        ++runs; totalNotes += (long) r.notes.size();
                        CHECK(countOutOfScale(r) == 0, "scale=%d root=%d seed=%u", scale, root, seed);
                        CHECK(!r.notes.empty(), "empty result");
                        for (const auto& n : r.notes) {
                            CHECK(n.pitch >= 0 && n.pitch <= 127, "pitch %d", n.pitch);
                            CHECK(n.velocity >= 1 && n.velocity <= 127, "vel %d", n.velocity);
                            CHECK(n.start >= 0 && n.length > 0, "timing");
                        }
                    }
    std::printf("checked %ld runs, %ld notes\n", runs, totalNotes);

    // Determinism
    { Request rq; rq.seed = 42; rq.complexity = 0.8;
      Result a = generate(rq, lib), b = generate(rq, lib);
      CHECK(a.notes.size() == b.notes.size(), "determinism size");
      for (size_t i = 0; i < a.notes.size() && i < b.notes.size(); ++i)
          CHECK(a.notes[i].pitch == b.notes[i].pitch && a.notes[i].start == b.notes[i].start, "determinism note %zu", i);
      Request rq2 = rq; rq2.seed = 43;
      Result c = generate(rq2, lib);
      bool differs = a.notes.size() != c.notes.size();
      for (size_t i = 0; !differs && i < a.notes.size(); ++i) differs = a.notes[i].pitch != c.notes[i].pitch || a.notes[i].start != c.notes[i].start;
      CHECK(differs, "different seeds should differ"); }

    // Theory sanity
    { Scale cmaj{0, findScale("major")};
      CHECK(cmaj.contains(60) && !cmaj.contains(61), "C major membership");
      CHECK(cmaj.snap(61) == 60, "tie goes down");
      Scale am{9, findScale("minor")};
      Chord c = buildChord(am, 0, 1, 0, false);
      CHECK(c.name == "Am7", "got %s", c.name.c_str());
      Chord d = buildChord(am, 5, 2, 0, false);
      CHECK(d.name == "Fmaj9", "got %s", d.name.c_str());
      CHECK(parseRoot("Bb") == 10 && parseRoot("F#") == 6, "parseRoot"); }

    // Tool layer
    { Json a = Json::parse("{\"root\":\"C\",\"scale\":\"major\",\"notes\":[60,61,63,66]}");
      Json s = callTool("snap_to_scale", a, lib);
      CHECK(s["ok"].boolean(), "snap ok");
      CHECK(s["notes"].at(1).integer() == 60 && s["notes"].at(2).integer() == 62 && s["notes"].at(3).integer() == 65, "snap values %s", s.dump().c_str());
      Json bad = callTool("scale_notes", Json::parse("{\"root\":\"H\",\"scale\":\"major\"}"), lib);
      CHECK(!bad["ok"].boolean(true), "bad root rejected");
      Json ga = Json::parse("{\"root\":\"F#\",\"scale\":\"phrygian\",\"bars\":4,\"style\":\"trap\",\"bass\":true}");
      ga.set("output_path", tmpFile("hhmg_test.mid"));
      Json g = callTool("generate_midi", ga, lib);
      CHECK(g["ok"].boolean() && g["out_of_scale_notes"].integer() == 0, "generate_midi %s", g.dump().c_str());
      CHECK(toolSchemas().size() >= 6, "schemas");
      Json rt = Json::parse(lib.toJson().dump());
      PatternLibrary lib2; lib2.mergeJson(rt);
      CHECK(lib2.rhythms.size() == lib.rhythms.size(), "library JSON round trip"); }

    // Style guide: every recipe names a real scale, and generating from it stays in key
    { for (const auto& rc : recipes()) {
          int sc = findScale(rc.scale);
          CHECK(sc >= 0, "recipe %s scale", rc.id.c_str());
          CHECK(rc.bpmMin < rc.bpmMax && rc.bars >= 1, "recipe %s ranges", rc.id.c_str());
          for (int d : rc.progression) CHECK(d >= 1 && d <= 7, "recipe %s degrees", rc.id.c_str());
          Request rq; rq.scale = sc; rq.mix = {rc.rnb, rc.trap, rc.hiphop}; rq.complexity = rc.complexity; rq.humanize = rc.humanize;
          rq.swing = rc.swing; rq.bass = rc.bass; rq.bars = rc.bars; rq.progression = rc.progression; rq.bpm = rc.bpmMin;
          CHECK(countOutOfScale(generate(rq, lib)) == 0, "recipe %s in scale", rc.id.c_str());
      }
      Json g = callTool("style_guide", Json::parse("{\"query\":\"dark trap\"}"), lib);
      CHECK(g["ok"].boolean() && g["matches"].at(0)["id"].str() == "dark_trap", "style_guide ranks dark_trap first: %s", g["matches"].at(0)["id"].str().c_str());
      Json fa = callTool("style_guide", Json::parse("{\"query\":\"ترپ تاریک\"}"), lib);
      CHECK(fa["matches"].at(0)["id"].str() == "dark_trap", "persian query");
      Json n = callTool("style_guide", Json::parse("{\"query\":\"smooth romantic r&b\"}"), lib);
      CHECK(n["matches"].at(0)["id"].str() == "rnb_smooth", "smooth rnb"); }

    // Style recognition: generate -> write SMF -> read back -> analyse
    { auto sameSet = [](int r1, int s1, int r2, int s2) { Scale A{r1, s1}, B{r2, s2}; for (int p = 0; p < 12; ++p) if (A.contains(p) != B.contains(p)) return false; return true; };
      int setOk = 0, trapTonic = 0, n = 0, trapN = 0;
      for (int root = 0; root < 12; ++root)
          for (const char* sc : {"minor", "major", "dorian", "phrygian", "harmonic_minor"})
              for (uint32_t seed = 1; seed <= 3; ++seed)
                  for (int trap = 0; trap < 2; ++trap) {
                      Request rq; rq.root = root; rq.scale = findScale(sc); rq.seed = seed; rq.bars = 4; rq.bass = false;
                      if (trap) rq.mix = {0, 1, 0};
                      MidiData md; std::string e;
                      CHECK(readMidi(writeMidi(generate(rq, lib)), md, e), "readMidi %s", e.c_str());
                      Analysis a = analyzeMidi(md, lib);
                      ++n; if (sameSet(a.root, a.scale, root, rq.scale)) ++setOk;
                      if (trap) { ++trapN; if (a.root == root) ++trapTonic; }
                  }
      std::printf("analysis: pitch set %d/%d, trap tonic %d/%d\n", setOk, n, trapTonic, trapN);
      CHECK(setOk >= n * 85 / 100, "pitch-set detection %d/%d", setOk, n);
      CHECK(trapTonic >= trapN * 95 / 100, "trap tonic detection %d/%d", trapTonic, trapN);
      int styleOk = 0;
      for (const auto& rc : recipes()) {
          Request rq; rq.root = 6; rq.scale = findScale(rc.scale); rq.mix = {rc.rnb, rc.trap, rc.hiphop}; rq.complexity = rc.complexity;
          rq.humanize = rc.humanize; rq.swing = rc.swing; rq.bars = rc.bars; rq.progression = rc.progression; rq.bpm = (rc.bpmMin + rc.bpmMax) / 2; rq.seed = 5; rq.bass = false;
          MidiData md; std::string e; readMidi(writeMidi(generate(rq, lib)), md, e);
          Analysis a = analyzeMidi(md, lib);
          std::string want = rc.rnb >= rc.trap && rc.rnb >= rc.hiphop ? "rnb" : rc.trap >= rc.hiphop ? "trap" : "hiphop";
          if (a.topStyle == want) ++styleOk;
      }
      std::printf("style recognised for %d/%d recipes\n", styleOk, (int) recipes().size());
      CHECK(styleOk >= 10, "style recognition %d/12", styleOk);

      // tools: analyze_midi + references
      Request rq; rq.seed = 9; Result res = generate(rq, lib);
      writeMidiFile(res, tmpFile("hhmg_an.mid"));
      Json anArgs = Json::object(); anArgs.set("path", tmpFile("hhmg_an.mid"));
      Json an = callTool("analyze_midi", anArgs, lib);
      CHECK(an["ok"].boolean() && an["suggested_generate_midi_settings"]["scale"].isString(), "analyze_midi %s", an.dump().c_str());
      CHECK(!callTool("analyze_midi", Json::parse("{\"path\":\"/etc/passwd\"}"), lib)["ok"].boolean(true), "analyze_midi refuses non-midi");
      PatternLibrary lib2 = lib;
      lib2.mergeJson(Json::parse("{\"references\":[{\"id\":\"a-1\",\"pack\":\"P\",\"name\":\"Dark one\",\"style\":\"trap\",\"mode\":\"minor\",\"root\":6,\"bpm\":140,\"bars\":8,\"degrees\":[1,6,3,7,1,6,3,7],\"rhythm\":[[0,2,90],[3,1,80],[6,2,90],[10,2,90]]}]}"));
      Json fr = callTool("find_references", Json::parse("{\"style\":\"trap\",\"mode\":\"minor\",\"bpm\":142}"), lib2);
      CHECK(fr["references"].size() == 1 && fr["references"].at(0)["id"].str() == "a-1", "find_references");
      Json gen = callTool("generate_midi", Json::parse("{\"root\":\"F#\",\"scale\":\"minor\",\"style\":\"trap\",\"reference_progression\":\"a-1\",\"reference_rhythm\":\"a-1\"}"), lib2);
      CHECK(gen["ok"].boolean() && gen["degrees"].size() >= 4 && gen["degrees"].at(0).integer() == 1 && gen["degrees"].at(1).integer() == 6, "reference progression used: %s", gen.dump().c_str());
      CHECK(gen["out_of_scale_notes"].integer() == 0, "reference generate in scale");
      CHECK(!callTool("generate_midi", Json::parse("{\"reference_progression\":\"nope\"}"), lib2)["ok"].boolean(true), "unknown reference rejected"); }

    // Voice -> MIDI: synthesise a sung phrase (harmonics + vibrato + breaths between notes) and transcribe it
    { const int sr = 16000;
      const int phrase[] = {57, 60, 64, 62, 60, 57, 55, 57};            // A3 C4 E4 D4 C4 A3 G3 A3
      const double bpm = 120, noteSec = 60.0 / bpm / 2;                   // eighth notes
      std::vector<float> audio((size_t) (sr * 0.3), 0.0f);
      uint32_t rngState = 12345;
      for (int pitch : phrase) {
          double f0 = 440.0 * std::pow(2.0, (pitch - 69) / 12.0);
          size_t n = (size_t) (sr * (noteSec - 0.04)), base = audio.size();
          audio.resize(base + n + (size_t) (sr * 0.04), 0.0f);
          double ph = 0;
          for (size_t i = 0; i < n; ++i) {
              double t = (double) i / sr, vib = 1.0 + 0.012 * std::sin(6.283185307 * 5.5 * t);   // ~20 cent vibrato
              ph += 6.283185307 * f0 * vib / sr;
              double env = std::min(1.0, t / 0.03) * std::min(1.0, (noteSec - 0.04 - t) / 0.03);
              double s = std::sin(ph) + 0.5 * std::sin(2 * ph) + 0.25 * std::sin(3 * ph);
              rngState = rngState * 1664525u + 1013904223u;
              audio[base + i] = (float) (0.3 * env * s + 0.002 * ((rngState >> 16) / 32768.0 - 1.0));
          }
      }
      TranscribeOptions to;
      Transcription tr = transcribeAudio(audio, sr, to);
      CHECK(tr.error.empty(), "transcribe error: %s", tr.error.c_str());
      CHECK(tr.notes.size() == 8, "expected 8 notes, got %zu", tr.notes.size());
      int hit = 0;
      for (size_t i = 0; i < tr.notes.size() && i < 8; ++i) if (tr.notes[i].pitch == phrase[i]) ++hit;
      std::printf("transcribe: %zu notes, %d/8 pitches exact, bpm %.1f\n", tr.notes.size(), hit, tr.bpm);
      CHECK(hit >= 7, "pitch accuracy %d/8", hit);
      CHECK(std::fabs(tr.bpm - 120) < 6 || std::fabs(tr.bpm - 60) < 3 || std::fabs(tr.bpm - 240) < 12, "bpm estimate %.1f", tr.bpm);
      // all three studio modes stay in scale and produce MIDI
      for (const char* mode : {"melody", "melody_plus", "similar"})
          for (const char* style : {"", "trap", "rnb", "hiphop"}) {
              Json opt = Json::object(); opt.set("style", style); opt.set("seed", 3);
              Json r = studioProcess(mode, tr.notes, tr.bpm, true, opt, lib);
              CHECK(r["ok"].boolean(), "studio %s", mode);
              std::vector<Note> cn; int cb = 0, cbars = 0; clipFromJson(r["clip"], cn, cb, cbars);
              CHECK(!cn.empty() && !clipToMidi(r["clip"]).empty(), "clip %s", mode);
              Analysis an = analyzeNotes({}, tr.notes, tr.bpm, lib, true);
              Scale sc{an.root, an.scale};
              for (const auto& n : cn) {
                  bool ok = n.channel == kMelody ? sc.contains(n.pitch) : sc.chordContains(n.pitch);
                  if (!ok) { CHECK(false, "studio %s style '%s' out-of-scale pitch %d", mode, style, n.pitch); break; }
              }
          }
      // WAV file round trip + voice_to_midi / analyze_audio tools
      { std::vector<uint8_t> wav = writeWav16(audio, sr);
        std::vector<float> back; int sr2 = 0; std::string e;
        CHECK(readWav(wav, back, sr2, e) && sr2 == sr && back.size() == audio.size(), "wav round trip %s", e.c_str());
        { std::FILE* f = std::fopen(tmpFile("hhmg_voice.wav").c_str(), "wb"); std::fwrite(wav.data(), 1, wav.size(), f); std::fclose(f); }
        Json aa = Json::object(); aa.set("path", tmpFile("hhmg_voice.wav"));
        Json ar = callTool("analyze_audio", aa, lib);
        CHECK(ar["ok"].boolean() && ar["transcribed_notes"].integer() == 8, "analyze_audio %s", ar.dump().c_str());
        for (const char* mode : {"melody", "melody_plus", "similar"}) {
            Json vm = Json::object(); vm.set("path", tmpFile("hhmg_voice.wav")); vm.set("mode", mode); vm.set("seed", 2);
            vm.set("output_path", tmpFile("hhmg_voice.mid"));
            Json vr = callTool("voice_to_midi", vm, lib);
            CHECK(vr["ok"].boolean() && vr["midi_file"].isString() && vr["note_count"].integer() > 0, "voice_to_midi %s: %s", mode, vr.dump().c_str());
        }
        Json bad = Json::object(); bad.set("path", "/etc/passwd");
        CHECK(!callTool("voice_to_midi", bad, lib)["ok"].boolean(true), "voice_to_midi refuses non-wav"); }

      // user dataset: key from file name, indexing, and "similar" drawing on a reference
      { int kr = -1; bool km = false;
        CHECK(keyFromName("Cymatics - Essential MIDI 34 - G Min.mid", kr, km) && kr == 7 && km, "keyFromName G Min");
        CHECK(keyFromName("loop - F# Maj.mid", kr, km) && kr == 6 && !km, "keyFromName F# Maj");
        CHECK(keyFromName("Bb Minor idea", kr, km) && kr == 10 && km, "keyFromName Bb Minor");
        CHECK(!keyFromName("drum loop 120.mid", kr, km), "keyFromName none");
        PatternLibrary mine = PatternLibrary::builtin(), delta;
        Request rq; rq.root = 7; rq.scale = findScale("minor"); rq.progression = {1, 6, 3, 7}; rq.bars = 8; rq.seed = 4; rq.melody = false;
        std::string e;
        CHECK(indexMidiBytes(writeMidi(generate(rq, mine)), "my pack", "Test loop - G Min.mid", mine, &delta, e), "indexMidiBytes %s", e.c_str());
        CHECK(delta.references.size() == 1 && delta.references[0].mode == "minor" && delta.references[0].root == 7, "reference profile");
        CHECK(delta.references[0].degrees.size() >= 4 && delta.references[0].degrees[0] == 1, "reference degrees");
        CHECK(Json::parse(delta.toJson().dump())["references"].size() == 1, "references persist in JSON");
        Json opt = Json::object(); opt.set("seed", 1);
        Json sim = studioProcess("similar", tr.notes, tr.bpm, true, opt, mine);
        CHECK(sim["ok"].boolean() && sim["inspired_by"]["name"].str("") == "Test loop - G Min", "similar draws on the dataset: %s", sim["inspired_by"].dump().c_str());
        opt.set("use_dataset", false);
        CHECK(!studioProcess("similar", tr.notes, tr.bpm, true, opt, mine).has("inspired_by"), "use_dataset=false");
        Json clipSim = similarToClip(sim["clip"], Json::object(), mine);
        CHECK(clipSim["ok"].boolean(), "similarToClip"); }

      Transcription bad = transcribeAudio(std::vector<float>((size_t) sr, 0.0f), sr, to);
      CHECK(!bad.error.empty(), "silence is rejected"); }

    // type-beat style bank
    { PatternLibrary bank = PatternLibrary::builtin();
      CHECK(bank.typeBeats.empty(), "builtin bank starts empty");
      bank.mergeJson(Json::parse("{\"type_beats\":[{\"name\":\"metro boomin\",\"count\":42,\"bpm\":[130,141,152],\"minor_share\":0.9,\"keys\":[\"F# minor\",\"C# minor\"],\"tags\":[\"dark\",\"hard\"],\"mix\":{\"rnb\":0.1,\"trap\":1,\"hiphop\":0.2}},"
                                  "{\"name\":\"smooth rnb\",\"count\":30,\"bpm\":[70,85,95],\"minor_share\":0.4,\"keys\":[\"A major\"],\"tags\":[\"rnb\"],\"mix\":{\"rnb\":1,\"trap\":0.2,\"hiphop\":0.2}}]}"));
      Json r = callTool("find_type_beat_style", Json::parse("{\"query\":\"Metro Boomin type beat\"}"), bank);
      CHECK(r["ok"].boolean() && r["styles"].size() >= 1 && r["styles"].at(0)["name"].str("") == "metro boomin", "type beat lookup %s", r.dump().c_str());
      CHECK(r["styles"].at(0)["sample_size"].integer() == 42, "sample size reported");
      const Json& sg = r["styles"].at(0)["suggested_generate_midi_settings"];
      CHECK(sg["bpm"].integer() == 141 && sg["root"].str("") == "F#" && sg["bass"].boolean(false), "suggested settings %s", sg.dump().c_str());
      Json gen = Json::object(); gen.set("root", sg["root"]); gen.set("scale", sg["scale"]); gen.set("bpm", sg["bpm"]); gen.set("style_mix", sg["style_mix"]);
      CHECK(callTool("generate_midi", gen, bank)["out_of_scale_notes"].integer() == 0, "generate from type-beat settings");
      CHECK(callTool("find_type_beat_style", Json::parse("{\"query\":\"zzz\"}"), bank)["styles"].size() == 0, "no match");
      CHECK(!callTool("find_type_beat_style", Json::parse("{\"query\":\"x\"}"), PatternLibrary::builtin())["note"].str("").empty(), "empty bank explains itself");
      CHECK(Json::parse(bank.toJson().dump())["type_beats"].size() == 2, "type beats persist"); }

    // MIDI file header
    { Request rq; Result r = generate(rq, lib); auto b = writeMidi(r);
      CHECK(b.size() > 40 && b[0] == 'M' && b[1] == 'T' && b[2] == 'h' && b[3] == 'd', "SMF header");
      CHECK(b[9] == 1, "format 1"); }

    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("all tests passed\n");
    return 0;
}
