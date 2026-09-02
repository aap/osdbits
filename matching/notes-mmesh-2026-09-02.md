# m-mesh agent: 0x22C900-0x22F800 big functions (mesh/glass/orb draw family)

Work dir: `/u/aap/.claude/jobs/58e316f8/tmp/m-mesh/`.  `ref/` is a
throwaway copy of `matching/`; nothing outside this dir was written.
All scores below were reproduced with `sh ref/try.sh src/<tu>.c <table>`
(retail compiler ee-gcc 2.9-ee-991111, `-O2 -Wall -fno-common
-fno-strict-aliasing`, unpad.py in the pipe).

Session was cut short by the coordinator's budget stop; one function was
driven to a verified structural plateau, and the family-wide source
idioms - which are the real yield - are proven with compiler probes.

## Scorecard

| va | size | role | verdict |
|---|---|---|---|
| 0x22C920 | 0x148 (82) | TEXCBUMP emboss emit | **close(39/82 aligned)** - all mechanisms reproduced, remainder is the register-identity/schedule tie class (src/meshemit.c) |
| 0x22CB58 | 0x190 (100) | flat fres^2 emit | analyzed, not attempted (child of 0x22CCE8 - see idioms; boundary + shape fully decoded below) |
| 0x22CD78 | 0x230 (140) | env-map/refl emit | analyzed, not attempted |
| 0x22CFA8 | 0x2f0 (188) | mesh transform + winding | analyzed (all struct offsets pinned), not attempted |
| 0x22D2E8 / 0x22D798 / 0x22D920 / 0x22E428 / 0x22EFF0 | | walkers/rod/orb | untouched (time) |

**No strict MATCH produced.**  What this session actually bought is
below - the idiom discoveries are family-wide and unblock every one of
the ~2400 instructions in this cluster.

## THE discovery: the family is written with GNU NESTED FUNCTIONS

gcc 2.9's MIPS static-chain register is **$2 ($v0)**.  Proof in the
image:

* `0x22CCE8` homes its args (`sw a0,4(sp); sw a1,0(sp)`), computes the
  fresnel (Normalize 0x2677E0 + InnerProduct 0x267818, sign folded with
  the branch-not-movz shape), then does `move v0,sp; jal 0x22cb58` -
  passing ITS OWN FRAME as the static chain.
* `0x22CB58` opens with `move s1,v0; sw v0,0(sp)` and reads the
  parent's homed vars off the chain: `0(s1)` = scene, `4(s1)` = face.
  Its own arg (fres, f12) stays in a register.
* `0x22C888` is the same wrapper pattern around child `0x22C4E0`
  (5 args homed at 16/4/0/8/12, `move v0,sp; jal 0x22c4e0`).
* Children are emitted IMMEDIATELY BEFORE their parent in .text
  (0x22C4E0 then 0x22C888; 0x22CB58 then 0x22CCE8) - that is gcc's
  output order for nested functions and explains those adjacencies.

Probes (probe/nest1.c, nest3.c) reproduce every element with the retail
compiler:

* parent homes child-referenced args to sp+0.., `move $2,$sp` at the
  call;
* child begins `move $3,$2; sw $2,0($sp)`;
* an **`inline` nested child called once is integrated**: no separate
  body, but the parent still homes the args AND materializes the chain
  as a plain register copy of sp (`move $8,$sp`), through which every
  child-body access goes;
* with **-fno-strict-aliasing** (osdbits' flag set - now proven
  load-bearing) each homed var is RELOADED from its slot after every
  store through an arbitrary pointer, i.e. once per emit-loop
  iteration.  Without the flag gcc caches them (probe nest1b vs nest2).

So 0x22C920 / 0x22CA68 / 0x22CD78, which home their own args and walk
them through an sp-copy register with per-iteration reloads, are
"parent + integrated inline child" single bodies.  A plain top-level
function CANNOT reproduce their codegen; the inline nested child is
mandatory.

