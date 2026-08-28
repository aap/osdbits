# binary matching

The retail OSDSYS turns out to be compiled with **ee-gcc 2.9-ee-991111
at -O2** (the SDK 1.x-era compiler; sources and a native build live in
the `~/src/ps2rev/eegcc` archive, installed as the freesce prefix).
Determined 2026-08-28 by compiling reconstructions against the real
image:

- `initTextShit` (0x214f20): **matches instruction for instruction**
  at the first attempt, delay-slot scheduling included.
- `DoSCEText` (0x214c20): identical structure (no stack frame, the
  `nop` before `bc1f`, the tail-call `j`); two source-shape deltas
  remain (register choice a1/a3, the min() slt/movz vs slti/movn
  idiom).
- ee-gcc 2.96-ee-001003 is close but wrong (inserts a spurious ra
  save/restore); 3.2-ee is clearly wrong.

This directory is the function-by-function matching workspace.  The
runnable reconstruction stays in `osdbits/` (it is a standalone
harness, deliberately restructured); functions proven here feed
corrections back into it, and eventually the roles may swap.

## Workflow

    ee-gcc -O2 -c src/<file>.c -o build/<file>.o     # freesce prefix
    python3 check.py build/<file>.o path/to/expanded-osdsys.bin

`check.py` compares each function in the object against the real bytes
at its address (table in `functions.txt`), masking only
relocation-affected bit fields (%gprel/%hi/%lo/j targets), and prints a
per-instruction diff for mismatches.

`expanded-osdsys.bin` is the decompressed retail OSDSYS module loaded
at 0x200000 (Sony data - NOT in the repo): decompress your BIOS's
OSDSYS file with ps2expand, or dump 0x200000+ from a PCSX2 savestate's
`eeMemory.bin`.
