#!/usr/bin/env python3
"""runall.py POOLDIR FUNCTAB [JOBS] - compile every .c under POOLDIR (recursively)
with ee-gcc -O2 and rank by check.py's strict masked score."""
import concurrent.futures as cf, json, subprocess, sys
from pathlib import Path
import score1

GCC = "/usr/local/freesce/ee/gcc/bin/ee-gcc"
IMAGE = "/u/aap/src/osdsys/expanded.bin"

def labels_for(root):
    out = {}
    for man in Path(root).rglob("sweep.json"):
        try: j = json.load(open(man))
        except Exception: continue
        for v in j.get("variants", []):
            out[str(man.parent / v["filename"])] = v.get("description", "")
    return out

def work(src, funcs, img, objroot):
    obj = objroot / (src.parent.name + "__" + src.stem + ".o")
    p = subprocess.run([GCC, "-O2", "-c", str(src), "-o", str(obj)],
                       capture_output=True, text=True)
    if p.returncode != 0:
        return (src, None, (p.stderr.strip().splitlines() or ["?"])[-1])
    try:
        return (src, score1.score(str(obj), img, funcs), None)
    except Exception as e:
        return (src, None, str(e))

def main():
    root = Path(sys.argv[1]); funcs = score1.load_table(sys.argv[2])
    jobs = int(sys.argv[3]) if len(sys.argv) > 3 else 8
    img = open(IMAGE, "rb").read()
    objroot = root / "_obj"; objroot.mkdir(parents=True, exist_ok=True)
    lab = labels_for(root)
    srcs = sorted(p for p in root.rglob("*.c") if "_obj" not in p.parts)
    rows, fails = [], 0
    with cf.ThreadPoolExecutor(jobs) as ex:
        for src, r, err in ex.map(lambda s: work(s, funcs, img, objroot), srcs):
            if r is None:
                fails += 1; continue
            for fn, s in r.items():
                if s is None: continue
                rows.append((s["strict"], s["differ"] + s["missing"] + s["extra"],
                             str(src), fn, s, lab.get(str(src), "")))
    rows.sort(key=lambda t: (t[0], t[1], t[2]))
    print("# %d built, %d rejected by the compiler" % (len(rows), fails))
    for strict, _, src, fn, s, d in rows[:40]:
        print("%-46s %s strict=%d/%d aligned=%d differ=%d miss=%d extra=%d  %s"
              % (Path(src).parent.name + "/" + Path(src).name, fn, strict, s["n"],
                 s["aligned"], s["differ"], s["missing"], s["extra"], d))
    return rows

if __name__ == "__main__":
    main()
