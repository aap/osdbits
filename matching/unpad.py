#!/usr/bin/env python3
"""unpad.py - remove gcc 2.9-ee-991111's R5900 short-loop nop padding
from an assembly file.

`mips_r5900_lengthen_loops' (config/mips/mips.c:8717, introduced in
2.9-ee-991111-01 and absent from every earlier ee/sky tree) pads any
loop of <= 6 instructions out to 7 by emitting nops before the closing
branch.  gcc 2.9's `condjump_p' returns 1 for a plain `(set (pc)
(label_ref))' too, so the pass fires on UNCONDITIONAL self-jumps as
well - e.g. the `for(;;);' after a panic printf.

Retail is 991111: flare_21A6D8 (0x21a6d8) matches byte-exact WITH
three padded `bc1t' angle-wrap loops, and 990721 - the newest tree
without the pass - loses 19 of the 126 already-matched functions.
But retail's PANIC loops are NOT padded (MatrixDrive_PushMatrix's at
0x2300e8 is a bare `b .' + nop), so retail's panic block is not a
compiler-visible `for(;;)' - most likely inline asm.  Stripping only
the UNCONDITIONALLY-closed pads reproduces both facts.

Signature: a run of the exact three-line sequence

	.set	noreorder
	nop
	.set	reorder

A genuine load-delay nop is emitted COMMENTED (`#nop`), so it is not
touched.  Only runs of >= MINRUN are removed, and only when the next
real instruction is a branch/jump - the pass's exact shape.
"""
import re
import sys

MINRUN = 2
SET_NO = re.compile(r"^\s*\.set\s+noreorder\s*$")
SET_RE = re.compile(r"^\s*\.set\s+reorder\s*$")
NOP = re.compile(r"^\s*nop\s*$")
# UNCONDITIONAL closing branch only.  Retail KEEPS the pad on
# conditional short loops - flare_21A6D8 (0x21a6d8) matches byte-exact
# WITH three padded `bc1t' angle-wrap loops - and lacks it only on the
# unconditional panic self-loop.  So the pass did run for retail; what
# retail's panic block is NOT is a compiler-visible `for(;;)'.  Limit
# the strip to `b'/`j' and both facts are reproduced.
BRANCH = re.compile(r"^\s*(b|j)\s+\$?[A-Za-z_$.]")


def unpad(lines):
    out = []
    i = 0
    n = len(lines)
    while i < n:
        # try to consume a maximal run of noreorder/nop/reorder triples
        j = i
        run = 0
        while (j + 2 < n and SET_NO.match(lines[j]) and NOP.match(lines[j + 1])
               and SET_RE.match(lines[j + 2])):
            run += 1
            j += 3
        if run >= MINRUN:
            # next non-blank, non-comment line must be a branch
            k = j
            while k < n and (not lines[k].strip()
                             or lines[k].lstrip().startswith(("#", "."))):
                if lines[k].lstrip().startswith(".") and \
                        not lines[k].lstrip().startswith((".set", ".loc",
                                                          ".stab", ".p2align",
                                                          ".align")):
                    break
                k += 1
            if k < n and BRANCH.match(lines[k]):
                i = j                      # drop the whole run
                continue
        out.append(lines[i])
        i += 1
    return out


def main():
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src
    lines = open(src).read().split("\n")
    new = unpad(lines)
    if len(new) != len(lines):
        sys.stderr.write("unpad: removed %d short-loop pad lines from %s\n"
                         % (len(lines) - len(new), src))
    open(dst, "w").write("\n".join(new))


if __name__ == "__main__":
    main()
