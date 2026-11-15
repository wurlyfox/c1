#!/usr/bin/env python3
"""Turn a Crash Bandicoot (NTSC-U, SCUS-94900) disc image into a c1 web pack.

The web build of c1 only needs the level files of the disc (S0/..S3/*.NSF and
*.NSD), about half of the image. This script reads them out of a .bin (raw
2352-byte sectors) or .iso (2048-byte sectors) and writes a ZIP that the web
page loads with "Choose game ZIP":

    c1-pack.zip
        manifest.json          format, serial and the list of files
        streams/s0000003.nsd   level files, lowercase names
        streams/s0000003.nsf
        ...

Usage:
    python3 make_c1_pack.py                      # look for the image by itself
    python3 make_c1_pack.py "Crash Bandicoot.bin"
    python3 make_c1_pack.py game.bin -o my-pack.zip
    python3 make_c1_pack.py game.bin --store     # no compression (bigger, faster)

Only the Python standard library is used. The pack contains game data: it is
for your own use, do not share it.
"""
import argparse
import json
import os
import re
import struct
import sys
import time
import zipfile
from pathlib import Path

FORMAT = "c1-web-pack"
VERSION = 1
EXPECTED_SERIAL = "SCUS-94900"
ASSET_NAME = re.compile(r"^S[0-9A-F]{7}\.NS[FD]$", re.I)
SECTOR = 2048
RAW_SECTOR = 2352
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"


class PackError(Exception):
    pass


