#!/usr/bin/env python3
"""gsprep.py - turn a PCSX2 binary GS dump into files the C replay harness eats.

    gsprep.py <dump.gs|dump.gs.zst> <outdir>

Writes into <outdir>:
    vram.bin        4 MB of GS local memory, raw physical (real-GS) layout
    stream.bin      flat record stream (see FORMAT below)
    pre_state.bin   the 425 bytes of GS register state that precede VRAM
    post_state.bin  the bytes between VRAM and the packet stream
    shot.png        the dump's embedded screenshot (PCSX2's own render)

stream.bin FORMAT (all little endian):
    "GSR1"  u32 nrec
    per record:  u8 type, u8 path, u16 _pad, u32 nqw, then nqw*16 bytes
        type 0  GIF transfer on `path`, nqw quadwords
        type 1  vsync; `path` = field, nqw = 0
        type 2  privileged-register snapshot; nqw = 512 (8192 bytes)

The record walk is decode.py's, unchanged: brute-force the stream start, then
trust PCSX2's own record framing.
"""
import struct, sys, os


def load(fn):
    d = open(fn, 'rb').read()
    if fn.endswith('.zst'):
        from compression import zstd
        d = zstd.decompress(d)
    return d


def try_walk(d, off, end):
    n = 0
    while off < end:
        pid = d[off]; off += 1
        if pid == 0:
            if off + 5 > end or d[off] > 3:
                return False
            size = struct.unpack_from('<I', d, off + 1)[0]
            if size == 0 or size % 16 or off + 5 + size > end:
                return False
            off += 5 + size; n += 1
        elif pid == 1:
            off += 1; n += 1
        elif pid == 2:
            off += 4; n += 1
        elif pid == 3:
            off += 8192; n += 1
        else:
            return False
    return off == end and n > 50


def find_stream(d, hint):
    end = len(d)
    for off in range(max(0, hint - 4096), end - 1000):
        if d[off] in (0, 1, 2, 3) and try_walk(d, off, end):
            return off
    return None


def main():
    fn, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    d = load(fn)
    slen = struct.unpack_from('<I', d, 20)[0]
    w, h = struct.unpack_from('<II', d, 28)
    shotsz = struct.unpack_from('<I', d, 40)[0]
    shotoff = 44 + slen
    stateoff = shotoff + shotsz
    vramoff = stateoff + 425
    vramend = vramoff + 4 * 1024 * 1024

    open(os.path.join(outdir, 'vram.bin'), 'wb').write(d[vramoff:vramend])
    open(os.path.join(outdir, 'pre_state.bin'), 'wb').write(d[stateoff:vramoff])

    stream = find_stream(d, vramend)
    if stream is None:
        sys.exit('no packet stream found')
    open(os.path.join(outdir, 'post_state.bin'), 'wb').write(d[vramend:stream])

    try:
        from PIL import Image
        Image.frombytes('RGBA', (w, h), d[shotoff:shotoff + shotsz]) \
            .convert('RGB').save(os.path.join(outdir, 'shot.png'))
    except Exception as e:
        print('screenshot:', e)

    recs = bytearray()
    n = 0
    counts = {}
    off, end = stream, len(d)
    while off < end:
        pid = d[off]; off += 1
        if pid == 0:
            path = d[off]
            size = struct.unpack_from('<I', d, off + 1)[0]
            recs += struct.pack('<BBHI', 0, path, 0, size // 16)
            recs += d[off + 5:off + 5 + size]
            off += 5 + size
        elif pid == 1:
            recs += struct.pack('<BBHI', 1, d[off], 0, 0)
            off += 1
        elif pid == 2:
            off += 4
            continue
        elif pid == 3:
            recs += struct.pack('<BBHI', 2, 0, 0, 512)
            recs += d[off:off + 8192]
            off += 8192
        else:
            sys.exit('bad record id %d at %#x' % (pid, off - 1))
        counts[pid] = counts.get(pid, 0) + 1
        n += 1

    with open(os.path.join(outdir, 'stream.bin'), 'wb') as f:
        f.write(b'GSR1' + struct.pack('<I', n))
        f.write(recs)
    print('%s -> %s' % (fn, outdir))
    print('  vram   @%#x   stream @%#x' % (vramoff, stream))
    print('  records %d  %s' % (n, counts))


main()
