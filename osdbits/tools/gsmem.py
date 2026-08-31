import sys, zipfile, struct
from PIL import Image

blockTable32 = [
 [ 0,  1,  4,  5, 16, 17, 20, 21],
 [ 2,  3,  6,  7, 18, 19, 22, 23],
 [ 8,  9, 12, 13, 24, 25, 28, 29],
 [10, 11, 14, 15, 26, 27, 30, 31]]
columnTable32 = [
 [ 0,  1,  4,  5,  8,  9, 12, 13],
 [ 2,  3,  6,  7, 10, 11, 14, 15],
 [16, 17, 20, 21, 24, 25, 28, 29],
 [18, 19, 22, 23, 26, 27, 30, 31],
 [32, 33, 36, 37, 40, 41, 44, 45],
 [34, 35, 38, 39, 42, 43, 46, 47],
 [48, 49, 52, 53, 56, 57, 60, 61],
 [50, 51, 54, 55, 58, 59, 62, 63]]

def addr32(x, y, tbp, bw):
    page = tbp//32 + (y//32)*bw + (x//64)
    px, py = x % 64, y % 32
    blk = blockTable32[py//8][px//8]
    col = columnTable32[py % 8][px % 8]
    return page*2048 + blk*64 + col

def load(path, off):
    z = zipfile.ZipFile(path)
    return z.read('GS.bin')[off:off+4194304]

def grab(mem, tbp, bw, w, h):
    out = bytearray(w*h*4)
    for y in range(h):
        for x in range(w):
            a = addr32(x, y, tbp, bw)*4
            out[(y*w+x)*4:(y*w+x)*4+4] = mem[a:a+4]
    return bytes(out)

def topng(raw, w, h, name, alpha=False):
    img = Image.new('RGB', (w, h)); px = img.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = raw[(y*w+x)*4:(y*w+x)*4+4]
            px[x, y] = (a, a, a) if alpha else (r, g, b)
    img = img.resize((w, h*2), Image.NEAREST)
    img.save(name)

if __name__ == '__main__':
    p = sys.argv[1]; off = int(sys.argv[2])
    mem = load(p, off)
    W, H = 640, 224
    for name, tbp in (('scr0', 0), ('scr1', 2240), ('wb3', 6720), ('wb4', 8960)):
        raw = grab(mem, tbp, 10, W, H)
        topng(raw, W, H, 'gs_%s.png' % name)
        topng(raw, W, H, 'gs_%s_a.png' % name, alpha=True)
        print(name, 'tbp', tbp, 'first px', raw[:8].hex())
