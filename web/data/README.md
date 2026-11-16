# Game data for "Download the game"

This folder is filled by

```bash
python3 tools/pack/split_pack.py c1-pack.zip
```

with `pack.json` (the list of parts and their SHA-256) and
`c1-pack.part001`, `part002`… (19 MB each). `web/build.sh` copies it next to
the page; without `pack.json` the start page does not show the download
option.

These files contain the game. They are ignored by git on purpose: keep them
on your own machine and do not publish or commit them.
