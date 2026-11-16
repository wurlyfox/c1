#!/usr/bin/env python3
"""Make the icons of the c1 web app (PWA).

With your game data, the icon is the game's own memory card icon (Crash's
face), taken from the title level of your disc:

    python3 tools/pack/make_icons.py "Crash Bandicoot (USA).bin"
    python3 tools/pack/make_icons.py c1-pack.zip
    python3 tools/pack/make_icons.py S0000019.NSF

make_c1_pack.py also does this by itself when it makes a pack.

Without an argument it writes a plain placeholder icon (no game art), which
web/build.sh uses when the game icon has not been made.

The icons go to web/img/icons/, which is ignored by git: the game icon is
game art and stays on your machine. Run web/build.sh afterwards.
"""
from __future__ import annotations

import argparse
import struct
import sys
import zipfile
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_OUT = HERE.parent.parent / "web" / "img" / "icons"
TITLE_NSF = "s0000019.nsf"

# Where the memory card icon lives in the title level. Found by tracing the
# save screens: frame 45 of the card icon animation, a 16x16 4-bit image in
# texture page 0x52C2E06F, at byte 368 of row 48, with the palette at
# position (7, 0) of the page.
TPAGE_EID = 0x52C2E06F
ICON_X, ICON_Y, ICON_W, ICON_H = 368, 48, 16, 16   # bytes, row, pixels
CLUT_WORD = 7 * 16 + 0 * 256
PAGE = 0x10000
ROW = 512

SIZES = {"icon-192.png": (192, 0.80), "icon-512.png": (512, 0.80),
         "icon-maskable-512.png": (512, 0.60), "apple-touch-icon.png": (180, 0.82)}
BACKGROUND = (0, 0, 0)  # the icon is drawn on black in the game too


class IconError(Exception):
    pass


# --- reading the game data ---------------------------------------------------

def decompress_page(buf: bytes, off: int) -> bytes:
    """Page compression of the engine (NSPageDecompress in src/ns.c)."""
    magic, _pad, length, skip = struct.unpack_from("<HHII", buf, off)
    if magic != 0x1235 or not 0 < length <= PAGE:
        raise IconError("not a compressed page")
    src = off + 12
    dst = bytearray()
    while len(dst) < length:
        prefix = buf[src]
        src += 1
        if prefix & 0x80:
            data = (prefix << 8) | buf[src]
            src += 1
            span = data & 7
            seek = (data >> 3) & 0xFFF
            span = 0x40 if span == 7 else span + 3
            if seek == 0 or seek > len(dst):
                raise IconError("bad compressed page")
            for _ in range(span):
                dst.append(dst[-seek])
        else:
            dst += buf[src:src + prefix]
            src += prefix
    src += skip
    dst += buf[src:src + PAGE - length]
    if len(dst) != PAGE:
        raise IconError("truncated compressed page")
    return bytes(dst)


def texture_pages(nsf: bytes):
    """Every copy of the texture page in the file. The header bytes can also
    turn up by chance inside compressed data, so the caller checks them."""
    head = struct.pack("<HHI", 0x1234, 1, TPAGE_EID)
    off = nsf.find(b"\x35\x12\x00\x00")
    while off >= 0:
        try:
            page = decompress_page(nsf, off)
            if page[:8] == head:
                yield page
        except (IconError, IndexError, struct.error):
            pass
        off = nsf.find(b"\x35\x12\x00\x00", off + 1)
    off = nsf.find(head)
    while off >= 0:
        if off + PAGE <= len(nsf):
            yield nsf[off:off + PAGE]
        off = nsf.find(head, off + 1)


def read_title_level(source: Path) -> bytes:
    name = source.name.lower()
    if name.endswith(".nsf"):
        return source.read_bytes()
    if name.endswith(".zip"):
        with zipfile.ZipFile(source) as z:
            for entry in z.namelist():
                if entry.lower().split("/")[-1] == TITLE_NSF:
                    return z.read(entry)
        raise IconError("the ZIP has no " + TITLE_NSF.upper())
    sys.path.insert(0, str(HERE))
    from make_c1_pack import Disc, PackError  # same folder
    try:
        disc = Disc(source)
    except PackError as e:
        raise IconError(str(e))
    try:
        for path, lba, size in disc.walk():
            if path.split("/")[-1].lower() == TITLE_NSF:
                return disc.read_extent(lba, size)
    finally:
        disc.close()
    raise IconError("the disc has no " + TITLE_NSF.upper())


