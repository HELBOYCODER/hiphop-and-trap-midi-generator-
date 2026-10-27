#include "hhmg/Tools.h"
#include <cstdio>
#include <cstdlib>

using namespace hhmg;

static int failures = 0;
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
      Json g = callTool("generate_midi", Json::parse("{\"root\":\"F#\",\"scale\":\"phrygian\",\"bars\":4,\"style\":\"trap\",\"bass\":true,\"output_path\":\"/tmp/hhmg_test.mid\"}"), lib);
      CHECK(g["ok"].boolean() && g["out_of_scale_notes"].integer() == 0, "generate_midi %s", g.dump().c_str());
      CHECK(toolSchemas().size() >= 6, "schemas");
      Json rt = Json::parse(lib.toJson().dump());
      PatternLibrary lib2; lib2.mergeJson(rt);
      CHECK(lib2.rhythms.size() == lib.rhythms.size(), "library JSON round trip"); }

    // MIDI file header
    { Request rq; Result r = generate(rq, lib); auto b = writeMidi(r);
      CHECK(b.size() > 40 && b[0] == 'M' && b[1] == 'T' && b[2] == 'h' && b[3] == 'd', "SMF header");
      CHECK(b[9] == 1, "format 1"); }

    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("all tests passed\n");
    return 0;
}
