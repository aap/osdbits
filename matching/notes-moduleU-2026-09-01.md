# OSDSYS menu module: how far is a binary match, and the harness for it

Work dir: `/u/aap/.claude/jobs/58e316f8/tmp/match/`.  Nothing outside it
was written — `osdbits/`, `matching/` and `osdsys_re/` are untouched, no
commits anywhere.  `ref/` is a throwaway **copy** of `matching/` used to
re-run its build without touching the real tree.

---

## 1. The distance answer

**Module U — the menu module, 0x21c910..0x230018 — is 323 functions and
19,758 instructions.**  Against that:

| | functions | instructions |
|---|---|---|
| Module U total | 323 | 19,758 |
| byte-exact **before** this session | 54 (16.7%) | 545 (2.8%) |
| byte-exact **after** this session | **73 (22.6%)** | **608 (3.1%)** |
| attempted at all | 84 (26.0%) | 901 (4.6%) |
| never touched | 239 | 18,857 |

Plus 9 functions of the shared `matrixDrive.c` service just above the
module (0x230018+), 5 of them byte-exact, new this session.

Reproduce with `python3 scoreboard.py`.

### Why "126/146" and "16.7%" are both true

`matching/`'s scoreboard is **126 of 146**, and I reproduced that
number exactly (`anim 11/11, blit 5/8, config 25/26, flare 11/15,
fog 0/4, menu 33/36, menudraw 38/43, text 3/3`).  It is a real 86% —
but of *attempted* functions, and the attempted set is small and
deliberately skewed to leaves:

* The 146 is the union of the eight `matching/*-functions.txt` tables,
  which span the **opening** (anim/blit/flare/fog/text) as well as the
  menu.  Only two tables are Module U: `menu-functions.txt` (36) and
  `menudraw-functions.txt` (43).
* Those two **overlap on 17 addresses** under different names —
  `0x22ac20` is `anmScaled` in one and `timerScale` in the other,
  `0x21d748` is `setLegendFlagA` / `setButtonOverride`, and so on.  So
  "33/36 + 38/43" double-counts: by unique ROM address it is
  **54 matched of 62 attempted**.
* 62 attempted addresses out of 323 is 19% of the functions but **3.9%
  of the instructions**, because almost everything matched so far is an
  8–100 byte accessor.  Median matched function: ~10 instructions.

**So the honest headline is: the menu module is ~3% matched by
instruction count, and the remaining 97% is where all the difficulty
is.**  What is left, by size:

```
   <=0x20    23 functions     68 insns
   <=0x40    21 functions    243 insns
   <=0x80    56 functions   1387 insns
   <=0x100   77 functions   3380 insns
   <=0x200   48 functions   4262 insns
   <=0x400   24 functions   4492 insns
   >0x400    12 functions   5150 insns
```

36 functions larger than 0x200 bytes hold 9,642 instructions — **49% of
the module in 11% of the functions.**  Those are the frame bodies, the
packet builders and the config-screen draw code, and none has been
attempted.

### The osdbits port is not a matching candidate

Task 4 asked for the baseline with the existing `osdbits/` code as the
candidate.  I compiled all four menu TUs with the retail compiler
(they build clean under 2.9-ee, unlike `opening.c`, which uses C99
mid-block declarations) and scored every `real: 0xADDR` annotation that
decorates a function definition — 50 port↔ROM pairings inside Module U:

```
   MATCH     5  (10%)      all five are the trivial timer accessors
   close     1  ( 2%)      SceneWalk 0x226700, 24/26
   far      44  (88%)
   ABSENT    0
```

Full table in `baseline.txt`.  This is expected and not a criticism:
`matching/README.md` says outright that the port "is a standalone
harness, deliberately restructured".  The measurement is still worth
having, because it says **the port's `real:` map is a naming/behaviour
map, not a source-shape map** — the 261 untouched functions will have to
be reconstructed from disassembly the way `matching/src/*.c` already
does, not lifted from `osdbits/`.

### Port functions that are structurally too far to match as-is

These need restructuring before they could ever be candidates.  I did
**not** touch `osdbits/`; this is the list and what each would need.

