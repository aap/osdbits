#!/usr/bin/env python3
# check.py - compare functions in a compiled .o against the real OSDSYS
# image, masking relocation-affected bit fields.
#
#     python3 check.py build/text.o path/to/expanded-osdsys.bin
#
# Function addresses come from functions.txt ("name va size" per line,
# hex).  The toolchain (ee-nm/ee-objcopy/ee-objdump) is taken from
# $EETOOLS or /usr/local/sce/ee/gcc/bin.

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
    with open(os.path.join(os.path.dirname(__file__) or ".", "functions.txt")) as f:
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

    # relocation offsets -> type
    relocs = {}
    for line in tool("ee-objdump", "-r", obj).splitlines():
        m = re.match(r"([0-9a-f]+) (R_MIPS_\w+)", line)
        if m:
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

    total = ok = 0
    for name, (va, size) in funcs.items():
        if name not in syms:
            continue
        soff = syms[name]
        good = bad = 0
        lines = []
        for i in range(0, size, 4):
            m = mask(soff + i)
            mine, = struct.unpack_from("<I", text, soff + i)
            real, = struct.unpack_from("<I", img, va - BASE + i)
            if mine & m == real & m:
                good += 1
            else:
                bad += 1
                lines.append("  %06x: real %08x  ours %08x" % (va + i, real, mine))
        total += 1
        ok += bad == 0
        print("%-24s %s  (%d/%d insns)" % (name,
            "MATCH" if bad == 0 else "mismatch", good, good + bad))
        for l in lines[:16]:
            print(l)
    print("%d/%d functions match" % (ok, total))

if __name__ == "__main__":
    main()
