# HipHop MIDI Generator — User Guide (English)

## 1. Install

**Pre-built (from CI):** open the repository's *Actions → Build VST3* run and download
`HipHopMidiGenerator-macOS-VST3.zip` or `HipHopMidiGenerator-Windows-VST3.zip`, unzip, then copy the `.vst3` bundle to:

| OS | Folder |
|---|---|
| macOS | `~/Library/Audio/Plug-Ins/VST3/` (or `/Library/Audio/Plug-Ins/VST3/`) |
| Windows | `C:\Program Files\Common Files\VST3\` |

macOS: the CI build is ad-hoc signed. If Gatekeeper blocks it: `xattr -dr com.apple.quarantine "HipHop MIDI Generator.vst3"`. Rescan plugins in your DAW.
In your DAW load **HipHop MIDI Generator** as an *instrument* track (it makes MIDI, not audio) and route its MIDI output to a synth — or just drag the MIDI out (below).

## 2. Build from source

Requirements: CMake ≥ 3.22, a C++17 compiler (Xcode 14+ / Visual Studio 2022), internet access for the first configure (JUCE 8.0.6 is fetched), or `-DJUCE_PATH=/path/to/JUCE`.

```bash
./scripts/build.sh                 # macOS: universal arm64+x86_64 -> dist/*.vst3
.\scripts\build.ps1                # Windows x64 (VS 2022 PowerShell) -> dist\*.vst3
cmake -S . -B build -DHHMG_BUILD_PLUGIN=OFF   # engine + CLI + tests only, no JUCE needed
```

GitHub Actions (`.github/workflows/build.yml`) builds and tests everything on macOS, Windows and Linux. Push a tag `v1.0.0` to publish a release with both zips.

## 3. Using the plugin

1. **Key / Scale** — 13 scales and modes (major, natural/harmonic/melodic minor, dorian, phrygian, lydian, mixolydian, locrian, phrygian dominant, major/minor pentatonic, blues).
2. **Style** — *R&B*, *Trap*, *Hip-Hop* knobs blend the three styles (they are normalised, so 1/1/0 = half R&B, half Trap).
3. **Complexity** — low: triads, sparse melody; high: 7ths/9ths/11ths, inversions, open voicings, denser melody.
4. **Humanize / Swing / Vel Var** — timing & velocity looseness, off-beat swing, velocity spread.
5. **Parts** — Chords, Melody, 808 Bass. **Melody Oct** places the melody. **Bars** 1–32.
6. **BPM** — manual, or *Host BPM* to follow the DAW.
7. **GENERATE** — a new idea each press. The **Seed** is shown; type it back to recall an idea (same settings + seed = identical result).
8. **Scale Lock** — live MIDI played into the plugin and imported MIDI files are snapped into the scale. Generated output is always in scale regardless.
9. The **piano roll** shows chords (blue), melody (amber) and 808 (red); rows that belong to the scale are tinted.

### Getting MIDI into the DAW
- **Drag MIDI to DAW** — drag the button onto a MIDI/instrument track or arrangement. Works in every DAW.
- **MIDI output** — with *Play with host* on, the clip loops in sync with the transport on MIDI channels 1 (chords), 2 (melody), 3 (808). Works where the host routes VST3 MIDI output (Cubase, Reaper, Bitwig, Studio One, FL Studio). Not Ableton Live; Logic Pro does not load VST3.
- **Drop a .mid file** onto the window to import (snapped to scale when Scale Lock is on) — then re-drag it out.

### Presets
The top-right box lists factory presets (Dark Trap, Smooth R&B, Boom Bap, Trap Soul, Neo Soul) and your own. **Save** stores all settings plus the AI prompt in `~/Documents/HipHopMidiGenerator/Presets/*.hhpreset`. The generated clip is also saved inside your DAW project.

## 4. AI prompt → MIDI

Type e.g. *"dark trap, F# phrygian, 142 BPM, eerie bell melody, sliding 808, a bit swung"* and press **Ask AI**.
The model picks settings by calling the plugin's tools; the engine writes the notes, and the knobs update to show what it chose.

- **Gemini**: choose *Gemini*, paste an API key (https://aistudio.google.com/apikey) or set the `GEMINI_API_KEY` environment variable. Model default `gemini-2.5-flash`.
- **OpenAI-compatible**: choose it, set *Endpoint* (e.g. Ollama `http://localhost:11434/v1`, LM Studio `http://localhost:1234/v1`), *Model* (e.g. `llama3.1`) and, if needed, a key. The model must support tool calling.
- The key is saved in `settings.xml` in your user application-data folder (not in DAW projects). Your prompt and the current settings are sent to the provider you choose.

### The AI knows the plugin (and explains itself)
The AI calls the `style_guide` tool first. It holds 12 style recipes (dark/eerie/melodic trap, trap soul, smooth R&B, neo soul, late-night R&B, boom bap, lo-fi, west coast, cinematic, bright R&B), the character of each scale and what every knob does, in English and Persian. The answer tells you which key/scale/BPM/settings were chosen, **why**, and how to tune it further.

**Fine-tuning:** `tools/finetune/make_dataset.py` turns the engine's own knowledge into a bilingual supervised-tuning set (prompt → `generate_midi` arguments + explanation), in Gemini or OpenAI chat format: `python3 tools/finetune/make_dataset.py --cli build/hhmg-cli --format gemini --out hhmg_tuning.jsonl`. Run the tuning job in your own Google Cloud / provider account. Without tuning, the system prompt + `style_guide` already give the model this knowledge.

## 5. MCP server (use the engine from Claude, Gemini CLI, Cursor…)

`hhmg-cli` (built with the project; in `dist/`) speaks the Model Context Protocol over stdio.

Tools: `style_guide`, `list_scales`, `scale_notes`, `snap_to_scale`, `check_in_scale`, `list_patterns`, `list_progressions`, `generate_midi`
(`generate_midi` accepts `output_path` to write a `.mid`).

```jsonc
// Claude Desktop (claude_desktop_config.json) / Gemini CLI & Cursor (mcpServers) — same shape
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
claude mcp add hhmg -- /absolute/path/to/hhmg-cli mcp --patterns /absolute/path/to/patterns.json   # Claude Code
echo '{"root":"A","scale":"minor","style":"trap","bars":4,"output_path":"/tmp/idea.mid"}' | hhmg-cli call generate_midi   # no MCP
```
Test: `printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"tools/list"}' | hhmg-cli mcp`. `--patterns` is optional.

## 6. Datasets (your MIDI → style profile)

```bash
# folders and zips of .mid files; style is inferred from path words (rnb / trap / hip hop...) or forced with --style
python3 tools/dataset/ingest.py ~/midi/rnb ~/midi/trap ~/Downloads/some_pack.zip --out patterns.json

# Telegram (needs your own login; see the script header)
pip install telethon
export TG_API_ID=... TG_API_HASH=...
python3 tools/dataset/telegram_fetch.py --channel pulseestore --message 2019 --album --out data/telegram
python3 tools/dataset/ingest.py data/telegram --out patterns.json
```
The plugin already contains the **POP909** profile (MIT, `data/patterns.pop909.json`, see `data/README.md`). Put your own `patterns.json` at `~/Documents/HipHopMidiGenerator/patterns.json` (loaded on start) or use **Load dataset…** in the plugin. Learned progressions, rhythms and motifs are *added* to the built-in set and used according to the Style knobs.
What is extracted: key (Krumhansl profiles), a diatonic chord degree per bar, one-bar onset grids of the melody, and short scale-degree contours. Original notes are not stored. Use material you are licensed to use; see `docs/ARCHITECTURE.md`.

## 7. Troubleshooting
- *No sound*: it outputs MIDI only — put an instrument after it, or drag the MIDI to a synth track.
- *Nothing plays from MIDI out*: the host must be playing, *Play with host* on, and the DAW must route VST3 MIDI output (see above).
- *AI error HTTP 400/403*: check the key / model name; for local models, check the endpoint and tool-calling support.
- Reset everything: delete `~/Documents/HipHopMidiGenerator/`.
