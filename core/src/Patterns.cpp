#include "hhmg/Patterns.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace hhmg {

// "start:len[:vel],..." in 16th steps
static Rhythm rhy(const char* name, const char* style, const char* spec, int bars = 1) {
    Rhythm r;
    r.name = name; r.style = style; r.bars = bars;
    std::stringstream ss(spec);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        double a = 0, b = 1; int v = 0;
        int n = std::sscanf(tok.c_str(), "%lf:%lf:%d", &a, &b, &v);
        if (n >= 2) r.notes.push_back({a, b, n == 3 ? v : (std::fmod(a, 4.0) == 0.0 ? 100 : 82)});
    }
    return r;
}

PatternLibrary PatternLibrary::builtin() {
    PatternLibrary L;
    auto& R = L.rhythms;
    R.push_back(rhy("rnb_glide", "rnb", "0:3,3:2,6:2,8:4,12:2,14:2"));
    R.push_back(rhy("rnb_sustain", "rnb", "0:6,6:2,8:3,11:5"));
    R.push_back(rhy("rnb_offbeat", "rnb", "2:2,4:4,10:2,12:4"));
    R.push_back(rhy("rnb_lilt", "rnb", "0:2,2:2,4:3,7:1,8:2,10:2,12:4"));
    R.push_back(rhy("rnb_late", "rnb", "0:4,5:3,8:2,10:2,13:3"));
    R.push_back(rhy("rnb_pad", "rnb", "0:8,8:3,11:2,14:2"));
    R.push_back(rhy("rnb_two_bar", "rnb", "0:3,3:3,6:2,8:4,16:2,18:2,20:3,24:6", 2));
    R.push_back(rhy("trap_sparse", "trap", "0:2,3:1,6:2,10:1,11:1,12:4"));
    R.push_back(rhy("trap_roll", "trap", "0:4,6:2,8:2,11:1,12:1,13:1,14:2"));
    R.push_back(rhy("trap_stutter", "trap", "0:1,1:1,3:2,6:2,8:4,14:2"));
    R.push_back(rhy("trap_dotted", "trap", "0:3,3:3,8:3,11:2,14:1,15:1"));
    R.push_back(rhy("trap_triplet", "trap", "0:2,4:2,7:1,8:1,10:2,12:1.333,13.333:1.333,14.667:1.333"));
    R.push_back(rhy("trap_long", "trap", "0:6,6:1,7:1,8:6,14:2"));
    R.push_back(rhy("trap_two_bar", "trap", "0:2,3:2,6:2,10:2,16:3,19:1,22:2,26:2,28:4", 2));
    R.push_back(rhy("hh_loop", "hiphop", "0:2,3:2,6:2,8:2,11:2,14:2"));
    R.push_back(rhy("hh_quarters", "hiphop", "0:4,4:2,8:4,12:2"));
    R.push_back(rhy("hh_dusty", "hiphop", "0:2,2:2,5:3,8:2,10:2,13:3"));
    R.push_back(rhy("hh_swing", "hiphop", "0:3,4:1,6:2,8:3,12:1,14:2"));
    R.push_back(rhy("hh_hold", "hiphop", "0:6,8:2,10:2,12:4"));
    R.push_back(rhy("hh_ghost", "hiphop", "2:2,6:2,10:2,14:2"));
    R.push_back(rhy("hh_two_bar", "hiphop", "0:3,3:2,6:2,8:3,12:2,16:2,18:2,22:2,24:4", 2));

    auto m = [&](const char* style, std::initializer_list<int> v) { L.motifs.push_back({style, v}); };
    m("rnb", {0, 1, 2, 1, 0, -1, 0});
    m("rnb", {0, 2, 1, 0, -2, -1});
    m("rnb", {0, -1, -2, 0, 1, 0});
    m("rnb", {0, 1, 1, -1, -1, 2, 0});
    m("trap", {0, 0, 2, 0, -1, 0, 3, 2});
    m("trap", {0, 0, 0, -2, 0, 1});
    m("trap", {0, 3, -1, 0, -2, 0});
    m("hiphop", {0, 0, 1, 0, -1, -2});
    m("hiphop", {0, 2, 0, -1, 1, 0});
    m("hiphop", {0, 1, 0, 0, -2, 0});

    auto p = [&](const char* style, const char* mode, std::initializer_list<int> d) { L.progressions.push_back({style, mode, d}); };
    p("rnb", "minor", {1, 6, 3, 7});  p("rnb", "minor", {1, 4, 7, 3});
    p("rnb", "minor", {2, 5, 1, 6});  p("rnb", "minor", {6, 7, 1, 1});  p("rnb", "minor", {1, 3, 6, 7});
    p("rnb", "major", {1, 3, 6, 4});  p("rnb", "major", {2, 5, 1, 6});
    p("rnb", "major", {1, 6, 2, 5});  p("rnb", "major", {4, 5, 3, 6});  p("rnb", "major", {1, 4, 2, 5});
    p("trap", "minor", {1, 6, 7, 1}); p("trap", "minor", {1, 7, 6, 7});
    p("trap", "minor", {1, 4, 5, 1}); p("trap", "minor", {6, 7, 1, 1}); p("trap", "minor", {1, 3, 7, 6});
    p("trap", "major", {1, 5, 6, 4}); p("trap", "major", {6, 4, 1, 5});
    p("hiphop", "minor", {1, 4, 1, 5}); p("hiphop", "minor", {1, 7, 6, 7});
    p("hiphop", "minor", {1, 1, 4, 4}); p("hiphop", "minor", {2, 7, 1, 1});
    p("hiphop", "major", {1, 4, 1, 5}); p("hiphop", "major", {1, 1, 4, 4}); p("hiphop", "major", {2, 5, 1, 1});
    return L;
}

