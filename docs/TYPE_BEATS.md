# Type-beat style bank / بانک سبک تایپ‌بیت

`find_type_beat_style` answers "what does a *Metro Boomin type beat* (or *dark drill*, *melodic trap*...) usually look like?" -
BPM range, minor/major share, common keys, tags - and returns matching `generate_midi` settings. It works in the VST3 plugin, the
Android app and the MCP server, and the AI calls it on its own when you name an artist or vibe.

## What is in it (and what is not)
Only **aggregate statistics from public listing metadata** (titles/descriptions/tags): count, BPM p10/median/p90, minor share, top keys, tags.
No audio, no MIDI, no link or title of any single beat, nothing copied from a beat. It tells the generator *where to aim*
(tempo, key mood, density), it does not imitate a particular track. The bank ships **empty**: no numbers are invented. Fill it yourself:

```bash
# 1) official YouTube Data API v3 (you provide the key)
python3 tools/typebeats/harvest.py --youtube-key $YT_KEY --query "metro boomin type beat" --query "dark drill type beat" \
    --query "travis scott type beat" --min-listings 8 --out data/typebeat_styles.json --merge

# 2) any JSONL export: {"title": "...", "description": "...", "tags": [...]} per line
python3 tools/typebeats/harvest.py --jsonl listings.jsonl --out data/typebeat_styles.json --merge

# 3) any website via an MCP server, e.g. a Pomerado integration (see below)
python3 tools/typebeats/harvest.py --mcp "node ./my-marketplace/mcp.mjs" --mcp-tool search_beats \
    --mcp-args '{"query":"{q}"}' --query "melodic trap type beat" --out data/typebeat_styles.json --merge
```
Use it in the apps: rebuild (the file is embedded) or copy `typebeat_styles.json` to `~/Documents/HipHopMidiGenerator/` (plugin loads it at start).
CLI/MCP: `hhmg-cli mcp --patterns data/typebeat_styles.json`.

## Turning "any site" into an API with Pomerado
[Pomerado](https://github.com/Pomerado/pomerado) (MIT) is an MCP-minting harness: you describe a task in plain language and it generates an
MCP server for a website, even one without an API. It needs its own LLM key and a browser, and it is not part of this repo.
Mint an integration for a site you are allowed to read (listing search -> title, description, tags), then pass its MCP command to
`harvest.py --mcp ...` as shown above. Rules of the road: check the site's terms of service and robots rules, rate-limit yourself, prefer official
APIs (YouTube Data API, partner feeds), and never download or re-host the audio. Marketplaces such as BeatStars/Airbit restrict scraping in their terms;
that is your call to make and your responsibility.

## بانک سبک تایپ‌بیت (فارسی)
ابزار `find_type_beat_style` می‌گوید «تایپ‌بیت فلان آرتیست یا حال‌وهوا» معمولاً چه BPM، چه حال کلید (مینور/ماژور)، چه کلیدها و تگ‌هایی دارد و تنظیمات
متناظر `generate_midi` را می‌دهد؛ در پلاگین، اپ اندروید و سرور MCP، و AI خودش وقتی اسم آرتیست یا حال‌وهوا بیاید صدایش می‌زند.
فقط **آمار تجمیعی از متادیتای عمومی** ذخیره می‌شود (تعداد، BPM، سهم مینور، کلیدها، تگ‌ها)؛ نه صدا، نه MIDI، نه لینک یا عنوان بیت‌ها.
بانک **خالی** شروع می‌شود و عددی ساختگی ندارد؛ با دستورهای بالا (YouTube API، فایل JSONL، یا MCP ساخته‌شده با Pomerado) خودتان پرش کنید.
شرایط استفاده‌ی هر سایت را رعایت کنید؛ بعضی بازارچه‌ها اسکرپ را در قوانینشان ممنوع کرده‌اند و مسئولیتش با شماست. صدای بیت‌ها را هرگز دانلود یا بازنشر نکنید.