def icon_from_page(page: bytes):
    clut = struct.unpack_from("<16H", page, CLUT_WORD * 2)
    if clut[0] not in (0x0000, 0x8000) or len(set(clut)) < 8:
        return None
    rows = []
    for y in range(ICON_H):
        line = page[(ICON_Y + y) * ROW + ICON_X:(ICON_Y + y) * ROW + ICON_X + ICON_W // 2]
        row = []
        for x in range(ICON_W):
            idx = (line[x // 2] >> 4) & 15 if x % 2 else line[x // 2] & 15
            c = clut[idx]
            row.append(((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31, ((c >> 10) & 31) * 255 // 31,
                        0 if c == 0 else 255))
        rows.append(row)
    if sum(1 for r in rows for p in r if p[:3] != (0, 0, 0)) < 60:
        return None
    return rows


def game_icon(nsf: bytes):
    """The 16x16 icon as rows of (r, g, b, a)."""
    for page in texture_pages(nsf):
        icon = icon_from_page(page)
        if icon:
            return icon
    raise IconError("the memory card icon was not found: is this the NTSC-U (SCUS-94900) title level?")


# A plain placeholder, used when the game icon has not been made: the
# letters "C1" in a 5x7 pixel font, orange on dark. No game art.
FONT = {"C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
        "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"]}


def placeholder_icon():
    on, off = (255, 138, 31, 255), BACKGROUND + (255,)
    rows = [[off] * 16 for _ in range(16)]
    for n, ch in enumerate("C1"):
        for y, bits in enumerate(FONT[ch]):
            for x, b in enumerate(bits):
                if b == "1":
                    rows[4 + y][2 + n * 7 + x] = on
    return rows


# --- writing -------------------------------------------------------------------

def png(width: int, height: int, pixels) -> bytes:
    raw = b"".join(b"\x00" + bytes(c for p in row for c in p) for row in pixels)
    chunk = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def render(icon, size: int, fill: float):
    """The icon scaled by a whole number of pixels (hard pixels, no blur),
    centred on the background; `fill` is the share of the size it may take."""
    scale = max(1, int(size * fill) // ICON_W)
    left = (size - ICON_W * scale) // 2
    top = (size - ICON_H * scale) // 2
    bg = BACKGROUND + (255,)
    out = []
    for y in range(size):
        iy = (y - top) // scale
        row = []
        for x in range(size):
            ix = (x - left) // scale
            if 0 <= iy < ICON_H and 0 <= ix < ICON_W and icon[iy][ix][3]:
                row.append(icon[iy][ix])
            else:
                row.append(bg)
        out.append(row)
    return out


def write_icons(icon, out: Path) -> None:
    out.mkdir(parents=True, exist_ok=True)
    for name, (size, fill) in SIZES.items():
        (out / name).write_bytes(png(size, size, render(icon, size, fill)))


def make_from_nsf(nsf: bytes, out: Path = DEFAULT_OUT) -> None:
    write_icons(game_icon(nsf), out)


def main() -> int:
    ap = argparse.ArgumentParser(description="Make the icons of the c1 web app.")
    ap.add_argument("source", nargs="?", type=Path,
                    help="disc image, game ZIP or S0000019.NSF (omit for a placeholder without game art)")
    ap.add_argument("-o", "--out", type=Path, default=DEFAULT_OUT, help="output folder (default: web/img/icons)")
    args = ap.parse_args()
    try:
        if args.source:
            make_from_nsf(read_title_level(args.source.expanduser()), args.out)
            print("Game icon written to %s. Now run web/build.sh." % args.out)
        else:
            write_icons(placeholder_icon(), args.out)
            print("Placeholder icon written to %s." % args.out)
    except (IconError, OSError, zipfile.BadZipFile) as e:
        print("Error: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
