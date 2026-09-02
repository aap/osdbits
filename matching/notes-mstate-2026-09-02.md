# m-state agent: scene/state/helper cluster + the 0x21CE58 special task

Work dir: `/u/aap/.claude/jobs/58e316f8/tmp/m-state/`.  Nothing outside
it written; `matching/`, `osdbits/` and git state untouched.  Harness:
copies of `matching/try.sh` + `check.py` + `unpad.py` + `vary.py` and the
prior session's `dis.sh`/`inventory.tsv` (from `../match/`).  Every score
below is reproduced output of `./try.sh` / `vary.py` in this directory.

Session was time-boxed by the coordinator mid-run; the special task and
the first two cluster functions were completed, the remaining 15 cluster
functions were disassembled (`dis/*.dis`) but not attempted.

## Scorecard

| fn | va | size | verdict | detail |
|---|---|---|---|---|
| InitMenuModule | 0x21CE58 | 0xC8 | **MATCH (50 insns), first try** | `src/init.c`, `init-functions.txt` |
| CarouselClock  | 0x225628 | 0x250 | close: 145/148 aligned, **0 differing words**, 3 missing / 3 extra | `src/carousel.c` |
| CarouselInit   | 0x225998 | 0x138 | far: 38/78 aligned (best of 15 swept variants) | `src/carousel.c` |

Attempted 3 of the 18 assigned (1 special + 17 >0x120 in range);
disassembly dumps for all 17 are in `dis/`.

### 0x21CE58 - the recorded blocker, now closed

One TU (`src/init.c`), one function, **byte-exact on the first
compile**.  It is exactly the reconstruction the port refused to be:
17 straight calls in ROM order (the three shares the port split into
menu:InitMenuScene / menuback:InitMenuBackdrop / menuconfig:
InitMenuConfig, interleaved) + `menuCamZOffset = -100.0f` as the tail:

    0x22EE88, 0x22BE18, 0x22B838, 0x22B128, 0x22AD38, 0x229698,
    0x22A9B8(0..9)  [ten explicit calls, NOT a loop - gcc 2.9 -O2
                     does not unroll, so the source wrote them out],
    0x21DBA0, 0x225998, 0x228460, 0x2287B0, 0x22ADD8(2),
    *(float*)(gp-28880) = -100.0f;

Nothing clever needed: void externs, one int-arg extern, one gp float.

### 0x225628 CarouselClock - close, schedule-tie class

145/148 aligned with **zero differing instruction words**; the two
residuals are pure placement: (a) the ROM schedules the `lwc1 f2,
tiltRate(gp)` two slots earlier than we do, (b) the ROM leaves the
0x2257CC `jal` delay slot as a nop where our reorg fills it with the
preceding `sh` (it fills the three analogous slots in the SAME function
both in ROM and ours - no source lever found; swept `float r = tiltRate`
placement variants, no effect).  This is `campaign/PENDING.md`'s
DrawSCEText class.

Semantics recovered (new, disassembly-proven):
* 0x22B720/0x22B590/0x22B640 return float hours / seconds / minutes.
* sel = (int)hours % 12; tilt eases toward sel*65536/12 (SNAPS when
  |(short)spin| < 201), spin eases toward seconds*65536/60,
  gp-28854 (short) eases toward hours*65536/12, gp-28856 (u16) toward
  seconds*65536/60, all at rate 0.1; carousel->split and gp-28852 are
  both `1.0f - minutes/60.0f` (two separate ClockMinutes() calls).

### 0x225998 CarouselInit - far (register/addressing plateau)

All content proven present and correct (see `src/carousel.c` comment);
15 variants swept over two probes (`src/probe_init.c`, `probe_init2.c`):
head as direct fields / u_long128* q / q[1];q[0] order / &rod[12] casts /
separate header+ring symbols; loop direct vs `Rod *r` vs continue-form.
Plateau 26-39/78 aligned.  The blocker: the ROM's allocator keeps
&carousel->curA (s1) and &rodColourOn (s0) live across the two mid-head
calls and derives every loop base from s1 (rod base = s1-0x250, header
sel via a FRESH lui/lw), performs TWO lq's from rodColourOn (no CSE) and
does NOT crossjump-merge the loop's two `sq colour` arms; every shape we
tried either CSEs the second lq, or keeps &carousel itself live, or
merges the arms.  Next lever to try (untried, out of budget): forcing
the alias barrier with the timer-duration store BETWEEN the two qword
stores, and/or `-fno-...`-free statement-permutation over the whole head
with genperm.py.

## Idioms / codegen findings (new, verified by probe)

1. **`(int)f` on the EE emits `cvt.w.s`, and freesce gas assembles
   `cvt.w.s` to the `trunc.w.s` ENCODING (0x46000064).**  mips.md's
   fix_truncsfsi2 returns cvt.w.s under TARGET_MIPS5900 (comment:
   "trunc.w.s isn't implemented on the r5900, but cvt.w.s truncates");
   the ROM's apparent trunc.w.s words therefore come from a plain C
   cast and byte-match ours.  Do NOT chase "trunc vs cvt" mismatches -
   there are none.
