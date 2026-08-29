#!/usr/bin/env python3
"""search.py - beam search over the n64-decomp-workbench sweep generators,
scored with check.py's strict masked compare against the OSDSYS image.

    python3 search.py BASE.c FUNCTAB [--depth N] [--beam N] [--jobs N]
                      [--lines LO..HI] [--out DIR]

The neighbourhood of a source is every variant the workbench's own
generators mint from it: commutative flips (C), copy elimination (K),
the four hoist classes into every dead carrier (H/O/P/A) at every line,
and live-range fusion (F) into every local.  Each neighbour is compiled
with ee-gcc -O2 and scored; the best distinct sources seed the next
round.
"""
from __future__ import annotations
import concurrent.futures as cf, hashlib, os, subprocess, sys, tempfile
from pathlib import Path

sys.path.insert(0, "/u/aap/othersrc/n64-decomp-workbench/src")
from decomp_workbench import sweep_generators as G
from decomp_workbench.csource import declarations
import score1

GCC = "/usr/local/freesce/ee/gcc/bin/ee-gcc"
IMAGE = "/u/aap/src/osdsys/expanded.bin"


def neighbours(path: Path, lo: int, hi: int):
    """Every variant text the workbench generators mint from `path`."""
    out: dict[str, str] = {}          # sha -> (text, label)
    labels: dict[str, str] = {}
    def take(manifest):
        for v in manifest.variants:
            h = hashlib.sha256(v.text.encode()).hexdigest()[:16]
            out.setdefault(h, v.text)
            labels.setdefault(h, v.key.label + ": " + v.description)
    def attempt(fn, **kw):
        try:
            take(fn(path, **kw))
        except Exception:
            pass
    attempt(G.commutative_family)
    attempt(G.copy_family)
    for line in range(lo, hi + 1):
        attempt(G.hoist_family, line=line, classes=("H", "O", "P", "A"))
    code = G._read_source(path)[1]
    for d in declarations(code):
        if not d.is_array:
            attempt(G.fusion_family, target=d.name)
    return out, labels


def build_and_score(item, workdir, funcs, img):
    h, text = item
    src = workdir / (h + ".c")
    src.write_text(text)
    obj = workdir / (h + ".o")
    p = subprocess.run([GCC, "-O2", "-c", str(src), "-o", str(obj)],
                       capture_output=True, text=True)
    if p.returncode != 0:
        return h, None
    try:
        r = score1.score(str(obj), img, funcs)
    except Exception:
        return h, None
    return h, r


def total(r, funcs):
    """(strict, differ+missing+extra) summed over the wanted functions."""
    s = d = 0
    for name in funcs:
        v = r.get(name)
        if v is None:
            return (10**6, 10**6)
        s += v["strict"]; d += v["differ"] + v["missing"] + v["extra"]
    return (s, d)


def main():
    a = sys.argv[1:]
    base = Path(a[0]); ftab = a[1]
    def opt(name, dflt):
        return a[a.index(name) + 1] if name in a else dflt
    depth = int(opt("--depth", 3)); beam = int(opt("--beam", 6))
    jobs = int(opt("--jobs", 12))
    outdir = Path(opt("--out", "search-out")); outdir.mkdir(exist_ok=True)
    lo, hi = (int(x) for x in opt("--lines", "1..%d" % len(base.read_text().splitlines())).split(".."))
    funcs = score1.load_table(ftab)
    img = open(IMAGE, "rb").read()

    seen: set[str] = set()
    b0 = build_and_score((hashlib.sha256(base.read_text().encode()).hexdigest()[:16],
                          base.read_text()), outdir, funcs, img)
    best = total(b0[1], funcs)
    print("base %s -> strict=%d rows=%d" % (base, best[0], best[1]))
    frontier = [(best, base.read_text(), "base")]
    seen.add(hashlib.sha256(base.read_text().encode()).hexdigest()[:16])
    overall = (best, base.read_text(), "base")
    tried = 0

    for round_ in range(1, depth + 1):
        pool: dict[str, str] = {}; labs: dict[str, str] = {}
        parent: dict[str, str] = {}
        for sc, text, lab in frontier:
            tmp = outdir / ("base%d.c" % abs(hash(text)) % 10**8) if False else None
            f = tempfile.NamedTemporaryFile("w", suffix=".c", delete=False, dir=outdir)
            f.write(text); f.close()
            n, nl = neighbours(Path(f.name), lo, hi)
            os.unlink(f.name)
            for h, t in n.items():
                if h in seen or h in pool:
                    continue
                pool[h] = t; labs[h] = nl[h]; parent[h] = lab
        print("round %d: %d new candidate(s) from %d base(s)"
              % (round_, len(pool), len(frontier)))
        if not pool:
            break
        results = []
        with cf.ThreadPoolExecutor(jobs) as ex:
            for h, r in ex.map(lambda kv: build_and_score(kv, outdir, funcs, img),
                               pool.items()):
                seen.add(h); tried += 1
                if r is None:
                    continue
                results.append((total(r, funcs), h))
        results.sort()
        if not results:
            break
        print("  built %d; best this round strict=%d rows=%d  (%s <- %s)"
              % (len(results), results[0][0][0], results[0][0][1],
                 labs[results[0][1]], parent[results[0][1]]))
        for sc, h in results[:8]:
            print("    strict=%-4d rows=%-4d %s  [%s]" % (sc[0], sc[1], labs[h], h))
        if results[0][0] < overall[0]:
            overall = (results[0][0], pool[results[0][1]], labs[results[0][1]])
            (outdir / "best.c").write_text(overall[1])
        frontier = [(sc, pool[h], labs[h]) for sc, h in results[:beam]]
        if results[0][0][0] == 0:
            print("EXACT MATCH"); break
    print("tried %d variant(s); best strict=%d rows=%d via %s"
          % (tried, overall[0][0], overall[0][1], overall[2]))
    (outdir / "best.c").write_text(overall[1])


if __name__ == "__main__":
    main()
