#!/usr/bin/env python3
"""run_family.py - compile a workbench sweep family with ee-gcc and rank it
with check.py's own strict masked score.

    python3 run_family.py DIR FUNCTAB [--jobs N] [--keep-best N]

DIR is a `decomp-workbench sweep <verb> --write DIR` directory (holds
sweep.json + the variant .c files), or any directory of .c files.
"""
import concurrent.futures as cf, json, os, subprocess, sys, shutil
from pathlib import Path
import score1

GCC = "/usr/local/freesce/ee/gcc/bin/ee-gcc"
IMAGE = "/u/aap/src/osdsys/expanded.bin"

def build_and_score(src, objdir, funcs, img):
    obj = Path(objdir) / (Path(src).stem + ".o")
    p = subprocess.run([GCC, "-O2", "-c", str(src), "-o", str(obj)],
                       capture_output=True, text=True)
    if p.returncode != 0:
        return (str(src), None, p.stderr.strip().splitlines()[-1:])
    try:
        r = score1.score(str(obj), img, funcs)
    except Exception as e:
        return (str(src), None, [str(e)])
    return (str(src), r, None)

def main():
    d = Path(sys.argv[1])
    funcs = score1.load_table(sys.argv[2])
    jobs = int(sys.argv[3]) if len(sys.argv) > 3 else 8
    img = open(IMAGE, "rb").read()
    objdir = d / "obj"; objdir.mkdir(exist_ok=True)
    srcs = sorted(p for p in d.glob("*.c"))
    labels = {}
    man = d / "sweep.json"
    if man.exists():
        j = json.load(open(man))
        for v in j.get("variants", []):
            labels[v["filename"]] = v.get("description", "") or v.get("label", "")
    rows = []
    with cf.ThreadPoolExecutor(jobs) as ex:
        for src, r, err in ex.map(lambda s: build_and_score(s, objdir, funcs, img), srcs):
            name = Path(src).name
            if r is None:
                rows.append((10**6, name, "BUILD FAIL: " + "; ".join(err)))
                continue
            for fn, s in r.items():
                if s is None:
                    rows.append((10**6, name, "%s: symbol absent" % fn)); continue
                rows.append((s["strict"], name,
                    "%s strict=%d/%d aligned=%d differ=%d miss=%d extra=%d ni=%d  %s"
                    % (fn, s["strict"], s["n"], s["aligned"], s["differ"],
                       s["missing"], s["extra"], s["insns"], labels.get(name, ""))))
    rows.sort()
    for score, name, txt in rows:
        print("%-34s %s" % (name, txt))
    return rows

if __name__ == "__main__":
    main()
