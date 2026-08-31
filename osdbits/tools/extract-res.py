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


# ---------------------------------------------------------------------
# The OSD text engine's static tables (menutext.c).
#
# The font PAGES are ordinary resources (FNTASCII & co. in FNTIMAGE,
# 4-bit indexed bitmaps), but three things the engine needs are plain
# .data inside the OSDSYS module itself and have to be read out of the
# decompressed image:
#
#   0x26FE60  97 x {u32 xoff; u32 width}  the ASCII glyph metrics
#             (glyph i = character 32+i; the cell is 32x40 and xoff is
#             the glyph's left inset inside it)
#   0x2715E0  16 x u32 RGBA                the font CLUT (grey ramp with
#                                          alpha - the antialiasing)
#   0x298B08  299 x char *                 the English osdGetString table
#
# OSDSYS is stored in the BIOS as an ELF whose .text/.data are packed
# with the same LZ scheme as the resources, so this re-uses expand().


OSDSYS_LOAD = 0x200000
FONT_METRICS_VA = 0x26FE60
FONT_METRICS_N = 97
FONT_CLUT_VA = 0x2715E0
STRINGS_VA = 0x298B08
STRINGS_N = 299


def expand_osdsys(bios):
    """Find OSDSYS in the ROMDIR and return its decompressed image, which
    loads at 0x200000."""
    entries = parse_romdir(bios, find_romdir(bios))
    if "OSDSYS" not in entries:
        raise SystemExit("error: no OSDSYS in this image")
    off, size = entries["OSDSYS"]
    osdsys = bios[off:off+size]
    # the compressed body follows the ELF header + loader stub; find it by
    # trying every 16-byte-aligned offset until expand() produces a blob
    # that starts where the module's .data does
    for start in range(0, 0x2000, 16):
        try:
            out = expand(osdsys[start:])
        except Exception:
            continue
        if len(out) > 0x80000:
            return out
    raise SystemExit("error: could not expand OSDSYS")


def write_font_tables(path, img):
    def rd(va, n):
        o = va - OSDSYS_LOAD
        return img[o:o+n]

    met = struct.unpack("<%dI" % (FONT_METRICS_N*2), rd(FONT_METRICS_VA, FONT_METRICS_N*8))
    clut = struct.unpack("<16I", rd(FONT_CLUT_VA, 64))
    ptrs = struct.unpack("<%dI" % STRINGS_N, rd(STRINGS_VA, STRINGS_N*4))

    def cstr(p):
        if not (OSDSYS_LOAD <= p < OSDSYS_LOAD + len(img)):
            return None
        s = img[p-OSDSYS_LOAD:]
        return s[:s.index(b"\0")]

    def cquote(b):
        out = ""
        for ch in b:
            if ch == 0x22 or ch == 0x5C:
                out += "\\" + chr(ch)
            elif 0x20 <= ch < 0x7F:
                out += chr(ch)
            else:
                out += "\\%03o" % ch
        return out

    with open(path, "w") as f:
        f.write("/* generated by tools/extract-res.py --tables - do not commit */\n\n")
        f.write("/* real 0x26fe60: {left inset, advance width} per ASCII glyph */\n")
        f.write("const int fontAsciiMetrics[%d][2] = {\n" % FONT_METRICS_N)
        for i in range(FONT_METRICS_N):
            c = 32 + i
            f.write("\t{ %3d, %3d },\t/* %s */\n" %
                (met[i*2], met[i*2+1], repr(chr(c)) if 32 <= c < 127 else "0x%02x" % c))
        f.write("};\n\n")
        f.write("/* real 0x2715e0: the 16-entry font CLUT (RGBA, 0x80 = opaque) */\n")
        f.write("unsigned int fontClut[16] = {\n")
        for i in range(0, 16, 4):
            f.write("\t" + " ".join("0x%08x," % clut[j] for j in range(i, i+4)) + "\n")
        f.write("};\n\n")
        f.write("/* real 0x298b08: the English osdGetString table */\n")
        f.write("const char *osdStringTable[%d] = {\n" % STRINGS_N)
        for i in range(STRINGS_N):
            s = cstr(ptrs[i])
            if s is None:
                f.write("\t0,\n")
            else:
                f.write("\t\"%s\",\n" % cquote(s))
        f.write("};\n")
        f.write("const int osdStringTableLen = %d;\n" % STRINGS_N)
    print("font tables (%d glyphs, %d strings) -> %s" % (FONT_METRICS_N, STRINGS_N, path))


