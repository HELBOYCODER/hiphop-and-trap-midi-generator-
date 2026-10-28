#!/usr/bin/env python3
"""Turn a folder / zip of MIDI files into a compact pattern library (patterns.json).

Only abstract musical statistics are stored (chord-degree loops, one-bar onset grids and short
scale-degree contours) - never the original note data - so the output is a style profile, not a copy.

    python3 ingest.py ~/midi/rnb ~/midi/trap packs/cymatics.zip --out patterns.json
    python3 ingest.py ~/midi/trap --style trap --max-per-kind 60
"""
import argparse
import collections
import io
import json
import math
import os
import re
import sys
import zipfile
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from midi_parse import MidiError, parse_midi  # noqa: E402

# Krumhansl-Kessler key profiles
MAJOR = [6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88]
MINOR = [6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17]
SCALES = {"major": [0, 2, 4, 5, 7, 9, 11], "minor": [0, 2, 3, 5, 7, 8, 10]}
STYLE_WORDS = {
    "rnb": ("rnb", "r&b", "r_b", "soul", "neo"),
    "trap": ("trap", "drill", "808", "dark", "migos", "future"),
    "hiphop": ("hip", "hop", "boom", "lofi", "lo-fi", "boombap", "rap"),
}


def correlate(x, y):
    mx, my = sum(x) / 12, sum(y) / 12
    num = sum((a - mx) * (b - my) for a, b in zip(x, y))
    den = math.sqrt(sum((a - mx) ** 2 for a in x) * sum((b - my) ** 2 for b in y))
    return num / den if den else 0


def estimate_key(notes):
    hist = [0.0] * 12
    for s, e, p, v, ch in notes:
        hist[p % 12] += (e - s)
    best = (-2, 0, "major")
    for root in range(12):
        rot = hist[root:] + hist[:root]
        for mode, prof in (("major", MAJOR), ("minor", MINOR)):
            c = correlate(rot, prof)
            if c > best[0]:
                best = (c, root, mode)
    return best[1], best[2]


KEY_RE = re.compile(r"[-_ ]([A-G])([#b]?)\s*(Maj(?:or)?|Min(?:or)?)\b", re.IGNORECASE)
PC = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def key_from_name(name):
    """Many sample packs (Cymatics...) state the key in the file name, e.g. 'MIDI 34 - G Min'. That beats
    estimating it: relative major/minor pairs are ambiguous to a histogram but not to the producer."""
    m = KEY_RE.search(os.path.splitext(os.path.basename(name.split("!")[-1]))[0])
    if not m:
        return None
    pc = (PC[m.group(1).upper()] + (1 if m.group(2) == "#" else -1 if m.group(2) == "b" else 0)) % 12
    return pc, "minor" if m.group(3).lower().startswith("min") else "major"


def infer_style(path, default):
    low = path.lower()
    for style, words in STYLE_WORDS.items():
        if any(w in low for w in words):
            return style
    return default


def iter_midi_blobs(paths):
    for root in paths:
        if os.path.isfile(root) and root.lower().endswith(".zip"):
            with zipfile.ZipFile(root) as z:
                for name in z.namelist():
                    if name.lower().endswith((".mid", ".midi")):
                        yield root + "!" + name, z.read(name)
        elif os.path.isfile(root):
            with open(root, "rb") as f:
                yield root, f.read()
        else:
            for d, _, files in os.walk(root):
                for fn in files:
                    full = os.path.join(d, fn)
                    if fn.lower().endswith((".mid", ".midi")):
                        with open(full, "rb") as f:
                            yield full, f.read()
                    elif fn.lower().endswith(".zip"):
                        yield from iter_midi_blobs([full])


def bar_ticks(ppq):
    return ppq * 4