*One ROM function the port defines N times* (the port gave each TU its
own private copy of a shared ROM helper — each copy is a legitimate
candidate for the one ROM function, they just cannot all be linked):

| ROM | port copies |
|---|---|
| 0x22ac20 | `menu:TimerInterp`, `menuback:BackTimerInterp`, `menutext:mtScale` |
| 0x22ac70 | `menu:TimerOpen`, `menuconfig:cfgOpen`, `menutext:mtOpen` |
| 0x22ac90 | `menuconfig:cfgClose`, `menutext:mtClose` |
| 0x22acc0 | `menu:TimerStep`, `menuback:BackTimerStep`, `menuconfig:cfgStep`, `menutext:mtStep` |
| 0x21ce58 | `menu:InitMenuScene`, `menuback:InitMenuBackdrop`, `menuconfig:InitMenuConfig` |

The timer family is already matched in `matching/src/menudraw.c`, so
those five rows cost nothing.  **0x21ce58 is the real problem**: the ROM
has one 50-instruction init and the port split it three ways by
subsystem.  Matching it means writing one function containing all three
shares in ROM order — a new TU, not an edit to the port.

*Port functions that merge or truncate a ROM function* (comment says so):

* `menuback:DrawKabeRibbon` — the port fuses **three** ROM functions
  (0x229130 packet + 0x2288c0 body + 0x228e78 cap).  Matching needs
  them split back into three.
* `menu:ClockTick` (0x22bb30) — port implements the ms integration
  "minus the RTC resync"; the ROM function is 186 instructions, the port
  is ~66.  Needs the resync arm written.
* `menuconfig:MeshFresnel` (0x22c888) — the port implements "0x22C888
  and 0x22CCE8's shared head"; the ROM has two separate functions that
  happen to start alike.
* `menuconfig:CarouselClock` (0x225628) — "share of the per-frame
  update"; ROM 148 instructions vs the port's partial.

---

## 2. Survey of the existing matching work

### `osdsys_re/` is a different binary — do not aim results at it

`osdsys_re/` is a **clone of someone else's project** (psx-place thread,
`README.md`): a splat decomp of **HDDOSD/HOSDSYS 1.10U** unpacked from
SUDC4, not the BIOS OSDSYS this project reverses.  `splat_config.yml`
gives it away: `gp_value: 0x377970` where ours is `0x2af070`, target
`OSDSYS_A_XLF_decrypted_unpacked.elf` fetched from archive.org, vram
0x200000 but a completely different image (sha1
`e932f3508313e2807467a0f354acc56869ea77f6`).  It holds `symbol_addrs.txt`
(6,371 symbols) and no C at all — its last four commits are "update:
Update symbol list".  Useful only as a **name donor** for equivalent
functions; results cannot migrate there.  Left strictly read-only.

### `matching/` is the real workspace

`/u/aap/src/ps2rev/osdsys/matching/`, tracked in the main repo, 13
commits.  Organisation:

* `src/<tu>.c` — reconstructed C, one file per functional cluster
  (`anim`, `blit`, `config`, `flare`, `fog`, `menu`, `menudraw`, `text`).
* `<tu>-functions.txt` — `name va size` (hex), bounds from
  `osdsys_dump.idb`.  Optional `#lit4 <va>` line names where the TU's
  `.lit4` pool starts in the image.
* `check.py <obj> <image> [table]` — the verdict.  Strict positional
  compare of the object's `.text` bytes against the image, masking only
  relocation-affected fields (`R_MIPS_HI16/LO16/GPREL16/LITERAL` →
  `0xffff0000`, `R_MIPS_26` → `0xfc000000`, `R_MIPS_32` → 0), and only
  from the `.text` reloc section (a `.rodata` jump table's `R_MIPS_32`
  entries alias low `.text` offsets and would silently mask real
  instructions).  On mismatch it prints a **resync diff**: difflib over
  opcode-normalised words with immediates/branch/jump targets blanked
  symmetrically, so one inserted instruction costs one line instead of
  desyncing the rest.  The MATCH verdict stays the strict compare.
* The `.lit4` pool check exists because reloc masking hides *which*
  literal an instruction references — two sources with different
  constants or different literal order can both score byte-exact.  It
  bit twice (a 0.9f read as 0.1f; a statement swap that reordered the
  pool).
