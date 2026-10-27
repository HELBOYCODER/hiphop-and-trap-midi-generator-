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