void PatternLibrary::mergeJson(const Json& j) {
    if (!j.isObject()) throw std::runtime_error("patterns: root must be an object");
    for (const auto& r : j["rhythms"].items()) {
        Rhythm x;
        x.name = r["name"].str("learned");
        x.style = r["style"].str("hiphop");
        x.bars = std::max(1, std::min(4, r["bars"].integer(1)));
        for (const auto& n : r["notes"].items())
            if (n.size() >= 2) x.notes.push_back({n.at(0).num(), n.at(1).num(), n.size() > 2 ? n.at(2).integer() : 90});
        if (!x.notes.empty()) rhythms.push_back(std::move(x));
    }
    for (const auto& r : j["motifs"].items()) {
        Motif x{r["style"].str("hiphop"), {}};
        for (const auto& n : r["steps"].items()) x.steps.push_back(n.integer());
        if (x.steps.size() >= 3) motifs.push_back(std::move(x));
    }
    for (const auto& r : j["progressions"].items()) {
        Progression x{r["style"].str("hiphop"), r["mode"].str("minor"), {}};
        for (const auto& n : r["degrees"].items()) x.degrees.push_back(n.integer());
        bool ok = x.degrees.size() >= 2;
        for (int d : x.degrees) ok = ok && d >= 1 && d <= 7;
        if (ok) progressions.push_back(std::move(x));
    }
    for (const auto& r : j["references"].items()) {
        Reference x;
        x.id = r["id"].str(""); x.pack = r["pack"].str(""); x.name = r["name"].str(""); x.style = r["style"].str("hiphop");
        x.mode = r["mode"].str("minor"); x.root = ((r["root"].integer() % 12) + 12) % 12; x.bars = r["bars"].integer();
        x.bpm = r["bpm"].num(); x.density = r["density"].num(); x.swing = r["swing"].num();
        for (const auto& d : r["degrees"].items()) if (d.integer() >= 1 && d.integer() <= 7) x.degrees.push_back(d.integer());
        for (const auto& n : r["rhythm"].items())
            if (n.size() >= 2) x.rhythm.push_back({n.at(0).num(), n.at(1).num(), n.size() > 2 ? n.at(2).integer() : 90});
        if (!x.id.empty() && !x.degrees.empty()) references.push_back(std::move(x));
    }
}

Json PatternLibrary::toJson() const {
    Json o = Json::object();
    Json rr = Json::array();
    for (const auto& r : rhythms) {
        Json x = Json::object();
        x.set("name", r.name); x.set("style", r.style); x.set("bars", r.bars);
        Json ns = Json::array();
        for (const auto& n : r.notes) { Json t = Json::array(); t.push(n.step); t.push(n.len); t.push(n.vel); ns.push(t); }
        x.set("notes", ns);
        rr.push(x);
    }
    o.set("rhythms", rr);
    Json mm = Json::array();
    for (const auto& m : motifs) {
        Json x = Json::object(); x.set("style", m.style);
        Json s = Json::array(); for (int v : m.steps) s.push(v);
        x.set("steps", s); mm.push(x);
    }
    o.set("motifs", mm);
    Json pp = Json::array();
    for (const auto& p : progressions) {
        Json x = Json::object(); x.set("style", p.style); x.set("mode", p.mode);
        Json s = Json::array(); for (int v : p.degrees) s.push(v);
        x.set("degrees", s); pp.push(x);
    }
    o.set("progressions", pp);
    Json refs = Json::array();
    for (const auto& r : references) {
        Json x = Json::object();
        x.set("id", r.id); x.set("pack", r.pack); x.set("name", r.name); x.set("style", r.style); x.set("mode", r.mode);
        x.set("root", r.root); x.set("bars", r.bars); x.set("bpm", r.bpm); x.set("density", r.density); x.set("swing", r.swing);
        Json d = Json::array(); for (int v : r.degrees) d.push(v); x.set("degrees", d);
        Json rh = Json::array(); for (const auto& n : r.rhythm) { Json t = Json::array(); t.push(n.step); t.push(n.len); t.push(n.vel); rh.push(t); }
        x.set("rhythm", rh);
        refs.push(x);
    }
    o.set("references", refs);
    return o;
}

} // namespace hhmg
