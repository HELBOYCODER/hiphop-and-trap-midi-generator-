# HipHop MIDI Generator

**EN** — A cross-platform **VST3** MIDI generator specialised in **Trap and Hip-Hop** (plus R&B): chords, melodies and 808 lines that are
**guaranteed to stay inside the chosen scale**, shaped by style blending, complexity, swing and humanisation, with a
text-prompt AI mode (Gemini or any OpenAI-compatible model) and an MCP server.

**FA** — یک پلاگین **VST3** چندسکویی برای تولید MIDI در سبک‌های **R&B، Trap و Hip-Hop**: آکورد، ملودی و خط ۸۰۸ که
**قطعاً داخل گام انتخاب‌شده می‌مانند**؛ با ترکیب سبک‌ها، پیچیدگی، سوینگ، انسان‌سازی، حالت پرامپت متنی (Gemini یا هر مدل سازگار با OpenAI) و سرور MCP.

| | |
|---|---|
| Plugin | JUCE 8 · VST3 · macOS (arm64 + x86_64 universal) · Windows x64 |
| Engine | dependency-free C++17 core (`core/`), deterministic per seed |
| Scale safety | 13 scales/modes; verified by `tests/test_core.cpp` (7 020 generations, 0 wrong notes) |
| Dataset | `tools/dataset/` — MIDI folders / zips / Telegram → `patterns.json` |
| AI | Gemini function-calling + OpenAI-compatible (Ollama, LM Studio…) |
| MCP | `hhmg-cli mcp` — stdio MCP server, same tools the plugin's AI uses |

## Documentation · مستندات

- English: [docs/USER_GUIDE.en.md](docs/USER_GUIDE.en.md) (install, build, MCP, usage, dataset)
- فارسی: [docs/USER_GUIDE.fa.md](docs/USER_GUIDE.fa.md) (نصب، بیلد، تنظیمات MCP، نحوه استفاده، دیتاست)
- Architecture & limits / معماری و محدودیت‌ها: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

## Quick start

```bash
./scripts/build.sh            # macOS / Linux  -> dist/*.vst3  (runs the tests)
.\scripts\build.ps1           # Windows x64 (VS 2022 PowerShell)
```

Binaries for macOS and Windows are produced by the GitHub Actions workflow
(`.github/workflows/build.yml`) — download them from the run's **Artifacts**, or push a `v*` tag to publish a release.