# ---------------------------------------------------------------------
# Module U's two static meshes (menuconfig.c).
#
# Both are plain .data inside OSDSYS, reached through a "scene struct"
# whose +0x04/+0x08/+0x0c/+0x10 fields are the face count and the three
# per-face arrays (0x22cfa8 reads them):
#
#   0x27e950  the carousel rod   16 faces, verts 0x27e050, normals
#             0x27e850, uvs 0x27e450          - the glass clock's hour rod
#   0x27efb0  the config cube     6 faces, verts 0x27ec50, normals
#             0x27edd0, uvs 0x27ee30          - the five item cubes
#
# plus the five cubes' placement table at 0x27f090 (stride 48:
# {float pos[4]; int colour[4]; float sizeBias, ...}), which 0x226d00
# walks.

SCENE_ROD = 0x27E950
SCENE_CUBE = 0x27EFB0
CUBE_TABLE_VA = 0x27F090
CUBE_TABLE_N = 5


def write_menu_geometry(path, img):
    def rd(va, n):
        o = va - OSDSYS_LOAD
        return img[o:o+n]

    def vecs(va, n):
        return [struct.unpack("<4f", rd(va + i*16, 16)) for i in range(n)]

    def flit(x):
        t = "%.9g" % x
        if "." not in t and "e" not in t and "E" not in t and "inf" not in t:
            t += ".0"
        return t + "f"

    def farr(f, name, rows, comment):
        f.write("/* %s */\n" % comment)
        f.write("const float %s[%d][4] = {\n" % (name, len(rows)))
        for r in rows:
            f.write("\t{ %s },\n" % ", ".join(flit(x) for x in r))
        f.write("};\n\n")

    with open(path, "w") as f:
        f.write("/* generated by tools/extract-res.py --tables - do not commit */\n\n")
        for tag, scene in (("Rod", SCENE_ROD), ("Cube", SCENE_CUBE)):
            nf, vp, np_, up = struct.unpack("<4I", rd(scene + 4, 16))
            farr(f, "menu%sVerts" % tag, vecs(vp, nf*4),
                 "real 0x%06x: %d faces x 4 vertices" % (vp, nf))
            farr(f, "menu%sNorms" % tag, vecs(np_, nf),
                 "real 0x%06x: one face normal per face (w = 0)" % np_)
            farr(f, "menu%sUVs" % tag, vecs(up, nf*4),
                 "real 0x%06x: one (u, v) per vertex" % up)
            f.write("const int menu%sFaces = %d;\n\n" % (tag, nf))
        pos, col, bias = [], [], []
        for i in range(CUBE_TABLE_N):
            e = CUBE_TABLE_VA + i*48
            pos.append(struct.unpack("<4f", rd(e, 16)))
            col.append(struct.unpack("<4I", rd(e + 16, 16)))
            bias.append(struct.unpack("<f", rd(e + 32, 4))[0])
        farr(f, "menuCubePos", pos,
             "real 0x%06x + 0x00: the five cubes' positions" % CUBE_TABLE_VA)
        f.write("/* real 0x%06x + 0x10: per-cube colour */\n" % CUBE_TABLE_VA)
        f.write("const int menuCubeColor[%d][4] = {\n" % CUBE_TABLE_N)
        for c in col:
            f.write("\t{ %s },\n" % ", ".join("0x%02x" % x for x in c))
        f.write("};\n\n")
        f.write("/* real 0x%06x + 0x20: per-cube size bias */\n" % CUBE_TABLE_VA)
        f.write("const float menuCubeBias[%d] = { %s };\n" %
                (CUBE_TABLE_N, ", ".join(flit(x) for x in bias)))
    print("menu geometry (rod + cube + %d placements) -> %s" % (CUBE_TABLE_N, path))


def main():
    ap = argparse.ArgumentParser(description="extract OSDSYS texture resources from a PS2 BIOS dump")
    ap.add_argument("image", help="BIOS image (or a bare TEXIMAGE file)")
    ap.add_argument("outdir", help="output directory (osdbits/res)")
    ap.add_argument("--container", default="TEXIMAGE", help="resource container to extract (default TEXIMAGE)")
    ap.add_argument("--raw", action="store_true", help="also write the raw expanded files")
    ap.add_argument("--tables", action="store_true",
                    help="instead of resources, write FONTDATA.inc (the text engine's\nglyph metrics, font CLUT and string table) and MENUGEOM.inc (Module U's\nrod and cube meshes), both read out of OSDSYS itself")
    args = ap.parse_args()

    data = Path(args.image).read_bytes()
    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    if args.tables:
        img = expand_osdsys(data)
        write_font_tables(outdir / "FONTDATA.inc", img)
        write_menu_geometry(outdir / "MENUGEOM.inc", img)
        return

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
