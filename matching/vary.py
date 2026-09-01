#!/usr/bin/env python3
"""vary.py - score many source variants of ONE function in a single build.

Write a probe .c containing several differently-shaped versions of the
same function, all with the same signature, named NAME_a, NAME_b, ...
Then

    python3 vary.py probe.c 0x230018 0x4c

compiles the probe once (retail compiler + unpad) and scores EVERY
top-level function in it against the same ROM address, best first.
That turns "which source shape does gcc 2.9 lower this way" from a
guess into a sweep - the lesson of matching/campaign/README.md, where
the win came from source-level statement/declaration levers rather
than from the workbench's within-statement mutators.
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, "/u/aap/src/ps2rev/osdsys/matching/campaign")
os.environ.setdefault("EETOOLS", "/usr/local/sce/ee/gcc/bin")
import score1

GCC = "/usr/local/freesce/ee/gcc/bin/ee-gcc"
IMG = open(os.environ.get("IMG", "/u/aap/src/osdsys/expanded.bin"), "rb").read()


def build(src):
    base = os.path.join(HERE, "build", os.path.basename(src)[:-2])
    os.makedirs(os.path.join(HERE, "build"), exist_ok=True)
    subprocess.run([GCC, "-O2", "-Wall", "-fno-common", "-fno-strict-aliasing",
                    "-S", src, "-o", base + ".s"], check=True)
    if not os.environ.get("NOUNPAD"):
        subprocess.run([sys.executable, os.path.join(HERE, "unpad.py"),
                        base + ".s"])
    subprocess.run([GCC, "-c", base + ".s", "-o", base + ".o"], check=True)
    return base + ".o"


def main():
    src, va, size = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
    obj = build(src)
    names = []
    for line in subprocess.run([os.path.join(score1.TOOLS, "ee-nm"), obj],
                               capture_output=True, text=True).stdout.split("\n"):
        m = re.match(r"[0-9a-f]+ T (\w+)", line)
        if m:
            names.append(m.group(1))
    tbl = {n: (va, size) for n in names}
    got = score1.score(obj, IMG, tbl)
    rows = []
    for n, r in got.items():
        if r is None:
            continue
        rows.append((r["strict"], -r["aligned"], n, r))
    rows.sort()
    print("target 0x%06x  %d insns" % (va, size // 4))
    for strict, negal, n, r in rows:
        print("  %-20s %s strict=%-4d aligned=%-4d differ=%-3d "
              "missing=%-3d extra=%d"
              % (n, "MATCH " if strict == 0 else "      ",
                 strict, r["aligned"], r["differ"], r["missing"], r["extra"]))
    if rows and rows[0][0] != 0:
        print("\nbest (%s) first differing words:" % rows[0][2])
        for a, real, ours in rows[0][3]["rows"][:14]:
            print("  %s real %s  ours %s"
                  % ("%06x" % a if a else "  --  ",
                     "%08x" % real if real is not None else "--------",
                     "%08x" % ours if ours is not None else "--------"))


if __name__ == "__main__":
    main()
