# m-text cluster scorecard (2D layer: 0x227500-0x229400 + 0x21C910-0x21E000, >0x120 bytes)

Work dir: `/u/aap/.claude/jobs/58e316f8/tmp/m-text/`.  Harness = copy of
`matching/` (`ref/`), same try.sh/check.py criterion, freesce
ee-gcc 2.9-ee-991111 -O2, unpad.py in the pipeline.  Nothing outside this
dir written.  All scores below reproduced with `./try.sh` right before
writing this file.

## Scorecard

| va | size | name | verdict | score | reproduce |
|---|---|---|---|---|---|
| 0x228278 | 0x124 | MainMenuInput | **MATCH** | 73/73 byte-exact | `./try.sh src/mmi.c mmi-functions.txt` |
| 0x228460 | 0x160 | initScreenPage | **close** | 82/88 aligned, 1 differ, 5 missing/extra | `./try.sh src/init.c init-functions.txt` |
| 0x228110 | 0x164 | DrawMainMenu | **close** | 80/89 aligned, **0 differ**, 9 missing/extra | `./try.sh src/dmm.c dmm-functions.txt` |
| 0x21ca38 0x21d0a0 0x21d1f8 0x21d3a0 0x21d590 0x21d7f8 0x21ddc0 0x21dff8 0x227560 0x2279b8 0x2288c0 0x228e78 | | | untouched | | dis dumps in `dis/` |

Both "close" functions are semantically and instruction-for-instruction
right; the residue is the known scheduler-tie class:

* **0x228110 (80/89, differ=0)**: every emitted word is identical to
  retail; 9 instructions sit in different slots.  Residue A: the
  screenH `lui/lw/srl/addu/sra` chain — retail leaves it after all 7
  prologue `sd`s with the `-14` in the jal delay slot, ours interleaves
  it into the `sd`s (the `sd` order itself, s4/s3/s1/ra/s5/s2/s0,
  matches).  Residue B: one `lui 0x1f`+`lw` pair (drawBufSel) placed
  after instead of before the camCtx lui.  volatile on the low words
  does NOT move it (tested).  Statement-order levers exhausted; this is
  the drawEdgeStrip/DrawSCEText sched-2 tie class.
* **0x228460 (82/88)**: the tail store block (6 stores + mflo + addus).
  Retail: S1 store between `lui hi(carouselTimer)` and `lui hi(cfgPage)`
  and `sw zero,24(a0)` after `addiu a1`.  All 720 statement
  permutations swept (plus a `t = dur40+dur40b` temp × 2160): plateau
  at 82/88 for every order with S4 placed before S1/S2; register
  assignment (v0=dur40b, v1=dur40, a2/a3=one-shot his, a0=cfgPage,
  a1=mainHdr, s1=a1+24) matches exactly in the best orders.

## New idioms (each reproduced by sweep, see src/probe_*.c)

1. **A ternary feeding arithmetic constant-folds through it.**
   `(IsPAL() ? 50 : 60) * 40 / 60` emits a select of the folded
   constants 33/40 — no mult, no div.  Retail's real
   `li a0,40; mult; li 60; beqzl/break; div` requires the ternary
   assigned to an `int rate;` FIRST, multiply/divide in a separate
   statement.  (probe_init2/3)
2. **Two consecutive same-constant selects need two distinct
   variables.**  Reusing one `rate` variable for both
   `rate = IsPAL() ? 50 : 60` computations coalesces them into a single
   callee-saved home (s2, +1 register, wrong shape).  Distinct
   variables (`rate`, `rate2`) reproduce retail exactly: 50→s0, 60→s1
   hoisted before the call block, select 1 = `movn v1,s0,v0` into a
   scratch, select 2 = `movz s0,s1,v0` coalesced onto the dead 50.
   (probe_init4: 39→72 aligned on this lever alone)