* `campaign/` — the automated variant search.  `score1.py` is check.py's
  comparison as an importable library; `stmtperm.py` relocates/permutes
  whole top-level statements; `runall.py`/`search.py`/`gen_all.sh` drive
  bulk variants.  `campaign/README.md` records the key verdict: the
  n64-decomp-workbench's own generators (all *within-statement* edits)
  closed **zero** register-allocation ties over 17.6k variants — what
  wins ties in gcc 2.9 is **statement placement and declaration facts**.
* `campaign/PENDING.md` — confirmed plateaus not to re-grind:
  `DrawSCEText` 109/111 (in-block sched tie), `DrawExtraBuf2` 141/160
  and `DrawIllegalText` 89/108 (register-name permutations, 2,501
  variants, no effect), `InitFog` 175/176 (sched slot).

### Toolchain (verified, not assumed)

* Compiler: **`/usr/local/freesce/ee/gcc/bin/ee-gcc`, gcc 2.9-ee-991111-01,
  `-O2`.**  Note `ee-gcc` on `$PATH` is the SCE **3.2**-ee-040921 — the
  wrong one; always give the freesce path explicitly.
* Binutils: **`/usr/local/sce/ee/gcc/bin`** (`$EETOOLS`), because
  freesce's `objcopy` has no `-j`.  But **assemble with freesce's**
  assembler (see §3) — SCE 3.0.3's gas loses 13 of the 126.
* Flags: `osdbits/Makefile` uses `-O2 -Wall -fno-common
  -fno-strict-aliasing`.  Only `-O2` matters for codegen here;
  `-Wall` earns its place because implicit-declaration warnings have
  twice been what found a mismatch (`OpeningThread` 32/32 came from a
  missing `InitOpening` prototype).
* Image: `/u/aap/src/osdsys/expanded.bin`, base 0x200000.

---

## 3. Compiler findings (the idiom log)

New this session, in rough order of value.

### 3.1 `lq`/`sq` needs a 128-bit *mode*, not alignment

`__attribute__((aligned(16)))` on a 16-byte struct, a typedef, or a
local **never** produces `lq`/`sq` in 2.9-ee — you get `ld`/`sd` pairs
(probed six ways).  The only thing that works is a TImode type, i.e.
`eetypes.h`'s

```c
typedef unsigned int u_long128 __attribute__((mode(TI)));
```

Any assignment of that type, or of a struct containing one, emits
`lq`/`sq`.  This single change took `MatrixDrive_TranslateV` from 12/21
to 17/21 and made `PushMatrix`'s matrix copy structurally right.
**Whenever the ROM shows `lq`/`sq`, the source used a 128-bit type.**

### 3.2 The R5900 short-loop nop pad — and that retail HAS it

`mips_r5900_lengthen_loops` (`config/mips/mips.c:8717`) pads any loop of
≤6 instructions out to 7 with nops before the closing branch.  It was
**introduced in 2.9-ee-991111-01** and is absent from every earlier tree
(`2.9-sky-990318`, `2.9-sky-990430`, `2.9-ee-990721` — checked in
`~/src/ps2rev/eegcc`).  It runs unconditionally under `TARGET_MIPS5900`;
there is no flag.  gcc 2.9's `condjump_p` returns 1 for a plain
`(set (pc) (label_ref))`, so it fires on **unconditional** self-jumps
too — a `for(;;);` after a panic `printf` gets 5 nops.

Retail **does** have the pad: `flare_21A6D8` (0x21a6d8) matches
byte-exact **with** three padded `bc1t` angle-wrap loops, and stripping
them loses that match.  Independently, `2.9-ee-990721` — the newest tree
without the pass — scores **107/146** against 991111's 126, so retail is
not an earlier revision.  I also tested every toolchain permutation:

| cc1 | as | corpus |
|---|---|---|
| 991111 | 991111 (freesce) | **126/146** |
| 990721 | 990721 | 107/146 |
| 991111 | SCE 3.0.3 | 113/146 |
| 991111 | 990721 | 113 vs 118 on the five TUs I ran (menu/menudraw/config unchanged, anim −1, flare −4) |

