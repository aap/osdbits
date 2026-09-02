#!/usr/bin/env python3
"""status.py - union all TU verdicts by ROM address; list unmatched
Module U functions <= a size cap."""
import os, sys
sys.path.insert(0, "ref/campaign")
os.environ.setdefault("EETOOLS", "/usr/local/sce/ee/gcc/bin")
import score1

IMG = open("/u/aap/src/osdsys/expanded.bin", "rb").read()
LO, HI = 0x21C910, 0x230018
CAP = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x120

JOBS = [("ref/build/%s.o" % t, "ref/%s-functions.txt" % t)
        for t in ("anim","blit","config","flare","fog","leaf",
                  "matrixdrive","menu","menudraw")]
JOBS.append(("ref/build/text.o", "ref/functions.txt"))
# my own work, if present
import glob
for t in sorted(glob.glob("*-functions.txt")):
    b = t[:-len("-functions.txt")]
    o = "build/%s.o" % b
    if os.path.exists(o):
        JOBS.append((o, t))

inv = {}
for l in open("inventory.tsv"):
    if l.startswith("#"): continue
    f = l.rstrip("\n").split("\t")
    inv[int(f[0],16)] = (int(f[1],16), f[2])

byaddr = {}
for obj, tab in JOBS:
    funcs = score1.load_table(tab)
    for name, r in score1.score(obj, IMG, funcs).items():
        va, sz = funcs[name]
        ok = r is not None and r["strict"] == 0
        if va not in byaddr or (ok and not byaddr[va][0]):
            byaddr[va] = (ok, sz, name)

inU = {a:v for a,v in byaddr.items() if LO <= a < HI}
nm = sum(1 for v in inU.values() if v[0])
print("Module U matched by address: %d of %d attempted (of %d total)" % (nm, len(inU), len(inv)))
print()
print("--- unmatched or untouched, size <= 0x%x ---" % CAP)
n = ni = 0
for a in sorted(inv):
    sz, name = inv[a]
    if sz > CAP: continue
    st = byaddr.get(a)
    if st and st[0]: continue
    tag = "attempted(%s)" % st[2] if st else ""
    print("0x%06x %#6x %-24s %s" % (a, sz, name, tag))
    n += 1; ni += sz//4
print("total: %d functions, %d insns" % (n, ni))