3. **Scalar `extern struct` folds member offsets into the reloc; the
   incomplete-array form does not.**  `extern Hdr mainHdr;` +
   `&mainHdr.anim` emits retail's one-pair
   `lui %hi(mainHdr+24); addiu %lo(mainHdr+24)`;
   `extern Hdr mainHdr[];` + `&mainHdr[0].anim` emits base + separate
   `addiu +24`.  Same for `camCtx.field` (`lw %lo(camCtx+8)`).
   Codegen law 10 amendment: the incomplete-array trick is for keeping
   things OUT of small data; when the ROM folds a field offset into
   %lo, the extern must be a scalar struct (fine when sizeof > the -G
   threshold).  (probe_dmm3)
4. **Write the loop row as `y0 + i*16`, not `y += 16`.**  Loop strength
   reduction then materializes retail's giv: `move s1,s4` in the
   preheader, `addiu s1,s1,16` in the latch delay slot, y0 dead after
   the preheader (its s4 reused for a colour %hi).  The `y += 16` form
   allocates y in one register end-to-end and never emits the move.
   (probe_dmm4: 68→79 aligned)
5. **A block-scoped temp on ONE call argument flips arg evaluation
   order.**  `{ int fld = camCtx.field; setDrawBuf(DBUFF, drawBufSel,
   0, fld); }` moved the camCtx lui ahead of the drawBufSel load and
   took the last content diff to zero.  (probe_dmm6, differ 1→0)
6. In this TU the low block is per-variable symbols (fresh `lui 0x1f`
   for screenH and drawBufSel, never a shared base even where menudraw's
   lesson B shares one) — calls intervene between every use, so either
   declaration works; separate `extern int screenHV[]` etc. is what was
   used.
7. **Tail-call suppression** (the 0x228278 MATCH, probe_mmi5): a
   `f(); return;` inside an if-arm becomes a `j f` tail call and breaks
   the ROM's shared-epilogue layout.  Retail's arms FALL OUT of the
   else-if chain (no `return` statements inside the arms) — then every
   call stays a `jal` followed by `b <epilogue>`.  Big lever: 46→59
   aligned in one step.
8. **Per-arm block-scoped locals defeat cross-jumping** (0x228278):
   `int cur, nc;` declared INSIDE each of the up/down arms gives each
   arm its own pseudos (cur = v1 in one arm, a1 in the other), so the
   two identical `hdr->cursor = cur` restore tails are NOT
   byte-identical and do not get cross-jumped; one shared `cur` in
   function scope allocates one register, makes the tails identical,
   and the compiler merges them 2 insns short of retail.  59→67.