def split_roles(tracks, ppq):
    """Return (chord_notes, [melody_track_notes, ...]). Drum channel 10 is ignored.

    Onsets within a 32nd note count as simultaneous, so strummed / humanized chords still read as chords.
    """
    chord, melody = [], []
    tol = max(1, ppq // 8)
    for notes in tracks:
        notes = [n for n in notes if n[4] != 9]
        if len(notes) < 4:
            continue
        onsets = collections.Counter(n[0] // tol for n in notes)
        avg_poly = sum(onsets.values()) / len(onsets)
        mean_pitch = sum(n[2] for n in notes) / len(notes)
        if avg_poly >= 2.3:
            chord.append(notes)
        elif mean_pitch >= 52 and avg_poly < 1.4:
            melody.append(notes)
    return sorted(n for g in chord for n in g), melody  # melody stays a list of tracks


def chord_degrees(chord_notes, ppq, root, mode):
    """One diatonic degree (1-7) per bar by matching bar pitch classes to diatonic triads."""
    if not chord_notes:
        return []
    bt = bar_ticks(ppq)
    nbars = max(n[0] for n in chord_notes) // bt + 1
    scale = SCALES[mode]
    out = []
    for b in range(min(nbars, 64)):
        weights = [0.0] * 12
        for s, e, p, v, ch in chord_notes:
            lo, hi = max(s, b * bt), min(e, (b + 1) * bt)
            if hi > lo:
                weights[(p - root) % 12] += hi - lo
        best, best_score = None, 0.0
        for deg in range(7):
            tones = {scale[deg], scale[(deg + 2) % 7], scale[(deg + 4) % 7]}
            score = sum(weights[t] for t in tones) - 0.5 * sum(w for i, w in enumerate(weights) if i not in tones)
            if score > best_score:
                best, best_score = deg + 1, score
        out.append(best)
    return out


def rhythm_bars(melody_notes, ppq):
    bt, st = bar_ticks(ppq), ppq / 4
    bars = collections.defaultdict(list)
    for s, e, p, v, ch in melody_notes:
        bars[s // bt].append((int(round((s % bt) / st)) % 16, max(1, min(16, int(round((e - s) / st)))), v))
    for _, notes in sorted(bars.items()):
        seen, uniq = set(), []
        for step, ln, v in notes:
            if step not in seen:
                seen.add(step)
                uniq.append([step, ln, max(60, min(110, v))])
        if 2 <= len(uniq) <= 10:
            yield uniq


def contours(melody_notes, root, mode, size=7):
    scale = SCALES[mode]
    degs = []
    for s, e, p, v, ch in melody_notes:
        rel = (p - root) % 12
        idx = min(range(7), key=lambda i: min(abs(scale[i] - rel), 12 - abs(scale[i] - rel)))
        degs.append(idx + 7 * ((p - root) // 12))
    steps = [max(-4, min(4, b - a)) for a, b in zip(degs, degs[1:])]
    for i in range(0, max(0, len(steps) - size), 3):
        win = steps[i:i + size]
        win = win[:size - 1]
        flips = sum(1 for a, b in zip(win, win[1:]) if a * b < 0)
        if (any(win) and sum(1 for w in win if abs(w) >= 4) <= 1  # singable leaps only
                and flips <= 2                                     # no trills
                and len(set(win)) >= 3):                           # no scale runs / repeated-note filler
            yield [0] + win


def pack_of(name, inputs):
    """Top-level folder / archive the file came from (for provenance in reference profiles)."""
    for root in inputs:
        base = root.rstrip("/")
        if name.startswith(base):
            rel = name[len(base):].lstrip("/!")
            parts = rel.split("/")
            return (parts[0] if len(parts) > 1 else os.path.basename(base)) or os.path.basename(base)
    return "unknown"


def make_reference(name, pack, style, root, mode, bpm, degs, melody_tracks, ppq, has_tempo, chord_notes=()):
    """One abstract per-file profile: no notes, only what the AI needs to find and combine real examples."""
    lead = max(melody_tracks, key=len) if melody_tracks else []
    if not lead and chord_notes:  # chord-only loop: its hit pattern is the rhythm (one entry per chord onset)
        tol, seen, lead = max(1, ppq // 8), set(), []
        for n in sorted(chord_notes):
            if n[0] // tol not in seen:
                seen.add(n[0] // tol)
                lead.append(n)
    bt, st = ppq * 4, ppq / 4
    nbars = max(1, max((n[1] for n in lead), default=0) // bt + 1) if lead else max(1, len(degs))
    swing_offsets = []
    for s0, e0, p0, v0, c0 in lead:
        step = s0 / st
        nearest = round(step)
        if nearest % 2 and abs(step - nearest) < 0.45:
            swing_offsets.append(max(0.0, step - nearest))
    swing = min(1.0, (sum(swing_offsets) / len(swing_offsets)) / 0.55) if swing_offsets else 0.0
    rhythm = max(rhythm_bars(lead, ppq), key=len, default=[]) if lead else []
    seq = [d for d in degs if d][:16]
    stem = os.path.splitext(os.path.basename(name.split("!")[-1]))[0]
    slug = re.sub(r"[^a-z0-9]+", "", pack.lower().replace("cymatics", ""))[:10] or "ref"
    rid = slug + "-" + format(zlib.crc32(name.encode()) & 0xFFFFFF, "06x")
    bpm_val = round(60000000 / bpm, 1) if has_tempo and bpm else 0
    if bpm_val:  # tempo heuristic overrides the folder-level assumption (stored as measured, not labelled by the pack)
        style = "trap" if bpm_val >= 128 else "hiphop" if 82 <= bpm_val < 128 else ("rnb" if mode == "major" else "trap")
    return {"id": rid, "pack": pack, "name": stem, "style": style, "root": root, "mode": mode,
            "bpm": bpm_val, "bars": nbars, "degrees": seq,
            "density": round(len(lead) / nbars, 2), "swing": round(swing, 2),
            "rhythm": [list(n) for n in rhythm]}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("inputs", nargs="+", help="folders, .zip packs or .mid files")
    ap.add_argument("--out", default="patterns.json")
    ap.add_argument("--style", choices=["rnb", "trap", "hiphop"], help="force style for all inputs")
    ap.add_argument("--default-style", default="hiphop", choices=["rnb", "trap", "hiphop"],
                    help="style when it cannot be inferred from the path (default hiphop)")
    ap.add_argument("--max-per-kind", type=int, default=80, help="cap per (style, kind)")
    ap.add_argument("--min-count", type=int, default=1, help="drop progressions seen fewer times")
    ap.add_argument("--references", action="store_true",
                    help="also write per-file profiles (key, BPM, chord degrees, rhythm; no notes) for AI lookup")
    args = ap.parse_args()

    prog_counts = collections.Counter()
    rhythm_counts = collections.Counter()
    motif_counts = collections.Counter()
    files = skipped = 0
    references = []

    for name, blob in iter_midi_blobs(args.inputs):
        try:
            midi = parse_midi(blob)
        except (MidiError, IndexError, struct_error()) as exc:
            skipped += 1
            print(f"skip {name}: {exc}", file=sys.stderr)
            continue
        style = args.style or infer_style(name, args.default_style)
        chord, melody = split_roles(midi["tracks"], midi["ppq"])
        all_notes = sorted(n for t in midi["tracks"] for n in t if n[4] != 9)
        if len(all_notes) < 8:
            skipped += 1
            continue
        files += 1
        root, mode = key_from_name(name) or estimate_key(all_notes)
        degs = chord_degrees(chord, midi["ppq"], root, mode)
        if args.references and len([d for d in degs if d]) >= 2:
            references.append(make_reference(name, pack_of(name, args.inputs), style, root, mode,
                                             midi["tempos"][0][1], degs, melody, midi["ppq"], midi.get("has_tempo", False), chord))
        for n in (4, 2):
            seq = [d for d in degs if d]
            for i in range(0, max(0, len(seq) - n + 1), n):
                win = tuple(seq[i:i + n])
                if len(win) == n and len(set(win)) > 1:
                    prog_counts[(style, mode, win)] += 1
        for track in melody:  # never merge voices: interleaving MELODY + BRIDGE fakes huge leaps
            for r in rhythm_bars(track, midi["ppq"]):
                rhythm_counts[(style, tuple(tuple(x) for x in sorted(r)))] += 1
            for m in contours(track, root, mode):
                motif_counts[(style, tuple(m))] += 1

    out = {"rhythms": [], "motifs": [], "progressions": [],
           "meta": {"files_used": files, "files_skipped": skipped}}

    def top(counter, key_style):
        per = collections.defaultdict(list)
        for key, c in counter.most_common():
            per[key_style(key)].append((key, c))
        for style, items in per.items():
            yield from items[:args.max_per_kind]

    long_loops = collections.Counter({k: c for k, c in prog_counts.items() if len(k[2]) >= 4})
    short_loops = collections.Counter({k: c for k, c in prog_counts.items() if len(k[2]) < 4})
    for counter, cap in ((long_loops, args.max_per_kind), (short_loops, max(4, args.max_per_kind // 6))):
        per = collections.defaultdict(list)
        for key, c in counter.most_common():
            if c >= args.min_count:
                per[(key[0], key[1])].append((key, c))
        for items in per.values():
            for (style, mode, win), c in items[:cap]:
                out["progressions"].append({"style": style, "mode": mode, "degrees": list(win), "count": c})
    for i, ((style, notes), c) in enumerate(top(rhythm_counts, lambda k: k[0])):
        out["rhythms"].append({"name": f"learned_{style}_{i}", "style": style, "bars": 1,
                               "notes": [list(n) for n in notes], "count": c})
    for (style, steps), c in top(motif_counts, lambda k: k[0]):
        out["motifs"].append({"style": style, "steps": list(steps), "count": c})

    if args.references:
        out["references"] = references
    with open(args.out, "w") as f:
        json.dump(out, f, indent=1)
    print(f"{files} files used, {skipped} skipped -> {args.out}: "
          f"{len(out['progressions'])} progressions, {len(out['rhythms'])} rhythms, {len(out['motifs'])} motifs")


def struct_error():
    import struct
    return struct.error


if __name__ == "__main__":
    main()
