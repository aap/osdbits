#!/usr/bin/env python3
# Summarize a drawlog: one line per FRAME-target segment within each vsync.
import sys, re

fn = sys.argv[1]
vsync_pick = int(sys.argv[2]) if len(sys.argv) > 2 else None
cur = None
frame_i = 0

class Seg:
    def __init__(self, fbp, line):
        self.fbp = fbp; self.line = line
        self.draws = 0; self.verts = 0
        self.prims = {}
        self.texs = set()
        self.alphas = set()
        self.tests = set()
        self.xy = None
        self.imgs = 0
        self.blts = set()

segs = []
lineno = 0
for line in open(fn):
    lineno += 1
    line = line.rstrip('\n')
    if line.startswith('====='):
        frame_i += 1
        continue
    if vsync_pick is not None and frame_i != vsync_pick:
        continue
    m = re.match(r'  FRAME fbp=(\d+)', line)
    if m:
        fbp = int(m.group(1))
        if cur is None or cur.fbp != fbp:
            cur = Seg(fbp, lineno)
            segs.append(cur)
        continue
    if cur is None:
        cur = Seg(-1, lineno)
        segs.append(cur)
    m = re.match(r'D\d+ (\S+) n=(\d+) x=\(([-\d.]+),([-\d.]+)\) y=\(([-\d.]+),([-\d.]+)\)', line)
    if m:
        p = m.group(1)
        cur.draws += 1
        cur.verts += int(m.group(2))
        cur.prims[p] = cur.prims.get(p, 0) + 1
        x0,x1,y0,y1 = (float(m.group(i)) for i in (3,4,5,6))
        if cur.xy is None:
            cur.xy = [x0,y0,x1,y1]
        else:
            cur.xy[0]=min(cur.xy[0],x0); cur.xy[1]=min(cur.xy[1],y0)
            cur.xy[2]=max(cur.xy[2],x1); cur.xy[3]=max(cur.xy[3],y1)
        continue
    m = re.match(r'  TEX0 tbp=(\d+)', line)
    if m:
        cur.texs.add(int(m.group(1))); continue
    m = re.match(r'  ALPHA (\S+.*)', line)
    if m:
        cur.alphas.add(m.group(1)); continue
    m = re.match(r'  TEST (.*)', line)
    if m:
        cur.tests.add(m.group(1)); continue
    if line.startswith('  IMAGE'):
        cur.imgs += 1; continue
    m = re.match(r'  BLT src bp=(\d+).*-> dst bp=(\d+)', line)
    if m:
        cur.blts.add((int(m.group(1)), int(m.group(2)))); continue

for s in segs:
    if s.draws == 0 and s.imgs == 0:
        continue
    prims = ','.join(f'{k}x{v}' for k,v in s.prims.items())
    xy = f' bbox=({s.xy[0]:.0f},{s.xy[1]:.0f})-({s.xy[2]:.0f},{s.xy[3]:.0f})' if s.xy else ''
    tx = (' tex=' + ','.join(str(t) for t in sorted(s.texs))) if s.texs else ''
    al = (' A=' + '|'.join(sorted(s.alphas))) if s.alphas else ''
    im = f' IMG={s.imgs}' if s.imgs else ''
    bl = (' blt=' + ','.join(f'{a}->{b}' for a,b in sorted(s.blts))) if s.blts else ''
    print(f'L{s.line:5d} FB={s.fbp:5d} d={s.draws:3d} v={s.verts:4d} {prims}{xy}{tx}{al}{im}{bl}')