9. **No pointer variable for a struct accessed from several arms**
   (0x228278, probe_mmi9 — the matching lever): write `mainHdr.cursor`
   directly in every arm.  Each arm's ebb-local cse then derives its
   own `addiu a0,s0,-24` base (reorg hoists the two copies into the
   guarding branch's delay slot and deletes them), and the cross arm's
   single read comes out as retail's `lw v0,-8(s0)` off the anim
   pointer.  A `Hdr *hdr = &mainHdr;` variable stays live into the
   cross arm and the read becomes `lw v0,16(a0)` — one word off
   forever.  67→72; dropping the variable → 73/73 MATCH.
10. **The shared then-tail goes at the END of the second arm behind a
   `goto out`** (0x228278): retail's shared `soundThunk(20992,1,6)`
   block sits between the down arm's restore and the cross test, both
   arms reaching it by `goto sound`, the down-arm restore skipping it
   with `goto out` (out: at function end).  Writing sound inline in
   either arm flips the `slt/bnez` sense and the block order.

## ROM structure discoveries

* **Two different header layouts coexist**: the config page 0x27BE28 is
  the 84-byte Page (mode +0x18 = the 0x27BE40 "screen flag", anim
  +0x1c = 0x27BE44), but the main-menu header 0x27BE90 is a SHORTER
  struct with its Anim at **+0x18** (0x27BEA8) — no `mode` field.
  0x228460 writes both through single shared bases, so this is
  source-level layout, not aliasing.
* **0x228460 body** (all confirmed): `*(short*)(gp-30432) = 0`;
  `0x267068(0x352800, 512.0f, camCtx+12, camCtx+16, 2048.0f, 2048.0f,
  1.0f, *(float*)(gp-32132), 1.0f, 65536.0f)` (9 float args, 10th on
  stack); `0x267630(0x352840)`; `*(gp-28844)=0`; `0x21ED18()`; then
  durations: `gp-30400 = rate*40/60`; `0x27EC00 = gp-30400`;
  `0x27EC40.duration = rate*40/60 + rate/6`; `cfg anim duration
  (0x27BE44) = *(gp-30380) + rate*40/60 + rate/6`; `gp-30396 = rate/6`
  (= the port's mainMenuDur); main anim duration = rate/6;
  `0x2217D8()`; `timerReset(0x27BEA8)`; tail:
  `if(0x204378()) { timerClose(0x27BEA8); j 0x2241C0 } else
  j 0x200B80(20500, 2)`.
* **0x228110 (DrawMainMenu)**: guard order is
  `timerIsState(0x27BEA8, 2)` FIRST, then 0x227FC0 (non-zero = "no
  other screen", draw proceeds); `setDrawBuf(0x1F0A10, drawBufSel, 0,
  *(int*)0x27B448)` — the field argument is an int at camCtx+8;
  `setBlend(1,2)`; `osdTextSetScale(1.0f)`; loop reloads
  nitems/cursor/items from the header every iteration; the if/else arms
  each contain a FULL `drawTextC(430, y+i*16, col, alpha,
  osdGetString(items[i].strid))` call (two calls in source — a ternary
  colour would emit a select, retail duplicates the block); colours
  0x27B830/0x27B840, %hi halves hoisted into s5/s4.
  `osdGetString`'s result is drawTextC's 5th arg in t0.
* **0x228278 (MainMenuInput, MATCH)** — full recovered source in
  `src/mmi.c`.  Semantics: guards timerIsState(0x27BEA8,2), 0x227FC0,
  and `*(int*)0x27B444 == 0` (a busy/override int at camCtx+4 the port
  does not model); pad word = *(gp-30316); bit 0x1000 = cursor up
  (decrement, restore old value on underflow — NOT clamp-to-zero: the
  old value is stored back), 0x4000 = cursor down (same pattern against
  nitems), both playing 0x2287A8(20992, 1, 6) on success; 0x20 =
  confirm: cursor 0 + fadeState()==0 → 0x227F50(0) (enter Browser),
  else cursor 1 → 0x227268() (enter System Configuration); 0x10 →
  0x2210C8() (leave).  So on THIS pad word 0x20 is confirm and 0x10 is
  cancel/back.  The port's MainMenuInput is semantically right except
  it lacks the 0x27B444 guard and the underflow-restore spelling.
* The port's semantic map (osdbits/menutext.c) is accurate for both
  close functions modulo those details (guard order; the y+i*16
  spelling; the main-menu Anim being embedded in its header rather than
  separate).

## Files

* `src/mmi.c` + `mmi-functions.txt` — 0x228278 **MATCH**.
* `src/init.c` + `init-functions.txt` — 0x228460 best variant
  (statement order S4,S2,S1,S3,S6,S5 relative to source field order).
* `src/dmm.c` + `dmm-functions.txt` — 0x228110 best variant.
* `cluster-functions.txt` — the three rows combined.
* `src/probe_init*.c`, `src/probe_dmm*.c`, `src/probe_mmi*.c` — the
  sweeps (kept: they document which levers were exhausted).
* `dis/0x*.dis` — ROM disassembly of all 15 cluster targets, for the
  next session.

## Verdict counts

attempted 3 / **matched 1 (0x228278, 73 insns)** / close 2 / far 0 /
untouched 12 (~2030 insns).  The closes: 235 of 250 attempted
instructions placed identically, 249 of 250 content-identical; both
residues are the documented scheduler-tie plateau class (statement
permutations swept to exhaustion: 720 orders on 0x228460, plus temp
variants; no content delta left in either).

Session was time-boxed by the coordinator after the third function;
the recommended next targets, cheapest first, are 0x21d3a0 /
0x21d590 / 0x21d7f8 (DrawTopBar / DrawIcon / DrawHintSet - the port's
menutext.c models all three closely and idioms 3/7/8/9 above should
transfer), then the big drawers 0x227560 / 0x2288c0.
