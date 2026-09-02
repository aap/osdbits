# m-leaf agent - small-function cluster + matrixDrive finish (2026-09-02)

Work dir: `/u/aap/.claude/jobs/58e316f8/tmp/m-leaf/`.  `ref/` is a
throwaway copy of `matching/` (nothing outside this dir written, no
commits).  Every score below was reproduced with `ref/try.sh` /
`ref/vary.py` on the retail toolchain (freesce 991111 cc1 + gas,
unpad.py in the pipe); reproduce with:

    sh ref/try.sh src/small1.c small1-functions.txt
    sh ref/try.sh src/matrixdrive2.c matrixdrive2-functions.txt

## Headline

* **Module U matched by address: 73 -> 96** (of 323; union of all
  matching/ tables plus my two TUs, scored per address by `status.py`).
  23 new byte-exact functions inside Module U, +1 more (RotZ) in the
  matrixDrive service region just above it = **24 new MATCHes**.
* Cluster remaining (unmatched, <= 0x120 bytes): 176 -> 154 functions
  (5,933 -> 5,684 insns).  The session was time-boxed by the parent
  before the next batch could start; the pipeline (status.py list ->
  dis.sh -> batch TU -> vary.py sweeps) is set up and cheap to resume.

## Scorecard - src/small1.c (small1-functions.txt), 23/27 MATCH

