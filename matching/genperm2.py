#!/usr/bin/env python3
"""genperm2.py SPEC.py > probe.c - generic single-statement-relocation
probe generator.  SPEC.py must define PRE (file header), PROTO (function
signature with %s for the name), HEAD (statements before the block),
STMTS (list of movable statements), TAIL (statements after), and
optionally BASE (initial order).  Emits every single relocation of BASE.
"""
import sys

spec = {}
exec(open(sys.argv[1]).read(), spec)
PRE, PROTO = spec["PRE"], spec["PROTO"]
HEAD, STMTS, TAIL = spec["HEAD"], spec["STMTS"], spec["TAIL"]
BASE = spec.get("BASE", list(range(len(STMTS))))
if len(sys.argv) > 2:
    BASE = [int(x) for x in sys.argv[2].split(",")]

out = [PRE]
seen = set()
n = 0
for i in range(len(BASE)):
    for j in range(len(BASE) + 1):
        o = BASE[:]
        v = o.pop(i)
        o.insert(j if j <= i else j - 1, v)
        k = tuple(o)
        if k in seen:
            continue
        seen.add(k)
        body = "\n\t".join([HEAD] + [STMTS[x] for x in o] + [TAIL])
        out.append(PROTO % ("p%03d" % n) + "{\n\t" + body + "\n}\n")
        n += 1
sys.stdout.write("".join(out))
sys.stderr.write("genperm2: %d variants\n" % n)
