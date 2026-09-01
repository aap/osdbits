#!/usr/bin/env python3
"""topng.py - render buffers out of a 4 MB GS memory image (real-GS layout).

    topng.py <vram.bin> [tbp:bw:w:h[:name] ...]

With no buffer specs it uses the OSDSYS System-Config set:
    scr0 0/10, scr1 2240/10, wb3 6720/10, wb4 8960/10, all 640x224 PSMCT32.
Writes <vram-stem>_<name>.png and <..>_<name>_a.png (alpha), 2x vertical
like osdbits/tools/gsmem.py does.

    topng.py -c A.bin B.bin tbp:bw:w:h    # compare one buffer between two
"""
import sys, struct
from PIL import Image

blockTable32 = [
    [0, 1, 4, 5, 16, 17, 20, 21],
    [2, 3, 6, 7, 18, 19, 22, 23],
    [8, 9, 12, 13, 24, 25, 28, 29],
    [10, 11, 14, 15, 26, 27, 30, 31]]
columnTable32 = [
    [0, 1, 4, 5, 8, 9, 12, 13],
    [2, 3, 6, 7, 10, 11, 14, 15],
    [16, 17, 20, 21, 24, 25, 28, 29],
    [18, 19, 22, 23, 26, 27, 30, 31],
    [32, 33, 36, 37, 40, 41, 44, 45],
    [34, 35, 38, 39, 42, 43, 46, 47],
    [48, 49, 52, 53, 56, 57, 60, 61],
    [50, 51, 54, 55, 58, 59, 62, 63]]


def addr32(x, y, tbp, bw):
    page = tbp // 32 + (y // 32) * bw + (x // 64)
    px, py = x % 64, y % 32
    return page * 2048 + blockTable32[py // 8][px // 8] * 64 + \
        columnTable32[py % 8][px % 8]


def grab(mem, tbp, bw, w, h):
    out = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            a = addr32(x, y, tbp, bw) * 4
            out[(y * w + x) * 4:(y * w + x) * 4 + 4] = mem[a:a + 4]
    return bytes(out)


def topng(raw, w, h, name, alpha=False):
    img = Image.new('RGB', (w, h)); px = img.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = raw[(y * w + x) * 4:(y * w + x) * 4 + 4]
            px[x, y] = (a, a, a) if alpha else (r, g, b)
    img.resize((w, h * 2), Image.NEAREST).save(name)


DEFAULT = [(0, 10, 640, 224, 'scr0'), (2240, 10, 640, 224, 'scr1'),
           (6720, 10, 640, 224, 'wb3'), (8960, 10, 640, 224, 'wb4')]


def parse(s):
    f = s.split(':')
    tbp, bw, w, h = (int(x) for x in f[:4])
    return tbp, bw, w, h, (f[4] if len(f) > 4 else 'b%d' % tbp)


def main():
    a = sys.argv[1:]
    if a and a[0] == '-c':
        ma = open(a[1], 'rb').read(); mb = open(a[2], 'rb').read()
        for spec in (a[3:] or [':'.join(map(str, s[:4])) + ':' + s[4]
                               for s in DEFAULT]):
            tbp, bw, w, h, name = parse(spec)
            ra, rb = grab(ma, tbp, bw, w, h), grab(mb, tbp, bw, w, h)
            n = w * h
            diff = sum(1 for i in range(n)
                       if ra[i * 4:i * 4 + 3] != rb[i * 4:i * 4 + 3])
            tot = sum(abs(ra[i] - rb[i]) for i in range(0, n * 4)
                      if i % 4 != 3)
            print('%-6s tbp=%-5d rgb-differing px %6d/%d (%5.1f%%)  '
                  'mean |d| %6.3f' % (name, tbp, diff, n, 100.0 * diff / n,
                                      tot / (n * 3.0)))
        return

    mem = open(a[0], 'rb').read()
    stem = a[0].rsplit('.', 1)[0]
    specs = [parse(s) for s in a[1:]] or DEFAULT
    for tbp, bw, w, h, name in specs:
        raw = grab(mem, tbp, bw, w, h)
        topng(raw, w, h, '%s_%s.png' % (stem, name))
        topng(raw, w, h, '%s_%s_a.png' % (stem, name), alpha=True)
        print(name, 'tbp', tbp, 'first px', raw[:8].hex())


main()
