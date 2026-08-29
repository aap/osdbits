#!/usr/bin/env python3
"""score1 - check.py's comparison as a callable, returning numbers.

    python3 score1.py OBJ IMAGE FUNCTAB          # print "name strict aligned differ ..."

`strict` is check.py's own MATCH criterion (count of words that differ
under the strict positional masked compare); 0 == MATCH.
"""
import os, re, struct, subprocess, sys, tempfile, difflib

TOOLS = os.environ.get("EETOOLS", "/usr/local/sce/ee/gcc/bin")
BASE = 0x200000

def tool(name, *args):
    return subprocess.run([os.path.join(TOOLS, name), *args],
        capture_output=True, text=True).stdout

def load_table(path):
    funcs = {}
    for line in open(path):
        line = line.split("#")[0].split()
        if len(line) == 3:
            funcs[line[0]] = (int(line[1], 16), int(line[2], 16))
    return funcs

IMM16 = set(range(4, 16)) | set(range(20, 28)) | set(range(30, 64)) | {1}
def norm(word):
    op = word >> 26
    if op in (2, 3):
        return word & 0xFC000000
    if op in IMM16 or (op == 17 and (word >> 21) & 31 == 8):
        return word & 0xFFFF0000
    return word

def score(obj, img, funcs):
    syms = {}
    for line in tool("ee-nm", obj).splitlines():
        m = re.match(r"([0-9a-f]+) [tT] (\w+)", line)
        if m:
            syms[m.group(2)] = int(m.group(1), 16)
    relocs = {}
    sec = None
    for line in tool("ee-objdump", "-r", obj).splitlines():
        m = re.match(r"RELOCATION RECORDS FOR \[(\S+)\]", line)
        if m:
            sec = m.group(1); continue
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
    out = {}
    for name, (va, size) in funcs.items():
        if name not in syms:
            out[name] = None; continue
        soff = syms[name]
        n = size // 4
        avail = max(0, min(n, (len(text) - soff) // 4))
        ours = list(struct.unpack_from("<%dI" % avail, text, soff))
        real = list(struct.unpack_from("<%dI" % n, img, va - BASE))
        masks = [mask(soff + 4*i) for i in range(avail)]
        strict = sum(1 for i in range(n)
            if i >= avail or ours[i] & masks[i] != real[i] & masks[i])
        sm = difflib.SequenceMatcher(None, [norm(w) for w in real],
            [norm(w) & masks[i] for i, w in enumerate(ours)], autojunk=False)
        neq = ndiff = 0
        rows = []
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            k = 0
            lim = (i2 - i1) if tag == "equal" else min(i2 - i1, j2 - j1)
            while k < lim:
                i, j = i1 + k, j1 + k
                if ours[j] & masks[j] == real[i] & masks[j]:
                    neq += 1
                else:
                    ndiff += 1
                    rows.append((va + 4*i, real[i], ours[j]))
                k += 1
            if tag != "equal":
                for i in range(i1 + k, i2):
                    rows.append((va + 4*i, real[i], None))
                for j in range(j1 + k, j2):
                    rows.append((None, None, ours[j]))
        missing = sum(1 for r in rows if r[2] is None)
        extra = sum(1 for r in rows if r[0] is None)
        out[name] = dict(n=n, strict=strict, aligned=neq, differ=ndiff,
                         missing=missing, extra=extra, insns=avail, rows=rows)
    return out

if __name__ == "__main__":
    obj, image, ftab = sys.argv[1], sys.argv[2], sys.argv[3]
    img = open(image, "rb").read()
    for name, r in score(obj, img, load_table(ftab)).items():
        if r is None:
            print("%-24s ABSENT" % name); continue
        print("%-24s strict=%d/%d aligned=%d differ=%d missing=%d extra=%d"
              % (name, r["strict"], r["n"], r["aligned"], r["differ"],
                 r["missing"], r["extra"]))
