<div dir="rtl">

# HipHop MIDI Generator — راهنمای کاربر (فارسی)

## ۱. نصب

**نسخه آماده (خروجی CI):** در GitHub به مسیر *Actions ← Build VST3* بروید و `HipHopMidiGenerator-macOS-VST3.zip` یا
`HipHopMidiGenerator-Windows-VST3.zip` را دانلود و از حالت فشرده خارج کنید؛ سپس پوشه‌ی `.vst3` را کپی کنید:

| سیستم‌عامل | مسیر |
|---|---|
| macOS | `~/Library/Audio/Plug-Ins/VST3/` |
| Windows | `C:\Program Files\Common Files\VST3\` |

در macOS نسخه‌ی CI امضای موقت (ad-hoc) دارد. اگر Gatekeeper مانع شد: `xattr -dr com.apple.quarantine "HipHop MIDI Generator.vst3"`. سپس در DAW پلاگین‌ها را دوباره اسکن کنید.
پلاگین را روی یک ترک **Instrument** بارگذاری کنید (خروجی‌اش MIDI است نه صدا) و خروجی MIDI را به یک سینت وصل کنید، یا MIDI را با درگ بیرون ببرید (پایین‌تر).

## ۲. بیلد از سورس

نیازمندی‌ها: CMake نسخه ۳٫۲۲ به بالا، کامپایلر C++17 (Xcode 14+ یا Visual Studio 2022)، و اینترنت در اولین پیکربندی (JUCE نسخه ۸٫۰٫۶ دانلود می‌شود) یا `-DJUCE_PATH=/path/to/JUCE`.

```bash
./scripts/build.sh                 # macOS: باینری Universal (arm64 + x86_64) در dist/
.\scripts\build.ps1                # Windows x64 (در PowerShell ویژوال استودیو ۲۰۲۲)
cmake -S . -B build -DHHMG_BUILD_PLUGIN=OFF   # فقط موتور + CLI + تست‌ها (بدون JUCE)
```

گردش‌کار GitHub Actions (`.github/workflows/build.yml`) همه چیز را روی macOS و Windows و Linux بیلد و تست می‌کند.
با زدن تگ `v1.0.0` یک Release با هر دو فایل zip ساخته می‌شود.

## ۳. استفاده از پلاگین

1. **Key / Scale** — ۱۳ گام و مود: ماژور، مینور (طبیعی/هارمونیک/ملودیک)، دورین، فریژین، لیدین، میکسولیدین، لوکرین، فریژین دامیننت، پنتاتونیک ماژور/مینور و بلوز.
2. **Style** — نوبِ *R&B*، *Trap* و *Hip-Hop* سبک‌ها را با هم ترکیب می‌کنند (نرمال می‌شوند؛ یعنی ۱ / ۱ / ۰ = نصف R&B و نصف Trap).
3. **Complexity** — کم: سه‌صدایی‌ها و ملودی کم‌نت؛ زیاد: هفتم، نهم، یازدهم، واژگونی‌ها، وویسینگ باز و ملودی پرنت‌تر.
4. **Humanize / Swing / Vel Var** — شلی زمان‌بندی و وِلاسیتی، سوینگ ضعیف‌ضرب‌ها، پراکندگی وِلاسیتی.
5. **بخش‌ها** — Chords ، Melody ، 808 Bass. گزینه‌ی **Melody Oct** اکتاو ملودی را تعیین می‌کند. **Bars** بین ۱ تا ۳۲.
6. **BPM** — دستی، یا با *Host BPM* هماهنگ با DAW.
7. **GENERATE** — هر بار ایده‌ی تازه. عدد **Seed** نمایش داده می‌شود؛ با وارد کردن همان عدد و همان تنظیمات، دقیقاً همان نتیجه برمی‌گردد.
8. **Scale Lock** — MIDI زنده‌ای که وارد پلاگین می‌شود و فایل‌های MIDI واردشده به گام کشیده (snap) می‌شوند. خروجی تولیدشده توسط خود موتور همیشه، با هر تنظیمی، داخل گام است.
9. **پیانو رول** آکوردها (آبی)، ملودی (کهربایی) و ۸۰۸ (قرمز) را نشان می‌دهد؛ ردیف‌های متعلق به گام کمی روشن‌ترند.

### رساندن MIDI به DAW
- **Drag MIDI to DAW** — دکمه را روی ترک MIDI/Instrument یا آرنجمنت بکشید و رها کنید. در همه‌ی DAWها کار می‌کند.
- **خروجی MIDI** — با روشن بودن *Play with host* کلیپ همگام با ترنسپورت لوپ می‌شود؛ کانال ۱ آکورد، کانال ۲ ملودی، کانال ۳ ۸۰۸. فقط در DAWهایی که خروجی MIDI پلاگین VST3 را مسیردهی می‌کنند (Cubase، Reaper، Bitwig، Studio One، FL Studio). در Ableton Live کار نمی‌کند و Logic Pro اصلاً VST3 بارگذاری نمی‌کند.
- **رها کردن فایل .mid** روی پنجره، آن را وارد می‌کند (با Scale Lock روشن به گام کشیده می‌شود) و بعد می‌توانید دوباره درگ کنید.

### پریست‌ها
جعبه‌ی بالا-راست پریست‌های کارخانه‌ای (Dark Trap، Smooth R&B، Boom Bap، Trap Soul، Neo Soul) و پریست‌های شما را نشان می‌دهد. **Save** همه‌ی تنظیمات و پرامپت AI را در `~/Documents/HipHopMidiGenerator/Presets/*.hhpreset` ذخیره می‌کند. کلیپ تولیدشده هم داخل پروژه‌ی DAW ذخیره می‌شود.

## ۴. پرامپت متنی ← MIDI با هوش مصنوعی

مثلاً بنویسید: *«dark trap, F# phrygian, 142 BPM, eerie bell melody, sliding 808, a bit swung»* و **Ask AI** را بزنید.
مدل با فراخوانی ابزارهای پلاگین تنظیمات را انتخاب می‌کند؛ نت‌ها را خود موتور می‌نویسد و نوب‌ها آنچه را مدل انتخاب کرده نشان می‌دهند.

- **Gemini**: گزینه‌ی *Gemini* را انتخاب کنید و کلید API را بگیرید (https://aistudio.google.com/apikey) و وارد کنید، یا متغیر محیطی `GEMINI_API_KEY` را تنظیم کنید. مدل پیش‌فرض `gemini-2.5-flash`.
- **سازگار با OpenAI**: این گزینه را انتخاب کنید و *Endpoint* (مثلاً Ollama: `http://localhost:11434/v1` ، LM Studio: `http://localhost:1234/v1`) و *Model* (مثلاً `llama3.1`) و در صورت نیاز کلید را بدهید. مدل باید از tool calling پشتیبانی کند.
- کلید در فایل `settings.xml` در پوشه‌ی داده‌ی برنامه‌ی کاربر ذخیره می‌شود (نه داخل پروژه‌های DAW). پرامپت و تنظیمات فعلی به سرویسی که انتخاب کرده‌اید ارسال می‌شود.

### هوش مصنوعی پلاگین را می‌شناسد و راهنمایی می‌کند
هوش مصنوعی ابتدا ابزار `style_guide` را صدا می‌زند. این ابزار ۱۲ دستور سبک (ترپ تاریک/مرموز/ملودیک، ترپ سول، R&B نرم، نئوسول، R&B شبانه، بوم‌بپ، لوفای، وست‌کوست، سینمایی، R&B شاد)، حال‌وهوای هر گام و کار هر نوب را به فارسی و انگلیسی دارد. پاسخ به شما می‌گوید کدام تونالیته، گام، BPM و تنظیمات انتخاب شد، **چرا**، و چطور بیشتر تنظیمش کنید.

**فاین‌تیون:** اسکریپت `tools/finetune/make_dataset.py` دانش خود موتور را به یک دیتاست دوزبانه‌ی آموزشی تبدیل می‌کند (پرامپت ← آرگومان‌های `generate_midi` + توضیح) در قالب Gemini یا OpenAI: `python3 tools/finetune/make_dataset.py --cli build/hhmg-cli --format gemini --out hhmg_tuning.jsonl`. خود Job تیونینگ را باید در حساب Google Cloud یا ارائه‌دهنده‌ی خودتان اجرا کنید. بدون تیونینگ هم پرامپت سیستمی + `style_guide` همین دانش را به مدل می‌دهند.

## ۵. سرور MCP (استفاده از موتور در Claude، Gemini CLI، Cursor و ...)

برنامه‌ی `hhmg-cli` (در پوشه‌ی `dist/`) پروتکل MCP را روی stdio پشتیبانی می‌کند.

ابزارها: `style_guide`، `list_scales`، `scale_notes`، `snap_to_scale`، `check_in_scale`، `list_patterns`، `list_progressions`، `generate_midi`
(ابزار `generate_midi` پارامتر `output_path` را برای نوشتن فایل `.mid` می‌پذیرد).

```json
{
  "mcpServers": {
    "hhmg": {
      "command": "/absolute/path/to/hhmg-cli",
      "args": ["mcp", "--patterns", "/absolute/path/to/patterns.json"]
    }
  }
}
```
```bash
claude mcp add hhmg -- /absolute/path/to/hhmg-cli mcp --patterns /absolute/path/to/patterns.json    # Claude Code
echo '{"root":"A","scale":"minor","style":"trap","bars":4,"output_path":"/tmp/idea.mid"}' | hhmg-cli call generate_midi   # بدون MCP
```
آزمایش: `printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"tools/list"}' | hhmg-cli mcp`. گزینه‌ی `--patterns` اختیاری است.

## ۶. دیتاست (MIDI خودتان ← پروفایل سبک)

```bash
# پوشه‌ها و zip های حاوی .mid ؛ سبک از کلمات مسیر (rnb / trap / hip hop ...) حدس زده می‌شود یا با --style اجبار می‌شود
python3 tools/dataset/ingest.py ~/midi/rnb ~/midi/trap ~/Downloads/some_pack.zip --out patterns.json

# تلگرام (با حساب خودتان؛ توضیح در ابتدای اسکریپت)
pip install telethon
export TG_API_ID=... TG_API_HASH=...
python3 tools/dataset/telegram_fetch.py --channel pulseestore --message 2019 --album --out data/telegram
python3 tools/dataset/ingest.py data/telegram --out patterns.json
```
پروفایل **POP909** (مجوز MIT، فایل `data/patterns.pop909.json`) از قبل داخل پلاگین است. فایل `patterns.json` خودتان را در `~/Documents/HipHopMidiGenerator/patterns.json` بگذارید (هنگام اجرا بارگذاری می‌شود) یا از دکمه‌ی **Load dataset…** استفاده کنید. پروگرِشن‌ها، ریتم‌ها و موتیف‌های یادگرفته‌شده به مجموعه‌ی داخلی *اضافه* می‌شوند و بر اساس نوب‌های Style به کار می‌روند.
آنچه استخراج می‌شود: تونالیته (پروفایل‌های کرومهانسل)، درجه‌ی آکورد دیاتونیک برای هر میزان، الگوی شروع نت‌های ملودی در یک میزان، و کانتورهای کوتاه درجه‌ی گام. نت‌های اصلی ذخیره نمی‌شوند. فقط از موادی استفاده کنید که مجوزش را دارید؛ جزئیات در `docs/ARCHITECTURE.md`.

## ۷. عیب‌یابی
- *صدا نمی‌آید*: خروجی فقط MIDI است؛ یک ساز بعد از آن بگذارید یا MIDI را روی ترک سینت درگ کنید.
- *از خروجی MIDI چیزی پخش نمی‌شود*: میزبان باید در حال پخش باشد، *Play with host* روشن باشد و DAW باید خروجی MIDI پلاگین VST3 را مسیردهی کند.
- *خطای HTTP 400/403 در AI*: کلید و نام مدل را بررسی کنید؛ برای مدل محلی، آدرس Endpoint و پشتیبانی از tool calling را.
- بازنشانی کامل: پوشه‌ی `~/Documents/HipHopMidiGenerator/` را حذف کنید.

</div>
