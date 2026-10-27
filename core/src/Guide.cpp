#include "hhmg/Guide.h"
#include "hhmg/Theory.h"
#include <algorithm>
#include <cctype>

namespace hhmg {

const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> r = {
        {"dark_trap", "Dark trap", "ترپ تاریک", {"dark", "trap", "evil", "menacing", "ominous", "تاریک", "ترپ", "ترسناک"},
         0.1, 1.0, 0.2, "minor", 138, 150, 0.35, 0.15, 0.0, 0.3, true, 4, {1, 6, 7, 1},
         "Natural minor with i-VI-VII loops, sparse melody and a held 808 root gives the classic dark trap bed. Keep humanize low so it stays tight.",
         "مینور طبیعی با حلقه‌ی i-VI-VII، ملودی کم‌نت و ۸۰۸ ریشه‌ی کشیده، فضای کلاسیک ترپ تاریک را می‌سازد. Humanize را کم نگه دارید تا تایت بماند."},
        {"eerie_trap", "Eerie / horror trap", "ترپ ترسناک و مرموز", {"eerie", "horror", "spooky", "creepy", "drill", "ترسناک", "مرموز", "دریل"},
         0.0, 1.0, 0.0, "phrygian", 140, 146, 0.3, 0.1, 0.0, 0.25, true, 4, {1, 2, 1, 7},
         "Phrygian's flat 2nd (the semitone above the root) is the signature of eerie/drill melodies; a bell-like melody an octave up with a sliding 808 works well.",
         "درجه‌ی دوم بمل فریژین (نیم‌پرده بالای تونیک) امضای ملودی‌های مرموز و دریل است؛ ملودی زنگوله‌ای یک اکتاو بالاتر با ۸۰۸ اسلاید عالی جواب می‌دهد."},
        {"melodic_trap", "Melodic / emotional trap", "ترپ ملودیک و احساسی", {"melodic", "emotional", "sad", "pain", "lil", "ملودیک", "احساسی", "غمگین"},
         0.5, 0.9, 0.0, "harmonic_minor", 132, 148, 0.55, 0.3, 0.1, 0.35, true, 8, {1, 6, 3, 7},
         "Harmonic minor adds a leading tone for dramatic, sung-style melodies; higher complexity gives 7ths and a busier lead.",
         "هارمونیک مینور حساس (درجه‌ی هفتم بالابرده) اضافه می‌کند که برای ملودی‌های دراماتیک و آوازگونه عالی است؛ پیچیدگی بیشتر هفتم و لید پرنت‌تر می‌دهد."},
        {"trap_soul", "Trap soul", "ترپ سول", {"trap soul", "moody", "late night", "bryson", "نیمه شب", "ترپ سول"},
         0.6, 0.7, 0.0, "dorian", 126, 138, 0.6, 0.35, 0.1, 0.3, true, 8, {1, 4, 7, 3},
         "A blend of R&B voicings (9ths) with trap rhythm; dorian's natural 6th keeps minor harmony from sounding too dark.",
         "ترکیب وویسینگ‌های R&B (نهم) با ریتم ترپ؛ درجه‌ی ششم طبیعی دورین باعث می‌شود هارمونی مینور بیش از حد تاریک نشود."},
        {"rnb_smooth", "Smooth R&B", "R&B نرم و لطیف", {"smooth", "rnb", "r&b", "silky", "romantic", "love", "sensual", "لطیف", "عاشقانه", "آر اند بی"},
         1.0, 0.0, 0.1, "dorian", 70, 90, 0.85, 0.5, 0.25, 0.3, false, 8, {2, 5, 1, 6},
         "High complexity gives 7th/9th chords and open voicings; slow tempo, swing and strong humanize make it feel played by hand.",
         "پیچیدگی بالا آکوردهای هفتم و نهم و وویسینگ باز می‌دهد؛ تمپوی آرام، سوینگ و Humanize بالا حس نواخته‌شدن با دست را می‌دهد."},
        {"neo_soul", "Neo soul / jazzy R&B", "نئوسول و R&B جَزی", {"neo soul", "jazzy", "jazz", "warm", "lush", "نئوسول", "جز"},
         1.0, 0.0, 0.3, "major", 78, 94, 1.0, 0.7, 0.35, 0.35, false, 4, {1, 3, 6, 4},
         "Maximum complexity gives 9ths/11ths and inversions; major or dorian keeps it warm. Lots of humanize and swing for a laid-back pocket.",
         "پیچیدگی حداکثر، نهم و یازدهم و واژگونی می‌دهد؛ ماژور یا دورین حس گرم می‌دهد. Humanize و سوینگ زیاد برای حس ریلکس."},
        {"rnb_late_night", "Late-night R&B (minor)", "R&B شبانه (مینور)", {"night", "dark rnb", "moody rnb", "alternative", "شب", "شبانه"},
         0.9, 0.2, 0.0, "minor", 62, 80, 0.7, 0.45, 0.15, 0.3, false, 8, {6, 7, 1, 1},
         "VI-VII-i style loops in natural minor with slow tempo; add trap weight only for a little hi-hat-era bounce.",
         "حلقه‌هایی مثل VI-VII-i در مینور طبیعی با تمپوی آهسته؛ فقط برای کمی جَست ترپی وزن Trap را اضافه کنید."},
        {"boom_bap", "Boom bap / classic hip-hop", "بوم‌بپ و هیپ‌هاپ کلاسیک", {"boom bap", "classic", "90s", "old school", "dusty", "sample", "بوم بپ", "کلاسیک", "قدیمی"},
         0.1, 0.0, 1.0, "pentatonic_minor", 85, 96, 0.4, 0.6, 0.5, 0.35, true, 4, {1, 4, 1, 5},
         "Minor pentatonic melody over simple i-iv chords, strong swing and loose humanize gives the dusty sampled-loop feel.",
         "ملودی پنتاتونیک مینور روی آکوردهای ساده‌ی i-iv با سوینگ قوی و Humanize شل، حس لوپ سمپل‌شده‌ی غبارآلود را می‌دهد."},
        {"lofi_hiphop", "Lo-fi hip-hop", "هیپ‌هاپ لو‌فای", {"lofi", "lo-fi", "chill", "study", "relax", "لوفای", "آرام"},
         0.5, 0.0, 0.9, "dorian", 70, 88, 0.6, 0.7, 0.4, 0.4, false, 4, {2, 5, 1, 1},
         "7th chords with sloppy humanize and swing; keep melody sparse for a chill loop.",
         "آکوردهای هفتم با Humanize و سوینگ شل؛ ملودی را کم‌نت نگه دارید تا لوپ آرام شود."},
        {"west_coast", "West-coast / G-funk feel", "حس وست‌کوست و جی‌فانک", {"west coast", "g-funk", "funk", "bounce", "party", "وست کوست", "فانک"},
         0.2, 0.2, 1.0, "mixolydian", 90, 100, 0.5, 0.4, 0.3, 0.35, true, 4, {1, 4, 1, 5},
         "Mixolydian (major with a flat 7th) gives the bluesy, bright G-funk colour; keep the 808 on roots.",
         "میکسولیدین (ماژور با هفتم بمل) رنگ بلوزی و روشن جی‌فانک می‌دهد؛ ۸۰۸ را روی ریشه‌ها نگه دارید."},
        {"cinematic_trap", "Cinematic / epic trap", "ترپ سینمایی و حماسی", {"cinematic", "epic", "orchestral", "dramatic", "movie", "سینمایی", "حماسی"},
         0.3, 0.8, 0.2, "harmonic_minor", 128, 144, 0.7, 0.2, 0.0, 0.4, true, 8, {1, 6, 7, 1},
         "Harmonic minor with higher complexity and 8 bars lets the progression develop; octave 5-6 melody sounds like strings/choir.",
         "هارمونیک مینور با پیچیدگی بالاتر و ۸ میزان اجازه می‌دهد پروگرِشن رشد کند؛ ملودی در اکتاو ۵ تا ۶ شبیه زهی و کر می‌شود."},
        {"bright_rnb_pop", "Bright / feel-good R&B", "R&B شاد و روشن", {"happy", "bright", "feel good", "summer", "uplifting", "شاد", "روشن", "تابستان"},
         0.9, 0.1, 0.3, "major", 95, 112, 0.6, 0.4, 0.2, 0.3, false, 4, {1, 5, 6, 4},
         "Major with I-V-vi-IV style loops and moderate complexity keeps it uplifting.",
         "ماژور با حلقه‌های I-V-vi-IV و پیچیدگی متوسط حس شاد و بالابرنده می‌دهد."},
    };
    return r;
}

