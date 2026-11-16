#!/usr/bin/env python3
"""Split a c1 web pack (ZIP) into parts for the page's "Download the game" option.

Adapted from ReChan Web (tools/media_pack/split_pack.py).

    python3 tools/pack/split_pack.py c1-pack.zip

Writes into web/data/ of this repository (or the folder given with -o):
    pack.json              list of parts + SHA-256 of each part and of the whole pack
    c1-pack.part001 ...    parts of 19 MB (the last one smaller)

Run web/build.sh afterwards: the build copies web/data/ next to the page, and
the start page then offers to download the game. The browser downloads the
parts one by one, checks each against its SHA-256, keeps them in its own
storage so an interrupted download resumes, and installs the game from them.

The parts contain game data. They are for your own, local use: web/data/ is
ignored by git on purpose. Do not publish or commit them.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
import time
import zipfile
from pathlib import Path

FORMAT = "c1-pack-parts"
PACK_FORMAT = "c1-web-pack"
PART_PREFIX = "c1-pack.part"
DEFAULT_OUT = Path(__file__).resolve().parent.parent.parent / "web" / "data"


def main() -> int:
    ap = argparse.ArgumentParser(description="Split a c1 web pack into parts for the page to download.")
    ap.add_argument("zip", type=Path, help="pack created by make_c1_pack.py")
    ap.add_argument("-o", "--out", type=Path, help="output folder (default: web/data of this repository)")
    ap.add_argument("--part-mb", type=int, default=19, help="part size in MB (default 19)")
    args = ap.parse_args()

    src = args.zip.expanduser().resolve()
    if not src.is_file():
        sys.exit(f"Error: {src} does not exist")
    try:
        with zipfile.ZipFile(src) as z:
            manifest = json.loads(z.read("manifest.json"))
    except (zipfile.BadZipFile, KeyError, ValueError):
        sys.exit("Error: this file is not a c1 web pack (no manifest.json). Make it with make_c1_pack.py.")
    if manifest.get("format") != PACK_FORMAT:
        sys.exit("Error: this file is not a c1 web pack. Make it with make_c1_pack.py.")

    part_size = max(1, args.part_mb) * 1024 * 1024
    out = (args.out or DEFAULT_OUT).expanduser().resolve()
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob(PART_PREFIX + "*"):
        old.unlink()

    total = hashlib.sha256()
    parts = []
    with open(src, "rb") as f:
        index = 1
        while True:
            chunk = f.read(part_size)
            if not chunk:
                break
            total.update(chunk)
            name = f"{PART_PREFIX}{index:03d}"
            (out / name).write_bytes(chunk)
            parts.append({"name": name, "size": len(chunk), "sha256": hashlib.sha256(chunk).hexdigest()})
            print(f"  {name}  {len(chunk) / 1048576:5.1f} MB")
            index += 1

    info = {
        "format": FORMAT,
        "version": 1,
        "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "file": src.name,
        "size": src.stat().st_size,
        "sha256": total.hexdigest(),
        "serial": manifest.get("serial"),
        "packVersion": manifest.get("version"),
        "parts": parts,
    }
    (out / "pack.json").write_text(json.dumps(info, indent=2), encoding="utf-8")

    print()
    print(f"Done: {len(parts)} parts in {out}")
    if out == DEFAULT_OUT.resolve():
        print("Now run web/build.sh: the start page will offer to download the game.")
    else:
        print("Copy the contents of that folder (pack.json and every part) into web/data/, then run web/build.sh.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
