---
name: hhmg
description: Make, analyse and transform MIDI for trap / hip-hop / R&B with the HipHop MIDI Generator engine (scale-locked chords, melody, 808; voice or audio to MIDI; lyrics or poems to melody; style recognition; "similar" ideas drawn from a reference dataset). Use when the user wants MIDI, a beat idea, a melody from a hum or lyrics, or to know what style/key a MIDI file is.
---

# hhmg — agent-native MIDI engine

Everything is a JSON tool. No GUI is needed. Two equivalent ways to call it:

1. **CLI** (any agent with a shell): `hhmg-cli list` → `hhmg-cli describe <tool>` → `hhmg-cli call <tool> --args '{...}'`
2. **MCP** (stdio): `hhmg-cli mcp` exposes the same tools to any MCP client.

All output is JSON; exit code 0 means `"ok": true`.

## Workflow
1. `hhmg-cli list` and read the tool names. Do not guess names or ids.
2. Pick the path:
   - From scratch: `generate_midi` (`root`, `scale`, `bpm`, `bars`, `style_mix {rnb,trap,hiphop}`, `complexity`, `humanize`, `swing`, `chords`/`melody`/`bass`, `seed`, `output_path`).
   - From a MIDI file: `analyze_midi {path}` → key, mode, BPM, chord degrees, style scores.
   - From a recording: `voice_to_midi {path (.wav), mode: melody|melody_plus|similar}` or `analyze_audio`.
   - From lyrics / a poem (any language): `lyrics_to_midi {lyrics, root, scale, style}` or `hhmg-cli lyrics <file|->`.
   - Like an artist/vibe: `find_type_beat_style {query}` (aggregate statistics; check `sample_size`), then `generate_midi` with its suggested settings.
   - Real loops as reference: `find_references`, then pass `reference_progression` / `reference_rhythm` to `generate_midi`.
3. Pass `output_path` to get a `.mid` file; the response also reports `out_of_scale_notes` (always 0 — notes are snapped to the scale by construction).
4. Tell the user what was recognised or combined (key/scale/BPM/chords/references) in a few lines.

## Rules
- Never write note numbers yourself; call the tools. The engine keeps every note in the chosen scale.
- `type_beat` / reference data is aggregate metadata. Do not claim a beat was copied or downloaded.
- Datasets: `hhmg-cli index <dir> --out user_library.json` builds profiles (no notes stored) from the user's own MIDI; load with `--user-library`.
- Be honest about uncertainty: style/key detection is heuristic.

## Making other software agent-native
The pattern used here (see docs/AGENT_NATIVE.md): put the program's real logic behind a headless core, describe each capability as a JSON-schema tool, expose it as a CLI (`list`/`describe`/`call`, JSON out, exit codes) and as an MCP server, and ship a SKILL.md like this one. For GUI-only apps or websites, drive their import/export, scripting or automation interface instead of pixels.
