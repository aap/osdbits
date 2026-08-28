#!/usr/bin/env python3
# extract-res.py - regenerate osdbits/res/ from a PS2 BIOS dump.
#
# The repo does not ship Sony's texture data; this script extracts it
# from a BIOS image you dumped from your own console:
#
#     python3 tools/extract-res.py path/to/bios.bin osdbits/res
#
# It locates the ROMDIR (name[10], u16 extsize, u32 size entries, data
# packed in order, 16-byte aligned), finds the TEXIMAGE container (which
# is itself a nested ROMDIR of compressed subfiles), decompresses each
# subfile (the OSDSYS "expand" LZ scheme) and writes xxd -i style .inc
# files for res.c, plus the raw expanded data with --raw.
#
# A bare TEXIMAGE file (already extracted from a BIOS) also works as
# input.

import argparse
import struct
import sys
from pathlib import Path


def parse_romdir(data, base):
    """Parse a ROMDIR table at file offset base. Returns {name: (offset, size)},
    with data offsets computed the way the ROM lays files out: packed in
    entry order from the start of the ROMDIR's address space, 16-byte
    aligned."""
    entries = {}
    offset = 0
    pos = base
    while True:
        name = data[pos:pos+10].rstrip(b"\0")
        if not name:
            break
        extsize, size = struct.unpack("<HI", data[pos+10:pos+16])
        entries[name.decode("ascii", "replace")] = (offset, size)
        offset += (size + 0xF) & ~0xF
        pos += 16
    return entries


def find_romdir(data):
    """Find the ROMDIR table: a 16-byte-aligned 'RESET' entry followed by
    a 'ROMDIR' entry."""
    pos = 0
    while True:
        pos = data.find(b"RESET\0\0\0\0\0", pos)
        if pos < 0:
            raise SystemExit("error: no ROMDIR found (not a BIOS/TEXIMAGE image?)")
        if data[pos+16:pos+22] == b"ROMDIR":
            return pos
        pos += 1


def expand(src):
    """The OSDSYS resource decompressor (port of ps2expand.c): u32 size,
    then LZ blocks of 30 literal/backref flags packed into a u32 whose
    low 2 bits pick the offset/length split."""
    size, = struct.unpack("<I", src[0:4])
    p = 4
    dst = bytearray()
    ndesc = 0
    desc = 0
    while len(dst) < size:
        if ndesc == 0:
            desc = struct.unpack(">I", src[p:p+4])[0]
            p += 4
            n = desc & 3
            shift = 14 - n
            mask = 0x3FFF >> n
            ndesc = 30
        b = src[p]
        p += 1
        if desc & 0x80000000:
            h = b << 8 | src[p]
            p += 1
            back = 1 + (h & mask)
            m = 3 + (h >> shift)
            for _ in range(m):
                dst.append(dst[-back])
        else:
            dst.append(b)
        desc = (desc << 1) & 0xFFFFFFFF
        ndesc -= 1
    return bytes(dst)


def write_inc(path, symbol, data):
    with open(path, "w") as f:
        f.write("unsigned char %s[] = {\n" % symbol)
        for i in range(0, len(data), 12):
            row = data[i:i+12]
            f.write("  " + ", ".join("0x%02x" % b for b in row))
            f.write(",\n" if i + 12 < len(data) else "\n")
        f.write("};\n")
        f.write("unsigned int %s_len = %d;\n" % (symbol, len(data)))


def main():
    ap = argparse.ArgumentParser(description="extract OSDSYS texture resources from a PS2 BIOS dump")
    ap.add_argument("image", help="BIOS image (or a bare TEXIMAGE file)")
    ap.add_argument("outdir", help="output directory (osdbits/res)")
    ap.add_argument("--container", default="TEXIMAGE", help="resource container to extract (default TEXIMAGE)")
    ap.add_argument("--raw", action="store_true", help="also write the raw expanded files")
    args = ap.parse_args()

    data = Path(args.image).read_bytes()
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    base = find_romdir(data)
    entries = parse_romdir(data, base)

    if args.container in entries:
        # a full BIOS: descend into the container, which is a nested ROMDIR
        off, size = entries[args.container]
        data = data[off:off+size]
        base = find_romdir(data)
        entries = parse_romdir(data, base)
    # else: input was already a bare container

    n = 0
    for name, (off, size) in entries.items():
        if name in ("RESET", "ROMDIR", "EXTINFO") or name.startswith("-"):
            continue
        exp = expand(data[off:off+size])
        symbol = name + "_EXP"
        write_inc(outdir / (symbol + ".inc"), symbol, exp)
        if args.raw:
            (outdir / symbol).write_bytes(exp)
        print("%-10s %6d -> %6d" % (name, size, len(exp)))
        n += 1
    print("%d resources extracted to %s" % (n, outdir))


if __name__ == "__main__":
    main()