class Disc:
    """Read-only ISO 9660 view of a raw (2352) or cooked (2048) CD image."""

    def __init__(self, path):
        self.path = Path(path)
        self.f = open(path, "rb")
        self.size = os.fstat(self.f.fileno()).st_size
        head = self.f.read(16)
        self.raw = len(head) == 16 and head[:12] == SYNC
        # mode 2 (XA) sectors carry an 8 byte subheader after the 16 byte header
        self.data_offset = (24 if head[15] == 2 else 16) if self.raw else 0
        self.sector_size = RAW_SECTOR if self.raw else SECTOR
        pvd = self.read(16, 1)
        if pvd[1:6] != b"CD001":
            raise PackError("not a CD image this tool can read (use the .bin or .iso of the disc)")
        self.root = (struct.unpack_from("<I", pvd, 158)[0], struct.unpack_from("<I", pvd, 166)[0])

    def close(self):
        self.f.close()

    def read(self, lba, count):
        """User data of `count` sectors starting at `lba`."""
        self.f.seek(lba * self.sector_size)
        buf = self.f.read(count * self.sector_size)
        if len(buf) < count * self.sector_size:
            raise PackError("the disc image is incomplete")
        if not self.raw:
            return buf
        out = bytearray(count * SECTOR)
        for i in range(count):
            src = i * RAW_SECTOR + self.data_offset
            out[i * SECTOR:(i + 1) * SECTOR] = buf[src:src + SECTOR]
        return bytes(out)

    def read_extent(self, lba, size):
        out = bytearray()
        total = -(-size // SECTOR)
        done = 0
        while done < total:
            n = min(512, total - done)
            out += self.read(lba + done, n)
            done += n
        return bytes(out[:size])

    def walk(self, lba=None, size=None, depth=0, prefix=""):
        """Yields (path, lba, size) for every file on the disc."""
        if lba is None:
            lba, size = self.root
        data = self.read_extent(lba, size)
        off = 0
        while off < len(data):
            length = data[off]
            if length == 0:  # records do not cross sectors
                off = (off // SECTOR + 1) * SECTOR
                continue
            ext, sz = struct.unpack_from("<I", data, off + 2)[0], struct.unpack_from("<I", data, off + 10)[0]
            flags = data[off + 25]
            name = data[off + 33:off + 33 + data[off + 32]]
            off += length
            if name in (b"\x00", b"\x01"):
                continue
            name = name.decode("ascii", "replace")
            if flags & 2:
                if depth < 4:
                    yield from self.walk(ext, sz, depth + 1, prefix + name + "/")
            else:
                yield prefix + name.split(";")[0], ext, sz


def disc_serial(disc, files):
    """Serial from SYSTEM.CNF (BOOT = cdrom:\\SCUS_949.00;1 -> SCUS-94900)."""
    for path, lba, size in files:
        if path.upper() == "SYSTEM.CNF":
            text = disc.read_extent(lba, min(size, 4096)).decode("ascii", "replace")
            m = re.search(r"([A-Z]{4})[_-](\d{3})\.(\d{2})", text)
            if m:
                return "%s-%s%s" % m.groups()
    return None


def looks_like_image(path):
    try:
        return path.is_file() and path.suffix.lower() in (".bin", ".iso", ".img") and path.stat().st_size > 50 * 1024 * 1024
    except OSError:
        return False


def find_images():
    home = Path.home()
    here = Path(__file__).resolve().parent
    folders = [Path.cwd(), here, here.parent, home / "Downloads", home / "Desktop", home / "Documents"]
    seen, found = set(), []
    for folder in folders:
        try:
            entries = sorted(folder.iterdir())
        except OSError:
            continue
        for path in entries:
            if path not in seen and looks_like_image(path):
                seen.add(path)
                found.append(path)
    return found


def choose_image():
    """Finds the game image among the usual folders; asks when there are several."""
    candidates = []
    for path in find_images():
        try:
            disc = Disc(path)
            serial = disc_serial(disc, list(disc.walk()))
            disc.close()
        except (PackError, OSError, struct.error):
            continue
        if serial == EXPECTED_SERIAL:
            candidates.append(path)
    if not candidates:
        raise PackError(
            "no Crash Bandicoot (%s) image found.\n"
            "Put your .bin next to this script, or in Downloads, Desktop or Documents,\n"
            "or pass its path:  python3 make_c1_pack.py \"path/to/game.bin\"" % EXPECTED_SERIAL)
    if len(candidates) == 1:
        return candidates[0]
    print("Several images found:")
    for i, path in enumerate(candidates, 1):
        print("  %d. %s" % (i, path))
    while True:
        answer = input("Which one? [1-%d]: " % len(candidates)).strip()
        if answer.isdigit() and 1 <= int(answer) <= len(candidates):
            return candidates[int(answer) - 1]


def make_pack(image, out, store=False, force=False, log=print):
    disc = Disc(image)
    try:
        files = list(disc.walk())
        serial = disc_serial(disc, files)
        if serial != EXPECTED_SERIAL:
            message = "this image is %s; c1 is written for the NTSC-U release (%s)" % (serial or "not a PlayStation disc", EXPECTED_SERIAL)
            if not force:
                raise PackError(message + ".\nUse --force to build the pack anyway.")
            log("Warning: " + message)
        assets = sorted((p.split("/")[-1].lower(), lba, size) for p, lba, size in files if ASSET_NAME.match(p.split("/")[-1]))
        if not assets:
            raise PackError("no S00000xx.NSF / .NSD level files on this disc")
        names = {a[0] for a in assets}
        missing = sorted(n for n in names if (n[:-1] + ("d" if n.endswith("f") else "f")) not in names)
        if missing:
            log("Warning: these files have no .NSF/.NSD partner: " + ", ".join(missing))

        total = sum(a[2] for a in assets)
        log("%s: %s, %d level files, %.1f MB of %.1f MB"
            % (Path(image).name, serial, len(assets), total / 1048576, disc.size / 1048576))
        manifest = {
            "format": FORMAT,
            "version": VERSION,
            "serial": serial,
            "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "files": [{"name": name, "size": size} for name, _, size in assets],
        }
        tmp = Path(str(out) + ".part")
        done = 0
        with zipfile.ZipFile(tmp, "w", zipfile.ZIP_STORED if store else zipfile.ZIP_DEFLATED,
                             compresslevel=None if store else 6) as z:
            z.writestr("manifest.json", json.dumps(manifest, indent=1))
            for name, lba, size in assets:
                z.writestr("streams/" + name, disc.read_extent(lba, size))
                done += size
                log("\r  packing %3d%%  %s" % (done * 100 // total, name), end="")
        log("")
        os.replace(tmp, out)
    finally:
        disc.close()
    log("Created %s (%.1f MB)" % (out, Path(out).stat().st_size / 1048576))
    return manifest


def main():
    ap = argparse.ArgumentParser(description="Make a c1 web pack (ZIP) from a Crash Bandicoot disc image.")
    ap.add_argument("image", nargs="?", help=".bin or .iso of the disc (found automatically when omitted)")
    ap.add_argument("-o", "--output", help="ZIP to write (default: c1-pack.zip next to the image)")
    ap.add_argument("--store", action="store_true", help="do not compress (bigger file, faster to make and load)")
    ap.add_argument("--force", action="store_true", help="build the pack even if the disc is not " + EXPECTED_SERIAL)
    ap.add_argument("--pause", action="store_true", help="wait for Enter before exiting (used by the double-click launchers)")
    args = ap.parse_args()
    code = 0
    try:
        image = Path(args.image) if args.image else choose_image()
        if not image.is_file():
            raise PackError("file not found: %s" % image)
        out = Path(args.output) if args.output else image.with_name("c1-pack.zip")
        make_pack(image, out, store=args.store, force=args.force)
        print("\nOpen the c1 web page and pick this file with \"Choose game ZIP\".")
    except PackError as e:
        print("\nError: %s" % e, file=sys.stderr)
        code = 1
    except KeyboardInterrupt:
        print("\nCancelled.", file=sys.stderr)
        code = 130
    if args.pause:
        try:
            input("\nPress Enter to close...")
        except EOFError:
            pass
    return code


if __name__ == "__main__":
    sys.exit(main())