static const struct { const char* id; const char* en; const char* fa; } kScaleCharacter[] = {
    {"major", "bright, happy, resolved", "روشن، شاد و قطعی"},
    {"minor", "sad, dark, the default for trap/R&B", "غمگین و تاریک؛ پیش‌فرض ترپ و R&B"},
    {"harmonic_minor", "dramatic, cinematic, leading tone gives tension", "دراماتیک و سینمایی؛ حساس بالابرده تنش می‌سازد"},
    {"melodic_minor", "jazzy minor, smooth upward lines", "مینور جَزی با خطوط صعودی نرم"},
    {"dorian", "minor with a hopeful 6th - smooth R&B, neo soul, lo-fi", "مینور با ششم امیدوار؛ R&B نرم، نئوسول و لوفای"},
    {"phrygian", "dark and eerie flat 2nd - drill, horror trap", "فا دوم بمل تاریک و مرموز؛ دریل و ترپ ترسناک"},
    {"lydian", "dreamy, floating, raised 4th", "رویایی و معلق؛ درجه‌ی چهارم بالابرده"},
    {"mixolydian", "major with bluesy flat 7th - funk, west coast", "ماژور با هفتم بمل بلوزی؛ فانک و وست‌کوست"},
    {"locrian", "unstable, tense, rarely used for full progressions", "ناپایدار و پرتنش؛ به‌ندرت برای پروگرِشن کامل"},
    {"phrygian_dominant", "exotic, Middle-Eastern flavour, major chord on the root", "حس شرقی و عربی؛ آکورد ماژور روی تونیک"},
    {"pentatonic_major", "simple, open, singable", "ساده، باز و قابل‌خواندن"},
    {"pentatonic_minor", "safe minor lead notes - boom bap and trap melodies", "نت‌های لید مینور بی‌خطر؛ ملودی بوم‌بپ و ترپ"},
    {"blues", "minor pentatonic plus the blue note (b5) for grit", "پنتاتونیک مینور به‌علاوه‌ی نت بلو (بمل پنجم) برای زبری"},
};