2. **Integer abs: write `s = s < 0 ? -s : s;` (or __builtin_abs).**
   Both produce the abssi2 pattern `bltzl s,1f; negu s` that the ROM
   uses; `if (s < 0) s = -s;` gets if-converted to slt+subu+movz
   (4 words, no branch).  Extends note §3.3 (which covered the float
   case) to the int case with a different winning spelling.
3. **`% 12` (and any constant modulo) is a REAL div/divu with the
   `beqzl reg,1f; break 7` zero-check** - this compiler never
   strength-reduces division by constants.  Signed `%` = div+mfhi;
   make the operand unsigned (`unsigned sel`) when the ROM shows divu.
   Inside a loop the divisor AND the zero for the check get hoisted
   into registers (`li t0,12` / `move t3,zero`, check becomes
   `beql t0,t3`).
4. **Ease-statement register shape: declare the delta as `short d`,
   not `int d` + `(short)d` casts.**  `d = (int)(target) - u16field;
   field = d * 0.1f + (short)field;` reproduces the ROM's
   subu/sll/sra register roles exactly (int d + casts swaps v0/v1
   roles in all three blocks and costs ~13 words).  This single
   change took 0x225628 from 132/148 to 145/148.
5. **Duplicate .lit4 values are legal evidence of a variable+literal
   pair**: gp-32172 and gp-32168 are BOTH 0.1f - the first is a real
   (s)data variable (the tilt rate, read once), the second is the
   .lit4 pool entry for the literal `0.1f` the other three easing
   statements share (loaded once into a callee-saved f22 across
   calls, which only a literal/const can be).
6. **Float constants with zero low halves confirmed again**: 65536.0f
   (0x4780), 60.0f (0x4270), 12.0f (0x4140), 1.0f, -100.0f (0xC2C8)
   all materialize as `lui at,HH00; mtc1` with no pool entry.

## Data-shape discoveries (feed back into docs/osdbits)

* **The carousel is ONE object at 0x34E6C0**, not header+ring:
  `{u32 sel; u16 spin; u16 tilt; int[2]; Rod rod[12]; u_long128 curA,
  curB; float split;}` - the 0x225628 `swc1 f0,624(s0)` (split via the
  header base) and 0x225998's `t4 = s1 - 0x250` derivation prove both
  halves share a symbol.
* **Rod record (48B): `progress` is at +0x00 and the colour qword at
  +0x10** (docs/menu-config.md's "+0x10 qword" for the cube table is
  the same convention); NOT +0x10/+0x20 as a first reading of
  0x225998 suggests - the modulo store's base register is the STRUCT
  base (disp 16 = 0x10 rod offset + 0x00 field).
* 0x27EB00 = carousel Timer; 0x27EB10/0x27EB20 = off/selected rod
  colour qwords; 0x34E960 = orbPhase[7] (SEVEN entries - the ROM loop
  stores 7, matching NUMORBS=7, not the ring's 12).
* TU grouping hint: 0x21CE58 needs nothing but externs, so it can live
  in its own TU exactly as `src/init.c` does; 0x225628/0x225998
  belong with 0x225AD0/0x225BF8/0x225F80/0x226028 in a carousel TU
  (they share the Carousel struct; declaration order = address order).

## Files

* `src/init.c` + `init-functions.txt` - the 0x21CE58 MATCH.
* `src/carousel.c` + `carousel-functions.txt` - 0x225628 (145/148
  aligned, 0 differ) + 0x225998 (38/78).
* `src/probe_init.c`, `src/probe_init2.c`, `src/probe_clock.c`,
  `src/probe_clock2.c` - the sweeps (vary.py-ready).
* `dis/*.dis` - disassembly of all 17 assigned cluster functions
  (0x225628, 0x225998, 0x226028, 0x2261B8, 0x226360, 0x226BB8,
  0x226D00, 0x227390, 0x22A290, 0x22A4C8, 0x22A6D8, 0x22B138,
  0x22B2A8, 0x22B968, 0x22BB30, 0x22BE30, 0x22C3C0, 0x22C4E0) for
  the next session.

## Assignment bookkeeping

The >0x120 rule leaves these unattempted in my ranges: 0x226028(0x18C),
0x2261B8(0x1A8), 0x226360(0x380), 0x226BB8(0x13C), 0x226D00(0x2A4),
0x227390(0x1D0), 0x22A290(0x128), 0x22A4C8(0x210), 0x22A6D8(0x2E0),
0x22B138(0x138), 0x22B2A8(0x14C), 0x22B968(0x128), 0x22BB30(0x2E8),
0x22BE30(0x128), 0x22C4E0(0x3A8), plus borderline 0x22C3C0 (exactly
0x120, named as "the zoom blur" in my brief).  The prompt's named
addresses 0x225AD0/B68/BF8/DD8/F80, 0x226700/7E8/FA8, 0x22AC10-cluster,
0x22AD38, 0x22A0C0/198/3B8, 0x22AB90, 0x22BF58, 0x22BFD0 are all
<=0x120 and per the size rule belong to the small-function agent
(0x22A720 and 0x2272C0 are not function starts - they are inside
0x22A6D8 and 0x227268 respectively; the "config entry" is 0x227268,
0x88 bytes, also small-agent).
