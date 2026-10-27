#!/usr/bin/env python3
"""Objective melody 'maturity' metrics for generated MIDI (melody track = MIDI channel 2).

childish_runs : share of notes inside stepwise runs of 3+ in the same direction (scale-run / nursery-rhyme feel)
repeat_ratio  : share of consecutive equal pitches (hooky, trap-style repetition)
leap_ratio    : share of leaps >= 3 semitones
density       : melody notes per bar
palette       : distinct pitch classes per 4 bars (small = memorable hook)
rest_ratio    : share of the bar grid (16ths) with no melody note sounding

    python3 melody_metrics.py --cli ../../build/hhmg-cli --style trap --n 200
"""
import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "dataset"))
from midi_parse import parse_midi  # noqa: E402


def melody_notes(path):
    m = parse_midi(open(path, "rb").read())
    for t in m["tracks"]:
        if t and t[0][4] == 1:
            return sorted(t), m["ppq"]
    return [], m["ppq"]


def metrics(notes, ppq, bars):
    pitches = [n[2] for n in notes]
    if len(pitches) < 4:
        return None
    iv = [b - a for a, b in zip(pitches, pitches[1:])]
    in_run = set()
    for i in range(len(iv) - 1):
        if iv[i] != 0 and iv[i + 1] != 0 and (iv[i] > 0) == (iv[i + 1] > 0) and abs(iv[i]) <= 2 and abs(iv[i + 1]) <= 2:
            in_run.update({i, i + 1, i + 2})
    step = ppq / 4
    grid = set()
    for s, e, *_ in notes:
        for g in range(int(s // step), int(max(s + 1, e - 1) // step) + 1):
            grid.add(g)
    return {
        "childish_runs": len(in_run) / len(pitches),
        "repeat_ratio": sum(1 for d in iv if d == 0) / len(iv),
        "leap_ratio": sum(1 for d in iv if abs(d) >= 3) / len(iv),
        "density": len(pitches) / bars,
        "palette": len({p % 12 for p in pitches}) * (4 / max(bars, 1)) if bars else 0,
        "rest_ratio": 1 - len(grid) / (bars * 16),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", required=True)
    ap.add_argument("--style", default="trap", choices=["rnb", "trap", "hiphop"])
    ap.add_argument("--scale", default="minor")
    ap.add_argument("--n", type=int, default=100)
    ap.add_argument("--bars", type=int, default=4)
    ap.add_argument("--complexity", type=float, default=0.5)
    a = ap.parse_args()
    rows = []
    with tempfile.TemporaryDirectory() as d:
        for i in range(a.n):
            path = os.path.join(d, "x.mid")
            req = {"root": i % 12, "scale": a.scale, "style": a.style, "bars": a.bars, "seed": i + 1,
                   "chords": False, "melody": True, "humanize": 0.2, "complexity": a.complexity, "output_path": path}
            subprocess.run([a.cli, "call", "generate_midi"], input=json.dumps(req), capture_output=True, text=True, check=True)
            notes, ppq = melody_notes(path)
            r = metrics(notes, ppq, a.bars)
            if r:
                rows.append(r)
    print(f"{a.style}/{a.scale} n={len(rows)}  " + "  ".join(f"{k}={statistics.mean(r[k] for r in rows):.2f}" for k in rows[0]))


if __name__ == "__main__":
    main()
