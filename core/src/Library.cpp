#include "hhmg/Library.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace hhmg {
namespace {
std::string lower(std::string s) { for (auto& c : s) c = (char) std::tolower((unsigned char) c); return s; }
std::string stem(const std::string& n) { auto p = n.find_last_of("/\\"); std::string f = p == std::string::npos ? n : n.substr(p + 1); auto d = f.find_last_of('.'); return d == std::string::npos ? f : f.substr(0, d); }
std::string hex6(const std::string& s) { uint32_t h = 2166136261u; for (unsigned char c : s) { h ^= c; h *= 16777619u; } char b[16]; std::snprintf(b, sizeof b, "%06x", h & 0xFFFFFF); return b; }
std::string slug(const std::string& s) { std::string o; for (char c : lower(s)) if (std::isalnum((unsigned char) c)) o += c; return o.substr(0, 10); }
}

bool keyFromName(const std::string& name, int& root, bool& minor) {
    std::string n = stem(name);
    for (size_t i = 0; i + 4 < n.size() + 1; ++i) {
        if (i > 0 && !(n[i - 1] == ' ' || n[i - 1] == '-' || n[i - 1] == '_')) continue;
        char c = (char) std::toupper((unsigned char) n[i]);
        if (c < 'A' || c > 'G') continue;
        size_t j = i + 1; int acc = 0;
        if (j < n.size() && (n[j] == '#' || n[j] == 'b')) { acc = n[j] == '#' ? 1 : -1; ++j; }
        while (j < n.size() && n[j] == ' ') ++j;
        std::string rest = lower(n.substr(j, 3));
        bool isMin = rest == "min", isMaj = rest == "maj";
        if (!isMin && !isMaj) continue;
        size_t after = j + 3;
        if (after < n.size() && std::isalpha((unsigned char) n[after]) && lower(n.substr(j, 5)) != "minor" && lower(n.substr(j, 5)) != "major") continue;
        static const int base[] = {9, 11, 0, 2, 4, 5, 7};
        root = ((base[c - 'A'] + acc) % 12 + 12) % 12; minor = isMin;
        return true;
    }
    return false;
}

bool indexMidiBytes(const std::vector<uint8_t>& bytes, const std::string& pack, const std::string& name,
                    PatternLibrary& lib, PatternLibrary* delta, std::string& error) {
    MidiData md;
    if (!readMidi(bytes, md, error)) return false;
    int froot = -1, fscale = -1; bool fminor = true;
    if (keyFromName(name, froot, fminor)) fscale = findScale(fminor ? "minor" : "major"); else froot = -1;
    Analysis a = analyzeMidi(md, lib, froot, fscale);
    if (a.degrees.size() < 2) { error = "no clear chord progression found"; return false; }
    const Scale sc{a.root, a.scale};

    Reference r;
    r.pack = pack.empty() ? "my library" : pack;
    r.name = stem(name);
    r.id = slug(r.pack) + "-" + hex6(r.pack + "/" + name);
    r.style = a.topStyle;
    r.mode = sc.isMinorish() ? "minor" : "major";
    r.root = a.root; r.bars = a.bars; r.bpm = md.hasTempo ? md.bpm : 0; r.density = a.density; r.swing = a.swing;
    r.degrees.assign(a.degrees.begin(), a.degrees.begin() + (long) std::min<size_t>(16, a.degrees.size()));
    r.rhythm = a.rhythm;

    auto addTo = [&](PatternLibrary& L) {
        for (const auto& x : L.references) if (x.id == r.id) return;    // already indexed
        L.references.push_back(r);
        Progression p{r.style, r.mode, {}};
        for (size_t i = 0; i < r.degrees.size() && i < 4; ++i) p.degrees.push_back(r.degrees[i]);
        if (p.degrees.size() >= 2) L.progressions.push_back(p);
        if (r.rhythm.size() >= 2 && r.rhythm.size() <= 10) { Rhythm rh; rh.name = "user_" + r.id; rh.style = r.style; rh.bars = 1; rh.notes = r.rhythm; L.rhythms.push_back(rh); }
    };
    addTo(lib);
    if (delta) addTo(*delta);
    return true;
}

int indexMidiFolder(const std::string& dir, PatternLibrary& lib, PatternLibrary* delta, int maxFiles, std::string& error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) { error = "not a folder: " + dir; return 0; }
    int added = 0;
    const std::string pack = fs::path(dir).filename().string();
    for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; it != end && added < maxFiles; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        std::string ext = lower(it->path().extension().string());
        if (ext != ".mid" && ext != ".midi") continue;
        if (fs::file_size(it->path(), ec) > 5u * 1024 * 1024) continue;
        std::ifstream f(it->path(), std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::string e;
        size_t before = lib.references.size();
        if (indexMidiBytes(bytes, pack, it->path().filename().string(), lib, delta, e) && lib.references.size() > before) ++added;
    }
    return added;
}

} // namespace hhmg
