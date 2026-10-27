import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
from midi_parse import parse_midi  # noqa: E402
import ingest  # noqa: E402

PPQ = 480


def vlq(v):
    out = [v & 0x7F]
    while v >> 7:
        v >>= 7
        out.append((v & 0x7F) | 0x80)
    return bytes(reversed(out))


def make_midi(chord_degrees_roots, melody):
    """Format-1 file: tempo track, a chord track (one triad per bar) and a melody track."""
    def track(events):
        events.sort()
        body, last = b"", 0
        for tick, data in events:
            body += vlq(tick - last) + data
            last = tick
        body += b"\x00\xff\x2f\x00"
        return b"MTrk" + struct.pack(">I", len(body)) + body

    tempo = track([(0, b"\xff\x51\x03\x07\xa1\x20")])
    chords = []
    for bar, (root, minor) in enumerate(chord_degrees_roots):
        for iv in (0, 3 if minor else 4, 7):
            chords.append((bar * 4 * PPQ, bytes([0x90, 48 + root + iv, 80])))
            chords.append((bar * 4 * PPQ + 4 * PPQ - 10, bytes([0x80, 48 + root + iv, 0])))
    mel = []
    for tick, pitch, length in melody:
        mel.append((tick, bytes([0x91, pitch, 90])))
        mel.append((tick + length, bytes([0x81, pitch, 0])))
    head = b"MThd" + struct.pack(">IHHH", 6, 1, 3, PPQ)
    return head + tempo + track(chords) + track(mel)


class IngestTests(unittest.TestCase):
    def setUp(self):
        # A minor: Am | F | C | G  -> degrees 1 6 3 7
        prog = [(9, True), (5, False), (0, False), (7, False)] * 2
        mel = []
        for bar in range(8):
            for step, pitch in ((0, 69), (4, 72), (8, 71), (12, 69)):
                mel.append((bar * 4 * PPQ + step * PPQ // 4, pitch, PPQ // 2))
        self.blob = make_midi(prog, mel)

    def test_parse(self):
        m = parse_midi(self.blob)
        self.assertEqual(m["ppq"], PPQ)
        self.assertEqual(m["tempos"][0][1], 500000)
        self.assertEqual(len(m["tracks"]), 3)
        self.assertEqual(len(m["tracks"][2]), 32)

    def test_key_and_progression(self):
        m = parse_midi(self.blob)
        notes = sorted(n for t in m["tracks"] for n in t)
        root, mode = ingest.estimate_key(notes)
        self.assertEqual((root, mode), (9, "minor"))
        chord, melody = ingest.split_roles(m["tracks"], m["ppq"])
        degs = ingest.chord_degrees(chord, m["ppq"], root, mode)
        self.assertEqual(degs[:4], [1, 6, 3, 7])
        rhythms = list(ingest.rhythm_bars(melody, m["ppq"]))
        self.assertTrue(rhythms and [r[0] for r in rhythms[0]] == [0, 4, 8, 12])

    def test_cli_writes_library(self):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "trap"))
            with open(os.path.join(d, "trap", "a.mid"), "wb") as f:
                f.write(self.blob)
            out = os.path.join(d, "patterns.json")
            subprocess.run([sys.executable, "-I", os.path.join(os.path.dirname(HERE), "ingest.py"),
                            os.path.join(d, "trap"), "--out", out], check=True, capture_output=True)
            lib = json.load(open(out))
            self.assertEqual(lib["meta"]["files_used"], 1)
            self.assertTrue(any(p["degrees"] == [1, 6, 3, 7] and p["style"] == "trap" for p in lib["progressions"]))


if __name__ == "__main__":
    unittest.main()
