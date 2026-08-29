#!/usr/bin/env python3
"""stmtperm.py - the variant family the n64-decomp-workbench does not have:
permute / relocate whole top-level statements inside a line range.

    python3 stmtperm.py SRC.c LO..HI FUNCTAB [--keep A,B,C] [--jobs N] [--out DIR]

Every one-line statement in SRC.c lines LO..HI is treated as movable; the
family is every permutation of them (capped), compiled with ee-gcc -O2 and
scored with check.py's strict masked compare against the OSDSYS image.
--keep names 1-based indices (into the block) whose relative order must be
preserved -- use it for statements with a real data dependency.

  # reproduces the ProcessOpeningAnimation match:
  python3 stmtperm.py ../src/anim.c 402..416 poa-functions.txt
"""
import concurrent.futures as cf, itertools, subprocess, sys
from pathlib import Path
import score1

GCC = "/usr/local/freesce/ee/gcc/bin/ee-gcc"
IMAGE = "/u/aap/src/osdsys/expanded.bin"
CAP = 40000

def main():
    a = sys.argv[1:]
    src = Path(a[0]); lo, hi = (int(x) for x in a[1].split("..")); ftab = a[2]
    def opt(n, d): return a[a.index(n) + 1] if n in a else d
    keep = [int(x) for x in opt("--keep", "").split(",") if x]
    jobs = int(opt("--jobs", 16)); out = Path(opt("--out", "stmtperm-out"))
    out.mkdir(exist_ok=True)
    funcs = score1.load_table(ftab); img = open(IMAGE, "rb").read()
    L = src.read_text().split("\n")
    block = [(i, L[i]) for i in range(lo - 1, hi) if L[i].strip()]
    idx = [i for i, _ in block]; texts = [t for _, t in block]
    print("%d movable statement(s) in %s:%d..%d" % (len(texts), src, lo, hi))
    slots = sorted(idx) + [i for i in range(lo - 1, hi) if L[i] not in ("",) and i not in idx]
    n = len(texts)
    if "--full" in a:
        gen = itertools.permutations(range(n))
    else:
        # single relocation: one statement moved to every other slot.  This is
        # the cheap O(n^2) family, and it is the one that closed
        # ProcessOpeningAnimation.
        def moves():
            yield tuple(range(n))
            for i in range(n):
                for j in range(n):
                    if i == j: continue
                    o = [x for x in range(n) if x != i]; o.insert(j, i)
                    yield tuple(o)
        gen = moves()
    seen = set(); cand = []
    for p in gen:
        if p in seen: continue
        seen.add(p)
        if keep and any(p.index(keep[k] - 1) > p.index(keep[k + 1] - 1)
                        for k in range(len(keep) - 1)):
            continue
        cand.append(p)
        if len(cand) >= CAP: break
    print("%d ordering(s)" % len(cand))

    def work(p):
        M = list(L)
        for slot, s in zip(sorted(idx), p):
            M[slot] = texts[s]
        f = out / ("p" + "".join("%x" % x for x in p) + ".c")
        f.write_text("\n".join(M))
        o = f.with_suffix(".o")
        r = subprocess.run([GCC, "-O2", "-c", str(f), "-o", str(o)],
                           capture_output=True, text=True)
        if r.returncode:
            return None
        try:
            s = score1.score(str(o), img, funcs)
        except Exception:
            return None
        tot = sum(v["strict"] for v in s.values() if v)
        return (tot, str(f), s)

    rows = []
    with cf.ThreadPoolExecutor(jobs) as ex:
        for r in ex.map(work, cand):
            if r: rows.append(r)
    rows.sort(key=lambda t: t[0])
    for tot, f, s in rows[:10]:
        print("%-30s %s" % (Path(f).name,
              "  ".join("%s strict=%d/%d" % (k, v["strict"], v["n"])
                        for k, v in s.items() if v)))
    if rows and rows[0][0] == 0:
        print("EXACT: %s" % rows[0][1])

if __name__ == "__main__":
    main()