### Home-slot order = child first-reference / definition order

The homed-arg slot order is NOT parameter order; it is the order the
child(ren) first reference the vars, and with several children the
definition order of the children (probe series bump3-bump5).  0x22C920's
observed ox@0 oy@4 col@8 f@12 is reproduced exactly by three inline
children defined in the order: mkst (refs ox,oy), mkrgba (refs col),
emit (refs f).  This gives per-function slot maps for the untouched
siblings:

* 0x22CA68 (black emit): f@0 - one child, refs f only.
* 0x22CD78 (refl emit): f@0, aa@4, sc@8 - child ref order f, aa, sc.
* 0x22CCE8: sc@0, f@4; 0x22C888: x@0, sc@4, y@8, ?@12, f@16 (args
  homed for children 0x22CB58 / 0x22C4E0; the slot order pins the
  order those children reference them - useful when reconstructing the
  children's bodies).

## The packet idioms

* `0x3529B0` is a menudraw-style `Pkt` struct: `cur`@+0 (the emits'
  `10672(v1)` loads), `giftag`@+20 (0x22CB58/0x22CA68 do
  `sw <tagpos>,20(s0)` after copying the GIFtag template).  0x22CB58 and
  0x22CA68 call pktOpen (0x2293E0) / pktKick (0x2294B8) themselves;
  0x22C920 and 0x22CD78 append raw (their caller opened the packet).
* GIFtag REGLIST templates are copied with **`lq`/`sq`** from
  0x27F870/880/890/8A0/8B0 (`lq a0,0(v0); sq a0,0(cur)`) - a 16-byte
  struct containing a `u_long128` (mode(TI)) per the established law.
  0x22D920's arms do the same from 0x27F890/8A0 (`lq a1,16(v0)` = the
  +0x10 template).
* Payload pushes go through `u32 *p = pk.cur` with the 64-bit register
  value stored as `*p++ = v; p[0] = v >> 32; p[1] = ...` - the
  mixed post-increment/indexed shape is what produces the ROM's single
  mid-stream `addiu cur,cur,4` with displacement addressing after it
  (probes p1.c/p2.c: a `u64 *` gives `sd` - wrong; all-`*p++` gives
  addu-after-every-store - wrong).
* 64-bit value composition: `v` is a SIGNED `long` (`s64`); shifts must
  be 64-bit (`(s64)x<<8` etc. produce the ROM's `dsll`, an `int` shift
  would emit `sll`); `v >> 32` gives the `dsra32`; the low-word store
  through `u32*` gives the canonical truncate `dsll32+dsra32+sw`.
* Float-bits reads out of struct fields are direct `*(u32*)&vp->q` -
  `lwu` (zero-extending, so type it u32).
* Float-bits of freshly COMPUTED floats round-trip the stack
  (`swc1 36/40(sp)` + `lwu`).  Writing `*(u32*)&s` on a scalar local
  gets folded by the front end into a register bit-move (`mfc1` +
  shifts) - WRONG.  Taking the address into pointer VARIABLES
  (`u32 *ps = (u32*)&s;` ... `*ps`) forces real slots and the exact
  swc1/lwu sequence, and also reproduces 0x22C920's frame size (-48)
  exactly.  A `float st[2]` array also forces memory but gets 16-byte
  alignment (frame -64) - wrong; a union F2I gets registered - wrong.

## Data layout (verified against 0x22CFA8/0x22C920/0x22CA68/0x22CB58/0x22CD78 loads)

Face record 0x160 bytes (banks: rods at 0x3529D0 = gp?-relative
`s6+10704` in 0x22D2E8):

    MeshVert v[4];       // 0x50 each: cam[4]@0, u@0x10, v@0x14,
                         //   proj[4]@0x20, fix[4]@0x30 (FTOI4), q@0x40
    float normal[4];     // 0x140
    int cull;            // 0x150   (=!(cross>0) screen winding)

