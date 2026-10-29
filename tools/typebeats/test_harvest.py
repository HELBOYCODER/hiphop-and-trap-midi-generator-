import os, sys, unittest
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harvest


class HarvestTests(unittest.TestCase):
    def test_parse(self):
        p = harvest.parse_listing('(FREE) METRO BOOMIN x FUTURE TYPE BEAT - "SHOTS"', "BPM 126 key Emin | Hard Rap/Trap")
        self.assertEqual(p["artists"], ["metro boomin", "future"])
        self.assertEqual((p["bpm"], p["root"], p["minor"]), (126, 4, True))
        self.assertIn("hard", p["tags"])
        q = harvest.parse_listing("Dark Drill Type Beat | 142 BPM | F# minor")
        self.assertEqual((q["bpm"], q["root"], q["minor"]), (142, 6, True))
        self.assertEqual(q["artists"], [])           # a vibe, not an artist
        self.assertIn("drill", q["tags"])
        r = harvest.parse_listing("Smooth RnB Type Beat 90 BPM A Major")
        self.assertEqual((r["bpm"], r["root"], r["minor"]), (90, 9, False))
        self.assertIsNone(harvest.parse_listing("how to mix vocals")["bpm"])

    def test_aggregate(self):
        rows = [{"title": f"Metro Boomin type beat {b} BPM {k}"} for b, k in
                [(140, "F# minor"), (142, "F# min"), (138, "C# minor"), (150, "A minor"), (141, "F minor"), (139, "G major")]]
        styles = harvest.aggregate(rows, min_listings=5)
        self.assertEqual(len(styles), 1)
        s = styles[0]
        self.assertEqual((s["name"], s["count"]), ("metro boomin", 6))
        self.assertTrue(138 <= s["bpm"][1] <= 142 and s["bpm"][0] <= s["bpm"][1] <= s["bpm"][2])
        self.assertAlmostEqual(s["minor_share"], 5 / 6, places=2)
        self.assertEqual(s["keys"][0], "F# minor")
        self.assertEqual(harvest.aggregate(rows[:3], min_listings=5), [])   # too few listings -> no statistics


if __name__ == "__main__":
    unittest.main()
