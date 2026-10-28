"""
Extract the game's asset files from a disc image of your own copy of
Crash Bandicoot.

Reads the ISO9660 filesystem of a .bin (raw 2352-byte sectors) or .iso
(2048-byte sectors) image, copies every .NSD/.NSF file from the S0-S3
directories into the streams directory, and renames them to lowercase.

usage: python tools/extract_streams.py <disc image> [output dir]

The output dir defaults to the streams directory next to this script's
parent directory.
"""
import os
import struct
import sys

SYNC = b"\x00" + b"\xff" * 10 + b"\x00"


def extract(image_path, out_dir):
    f = open(image_path, "rb")

    # raw images start every sector with a 12-byte sync pattern
    f.seek(16 * 2352)
    if f.read(12) == SYNC:
        sector_size = 2352
        f.seek(16 * 2352 + 15)
        # mode 1 user data starts at byte 16 of a raw sector, mode 2 form 1 at byte 24
        data_off = 16 if f.read(1) == b"\x01" else 24
    else:
        sector_size, data_off = 2048, 0

    def read(lba, nbytes):
        out = bytearray()
        while len(out) < nbytes:
            f.seek(lba * sector_size + data_off)
            chunk = f.read(2048)
            if not chunk:
                sys.exit("error: %s ends early; is it a complete disc image?" % image_path)
            out += chunk
            lba += 1
        return bytes(out[:nbytes])

    def entries(lba, size):
        buf = read(lba, size)
        i = 0
        while i < len(buf):
            n = buf[i]
            if n == 0:  # records don't cross sector boundaries; skip to next sector
                i = (i // 2048 + 1) * 2048
                continue
            rec = buf[i:i + n]
            elba, esize = struct.unpack_from("<I", rec, 2)[0], struct.unpack_from("<I", rec, 10)[0]
            name = rec[33:33 + rec[32]]
            if name not in (b"\x00", b"\x01"):  # skip . and ..
                yield name.decode("ascii").split(";")[0], elba, esize, bool(rec[25] & 2)
            i += n

    pvd = read(16, 2048)
    if pvd[1:6] != b"CD001":
        sys.exit("error: %s is not a recognised disc image (no ISO9660 volume found)" % image_path)
    root = pvd[156:156 + 34]

    os.makedirs(out_dir, exist_ok=True)
    count = 0
    for dname, dlba, dsize, isdir in entries(struct.unpack_from("<I", root, 2)[0],
                                             struct.unpack_from("<I", root, 10)[0]):
        if not (isdir and dname in ("S0", "S1", "S2", "S3")):
            continue
        for fname, flba, fsize, fisdir in entries(dlba, dsize):
            if fisdir or not fname.upper().endswith((".NSD", ".NSF")):
                continue
            with open(os.path.join(out_dir, fname.lower()), "wb") as o:
                o.write(read(flba, fsize))
            count += 1
    if count == 0:
        sys.exit("error: no .NSD/.NSF files found in S0-S3; is this a Crash Bandicoot disc?")
    print("extracted %d files to %s" % (count, out_dir))


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__.strip())
    default_out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "streams")
    extract(sys.argv[1], os.path.normpath(sys.argv[2] if len(sys.argv) == 3 else default_out))