So **991111 cc1 + 991111 gas is retail's toolchain**, confirmed four ways.

**The open anomaly:** retail's *panic* self-loops are NOT padded —
`MatrixDrive_PushMatrix`'s at 0x2300e8 is a bare `b .` + `nop`, with a
single alignment nop before it from `LOOP_ALIGN` (`DEFAULT_LOOP_ALIGN 3`
in `elf5900.h`, i.e. 8-byte, which retail's 0x230168 loop head obeys).
Both cc1 *and* the assembler pad, and the assembler pads even
hand-written `.set noreorder` inline asm, so no C or asm construct I
could find reproduces it.  Most likely retail's panic tail came from a
separately-assembled object or a gas built without the fix.  **Practical
consequence: any ROM function whose body ends in an unpadded short
unconditional loop cannot reach a strict MATCH with this toolchain.**
Known instances: `MatrixDrive_PushMatrix`, `MatrixDrive_PopMatrix`.

`unpad.py` strips the pad at the `.s` level, restricted to
unconditionally-closed loops so flare's conditional pads survive — it
reproduces both facts and keeps the corpus at 126.  It does not help
these two because the assembler re-inserts it; keep it in the pipeline
anyway, it is the right stand-in and it is verified neutral.

### 3.3 One float temp instead of two `return`s shares the tail

`MatrixDrive_Sin` went from 8/19 to **19/19 MATCH** on this pair of
levers, found by sweep:

* `i = a < 0 ? -a : a;` produces a real `bgez`+`negu`; the equivalent
  `if (i < 0) i = -i;` gets **if-converted to `movz`**.  When the ROM
  shows a branch around a negation, write the `?:`.
* Two `return`s (`if (neg) return -T[i]; return T[i];`) make gcc
  **duplicate the whole tail** (two copies of the address arithmetic and
  the load).  Assigning to one `float r` and negating in place shares it.

### 3.4 Loop-invariant hoisting: `const` MEM yes, `CONST_DOUBLE` never

Retail's `InitSinTable` hoists two doubles into callee-saved `s3`/`s2`
before a loop that contains a call.  This compiler **never** hoists a
double *literal* out of such a loop — not at `-O1`, `-O2`, `-O3`,
`-funroll-loops` or `-fno-builtin`; it rematerialises `lui at; ld a1,N(at)`
every iteration.  It *does* hoist a `const` MEM (RTX_UNCHANGING), so:

```c
extern const double mdHalfPi[];   /* incomplete array: keeps it out of $gp */
extern const double mdTabSize[];
```

took the function from 24/42 to **37/42 with the right instruction
count** — everything matches except the `lui` temp register (`$at` for
retail's literal-pool reference vs `$a0` for our symbol reference).
Combining the two known tricks matters: `extern const double x;` alone
hoists but lands in small data (one `ld` off `$gp`); the **incomplete
extern array** form is what forces the `lui`/`%lo` pair.

### 3.5 Float constants with a zero low half avoid the literal pool

`lui at,0x4040 / mtc1 at,$f0` is 2.9-ee's materialisation of `3.0f`;
`lui at,0x3f80 / mtc1` is `1.0f`.  No `.lit4` entry is created.  Seeing
this pattern in the ROM tells you the constant exactly, and writing the
plain literal in C reproduces it.

### 3.6 Statement placement is still the only tie-breaker that works

Confirmed again.  `MatrixDrive_RotX` went 43/50 → 47/50 → **49/50** by
two rounds of single-statement relocation (`genperm.py`, 65 variants per
round, ~2 s each).  The last word is an in-block schedule tie — the same
class `campaign/PENDING.md` already documents for `DrawSCEText`; no
statement move reaches it.  Neither does anything for `scale_228898`
(0x228898): six different source spellings of `p[1]=y*3*s; p[0]=x*3*s;`
produce **byte-identical** output, all 5/9.

### 3.7 Boundary caveat: the `jr ra` scan over-splits duplicated tails

IDA misses 37 functions in Module U (see §4), and the recovery scan
finds them — but it also **splits a function that returns from two
arms**.  0x220640 is one 20-instruction function whose odd-index arm
starts at 0x22066c; the scan reported 0x22066c as a separate function
because a `jr ra` precedes it.  Always check whether a "function" whose
first instruction reads a caller-saved register that is *not* an
argument register (0x22066c starts `addu v1,a0,v1`) is really an arm of
its predecessor.

---

## 4. The inventory

`inventory.py` builds the Module U function set and writes
`inventory.tsv`; `inventory-matrix.txt` is the readable matrix asked for
(va, size, IDB name, osdbits port function, port-vs-ROM verdict,
matching/ state).

* Boundaries come from `osdsys_dump.idb` (`idautils.Functions()`, which
  `flare-functions.txt` already notes is more reliable than `get_func`
  here), **unioned** with starts recovered by scanning for `jr ra` +
  delay slot + nop padding *and* for the first instruction after every
  IDA `endEA` (that second rule catches functions whose predecessor
  tail-calls with `j` instead of returning — three more).
* IDA knows **286**; the scan finds **37** more; total **323**.
* Sizes use IDA's `endEA - startEA`, which **excludes** the trailing
  alignment nop — the convention `matching/*-functions.txt` already
  uses.  With that convention there are zero disagreements between IDA's
  size and the next start (133 of 286 differ by exactly the one padding
  word, which is why it matters).
* Cross-referenced against 296 `real: 0x2xxxxx` annotations harvested
  from `osdbits/` (`harvest.py` → `harvest.tsv`): 61 decorate a function
  definition, 11 a data object, 224 are references in prose.  Of the 323
  Module U functions, **40 have a port function, 67 are mentioned only
  in passing, 216 are not mentioned at all.**

---

## 5. The harness

Everything follows `matching/`'s conventions so results can migrate.

| file | what |
|---|---|
| `try.sh SRC.c TABLE` | compile with the retail compiler → `unpad.py` → assemble → `ref/check.py`.  The main loop. |
| `vary.py PROBE.c VA SIZE` | score **every** function in a probe file against one ROM address, best first.  Turns "which source shape does gcc lower this way" into a sweep. |
| `genperm.py [order]` | emit all single-statement relocations of a statement list, for `vary.py`.  Sized-down `stmtperm.py`. |
| `unpad.py FILE.s` | strip the 991111 short-loop pad (§3.2).  Verified neutral on the 126. |
| `dis.sh ADDR [SIZE]` | ROM disassembly of one function, size from the inventory. |
| `inventory.py` | build `inventory.tsv` (the 323). |
| `harvest.py` | pull `real:` annotations out of `osdbits/`. |
| `baseline.py` | score the osdbits port against the ROM → `baseline.txt`. |
| `coverage.py` / `scoreboard.py` | the address-unique progress number. |
| `idbdump.py` | dump the IDB function list (needs the python-idb venv). |

Scoring is `matching/campaign/score1.py` imported directly — the
project's own criterion, not a reimplementation.

New sources: `src/matrixdrive.c`, `src/leaf.c`, tables
`matrixdrive-functions.txt`, `leaf-functions.txt`.  Probes:
`src/probe_sin.c`, `probe_tv.c`, `probe_rotx.c`, `probe_rotx2/3.c`,
`probe_sintab.c`, `probe_push.c`, `probe_leaf2.c`.

---

## 6. What got matched

### `src/leaf.c` — 19/21, the smallest untouched Module U functions

19 matched, 18 of them **first try**.  Empty bodies, `j`-tail-call
thunks, absolute-global getters/setters, and two call-then-tail-call
sequences.

```
nullsub_21CFD0 thunk_21F978 thunk_221900 thunk_223650 thunk_225310
thunk_2287A8 thunk_226CF8 thunk_22B588 get_22B790 get_22B7A0
get_22B7B0 set_22B950 clr_22B960 thunk_22ED10 thunk_22EE88
set_221910 seq_225978 seq_227198 open_223658           = 19 MATCH
scale_228898  5/9   in-block schedule tie, six spellings identical
get_220640    7/20  reconstruction not yet right (see §3.7)
```

### `src/matrixdrive.c` — 5/10, the shared matrix service

The real source file name is known: the panic strings at 0x2a4bd8 /
0x2a4c20 are `"%s::MatrixDrive_PushMatrix: Matrix stack over flow!!\n"`
and `...PopMatrix...`, with `__FILE__ = "matrixDrive.c"` at 0x2a4c10.
So these are genuinely `MatrixDrive_*`.  The stack is 16 × 64 bytes at
0x368200, the sine table is 16,385 floats at 0x3581f0 (a quarter wave,
`sin(i * (pi/2) / 16385.0)` — note the divisor is the *count*, 16385,
not 16384; that is retail's own off-by-one, reproduced deliberately),
`mtxSp` is `gp-30300` = 0x2a7a14 and the table-built flag is
`gp-30304` = 0x2a7a10.

```
MatrixDrive_Sin        0x230018  MATCH (19)
MatrixDrive_Cos        0x230068  MATCH (10)
MatrixDrive_Init       0x230090  MATCH  (9)
MatrixDrive_GetMatrix  0x230180  MATCH  (6)
MatrixDrive_Translate  0x230440  MATCH (13)
MatrixDrive_RotX       0x230198  49/50  in-block sched tie (§3.6)
MatrixDrive_TranslateV 0x2303e8  17/21  addressing tie: the ROM adds 48
                                        to the base register, we fold it
                                        into the sq displacement (one
                                        instruction shorter); no address
                                        spelling tried moves it
MatrixDrive_InitSinTable 0x22ff70 37/42 right instruction count, 5 words
                                        differ = `$at` vs `$a0` as the
                                        lui temp (§3.4)
MatrixDrive_PushMatrix 0x2300b8  15/32  blocked by the panic pad (§3.2);
                                        also reloads mtxSp and keeps two
                                        base pointers where we keep one
MatrixDrive_PopMatrix  0x230138  12/17  blocked by the panic pad only
```

`0x230198` is `RotX`, not RotZ: row 0 is `(1,0,0,0)` and row 3 is
`(0,0,0,1)`.  0x230260 and 0x230328 (both untouched) will be RotY/RotZ.

---

## 7. Recommended path

1. **Fix the counting first.**  Merge `menu-functions.txt` and
   `menudraw-functions.txt` into one address-keyed table, or at least
   score by address — the 17-address overlap means the current
   scoreboard overstates Module U progress by ~27%.  `scoreboard.py`
   does it right; adopting it in `matching/` is a ten-minute change.

2. **Report instructions, not just functions.**  22.6% of functions is
   3.1% of the module.  Every planning decision changes when you look at
   the instruction column.

3. **Sweep the remaining 44 tiny functions next** (`<=0x40`, 68+243
   instructions).  `leaf.c` matched 18 of 21 on the first compile at
   roughly two minutes per function; the same is very likely for the
   rest.  It is cheap and it grows the *function* count fast — but be
   honest that it barely moves the instruction count.

4. **Then attack the 36 functions >0x200 bytes (9,642 instructions,
   49% of the module).**  This is the actual project.  These have no
   port counterpart worth reusing and will need the same
   disassembly→reconstruct→sweep loop that `flare.c`/`fog.c` used.
   Budget by instruction count, not by function count.

5. **Reconstruct 0x21ce58 as one function** in a new TU.  It is the one
   place where the port's split is actively in the way, and it is the
   module's init — a natural anchor for the surrounding TU's globals.

6. **Use `vary.py` before hand-iterating.**  Every win this session came
   from a sweep, not from reading code: `Sin` (9 shapes → MATCH),
   `InitSinTable` (13 shapes → +13 aligned), `RotX` (130 orders →
   49/50).  Each sweep is one compile and about two seconds.

7. **Record the panic-loop pad as a known non-matchable tail** so nobody
   re-derives §3.2.  If a full match of `PushMatrix`/`PopMatrix` really
   matters, the remaining lead is a gas built from the 991111 source
   with `mips_r5900_lengthen_loops`'s assembler-side twin disabled —
   aap has the source tree at `~/src/ps2rev/eegcc/2.9-ee-991111-01/src`.

8. **Keep the `.lit4`/`.lit8` pool check on.**  It has already caught two
   false matches in this project, and §3.4 shows literal-pool behaviour
   is exactly where this compiler diverges most.