| function | va | insns | verdict |
|---|---|---|---|
| thunk_21E3B0 | 0x21e3b0 | 2 | MATCH (5th-arg-adding tail thunk: `j` + `li t0,1`) |
| thunk_21F150 | 0x21f150 | 2 | MATCH |
| nullsub_21F158 | 0x21f158 | 2 | MATCH (scan missed it; 0x21f150's inventory size 0xc is really thunk@150 + nullsub@158) |
| nullsub_21F160 | 0x21f160 | 2 | MATCH |
| nullsub_220798 | 0x220798 | 2 | MATCH |
| clr_21EE50 | 0x21ee50 | 10 | MATCH |
| step_2202C8 | 0x2202c8 | 12 | MATCH (timerStep(ptr+7) through a $gp pointer) |
| step_2217A8 | 0x2217a8 | 12 | MATCH |
| close_223680 | 0x223680 | 13 | MATCH |
| reset_225CF0 | 0x225cf0 | 10 | MATCH (order: a=0; b=gpfloat; c=-1.0f; gpflag=0) |
| depth_225D18 | 0x225d18 | 10 | MATCH (MulMatrix into local, return m[3][2]) |
| idle_226950 | 0x226950 | 11 | MATCH (`return timerIsState(fn(),0)==0`) |
| step_226FA8 | 0x226fa8 | 10 | MATCH |
| step_227DE8 | 0x227de8 | 12 | MATCH |
| step_2283A0 | 0x2283a0 | 12 | MATCH |
| push_229A18 | 0x229a18 | 12 | close (7 differ, right count): register-naming tie; see below |
| push_229A48 | 0x229a48 | 9 | close (2): only the initial lui/lw pair swapped - in-block sched tie |
| clamp_229A70 | 0x229a70 | 13 | FAR (4/13 aligned): unexplained frame artifact, see open question |
| bcd_22B270 | 0x22b270 | 14 | FAR (7/14 aligned): same artifact |
| clear_22FE88 | 0x22fe88 | 13 | MATCH (do-while + `(int)` pointer compare) |
| chk_21EC98 | 0x21ec98 | 16 | MATCH |
| chk_21ECD8 | 0x21ecd8 | 16 | MATCH |
| fetch_224FF0 | 0x224ff0 | 17 | MATCH (`t->idx * 48` written as a plain multiply -> real `mult`) |
| open_226B28 | 0x226b28 | 17 | MATCH |
| close_226B70 | 0x226b70 | 17 | MATCH |
| open_227F08 | 0x227f08 | 17 | MATCH |
| arm_2272F0 | 0x2272f0 | 17 | MATCH |

## Scorecard - src/matrixdrive2.c + the old matrixdrive.c residuals

| function | va | verdict |
|---|---|---|
| MatrixDrive_RotY | 0x230260 | close: 44/50 strict, 46/50 aligned.  Exhausted: all 2880 statement permutations (one 6 s vary.py build) + 8 RTL-shape levers (chained zeros, zero-via-temp, row pointers, literal 1.0f, ns temp, both memsets first, call order swap) all plateau at 6.  Residual = 4 words where the ROM weaves the two row-0 zero stores into the first memset's call setup, + the one/c store swap after it.  Same in-block sched-tie class as RotX's 49/50 (campaign/PENDING.md). |
| MatrixDrive_RotZ | 0x230328 | **MATCH (47)**.  Winning order puts `m[0][0]=c` late (between `m[1][1]=c` and the row-1 zeros) - found by one genperm sweep, 3 of 57 variants match. |
| MatrixDrive_RotX | 0x230198 | untouched, stays 49/50 (documented plateau). |
| MatrixDrive_TranslateV | 0x2303e8 | 17/21 aligned confirmed stuck: tried 11 more addressing spellings (struct-row field, char-cast +48 bound to the symbol, row-array views, q[mtxSp*4] through a u_long128* to force `(sym+48)` as an atomic constant) - ALL compile to byte-identical output; gcc 2.9 reassociates every one to `(sym + idx*64) + 48` and folds the 48 into the sq displacement, while the ROM rebuilds `sym+48` from the live s0 with `addiu s0,s0,48`.  No source spelling reaches it. |
| MatrixDrive_InitSinTable | 0x22ff70 | untouched, stays 37/42 ($at-vs-$a0 lui temp, documented). |
| MatrixDrive_PushMatrix / PopMatrix | 0x2300b8 / 0x230138 | pad-blocked (notes-moduleU §3.2) - NOT forced, left as documented non-matchable tails. |

## The mdRot axis verdict (asked for explicitly)

**osdbits has NO axis-naming swap - its mdRotX/Y/Z names are correct.**
Read straight off the ROM's stack stores (each rotator builds the local
matrix at $sp then MulMatrix's it onto the stack top):

* **0x230198**: row0=(1,0,0,0), row1=(0, c, s, 0), row2=(0, -s, c, 0),
  row3=(0,0,0,1) -> **RotX**.
* **0x230260**: row0=(c, 0, -s, 0), row1=(0,1,0,0), row2=(s, 0, c, 0)
  -> **RotY**.
* **0x230328**: row0=(c, s, 0, 0), row1=(-s, c, 0, 0), row2=(0,0,1,0)
  -> **RotZ**.

(The task brief's "0x230240/0x2302E8" addresses were mid-function; the
real starts are 0x230260/0x230328.)  osdbits `menu.c`/`inc.h` map
mdRotX=0x230198, mdRotY=0x230260, mdRotZ=0x230328 - exactly what the
trig/row layout says.  All three use the same row-vector (post-multiply)
sign convention: +s above the diagonal for X and Z (m[1][2], m[0][1]),
-s above for Y (m[0][2]) - i.e. the standard right-handed set.  The
suspicion that 0x230198 "should be RotZ per osdbits naming" is refuted;
the earlier session's note ("0x230198 is RotX") AGREES with osdbits.

## New compiler idioms (additions to the log)

1. **Signed `slt` on a pointer loop = `(int)` cast compare, and
   do-while kills the entry guard.**  0x22fe88's loop tests
   `slt` (signed) with no pre-test.  `for (p = a; p < &a[10]; p++)`
   gives `sltu` + an entry guard on incomplete extern arrays;
   `p = a; do { ... p++; } while ((int)p < (int)&a[10]);` is byte-exact.
   Whenever a ROM address-comparison loop uses `slt` not `sltu`, the
   source compared through int.
2. **n32 5th argument in `t0`**: a two-instruction thunk
   `j f; li t0,1` is just `void g(a,b,c,d){ f(a,b,c,d,1); }` - gcc
   passes a0-a3 through untouched (0x21e3b0).
3. **24-byte GIF-node structs must not contain `u_long128`** - the
   TImode member's 16-byte alignment pads sizeof to 32 and breaks the
   `p + 1` advance; the retail shape is a plain-int struct with the
   qword stored through a `u_long128 *` cast (0x229a18/0x229a48).
4. **Argument loads happily go in jal delay slots** (`jal f; lw a0,12(s0)`
   caller pattern, 0x21ec98 family) - no special source shape needed,
   -O2 does it; don't let it mislead structure reading.
5. **`t->idx * 48` compiles to a real `mult`** (0x224ff0) - 991111 does
   not synth-mult by-48 scaling here; write the multiply, not shift-adds.
6. **Float -1.0f materialises as `lui at,0xbf80/mtc1`** (0x225cf0), the
   same zero-low-half no-literal-pool path as 3.0f/1.0f (notes §3.5).
7. **Open question - the undefined `sw $2,0($sp)` frame artifact.**
   0x229a70 (a 3-way clamp) and 0x22b270 (bin->BCD) both open a 16-byte
   frame and store UNDEFINED $v0 to 0($sp) right after the prologue
   (bcd also carries a dead `li a0,10`).  Probably the same retail TU.
   ~12 source shapes (if/else chains, ?:, uchar temps/returns, split
   div/mod temps) never reproduce it; it looks like the 2.9
   uninitialized-pseudo stack-slot bug triggered by something in the
   original source (an uninitialized variable on some path?).  Both
   functions are otherwise structurally right (branch+movn clamp;
   div/mfhi/mflo BCD).  Worth one dedicated dig - whatever construct
   causes it probably recurs in that TU's bigger functions.
8. **RotZ's statement-order win generalises the §3.6 lesson**: even
   inside a straight-line store block, moving ONE assignment
   (`m[0][0]=c` to after `m[1][1]=c`) flips the whole block's schedule
   into place.  Sweeps stay the only tool that finds these; the 2880-
   variant full-permutation sweep for RotY compiled and scored in 6 s,
   so exhausting a store block is now routine.

## Files

* `src/small1.c` + `small1-functions.txt` - batch 1 TU (27 functions).
* `src/matrixdrive2.c` + `matrixdrive2-functions.txt` - RotY/RotZ.
* `genperm2.py` - generic single-statement-relocation probe generator
  (spec-file-driven version of matching/genperm.py).
* `status.py` - unions all matching/ tables + my TUs by ROM address and
  lists the remaining unmatched functions under a size cap.
* `buildall.sh` - rebuild every ref TU with the retail toolchain.
* `ref/src/probe_*.c` - the sweeps (roty 1-6, rotz 1-2, tv 2-3, s1-s10).

## Handoff for the next pass over this cluster

`python3 status.py 0x120` prints the 154 remaining (5,684 insns),
smallest first is fastest: the 21 functions of 0x40-0x80 bytes are the
same shapes as this batch (timer open/close/step guards, packet
pushers, $gp accessors) and should land at a similar ~85% first-try
rate.  Beware three known traps: the two sched-tie pushers above, the
sw-$2 artifact TU (0x229a70/0x22b270 neighbourhood - probably also
0x229aa8/0x229b38), and inventory rows whose "function" is really a
second return arm (notes-moduleU §3.7).
