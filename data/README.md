# data/

`patterns.pop909.json` — abstract statistics (chord-degree loops, one-bar onset grids, short scale-degree
contours) extracted by `tools/dataset/ingest.py` from the **POP909 dataset**
(https://github.com/music-x-lab/POP909-Dataset, MIT License, © 2020 Music X Lab; Z. Wang et al., "POP909: A Pop-song
Dataset for Music Arrangement Generation", ISMIR 2020). No original MIDI notes are stored. Tagged `rnb`.

Reproduce:

    git clone --depth 1 https://github.com/music-x-lab/POP909-Dataset
    python3 tools/dataset/ingest.py POP909-Dataset/POP909 --style rnb --max-per-kind 60 --min-count 2 \
        --out data/patterns.pop909.json

It is compiled into the plugin and can be passed to the CLI with `--patterns data/patterns.pop909.json`.
`data/raw/` and `data/telegram/` are git-ignored: put third-party packs there, never commit them.

`patterns.cymatics.json` — the same kind of abstract statistics from the Cymatics free MIDI packs provided by the
project owner (who states they hold the licence): *Essential MIDI Collection Bundle*, *Scarlet Instrument Loops & MIDI*,
*Moonlight Instrument Loops & MIDI* (418 files). 222 progressions, 160 rhythms, 140 motifs, tagged both `trap` and
`hiphop` (an assumption: the packs carry no style label). The MIDI files themselves are **not** in this repository.
Not used: *Vibrance* (San Holo / future bass, off-genre), *Zodiac Platinum* (WAV only), *Hyperion* (MIDI inside the
archive is password-protected - not opened), and the large paid-looking packs. Both JSON files are compiled into the plugin.
