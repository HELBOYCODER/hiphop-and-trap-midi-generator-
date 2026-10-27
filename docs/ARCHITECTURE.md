# Architecture and honest limits / معماری و محدودیت‌ها

```
core/           dependency-free C++17
  Theory        scales, chord stacking, snap-to-scale
  Patterns      rhythm / motif / progression library (+ built-in seed set)
  Generator     chords, melody, 808; swing + humanize + velocity
  MidiFile      SMF format-1 writer
  Tools         tool layer (JSON in/out) shared by plugin AI and MCP
cli/            hhmg-cli: `tools`, `call <tool>`, `mcp` (stdio MCP server)
plugin/         JUCE VST3: processor (transport-locked MIDI out), editor, AI bridge, presets
tools/dataset/  midi_parse.py, ingest.py, telegram_fetch.py
tests/, plugin/Tests/, tools/dataset/tests/
```

## How "never out of key" is enforced

1. Chords are stacked thirds **on the scale itself** (for pentatonic/blues scales: on the parent 7-note scale), so every chord tone is a scale tone by construction.
2. Melody notes are picked as *scale degrees*, never as semitones; strong beats are pulled onto chord tones, phrases end on the tonic / a chord tone.
3. A final `snap` pass moves any note outside the scale set to the nearest member — it is an identity operation on normal output and a safety net otherwise.
4. `countOutOfScale()` verifies the result; `tests/test_core.cpp` runs it for every scale × 12 roots × 5 style blends × 3 complexities × seeds, and the processor test re-checks the MIDI that actually leaves the plugin.
5. The AI never writes notes. It can only call tools; `generate_midi` runs the same engine (and strips `output_path`, so a model cannot write files from inside your DAW).

Pentatonic/blues note: the melody uses the pentatonic/blues set; chords come from the parent major/minor scale, which is a superset (except the blues ♭5, which only the melody may use).

## What is *not* in the box (please read)

- **Built-in dataset.** POP909 (MIT) was ingested (2 897 files) and is compiled in as `data/patterns.pop909.json`: 140 progressions, 60 rhythms, 60 motifs, tagged `rnb`. It is *pop*, not hip-hop/trap, so it mostly improves R&B-leaning chords and melodies; trap/hip-hop quality will improve once you supply licensed hip-hop MIDI (e.g. the Cymatics packs you have a licence for - upload them or drop them in `data/raw/` and run `ingest.py`). I did not scrape third-party re-upload sites.
- **Fine-tuning.** `tools/finetune/` prepares the training data; the tuning job itself was not run here.
- **Dataset content.** The Telegram post and Cymatics packs are third-party material. The repo ships a hand-written seed library (≈21 rhythms, 10 motifs, 24 progressions) and the tooling to analyse files **you** obtain; the plugin stores only abstract statistics from them, and `data/telegram`/`data/raw` are git-ignored. The public `t.me` preview exposes no files, so `telegram_fetch.py` signs in with your own Telegram account (untested against Telegram from this build environment). Cymatics packs must be downloaded from Cymatics.
- **Compiled macOS/Windows binaries.** They are built by CI on macOS/Windows runners, not committed. The Linux build in this repo's development environment compiled and passed all tests, but macOS/Windows jobs have to run on GitHub. macOS builds are ad-hoc signed; for distribution add your Apple Developer ID signing + notarization.
- **MIDI output depends on the DAW.** VST3 MIDI output works in Cubase, Reaper, Bitwig, Studio One, FL Studio (route via MIDI out). **Ableton Live ignores MIDI output of VST3 instruments, and Logic Pro does not load VST3 at all (AU only; no AU build is included)** — in Ableton use *Drag MIDI to DAW*.
- **"Learning".** The dataset layer is statistical pattern extraction (key estimation, per-bar chord degrees, onset grids, contours), not a trained neural model; the LLM only chooses settings via tools.
- No audio is produced (it is a MIDI tool); route its MIDI to an instrument.