0x22CFA8's real signature is NOT the port's:
`(float *ox, float *oy, float *oz, MeshFace *faces, Scene *sc)`,
faces may be NULL (early-out after writing the projected origin minus
2048/2048/none through the three out-pointers).  Scene: world matrix
embedded at +0x20 (not a pointer), viewscreen ptr @+0x60, camera ptr
@+0x64, scale triple @+0x68/6C/70, nfaces @+4, verts ptr @+8 (stride
16/vertex), norms @+12 (16/face), uvs @+16 (16/vertex... uv stride 16
per 4 verts*4B? loads are `i*64 + [sc+16]`, 2 floats per vertex read at
+0/+4 with stride 16 per vertex).  Colour blocks: +0x80 (flat), +0xA0
(bump col, 4 ints), +0xB0/B4 (bump uv offset pair), +0xC0 (refl col).
Library calls: 0x267860 MulMatrix, 0x2678A8 ApplyMatrix, 0x2676E0
ScaleVector(dst,src,f12), 0x267668 FTOI4, 0x2676F8 SubVector, 0x2677E0
Normalize, 0x267818 InnerProduct, 0x267050 ScaleVector?, 0x267710
AddVector, 0x25A368 float->int (called with f12, result in v0).
Constant origin vector at 0x27F860.

Inside 0x22CFA8 the per-vertex scaled position is built at sp+144..156
and then COPIED 16 bytes to sp+128 with `ld/sd` pairs before
ApplyMatrix - an 8-byte-aligned aggregate assignment (struct of two
u64s / u_long pair, NOT u_long128 which would be lq/sq, NOT float[4]
which would be lw/sw) - unresolved which source spelling; try a
`sceVu0FVECTOR`-typedef'd union or a struct {u_long l[2]} copy.

## 0x22C920 remaining delta (close, 39/82 aligned, reproduced)

All 82 instructions present with the right mechanisms (same count, no
missing/extra after alignment nets out); every difference is register
identity (ours $6/$3/$4... vs real v1/a2/a3...) plus the schedule
cascade that follows from it, starting at the very first `lui`.  The
known tie class from campaign/PENDING.md.  Two sub-shapes still worth
sweeping when resumed:

* real computes the T statement's float chain before S (loads
  v@+20/oy first) while keeping slots s@36 < t@40; with `s` textually
  first ours keeps S first.  A shared `float q = vp->q` local plus
  statement swap moves single instructions but did not close the gap
  (bump9 probes).
* the prologue interleave of the four homing stores with the packet
  header writes.

vary.py one-liner to resume:
`python3 ref/vary.py probe/bumpA.c 0x22c920 0x148`.

## Files

* `src/meshemit.c` + `meshemit-functions.txt` - the 0x22C920 candidate
  (verified 39/82 aligned via try.sh).
* `probe/nest1.c nest2.c nest3.c` - the nested-function/homing proofs.
* `probe/p1.c p2.c` - the u32*/64-bit store-split proofs.
* `probe/bump1..9,A.c` - the slot-order/bitcast sweep history.
* `modU.dis` - full Module U disassembly (objdump mips:5900).
* `dis.sh` - window disassembler.

## Handoff notes for the rest of the cluster

* 0x22CA68 is <=0x120 (another agent's), but whoever takes 0x22CB58 /
  0x22CD78 should start from the nested-function skeletons here; both
  are one-afternoon functions now that the idiom is known.
* 0x22D2E8/0x22D798/0x22D920/0x22E428/0x22EFF0 all call this family
  with plain args (no chain), so they are ordinary functions; their
  giftag pushes use the lq/sq template copy + `sw tagpos,20(pk)` +
  cur bumps seen in 0x22CB58.
* IDA's 0x22E428 (0x838) and 0x22EFF0 (0xe94) spans likely hold the
  0x22E4D0/0x22E9A8 and 0x22F0CC+ sub-functions the task named -
  boundaries not yet re-derived (check for `jr ra` + parent-adjacency:
  nested children sit immediately before their parent).
