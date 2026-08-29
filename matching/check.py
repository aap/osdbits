#!/usr/bin/env python3
# check.py - compare functions in a compiled .o against the real OSDSYS
# image, masking relocation-affected bit fields.
#
#     python3 check.py build/text.o path/to/expanded-osdsys.bin [functions.txt]
#
# Function addresses come from functions.txt ("name va size" per line,
# hex) or the table named as the third argument.  The toolchain
# (ee-nm/ee-objcopy/ee-objdump) is taken from $EETOOLS or
# /usr/local/sce/ee/gcc/bin.

import difflib
import os
import re
import struct
import subprocess
import sys
import tempfile

TOOLS = os.environ.get("EETOOLS", "/usr/local/sce/ee/gcc/bin")
BASE = 0x200000

def tool(name, *args):
    return subprocess.run([os.path.join(TOOLS, name), *args],
        capture_output=True, text=True).stdout

def main():
    obj, image = sys.argv[1], sys.argv[2]
    img = open(image, "rb").read()

    funcs = {}
    ftab = sys.argv[3] if len(sys.argv) > 3 else \
        os.path.join(os.path.dirname(__file__) or ".", "functions.txt")
    with open(ftab) as f:
        for line in f:
            line = line.split("#")[0].split()
            if len(line) == 3:
                funcs[line[0]] = (int(line[1], 16), int(line[2], 16))

    # symbol -> .text offset
    syms = {}
    for line in tool("ee-nm", obj).splitlines():
        m = re.match(r"([0-9a-f]+) [tT] (\w+)", line)
        if m:
            syms[m.group(2)] = int(m.group(1), 16)

    # relocation offsets -> type; ONLY the .text section's records (a
    # .rodata jump table's R_MIPS_32 entries alias low .text offsets
    # and would silently mask real instructions)
    relocs = {}
    sec = None
    for line in tool("ee-objdump", "-r", obj).splitlines():
        m = re.match(r"RELOCATION RECORDS FOR \[(\S+)\]", line)
        if m:
            sec = m.group(1)
            continue
        m = re.match(r"([0-9a-f]+) (R_MIPS_\w+)", line)
        if m and sec == ".text":
            relocs[int(m.group(1), 16)] = m.group(2)

    with tempfile.NamedTemporaryFile(suffix=".bin") as tf:
        subprocess.run([os.path.join(TOOLS, "ee-objcopy"), "-O", "binary",
            "-j", ".text", obj, tf.name], check=True)
        text = open(tf.name, "rb").read()

    def mask(off):
        t = relocs.get(off)
        if t in ("R_MIPS_HI16", "R_MIPS_LO16", "R_MIPS_GPREL16", "R_MIPS_LITERAL"):
            return 0xFFFF0000
        if t == "R_MIPS_26":
            return 0xFC000000
        if t == "R_MIPS_32":
            return 0
        return 0xFFFFFFFF

    # opcode-derived normalization for ALIGNMENT only (applied to both
    # sides symmetrically): blank out fields that legitimately shift
    # when an instruction is inserted/removed or an address moves -
    # branch offsets, j/jal targets, and every I-type immediate.
    # COP0/1/2 (16-19) and MMI (28) encode registers down low, so only
    # the cop1 branch (rs=8) is blanked there; SPECIAL (0) never is.
    IMM16 = set(range(4, 16)) | set(range(20, 28)) | set(range(30, 64)) | {1}
    def norm(word):
        op = word >> 26
        if op in (2, 3):
            return word & 0xFC000000
        if op in IMM16 or (op == 17 and (word >> 21) & 31 == 8):
            return word & 0xFFFF0000
        return word

    # optional literal-pool check: a "#lit4 <va>" line in the functions
    # table names where this TU's .lit4 pool starts in the image.  Reloc
    # masking hides WHICH literal an instruction references, so two
    # sources with different constants or different literal ORDER can
    # both score byte-exact (bitten twice: a 0.9f read as 0.1f, and a
    # statement swap that reordered the pool) - comparing the pool
    # bytes themselves closes the hole.
    lit4va = None
    with open(ftab) as f:
        for line in f:
            m = re.match(r"#lit4\s+0x([0-9a-f]+)", line.strip(), re.I)
            if m:
                lit4va = int(m.group(1), 16)
    if lit4va is not None:
        with tempfile.NamedTemporaryFile(suffix=".bin") as tf:
            subprocess.run([os.path.join(TOOLS, "ee-objcopy"), "-O", "binary",
                "-j", ".lit4", obj, tf.name], check=True)
            lit = open(tf.name, "rb").read()
        bad = [i for i in range(0, len(lit), 4)
            if lit[i:i+4] != img[lit4va - BASE + i:lit4va - BASE + i + 4]]
        if bad:
            print(".lit4 pool: %d/%d words differ from image @%x:" %
                (len(bad), len(lit)//4, lit4va))
            for i in bad[:8]:
                print("  +%03x: real %s ours %s" % (i,
                    img[lit4va - BASE + i:lit4va - BASE + i + 4][::-1].hex(),
                    lit[i:i+4][::-1].hex()))
        else:
            print(".lit4 pool: %d words MATCH @%x" % (len(lit)//4, lit4va))

    total = ok = 0
    for name, (va, size) in funcs.items():
        if name not in syms:
            continue
        soff = syms[name]

        n = size // 4
        avail = max(0, min(n, (len(text) - soff) // 4))
        ours = list(struct.unpack_from("<%dI" % avail, text, soff))
        real = list(struct.unpack_from("<%dI" % n, img, va - BASE))
        masks = [mask(soff + 4*i) for i in range(avail)]

        # strict positional pass - the actual MATCH criterion
        bad = sum(1 for i in range(n)
            if i >= avail or ours[i] & masks[i] != real[i] & masks[i])
        total += 1
        ok += bad == 0
        if bad == 0:
            print("%-24s MATCH  (%d insns)" % (name, n))
            continue

        # resync pass: align normalized token streams so one extra or
        # missing instruction costs one line, not the rest of the
        # function.  a=real, b=ours.
        sm = difflib.SequenceMatcher(None,
            [norm(w) for w in real],
            [norm(w) & masks[i] for i, w in enumerate(ours)],
            autojunk=False)
        neq = ndiff = 0
        lines = []
        def pair(i, j):
            m = masks[j]
            if ours[j] & m == real[i] & m:
                return True
            lines.append("  %06x: real %08x  ours %08x"
                % (va + 4*i, real[i], ours[j]))
            return False
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal":
                for k in range(i2 - i1):
                    if pair(i1 + k, j1 + k):
                        neq += 1
                    else:
                        ndiff += 1
            else:
                # pair up the overlap, then one-sided leftovers
                k = 0
                while k < i2 - i1 and k < j2 - j1:
                    if pair(i1 + k, j1 + k):
                        neq += 1
                    else:
                        ndiff += 1
                    k += 1
                for i in range(i1 + k, i2):
                    lines.append("  %06x: real %08x  ours --------  MISSING"
                        % (va + 4*i, real[i]))
                for j in range(j1 + k, j2):
                    lines.append("  %06x: real --------  ours %08x  EXTRA"
                        % (va + 4*min(i2, n - 1), ours[j]))
        nplus = avail - neq - ndiff
        nminus = n - neq - ndiff
        if avail < n:
            lines.append("  (object .text ends %d insns short)" % (n - avail))
        print("%-24s mismatch  (%d/%d aligned, %d differ, %d missing, %d extra)"
            % (name, neq, n, ndiff, max(0, nminus), max(0, nplus)))
        for l in lines[:24]:
            print(l)
        if len(lines) > 24:
            print("  ... %d more" % (len(lines) - 24))
    print("%d/%d functions match" % (ok, total))

if __name__ == "__main__":
    main()
