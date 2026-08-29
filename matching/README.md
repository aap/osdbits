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

For a mismatch, check.py prints an ALIGNED diff (difflib over
opcode-normalized words, immediates/branch/jump targets blanked
symmetrically) so an inserted or deleted instruction costs one line
instead of desyncing the rest of the function; the MATCH verdict is
still the strict positional masked compare.

## related tooling: n64-decomp-workbench

`~/othersrc/n64-decomp-workbench` (pure-stdlib Python, `pip install -e`
or PYTHONPATH) is an original late-stage MIPS mismatch diagnoser from
the N64 IDO scene.  Verdict after a deep look (2026-08-29):

- Its comparator stack is generic MIPS-over-objdump text and WORKS on
  R5900 ee-gcc objects (verified on our opening.o incl. sqrt.s/lq/sq/
  MMI) - but use a modern `/bin/objdump`, not ee-objdump (no
  `--disassemble=SYM`), and know that `score --rom` reads words
  big-endian (score.py:216) so ROM-vs-object scoring silently breaks
  on PS2 images; `shift_align.comparable_text` also mis-normalizes
  branch targets for any function not at .text offset 0.
- check.py's aligned diff already covers its core alignment idea;
  `align-dumps`/`view-dumps` remain useful as a second opinion with
  mechanism classification (register/schedule/constant lanes): feed it
  `objdump -d -r` of our .o and `objdump -D -b binary -m mips:5900 -EL`
  windows of the image.
- Most interesting untapped piece: `sweep`/`campaign` - its OWN
  compiler-agnostic textual C variant generators (commutative flips,
  carrier locals, live-range fusion) driven by an arbitrary
  `--compile-command`, i.e. a permuter that could take ee-gcc directly.
  Candidate for the tie-class residuals (register-allocation ties that
  no source-order lever moves).  Its decomp-permuter driver proper is
  IDO-shaped - ignore, along with all trace-*/instrument/oracle/pass
  commands (IDO compiler internals).
- The case-studies/*.md are compiler-independent matching methodology
  and worth reading on their own.
