#!/usr/bin/env python3
"""Build the type-beat style bank (data/typebeat_styles.json) from public listing metadata.

What it stores: per "<artist/vibe> type beat" style, only aggregate statistics - listing count, BPM range,
minor/major share, common keys, tag counts. No audio, no MIDI, no titles or links of individual beats.
The app and the AI read it with the `find_type_beat_style` tool.

Sources (pick any, combine freely):
  --youtube-key KEY --query "metro boomin type beat" [--query ...]   official YouTube Data API v3 (you supply the key)
  --jsonl listings.jsonl                                             one JSON object per line: title, description?, tags?
  --mcp "node /path/to/server/mcp.mjs" --mcp-tool search_beats --mcp-args '{"query":"{q}"}' --query "dark drill"
        any stdio MCP server, e.g. an integration minted with Pomerado (https://github.com/Pomerado/pomerado) for a
        marketplace you are allowed to use; the tool's JSON result may be a list or {"results":[...]} of listings.

Respect each site's terms of service and rate limits; prefer official APIs. Example:
    python3 harvest.py --youtube-key $YT_KEY --query "metro boomin type beat" --query "dark drill type beat" \
        --out ../../data/typebeat_styles.json --min-listings 8
"""
import argparse
import collections
import json
import re
import statistics
import subprocess
import sys
import urllib.parse
import urllib.request

NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
KEY_RE = re.compile(r"\b([A-G])\s?([#b♯♭]?)\s?[-_ ]?(min(?:or)?|maj(?:or)?|m)\b(?!\w)", re.IGNORECASE)
BPM_RE = re.compile(r"(?:\bbpm\s*[:\-]?\s*(\d{2,3})\b|\b(\d{2,3})\s*bpm\b)", re.IGNORECASE)
TYPE_RE = re.compile(r"^\W*(?:\(?\s*(?:free|prod|hard|new|sold|exclusive)\s*\)?\W*)*(.+?)\s*type\s*beat", re.IGNORECASE)
TAGS = {
    "dark": ("dark", "evil", "scary", "horror", "eerie"), "melodic": ("melodic", "melody"), "sad": ("sad", "emotional", "pain"),
    "hard": ("hard", "aggressive", "banger"), "drill": ("drill",), "rnb": ("r&b", "rnb", "soul", "smooth"),
    "boom bap": ("boom bap", "boombap", "old school", "90s"), "lofi": ("lofi", "lo-fi", "chill"), "trap": ("trap",),
    "guitar": ("guitar",), "piano": ("piano",), "bells": ("bell",), "afro": ("afro",), "latin": ("reggaeton", "latin"),
}
# words that describe a vibe rather than an artist
VIBE_WORDS = {"dark", "melodic", "sad", "hard", "drill", "trap", "rnb", "r&b", "boom", "bap", "boombap", "lofi", "lo-fi", "emotional", "guitar",
              "piano", "type", "free", "chill", "smooth", "soul", "hype", "old", "school", "afro", "evil", "scary", "banger", "sample", "bouncy"}


def parse_listing(title, description="", tags=()):
    text = " ".join([title or "", description or "", " ".join(tags or [])])
    out = {"artists": [], "bpm": None, "root": None, "minor": None, "tags": set()}
    m = TYPE_RE.search(title or "")
    if m:
        raw = re.sub(r"[\"'\[\]\(\)]", " ", m.group(1))
        for part in re.split(r"\s+(?:x|X|&|\+|,|ft\.?|feat\.?|and)\s+|,", raw):
            name = re.sub(r"\s+", " ", part).strip(" -_|").lower()
            if 2 <= len(name) <= 30 and not all(w in VIBE_WORDS for w in name.split()):
                out["artists"].append(name)
    b = BPM_RE.search(text)
    if b:
        v = int(b.group(1) or b.group(2))
        if 50 <= v <= 220:
            out["bpm"] = v
    k = KEY_RE.search(text)
    if k:
        letter, acc, mode = k.group(1).upper(), k.group(2), k.group(3).lower()
        root = (NOTE[letter] + (1 if acc in ("#", "♯") else -1 if acc in ("b", "♭") else 0)) % 12
        out["root"], out["minor"] = root, mode.startswith("m") and not mode.startswith("maj")
    low = text.lower()
    for tag, words in TAGS.items():
        if any(w in low for w in words):
            out["tags"].add(tag)
    return out


