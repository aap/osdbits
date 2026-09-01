#!/usr/bin/env python3
# Walk a decoded .pkl and emit a per-draw log: GS state + vertex kicks.
import pickle, sys
VERBOSE = False

PRIMN = ['POINT','LINE','LINESTRIP','TRI','TRISTRIP','TRIFAN','SPRITE','INVALID']

def fr(v):  # FRAME
    return f'fbp={(v&0x1ff)*32} fbw={(v>>16)&0x3f} psm={(v>>24)&0x3f}' + (f' msk={v>>32:08x}' if v>>32 else '')
def tex0(v):
    return (f'tbp={v&0x3fff} tbw={(v>>14)&0x3f} psm={(v>>20)&0x3f} '
            f'tw={1<<((v>>26)&0xf)} th={1<<((v>>30)&0xf)} tcc={(v>>34)&1} tfx={(v>>35)&3}'
            + (f' cbp={(v>>37)&0x3fff}' if (v>>37)&0x3fff else ''))
def test(v):
    s = []
    if v&1: s.append(f'ATE atst={(v>>1)&7} aref={(v>>4)&0xff} afail={(v>>12)&3}')
    if (v>>14)&1: s.append(f'DATE datm={(v>>15)&1}')
    if (v>>16)&1: s.append(f'ZTE ztst={(v>>17)&3}')
    return ' '.join(s) or 'off'
def alpha(v):
    return f'{(v)&3}{(v>>2)&3}{(v>>4)&3}{(v>>6)&3}' + (f' fix={(v>>32)&0xff}' if ((v>>4)&3)==2 else '')
def primdec(p):
    f = []
    if p&8: f.append('IIP')
    if p&16: f.append('TME')
    if p&32: f.append('FGE')
    if p&64: f.append('ABE')
    if p&128: f.append('AA1')
    if p&256: f.append('FST')
    if p&512: f.append('CTX2')
    return PRIMN[p&7] + ('+' + '+'.join(f) if f else '')
def zbuf(v):
    return f'zbp={(v&0x1ff)*32} psm={(v>>24)&0xf}' + (' ZMSK' if (v>>32)&1 else '')

class St:
    lastuv = (0.0, 0.0)

