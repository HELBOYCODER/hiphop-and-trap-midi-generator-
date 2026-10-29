# HipHop MIDI Generator · مولد MIDI هیپ‌هاپ

**EN | [فارسی ↓](#فارسی)**

---

## English

A **VST3 plugin (macOS + Windows)** and an **Android app** that generate and understand MIDI for **Trap, Hip-Hop and R&B**.
Chords, melodies and 808 lines **never leave the chosen scale**; the AI is connected to the same tools, a reference dataset and external MCP sites.

### What it does

| | |
|---|---|
| **Generator** | Chords + melody + 808 in 13 scales/modes; style blend (R&B / Trap / Hip-Hop), complexity, swing, humanize, velocity variation, seed, presets |
| **Scale lock** | Every note is snapped to the scale by construction; verified by tests (7 020 generations, 0 wrong notes) |
| **Record / import audio** | Hum or sing → melody MIDI, melody + chords + 808, or a *similar* new idea (key, tempo and feel recognised) |
| **Style recognition** | Drop a MIDI file or audio → key, mode, BPM, chord degrees, swing and R&B/Trap/Hip-Hop scores |
| **Similar + dataset** | Ideas drawn from reference profiles (POP909, Cymatics-derived stats, your own MIDI folder). Only profiles are stored, never the notes |
| **Lyrics / poems → notes** | Any language (Persian included): one line per bar, stressed syllables on chord tones |
| **AI** | Gemini or any OpenAI-compatible endpoint (OpenAI, OpenRouter, Ollama, LM Studio); the model calls engine tools, so output stays in scale. Busy models (HTTP 503/429) are retried, then Gemini falls back to a lighter model |
| **MCP** | Built-in MCP server (`hhmg-cli mcp`) **and** an MCP client in the apps: Hugging Face's public MCP server is connected by default (dataset/model search); add any streamable-HTTP MCP |
| **Type-beat style bank** | Aggregate BPM/key/tag statistics per artist/vibe (the bank ships empty; fill it with `tools/typebeats/harvest.py`) |
| **Android** | Voice → MIDI, similar, generator tab with all plugin controls, piano roll with the pitch curve of your voice, playback, MIDI export/share, AI, MCP |
| **Agent-native** | `hhmg-cli list / describe / call` (JSON out), stdio MCP, and a ready skill: [`skills/hhmg/SKILL.md`](skills/hhmg/SKILL.md) |

### Download & install

Binaries are built by GitHub Actions: open the latest run of **Build VST3** / **Android APK** under the [Actions tab](../../actions) and download the artifacts (or push a `v*` tag to publish a release).

- **macOS** – universal (arm64 + x86_64), ad-hoc signed. Copy `HipHop MIDI Generator.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
- **Windows x64** – copy the `.vst3` folder to `C:\Program Files\Common Files\VST3\`.
- **Android** – install the APK (allow unknown sources); first launch asks for microphone access.

Load the plugin on an **instrument** track, press **GENERATE**, drag **DRAG MIDI TO DAW** onto a track.

### Build from source

```bash
./scripts/build.sh                      # macOS / Linux -> dist/*.vst3 (runs the tests)
.\scripts\build.ps1                     # Windows x64 (VS 2022 PowerShell)
g++ -std=c++17 -Icore/include core/src/*.cpp tests/test_core.cpp -o t && ./t   # core tests only
```

### Use it from an AI agent

```bash
hhmg-cli list
hhmg-cli call generate_midi --args '{"root":"F#","scale":"phrygian","bpm":142,"bars":4,"style_mix":{"trap":1},"output_path":"idea.mid"}'
printf 'dark city lights\nwe rise tonight\n' | hhmg-cli lyrics - --style trap --out lyrics.mid
hhmg-cli mcp          # stdio MCP server for Claude Desktop, Cursor, ...
```

### Honest limits

- The plugin has been built and unit-tested by CI but not yet verified inside every DAW; the Android UI has not been checked on every device.
- AI features need your own API key; key/style detection and voice transcription are heuristic and work best on clean, monophonic input.
- The type-beat bank is empty until you harvest data you are allowed to use. No audio is ripped or redistributed.

### Documentation

- English guide: [docs/USER_GUIDE.en.md](docs/USER_GUIDE.en.md)
- Android: [docs/ANDROID.md](docs/ANDROID.md) · Type beats: [docs/TYPE_BEATS.md](docs/TYPE_BEATS.md)
- Agents / MCP: [docs/AGENT_NATIVE.md](docs/AGENT_NATIVE.md) · Architecture: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- Dataset data & licences: [data/README.md](data/README.md)

---

## فارسی

یک **پلاگین VST3 (مک و ویندوز)** و یک **اپ اندروید** برای ساخت و تحلیل MIDI در سبک‌های **ترپ، هیپ‌هاپ و R&B**.
آکورد، ملودی و خط ۸۰۸ **هرگز از گام انتخاب‌شده بیرون نمی‌روند**؛ هوش مصنوعی به همان ابزارها، به دیتاست مرجع و به سایت‌های MCP بیرونی وصل است.

### امکانات

| | |
|---|---|
| **جنریتور** | آکورد + ملودی + ۸۰۸ در ۱۳ گام/مود؛ ترکیب سبک (R&B / Trap / Hip-Hop)، پیچیدگی، سوینگ، انسان‌سازی، تنوع ولوم، seed، پریست |
| **قفل گام** | هر نت ساختاری به گام چسبانده می‌شود؛ با تست تأیید شده (۷٬۰۲۰ تولید، صفر نت غلط) |
| **ضبط یا ایمپورت صدا** | هوم یا آواز ← ملودی MIDI، ملودی + آکورد + ۸۰۸، یا ایدهٔ *مشابه* جدید (کلید، تمپو و فیل تشخیص داده می‌شود) |
| **تشخیص سبک** | فایل MIDI یا صدا را بیندازید ← کلید، مود، BPM، درجات آکورد، سوینگ و امتیاز R&B/Trap/Hip-Hop |
| **Similar + دیتاست** | ایده از پروفایل‌های مرجع (POP909، آمار برگرفته از Cymatics، پوشهٔ MIDI خودتان). فقط پروفایل ذخیره می‌شود، نه نت‌ها |
| **شعر / متن ترانه ← نت** | هر زبانی (فارسی هم): هر خط یک میزان، هجاهای تأکیدی روی نت‌های آکورد |
| **هوش مصنوعی** | Gemini یا هر endpoint سازگار با OpenAI (OpenAI، OpenRouter، Ollama، LM Studio)؛ مدل ابزارهای موتور را صدا می‌زند پس خروجی داخل گام می‌ماند. مدل شلوغ (خطای ۵۰۳/۴۲۹) چند بار دوباره تلاش می‌شود و Gemini به مدل سبک‌تر می‌رود |
| **MCP** | سرور MCP داخلی (`hhmg-cli mcp`) **و** کلاینت MCP در اپ‌ها: سرور عمومی Hugging Face به‌صورت پیش‌فرض وصل است (جستجوی دیتاست/مدل)؛ هر MCP از نوع streamable-HTTP را می‌توانید اضافه کنید |
| **بانک سبک تایپ‌بیت** | آمار تجمیعی BPM/کلید/تگ برای هر خواننده/حال‌وهوا (بانک خالی عرضه می‌شود؛ با `tools/typebeats/harvest.py` پر کنید) |
| **اندروید** | صدا ← MIDI، similar، تب جنریتور با همهٔ کنترل‌های پلاگین، پیانو رول با منحنی فرکانس صدای شما، پخش، خروجی/اشتراک MIDI، AI، MCP |
| **ایجنت‌محور** | `hhmg-cli list / describe / call` (خروجی JSON)، MCP روی stdio و فایل مهارت آماده: [`skills/hhmg/SKILL.md`](skills/hhmg/SKILL.md) |

### دانلود و نصب

فایل‌ها با GitHub Actions ساخته می‌شوند: در [تب Actions](../../actions) آخرین اجرای **Build VST3** و **Android APK** را باز کنید و Artifactها را بگیرید (یا یک تگ `v*` بزنید تا Release منتشر شود).

- **مک** – یونیورسال (arm64 + x86_64)، امضای ad-hoc. فایل `HipHop MIDI Generator.vst3` را در `~/Library/Audio/Plug-Ins/VST3/` بگذارید.
- **ویندوز x64** – پوشهٔ `.vst3` را در `C:\Program Files\Common Files\VST3\` بگذارید.
- **اندروید** – APK را نصب کنید (نصب از منبع ناشناس را مجاز کنید)؛ بار اول دسترسی میکروفون می‌خواهد.

پلاگین را روی یک ترک **Instrument** بارگذاری کنید، **GENERATE** را بزنید و **DRAG MIDI TO DAW** را روی ترک بکشید.

### ساخت از روی سورس

```bash
./scripts/build.sh                      # مک / لینوکس -> dist/*.vst3 (همراه با تست‌ها)
.\scripts\build.ps1                     # ویندوز x64 (PowerShell نسخهٔ VS 2022)
g++ -std=c++17 -Icore/include core/src/*.cpp tests/test_core.cpp -o t && ./t   # فقط تست هستهٔ موتور
```

### استفاده از ایجنت هوش مصنوعی

```bash
hhmg-cli list
hhmg-cli call generate_midi --args '{"root":"F#","scale":"phrygian","bpm":142,"bars":4,"style_mix":{"trap":1},"output_path":"idea.mid"}'
printf 'dark city lights\nwe rise tonight\n' | hhmg-cli lyrics - --style trap --out lyrics.mid
hhmg-cli mcp          # سرور MCP روی stdio برای Claude Desktop، Cursor و ...
```

### محدودیت‌های صادقانه

- پلاگین با CI ساخته و تست واحد شده، اما هنوز در همهٔ DAWها بررسی نشده؛ رابط اندروید هم روی همهٔ دستگاه‌ها دیده نشده است.
- قابلیت‌های AI به کلید API خودتان نیاز دارند؛ تشخیص کلید/سبک و تبدیل صدا به نت تقریبی (heuristic) است و روی ورودی تمیز و تک‌صدا بهتر کار می‌کند.
- بانک تایپ‌بیت تا وقتی دادهٔ مجاز را جمع نکنید خالی است. هیچ صدایی ریپ یا بازتوزیع نمی‌شود.

### مستندات

- راهنمای فارسی: [docs/USER_GUIDE.fa.md](docs/USER_GUIDE.fa.md)
- اندروید: [docs/ANDROID.md](docs/ANDROID.md) · تایپ‌بیت: [docs/TYPE_BEATS.md](docs/TYPE_BEATS.md)
- ایجنت / MCP: [docs/AGENT_NATIVE.md](docs/AGENT_NATIVE.md) · معماری: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- دادهٔ دیتاست و مجوزها: [data/README.md](data/README.md)