static std::string lower(std::string s) { for (auto& c : s) c = (char) std::tolower((unsigned char) c); return s; }

Json styleGuide(const Json& a) {
    std::string q = lower(a["query"].str("") + " " + a["mood"].str("") + " " + a["style"].str(""));
    std::string style = a["style"].str("");
    struct Scored { double score; const Recipe* r; };
    std::vector<Scored> ranked;
    for (const auto& r : recipes()) {
        double s = 0;
        for (const auto& k : r.keywords) if (q.find(k) != std::string::npos) s += 2.0 + (double) k.size() / 20.0;
        if (style == "rnb") s += r.rnb; else if (style == "trap") s += r.trap; else if (style == "hiphop") s += r.hiphop;
        ranked.push_back({s, &r});
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const Scored& x, const Scored& y) { return x.score > y.score; });

    Json out = Json::object();
    Json list = Json::array();
    const size_t limit = a["all"].boolean(false) ? ranked.size() : 3;
    for (size_t i = 0; i < ranked.size() && i < limit; ++i) {
        const Recipe& r = *ranked[i].r;
        Json x = Json::object();
        x.set("id", r.id); x.set("label", r.label_en); x.set("label_fa", r.label_fa);
        Json mix = Json::object(); mix.set("rnb", r.rnb); mix.set("trap", r.trap); mix.set("hiphop", r.hiphop);
        x.set("style_mix", mix);
        x.set("scale", r.scale);
        x.set("bpm_range", std::to_string(r.bpmMin) + "-" + std::to_string(r.bpmMax));
        x.set("bpm", (r.bpmMin + r.bpmMax) / 2);
        x.set("complexity", r.complexity); x.set("humanize", r.humanize); x.set("swing", r.swing);
        x.set("velocity_variation", r.velocityVariation); x.set("bass", r.bass); x.set("bars", r.bars);
        Json pr = Json::array(); for (int d : r.progression) pr.push(d);
        x.set("progression", pr);
        x.set("why", r.why_en); x.set("why_fa", r.why_fa);
        list.push(x);
    }
    out.set("matches", list);
    out.set("key_tip", "Any root works; pick the key by vocal range or sample. Minor scales: A, F#, C#, D, G are common for trap/R&B. Low roots (E, F, F#) make heavier 808s.");

    Json sc = Json::array();
    for (const auto& c : kScaleCharacter) { Json x = Json::object(); x.set("scale", c.id); x.set("character", c.en); x.set("character_fa", c.fa); sc.push(x); }
    out.set("scale_character", sc);

    if (a["include_help"].boolean(true)) {
        Json h = Json::object();
        h.set("style_mix", "rnb/trap/hiphop weights 0-1, normalised. R&B = lush 7th/9th chords, glide rhythms; Trap = sparse, repeated notes, bigger leaps, 808; Hip-Hop = swung, loop-based, simple chords.");
        h.set("complexity", "0 = triads and sparse melody; 1 = 7th/9th/11th chords, inversions, open voicings, denser melody.");
        h.set("humanize", "Timing and velocity looseness. Low (0-0.2) for tight trap; high (0.5-0.8) for played-by-hand soul/lo-fi.");
        h.set("swing", "Delays off-beat 16ths. 0 straight; 0.3-0.5 boom bap / lo-fi.");
        h.set("velocity_variation", "Spread of note loudness.");
        h.set("bass", "Adds an 808-style root line that follows the chords.");
        h.set("progression", "Optional explicit chord degrees 1-7 in the chosen scale.");
        h.set("seed", "Same seed and settings reproduce the same result; change it for a new idea.");
        out.set("settings_help", h);
    }
    return out;
}

} // namespace hhmg