def run(pklfn, outfn):
    ev = pickle.load(open(pklfn,'rb'))
    out = open(outfn, 'w')
    st = {}
    prim = None
    verts = []          # list of (x,y,z) in pixels
    rgba = None
    firstrgba = None
    uvmin = uvmax = None
    dirty = []
    nvtx_total = 0
    drawidx = 0
    def flush():
        nonlocal verts, drawidx, uvmin, uvmax, firstrgba
        if not verts:
            return
        xs = [v[0] for v in verts]; ys = [v[1] for v in verts]; zs = [v[2] for v in verts]
        uv = ''
        if uvmin is not None:
            uv = f' uv=({uvmin[0]:.0f},{uvmin[1]:.0f})-({uvmax[0]:.0f},{uvmax[1]:.0f})'
        col = f' rgba={firstrgba}' if firstrgba else ''
        out.write(f'D{drawidx:04d} {primdec(prim) if prim is not None else "?"} n={len(verts)}'
                  f' x=({min(xs):.1f},{max(xs):.1f}) y=({min(ys):.1f},{max(ys):.1f})'
                  f' z=({min(zs)},{max(zs)}){uv}{col}\n')
        drawidx += 1
        verts = []; uvmin = uvmax = None; firstrgba = None
    def setreg(name, v):
        nonlocal prim, rgba, firstrgba, uvmin, uvmax
        if name in ('XYZ2','XYZF2','XYZ3','XYZF3'):
            x = (v&0xffff)/16.0; y = ((v>>16)&0xffff)/16.0
            z = (v>>32) & (0xffffff if 'F' in name else 0xffffffff)
            if firstrgba is None: firstrgba = rgba
            verts.append((x,y,z))
            if VERBOSE:
                u = St.lastuv
                out.write(f'    v ({x:.1f},{y:.1f},{z}) uv=({u[0]:.2f},{u[1]:.2f}) rgba={rgba}\n')
            return
        if name == 'RGBAQ':
            rgba = f'{v&0xff:02x}{(v>>8)&0xff:02x}{(v>>16)&0xff:02x}{(v>>24)&0xff:02x}'
            return
        if name == 'UV':
            u = (v&0x3fff)/16.0; vv = ((v>>16)&0x3fff)/16.0
            St.lastuv = (u, vv)
            if uvmin is None:
                uvmin = [u,vv]; uvmax = [u,vv]
            else:
                uvmin[0]=min(uvmin[0],u); uvmin[1]=min(uvmin[1],vv)
                uvmax[0]=max(uvmax[0],u); uvmax[1]=max(uvmax[1],vv)
            return
        if name == 'ST' or name == 'FOG' or name == 'NOP' or name=='TEXFLUSH':
            return
        # state register: flush current draw, log the change
        flush()
        if name == 'PRIM':
            prim = v & 0x7ff
            out.write(f'  PRIM {primdec(prim)}\n')
        elif name == 'FRAME_1':
            out.write(f'  FRAME {fr(v)}\n')
        elif name == 'TEX0_1':
            out.write(f'  TEX0 {tex0(v)}\n')
        elif name == 'TEST_1':
            out.write(f'  TEST {test(v)}\n')
        elif name == 'ALPHA_1':
            out.write(f'  ALPHA {alpha(v)}\n')
        elif name == 'ZBUF_1':
            out.write(f'  ZBUF {zbuf(v)}\n')
        elif name == 'XYOFFSET_1':
            out.write(f'  XYOFF ({(v&0xffff)/16.0},{((v>>32)&0xffff)/16.0})\n')
        elif name == 'SCISSOR_1':
            out.write(f'  SCISSOR x={v&0x7ff}-{(v>>16)&0x7ff} y={(v>>32)&0x7ff}-{(v>>48)&0x7ff}\n')
        elif name == 'BITBLTBUF':
            out.write(f'  BLT src bp={(v&0x3fff)} bw={(v>>16)&0x3f} psm={(v>>24)&0x3f} -> dst bp={(v>>32)&0x3fff} bw={(v>>48)&0x3f} psm={(v>>56)&0x3f}\n')
        elif name == 'TRXPOS':
            out.write(f'  TRXPOS s=({v&0x7ff},{(v>>16)&0x7ff}) d=({(v>>32)&0x7ff},{(v>>48)&0x7ff}) dir={(v>>59)&3}\n')
        elif name == 'TRXREG':
            out.write(f'  TRXREG {v&0xfff}x{(v>>32)&0xfff}\n')
        elif name == 'TRXDIR':
            out.write(f'  TRXDIR {v&3}\n')
        else:
            out.write(f'  {name} {v:016x}\n')
    for e in ev:
        k = e[0]
        if k == 'VSYNC':
            flush()
            out.write(f'===== VSYNC field={e[1]} =====\n')
        elif k == 'TAG':
            _, nloop, eop, flg, nreg, regs, pre, primv = e
            if pre and nloop:
                flush()
                prim = primv
                out.write(f'  PRIM(tag) {primdec(prim)}\n')
            if flg == 2 or flg == 3:
                flush()
        elif k == 'AD':
            setreg(e[1], e[2])
        elif k == 'PK':
            name, lo, hi = e[1], e[2], e[3]
            if name == 'XYZF2' or name == 'XYZ2':
                # packed formats: X in lo[0:16], Y in lo[32:48], Z in hi
                x = (lo & 0xffff)/16.0; y = ((lo>>32)&0xffff)/16.0
                if name == 'XYZF2':
                    z = (hi>>4) & 0xffffff
                    adc = (hi>>47)&1
                else:
                    z = hi & 0xffffffff
                    adc = (hi>>47)&1
                nm = name + ('adc' if adc else '')
                setreg('XYZ3' if adc else 'XYZ2', (int(x*16)&0xffff) | ((int(y*16)&0xffff)<<16) | (z<<32))
            elif name == 'RGBAQ':
                setreg('RGBAQ', (lo&0xff) | (((lo>>32)&0xff)<<8) | ((hi&0xff)<<16) | (((hi>>32)&0xff)<<24))
            elif name == 'UV':
                setreg('UV', (lo&0x3fff) | (((lo>>32)&0x3fff)<<16))
            elif name == 'ST':
                pass
            elif name == 'PRIM':
                setreg('PRIM', lo)
            elif name == 'TEX0_1':
                setreg('TEX0_1', lo)
            elif name == 'FOG':
                pass
            else:
                flush()
                out.write(f'  PK:{name} {lo:016x} {hi:016x}\n')
        elif k == 'RL':
            # REGLIST: raw 64-bit register values, register from tag list
            name, v = e[1], e[2]
            setreg(name, v)
        elif k == 'IMG':
            flush()
            out.write(f'  IMAGE {e[1]} qwords\n')
        elif k == 'REGS':
            pass
    flush()
    out.close()

VERBOSE = len(sys.argv) > 3
run(sys.argv[1], sys.argv[2])
print('ok', sys.argv[2])
