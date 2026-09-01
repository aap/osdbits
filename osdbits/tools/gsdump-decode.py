#!/usr/bin/env python3
# Generalized PCSX2 binary GS dump decoder (based on docs/gsdump-gifdecode.py).
# For each .gs: extract embedded screenshot as .png, find packet stream start
# by brute-force scan, walk GIF stream, pickle events.
import struct, pickle, sys, glob

REGN = {0x00:'PRIM',0x01:'RGBAQ',0x02:'ST',0x03:'UV',0x04:'XYZF2',0x05:'XYZ2',
0x06:'TEX0_1',0x07:'TEX0_2',0x08:'CLAMP_1',0x09:'CLAMP_2',0x0a:'FOG',0x0c:'XYZF3',0x0d:'XYZ3',
0x14:'TEX1_1',0x15:'TEX1_2',0x16:'TEX2_1',0x17:'TEX2_2',0x18:'XYOFFSET_1',0x19:'XYOFFSET_2',
0x1a:'PRMODECONT',0x1b:'PRMODE',0x1c:'TEXCLUT',0x22:'SCANMSK',0x34:'MIPTBP1_1',0x35:'MIPTBP1_2',
0x36:'MIPTBP2_1',0x37:'MIPTBP2_2',0x3b:'TEXA',0x3d:'FOGCOL',0x3f:'TEXFLUSH',
0x40:'SCISSOR_1',0x41:'SCISSOR_2',0x42:'ALPHA_1',0x43:'ALPHA_2',0x44:'DIMX',0x45:'DTHE',
0x46:'COLCLAMP',0x47:'TEST_1',0x48:'TEST_2',0x49:'PABE',0x4a:'FBA_1',0x4b:'FBA_2',
0x4c:'FRAME_1',0x4d:'FRAME_2',0x4e:'ZBUF_1',0x4f:'ZBUF_2',0x50:'BITBLTBUF',0x51:'TRXPOS',
0x52:'TRXREG',0x53:'TRXDIR',0x54:'HWREG',0x60:'SIGNAL',0x61:'FINISH',0x62:'LABEL'}

class GifWalker:
    def __init__(self, out):
        self.state = 'TAG'
        self.nloop = 0; self.eop = 0; self.nreg = 0; self.regs = []
        self.reg_i = 0; self.out = out
    def qword(self, lo, hi):
        o = self.out
        if self.state == 'TAG':
            nloop = lo & 0x7fff; eop = (lo>>15)&1
            pre = (lo>>46)&1; prim = (lo>>47)&0x7ff; flg = (lo>>58)&3; nreg = (lo>>60)&0xf
            if nreg == 0: nreg = 16
            regs = [(hi>>(4*i))&0xf for i in range(nreg)]
            self.nloop = nloop; self.eop = eop; self.nreg = nreg; self.regs = regs; self.reg_i = 0
            o.append(('TAG', nloop, eop, flg, nreg, regs, pre, prim))
            if nloop == 0:
                if eop: o.append(('EOP',))
            else:
                self.state = ['PACKED','REGLIST','IMAGE','IMAGE'][flg]
        elif self.state == 'PACKED':
            r = self.regs[self.reg_i]
            if r == 0xe:
                reg = hi & 0xff
                o.append(('AD', REGN.get(reg, hex(reg)), lo))
            else:
                o.append(('PK', REGN.get(r, hex(r)), lo, hi))
            self.reg_i += 1
            if self.reg_i == self.nreg:
                self.reg_i = 0; self.nloop -= 1
                if self.nloop == 0:
                    if self.eop: o.append(('EOP',))
                    self.state = 'TAG'
        elif self.state == 'REGLIST':
            for v in (lo, hi):
                r = self.regs[self.reg_i]
                o.append(('RL', REGN.get(r, hex(r)), v))
                self.reg_i += 1
                if self.reg_i == self.nreg:
                    self.reg_i = 0; self.nloop -= 1
                    if self.nloop == 0: break
            if self.nloop == 0:
                if self.eop: o.append(('EOP',))
                self.state = 'TAG'
        elif self.state == 'IMAGE':
            if not o or o[-1][0] != 'IMG':
                o.append(['IMG', 0])
            o[-1][1] += 1
            self.nloop -= 1
            if self.nloop == 0:
                if self.eop: o.append(('EOP',))
                self.state = 'TAG'

def try_walk(d, off, end):
    """Return True if record stream parses cleanly from off to exactly end."""
    n = 0
    while off < end:
        pid = d[off]; off += 1
        if pid == 0:
            if off+5 > end: return False
            path = d[off]
            if path > 3: return False
            size = struct.unpack_from('<I', d, off+1)[0]
            if size == 0 or size % 16 or off+5+size > end: return False
            off += 5+size; n += 1
        elif pid == 1:
            off += 1; n += 1
        elif pid == 2:
            off += 4; n += 1
        elif pid == 3:
            off += 8192; n += 1
        else:
            return False
    return off == end and n > 50

def find_stream(d):
    end = len(d)
    for off in range(0x52c000, end - 1000):
        if d[off] in (0,1,2,3) and try_walk(d, off, end):
            return off
    return None

def decode(fn):
    d = open(fn,'rb').read()
    w_, h_ = struct.unpack_from('<II', d, 28)
    shotsz = struct.unpack_from('<I', d, 40)[0]
    slen = struct.unpack_from('<I', d, 20)[0]
    shotoff = 44 + slen
    # write screenshot as png
    try:
        from PIL import Image
        im = Image.frombytes('RGBA', (w_, h_), d[shotoff:shotoff+shotsz])
        im.convert('RGB').save(fn[:-3] + '.png')
    except Exception as e:
        print('  screenshot fail:', e)
    off = find_stream(d)
    if off is None:
        print('  NO STREAM FOUND'); return
    events = []
    walkers = {}
    end = len(d)
    nrec = 0
    while off < end:
        pid = d[off]; off += 1
        if pid == 0:
            path = d[off]; size = struct.unpack_from('<I', d, off+1)[0]
            data = d[off+5:off+5+size]; off += 5+size
            events.append(('XFER', path, size//16))
            if path not in walkers:
                walkers[path] = GifWalker(events)
            w = walkers[path]
            for i in range(0, len(data), 16):
                lo, hi = struct.unpack_from('<QQ', data, i)
                w.qword(lo, hi)
        elif pid == 1:
            events.append(('VSYNC', d[off])); off += 1
        elif pid == 2:
            off += 4
        elif pid == 3:
            off += 8192; events.append(('REGS',))
        nrec += 1
    pickle.dump(events, open(fn[:-3] + '.pkl','wb'))
    from collections import Counter
    c = Counter(e[0] for e in events)
    print(f'  stream@{off and hex(off)} recs={nrec} events={len(events)}', dict(c))

for fn in sorted(glob.glob(sys.argv[1] if len(sys.argv)>1 else '*.gs')):
    print(fn)
    decode(fn)