def aggregate(listings, min_listings=5):
    groups = collections.defaultdict(list)
    for l in listings:
        p = parse_listing(l.get("title", ""), l.get("description", ""), l.get("tags", []))
        if p["bpm"] is None:
            continue
        keys = p["artists"] or []
        for v in p["tags"]:
            if v in ("dark", "melodic", "sad", "drill", "rnb", "boom bap", "lofi", "hard"):
                keys = keys + [v]          # vibe styles ("dark", "drill"...) are banks too
        for name in keys:
            groups[name].append(p)
    styles = []
    for name, items in groups.items():
        if len(items) < min_listings:
            continue
        bpms = sorted(i["bpm"] for i in items)
        q = lambda f: bpms[min(len(bpms) - 1, int(f * (len(bpms) - 1)))]
        keyed = [i for i in items if i["root"] is not None]
        minor_share = sum(1 for i in keyed if i["minor"]) / len(keyed) if keyed else 0.6
        kc = collections.Counter((i["root"], i["minor"]) for i in keyed)
        tagc = collections.Counter(t for i in items for t in i["tags"])
        top_tags = [t for t, _ in tagc.most_common(5)]
        mix = {"rnb": 0.1, "trap": 1.0, "hiphop": 0.2}
        if "rnb" in tagc and tagc["rnb"] >= len(items) * 0.3:
            mix = {"rnb": 1.0, "trap": 0.3, "hiphop": 0.2}
        if "boom bap" in tagc and tagc["boom bap"] >= len(items) * 0.3:
            mix = {"rnb": 0.1, "trap": 0.0, "hiphop": 1.0}
        styles.append({
            "name": name, "count": len(items),
            "bpm": [q(0.1), int(statistics.median(bpms)), q(0.9)],
            "minor_share": round(minor_share, 2),
            "keys": [f"{NAMES[r]} {'minor' if mn else 'major'}" for (r, mn), _ in kc.most_common(3)],
            "tags": top_tags, "mix": mix,
        })
    styles.sort(key=lambda s: -s["count"])
    return styles


# ---- sources --------------------------------------------------------------------------------------------------------
def from_youtube(key, queries, per_query=50):
    for q in queries:
        params = urllib.parse.urlencode({"part": "snippet", "type": "video", "maxResults": per_query, "q": q, "key": key})
        with urllib.request.urlopen("https://www.googleapis.com/youtube/v3/search?" + params, timeout=30) as r:
            data = json.load(r)
        for it in data.get("items", []):
            sn = it.get("snippet", {})
            yield {"title": sn.get("title", ""), "description": sn.get("description", ""), "tags": []}


def from_jsonl(path):
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                yield json.loads(line)


def from_mcp(command, tool, args_template, queries):
    """Minimal stdio MCP client: initialize, then call `tool` once per query."""
    proc = subprocess.Popen(command, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def rpc(i, method, params=None):
        msg = {"jsonrpc": "2.0", "id": i, "method": method}
        if params is not None:
            msg["params"] = params
        proc.stdin.write(json.dumps(msg) + "\n"); proc.stdin.flush()
        while True:
            line = proc.stdout.readline()
            if not line:
                raise RuntimeError("MCP server closed")
            try:
                reply = json.loads(line)
            except ValueError:
                continue
            if reply.get("id") == i:
                return reply
    try:
        rpc(1, "initialize", {"protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "hhmg-harvest", "version": "1"}})
        proc.stdin.write(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n"); proc.stdin.flush()
        for n, q in enumerate(queries, start=2):
            args = json.loads(args_template.replace("{q}", q.replace('"', '\\"')))
            reply = rpc(n, "tools/call", {"name": tool, "arguments": args})
            for part in reply.get("result", {}).get("content", []):
                if part.get("type") != "text":
                    continue
                try:
                    data = json.loads(part["text"])
                except ValueError:
                    continue
                items = data if isinstance(data, list) else data.get("results") or data.get("items") or data.get("beats") or []
                for it in items:
                    yield {"title": it.get("title") or it.get("name") or "", "description": it.get("description", ""),
                           "tags": it.get("tags") or ([it["genre"]] if it.get("genre") else [])}
    finally:
        proc.terminate()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--youtube-key")
    ap.add_argument("--query", action="append", default=[])
    ap.add_argument("--jsonl")
    ap.add_argument("--mcp"); ap.add_argument("--mcp-tool"); ap.add_argument("--mcp-args", default='{"query":"{q}"}')
    ap.add_argument("--min-listings", type=int, default=5)
    ap.add_argument("--out", default="typebeat_styles.json")
    ap.add_argument("--merge", action="store_true", help="keep styles already in --out that this run did not produce")
    a = ap.parse_args()

    listings = []
    if a.youtube_key and a.query:
        listings += list(from_youtube(a.youtube_key, a.query))
    if a.jsonl:
        listings += list(from_jsonl(a.jsonl))
    if a.mcp and a.mcp_tool and a.query:
        listings += list(from_mcp(a.mcp, a.mcp_tool, a.mcp_args, a.query))
    if not listings:
        sys.exit("no listings: give --youtube-key + --query, --jsonl, or --mcp + --mcp-tool + --query")
    styles = aggregate(listings, a.min_listings)
    if a.merge:
        try:
            old = {s["name"]: s for s in json.load(open(a.out))["type_beats"]}
            old.update({s["name"]: s for s in styles})
            styles = sorted(old.values(), key=lambda s: -s["count"])
        except (OSError, ValueError, KeyError):
            pass
    with open(a.out, "w", encoding="utf-8") as f:
        json.dump({"type_beats": styles, "meta": {"listings_read": len(listings), "note": "aggregate statistics from public listing metadata"}}, f, indent=1, ensure_ascii=False)
    print(f"{len(listings)} listings -> {len(styles)} styles in {a.out}")


if __name__ == "__main__":
    main()
