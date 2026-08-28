Tower subsystem — decode notes (osdsys_dump.idb)
================================================
reverse-engineering notes for implementing the towers in osdbits.
all addresses = real OSDSYS VAs (file offset + 0x200000).

BIG PICTURE
-----------
The tower field is the boot-history visualization: OpeningInitTowersFog
parses the OSDSYS boot history (21 entries x 22 bytes @ 0x1f0138,
"history" in the IDB; bytes: +16 = type, +17 = bitmask, +18 = index)
and marks tower cells in a 20x20 flag grid. The towers are drawn per
cell by EE-side helpers that fill VIF1 parameter blocks in the gap area
between the VU1 program blobs; a VIF1 DMA then feeds each block to VU1
microcode, which transforms and draws the tower geometry. The tower
wall texture is TEXOWAL0 (mipmapped, vif1SetTextureMIP(tex, 1, 5, -65)).

DATA (BSS = runtime scratch, zero at load)
------------------------------------------
  0x325f90  int towerFlags[20][20]   (stride 80 = 20 ints; memset 1600)
  0x3265d0  float towerGrid[20][20]  (heightfield, computed by sub_217e30)
  0x326c10  float towerPos[14][9][4] (stride 144; screen positions,
                                      transformed from 0x28a670 by
                                      OpeningInitTowersFog)
  0x3273f0  float towerA[14][9]      (stride 36; alpha values)
  0x3275e8  float towerB[14][9]      (stride 36)
  0x3277e0  float towerC[14][9]      (stride 56)
  0x327af0  float towerD[14][9]      (stride 56)

DATA (.data, runtime-initialized)
---------------------------------
  0x279f30  scene vectors (6 x vec4, filled by InitOpeningScene) -
            CORRECTED 2026-08-24, was wrongly given as 0x289f30 here
            (off by 0x10000) - re-derived from InitOpeningScene disasm
            and confirmed against the live savestate (position.z reads
            back 23.08, mid-flight from its init 16.0; light1/2/3 read
            back their unchanged init constants - see "the radial
            brightness falloff" section below):
              [0] position {0, 0, 16.0, 0}
              [1] fwdDir   {0, 0, 1.0, 1.0}
              [2] upDir    {0, 1.0, 0, 1.0}
              [3] light1   {0, -1.0, 0, 0}
              [4] light2   {0.5, 0.5, 0, 0}
              [5] light3   {-0.5, -0.5, 0, 0}
  0x28a210  table read by OpeningInitLightsCubes (16-byte entries, idx =
            s0%5)
  0x28a260  tower cell table: 21 rows x 6 pairs of (row,col) ints
            (48 bytes/row) -- which cells each history entry enables
  0x28a650  runtime copy of the 6 VIF1-block pointers (from 0x27a650)
  0x28a670  tower source positions: 14 x 9 x vec4 (stride 144)
  0x28ae50  vertex build buffer (tower_218318 dest)
  0x28aeb0  int: tower texture index (read by DrawTowers)
  0x28aee0  float table A (indexed by col*4; >=14-type path uses
            ((type-14)%10 + 4)*4)
  0x28af18  float table B (same indexing)  (= "flare_21AF18" region)
  0x288b40  Texture struct array, 240 bytes/entry (DrawTowers picks
            textures[*(0x28aeb0)])
  0x27a650  static: 6 pointers into VU-gap parameter blocks
            (0x26873c, 0x2682cc, 0x2683e8, 0x268504, 0x268620, 0x2681b0)
  0x2a7188  0.85    0x2a718c  0.8
  0x2a7190  0.01745329 (deg2rad)  0x2a7194  1.5707964 (PI/2)
  0x2a7198  -0.12   0x2a719c  4.8
  0x2a7174  5202.0  0x2a7178  5.1   0x2a717c  2.55
  0x2a7180  -5.1    0x2a7184  10.2
  0x2a7758  spr alloc ptr 1 (gp-31000, set by sprInit)
  0x2a775c  spr alloc ptr 2 (gp-30996, set by sprInit)  [tower src data]
  0x2a7738  global float set by InitOpeningScene (from 0x2a7188 = 0.85)
  0x2a7748  VU status pointer (gp-31056, passed to status helper 0x266b08)
  0x2a77d0  21 x 22 bytes: expected-history table (compared with
            history[s0] via strcmp 0x25bddc)
  0x2a77f8  function pointer slot set by OpeningInitLightsCubes
            (= 0x216f88, the function right after DrawLights)

HELPERS (library region, all statically linked libvu0-style code)
-----------------------------------------------------------------
  0x2676f8 = vector subtract (sceVu0SubVector)   {a0} = {a1} - {a2}
  0x267710 = vector add      (sceVu0AddVector)
  0x267860 = matrix x 4 vectors: for i in 0..3: dst[i] = M * src[i]
             (a0=dst, a1=M, a2=src)
  0x2676b0 = matrix copy + row translate: copy 3 rows a1->a0, then
             dst[3] = src[3] + vec(a2)  (lqc2 vf4,(a2) vadd.xyz)
  0x267370 = composite rotation matrix from euler angles
             (a0=mat, a1=angles xyz): rotZ(mat, z); rotY(mat, y);
             rotX(mat, x)  [0x267510 = rotZ?, 0x2673c0 = rot step]
  0x266b08 = VU1 status poll: if a1==1 read (*(a0)>>8)&1 else
             *(a0)&0x100
  0x262418 = sync helper (probably sceGsSyncPath/DmaSync wrapper)
  0x25a4a8 = helper called from tower_218318 (0x25a4a8)
  0x253e38 = sqrtf   0x253c08 = sinf  0x25bddc = strcmp
  0x24dce0 = RPC/file helper also called by main

FUNCTION: sub_217e30 -- height grid compute (212 insns)
------------------------------------------------------
for row r = 0..19 (x = (r-10)*5.1):
  for col c = 0..19 (y = (c-10)*5.1):
    d1 = sqrt((x+2.55)^2 + (y+7.65)^2)
    d2 = sqrt((x-2.55)^2 + (y-7.65)^2)
    R  = sqrt(5202.0)   ( = 51*sqrt(2) )
    v  = clamp(255*(R - 2*d1)/R, 32, 255)
       + clamp(0.5*255*(R - 4*d2)/R, 32, 220)
    v *= 10.2
    v -= 10.0 * (( (r*(r+c)/(c+1)) % 11 ) - 5)
    towerGrid[r][c] = clamp(v, 32, 220)
(per-cell tweak = the (r*(r+c)/(c+1))%11 - 5 term; integer div)

FUNCTION: OpeningInitTowersFog (0x218e00, 276 insns)
----------------------------------------------------
1. memset(towerFlags, 0, 1600)
2. for s0 = 0..20:                       # history entries
     if strcmp(history[s0*22], 0x2a77d0+s0*22) == 0:
       type = history[s0*22+16]; bits = history[s0*22+17]
       idx  = history[s0*22+18]
       for t0 = 0..5:                    # 6 cells per entry
         (row,col) = cellTable[0x28a260][s0][t0]
         if t0 == idx:
           if type < 14:
             towerD[row][col] = tableA[0x28aee0][col]
             towerC[row][col] = tableB[0x28af18][col]
           else:
             k = (type-14) % 10
             towerD[row][col] = tableA[k+4]
             towerC[row][col] = tableB[k+4]
           towerFlags[row][col] = 1
         elif (bits >> t0) & 1:
           towerFlags[row][col] = 1
           towerD[row][col] = 1.0
           towerC[row][col] = 1.0
3. VU1 kick: D1_QWC(0x10009020)=0; D1_TADR(0x10009030)=0x2678e0;
   sw 2, 0x1000e010; RPC call 0x24dce0(0); D1_CHCR(0x10009000)=325
   (=chain-mode VIF1 transfer of the parameter block at 0x268020);
   wait: 0x266b08(*(gp-31056), 0, 0); sync 0x262418(0,0)
4. for r = 0..13, for c = 0..8:          # transform source positions
     (x,y,z) = srcTable[0x28a670][r][c]
     towerPos[r][c] = { (x+0.85)*4.0, (y-6.5)*4.0, (z+4.0)*12.0+150.0 }
5. for r = 0..13, for c = 0..8:          # alpha/halo tables
     f0 = towerD[r][c]*30.0;  if f0 < 3.0: f0 = 3.0
     towerA[r][c] = f0
     f3 = towerC[r][c]
     towerPos[r][c][2] += f3*30.0 - f0
     if 1.0 <= f3: towerC[r][c] = 1.0; towerB[r][c] = 0
     else:         towerB[r][c] = (float)(int)((1.0 - f3)*128.0)
6. sub_217e30()                           # height grid
7. InitFog()                              # 0x214f78
8. tail: j DrawToExtraBuf2 (0x214050)

FUNCTION: InitOpeningScene (0x218c88, 61 insns)
-----------------------------------------------
fills 0x279f30 scene vectors (see DATA), stores 0.85 to gp-30936,
vif1SetAD(TEXA, SET_TEXA(127,1,129)), vif1SetAD(FBA_1, 1),
tail j OpeningInitLightsCubes (0x217ab0).
=> aap's reconstruction (opening.c InitOpeningScene) matches this
   exactly in values; only the store order differs.

FUNCTION: OpeningInitLightsCubes (0x217ab0, 194 insns)
------------------------------------------------------
- reads constants 0x2a7188..0x2a719c (0.85, 0.8, deg2rad, PI/2, -0.12,
  4.8)
- writes function pointer 0x216f88 to gp-30824 (0x2a77f8)
- reads table 0x28a210 (16-byte entries, index s0%5)
- computes tower source positions with sin/cos into 0x28a670 etc.
- writes to 0x27b0f0 / 0x27b140 / 0x27b190 (parameter area)
- (aap's reconstruction = 76 insns, covers only the light-trail part;
  the tower position generation is missing)

FUNCTION: DrawOpeningScene (0x218d80, 31 insns)
-----------------------------------------------
  DrawTowers()                        # 0x2185c8
  DrawExtraBuf2(1, 2, 80, 0xffffff)   # 0x214240
  DrawToExtraBuf2()                   # 0x214050
  DrawFog()                           # 0x215238
  if *(float*)0x289f38 >= 73.0:       # global gate
    DrawLightsAndCubes()              # 0x217db8
  trail_fn_218b20()                   # 0x218b20
  tail j 0x218bd0

FUNCTION: DrawTowers (0x2185c8, 342 insns)  [core, partially decoded]
--------------------------------------------------------------------
  vif1SetZWrite(1); vif1SetZTest(1)
  # shadow walk (rows 0..13): s2 = walk value per row; reads
  # *(ptrGlobal+32*i)/+36*i (ptrGlobal = gp-30996); jal 0x2676f8
  # per row: for c in 0..8: f1 = |a| - |b|; if (float)s2 < f1: s2++
  #   (computes the per-row max "ceiling" = tower shadow level)
  # texture: idx = *(0x28aeb0); tex = &texArr[0x288b40 + idx*240]
  #   vif1SetTextureMIP(tex, 1, 5, -65)
  # rotation: f12 = ((frameCount % 360) - 180) * deg2rad(0x2a7190)
  #   f0 = sinf(f12)*10.0; f23 = f0 * deg2rad
  # per row r = 0..13, per col c = 0..8:
  #   if towerGrid[r][c] <= 0 or !towerFlags[r][c]: skip
  #   build quad: f2 = towerGrid[r][c]
  #     if f2 == 1.0: f0 = -1.0; grid[r][c] = -1.0 ... (special path)
  #     s1 = (r+3)+(c+6); a0 = s1*(r+3); div by 7 -> rounded-up
  #        multiple of 4; f0 = (s1*(r+3) - round4)*f22 + f23
  #   helper 0x267370(mat, angles from sp+16)
  #   helper 0x2676b0 / 0x267860 (matrix ops, src = towerPos[r][c])
  #   helper 0x266b08 status; tower_218210(0x80...); tower_218288(0,1)
  #   tower_218318(r, c, gridVal)   # builds 6x4 vertex quads
  #   tower_2184d0(a = s1*(r+4)/(c+1) + s1*(r+5)/(c+3))  # UV quads
  #   tower_218180()                 # copy tower params -> VU blocks
  #   VIF1 kick: D1_QWC=0; D1_TADR=0x268020; sw 2,0x1000e010;
  #   sync 0x262418(0,0); D1_CHCR=325
  # (aap has none of this in osdbits)

FUNCTION: tower_218180 (36 insns)
---------------------------------
t1 = *(gp-31000)  (spr alloc ptr)
copy 16 bytes: t1+0xc0 -> 0x268030 | uncached; t1+0x80 -> 0x268070;
t1+0x180 -> 0x2680f0 (4 words each)   [per-tower VU params]

FUNCTION: tower_218210 (30 insns)
---------------------------------
writes GIF-ish header block at 0x268150|uncached:
  qword {0x00008002, 0x10000000}, qword {14, 0}, words {0, 66, 0, 73}
  (0x268150/0x268180 area = second parameter block)

FUNCTION: tower_218288 (36 insns)
---------------------------------
args (a0, a1): builds the DMAtag + 6 block pointers:
  *(0x268020) = 0x8004; *(0x268024) = 0x2681a0
  for i in 0..5: addr = ptrTable[0x28a650][i] | uncached
    *(addr+0) = 0x8004; *(addr+4) = a3 (=0x30004000|tag stuff)
    *(addr+8) = 0;  *(addr+12) = 1042
  a0/a1 select variant (a0<<6 | a1<<7 | 28... tag word arithmetic)

FUNCTION: tower_218318 (109 insns)
----------------------------------
args (a0=row, a1=col, f12=grid value): builds 6 rows x 4 cols of
vertex quads into 0x28ae50, reading towerGrid values; if cell flag
0 -> negate; writes {x, y, z, 128.0} per vertex; calls 0x25a4a8 on
zero entries. (vertex positions from towerGrid neighbours)

FUNCTION: tower_2184d0 (61 insns)
---------------------------------
args (a0 = texture row index): writes UV quads for the 6 tower sides
into ptrTable[i]+0xd0: vertices {t, t, 1.0, 0} where t = a0/256.0
(4 vertices per quad, 16 words)

OPEN QUESTIONS / DECISION POINTS
--------------------------------
1. VU1 microcode: the per-tower pipeline runs VU1 code (parameter
   blocks at 0x268020/0x268150 etc. fed by VIF1 DMA, CHCR=325 chain
   transfers). osdbits has NO VU1 setup at all. Faithful port needs:
   upload vucode_1..7 + the per-tower VIF1 block protocol + the
   status-poll loop. Alternative: reimplement the tower quads EE-side
   (GIF direct, like aap's DrawFog). DECISION NEEDED.
2. Boot history: real code parses history @ 0x1f0138 (21 x 22B
   entries, type/bits/index bytes) vs the expected table at 0x2a77d0.
   osdbits has no history (standalone ELF). Default tower field
   needed (e.g. fake history or skip the check). DECISION NEEDED.
3. What fills 0x28a670 (tower source positions), 0x28aee0/0x28af18
   (float tables), 0x28a210, and the 0x27b0f0/0x27b140/0x27b190
   parameter tables: not yet traced (candidates: OpeningInitLights-
   Cubes body, sub_219f08, sub_21a438, sub_215798, InitFog).
   Most likely OpeningInitLightsCubes (it reads 0x28a210 and writes
   the 0x27bxxx area) and sub_219f08.
4. The VU1 prepass (OpeningInitTowersFog step 3) runs vucode_1
   (TADR=0x2678e0) before the transform loops - likely computes
   something the EE then reads back. Needs vucode_1 disasm to know.
5. 0x218b20 / 0x218bd0 (trail functions at the end of DrawOpening-
   Scene) not yet decoded.

LATER FINDINGS (2026-08-21, continued session)
----------------------------------------------
- VIF1 chain mechanics decoded: the per-tower chains are STATIC data in
  the image (0x268020 head: DMAtag {QWC=0x17,REF->0x2681a0} + STCYCL
  0x01000404 + UNPACK V4-32 0x6c168000 + ADDR word + float params
  (0x268048+: 102.4, 2048.0, matrices...); giftag block @0x268150
  (0x8002 NLOOP=2 EOP=1, regs 14, 66/73...); chain2 tag {QWC=0x6a,CNT}
  @0x2681a0; MSCAL 0x14000000 @0x268190; ptr block @0x2681b0).
  At runtime tower_218180 patches the UNPACK ADDR (0x268030) + params
  (0x268070/0x2680f0) from the spr buffer; tower_218288 rewrites the
  head tag to {QWC=4, REFE->0x2681a0} and the 6 sub-block tags.
- sendDma (0x22ee00): D1_QWC=0; D1_TADR=addr; *(0x1000e010)=2;
  FlushCache; D1_CHCR=325 (chain).  sub_22ee88 = sendDma(0x268860).
- UPLOADER FOUND: sub_231ce8 processes the 12-entry descriptor table
  @0x268b70 (stride 0x50): {0x70000000, 0,0,0, REF,progAddr,
  REF,paramAddr, REF,0x100000} with REF QWC = program size in qwords
  (programs referenced at start-8) + 14-qword param blocks; an
  uploaded-already flag sits at gp-30296.  The 0x70000000 marker word
  semantics still need vucode_1 cross-checking.
- NO MPG VIFcodes anywhere in the binary - uploads are all UNPACK/
  chain based.
- The tower field layout data (cell table 0x28a260, float tables
  0x28aee0/0x28af18, positions 0x28a670) is ALL zero in the static
  image: per aap it is loaded from the MEMORY CARD (boot history) at
  runtime.  osdbits uses RandomizeTowerField() as the test stand-in.

IMPLEMENTED (commit 3036eef)
----------------------------
osdbits/opening.c: vudataRelocate, sendDma, vu1Wait, HeightGrid,
RandomizeTowerField, towerPatchParams/Tags/GifTag, InitTowersFog,
DrawTowers (skeleton), wired into DrawOpeningScene.
osdbits/vudata.inc: 0x2678e0-0x26c000 verbatim (18,208 bytes).
osdbits/history.inc + history_expected.inc: boot history + expected
table from the dumps (behind TOWER_FIELD_FROM_HISTORY).

REMAINING (next sessions)
-------------------------
1. VU program upload at init: port sub_231ce8 (descriptor table
   processor) or equivalent; currently nothing loads the 7 programs
   into VU1.
2. tower_218318 (vertex quads) + tower_2184d0 (UV quads) ports - the
   per-tower data contract with vucode_1.
3. DrawTowers' shadow walk + rotation + matrix helpers
   (0x267370/0x2676b0/0x267860 = libvu0 equivalents).
4. OpeningInitLightsCubes position table generation (0x28a670 fill).
5. Exact towerPatchTags a3 field + param block layout (vucode_1.vsm
   cross-check).
6. Memcard cell/float tables for the real history path.

FINAL MECHANISM (2026-08-21, resolved with SCE eestruct.h + vucode_1.vsm)
-------------------------------------------------------------------------
- VIFcode ground truth (eestruct.h): MPG = 0x4a, MSCAL = 0x14, MSCALF =
  0x15, MSCNT = 0x17, STROW = 0x30, STCOL = 0x31, STMASK = 0x20, DIRECT =
  0x50, DIRECTHL = 0x51, UNPACK = 0x60|fmt, bit 15 (0x8000) = UNPACKR
  (main-memory source).
- Program upload: each program is preceded at start-8 by {NOP, MPG(vuaddr
  =0, num=size/8)}.  The descriptor table (44 entries x 0x50, first 12
  populated, covers vucode_3..7) is a REF-tag chain: {0x70000000 no-op
  CNT tag, REF param, REF 0x100000 no-op, REF program-8, REF param}.
  vucode_1 uploads via its own CNT tag at 0x2678e0 (the kick in
  OpeningInitTowersFog: D1_TADR=0x2678e0, CHCR=325); vucode_2 via
  sub_22ee88 = sendDma(0x268860).
- Per-tower stream (patched chain, TADR=0x268020, tag {0x8004 = REFE
  QWC=4 -> 0x2681a0}): 4 qwords from 0x2681a0 = FLUSHE(0x1000006a),
  NOP, STCYCL(0x01000404), UNPACKR V4-32 num=17 (0x6c118000), ADDR
  (0x2681b0, = 0x8004 qwords = TMPDATA/0x80040 packet), STROW 0x4000
  (0x30004000), NOP, NOP.
- Packet format (from vucode_1.vsm): 17 qwords = tag qword (low 15 bits
  = vertex count, bit 15 = ADC) + 4 x {vertex, normal, color, st}.
  The program transforms + lights + clips, writes GIF output, XGKICKs.
- Setup chain: the static 106-qword CNT chain at 0x2681a0 (0x2681b0+)
  contains the MSCNT words (at towerBlockPtrs[i]-0xc) + params; the
  giftag block at 0x268150 + MSCAL at 0x268190 belongs to the same
  region.
- REFRACTION FOUND: vucode_1 START1 (texture path) computes
  st.x += VF13*VF31*Q, st.y -= VF13*VF31*Q (light factor x tex params
  x 1/w) - aap's own comment "some kind of env map effect" at
  vucode_1.vsm:186.  This is the tower/cube refraction trick.

IMPLEMENTED (commit follow-up)
------------------------------
- InitTowersFog now kicks the real upload sequence: sendDma(vudata)
  (MPG upload of vucode_1) + sendDma(chain2) (static setup chain) +
  vu1Wait each.
- vudataRelocate fixed: patches descriptor REF address words e[2]
  (param), e[6] (program), e[8] (param2).

REMAINING
---------
1. Build the 17-qword packet per tower at TMPDATA (tag + 4 x
   {vertex,normal,color,st}) - ports of tower_218318 + tower_2184d0.
2. The exact per-cell quad geometry (which 4 vertices, normals from
   the light matrix, colors from towerGrid/towerC/D, st from the UV
   quad) - DrawTowers inner block decode is ~80% done.
3. Verify the setup-chain kick is what the real init does (or whether
   the 23-qword static head chain runs first) - observable in pcsx2.

## 2026-08-21: corrected chain model (verified against PCSX2 source + real disasm)

DMAC tag semantics (PCSX2 Hw.cpp hwDmacSrcChainWithStack, which runs the real
BIOS):
- TAG_NEXT (ID 2): MADR = tag+16, TADR = the ADDR field.  In-place QWC
  qwords, next tag = ADDR field.  That's why tower_218288 rewrites
  0x268024 = 0x2681a0 every tower.
- TAG_CNT (ID 1): data in place after the tag (PCSX2 model), next tag after
  the data.
- TTE is set in D1_CHCR (325 = 0x145): each tag's ADDR field is transferred
  to VIF1 as 2 zero words + ADDR + 0 (4 NOPs).

Real per-tower chain (one kick at 0x268020):
  {NEXT 23, 0x2681a0}: STCYCL + UNPACKR V4-32 num=22 + 22 qwords params
      + MSCAL(0) + BASE  (368 bytes exactly)
  {CNT 106, 0} at 0x2681a0: 6 packet blocks: STCYCL + UNPACKR(17) + 17-qword
      packet {header, 4 verts, 4 normals, 4 colors, 4 st}
  END tag after the blocks.

tower_218180 copies THREE FULL 4x4 matrices (64 bytes each, 4x16B rows) into
the param window: spr+0x0c0 -> qwords 0-3 (A), spr+0x080 -> qwords 4-7 (B),
spr+0x180 -> qwords 12-15 (light matrix).  The VU program:
  MulMatrix(0,4,22): obj->screen = A x B (result column-major at qwords 22-25)
  VF05-08 = light matrix rows; dot(normal, row) x light colors (qwords 8-11)
  XGKICK(18) first: the init packet at qword 18 = the tower_218210-written
  words {0x8002, 0x10000000, 14, 0, 0x44, 0x80, 66, 0, 0, 0, 73, 0, 0, 0, 0, 0}
  VF31 = qword 21 = tex-mapping flag; 0 = START0, else START1 (refraction)
  START0/START1: XTOP wait -> packet -> 4 verts -> XGKICK(21) -> loop.

tower_218288 block patch words: {0x8004, 0x304e4000, 0, 0x412} at the 6 block
ptrs {0x26873c, 0x2682cc, 0x2683e8, 0x268504, 0x268620, 0x2681b0} - these are
the packet header qword 0 (data), not VIFcodes.  b[1] =
((a0<<6)|(a1<<7)|28)<<15 | 0x30004000 with a0=0, a1=1 at the call site.

0x218318 / 0x2184d0 patch per-tower data inside the packet: +0x10 (vertex),
+0x90 and +0xd0 (st coords).

The DMA wait helper 0x266b08 polls *(gp-31056) & 0x100 where the slot holds
0x10009000 = D1_CHCR (pointer table at 0x2965d0: D0..D9 channel regs).

The tower spr struct (ptr at gp-30968, alloc'd from SPR at runtime; IDB dump
predates allocation so contents unrecoverable):
  0x000 base/identity, 0x080 model (B), 0x0c0 view (A), 0x180 light matrix,
  0x200 light vectors (quad), 0x240 rotation matrix.

## 2026-08-21 (evening): tag decode + patch-word corrections (from the real disasm)

The previous session's analysis was truncated mid-comparison; re-derived
from image.bin disasm + PCSX2 source.  Three corrections to the model
above, one of which was an actual rendering bug in our port:

1. TTE semantics (PCSX2 Vif1_Dma.cpp): the tag's UPPER 8 BYTES (words
   2-3) are transferred to VIF1, not the ADDR field.  The head at
   0x268020 = a 4-word tag {NEXT|QWC=23, ADDR=0x2681a0, STCYCL, UNPACK
   22}.  TTE sends {STCYCL, UNPACK}, then 23 qwords from tag+16 =
   22 param qwords + MSCAL.  BASE/OFFSET/NOP at 0x268194/198/19c are
   dead static bytes (not part of the chain).

2. tower_218288's per-tower patch words (disasm of 0x218288):
   *(p+0)=0x8004, *(p+4)=((a0<<6)|(a1<<7)|28)<<15|0x30004000,
   *(p+8)=0x412, *(p+12)=0.  The call site (0x2189d0) passes a0=0,
   a1=1 -> word 1 = 0x304e4000.  OUR PORT HAD WORDS 2/3 SWAPPED
   ({0x8004, 0x304e4000, 0, 0x412}) -> the GS saw NREG=3 with
   regs {PRIM,PRIM,PRIM} and treated every output vertex qword as a
   PRIM write.  FIXED in commit 4dfed3e.

3. The 4 words are the packet's GIF tag qword 0 (not VIFcodes):
   0x8004       = NLOOP=4, EOP=1
   0x304e4000   = PRE=1, PRIM=0x9c (TRIFAN | ABE | FIX), FLG=PACKED,
                  NREG=3 (the 0x30000000 constant IS the NREG field)
   0x412, 0     = REGS {ST, RGBAQ, XYZF2} matching the VU output
                  layout (st@VI07, color@VI08, vertex@VI09, stride 3)
   The static ROM state = {0x8004, 0x302e6000, 0x412, 0} = PRIM=0x5c
   (FST instead of FIX) = the init-time call with a0=1, a1=0.

4. There is NO per-tower block selection: all 6 packet tags are
   rewritten every tower.  The 6 packets = 6 pre-built quads (the
   tower's side faces - their static vertices differ: {2,-2,-30},
   {2,2,+30}, {0,0,+30}, ...), each UNPACKed to VU1 mem and consumed
   by one XTOP/XGKICK cycle of vucode_1.  Block layout (71 words =
   0x11c bytes, matching the 6-param-block gap): MSCNT + STCYCL +
   UNPACK(17) + 17-qword packet; block 5 (first in the CNT region,
   0x2681a8) has no MSCNT.  MSCNT restarts the program without
   re-initializing (no second MSCAL per packet).

5. Per-tower call order (caller at 0x218940+, inside the DrawTowers
   walk at 0x2185c8):
     0x266b08  DMA wait (previous tower)
     0x218210  giftag(a0=0, a1=0x80_00000044)  - our patch is
               byte-exact (verified from disasm)
     0x218288  tags(a0=0, a1=1)
     0x218318  vertex z + colours (a0=row, a1=col*4, f12 = tower
               value from 0x3265d0[row*80+col*4]): per block b and
               vertex v: z = +/-f0 where f0 = 0x3275e8[row*36+col*4]
               (sign flags at 0x27ae50 + b*16 + v*4, a STATIC table);
               colours = {0,0,0,128} if f0<=0 else {s,s,s,128} with
               s = f12 (block 0) or f12 * *(gp-32484) (blocks 1-5),
               f12 scaled by (0x3273f0 int ? int/128 : float/30)
     0x2184d0  st coords (a0 = (r+5)/(c+1) + (r+4)/(c+3), integer
               divisions): unit quad {u,u}, {u+1,u}, {u,u+1},
               {u+1,u+1} with u = a0/256, z=1, w=0, into all 6
               packets at +0xd0
     0x218180  params
     then D1_QWC=0 (0x10009020), D1_TADR=0x268020, FlushCache,
     D1_CHCR=325
   The walk also reads per-tower floats from 0x326c10 (144 bytes/row)
   for the shadow walk and the rotation = sinf((frame%360-180)*k)*10*k
   with k = *(gp-32480).

REMAINING (updated)
-------------------
1. Port tower_218318 + tower_2184d0 and wire them to the stand-in
   field (towerC/D/A/B): needs the 0x3265d0/0x326c10/0x3273f0/
   0x3275e8 table semantics + the 0x27ae50 sign table + *(gp-32484).
2. The DrawTowers walk structure (0x2185c8, shadow walk + s0-s7 index
   math) - currently our DrawTowers only does the rotate/translate/
   transform + patch + kick.
3. The helpers 0x2676b0 (rot matrix) / 0x267860 (transform quad) -
   our sceVu0* stand-ins are plausibly equivalent.
4. Position generator (0x28a670 fill) + memcard tables.

## 2026-08-21 (later): transform convention settled from the real disasm

The VU program's MulMatrix uses the canonical VU dot-product idiom
(MULAx ACC, VF04, VF08 etc. = the 4-lane dot into ONE accumulator
lane - same idiom as the vertex transform, which must be a true M x v),
so it computes R = A x B^T: result row i = {A_i.B_0, A_i.B_1, ...}
i.e. R[i][j] = sum_k A[i][k] B[j][k] = (A x B^T)[i][j].

The VU then applies R x v = A x (B^T x v): the MODEL matrix is applied
TRANSPOSED = row-vector convention (p.R + t.w), then the view A is
applied column-wise.  Therefore:
- 0x2676b0 builds the model as rotation rows 0-2 + position added to
  ROW 3 (vadd.xyz into vf5 = rot row 3).  tower_218180 copies all
  three spr matrices verbatim (word-interleaved rows, no transpose):
  spr+0xc0 -> q0-3 (A = view), spr+0x80 -> q4-7 (B = model),
  spr+0x180 -> q12-15 (light rows).
- 0x267860 (light matrix builder) uses the vmulax.xyzw broadcast form:
  out = v.x*M0 + v.y*M1 + v.z*M2 + v.w*M3 = v x M (row-vector apply)
  for each of the 4 quad vectors: light_i = quad_i . rotated.
- The EE-side helpers 0x2676f8/0x267710/0x267728/0x267798/0x2677e0/
  0x267818/0x267840 = vector sub/add, row-shuffle (pextlw/pextuw/
  pcpyld/pcpyud = the TRANSPOSE helper!), normalize, dot, cross -
  the matrix toolkit of the walk.

BUG FIXED (dd79280): our model had the position in column 3 -> through
the transpose-apply the w channel became pos.p + w (perspective
garbage: big, scaling, flickering) and xyz stayed p.R at the origin
(rotation without translation).  Now row 3, matching 0x2676b0.

Verified static colours: light colours q8-11 = {1.0, 0.8, 0.8, 0.4},
packet colours 128.0, normals {0,0,1,1} -> output {179,179,179,128}
= the white-grey tower colour (observed white = correct).  Clip
vectors q16/17 = {0,0,0,1}/{4095,4095,0,65535}.

TODO: the light-matrix apply in our port is R x q (sceVu0ApplyMatrix
column convention) vs the real q . R (row convention) - identical for
the z-only sway rotation and z-normals, differs by rotation sign for
x/y lights once per-tower rotations become nontrivial.

## 2026-08-22: pipeline verified end-to-end (via the minimal test)

The "huge green rectangle" was the diagnostic marker itself: its main
quad was 8x24 world units at z=30 (~580x920 px), drawn opaque last,
in front of the tower.  Its colour = VU1 mem q21 = 0x00008004 = the
output buffer's GIF tag -> PROOF the VU program consumed the packets
and wrote its output.  Shrunk the marker.

With TOWER_MINIMAL_TEST (single tower, identity rotation, (0,0,40)):
the tower renders correctly - a 4x60 column with per-face lighting
(the 6 blocks carry distinct normals {0,0,1},{0,1,0},{0,-1,0},
{-1,0,0},{1,0,0},{0,0,1} -> dot factors -> bright 179 front, 102/51
sides, dark back; alpha 128 composites over the fog).  aap saw the
green->yellow->black gradient = the face shading through the fog
blend.

VERIFIED: upload chain (blob head CNT 115: NOP NOP MPG(229) - the
1832-byte program at micro addr 0), per-tower chain (TTE 4-word tag,
22-qword param window, MSCAL, CNT 106 = 6 blocks {MSCNT, STCYCL,
UNPACK(17), packet}), packet layout (all 6 packets land at VU1 mem
q0 - XTOP returns TOPS=0; output buffer q21-33), tag patch words
{0x8004, 0x304e4000, 0x412, 0}, row-3 model translation, A x B^T
transform, clip (xyw-tested, z via FTOI flags), output REGS
{ST, RGBAQ, XYZF2}.

Field loop re-enabled (wedge stand-in: y = (r-7)*0.8 + jitter).

REMAINING
1. Port 218318/2184d0 (per-tower vertex z = heights, colours, st) -
   the field currently shows static-height towers with the ROM's
   baked packet data.
2. The per-tower rotation formula: the doc's
   ((r+c+9)*(r+3)%(c+7))*0.8 vs the walk disasm's
   ((s4+3+s7+6)*(s4+3)/7 % 4)*0.8 + f23 - reconcile when porting the
   full walk (the index mapping s4/s7/s5 needs the whole 0x2185c8).
3. Draw order: real = towers -> fog (the fog blend assumes the
   towers' ALPHA_1 state); ours = towers last.  Cosmetic for now.
4. The VU program's first-packet overwrite race (packet UNPACK vs
   init reads of q0-16) is survived by the real timing; ours is the
   same bytes.

## 2026-08-22 (late): aap corrections - towers are textured, do not rotate

- PRIM re-decoded: 0x304e4000 >> 15 & 0x7ff = 0xa2 (not 0x9c):
  TME=1, TRISTRIP, CTXT=1.  The towers are TEXTURED tristrips in
  context 1 (the extra buffer); the texture = TEX0_1 (the fog
  texture in the real program).  The static init variant 0x302e6000
  = PRIM 0x68 (TRIANGLES, FST).  All earlier "PRIM 0x9c/0x5c" notes
  are wrong.
- The towers do NOT rotate per tower: the walk's angle term is a
  loop-invariant constant division ((144/7) mod 4 = 0).  towerAngleTab
  (the runtime memcard table) is the only rotation source and is
  zero in practice.  The rotating elements of the opening are the
  CUBES, not the towers.
- tower_2184d0 ported (st = unit quad at u = ((r+5)/(c+1) +
  (r+4)/(c+3))/256, integer divisions).

## 2026-08-22 (later): texture ground truth, CORRECTED

- PRIM decode (PCSX2 Gif_Unit.h HW_Gif_Tag: NLOOP 0-14, EOP 15,
  dummy 16-45, PRE 46, PRIM 47-57, FLG 58-59, NREG 60-63, REGS
  64-127; PRIM = (tagword1 >> 15) & 0x7ff): 0x304e4000 -> PRIM 0x9C
  = TRISTRIP(4), IIP=1, TME=1, ABE=0, AA1=0, FST=1, CTXT=0, FGE=0.
  FST=1: S,T are DIRECT texel coords (no Q divide).  CTXT=0:
  context 1.  (An earlier "0xA2" and a "FST=0/CTXT=1" decode were
  both bad arithmetic.)
- The towers sample TEX0_1/TEX1_1 (context 1), set by the walk:
  real 0x218728 vif1SetTextureMIP(&OpeningTexList[*(0x27aeb0)], 1,
  5, -65).  towerWallTexID = *(0x27aeb0) = 6 (STATIC, not 0) ->
  OpeningTexList[6] = resid 0x1c = RESID_TEXOWAL0: the 256x256
  mipmapped wall texture (mxl=2 -> mipmap=1/mmin=5 =
  LINEAR_MIP_LINEAR).  aap was right: the towers' texture IS
  TEXOWAL0, in ROM.
- OpeningTexList (IDB name, 0x278b40, stride 240) order matches
  osdbits textures[]: [0] TEXOSCE (resid 0x25, 256x64, fmt 5),
  [1] TEXOFOG0 (0x27, usage 2, 128x128, fmt 2/CT16), [2] TEXOFOG1,
  [3] TEXOFOG2, [4] TEXOFOG3, [5] TEXOFOG4, [6] TEXOWAL0 (0x1c,
  usage 1, 256x256, mxl 2, CT16), [7] TEXOCRLE, [8] TEXOCRBL,
  [9] TEXOFLAR, [10] TEXOREF.  aap's 2023 table was exact - the
  TEXOFOG0 "correction" (256x64/format 5) was WRONG and reverted.
- Real dataOffset = 20 for the fog/wall/flare/ref entries (matches
  aap's table).  The EXP payloads: 20-byte container header +
  payload (+4 spare).  The "24-byte header" claim was file-size
  arithmetic only; the real structs say 20.
- Real vif1SetTextureMIP writes only context 1: TEX1_1 (0x14),
  MIPTBP1_1 (0x34), MIPTBP2_1 (0x36), TEX0_1 (0x06).
- vif1SetFramebuffer's 5th arg (t0) = the clear flag (0/1), NOT a
  context selector; it always writes FRAME_1.
- Real DrawOpeningScene order: DrawTowers -> DrawExtraBuf2(1,2,80,128)
  -> DrawToExtraBuf2 -> DrawFog -> DrawLightsAndCubes -> 0x218b20.
  (towers first; extraBuf2 = frame-feedback/trail buffer composited
  before the fog.)
- InitTexture's expansion-path skip WAS dataOffset/4 words for all
  formats; for format 5 (1 output word per 2 raw bytes) the correct
  skip = dataOffset/2, for 3/4 (1 word per byte) = dataOffset.
  Currently moot (no expanded texture has dataOffset != 0) but fixed.
- The fb-self-reflection theory (TEX0_2 = TBP 0) was WRONG - it
  followed from the backwards FST/CTXT bits.  Scrapped.

## 2026-08-22 (Claude session): opening.c vs real disasm - found and fixed the ST bug

Independently re-disassembled expanded.bin (objdump -m mips:5900, VA base
0x200000, matches the "byte-identical to the IDB RAM segment" provenance
above) and checked opening.c's tower_2184d0/218318 ports against the raw
instructions, rather than trusting either this doc or the port.

**PRIM decode re-verified bit-by-bit (python, not head math): 0x304e4000
-> PRIM=0x9c -> TRISTRIP(4), IIP=1, TME=1, FGE=0, ABE=0, AA1=1, FST=0,
CTXT=0, FIX=0.**  The "FST=1: direct texel coords" claim in the
2026-08-22 (later) section above is WRONG - FST is bit 8 of PRIM
(0x9c = 0b1_0011_100, bit8=0) = **STQ mode (normalized S,T, scaled by
the texture's width/height)**, not UV/direct-texel mode.  This matters
for interpreting tower_2184d0's `u = a0/256`: it lines up with
TEXOWAL0's 256px width because S,T are fractions of the texture, not
raw texel addresses.  Doesn't change any packet bytes (PRIM is a
hardcoded constant already copied verbatim by towerPatchTags), just
corrects the doc's reasoning.

**tower_2184d0 disasm (0x2184d0-0x2185c4), confirms the doc's formula
exactly:** u = (float)a0 * (1.0/256.0); loop i=0..5 over
ptrTable[0x27a650][i] (== towerBlockPtrs), writes 4 vertices at
blockPtr+0xd0 = {u,u,1,0}, {u+1,u,1,0}, {u,u+1,1,0}, {u+1,u+1,1,0} -
i.e. the "+1" corner offset is a bare `+1.0`, NOT `+1.0/256`.

**BUG FOUND: opening.c's towerPatchST divided the +1 corner offsets by
256 AGAIN** (`u + st[v][0]/256.0f` instead of `u + st[v][0]`), which
collapses the whole quad's ST span to 1/256th of a texel - every
tower face samples a single near-fixed point on TEXOWAL0 instead of
the intended full-texture-width mapping.  This is almost certainly
what caused "no nice looking texture" - flat, undetailed faces rather
than a mapped wall texture.  **Fixed** (towerPatchST now emits
`u + st[v][0]` / `u + st[v][1]`, matching the disasm exactly).  Comments
in towerPatchST and DrawTowers updated to match (FST=0, and drop the
stale "PRIM 0xa2 / TEX0_1 fog texture" note in towerPatchST's header,
which predated the PRIM/texture corrections above).  Builds clean with
ee-gcc, no warnings.

**tower_218318 disasm (0x218318-0x2184cc) - does NOT match opening.c's
towerPatchVertices, needs a follow-up session:**
The z-sign-flag / color-write part (towerSignFlags table, blockPtr+0x18
per-vertex z, blockPtr+0x90 per-vertex colour, block-0 vs block-1..5
`s = f12` vs `s = f12*f3`, alpha always 128, `{0,0,0,128}` when
z<=0) matches opening.c EXACTLY, confirmed instruction-by-instruction.

But the computation of `f12` (the colour/height scale fed into that
part) is NOT what opening.c does.  Real code, before the per-block
loop:
```
v0 = *(int*)&towerA[row][col]        # 0x3273f0, RAW INT reinterpret
                                      # of the float bits (lw, not lwc1)
if (v0 != 0)                         # true for any legit towerA value
                                      # (towerA is always >= 3.0 per
                                      # InitTowersFog step 5 - a normal
                                      # float's bit pattern is nonzero)
    f12 = f12_in * ((float)v0 / 128.0)   # v0 cast int->float, NOT
                                          # reinterpreted back to the
                                          # original float value
else
    f12 = f12_in * (towerB[row][col] / 30.0)   # 0x3275e8
```
(addresses confirmed via exact addiu immediates: 0x73f0/0x75e8 off a
0x320000 lui, both row-stride 36 = the towerA/towerB arrays per the
DATA table up top, NOT towerC/towerD.)  f12_in is the grid value
argument (towerGrid[row][col], per the caller).

This is suspicious as transcribed - reinterpreting a legitimate
alpha float's bits as an int and dividing by 128 gives an enormous
nonsense colour scale, so either (a) towerA's memory gets overwritten
with a genuine small int for some other purpose between InitTowersFog
and DrawTowers's per-tower walk (no writer found yet), or (b) this
register (v0, whatever it's really reading) isn't towerA at all and
the address arithmetic needs re-checking against a wider slice of the
caller. Current opening.c stand-in
(`f0 = towerD*30; f12 = towerGrid*towerD`) does NOT reproduce this and
is a bigger source of "not quite right" shading/proportions than the
ST bug was.  NEXT SESSION: trace who else writes 0x3273f0/0x3275e8
(check the DrawTowers shadow-walk and OpeningInitTowersFog step 4/5
again, and whether InitTowersFog's step-5 loop runs AFTER or interleaved
with per-tower draws) before touching towerPatchVertices.

**Also unresolved: the caller's `a0` index passed into tower_2184d0
is NOT simply `(r+5)/(c+1)+(r+4)/(c+3)`.**  The call site (0x218a00-
0x218a38, inside the walk) computes it as
`s1*(row+5)/(s2+1) + s1*(row+4)/(s2+3)` - there's an extra `mult v1,s0,a1`
/`mult1 a0,s1,a0` multiply by a loop register before each division that
neither this doc's earlier note nor opening.c's towerPatchST accounts
for.  s0/s1/s2 weren't traced back to their definitions (that needs the
outer walk from 0x2185c8, same open item as "REMAINING #2" above) so it
isn't clear yet whether s1==col or something else, or whether s2==col+
some offset.  Lower priority than the two bugs above (it only shifts
*which* strip of TEXOWAL0 each tower samples, not whether texture shows
up at all), but towerPatchST's formula should be treated as unverified
until this is traced.

## 2026-08-23 (Claude session): DrawTowers walk (0x2185c8) traced - index mapping resolved, two more real bugs found

Full instruction-by-instruction trace of DrawTowers (0x2185c8-0x218b1c,
~340 insns), objdump -m mips:5900 on expanded.bin.  Register roles
confirmed by tracing every def back to its assignment (not guessed):

- **s4 = outer row loop (0..13), s7 = inner col loop (0..8)** - passed
  directly as tower_218318's (a0,a1) at the call site (0x2189f0/
  0x2189f4: `move a0,s4` / `move a1,s7`).  Confirms opening.c's (r,c)
  loop variables line up with the real row/col 1:1.
- **s0 = row+3** (`addiu s0,s4,3` @0x218804), **s2 = col+6**
  (`addiu s2,s7,6` @0x218814), **s1 = s0+s2 = row+col+9**
  (`addu s1,s0,s2` @0x218830/0x2188ac, same in both branches of the
  towerC==1.0 special case).  These three are callee-saved registers,
  untouched between their assignment and the 0x218a08 UV call site, so
  the chain holds across the ~500 bytes / helper calls in between.

**BUG FOUND #1 (index mapping resolved): tower_218318's grid sample is
`towerGrid[row+3][col+6]`, not `towerGrid[row][col]`.**  At 0x2189e0-
0x218a04: `v0 = s2*4 + s0*80 + 0x3265d0` (0x3265d0 = towerGrid base,
row-stride 80 = 20 ints, matching the DATA table's `towerGrid[20][20]`)
`; f12 = *(float*)v0` = `towerGrid[s0][s2]` = `towerGrid[row+3][col+6]`.
This explains why towerGrid is declared 20x20 while the tower field
itself is only 14x9: **the tower field is a 14x9 window into a larger
20x20 procedural heightfield, offset by (+3,+6)**, not the top-left
corner of it.  opening.c's DrawTowers gate (`towerGrid[r][c] <= 0`) and
towerPatchVertices's f12 sample were both reading the wrong corner of
the heightfield - fixed to `towerGrid[r+3][c+6]` in both places (the
towerC-driven special-case write `towerGrid[...] = -1.0` was moved to
the same shifted cell, since that's the one actually read back).

**BUG FOUND #2 (missing entirely): there IS a per-frame sway rotation,
just not a per-tower one.**  Before the r,c loop, DrawTowers computes
(0x218730-0x21876c):
```
f12 = (float)((frameCount % 360) - 180)
f12 = f12 * DEG2RAD              # DEG2RAD = *(gp-32480) = 0x2a7190
f0  = sinf(f12) * 10.0
f23 = f0 * DEG2RAD
```
`f23` is loop-invariant across every tower drawn that frame (computed
once, reused for all r,c) and gets added into the Z-axis euler angle
(`angles[2]`) inside the per-cell rotation build.  This directly
contradicts the "2026-08-22 (later): towers do NOT rotate" note above
- that note was checking a genuinely loop-invariant *per-cell extra*
term (see below) and over-generalized to "no rotation at all".  Ported
as `towerSway` in opening.c (computed once per DrawTowers() call,
added to `angles[2]`) - gives the towers the gentle collective sway the
real animation has, which opening.c had completely lacked (angles were
always all-zero).

**Still NOT ported: the per-cell additive term to angles[2].**  Same
code region, two branches selected by `towerC[row][0] == 1.0`
(`s8 = 0x3277e0 + row*56`, i.e. towerC's real address/stride, read
BEFORE any column offset is added - so this branch check itself is
per-row, not per-cell, at the point it's read; s8 does get +4/col
before the *next* row via the loop increment, so by the next glance
it'd be towerC[row][col], but the read for THIS row's branch decision
happens at column 0). Both branches compute the identical
`s1*(s0+row_off)` / 7 (rounded up to a multiple of 4) * PI/2 term
(`(row+col+9)*(row+3+{4,5}) / 7`, rounded, times `*(gp-32476)` = PI/2)
but combine it with f23 differently (traced far enough to see one
branch does `f23 + term` before merging, the other merges via a
separate `f1` whose provenance I didn't finish tracing).  NOT ported -
risk of getting the merge wrong felt higher than the payoff (a small
per-tower angle offset) given the sway alone was the bigger visible
gap.  NEXT SESSION: finish tracing `f1` in the 2188ac-218920 range to
either port this cleanly or confirm it's genuinely negligible.

**RESOLVED 2026-08-25 (was: "left as an open question, not implemented"
- see below): the shadow-ceiling gate is a real, provable no-op.** aap
asked to finally check this out properly. Full register-by-register trace
of loop1 (0x218650-0x2186f4), resolving the "same two floats for all 126
cells" mystery completely:
- `sceVu0SubVector(dst, a1, a2)` (0x2676f8, called once per cell, dst
  growing by 32 bytes/cell across the WHOLE double loop - confirms the
  "destination does increment" observation): `a1` = `&towerPos[row][col]`
  (traced via `s1`, which walks `towerPos`'s real base 0x326c10 row-by-row
  then col-by-col, matching its 144-byte/36-byte strides exactly); `a2` =
  `0x279f30` = the scene-vectors table's FIRST entry = camera `position`
  (confirmed against the already-known `0x279f30` address from earlier
  sessions). So this call computes, per cell: **displacement = towerPos -
  cameraPosition** - genuinely per-cell, not constant. Makes sense as a
  real "how far is this tower from the camera" computation.
- BUT the gate's OWN input, `*(ptrGlobal+32)`/`*(ptrGlobal+36)` where
  `ptrGlobal = *(gp-30996)`, is a **fixed pointer variable, never
  reassigned anywhere in this function** - so every read of it returns the
  SAME base address, and `+32`/`+36` land on the FIRST cell's result
  specifically (cell (0,0), the very first `sceVu0SubVector` output written
  in the whole loop) - not "this cell's" result. Every one of the 126
  gate checks reads cell (0,0)'s displacement, never its own cell's.
- The gate value itself: `f1 = |dx| + |dy|` (re-derived via the branch-
  likely idiom at 0x2186a8, confirming the earlier session's formula was
  right) using cell (0,0)'s displacement - a sum of two absolute values,
  so **mathematically guaranteed non-negative**, zero only in the
  measure-zero case where the camera sits at cell (0,0)'s exact x,y
  position (never happens on the real orbiting camera path).
- The gate check downstream is `if(!(0 < storedValue)) skip`. Since
  `storedValue` is always the same non-negative constant for every cell,
  **this gate never skips anything, ever, for any real camera position**.
  It's genuine ROM behavior (not a decompiler artifact or a misread), it's
  just provably inert for rendering purposes - same character as the
  angles[2] "null-result proof" from earlier this project.
**Conclusion: intentionally NOT porting this is correct, not a
compromise** - it would be real, verified-inert code if added. Nothing
further to do here.

## 2026-08-25 (same session, continued): field randomization, and towers called "as finished as reasonable" for now

aap: pushed back on the `DrawExtraBuf2` theory for the brightness falloff
(no strong evidence pins it there specifically - noted, left alone, not
investigated further this session). Deferred the real memcard parse for
now, but asked for the fixed 21-tower field to be randomized "with
reasonable values" instead of always showing the exact same field, plus
the shadow-ceiling gate write-up above, with the goal of getting towers
as complete as reasonable before moving on to cubes.

**`RandomizeTowerField()` replaces `LoadCapturedTowerField()`/
`capturedTowers[]`** as what `InitTowersFog` actually calls. Not a fresh
guess - fit from the same real 21-cell captured field `LoadCapturedTowerField`
used (that raw table is no longer kept live in `opening.c`, but is fully
preserved in git history and this doc's earlier "2026-08-23" section):
density (21/126 ≈ 1-in-6), the towerC distribution (18/21 `0.4`, one each
of `0.6`/`0.8`/`1.0`), the towerC==1.0 ↔ towerD==0.3 pairing, and a linear
position fit against (row,col) - `y = -5.2*c + 21.0048` matches the real
data to float precision, `x = 5.184*r - 35.598` to within ~1 unit (some
residual, likely scene rotation at the capture moment, not modeled). The
~5.2 pitch lines up with `HeightGrid()`'s own 5.1-unit spacing, which is a
good sanity check that this is the right neighborhood. z: 17/21 real
cells sit at one exact baseline value, the other 4 scattered with no
r,c-discernible pattern from just 4 samples - modeled as an occasional
random offset rather than invented as a formula. Builds clean.

**Towers status after this session's work**: the core rendering pipeline
(geometry, texture, VU1 chain) is disasm-verified AND now byte-exact
against the real ROM, including the microcode. The shadow-ceiling gate is
resolved (confirmed inert, correctly left out). The field is randomized
with real-data-derived statistics rather than static. What's left is
exactly the deliberately-deferred memcard history parse (and, separately,
the `vif1SetClamp` call and cubes themselves, which belong to
`DrawLightsAndCubes`/`DrawCube`, not `DrawTowers`/`InitTowersFog`) - aap's
stated next task.
Did not implement this gate in opening.c - not confident enough in the
read to be worth the risk of hiding towers that should render.

## 2026-08-23 (later): aap supplied a live PCSX2 savestate - ground truth, not guesses

aap captured `20020207-164243 (00000000).01.p2s` mid-animation on real
OSDSYS.  This is a full PCSX2 savestate zip: `eeMemory.bin` (32MB EE
RAM, VAs <0x2000000 map 1:1 to file offset), `vu1Memory.bin`/
`vu1MicroMem.bin` (VU1 data/code memory), `GS.bin`, and `Screenshot.png`.
Used it to check our reconstruction against REAL runtime state instead
of static-ROM disassembly + pseudocode - several things confirmed,
one real correction found.

**The screenshot**: a field of ~20 similar light-grey TEXTURED boxes
(visible wall-texture grain) in blue fog, 3 light trails (coloured
lines with glowing endpoint dots) converging near center, and ONE
visually distinct dark pyramid/diamond-shaped object that the trails
point at.  No tall thin "tower" pillars anywhere - the mental model of
"towers" here should be "the boxes", not literal towers.

**Confirmed our port is the right code, byte-for-byte where it
matters**: `*(int*)0x27aeb0` (towerWallTexID) = 6 = TEXOWAL0, live,
matching `TEXID_WAL0`.  `vu1MicroMem.bin`'s first 1832 bytes are
byte-identical to `vucode_1` (the only program resident in VU1 code
mem at capture time).  `vu1Memory.bin` qword 18-20 (the giftag block)
= `{0x8002, 0x10000000, 14, 0, 0x44, 0x80, 66, 0, 0, 0, 73, 0}` -
byte-for-byte what `towerPatchGifTag()` already writes.  So
`OpeningInitTowersFog`/`DrawTowers` (what we've been calling "towers")
is, with very high confidence, what's drawing everything in that
screenshot - it is not a mix-up with some other function.

**`DrawLightsAndCubes` (0x217db8) is NOT the box renderer.**  Checked
it for the first time: it's a ~30-instruction dispatcher (one call to
0x2166c8, one to `sub_212bd0(1,1,0,0,0,0)`, then a 5-iteration loop
calling `0x217520(sp, i)` for i=0..4) with no texture/VU-program setup
of its own.  5 iterations matches `OpeningInitLightsCubes`'s 5-entry
table (`0x28a210`, idx=s0%5) and the screenshot's handful of light
trails.  This is almost certainly the light-trail system, not cube
geometry - "Cubes" in its aap-given 2023 name is likely either a
misnomer or refers to tiny point-sprite markers at the trail ends, not
the big textured boxes.

**towerFlags/towerC/towerD/towerPos extracted directly from
`eeMemory.bin`**: 21 of 126 field cells were flagged (matches the 21-
entry boot history exactly).  20 of them are near-identical
(`C`=0.4-0.8, `D`=0.1).  Exactly ONE (row 12, col 0) is a clean 3x
outlier: `C=1.0, D=0.3` - this is the `towerC==1.0` "special path"
cell found in the DrawTowers walk trace above, and per that trace, its
rotation-angle computation SKIPS the shared sway term (`f23`) that the
other 20 cells' branch includes.  20 uniform swaying cells + 1 unique
non-swaying 3x-scaled cell lines up strikingly well with the one
visually distinct object in the screenshot.  Best current hypothesis
for aap's "towers vs cubes" memory: **it's not two draw functions, the
distinction lives inside DrawTowers as this per-cell branch** - the
"cube" look (many, uniform, swaying, presumably refracting via
vucode_1's START1 path) vs the "tower" look (singular/rare, static,
presumably plain-textured via START0) may just be different rows of
the SAME memcard-driven field.  Not proven - still haven't traced what
sets vucode_1's START0/START1 selector per-cell (qword 21 of the
per-tower param window; the doc's earlier "VF31 = qword 21" note may
itself need re-checking, since the OUTPUT buffer's q21 was observed to
just be a copy of the packet's input GIF tag, not obviously a flag -
see the raw dump below).

**CORRECTION to the towerA/B/C/D address mapping**: re-derived
empirically by testing InitTowersFog step 5's own formula
(`towerA=clamp(D*30,3)`, `towerB=(C>=1)?0:(int)((1-C)*128)`) against
the captured values at all 4 candidate addresses, for all 21 real
cells - exact match, zero error, when read as:
  - `0x3273f0` (stride 36) = **towerB**, and it is genuinely stored as
    a raw INT (`lw` matches `int((1-C)*128)` exactly; reading the same
    bytes as a float, expecting `swc1`-stored data, gives 0.0 for
    every single cell - confirms `tower_218318`'s `lw` load of this
    address from the earlier session was reading it correctly as an
    int, and that opening.c's `float towerB[14][9]` declaration is a
    latent type mismatch, currently harmless since nothing reads
    towerB back in our port yet).
  - `0x3275e8` (stride 36) = **towerA**, a float, `clamp(D*30, 3)`.
  - `0x3277e0` (stride 56) = **towerC**, a float (0.4-1.0 range here).
  - `0x327af0` (stride 56) = **towerD**, a float (0.1/0.3 here).
This matches the ORIGINAL (pre-2026-08-23) address/name table at the
top of this doc exactly - the mid-session confusion in the
"tower_218318 disasm" section (which called the int-test address
"towerA" and the /30 fallback address "towerB", i.e. swapped) was
wrong and is superseded by this empirical check.  `tower_218318`'s
real f12-scale formula (still not ported into `towerPatchVertices`) is
therefore: `f12 = f12_in * ((int)towerB[row][col]/128.0)` when
`towerB!=0`, else `f12_in * (towerA[row][col]/30.0)` - both branches
now read as sensible 0-1-ish scale factors, not the "astronomical
garbage" the previous session flagged as suspicious (that suspicion
was the swapped-label bug, not a real problem with the code).

**IMPLEMENTED**: `RandomizeTowerField()` replaced with
`LoadCapturedTowerField()` in opening.c - a literal table of the 21
real (row, col, C, D, pos) values from the savestate (pos.z stored as
the PRE-step-5 base, so InitTowersFog's existing step 5 loop
reproduces the captured end state exactly rather than double-applying
its `+= C*30-A` adjustment).  towerA/towerB are intentionally NOT set
directly - step 5 already derives them from D/C, matching the real
formula now confirmed above.  Builds clean.

**Per aap**: tower height should track how many times a given game
has been booted (a play-count signal from the boot history), so
`towerD` is presumably derived from a per-game boot count.  This
savestate's D values (mostly 0.1, one 0.3) are consistent with "one
game played ~3x more than the others" but don't by themselves prove
the exact history->D formula - that's still `TOWER_FIELD_FROM_HISTORY`'s
open TODO (parsing `bootHistory`/`0x1f0138` and the still-untraced
`0x28a260` cell table + `0x28aee0`/`0x28af18` float tables).  Getting a
SECOND savestate from a memory card with more/different play counts
would help confirm the D-vs-playcount relationship directly.

## 2026-08-23 (later still): draw-order regression once the real field actually renders

Once towers were rendering ~20 real, solidly-visible boxes (instead of
a mostly-offscreen random field), aap reported fog gone and lights
"disappearing behind blocks."  Not a Z-test bug: `DrawOpeningScene`
had towers drawn LAST (`DrawFog(); DrawLightsAndCubes(); DrawTowers();`)
on a documented theory that fog's six layers would otherwise render
towers invisible without the (unported) extra-buffer compositing.
Checked fog's actual blend: `pktSetAlphaBlend(1,0,20)` ->
`BlendModes[0]={a=0,b=2,c=2,d=1}` -> GS ALPHA eqn `Cd_new=((A-B)*C)/128
+D` = `Cs*20/128 + Cd_old` - **additive**, not replace/opacity.
Additive blending can only brighten what's underneath, never erase
it, so the "towers become invisible" concern doesn't apply.  With
towers drawn last instead, real (now-populated) opaque tower quads
were simply painting over fog/light-glow pixels drawn earlier the
same frame - draw order, not depth.  Reordered to `DrawTowers();
DrawFog(); DrawLightsAndCubes();` (matches the real game's order per
the DATA/FUNCTION notes above, minus the still-unported
DrawExtraBuf2/DrawToExtraBuf2 step).  Also gave `DrawFog` its own
explicit `vif1SetZTest(0)` (it previously only reset ZWrite, silently
inheriting ZTest from whatever ran before it - harmless cross-frame
leakage before, but worth being explicit now that towers write
Z solidly across a real depth range that overlaps fog's fixed Z~109-134
plane).  Not yet visually re-verified (no PCSX2 here) - aap to test.

## 2026-08-23 (later still): RESOLVED - the real field renders, same positions as the original

Full regression arc, bisected live with aap against a real PCSX2 build
(no emulator available in this environment - every step below was
tested by aap, not simulated):

1. Draw order (`DrawTowers(); DrawFog(); DrawLightsAndCubes();` instead
   of towers-last) - real, and correct per the doc's FUNCTION notes,
   but NOT the cause of the black-screen regression on its own
   (aap: "looks like before" after this alone).
2. Bisected via independent kill switches (`TOWER_UPLOAD_VU1`/
   `TOWER_LOAD_FIELD`/`TOWER_DRAW`, still in the code as debugging
   scaffolding): fog+lights alone (`TOWER_UPLOAD_VU1` only) rendered
   fine - confirmed the regression was in tower INIT, not the
   per-frame draw call, and not the VU1 upload/`vudataRelocate` (both
   unchanged code).
3. `TOWER_DRAW_LIMIT` (cap on how many real cells reach the pipeline
   per frame, also left in the code): capped to 1 real cell = scene
   drew again but the tower itself was invisible.  Capped to 0 = same
   PCSX2 log line ("Gif Unit - GS packet size exceeded VU memory
   size!") as with 1 - proved that warning comes from the one-time
   VU1 upload/setup-chain kick (unchanged code, present even with
   zero per-tower kicks), NOT from per-tower data.  Red herring for
   the visibility question, ruled out definitively rather than assumed.
4. ACTUAL BUG: `towerPatchVertices`'s colour-scale approximation used
   `towerGrid[r+3][c+6] * towerD[r][c]` (≈10% brightness for the
   captured field's D=0.1 cells) instead of the REAL formula already
   derived and written up above (`towerGrid[r+3][c+6] *
   (towerB!=0 ? towerB/128.0 : towerA/30.0)`, ≈59% brightness for most
   of the captured cells) - towers were rendering but too dim to see
   against the fog.  Fixed: `towerPatchVertices` now uses
   `towerA[r][c]` for the height (f0, was already numerically
   equivalent to the old `towerD*30` for these cells, so no visible
   change there) and the real `towerB`/`towerA`-based scale for f12.
5. With the brightness fix, testing cell (0,0) alone (`TOWER_DRAW_
   LIMIT 1`) still showed nothing - (0,0) has one of the largest Y
   offsets in the captured table (21.0, vs most cells near 0) and was
   suspected to be projecting outside the visible window on its own.
   Uncapped (`TOWER_DRAW_LIMIT -1`, all 21 real cells): **towers
   render, in the same positions as aap remembers from the real
   OSDSYS.**  Confirms LoadCapturedTowerField's position table, the
   camera reconstruction (InitOpeningScene/Process), and the towerGrid
   [r+3][c+6] windowing are all correct.

Remaining, per aap: "lighting isn't right yet" - next thing to chase.
Candidates already flagged as unported/approximate in this doc: the
per-cell PI/2 rotation term (angles[2]'s non-sway component), the
"shadow ceiling" gate (loop1, not implemented), and the light-matrix/
normal shading in `towerStruct.quad` (verify against the real per-
face dot-product values this doc records under "2026-08-22: pipeline
verified end-to-end").

**Not yet checked**: `GS.bin` (the GS privileged registers + VRAM,
4MB in the zip) - would let us confirm the live `TEX0_1` register's
texture base pointer against TEXOWAL0's expected TBP, and potentially
inspect VRAM directly for the wall texture pixels.  Also haven't taken
a second savestate a few frames apart to directly observe which cells
move/sway frame-to-frame, which would be the cleanest possible
confirmation of the sway-branch hypothesis above.

## 2026-08-24: per-cell angles[2] term ported - lighting variety fix

Picked up the "NEXT SESSION" item from 2026-08-23: finished tracing
`f1`'s provenance in DrawTowers 0x2188ac-0x218920 (both towerC
branches), objdump -m mips:5900 on expanded.bin, instruction by
instruction. Resolves the doc's earlier "{4,5}" row-offset ambiguity
and the open question of what the two branches merge into `angles[2]`.

**Both branches (towerC==1.0 special cell and the normal ~20 cells)
compute the IDENTICAL per-cell term**, `s0` never gets an extra +4/+5
- that earlier note was a guess made before the direct trace existed:
```
s0 = row+3, s2 = col+6, s1 = s0+s2 = row+col+9      (both branches)
a0 = s1*s0 = (row+col+9)*(row+3)                     (mult @0x218838/0x2188b4)
q  = a0 / 7                                          (div, s5=7; both branches)
idx = q mod 4    (sra 2/sll 2/subu pattern - a no-op
                   round-toward-zero adjustment here since
                   all operands are non-negative for every
                   real (row,col))
term = idx * PI_2                                    (mul.s f0,f0,f22; f22 = *(gp-32476) = 1.5707964)
```
The two branches differ only in how `term` merges with the shared sway
(`f23`) at the 0x218920 join point:
  - normal path (towerC != 1.0): delay-slot `f0 = f23 + f0` before the
    branch to the join, so final = `table_val + towerSway + term`.
  - towerC==1.0 special path: falls through with no `f23` add, so
    final = `table_val + term` (no sway) - matches this doc's earlier
    savestate-derived guess ("its rotation-angle computation SKIPS the
    shared sway term") exactly, now confirmed at the instruction level.
`table_val` = `*(float*)0x27aed8`, one entry of the 4-float
`towerAngleTab` (0x27aed0/0x27aed4/0x27aed8/0x27aedc, read with a
`+f21` that is provably 0.0 throughout this code region - f21 is set
to 1.0 only inside the earlier, still-unported shadow-ceiling walk
(loop1, 0x218628) and reset to 0.0 at 0x218720 before the per-cell
loop starts, never touched again until the function epilogue restores
the caller's value). Read all 4 floats directly from expanded.bin:
all zero. So `towerAngleTab` contributes nothing - confirms opening.c's
existing `{0,0,0,0}` table was already correct, and the entire visible
per-cell rotation comes from `term` alone.

**Effect**: `term` is a quantized 90-degree step (`idx` cycles 0-3 as
(row,col) vary), so each cell's tower gets its own fixed 0/90/180/270-
degree yaw baked in on top of the shared sway. Since the light matrix
(`towerStruct.transformed`, uploaded to VU1 qwords 12-15) is the light
vectors rotated by this SAME per-tower rotation matrix, this term is
what makes different cells show their brightest face in different
apparent directions - without it (opening.c's prior state), every
tower shared the exact same facing and looked uniformly lit, which is
the concrete mechanism behind aap's "lighting isn't right" report.

**IMPLEMENTED** in `DrawTowers`: `term = ((r+c+9)*(r+3)/7 % 4) * PI_2`
computed per cell, folded into `angles[2]` alongside the existing
`towerSway`, with the towerC==1.0 sway-skip. Builds clean. Not yet
visually re-verified against a real PCSX2 build (no emulator here) -
aap to test.

Still open, unchanged from 2026-08-23: the shadow-ceiling gate (loop1)
and the memcard boot-history parsing path (`TOWER_FIELD_FROM_HISTORY`).

## 2026-08-24 (later): light/cube subsystem - real positions traced, wired in

aap: towers now look right, but "the original lighting is concentrated
on the center, ours is more spread out" and "the initial positions of
the 4 lights are different". This is a DIFFERENT subsystem from the
tower-face lighting fixed earlier today - the light TRAILS/glow orbs
(opening.c's `DrawLights`/`InitLightsCubes`, called from
`DrawLightsAndCubes`). Unlike the tower code, this subsystem had never
been disasm-traced at all: the existing code was pure invention from
an earlier session (`// TODO`, `// unused, perhaps earlier formula?`,
`// weird setting...`, `// BUG: this isn't updated in original` are
all still literally in the code - a tell that nobody had checked it
against ROM yet).

**Function map (objdump -m mips:5900 on expanded.bin, address-boundary
method: every `addiu sp,sp,-N` / `jr ra` pair in 0x215fd0-0x217e30),
first hard data this project has had on this subsystem**:
```
0x215fd0  (144-byte frame) - unidentified, ends 0x2166c0
0x2166c8  (272-byte frame) - unidentified, ends 0x216f80 - called ONCE
          (unconditionally) by DrawLightsAndCubes, before the per-
          instance loop. NOT traced this session (real "DrawLights"
          equivalent almost certainly lives here or nearby - the
          0x217520 function below turned out to be per-INSTANCE
          state/positioning, not the trail-line renderer opening.c's
          DrawLights() also contains).
0x216f88  (112-byte frame) - the function-pointer OpeningInitLights-
          Cubes stores to gp-30824 (0x2a7808, corrected from an
          earlier doc note's 0x2a77f8 - re-derived this session with
          gp=0x2af070, itself cross-checked two independent ways via
          the already-known DEG2RAD/PI_2 pair). NOT traced.
0x217524  ("0x217520", 2624-byte frame) - DrawLightsAndCubes's per-
          instance worker, called 5x with (sp, i) for i=0..4. Mostly
          GS packet TEMPLATE construction (the entire 2624-byte frame
          is scratch for that, not per-instance state) + one call to
          0x21bf88(sp, 0x27b3f0, i) whose return value gates a skip-
          the-rest branch (clip/visibility test, structurally like our
          own sceVu0ClipAll early-out) - the ACTUAL per-frame position
          update almost certainly happens inside 0x21bf88, NOT traced
          this session.
0x217ab8  ("0x217ab0" per the original survey note) - OpeningInit-
          LightsCubes, matches the original ~194-insn estimate closely
          (186 measured). FULLY traced this session (see below).
0x217dbc  ("0x217db8") - DrawLightsAndCubes itself: genuinely the
          "~30-instruction dispatcher" the original survey said (the
          1200-byte frame is just a struct passed by address to the
          calls it makes, not local computation) - CONFIRMED, matches
          the original note exactly: one call to 0x2166c8, one to
          0x212bd0(1,1,0,0,0,0), then `for(i=0;i<5;i++) 0x217520(sp,i)`.
```

**OpeningInitLightsCubes, first loop (0x217b68-0x217c54, i=0..4) fully
decoded** (manually simulated instruction-by-instruction, not just
skimmed - the div-by-zero guard idioms and register reuse made this
error-prone enough that a naive read would likely have gotten it
wrong): reads a REAL, ROM-baked (not random) 5-entry seed table at
0x27a210 (16 bytes/entry, 3 floats + pad), and derives two further
per-instance arrays:
```
d = (i-2) * 0.8, except d = 0.9 when i==2 (avoids the div-by-zero
    that (i-2)==0 would otherwise cause below)
anchor[i]  (-> 0x27b0f0) = { seed[i].x*3.5, seed[i].y*3.5,
                              seed[i].z*-15.0+150.0, 0 }
rate[i]    (-> 0x27b190) = { 0.0031/d, d*0.0022, d/1000.0+0.0013, 0 }
```
A third array (-> 0x27b140, "outB" in this session's working notes) is
ALSO written by this loop (as `{v,v,v,v}`, v = d*(i%3)*3.7 + 0.2856)
but does NOT match that simple form when read back from a live
savestate - see below, it's evidently live per-frame state, not a
fixed init value.

**VERIFIED EXACTLY against the live PCSX2 savestate**
(`20020207-164243 (00000000).01.p2s`, python3.14's zipfile reads its
zstd-compressed members directly - no external tool needed):
`eeMemory.bin` @0x27b0f0 (anchor) and @0x27b190 (rate) match this
derivation to float precision, all 5 instances, every component - not
approximately, exactly. This is the strongest possible confirmation
short of a source diff. @0x27b140 ("outB") does NOT match the simple
`{v,v,v,v}` init-time form when read from the savestate - its live
values are multi-radian-scale and vary per component, consistent with
an angle that's been integrated frame-by-frame since init (plausibly
using `rate` or something close to it) inside the untraced 0x21bf88,
not a static value. A separate array at 0x27b3f0 (referenced by the
0x21bf88 call in the per-instance worker) reads as 5 IDENTICAL
{112,112,152,128} entries followed by zero padding - confirms exactly
5 real instances (not padded further), but is otherwise probably a
shared clip/viewport constant, not per-instance data.

**Root cause of "concentrated vs spread out"**: opening.c's prior
`DrawLights` fabricated an orbit with amplitude growing per light index
(`(10.0f-l)*cos(...)`, `(3.0f+l)*sin(...)`, up to +-10 units) around a
shared center. The REAL anchors are 5 fixed, scattered points with
magnitude ~3-13 units each (not radius-increasing, not orbiting a
shared center) - e.g. anchor[0]=(12.5,1.9,111.1), anchor[3]=
(-13.1,-8.3,84.5). Any procedural sway on top of these real anchors is
inherently much more localized than the old formula's wide shared
orbit, which is exactly aap's "spread out" report.

**Root cause of "initial positions differ" (two independent causes,
both fixed)**: (1) the position formula above never used the real
per-instance anchors at all - fixed by wiring in the real table.
(2) `lightsSeed = rand()/2345 + 3456` used DIVISION; the real code
(0x217d80-0x217d88: `div zero,v0,v1; mfhi a0`) uses the REMAINDER, i.e.
`rand() % 2345 + 3456` - confirmed against the savestate too (its live
seed slot at gp-28892/0x2a7f94 reads 4002, squarely inside the real
formula's 3456-5800 band; a division would produce something far
outside that band for any rand() with an appreciable range). This
seed feeds the whole animation's phase from frame 0, so getting the
operator wrong desynchronized everything downstream even before the
position-formula bug.

**Also confirmed: 5 real light/cube instances, not 4** - opening.c's
arrays and loop bounds were all `4`; bumped to `NUM_LIGHTS 5`
throughout (`lightPositions`, `lightMatrixHistory`, `lightTrailVerts`,
both loops in `DrawLights`, the init loop in `InitLightsCubes`).
`lightColors[]` still has only the 4 previously-identified colours
(not re-verified this session) - cycled `%4` for the 5th instance
rather than guessing a new one.

**IMPLEMENTED**: `lightSeedTable`/`lightAnchor`/`lightRate` added,
computed in `InitLightsCubes` per the verified formula above;
`DrawLights`'s position line replaced with `lightAnchor[l] + (small
placeholder sway)`; `lightsSeed` fixed to `%`; light count bumped to
5. Builds clean.

**NOT verified / still open, clearly flagged in the code**: the exact
per-frame trajectory around each anchor (what 0x21bf88 actually
computes from `rate`/"outB") - the current placeholder sway (unit-
radius circle, frequency = `rate[l].y`) is a deliberately modest,
clearly-commented stand-in, not a disasm-ported formula. Also
untraced: 0x2166c8 (called once per frame before the instance loop -
likely the actual trail-line "DrawLights", separate from the 5
instances), 0x216f88 (the stored function-pointer callback), and
0x21bf88 itself. Next session, if aap confirms the anchor positions
now look right: trace 0x21bf88 to replace the placeholder sway with
the real trajectory, and/or trace 0x2166c8 to check whether it's a
separate "4 real trail lines" system distinct from these 5 instances
(aap's phrasing "the 4 lights" may refer to that function, not this
one - not disambiguated yet, only inferred from the instance-count
match being circumstantial).

## 2026-08-24 (later still): terminology correction + orb-light amplitude + a null-result proof for the tower shading term

aap clarified: "the 4 lights" in all of today's reports = the orbiting
glow particles (`DrawLights`/`InitLightsCubes`, the section above), NOT
the tower face shading. Both were independently reported wrong, but
they're unrelated subsystems - don't conflate them again.

**Orb-lights regression**: after wiring in the real anchor positions,
aap reported the orbs "not moving anymore." The placeholder sway
added alongside the real anchors used radius 1.0 - almost certainly
just too small to perceive against the scene's scale (anchors
themselves are ~3-13 units apart; towers/camera work in the tens-to-
hundreds range), even though it WAS animating every frame (phase
still advances via frameCount * lightRate). Bumped to `SWAY_RADIUS
3.0f` - still far short of the old fabricated orbit's ~9-10 unit
amplitude, so it should stay "concentrated," but should now actually
read as moving. This constant is a tuning knob, not a verified value -
told aap to report back if still wrong in either direction.

**Tower shading: the per-cell angles[2] term is very likely a genuine
visual no-op for this geometry, not a broken port.** aap reported the
tower shading "still looks exactly like before" after the earlier
angles[2] fix. Worked out why, and it's NOT the port being wrong:

1. Disassembled the ACTUAL linked `sceVu0ApplyMatrix` from
   `/usr/local/sce/ee/lib/libvu0.a` (objdump -m mips:5900) rather than
   trusting the naming convention - this resolves the long-standing
   "TODO: the light-matrix apply in our port is R x q (column
   convention) vs the real q . R (row convention)" note from
   2026-08-21 (## transform convention settled), which turns out to
   have been an unverified assumption, not a checked fact:
   ```
   vmulax.xyzw  ACC, vf4, vf8x     ; vf4 = M row 0
   vmadday.xyzw ACC, vf5, vf8y     ; vf5 = M row 1
   vmaddaz.xyzw ACC, vf6, vf8z     ; vf6 = M row 2
   vmaddw.xyzw  vf9, vf7, vf8w     ; vf7 = M row 3
   ```
   dst = v.x*M[0] + v.y*M[1] + v.z*M[2] + v.w*M[3] = v x M, the exact
   row-vector convention the real 0x267860 walk trace already
   documented. So `sceVu0ApplyMatrix(transformed[k], rotated, quad[k])`
   IS the right call, with the right operand order - **this TODO is
   RESOLVED, no bug here.** (Worth remembering: that old note was
   guessed from the function's name/signature shape, never actually
   disassembled - a reminder to verify library calls the same way as
   ROM code in this codebase, not just ROM code.)
2. Given that, dot(local_normal, transformed_row) = dot(local_normal,
   quad_row x R) = dot(R x local_normal, quad_row) (identity for
   orthogonal R: dot(a, R^T b) = dot(R a, b)) - i.e. the shading result
   is mathematically equivalent to rotating the FIXED local normal
   forward by R and dotting it against the fixed light vectors. Since
   the vertex/geometry transform ALSO effectively applies forward
   rotation R (the already-verified, working model-matrix pipeline),
   geometry and "effective shading normal" rotate together, in the
   same sense, by the same R.
3. For a rigid rotation applied identically to both a box's shape and
   its per-face shading assignment, the rendered image is invariant
   whenever the box's footprint and per-face texturing have the
   matching rotational symmetry - which these do at exactly the 90 deg
   steps `term` produces: the box footprint is not stated as non-
   square anywhere in what's been traced, and `towerPatchST` uses the
   SAME `u` (texture offset) for all 6 blocks regardless of which
   side, so there's nothing asymmetric for a 90 deg step to reveal.
   **A 90 deg-quantized Z rotation of a square, uniformly-textured box,
   with correspondingly-rotated shading, is provably a no-op image** -
   this matches aap's "exactly like before" report precisely, and
   explains it as an expected mathematical consequence rather than a
   broken port.

Net: the angles[2] term is very likely correctly ported (still worth
keeping - it's what the ROM does), but it was the WRONG explanation
for aap's original "the lighting is different" report from the start
of today's session - that report almost certainly meant the orb-lights
subsystem (positions/spread), which is what's now been substantively
fixed above; the tower-face shading may need a different, still-
unidentified cause investigated if aap says it's still off, with a
more specific description of WHAT looks wrong (colour? overall
brightness? gradient direction? interaction with fog?) needed before
guessing further - per [[feedback-debugging-methodology]], don't keep
proposing code changes on pure code-reading without something concrete
(a savestate, a specific visual symptom) to check them against.

## 2026-08-24 (later still): the radial brightness falloff - five theories ruled out, one strong lead found

aap gave the specific description asked for: "the original has the
brightest towers around the center of the screen and then it drops
off. our towers are relatively evenly lit everywhere." Confirmed this
is REAL (not a misremembering) two ways: (1) the existing savestate's
own `Screenshot.png` shows it - corner boxes are barely visible, near-
black; (2) aap supplied a second reference image, `full_opening_yt.png`
(repo root - a fuller memcard's field, many more towers), which shows
the same falloff even more clearly across a wider field, with a visible
blue-white wash on the central boxes that fades to plain dark grey at
the edges.

Went hunting for the mechanism. Each of the following was checked
against real disassembly or real savestate data (not just read-and-
guess) and RULED OUT - recording all five since re-deriving this took
real effort and the next session (or a future me) shouldn't redo it:

1. **Shadow-ceiling loop1** (the long-standing "not implemented"
   candidate from 2026-08-23). Re-read its instructions carefully: it
   reads `*(ptrGlobal+32)`/`*(ptrGlobal+36)` with ptrGlobal = a FIXED
   address (gp-30996), never indexed by row/col within the loop -
   confirms the earlier session's own suspicion ("reads the SAME two
   floats for all 126 cells") - whatever this computes, it can't
   produce a position-varying result. Also confirmed (searched the
   entire rest of DrawTowers for any read of the stack buffer loop1
   writes into) that nothing ever reads that buffer back within
   DrawTowers - it may be write-only/dead code, or consumed by a
   function this session didn't check.
2. **towerGrid** (the sub_217e30 heightfield, multiplies directly into
   towerPatchVertices' brightness). Computed the REAL formula across
   the entire 20x20 grid in Python: it saturates to its outer clamp
   ceiling (220) EVERYWHERE, including at the extreme corners - the
   floor-clamped inputs alone (32+32=64, *10.2=652.8) already exceed
   220 before the outer clamp even applies. towerGrid is a hardcoded
   constant in practice, contributes zero spatial variation. (This
   also means the "two radial bumps" comment on HeightGrid is
   technically true of the pre-clamp math but misleading about the
   actual output - worth another comment pass someday.)
3. **The side-face factor `f3`** (`*(gp-32484)`, doc previously noted
   the backing table has multiple entries {0.5,0.6,0.7,...}).
   Disassembled the real `tower_218318` (0x218318-0x2184c4, 106 insns)
   fully: it's a single fixed-offset `lwc1` with no computed index,
   executed once per tower. Only entry 0 (0.5) is ever read by this
   code path - opening.c's hardcoded `f3 = 0.5f` is correct as is; the
   table's other entries are unused here (maybe read by something
   else, not investigated).
4. **light1/light2/light3 changing per-frame** (aap's own hypothesis:
   "it's like the light position(s) is/are different"). Found their
   real address the hard way - the doc's existing note (0x289f30) was
   WRONG by 0x10000 (should be 0x279f30 - confirmed via fresh
   InitOpeningScene disasm, `position.z=16.0` lands at +8 from that
   base, matching). Read that address from the savestate: light1/2/3
   read back EXACTLY their init constants ({0,0,-1,0} etc), unchanged
   mid-animation - confirms they really are static, direction-only,
   position-independent, exactly as opening.c already has them. Ruled
   out.
5. **GS hardware fog** (the PRIM register's own FGE bit, separate from
   our texture-based "DrawFog"). Decoded PRIM 0x9C's bits: FGE=0. Not
   enabled for the tower draw.

**Strong remaining lead, not yet ported at all: `DrawExtraBuf2`/
`DrawToExtraBuf2`** (0x214240 / 0x214050 - addresses already in this
doc since 2026-08-22, called `DrawTowers -> DrawExtraBuf2(1,2,80,128)
-> DrawToExtraBuf2 -> DrawFog` in the real `DrawOpeningScene`, but
never traced or ported - opening.c's `DrawOpeningScene` has a literal
`// TODO: DrawExtraBuf2/DrawToExtraBuf2 gate + trail functions` where
this belongs). Disassembled both this session:
- `DrawToExtraBuf2` (112 insns): builds screen-sized rects, calls a
  buffer-clear/select helper (0x212e40) then what looks like a GS
  image-transfer/blit (0x212d70) using the CURRENT framebuffer as
  source - i.e. this SAVES the current frame into a separate buffer.
- `DrawExtraBuf2` (158 insns, args 1,2,80,128): constructs GS ALPHA/
  TEX registers by hand (via 0x212898, a raw register-setter, called
  several times with different small integer "register index"-looking
  args: 78, 6, 20) and calls two more image-transfer-shaped helpers
  (0x212af8, 0x2128d0) with SOURCE and DEST rects built earlier - i.e.
  this DRAWS that saved buffer's content back onto the screen, with
  some alpha blend.

Together this reads as a **feedback/accumulation buffer**: each frame,
save the current frame (DrawToExtraBuf2), then next frame, blend the
PREVIOUS frame's saved content back in (DrawExtraBuf2) before drawing
fog. A frame-feedback loop like this naturally produces exactly a
"persistent glow where geometry stays in view, fades where it doesn't"
effect - screen areas with towers that stay in frame across many
frames (typically the center, if the camera drifts/rotates slowly)
would accumulate brightness turn after turn, while areas towers only
pass through briefly (the edges, as they scroll in/out with camera
movement) would not - which matches both reference screenshots'
character (a soft, glowy gradient, not a sharp per-object multiplier)
better than any of the five ruled-out theories above.

**NOT ported, NOT confirmed - this needs real work, likely its own
session(s)**: fully understanding this needs at least six more
untraced helper functions (0x212820, 0x212898, 0x2127a8, 0x212aa0,
0x212af8, 0x2128d0, plus 0x212b90/0x212d70/0x212e40 referenced from
DrawToExtraBuf2) - all raw GS-register-construction/image-transfer
primitives below the vif1SetAD/pktSetAD level our port currently uses,
a different layer than anything ported so far. This is comparable in
scope to the original VU1-upload-chain work that took several sessions
for towers - budget accordingly.

## 2026-08-24 (later still): four vs five, and a real early-frame savestate for the lights

aap corrected two things about the previous "5-instance" light work:
(1) terminology - "the 4 lights" always meant the orbiting glow
particles (`DrawLights`), never the tower face shading; (2) count -
"there should only be 4 lights, of different color" (aap saw two
green ones after the 5-instance change: `lightColors[4%4]` repeating
`lightColors[0]`). Combined with the 2026-08-24 finding that
`DrawLightsAndCubes`'s real dispatcher calls `0x2166c8` ONCE (unindexed)
before the 5x `0x217520(sp,i)` loop, the working theory is now: the
5-instance data this session verified against the savestate
(`cubeSeedTable`/`cubeAnchor`/`cubeRate`, formerly `lightSeedTable`/
`lightAnchor`/`lightRate`) is `DrawLightsAndCubes`'s CUBE half, not
its light-trail half. Reverted opening.c to `NUM_LIGHTS 4`, renamed the
5-instance arrays to `cube*`/`CUBE_INSTANCES` and left them computed-
but-unused (real, verified data, just for the wrong element) for a
future session that traces `0x2166c8` (the actual likely light-trail
dispatcher, still untouched) or `0x217520`/`0x21bf88` far enough to
wire up real cube rendering.

**Bonus finding while re-deriving the cube data**: the "outB" array
(0x27b140) that didn't fit a simple closed form on 2026-08-24 - a
SECOND savestate (see below) at a different frameCount let this get
solved by direct comparison rather than more disasm: `outB[i][k]`
(k=0,1,2) = `initValue + frameCount * cubeRate[i][k]` (plain linear
integration from frame 0, no decay/clamping) while `outB[i][3]` (w)
stays frozen at its frame-0 value forever. Confirmed EXACTLY (matched
to 5 decimal places) against both savestates' actual frameCount (6 and
170) and cubeRate. Not wired into anything (belongs to the cube data,
see above) but recorded since it's a clean, fully solved result.

**aap supplied a second savestate**, `20020207-164243 (00000000).02.p2s`
(repo root) - one of the very first frames visible after boot
(`frameCount` reads 6, vs 170 in the `.01` savestate - same boot
session, `lightsSeed` reads 4002 in both, confirming it's the same
run). Used it two ways:
1. **Confirmed `light1`/`light2`/`light3` are static** here too
   (same values as the `.01` savestate, see the "radial brightness
   falloff" section above) - not new info, but consistent.
2. **Measured the real light dots' screen positions directly from the
   screenshot** (`Screenshot.png` in the zip) with a small Python
   color-cluster script (find local maxima of `g-(r+b)/2` for green,
   `r-g` for pink/red, etc, since PIL/Pillow is available in this
   environment). Found 3 of aap's 4 named lights as clean, isolated
   glowing dots: green (211,297)px, a pale purple-blue dot at
   (242,340)px (read as "blue" - the pink layer's white "core" appears
   to lighten/desaturate the true colour, worth remembering when eyeballing
   these), and a red/pink dot at (425,222)px, against a ~(320,240)
   screen center. All three land in exactly the octant aap described
   from memory (green/blue clearly SW, "pink" clearly E) - real,
   independent, pixel-level confirmation of aap's recollection, not
   just trusting it blind. Could NOT find an isolated red dot - the
   only red-ish pixels found sit directly on the central pyramid
   object's own texture (two small red marks baked into its faces,
   confirmed by cropping/zooming - see also the `.01` savestate's
   screenshot, same marks visible there too), consistent with "red
   near center" if the actual red light sits very close to/behind that
   object.
3. **Compared the same green dot's position in BOTH savestates**
   (211,297 at frame 6 vs 202,282 at frame 170 in the `.01`
   screenshot) - only ~9-15px of net drift over 164 frames, but the
   `.01` screenshot's visible trails (128-sample history) show long,
   clearly CURVED arcs for the other lights (see the pink/red crop) -
   the path curves rather than moving in a straight line, consistent
   with a genuine orbit/sway whose current endpoint just happens to be
   near its frame-6 position again, not evidence of slow motion. This
   is why `SWAY_RADIUS` was bumped back up to 8.0 (from the too-small
   3.0 that read as "not moving") rather than trusting the small net-
   displacement number at face value.

**IMPLEMENTED**: `lightAnchor[4]` values refined using the measured
pixel positions (rough ~10px/unit, screen-Y-down-to-world-Y-up flip,
NOT a real camera unprojection - `sceVu0CameraMatrix`'s `InversMatrix`
step would be needed for that, not disassembled this session).
`SWAY_RADIUS` raised to 8.0 to match the trail-length evidence above.
Motion frequency switched from the (probably-wrong-subsystem)
`cubeRate`-driven phase to the ORIGINAL pre-session code's frequency
shape (`0.01`/`0.005 * (l+10)*0.1`, asymmetric between the x-ish and
y-ish terms), still phase-shifted so frame 0 lands exactly on
`lightAnchor` rather than at a `lightsSeed`-scrambled point. Builds
clean, not yet visually re-verified by aap.

**Explicitly NOT verified**: the real per-frame trajectory formula
(still needs `0x2166c8` or `0x217520`/`0x21bf88` traced properly -
everything above is either aap's direct recollection, pixel
measurements from real screenshots, or a deliberately-labelled
placeholder, not a disasm-derived formula for the actual light-trail
motion). aap's own plan going forward: get this into a state usable
for their own visual comparison against the original and iterate from
there, rather than continuing pure back-and-forth guessing this
session.

## 2026-08-24 (new session, after the opening.c reset): DrawLights fully identified by name - aap's original formula is disasm-CORRECT, only one real bug found

Context: `opening.c` was reset to aap's own pre-AI version (see the "CRITICAL"
project-memory note / [[project-osdsys-towers-status]]) - none of the
`lightAnchor`/`cubeAnchor`/`NUM_LIGHTS 5` work above is active in the current
file. aap's ask this session: get the light/orb **initial state** exactly
right, without knowing why it currently looks wrong.

**Found real function names this time** - `osdsys_dump.idb` (the fuller
emulator-dump database, distinct from the small `OSDSYS.idb` which only
covers the sub-0x101000 bootstrap loader and has just 6 names total) has
actual IDA-assigned names for this whole subsystem, queried via
`ida_funcs.get_func(ea)` + `idautils.Names()` (python-idb's `func_t` here
exposes `.startEA`/`.endEA`, not `.start_ea`/`.end_ea` - a gotcha worth
remembering):
```
0x2166c8  DrawLights            (the "unidentified, NOT traced" function from
                                  earlier sessions - it's real name, no
                                  ambiguity left)
0x217520  DrawCube              (= the "0x217520 DrawLightsAndCubes per-
                                  instance worker" from earlier sessions -
                                  CONFIRMS it's the cube half, not lights,
                                  independently of the 2026-08-24 "four vs
                                  five" correction above - same conclusion,
                                  now from ground truth instead of inference)
0x21bf88  cube_21BF88           (also cube-side, matches)
0x217ab0  OpeningInitLightsCubes
0x217db8  DrawLightsAndCubes
0x25b478  rand
0x253a80  cosf
0x253c08  sinf
```
Also confirmed structurally: `DrawLights` is gp-relative addressed throughout,
and the cube anchor/rate table (`0x27b0f0`/`0x27b190`) is `gp - 0x33F80`
(212864) from `gp=0x2AF070` - far outside the +-32767 range a 16-bit
gp-relative immediate can reach, so `DrawLights` cannot physically reference
that table. The two subsystems are provably independent, not just
conventionally separated.

**Instruction-by-instruction trace of `DrawLights` (0x2166c8), the per-light
position update (outer loop bound confirmed = 4 via `slti v0,a0,4` /
`bnez ...,0x216738` at 0x217bec/f0 - so `NUM_LIGHTS 4` is right, matching
aap's code and the earlier "four vs five" correction, not the 5-instance
cube count)**:
```
v0 = frameCount + lightsSeed + l*17         (gp-31088 = frameCount,
                                              gp-28892 = lightsSeed)
c  = cosf(v0 * 0.01 * (l+10) * 0.1)          (consts @0x2a710c/0x2a7110,
                                              read from expanded.bin: exactly
                                              0.01f and 0.1f)
v0 = frameCount + lightsSeed + l*15
s  = sinf(v0 * 0.005 * (l+10) * 0.1)         (consts @0x2a7114/0x2a7118:
                                              exactly 0.005f and 0.1f)
lightPositions[l][0] = (10-l)*c
lightPositions[l][1] = (3+l)*s
lightPositions[l][2] = c*12 + 88
lightPositions[l][3] = 0
```
**This is byte-for-byte what aap's own pre-AI `opening.c` already had**
(`(10.0f-l)*c`, `(3.0f+l)*s`, `c*12.0f + 88.0f`, the exact same 0.01/0.1 and
0.005/0.1 constants, the same `l*17`/`l*15` phase offsets) - the earlier
(reverted) session's "pure invention from an earlier AI session... nobody
had checked it against ROM" verdict on this code was **wrong**: it just
hadn't been checked yet, and once checked, it's correct. The stray inline
`rand();	// eh?` call inside the per-vertex colour loop and the `// TODO`/
`// weird setting...` comments are aap's own uncertainty markers, not
evidence of fabrication - not yet re-checked against ROM this session (lower
priority than the position/seed formula aap actually asked about), but
should no longer be assumed guilty by default either.

**`OpeningInitLightsCubes` (0x217ab0-0x217db8) also fully re-checked for the
light-specific parts** (the first ~194-insn block, 5-iteration cube seed/
anchor/rate loop, is the cube half already covered above and still
irrelevant here): a SEPARATE 4-iteration loop (`slti v0,s0,4` /
`bnez ...,0x217c60`) does, per iteration:
1. `cosf(frameCount * 0.005 * (l+1))` and `sinf(frameCount * 0.003 * (l+1))`
   (consts @0x2a716c=0.005 exactly, @0x2a7170=0.003 exactly) - **results
   discarded, never stored anywhere.** This is a genuine dead call in the
   ROM itself. Matches aap's own code EXACTLY, including the comment
   ("// unused, perhaps earlier formula?") already correctly flagging it as
   unused - aap had this right from the start.
2. Zero `lightTrailVerts[l][i][0..2]` for i=0..127 (x/y/z only, w untouched) -
   matches aap's code exactly.
3. Call `sceVu0UnitMatrix`-shaped helper (0x267630) 4x on 64-byte-strided
   pointers - matches `for(i=0;i<4;i++) sceVu0UnitMatrix(lightMatrixHistory[l][i])`
   exactly.

Then, unconditionally (once, not per-light), at the very end of the function:
```
217d78:  jal   0x25b478        # rand()
217d80:  li    v1,2345
217d84:  div   zero,v0,v1
217d88:  beqzl v1,...; break    # (div-by-zero guard, standard idiom)
217da4:  mfhi  a0                # a0 = v0 % 2345  <- REMAINDER, not quotient
217da8:  addiu a0,a0,3456
217dac:  sw    a0,-28892(gp)     # lightsSeed
```
**Confirms the one real, disasm-verified bug**: aap's `opening.c` had
`lightsSeed = rand()/2345 + 3456` (division). Real ROM uses `mfhi` after
`div`, i.e. the REMAINDER: `rand() % 2345 + 3456`. Since `lightsSeed` feeds
directly into every light's very first frame-0 phase (`frameCount(=0) +
lightsSeed + l*17/l*15`), this single operator was enough to desync all 4
orbs' initial positions from frame 0 onward - it's a strong, direct answer
to "the initial positions... are different" without needing any placeholder
anchor/sway data at all. **FIXED** in `opening.c` (one-line change, `/` -> `%`,
comment cites this trace). Builds clean.

**Not re-checked this session** (lower priority, not what aap asked about):
the trail-line LINESTRIP rendering block later in `DrawLights` (opening.c's
own `// BUG: this isn't updated in original` comment on the `x1 = x0`/
`y1 = y0` lines) - flagged by aap's own past self as suspect, still
unverified against this disassembly. Worth a follow-up pass if the orb
POSITIONS now look right but their TRAILS still look wrong.

**Net for "initial state"**: no anchor table, no new sway model, no rewrite
needed - the position formula was already correct. The fix is the one-line
`lightsSeed` operator. aap to confirm visually.

## 2026-08-24 (same session, continued): why the harness starts "earlier than the BIOS" - the rand() call-count gap, and a real-time-clock seeding hypothesis

aap: "i think were starting a bit earlier than the BIOS maybe. perhaps that
rand()... can we trace that? by setting a breakpoint somewhere maybe?"
Answered entirely via static call-graph analysis on `osdsys_dump.idb`
(`idautils.CodeRefsTo(ea, 1)` walked upward through `ida_funcs.get_func(ea)
.startEA` repeatedly) - no live emulator/breakpoint needed for this part.

**Confirmed: real `InitOpening` (0x211db0) - which is exactly opening.c's
`Init()`, same call sequence, same names now resolvable - calls a chain that
makes 4 unconditional `rand()` calls before the animation ever reaches
`OpeningInitLightsCubes`.** Full call order inside `InitOpening`, all names
from the IDB:
```
InitOpening (0x211db0):
  0x212258                          (unnamed)
  0x215f18                          (unnamed - also called per-frame by
                                      ProcessOpeningAnimation, so likely a
                                      shared fog/state-update helper, not
                                      light-related)
  OpeningInitTowersFog (0x218e00)   <- matches opening.c's InitTowersFog()
  0x219f08 -> 0x215798              <- **4x rand()** here (0x2158a0,
                                      0x215940, 0x215968, 0x215990) - NOT
                                      traced further this session, but this
                                      is exactly the gap opening.c's Init()
                                      already flagged with "// ??? flare and
                                      illegal stuff?" (DoOpeningIllegal's
                                      switch has a DoIllegalDisc case, so
                                      "flare and illegal" is a very
                                      plausible real name for whatever
                                      0x215798 sets up)
  0x214f20, 0x205e88                (unnamed)
  frameCount = *(0x1f0000+3136)     (a counter/timer read, stored to
                                      gp-31088 - NOT investigated further,
                                      separate question from lightsSeed)
```
Then, still same thread, `OpeningThread`'s outer loop calls `DoOpeningIllegal`
-> `DoOpening` -> `DrawOpening` -> `InitOpeningScene` -> `OpeningInitLights-
Cubes`, whose own `rand()%2345+3456` is therefore always the **5th**
`rand()` call since boot on this thread, never the 1st.

Checked opening.c directly: the ONLY two `rand()` calls anywhere in the
harness are `lightsSeed`'s own line and the one inside `DrawLights` (already
confirmed real, see above) - i.e. the harness's `lightsSeed` draws from
`rand()` call #1, not #5. This is a precise, disasm-confirmed explanation for
aap's "starting earlier" intuition, not a guess.

**Bigger, only-partially-resolved finding: the RNG looks real-time-clock
seeded, in an UNRELATED thread - a genuine scheduling race, not something a
static trace can fully pin down.** Found both `srand()` (0x25b468) call
sites in the entire ROM (`idautils.CodeRefsTo` on the function, only 2
results) - both live inside `ThreadB` (0x20f7c0), NOT `OpeningThread`, at
0x210bc0 and 0x2110c0. Both build their seed argument by reading 5 small
fields from a fixed address (`*(0x1f0000+3260..3276)`) and packing them with
a shift-and-OR chain (`sll 5; or; sll 5; or; sll 6; or; sll 6; or`) - the
classic shape for packing a date/time struct (e.g. year/month/day/hour/
minute) into one int for a seed. NOT confirmed which fields exactly (would
need to identify what's really at `0x1f0000+32xx` - looks like a fixed
kernel/hardware struct, not chased further this session). If this reading
is right: **the real console's `lightsSeed` - and therefore the whole orb
arrangement - genuinely differs every real power-on**, and whether
`ThreadB`'s `srand()` call lands before or after `OpeningThread` reads the
shared `rand()` stream is a live scheduling race between two independent
kernel threads, not resolvable from the static call graph alone. This means
there may be no single "correct" reference sequence to match frame-for-
frame against one captured savestate/video - matching the FORMULA (already
done, see above) is the right target, not reproducing one boot's literal
numbers.

**IMPLEMENTED** (aap's choice, from the three offered): a placeholder
`rand(); rand(); rand(); rand();` added in `Init()` exactly where the real
0x219f08/0x215798 chain runs (right after `InitTowersFog()`), clearly
commented with this trace - burns the same rand()-call COUNT as real
hardware without porting the chain itself. Builds clean. Does NOT solve the
`ThreadB` race/RTC-seed question - `lightsSeed`'s absolute value will still
generally differ from any one specific real capture, expectedly so per the
finding above.

**Not done, offered but declined for now**: fully tracing 0x219f08/0x215798
to properly implement whatever "flare and illegal stuff" really is, and
setting a live PCSX2 breakpoint on `rand`/`srand` (real EE VAs 0x25b478/
0x25b468, unadjusted - aap has PCSX2 access, this session doesn't) to
observe the `ThreadB` vs `OpeningThread` race directly for one real boot,
which is the only way to fully resolve it beyond the static-analysis
hypothesis above.

## 2026-08-24 (same session, continued): towers ported back into opening.c, and DrawExtraBuf2/DrawToExtraBuf2 turn out to be built from already-ported primitives

aap: bring the towers back into `opening.c` (which had none since the
reset), and revisit "something about the lighting that wasn't right" - the
radial brightness falloff from the 2026-08-24 "five theories ruled out, one
strong lead found" section above.

**Towers: ported wholesale from `opening.c.ai-reference` into `opening.c`.**
This is NOT the same situation as the light subsystem's reverted work - the
tower port there was independently validated multiple times before the
reset (a live PCSX2 render confirmed "towers now look right" on 2026-08-23,
several fields were savestate-cross-checked exactly, e.g. `towerA`/`towerB`
reproduced a live savestate's values to the exact float/int for all 21 real
cells - see the field-capture comment in the ported code). Brought in:
the VU1 upload chain (`vudataRelocate`/`sendDma`/`vu1Wait`, `vudata.inc`),
the tower field arrays and `HeightGrid`, `LoadCapturedTowerField` (the
savestate-captured real field - memcard history parsing itself is still a
TODO), the per-tower chain patch helpers (`towerPatchParams`/`Tags`/
`GifTag`/`Vertices`/`ST`), `InitTowersFog`, and `DrawTowers`, plus the
`TOWER_UPLOAD_VU1`/`TOWER_LOAD_FIELD`/`TOWER_DRAW` bisection toggles
(left in per [[feedback-debugging-methodology]] - they were the actual tool
that got this working the first time). `DrawOpeningScene` now calls
`DrawTowers()` before `DrawFog()`, matching the real draw order
(`DrawTowers -> DrawExtraBuf2 -> DrawToExtraBuf2 -> DrawFog ->
DrawLightsAndCubes`) established in the 2026-08-23 "draw-order regression"
section. Builds clean, not yet visually re-verified by aap (no emulator in
this session).

**DrawExtraBuf2/DrawToExtraBuf2 re-investigated with the `osdsys_dump.idb`
naming trick (see [[reference-osdsys-tooling]]) - the old "six untraced
helpers, comparable in scope to the VU1 upload chain" estimate from
2026-08-24 earlier today was WRONG, and by a lot.** Queried real names for
all six: they are `vif1Begin`/`vif1End`/`pktSetAD`/`pktSetTEST_1`/
`pktSetAlphaBlend`/`pktSetTexRect` - i.e. exactly the same GS-packet
primitives `opening.c` already has fully ported and uses throughout
(`DrawFog`, `DrawLights`, `DrawTowers`). Two more addresses used inside
these functions resolved too: `0x213ff0` = `vif1SetXYOffset`, `0x213f50`
= `vif1SetZWrite` - also already-ported. This is NOT a new, unported GS
layer - it's a normal draw sequence built entirely from primitives that
already exist in this codebase.

**Partial trace of `DrawToExtraBuf2` (0x214050-0x21423c, 123 insns) done
this session** (not finished/verified end-to-end): opening sequence is
`vif1SetXYOffset(0, *(0x1f0c44)); vif1SetZWrite(0);
vif1SetFramebuffer(extraBufFbp, 0, width, height, 1)` - i.e. switch the
draw target to a separate buffer and clear it, Z-write off. Then a
frameCount-parity branch (`frameCount & 1`) changes how a width/FBW-shaped
value gets packed into a `vif1SetAD`-style register write (register 6 =
TEX0_1) - looks like a two-buffer PING-PONG scheme (alternate which of two
buffers is written vs sampled each frame), which would explain a genuine
frame-to-frame accumulation effect. A `vif1SetTexRect` call follows with a
DEST rect at half the tracked width and a SOURCE rect at full width -
not yet understood why (possibly a half-horizontal-resolution storage
buffer, not confirmed). `DrawExtraBuf2` (0x214240-0x2144c0, 160 insns) -
the "draw the saved buffer back" half - not traced yet this session.

**Deliberately NOT ported this session**: getting the exact FBP table
address, the ping-pong parity logic, and the half-width rect right needs
more careful, uninterrupted tracing than was left in this session's budget,
and this piece has no independent verification method available yet (no
live savestate captured mid-way through this specific code path) - porting
it now would mean guessing at exactly the kind of GS-register/address
arithmetic detail that's easy to get subtly wrong and hard to visually
diagnose after the fact. Given [[feedback-debugging-methodology]]'s lesson
about not stacking unverified guesses, paused here to sync with aap rather
than pushing through - offered to continue next.

## 2026-08-25: the tower VU1/DMA blob rebuilt as real dvp-as source - OFF_* macros and vudataRelocate() eliminated

aap: the `OFF_CHAIN_HEAD`/`OFF_UNPACK_ADDR`/etc macros and `vudataRelocate()`
made the tower code hard to follow - asked to try assembling the real chain
with `ee-dvp-as` (found in the SCE toolchain, `/usr/local/sce/ee/gcc/bin`)
and patch via real linker symbols instead of hand-computed byte offsets
into a raw extracted blob (`vudata.inc`).

**Fully done, verified, and landed.** `osdbits/towerchain.dsm` (new file)
reconstructs the entire real tower VU1 upload + per-tower DMA chain as
actual assembly source, using the existing `vucode_1.vsm` (an already-
annotated VU microcode source, not authored this session - predates this
project's AI-agent involvement) for the microcode itself. Verification
method: extracted the real ROM bytes at 0x2678e0-0x268870 directly from
`expanded.bin`, assembled `towerchain.dsm` with `ee-dvp-as`, and diffed the
output byte-for-byte against those real bytes - not "looks plausible",
actually bit-compared.

Process (kept here since it's a reusable technique for reconstructing any
other VU1/DMA blob in this codebase, e.g. the cube subsystem's own VU
program if that's ever tackled the same way):
1. First pass: wrote the chain as two `DMAcnt *` regions (upload, then
   params+packets) with named labels (`TowerView`/`TowerModel`/
   `TowerLightMatrix`/`TowerGifTag`/`TowerBlock0`-`5`) and the bulk literal
   qwords emitted by a small Python script directly from the extracted
   real bytes (not hand-transcribed - avoids transcription errors on ~130
   qwords of data). Assembled and diffed: matched to within 3 small,
   individually-explained differences.
2. Fixed a missing `BASE 0`/`OFFSET 512` pair (real bytes had them packed
   into the same qword as `MSCAL 0`, cheap to add once spotted in the
   diff).
3. The remaining tag-type mismatch (assembler defaulted to a CNT-type tag
   between the two regions, real ROM uses NEXT with an explicit target
   address) needed `DMAnext *, <label>` instead of a second `DMAcnt *` -
   aap pointed at `/usr/local/sce/ee/sample/basic3d/vu1/cube.dsm` which
   demonstrated the exact pseudo-op (`DMAnext *, My_cube_start`). Restruct-
   ured the packet region under its own label (`TowerPackets`) and pointed
   `TowerChain` at it with `DMAnext` - after this, **byte-for-byte exact**
   against the real ROM at the object-file level (only the tag's own
   address field differs, and only because it's now a proper relocation -
   see below).
4. Confirmed via `ee-objdump -r` that the tag's address field carries a
   real relocation (`R_MIPS_DVP_27_S4` against `.vutext`) rather than a
   baked-in absolute address - this is exactly why `vudataRelocate()` can
   go away: the LINKER now does automatically, correctly, for every build,
   what that function used to do by hand at runtime once.
5. Wired into `opening.c`: replaced the `OFF_*`/`VUDATA_BASE_VA` macros and
   `#include "vudata.inc"` with `extern` declarations for the 12 real
   symbols (`__attribute__((section(".vudata")))`, matching the pattern
   from the SDK's own `hako` VU1 sample - confirmed this exact pattern
   builds and links correctly via a standalone test build of that sample
   first, before relying on it here); replaced `vudata + OFF(...)` call
   sites in `towerPatchParams`/`Tags`/`GifTag`/`Vertices`/`ST` with direct
   references to the named symbols (`towerBlocks[6]` now holds pointers to
   the 6 real block symbols, in the same index order the old
   `towerBlockPtrs[]` real-address array used, so `towerSignFlags[b]` and
   the `b==0` special case still line up correctly); deleted
   `vudataRelocate()` entirely; `osdbits/Makefile` already had a `.dsm.o`
   rule wired to `ee-dvp-as` (unused until now) - just added `towerchain.o`
   to `OBJS`. Deleted `osdbits/vudata.inc` (confirmed nothing else
   referenced it first).
6. **Re-verified byte-exactness on the FINAL LINKED `main.elf`**, not just
   the standalone object file (`ee-nm`/`ee-objcopy -j .vutext` + the same
   Python diff) - confirmed the tag's address field now holds the REAL
   address of `TowerPackets` in this specific build (0x121000, matching
   `ee-nm`'s own symbol table exactly), correctly different from the real
   ROM's own baked-in 0x2681a0 for the obvious reason (different link
   base) while being internally, semantically correct - the strongest
   verification short of running it. All 106 qwords of packet data plus
   params/giftag: still byte-for-byte identical to the real ROM.
7. Builds clean end-to-end (`make clean && make` in `osdbits/`), zero
   behavior change intended - not yet visually re-verified by aap.

**Update: that "separate" vucode_1.vsm byte mismatch was the actual cause
of a real regression, and IS now fixed.** After landing the rewire, aap
reported towers had gone completely invisible. Re-verified every layer
against a fresh savestate before touching code (per
[[feedback-debugging-methodology]]): the uploaded VU1 microcode in
`vu1MicroMem.bin` matched our assembled output byte-for-byte, `TowerView`/
`TowerModel`/`TowerGifTag`/`TowerBlock0`-`5` all showed correctly-patched
(not static-default) values, vertex/color/ST data all looked plausible -
every layer of OUR code checked out. The remaining suspect was the one
byte `vucode_1.vsm` had already been flagged as not matching the real ROM.
Decoded which instruction it belonged to (instruction #64 of the upload,
file line 75): `MULq.xyz VF24, VF25, Q` - the perspective-divide step
(`VF24.xyz = VF24.xyz * Q`, right after `DIV Q, VF00w, VF24w` computes Q).
Confirmed the fix empirically rather than by hand-decoding VU opcode bit
layouts (error-prone) - wrote a minimal one-instruction `.dsm`/`.vsm` test
harness, tried plausible register substitutions, reassembled, and compared
raw bytes against the real ROM's value until an exact match: `VF24, VF24,
Q` (source should be `VF24`, not `VF25` - a one-character typo, `24`↔`25`
elsewhere earlier in the file were correct, this one instruction wasn't).
**Fixed in both `vucode_1.vsm` (aap's original, at `/u/aap/src/osdsys/`)
and `osdbits/vucode_1.vsm`.** Re-verified: the assembled microcode is now
**100% byte-exact** against the real ROM (previously 1831/1832 bytes;
now 1832/1832) - strictly better than the old `vudata.inc` situation ever
let us confirm, since that was a black-box extraction with no independent
check on its own correctness. Rebuilt clean, not yet re-tested by aap.

**Reusable technique for next time** (see [[reference-osdsys-tooling]]):
`ee-dvp-as` (in the SCE toolchain, not a separate install) plus a real
`vumacros.h`/pseudo-op set (`DMAcnt`/`DMAnext`/`DmaRef`/`.DmaData`/
`unpack[r]`/`MSCNT`/`MSCAL`/`BASE`/`OFFSET`/`iwzyx`) can reconstruct any
VU1/DMA blob this project has as a raw byte extraction, turning hand-
computed offsets into named, linker-relocated symbols - verify by
assembling and diffing against real ROM bytes extracted the same way
towers-analysis.md already does elsewhere in this doc, not by trusting the
pseudo-op syntax alone. The SDK's own samples
(`/usr/local/sce/ee/sample/vu1/*`, `/usr/local/sce/ee/sample/basic3d/vu1/*`,
`/usr/local/sce/ee/sample/advanced/anti/refmap-4times/*`) are real,
working, buildable references for the exact pseudo-op syntax - `hako` for
the basic DMAcnt+MPG+unpack pattern, `refmap-4times/mug.dsm`+`packet.dsm`
for repeated MSCNT-separated unpack blocks (exactly the tower per-block
shape), `basic3d/vu1/cube.dsm` for `DMAnext`.

## 2026-08-24 (same session, continued): first visual comparison against real OSDSYS - towers confirmed working, "dark block" was a red herring (it's the unported refractive cubes)

aap ran the build in PCSX2 and compared against real `PS2 BIOS (USA)` boot
screenshots (`.var/app/net.pcsx2.PCSX2/config/PCSX2/snaps`, both real and
ours). **Tower field positions confirmed correct by aap.** Zoomed crops of
matching ordinary boxes (Python/PIL, per [[reference-osdsys-tooling]])
showed similar blur and front/side shading contrast between ours and real -
no evidence of a general mip or lighting-formula bug. Also re-confirmed the
mip call args fresh against disasm (0x218728: a1=1,a2=5,a3=-65, matches the
port exactly).

**One large, dark, foreground shape in both real screenshots had no match
in ours.** Initially investigated as a possible tower bug - traced a
plausible-looking mechanism (the `towerC==1.0` special cell's `towerGrid
[r+3][c+6] = -1.0` write might permanently self-skip on every frame after
the first, since `HeightGrid()` only runs once at init - confirmed via
`CodeRefsTo` that nothing else calls it). Hit real register-reuse ambiguity
trying to confirm this against fresh disasm and could not verify it with
confidence. **aap clarified: this dark object is NOT a tower at all - it's
one of the refractive cubes** (`DrawCube`/`cube_21BF88`, the 5-instance
`cubeAnchor`/`cubeRate` half of `DrawLightsAndCubes` identified on
2026-08-24 earlier - see the "four vs five" section). Cubes were never
ported into `opening.c` (deliberately deferred - aap wanted towers solid
first), so of course nothing renders there. **The `towerGrid` self-skip
theory above is UNCONFIRMED and was never actually the explanation for
anything observed - it's a loose thread, not a demonstrated bug.** Worth a
quick check sometime (does the real captured field's one `towerC==1.0` cell,
row 12 col 0, actually stay visible for aap in a multi-second real capture,
or does IT also vanish after frame 1? that would settle it either way) but
not urgent - nothing currently observed depends on it being wrong.

**Towers status, end of this session**: positions, draw order, texture/mip
setup, per-tower geometry+colour+UV patching, and the VU1 upload chain are
all ported and now visually confirmed against real OSDSYS (first time this
has been checked since the reset). Known incomplete pieces, none blocking
the above: real memcard history parsing (using a fixed captured field
instead - [[project-osdsys-towers-status]] has the "how complete" summary),
the shadow-ceiling gate, DrawExtraBuf2/DrawToExtraBuf2 (radial brightness
falloff), and - the actual next step per aap - the refractive cubes
(`DrawCube`), not started.

## 2026-08-24 (same session, continued): aap's own IDA lookup caught a real bug - sendDma()'s FlushCache/sceGsSyncPath mixup, and the "DrawTowers looks unfamiliar" report resolved

aap opened `osdsys_dump.idb` themselves and looked at `DrawTowers` directly
- correctly identified as the right database/address (confirmed: `OSDSYS.idb`,
aap's own 2023 file, only covers 0x100000-0x14c964, nowhere near
`DrawTowers` at 0x2185c8 - that was ruled out first). aap's specific,
concrete objection: "no sceGsSyncPath in our code," suspecting this was
unverified deepseek-session output. Took this seriously per
[[user-ps2-reveng-background]] ("when aap says something looks wrong, it
usually is") and did a full, systematic call-list diff instead of spot-
checking: extracted every `jal` inside `DrawTowers`'s real bounds (0x2185c8-
0x218b20, confirmed via `ida_funcs.get_func`) and resolved each target's
real name via the IDB. Two real findings, one confirms a bug aap's
instinct caught, the other clears a false alarm:

**Confirmed bug: `sendDma()`'s "FlushCache before the kick" is right for
the ONE-TIME vucode_1 upload but WRONG for the per-tower kick.** Disassem-
bled both real DMA-kick sequences directly (not paraphrasing old notes):
- `OpeningInitTowersFog`'s upload (0x218ff0-0x219044): QWC=0; TADR=addr&
  mask; `*(0x1000e010)=2`; **FlushCache(0)** (0x24dce0, confirmed by name -
  an EARLIER doc note calling this an "RPC/file helper" was simply wrong);
  CHCR=325. This matches `sendDma()` exactly - it was never broken for
  this call site.
- `DrawTowers`'s per-tower kick (0x218a48-0x218a90): QWC=0; TADR=addr&mask;
  `*(0x1000e010)=2`; **sceGsSyncPath(0,0)** (0x262418); CHCR=325. NO
  FlushCache anywhere in this window. `opening.c` was calling the shared
  `sendDma()` (with its FlushCache) for BOTH sites - correct for init,
  wrong for every single per-tower kick. This is exactly the "no
  sceGsSyncPath" gap aap spotted.
- Also real, also missing: `DrawTowers` does NOT wait for each tower's DMA
  before starting the next one (pipelined) - the only synchronization
  after a per-tower kick is ONE final `sceGsSyncPath(0,0)` after the
  ENTIRE r,c loop finishes (0x218ad8). `opening.c` was calling `vu1Wait()`
  (a manual D1_CHCR poll) after every tower, serializing what the real
  game pipelines.
- **FIXED**: added `towerKick()` (the real per-tower sequence, sync-then-
  kick, no FlushCache) and `towerSyncEnd()` (the one trailing
  `sceGsSyncPath`), used in place of `sendDma()`/`vu1Wait()` for both
  per-tower call sites (the live loop and the disabled `TOWER_MINIMAL_TEST`
  path). Also added the missing trailing `sceGsSyncPath(0,0)` to the
  INIT-upload call site (real has `sceDmaSync(...)` THEN `sceGsSyncPath
  (0,0)` after the kick; `vu1Wait()` covers the former, the sync call was
  simply absent before). Builds clean, not yet visually re-verified.

**False alarm, but worth documenting since it explains why DrawTowers
"looks different" from the port in other ways too**: the single real calls
to `sceVu0RotMatrix`/`sceVu0TransMatrix`/`sceVu0MulMatrix` (visible in IDA
as ONE `jal` each) show up in `opening.c` as several separate calls
(`sceVu0RotMatrixZ`+`Y`+`X`, `sceVu0CopyMatrix`+manual row-3 add, 4x
`sceVu0ApplyMatrix` in a loop). Disassembled all three SDK wrappers
directly from `libvu0.a` (same technique as the earlier `sceVu0ApplyMatrix`
convention check) to settle whether this is a bug or just a different
shape: **confirmed exact matches, not approximations** -
`sceVu0RotMatrix(m0,m1,rotVec)`'s own body IS literally `RotMatrixZ(m0,m1,
rotVec.z); RotMatrixY(m0,m0,rotVec.y); RotMatrixX(m0,m0,rotVec.x)` (tail
call chain, traced instruction-by-instruction); `sceVu0TransMatrix(m0,m1,
tv)`'s body IS literally "copy rows 0-2, row3.xyz = m1.row3.xyz+tv.xyz,
row3.w untouched" (VU0 macro-mode, `lqc2`/`sqc2`/`vadd.xyz`); `sceVu0Mul-
Matrix(m0,m1,m2)`'s body IS, per row, the exact same `vmulax/vmadday/
vmaddaz/vmaddw` sequence `sceVu0ApplyMatrix` uses, applied to each of m2's
4 rows. So `opening.c`'s expansion produces bit-identical results to the
single real calls - genuinely not a bug, just inlined by hand rather than
calling the wrapper. Added an explicit comment in the code citing this
verification, since the visual mismatch in IDA (1 call vs several) is real
and will trip up the next person who checks too, same as it did aap this
session.

**Process note for next time**: this whole investigation started from
aap's specific, first-hand IDA observation, not a report of "it looks
wrong" - and it was right to treat it as a real lead rather than
reassurance. The fix wasn't "trust the port less," it was "diff the real
call list systematically" - which cleanly separated one genuine bug from
two false alarms in the same pass, instead of leaving all three as vague
suspicion.

## 2026-08-24 (same session, continued): call structure sketched as named empty stubs

aap: sketch more of the real call structure into `opening.c` itself, even
as empty stubs, so what's missing (and where it plugs in) is visible in
the code rather than only in this doc. Walked every real `jal`/`j` from the
opening-animation's entry points (`OpeningThread`, `InitOpening`,
`DoOpeningIllegal`, `ProcessOpening`, `DrawOpening`, `DrawOpeningScene`,
`OpeningInitTowersFog`, `DrawTowers`, `OpeningInitLightsCubes`,
`DrawLightsAndCubes`, `DrawLights`, `DrawCube`, `cube_21BF88`,
`DrawExtraBuf2`, `DrawToExtraBuf2`, `ProcessOpeningAnimation`, `sub_217e30`)
down to depth ~2-3, resolving every target's real name - 185 functions
total, most of them libc/SDK float-emulation or heap-allocator internals
(`dpmul`, `_malloc_r`, etc.) not relevant here.

**Added 20 new named empty-stub functions** (real address + real callee
list in a comment on each, wired into their actual real call sites) for
everything in the opening-animation's own call tree that isn't ported yet:
- **Cube subsystem** (`DrawCube` and its 7 real callees down to
  `cube_21BF88`/`cube_21BC90`) - wired into `DrawLightsAndCubes`. This is
  the roadmap for aap's stated next task.
- **`DrawExtraBuf2`/`DrawToExtraBuf2`** - wired into `DrawOpeningScene`
  (both) and `OpeningInitTowersFog`'s tail call (`DrawToExtraBuf2` only).
- **The two "trail functions"** at the end of `DrawOpeningScene`
  (`sub_218b20`->`sub_2144c0`, `sub_218bd0`->`DrawSomeSprite2`+
  `fp_25A368`) - previously just a `// TODO` comment.
- **The "flare and illegal stuff" chain** (`sub_219f08`->`sub_21a438`+
  `sub_215798`+`sub_219cb8`->`sub_21b690`) - replaces the old inline
  `rand();rand();rand();rand();` placeholder in `Init()`; the 4 real
  `rand()` calls now live in their real home, `sub_215798`'s stub.

All stubs are genuinely empty (or call only their own real children, also
empty) - zero behavior change, confirmed by `sub_215798` being the only one
with a body at all (the 4 `rand()` calls, unchanged from before). Builds
clean, no unused-function warnings (every stub is reached from its real
parent, matching the real call graph exactly rather than sitting
unreferenced). Not wired: `sub_21b690`'s OTHER real call site (from
`OpeningInitLightsCubes`, which is otherwise fully verified/ported) - no
confident place to add that call without tracing `sub_21b690` itself
first, so left as a comment instead of a guess. Also out of scope this
pass: the illegal-disc/flare warning screen (`DrawIllegalScene`'s real
children - `DrawRedFlare`, `DrawIllegalFog`, `DrawIllegalCubes`, the
`flare_*` functions, `DrawIllegalCube`, `DrawSCEText`/`DrawIllegalText`) -
a large sibling subsystem, currently still aap's original empty
`InitIllegalScene`/`DrawIllegalScene` stubs, not sketched this pass since
aap's focus has been the normal opening animation, not the warning screen.

## 2026-08-24 (same session, continued): stale-comment cleanup, and pushing further into DrawToExtraBuf2's TEX0_1 encoding

aap: clean up comments that are now stale from this session's verification
work (not a request to touch aap's own pre-session uncertainty markers in
untouched areas - those are still genuinely open and were left alone), and
restated the actual objective: the tower lighting / radial-falloff
question, still unresolved.

**Cleaned up** (all in `opening.c`, all confirmed resolved THIS session,
none of aap's original notes elsewhere touched): the bare `// TODO:` atop
`InitLightsCubes` (now says what's actually missing - the cube-half seed
table); "unused, perhaps earlier formula?" on the dead `cosf`/`sinf` calls
(now states plainly they're confirmed-real dead ROM code); the commented-
out "other formula from init" dead code in `DrawLights` (deleted - it was
never actually an alternative for this location, just early uncertainty
now resolved); `rand(); // eh?` (now cites the disasm confirmation); the
bare `// TODO` in `DrawLightsAndCubes` (now names the real remaining gap -
an untraced `vif1SetClamp` call between `DrawLights` and `DrawCube`); the
`// ...`/`// InitText();` placeholders in `Init()` (replaced with a
confirmed-complete real call sequence comment, and a new `initTextShit`
stub for the real call that placeholder stood in for).

**Pushed further into `DrawToExtraBuf2`** (0x214050-0x21423c) to try to
finish last session's partial trace. Confirmed structure, high confidence:
`vif1SetXYOffset(0, *(0x1f0c44))`; `vif1SetZWrite(0)`;
`vif1SetFramebuffer(extraBufFbp, 0, fullWidth, height, 1)` (clears the
WHOLE buffer, not half - corrects the partial trace's "half-width dest"
guess, which turned out to belong to a LATER `vif1SetTexRect` call, not
the framebuffer setup); a `frameCount & 1` branch that either includes or
omits a `(width*height)>>6` term when building a value later written to
`vif1SetAD(SCE_GS_TEX0_1, ...)` (register 6, confirmed against
`eestruct.h` - not a guess); a second `vif1SetAD(SCE_GS_TEX1_1, 96)`
(register 20, confirmed against `eestruct.h`) which decodes cleanly to
`SCE_GS_SET_TEX1(0,0,LINEAR,LINEAR,0,0,0)` - bilinear, no mipmap, matching
the pattern already used elsewhere in this file.

**Where it's stuck**: the `(width*height)>>6` term doesn't cleanly fit
TEX0's TBW field (which should just be `width>>6`, a small number - TBW
from a width*height product would be enormous and clearly wrong for that
field). Either this term belongs to a different TEX0 subfield than
assumed, or the width/height identification itself is off. Did NOT push a
guess into the port for this reason - the register-arithmetic risk here is
exactly the kind that produced the confirmed `sceGsSyncPath` bug earlier
today, and this one doesn't have as clean a way to spot-check.

**Tried and set aside**: reading the extra-buffer's actual pixel content
back out of a savestate's `GS.bin` (4MB, VRAM + GS registers) to settle
this empirically instead of by hand - a live TEX0_1 register read
wouldn't isolate `DrawToExtraBuf2`'s write specifically (later calls the
same frame, `DrawFog`/`DrawLights`, overwrite TEX0_1 again before the
savestate captures anything), and decoding raw VRAM into a viewable image
needs the FBP/PSM/dimensions nailed down first - which is exactly what's
uncertain. Not pursued further this session; flagged as a real option if
aap wants to keep going on this specific question.

## 2026-08-25: the refractive cubes ported into opening.c, and a real framebuffer-restore bug found and fixed before first test

aap: "let's get to the cubes then. this is what i'm after! the fancy
refractions that are so characteristic for the ps2... i would be opposed
to the idea of drawing them in solid color first, so we know we match the
position and scale and all that" - explicit instruction to port the real
mechanism directly, not a solid-colour placeholder-then-iterate pass.

**Approach**: 4 fork subagents traced the previously-sketched empty cube
stubs (`cube_21BC90`/`cube_21BF88`/`sub_21c7a8`/`CubeTextureFuckery`/
`cube_21B798`/`DrawTexturedQuad`/`cube_21BBE0`/`cube_21BA08`/old
`DrawCube()`) against the real disasm in parallel, then real ROM data
tables (cube corner geometry, face->vertex table, face normals at
0x27b090, UV template, the four colour vectors at 0x27b040/50/60/70) were
pulled directly out of `expanded.bin` rather than invented - the corner
geometry was cross-checked as internally consistent by confirming it
reproduces the real face-normal table's sign pattern before trusting it.

**Ported** (all in `opening.c`, replacing every empty cube stub):
`CUBE_INSTANCES` (5) with a verified real per-instance seed table;
`InitCubes()` (verified `cubeAnchor`/`cubeRate`/`cubeOutB` init formulas);
`CubeCaptureBuffer()` (real `sub_21c7a8`, 0x21c7a8 - captures the prior
frame into `extraBuf2` for the refraction sample); `CubeFaceSetup()` (real
`cube_21BC90`, 0x21bc90 - per-face normal/visibility/lighting, simplified
per the in-code comment: a perfect cube's per-face normal collapses what
the real code does per-vertex into one shared value, numerically
identical); `CubeTransformAndClip()` (real `cube_21BF88`, 0x21bf88 -
integrate+wrap rotation, build the rotate/translate/view matrix chain,
clip-test, transform all 8 corners); `DrawCubeFace()`/`DrawCube()` (real
`DrawTexturedQuad` 0x21c560 plus `cube_21B798`/`21BBE0`/`21BA08` and the
0x216f88 callback, merged into two functions since the real boundaries
between them don't matter once the algorithm is known - same approach
already used for `DrawLights`). Wired into `InitLightsCubes()`/
`DrawLightsAndCubes()`. Built clean (two `const`-qualifier warnings fixed
by dropping `const` from `cubeCorners`/`cubeFaceNormal`, matching this
file's existing convention for similar tables).

**Flagged, not blocking** (per aap's "Start porting now" - narrow,
specific gaps, not a call to stop): the exact scrolling-UV phase source
for the refraction sample (a not-fully-traced 0x257918 double-precision
helper - `DrawCubeFace` mode 1 uses the cube's own screen position
instead, the standard cheap screen-space-refraction substitute); two
colour-clamp scale constants inside the real 0x216f88 callback; the exact
struct offsets `cube_21BF88`/`cube_21BC90` use internally (medium
confidence per the forks - the ported code reproduces the algorithm, not
necessarily byte-identical intermediate struct layout); the untraced
`vif1SetClamp` call `DrawLightsAndCubes` should have between `DrawLights`
and `DrawCube` (still just a comment, not added); the real 4-way texture
selector (`CubeTextureFuckery`) simplified to always use the captured
buffer, with `TEXID_REF` set only so the surrounding alpha-blend state
matches what the rest of the file expects.

**Real bug found and fixed before any PCSX2 test** (self-caught, not
reported by aap): while double-checking `CubeCaptureBuffer()` against
`vif1SetFramebuffer()`'s own definition, realized it redirects
`SCE_GS_FRAME_1`/`SCE_GS_SCISSOR_1` to `extraBuf2` for the capture blit
and never pointed them back afterward. Those are the *same physical GS
registers* the real screen's draw target uses - `main.c`'s `StartFrame()`
sets them once per frame via `sceGsPutDrawEnv` (a GIF-direct write off
`db.draw0`/`db.draw1`, chosen by `evenOddFrame`), and `vif1SetFramebuffer`
overwrites them through a completely different path (a plain VIF1
packet), so the two don't know about each other - whichever wrote last
wins. Left unfixed, every cube face `DrawCube()` draws right after
`CubeCaptureBuffer()` returns would have rendered into `extraBuf2`
instead of the visible screen - not a subtle artifact, the cubes simply
would not have appeared at all.

**Fix**: `CubeCaptureBuffer()` now ends with a second `vif1SetFramebuffer`
call (`clear=0`, so it only rewrites `FRAME_1`/`SCISSOR_1`, confirmed from
`vif1SetFramebuffer`'s own body that this is side-effect-free) pointed at
this frame's real draw buffer - `db.draw0.frame1` or `db.draw1.frame1`
(`FBP`/`PSM` fields, from `eestruct.h`'s `sceGsFrame` bitfield layout) per
`evenOddFrame`, same selector `main.c` itself uses. `db` and
`evenOddFrame` are both already `extern`-declared in `inc.h`. Rebuilt
clean after the fix.

**Status**: cube port is structurally complete and builds clean, but has
NOT yet been visually tested in PCSX2 - that's the next step.

## 2026-08-25 (same session, continued): aap tested, cubes invisible - found the real half-extent, and a self-caught false alarm along the way

aap: "cool, not seeing any cubes yet unfortunately."

Before asking for a savestate, re-checked the cube port's own "verified"
claims against fresh disasm, since the debugging-methodology lesson from
earlier this session (check ground truth before trusting a prior pass's
confidence) applies to THIS session's own work too, not just old ones.
Two things happened:

1. **False alarm, corrected before it caused damage**: a scan for an
   8-vector literal corner table near the real `cubeFaceVerts`/
   `cubeFaceNormal` addresses found a plausible-looking ±2.64 table at
   0x27ec50. It failed a planarity check against 4 of the 6 real
   `cubeFaceVerts` faces - correctly NOT trusted or ported. Root cause
   found by tracing the actual `sceVu0ClipAll` call's real argument
   register instead of pattern-matching: the real corner-points buffer
   is 0x27b370, zeroed BSS - computed fresh every frame, not read from a
   literal table at all. 0x27ec50 was an unrelated coincidental match.

2. **Real bug found and fixed**: a fork traced the real corner-generator
   (`sub_21b690`, 0x21b690 - previously mis-labeled in this doc as pure
   "flare and illegal stuff" init; it's actually a shared helper, also
   called from the unrelated `DrawIllegalCube`) and fully decoded its 32
   `swc1` stores. The real per-corner sign pattern is EXACTLY what this
   port already had (`bit0`=X, `bit1`=Y, `bit2`=Z, standard binary-
   counting order) - but the half-extent is a real runtime float
   (0x2a714c, confirmed by direct read = 1.8), not the 1.0 this port
   assumed. Volume ratio 1.8^3/1.0^3 ~ 5.8x - a cube about 2.5x too
   small linearly, easily enough to be imperceptible against a 640x224
   screen at the ~130-165-unit anchor depths this file uses. Fixed:
   `cubeCorners` now uses a `CUBE_HALF_EXTENT` (1.8f) constant instead of
   bare 1.0f. Face-normal table re-validated clean against the corrected
   corners (all 6 real `cubeFaceVerts` faces are planar with normals
   matching `cubeFaceNormal`'s real values in the port's existing order -
   no pairing bug, contrary to a mid-investigation worry). Rebuilt clean.

**Flagged, not fixed (medium confidence, real but likely cosmetically
minor)**: the real matrix pipeline is a cheaper approximation this port
doesn't replicate - real code computes `transformed = cameraMatrix *
rotated` (rotation only, translation NOT baked in) and applies a single
shared `1/w` (from one perspective-divide of just the cube's anchor
point) to all 8 rotated corner offsets, rather than a proper per-vertex
divide. This port does a full proper per-vertex `ClipAll`+
`sprTransformVertex` with a translation-inclusive matrix. For an object
this small relative to camera distance these should look nearly
identical; not chased further given the half-extent bug is the much
stronger, fully-confirmed explanation for "no cubes visible" - the loop's
remaining `ApplyMatrix` calls (how the anchor's screen position combines
back in) were not fully decoded.

**Status**: half-extent bug fixed, builds clean. Not yet re-tested by
aap - that's the immediate next step.

## 2026-08-25 (same session, continued): still invisible after the half-extent fix - bisected to CubeCaptureBuffer, real ROM's own mechanism stays unresolved

aap: "hm. still no cubes. can we draw them in bright red maybe? so
they're easy to spot. maybe the xform is off"

Added `CUBE_DEBUG_RED` (opening.c): forces every cube face to flat,
fully-opaque, untextured bright red, no blending, no Z-test, and added an
independent hardcoded reference rectangle in `DrawLightsAndCubes` (drawn
via the already-trusted `vif1SetFlatRect()`, no dependency on any cube
code at all) - to separate "shared draw mechanism broken" from "cube-
specific transform/clip/capture broken." Then bisected round by round
(each a real PCSX2 test, not a guess):

1. Both cull-check and `CubeCaptureBuffer()` bypassed: reference square
   AND red cube shapes both visible - shared draw mechanism is fine, real
   transform output does land on screen when nothing else interferes.
2. Cull check re-enabled, `CubeCaptureBuffer()` still skipped: cubes
   still visible - `sceVu0ClipAll`'s cull result is NOT the problem.
3. `CubeCaptureBuffer()` re-enabled (cull respected): reference square
   still visible, **cubes gone** - confirmed, definitively, empirically:
   `CubeCaptureBuffer()` is the actual cause.

Since the reference square (drawn to the real screen just before cube
code runs) survives step 3, `CubeCaptureBuffer`'s redirect-to-`extraBuf2`
itself is NOT hitting the real screen (correct) - but the restore-back
step added when the framebuffer-restore bug was first found and fixed
must be landing somewhere wrong (not extraBuf2, not the real screen -
some third, incorrect target), since simply never calling
`CubeCaptureBuffer()` at all (step 1/2) is empirically equivalent to a
*working* restore, and the current explicit restore code is not.

Two more forks tried to find the real ROM's own answer directly (not
guess ours): first found the definitive fact that neither `sub_21c7a8`
(the capture function itself) nor `DrawCube` (0x217520, all 6
faces/2 passes) nor `DrawLightsAndCubes` (0x217db8, all 5 cube
instances) ever call `vif1SetFramebuffer` a second time anywhere - no
restore inside that whole call chain. A second fork extended the search
much further (checked `DrawOpeningScene`, `DrawOpening`,
`DoOpeningIllegal`'s frame loop, `DrawEnd` and its callees, the
extraBuf1/extraBuf2 ping-pong blur function, `CubeTextureFuckery`'s own
`vif1SetFramebuffer` call - confirmed that one just re-asserts the SAME
extraBuf2 target, not a restore), ruled out a raw non-wrapped register
write anywhere in that graph, ruled out extraBuf1/extraBuf2 overlapping
the real screen buffers in VRAM (confirmed non-overlapping via the
allocator's own reservation order), and ruled out (medium confidence)
a GS dual-display-circuit hardware-compositing explanation - `main.c`'s
own `sceGsSetDefDBuff`-based double-buffer setup is the only PMODE/
DISPFB-adjacent configuration found, nothing OSDSYS-specific. **Bottom
line: by every static code path checked, the real ROM's `SCE_GS_FRAME_1`
genuinely stays pointed at the offscreen capture buffer for the rest of
the frame, by every path we could trace - contradicting real hardware
clearly showing cubes. This is a real, unresolved mystery**, most likely
needing a live PCSX2 GS-register/VRAM check (not available this session)
rather than further static disassembly.

**Pragmatic fix applied** (not a real solution to the mystery): stopped
calling `CubeCaptureBuffer()` from `DrawCube` for now. Justified beyond
just "it's empirically what works" - `DrawCubeFace`'s mode=1 (refraction)
pass doesn't actually sample `extraBuf2` dynamically in the current code
anyway (checked while investigating: it only sets ST+RGBAQ, the TEX0_1/
TEX1_1 override an earlier comment claimed it did doesn't exist - it
just draws whatever static `TEXID_REF` texture `DrawCube` bound before
the pass), so the capture buffer isn't wired to anything visually
meaningful yet regardless. `CubeCaptureBuffer()` itself is NOT deleted -
left in place, unused, for whenever the restore-target bug (or the real
ROM's own mechanism) gets solved. Also disabled `CUBE_DEBUG_RED` (set to
0) so the next test shows real lit geometry, not solid red. Rebuilt
clean.

**Status**: should now show real (non-debug) cube geometry with real
per-face lighting and the lit-base pass only meaningfully connected (the
refraction overlay pass still runs but samples a static placeholder
texture, not a real capture) - not yet tested by aap.

## 2026-08-25 (same session, continued): DrawToExtraBuf2/DrawExtraBuf2 fully traced and ported - the real frame-capture ping-pong

aap: "continue with the cubes? that refraction i'm really excited about."
Prerequisite work: `DrawToExtraBuf2`/`DrawExtraBuf2` (the per-frame
"save"/"draw back" pair that's supposed to feed the cube's own
`CubeCaptureBuffer`) were still empty stubs - `extraBuf1` never had
real content, so even a working `CubeCaptureBuffer` would have nothing
meaningful to sample. Both fully traced from real disasm this pass and
ported; `CubeCaptureBuffer`/`DrawCube`/`DrawLightsAndCubes` intentionally
left untouched (separate parallel work on the `SCE_GS_FRAME_1` restore
mystery).

**`DrawToExtraBuf2`** (0x214050, "save current frame" half) - HIGH
confidence, direct disasm: `vif1SetXYOffset(0,...)` (disable interlace
half-pixel jitter for this internal buffer), `vif1SetZWrite(0)`,
redirect `SCE_GS_FRAME_1` to one buffer with `clear=1`, read from the
OTHER buffer via a frame-parity-selected `TEX0_1`, squeeze the FULL
source rect into a HALF-WIDTH dest rect (same `screenW/2` halving idiom
used elsewhere in this file - confirms an earlier session's "half-width
dest" guess was right all along, just for a different call than the one
it got attributed to at the time), then `vif1SetZWrite(1)` and
`vif1SetXYOffset(1,...)` to restore the normal jittered offset before
returning.

The real ping-pong's frame-parity `TEX0` read address is a literal `0`
or `(screenW*screenH)>>6`, not an offset added to any runtime base we
could find in the disassembly - this is EXACTLY the block-count gap
between our own sequentially-`gsAllocBuffer`'d `extraBuf1`/`extraBuf2`
(same `>>6` formula `gsAllocExtraBuffers`/`gsAllocPtr` already use), so
it ports cleanly onto our two buffers even though the real ROM's own
addressing looked absolute rather than base-relative. MEDIUM confidence
on WHY the real values are absolute (a fixed low VRAM allocation for
this specific pair seems likely - would need a live VRAM layout check to
confirm) but HIGH confidence the ported behavior (alternate which of
`extraBuf1`/`extraBuf2` is written vs read each frame, keyed on
`evenOddFrame`) is functionally equivalent for our purposes.

Two things NOT ported as genuine simplifications, not oversights: real
code also does a second `vif1SetFramebuffer(clear=1)` at the end,
clearing the buffer just used as the read source - skipped as redundant
with this same function's own top-of-function clear the next time that
buffer is written (every write path already clears first). Real `TEX0`'s
height exponent is a fixed literal `9` (512), not `GetTexExponent(screenH)`
(which gives 8 for 224/256) - ported with `GetTexExponent` for
consistency with the rest of the file, since it only affects
texture-wrap addressing outside the rect we actually draw, MEDIUM
confidence this doesn't matter visually.

MEDIUM confidence, not independently confirmed: the real `XYOFFSET`
halfpx argument (real address `0x1f0c44`) is assumed to be this ROM's
own `evenOddField`-equivalent - matches its exact "half-pixel interlace
jitter" role everywhere else this file already uses `evenOddField`.

**`DrawExtraBuf2`** (0x214240, "draw the saved buffer back" half) -
previously thought completely untraced; actually HIGH confidence now,
direct disasm: temporarily disables Z-test and Z-write, reads from
whichever buffer `DrawToExtraBuf2` just wrote THIS frame (the mirror
image of that function's read/write pair), stretches the HALF-WIDTH
saved capture back out to a FULL-WIDTH dest rect (the exact inverse of
`DrawToExtraBuf2`'s squeeze), and blends it onto the real screen via
`vif1SetAlphaBlend(1,2,80)` with alpha blending genuinely enabled (real
arg is `-1`/nonzero i.e. true, unlike `DrawToExtraBuf2`'s opaque `abe=0`
copy). Z-test/write restored before returning.

This is a real, deliberate translucent composite of recent frames drawn
back into the scene every single frame - a real, evidence-backed
candidate for the radial-brightness-falloff lead flagged much earlier
this project (previously just a name with an unconfirmed mechanism, now
an actual traced blend operation). Not re-litigating that closed
investigation here since aap asked to leave it alone at the time - noting
this only because the mechanism is now concretely understood, not to
reopen the question uninvited.

**Also checked**: `sub_2144c0` (0x2144c0, called from `sub_218b20`, one
of `DrawOpeningScene`'s two tail "trail functions") - confirmed (via its
own `vif1SetFramebuffer` calls and a literal reference to the same real
`extraBuf2`-role address `sub_21c7a8`/`CubeCaptureBuffer` uses) that it's
a THIRD stage of this same buffer system: a multi-pass ping-pong loop
between two buffers, matching an earlier fork's independent finding "a
ping-pong blur effect confined entirely within the extraBuf1/extraBuf2
pair." NOT ported this pass - it's a softening/blur refinement on top of
the core capture-and-composite mechanism now ported above, not required
for basic refraction to work, and lower priority than unblocking
`CubeCaptureBuffer` itself. Flagged as a real, scoped follow-up.

Builds clean. Not yet visually tested (this alone doesn't fix cube
refraction - `CubeCaptureBuffer` is still disabled pending the separate
`SCE_GS_FRAME_1` restore investigation - but `extraBuf1`/`extraBuf2` now
carry real per-frame content for whenever that's resolved, and the
additive-ish background composite from `DrawExtraBuf2` should be visible
on its own even with cubes' own capture step still off).

## 2026-08-25 (same session, continued): the framebuffer-restore mystery solved - it's a harness-side race, not a real-ROM mechanism

Enabling `EXTRABUF_PING_PONG` (the newly-ported `DrawExtraBuf2`/
`DrawToExtraBuf2`) to sanity-check it didn't regress anything else
instead produced real corruption (aap: "look very broken and glitchy" -
background gone, striped garbage in place of towers/fog). Confirmed the
"never restores FRAME_1, yet works on real hardware" pattern found for
`CubeCaptureBuffer` is NOT cube-specific - reverted both to no-op stubs
(behind the toggle, code kept) to get back to known-good.

Rather than keep chasing this through more static disassembly, went
empirical: extracted `eeMemory.bin` and `PCSX2 Internal Structures.dat`
from a savestate taken mid-corruption, computed `sceGsDBuff`'s real field
offsets via a tiny `ee-gcc`-compiled probe (`offsetof` on the real SDK
struct - `draw0`@0x60, `draw1`@0x150 within `db`, confirmed
`sizeof(sceGsFrame)`=8, matching the real GS hardware register shape
exactly), and searched the savestate's internal GS state for a word
shaped like `FBW=10,PSM=0` (this port's real screen format) to find the
GS's actual live `SCE_GS_FRAME_1` value directly - not available in
`GS.bin` itself (that turned out to hold VRAM + a small privileged-
register/CSR-ish header, not the drawing-context registers; `PCSX2
Internal Structures.dat`, previously unused this project, has PCSX2's own
internal `GSState`, which does).

**Result**: at that moment, `evenOddFrame` (read from `eeMemory.bin`) was
0 - which by this file's own selection convention (`evenOddFrame==0 ?
draw0 : draw1`, matching `main.c`'s `SwapBuffers`/`sceGsPutDrawEnv`
convention exactly) should mean `db.draw0` (FBP 70) is the active target.
But the GS's actual live `SCE_GS_FRAME_1` read FBP 0 - `db.draw1`'s value,
the OTHER buffer. Since towers/fog visibly render correctly in this same
savestate using nothing but whatever `SCE_GS_FRAME_1` already is (no
redirect of their own), FBP 0 must be the genuinely correct, currently-
displayed buffer - meaning `evenOddFrame`, read fresh at an arbitrary
point mid-frame, can already reflect a NEWER value than what the GS was
actually last told to use.

**Root cause**: `main.c`'s `evenOddFrame` is flipped by `SwapBuffers()` on
a separate thread (`SwapThread`, woken by the vblank handler), synced
with the render thread only via the `WaitNextFrame()`/`StartFrame()`
semaphore handshake at the TOP of each frame - nothing stops it from
being read again, mid-frame, well after that handshake, by code (like
`CubeCaptureBuffer`'s restore, or `DrawToExtraBuf2`/`DrawExtraBuf2`'s
buffer selection) that assumes it's still the value that was true when
the frame's `sceGsPutDrawEnv` last ran. This is a property of aap's own
simplified test-harness threading model, not something in the real ROM -
plausibly only started mattering once cube/extrabuf rendering made a
single frame's draw-call count (and wall-clock cost) large enough for
this window to matter.

**Fix**: added `stableEvenOddFrame`/`stableEvenOddField` (declared near
`extraBuf1`/`extraBuf2`), captured once per frame at the top of
`Process()` (the first thing every frame does, right after the last
`WaitNextFrame()` returned - the one point in the loop the handshake
actually guarantees these values are current). `CubeCaptureBuffer`'s
restore and `DrawToExtraBuf2`/`DrawExtraBuf2`'s buffer selection now read
these stable snapshots instead of the live globals. Re-enabled
`CubeCaptureBuffer()`'s call in `DrawCube` and `EXTRABUF_PING_PONG`.

**Also wired up real dynamic refraction sampling**: `DrawCubeFace`'s
mode=1 pass previously bound a static named texture (`TEXID_REF`) that
never actually got overridden despite a comment claiming otherwise (an
earlier session's stale claim, corrected in passing) - added
`CubeSetRefractionTexture()` (mirrors `CubeCaptureBuffer`'s own
`extraBuf1` read, same `TEX0_1`/`TEX1_1` pattern) so the refraction pass
now genuinely samples `extraBuf2`, the buffer `CubeCaptureBuffer` just
composited, via the cube's own screen position as UV (already-existing
ST computation, the standard cheap screen-space refraction trick).

Rebuilt clean, no warnings (the long-standing "`CubeCaptureBuffer`
defined but not used" warning is finally gone too, now that it's called
again). Not yet re-tested by aap - this is the first build where the
whole chain (per-frame capture -> cube-side composite -> dynamic
refraction sample) is actually wired end to end.

## 2026-08-25 (same session, continued): PCSX2's GS dump feature settles the mystery differently than expected - it's a packet-pipeline timing issue, not an FBP value

Two more restore attempts (inverted convention, then hardcoded `db.draw1`)
both still showed "no cubes visible." aap found and used PCSX2's GS dump
debugging feature (a complete per-draw-call trace of every real GS
command, in `osdbits/gsdump/` - not committed, regenerated per test) -
far richer ground truth than savestate register snapshots, and the tool
that finally cracked this.

**First finding (a fork's systematic scan, alpha-value fingerprinting):**
`DrawCubeFace()`'s two RGBAQ alpha constants (64 for the lit pass, 48 for
the refraction pass - both distinctive, used nowhere else) appear NOWHERE
in a 610-draw dump. `DrawCubeFace` never executes, full stop - the whole
FBP-restore investigation had been chasing the wrong layer.

**Second finding (a savestate cross-check on the exact same build):** all
8 corners of the cube's actual transformed geometry, hand-simulated in
Python against `sceVu0ClipAll`'s real algorithm (disassembled from
`libvu0.a` - a proper 4x4 row-vector transform then a homogeneous
`point.xyz` vs `clipMin/clipMax.xyz * point.w` test, X/Y/W only, Z is NOT
tested) pass every bound comfortably. Culling is not blocking this
instance's visibility either.

**Third finding (a second fork, exhaustive ordered walk through the
614-draw dump, not spot-checking):** `CubeCaptureBuffer()`'s capture blit
(a very distinctive full-640x224, `TME=1 ABE=0` white sprite) DOES appear,
consistently, once per frame - meaning ONLY ONE of the 5 cube instances
per frame passes the cull test (a separate, real issue, not investigated
further this round), and confirming `CubeCaptureBuffer` really does run.
The double-buffer swap (`db.draw0`/`db.draw1`, FBP 70/0) DOES alternate
correctly frame-to-frame in this dump - the earlier "always reads
db.draw1" conclusion was a 3-sample coincidence from manually-triggered
savestates, not a real stuck-buffer bug. But after EVERY SINGLE
`CubeCaptureBuffer` occurrence in the whole dump, the next draw is
unrelated content (clears/towers/fog) - never a cube face. A fresh,
careful re-run of the alpha-64/48 search (and a direct scan for ANY
4-vertex TRISTRIP anywhere in the dump) confirms zero cube-face draws
exist, dump-wide.

**Fourth finding (direct compiled-code inspection, not just C source
reading):** disassembled `DrawCube`'s ACTUAL machine code from our own
`main.elf` (`ee-objdump`) and traced it instruction-by-instruction against
the C source - a faithful, correct compilation. The pass/face loop
structure mathematically guarantees `DrawCubeFace` gets called for face 0
in whichever pass matches `faceSign[0]`, immediately after
`CubeCaptureBuffer()` returns - no compiler bug, no dead code, no missed
branch. If `CubeCaptureBuffer()` genuinely runs (confirmed by finding 3),
the compiled code cannot skip the face loop.

**Given the C logic is provably correct and the culling/geometry is
provably fine, the remaining explanation points at the packet/DMA
pipeline itself, not application logic.** `CubeCaptureBuffer()` fires 8
back-to-back `vif1Begin()`/`vif1End()` packets (framebuffer redirect,
Z-test/write, alpha blend, TEX0/TEX1 x2, the capture TexRect, the
restore) with zero synchronization between them - each `vif1Begin()`
alternates between only 2 fixed packet-buffer slots
(`vifPackets[2]`/`sprChains[2]`, confirmed by reading the code directly:
`vifPacketBit ^= 1` / `sprChainBit = !sprChainBit` every call, no wait for
the DMA on the slot being reused to have actually finished). This is
exactly the class of bug the tower subsystem already hit and fixed this
session (`sendDma()`'s `FlushCache` vs the real per-tower kick's
`sceGsSyncPath` - see the fifth tower result above) - DrawTowers now
explicitly separates "kick" from "sync" for precisely this reason, but
`CubeCaptureBuffer`/`DrawCube` never got the same treatment since it was
ported assuming (incorrectly, it now looks like) that fire-and-forget
`vif1Begin/End` bursts are always safe. Scratchpad allocator sizing was
checked and ruled out separately (`sprAllocChains()` gets roughly 7.5KB
per chain buffer after `InitSPR`'s permanent allocations - comfortably
larger than any single packet this file builds).

**Fix attempted, not yet tested:** added `sceGsSyncPath(0, 0)` right after
`CubeCaptureBuffer()` returns, before the face-drawing loop starts -
matching the pattern already proven to fix an identical-shaped bug in
towers. Also reverted the FBP-restore convention back to a proper
`stableEvenOddFrame`-selected pick (matching finding 3's confirmation that
double-buffering genuinely alternates) instead of the hardcoded
`db.draw1` from the previous (wrong-premised) round. Rebuilt clean.

## 2026-08-25 (same session, continued): the real bug - extraBuf1/extraBuf2 are in the wrong unit for FRAME.FBP

aap tested the sync fix: identical "no cubes" result, and a byte-for-byte
identical GS dump draw sequence before and after adding `sceGsSyncPath` -
conclusively ruling out packet-pipeline timing as the cause.

Went back to the dump to re-verify the `CubeCaptureBuffer` signature
identification, more carefully this time (checking `TEX0.TBP0`/`PSM`, not
just the destination rect/color) - and found the earlier identification
was wrong: what looked like `CubeCaptureBuffer`'s blit (a repeating,
evenly-spaced full-screen white sprite targeting FBP 210) actually reads
from `TBP0` values matching `db.draw0`/`db.draw1` in `PSM_C24` format -
not `extraBuf1` in `PSMCT32` at all. A precise search for the REAL
signature (destination FBP matching `extraBuf2`, source `TBP0` matching
`extraBuf1`, `PSM_C32`) found nothing anywhere in the dump.
`CubeCaptureBuffer()` was never actually running either - the whole
packet-pipeline-timing investigation had been chasing a misidentified
draw call.

Settled the actual unit question empirically instead of by further
formula-derivation (which had produced contradictory answers on two
earlier attempts): read a real, definitely-working texture's
`gsAllocBuffer`-derived `tbp[0]` from a savestate (`textures[TEXID_WAL0]`,
the tower wall texture) and found it in the dump's `TEX0.TBP0` for an
actual tower draw call, EXACT match, no scaling (9344 = 0x2480 both
sides). Cross-referenced against `db.draw0`'s real `FRAME.FBP` (70,
independently confirmed correct - a 640x224 PSMCT32 buffer needs exactly
573440/8192 = 70 pages in the GS's real 8192-byte/2048-pixel page unit)
and `gsAllocBuffer`'s own allocation delta for two same-sized 640x224
buffers (2240, matching its `(w*h*bpp)/8/256` formula) - the ratio is
exactly 32. **`gsAllocBuffer` (and therefore `extraBuf1`/`extraBuf2`)
returns values in 256-byte-block units - correct to use directly for
`TEX0.TBP0` (a 14-bit field covering all of VRAM at that granularity,
confirmed by the exact match above), but `FRAME.FBP` is a 9-bit field
using the 32x coarser 8192-byte page unit.** `CubeCaptureBuffer()`'s
`vif1SetFramebuffer(extraBuf2, ...)` call was passing the raw 256-byte-
block value (6720) directly as `FBP`, 13x over that field's 9-bit range
(max 511) - silently wrapping/corrupting into some unrelated VRAM
location instead of the intended scratch buffer, which is exactly why
the whole capture step (and everything meant to follow it) never showed
up anywhere recognizable in the GS dump.

**Fix**: divide by 32 (`extraBuf2/32`) at both `vif1SetFramebuffer` call
sites that target a `gsAllocBuffer`-derived buffer -
`CubeCaptureBuffer()` and the still-disabled `DrawToExtraBuf2()`. The
TEX0.TBP0 reads (`extraBuf1` in `CubeCaptureBuffer`, `extraBuf2` in
`CubeSetRefractionTexture`) were already correct as-is, no change needed
there - only the FRAME-targeting calls had the unit mismatch. Rebuilt
clean. This is the first fix in this whole investigation with a
complete, verified, non-contradictory explanation for every previously
confusing observation (the misidentified draw signature, the "packet
timing had zero effect" result, the wrapped/overflowing field) - high
confidence this is the actual root cause, not yet confirmed visually.

## 2026-08-25 (same session, continued): the /32 fix was real, but not sufficient - direct instrumentation plus a fresh GS dump finally pin the actual bug

Still no cubes after the /32 fix. Added direct runtime instrumentation
(`cubeCullResult[5]`, `cubeFacesDrawn[5]`, read straight from a savestate)
rather than more disassembly inference: `cubeCullResult = (0,0,0,0,0)`
(nothing culled), `cubeFacesDrawn = (6,6,6,6,6)` (every face drawn, every
instance, every frame). Also disassembled `sceDmaSync` directly from
`libdma.a` and confirmed `vif1End()`'s `sceDmaSync(chVIF1,0,0)` (mode=0)
genuinely blocks on the DMA channel before every send - ruling out packet
corruption from rapid succession. Checked `frameCount` vs `vblankCount`
from a savestate (both exactly equal) - ruling out a render-thread-
falling-behind-realtime race too. Every C-level layer confirmed clean.

A fresh, tightly-scoped GS dump search (fork) then found where the
packets actually go: cube-shaped geometry (24-vertex tristrips, matching
UV/color/screen-position pattern for a small rotating object) genuinely
renders every frame - but its `FRAME.FBP` is `extraBuf2`, never the real
screen, in all 18 occurrences checked. The capture-buffer redirect
(correct destination now, post-/32-fix) and the restore-back-to-real-
screen (also correctly computed, per the code) are BOTH present and
BOTH get sent as packets in the right program order - but the restore
isn't taking effect before the face-drawing loop's own packets execute.

**Likely explanation**: `vif1End()`'s `sceDmaSync` only guarantees the
EE-to-VIF1 DMA transfer completed, not that the GS has finished actually
DRAINING/EXECUTING everything already queued (particularly the capture
blit, a full 640x224 texture read - real GS work, not just a tiny
register write). The restore packet can be sent and even received by
VIF1/GIF while the blit is still executing on the GS core; without an
explicit wait for GS-side completion, there's no guarantee it's applied
before the face loop's own packets land right behind it in short order.

**Fix attempted**: moved `sceGsSyncPath(0,0)` from after the restore to
BEFORE it - wait for the capture blit to fully finish, then redirect back.
The earlier "sceGsSyncPath had zero effect" result (previous section)
doesn't invalidate this - that test happened before the /32 fix, when the
redirect itself still landed in the wrong place for an unrelated reason,
so it was never a real test of sync placement. Rebuilt clean, not yet
tested.

## 2026-08-26: SOLVED - the "invisible cubes" root cause was an empty SCISSOR, not FRAME/sync/DMA at all

The sync-before-restore fix above also had no effect ("oof, still no
cubes"). Starting over with fresh eyes, the previously-dismissed binary
single-frame dump (`frame.gs`, snapped by aap from the /32-fix build)
turned out to be parseable after all - and decisive. It is PCSX2's raw
GS dump format: header + serialized GSState + a raw, in-order stream of
every GIF transfer the GS received (2004 transfers over 4 frames here).
Decoding every GIF tag and A+D write in that stream showed, per frame,
per cube instance:

- `FRAME_1 -> FBP=210` (extraBuf2, correct post-/32 value) - ARRIVES.
- The capture blit sprite (UV 0..640/0..224, correctly XYOFFSET-adjusted
  window coords, `TEX0=0x1180`) - ARRIVES.
- `FRAME_1` restore to the correct per-frame parity buffer (FBP=0/70,
  alternating properly) - ARRIVES.
- Both face-draw passes per face - tristrips with *sane on-screen
  coordinates* (a ~40px cube around pixel (430..480, 101..123)), correct
  lit colors (`c,c,c,64` / `96,96,96,48`), correct blends - ALL ARRIVE,
  in program order.

So the entire 2026-08-25 mystery ("packets vanish", "restore not taking
effect", "DMA pipeline timing") was chasing a ghost: everything reaches
the GS correctly. What kills it is one register value visible in that
same stream: the `SCISSOR_1` written by `vif1SetFramebuffer` decodes to
**SCAX0=0, SCAX1=0, SCAY0=639, SCAY1=223** - an empty region (x in
[0,0], y in [639,223], inverted). Root cause in the source, a classic
macro-argument-order footgun:

    SCE_GS_SET_SCISSOR(scax0, scax1, scay0, scay1)   // SDK order: x0, x1, y0, y1
    ...SET_SCISSOR(0, 0, width-1, height-1)          // called as if x0, y0, x1, y1

Every pixel of every draw between any `vif1SetFramebuffer` call and the
next frame's `sceGsPutDrawEnv` (which rewrites SCISSOR correctly) was
scissored away. This explains, with zero contradictions, every symptom
of the whole investigation:

- cubes invisible exactly when `CubeCaptureBuffer()` runs (its redirect
  AND its restore both pass through the broken scissor write) - even in
  `CUBE_DEBUG_RED` mode (scissor kills red pixels too);
- cubes fine with capture disabled (nothing touches SCISSOR mid-frame);
- the reference rectangle surviving (drawn before the first capture);
- `EXTRABUF_PING_PONG`'s "broken and glitchy" corruption (its
  `vif1SetFramebuffer` call runs EARLY in the frame, so fog/lights/cubes
  after it all vanished - looked exactly like "background gone");
- zero cube draw records in the folder-based GS dumps: PCSX2 rejects
  fully-scissored prims before creating a per-draw record (verified:
  per-frame `frame_stats.txt` `Draw:` counts exactly match the files
  written - the dump was never "sampling", the GS genuinely made no
  cube draws);
- late cube A+D writes still arriving (register writes aren't pixels -
  the folder dump's next-frame-clear context showed `TEX0=0x1a40`
  (extraBuf2, only `CubeSetRefractionTexture` sets that) and
  `ALPHA={0,2,0,1} FIX=64` (the mode-1 face blend) as residue);
- every FRAME-restore variant and sync placement doing nothing (the
  restore logic was correct the whole time);
- and why the real ROM has no such problem: this is a transcription bug
  in the harness's own `vif1SetFramebuffer` (previously dead code -
  nothing exercised it until the cube/extrabuf work).

**Second real bug found in the same pass** (also dump-proven): the
folder dump's residual `ZBUF` showed `ZBP=0x1180` = 4480 blocks =
`extraBuf1`. `gsInitAlloc()` started its VRAM heap at
`2*(screenW*screenH)/64`, i.e. right after the two colour buffers - but
`sceGsSetDefDBuff` places the PSMZ32 Z buffer there (pages 140..210).
So `extraBuf1` WAS the live Z buffer. Fixed: heap base is now
`3*(screenW*screenH)/64` (everything shifts up 70 pages; textures still
fit comfortably in the 512-page VRAM).

**Changes made** (built clean, awaiting aap's visual test):
1. `vif1SetFramebuffer`: scissor args fixed to `(0, w-1, 0, h-1)`.
2. `gsInitAlloc`: heap base past the Z buffer.
3. `CubeCaptureBuffer`: removed the `sceGsSyncPath(0,0)` workaround
   (PATH2 is strictly ordered; the dump proves the sequence arrives in
   program order) and corrected the stale wrong comments here,
   in `DrawCube`, and at `EXTRABUF_PING_PONG`.

**Expected on next test**: cubes visible again (lit translucent grey,
like the known-good capture-disabled build). The refraction overlay
samples a copy of `extraBuf1`, which is now stable-but-uninitialized
VRAM (black in PCSX2 => the mode-1 pass modulates to nothing), so no
garbage overlay is expected either.

**Known remaining work for REAL dynamic refraction** (deliberately not
in this build, one verified step at a time): re-enable
`EXTRABUF_PING_PONG` after giving `DrawToExtraBuf2` (a) a FRAME_1
restore at the end (it currently redirects and never restores - fine
for the real ROM somehow, still untraced, but fatal in this harness),
and (b) a `vif1SetZTest(0)/(1)` pair around its blit -
`vif1SetFramebuffer(clear=1)`'s tail leaves `TEST_1` at
ate=1/ATST_LESS/aref=0, which kills every pixel; `CubeCaptureBuffer`
clears that state via its own ZTest call but `DrawToExtraBuf2` doesn't.
Then (c) give the cube capture its own third buffer (it currently
composites into extraBuf2, which the ping-pong pair also uses as a
half), reading the parity-selected just-saved half-width buffer
de-squeezed (`&full <- &half`, the inverse of DrawToExtraBuf2's
squeeze - that de-squeeze is *why* the intermediate capture exists: it
lets the face pass sample with plain screen-position STs). Possibly a
`TEXFLUSH` before sampling a just-rendered buffer, if real hardware/
PCSX2 shows stale-texture-cache artifacts.

Tooling note for future sessions: the `frame.gs` parser is saved as
`docs/gsdump-gifdecode.py` (walks GIF tags + A+D writes; the packet
stream in that particular file started at offset 0x52e22d, found by
brute-force scan - the header's offsets are unobvious). This raw-stream view is strictly stronger
evidence than the folder-of-text-files dump for "did X reach the GS"
questions, since the folder dump only records draws that survive
scissor/setup rejection.

## 2026-08-26 (cont.): cubes confirmed back on screen; extrabuf roles corrected - no ping-pong, the save reads THE SCREEN

aap confirmed the scissor-fix build renders the cubes again ("hah! i see
the cubes again") - fresh savestate `main (363D369D).01.p2s` screenshot
shows towers + fog + orbs + all the big rotating cubes. The "env map is
wrong" (one cube sampling pink garbage, others black) was the expected
dead refraction source (feedback disabled, extraBuf1/2 = untouched VRAM).

Wiring the real source surfaced a misreading in the 2026-08-25 trace of
`DrawToExtraBuf2`: its frame-parity TEX0 read literals - 0 and
`(screenW*screenH)>>6` = 2240 blocks - had been mapped onto an
"extraBuf1<->extraBuf2 ping-pong" because their gap matched. But 0 and
2240 blocks are FBP pages 0 and 70: **`db.draw0`/`db.draw1`, the screen
buffers themselves.** The real function reads the screen as drawn so far
this frame (towers + `DrawExtraBuf2`'s composite - it runs between them
and fog/lights/cubes), parity-selected, and squeezes it half-width into
ONE fixed save buffer. There is no ping-pong. The two allocated
extrabufs are exactly what the cube code already assumed: extraBuf1 =
the save buffer, extraBuf2 = the cube capture's working copy. Feedback
loop: save_N = screen_N (incl. faded save_N-1) - the long-suspected
tower-glow/falloff mechanism, and no third buffer is needed after all.

**Changes in this build** (clean, awaiting test):
1. `DrawToExtraBuf2` enabled and corrected: reads the parity-selected
   screen buffer (`db.drawX.frame1.FBP*32`), writes fixed `extraBuf1`;
   plus the two fixes flagged earlier - `vif1SetZTest(0)/(1)` around the
   blit (clear=1's tail leaves TEST_1 at ate=1/LESS/aref=0) and a
   FRAME_1/SCISSOR restore stanza at the end.
2. `DrawExtraBuf2` enabled, src = fixed `extraBuf1`.
3. `CubeCaptureBuffer`: samples the save de-squeezed (`&full` dest <-
   `&half` tex, inverse of the squeeze - the reason the intermediate
   working copy exists: it makes plain screen-position STs valid).
4. `DrawCubeFace` mode-1 STs fixed to true screen-position sampling:
   `( (xy>>4) - XYOFFSET ) / 2^{tw,th}` - the old formula used raw
   window coords over a wrong scale (1.6x stretch + wrap offset), so it
   never sampled "behind the cube" even with a good capture. Real ROM's
   per-face scrolling-UV phase (0x257918) remains untraced; this is the
   clean screen-space baseline.
5. `EXTRABUF_PING_PONG` renamed `EXTRABUF_FEEDBACK` (=1) to stop the
   name lying about the mechanism.

**Expected on next test**: cube faces show a live, slightly-faded copy
of the scene behind them (screen-space refraction), towers gain the
soft feedback glow/falloff. First couple of frames may look dimmer
while the feedback loop warms up (save buffer starts empty). If things
look wrong, `EXTRABUF_FEEDBACK 0` isolates the new mechanism in one
toggle. Real-hardware caveat noted for later: no TEXFLUSH is issued
when binding just-rendered buffers as textures - PCSX2 doesn't need it,
real GS texture cache might.

## 2026-08-26 (cont. 2): "no effect" led to the FULL real cube pipeline - 10 passes, env textures, fresnel edge glow, all traced and ported

aap on the previous build: "cubes look very plain now, transparent but
no effect". Root cause: the port's 2-pass cube (invented lit pass + one
screen-space refraction sample) was a shadow of the real thing. Rather
than tweak it, the whole real pipeline was traced end to end:
`DrawCube` (0x217520), `CubeTextureFuckery` (0x21c400, the per-pass
target/texture/blend selector), `DrawTexturedQuad` (0x21c560),
`sub_21c7a8` (the capture), the three UV builders `cube_21B798/21BBE0/
21BA08`, the colour/ST callback (0x216f88), `vif1SetTexture_24_32`
(0x21c338), and `cube_21BF88`/`cube_21BC90` re-read completely.
Constants came from the real-BIOS savestate at the repo root (NB: an
earlier session's disasm reads of `lui 0x28 / addiu negative` addresses
as 0x28bxxx were off - they are 0x27bxxx, matching the long-known
0x27afe0/0x27b090 tables).

**The real mechanism, per cube instance:**
1. `sub_21c7a8`: stamp the CURRENT screen (towers, composite, fog,
   lights, any earlier cube) full-rect into the cube working buffer -
   sampled as PSMCT24 with TEXA{TA0=127, AEM=1}, colour {128,128,128,0}:
   non-black pixels get alpha 127, black gets 0. This manufactured
   "something is behind here" alpha is what the env layers key on.
2. Five passes composite the BACK faces INTO the working buffer:
   prev-frame screen at their screen positions (opaque, tinted), then
   scrolling env textures BLPR/REF/BLP/REF (textures[12]/[10]/[11] =
   TEXID_BLPR/REF/BLP - the actual "env map" content) with blends
   (1,5,128) and (1,8,42) - mode 8 = Cs*Ad+Cd, destination-alpha keyed.
3. Five passes draw the FRONT faces onto the SCREEN: the working buffer
   sampled back at screen positions (so background AND back faces show
   through - genuine two-depth glass), then the same four env layers.
4. Every pass sets FRAME itself and the sequence ends on screen passes -
   which is why the real ROM never "restores" FRAME_1 (2026-08-25's
   mystery, now closed for good).

Key sub-mechanisms (all disasm-verified):
- Per-vertex "light" = |dot(faceNormal, viewDirToVertex)| - a
  view-angle/fresnel term, NOT a light direction (the old port guessed
  light1). Downstream colour term e=(1-l)^2/2 peaks at silhouette
  edges: the glassy edge glow.
- UV builder 21B798 (screen-sampling passes): pixel position + the
  screen-projected face normal scaled by q*4*(w/2,h/2) (the refraction
  offset, depth-scaled) + centre-relative zoom, clamped to [1, w-1].
- UV builder 21BA08 (REF passes): normalize(camera-space corner) +
  projected normal * arg, xy swapped, +0.5 - classic fake env map.
- UV builder 21BBE0 (BLP/BLPR passes): unit quad UVs +- a scroll phase.
  The five phase values (805.0, 0.04, 0.001, -0.0178, 2.159994) are
  CONSTANTS (identical in two independent runtime captures; no writer
  in the whole disasm).
- Colour modes ((0x216f88): flat 128 for env layers; mode 2 =
  (base + e*32)*fix/128; mode 3 = base*(+-e*1e-4*pi + 2pi | e*1e-4*0.02
  + 1.2)*fix/128, clamped [0,128]. Base = per-face table 0x27b3f0 =
  {112,112,152} x5 + black bottom face. The pass's +0x40 "blend fix"
  field doubles as that /128 brightness: 122 (back) / 240 (front).
- PRIM: flat-shaded (IIP=0) tristrips, ABE per pass, AA1 on the
  screen-sampling passes, A=128 always, Q=1 for screen-ST passes /
  perspective q for env passes. All faces of a group in ONE packet.
- `DrawLightsAndCubes` really calls `vif1SetCLAMP_1(1,1,0,0,0,0)`
  between DrawLights and the cube loop (the "vif1SetClamp(?)" gap).
- Z: test AND write disabled for the whole cube stack (restored after).
- The 0x257918 "mystery function" in the callback is just s64->float
  conversion (soft-float helper), nothing exotic.

**Port**: all of the above is now in `opening.c` as a 10-entry
`cubePasses[]` table + `CubeSetupPass`/`CubeComputeUV`/`CubeVertexColor`/
`DrawCubePass`, real `CubeCaptureBuffer`, extended `CubeTransformAndClip`
(world corners, camera-space view dirs, per-corner q, screen centre) and
`CubeFaceSetup` (screen-projected normal, fresnel light). The cube no
longer reads the DrawToExtraBuf2 save at all (that pair stays enabled -
it is the tower-glow feedback, a separate mechanism). Builds clean.
Known deliberate simplifications: the real per-cube stack-copied pass
structs are shared statics here (read-only, equivalent); our faceSign is
the view-dot (same sign as the real 2D cross); AA1's 0.005 threshold
compares our normalized sign rather than the real cross magnitude
(edge-on cutoff, visually equivalent); NTSC literals (0.625/0.875 ST
clamps, 320/112 scales) generalized to screenW/H expressions.

NOT yet visually tested by aap. If it misbehaves, `CUBE_DEBUG_RED`
still exists, and the passes can be bisected by trimming `cubePasses[]`.

## 2026-08-26 (cont. 3): "kinda nice but too bright" - two wrong constants traced to a hand-computed gp error, fixed against the REAL BIOS's own GS dump

aap's savestate showed the cubes blazing white. The 19:39 folder dump in
`osdbits/gsdump/` turned out to contain, among the five interleaved runs,
**a dump of the REAL BIOS opening** (WAL0 at 0x2d40, CT24 screen reads =
the real capture) - per-draw ground truth for the real cube passes.
CAUTION for future sessions: the folder mixes five runs; only mtime
19:39 files are the real BIOS, and its draw numbering shares ranges with
our port's runs (an interleaving that briefly suggested a bogus
"one pass per frame" theory - artifact of mixed files, not real).

The surviving real cube draws validated the ported pass stack almost
exactly (pass A colors 107..116/128 = the mode-2 formula to the digit;
blends, AA1 pattern, capture's PABE/colour/CT24 read all match) - but
the REF layers drew at RGB (11,11,13)..(17,17,20) where the port
computed (128,128,91). Root cause: the mode-3 coefficients and the five
scroll/zoom constants had been read from hand-computed gp-relative
addresses that were WRONG by 0x80 (the exact mistake
reference-osdsys-tooling warns about; gp-32548 = 0x2a714c = the known
1.8 half-extent confirms the corrected math). Real values:
- scroll phases: -0.00375 / +0.00375 (work-buffer BLPR/BLP) and
  -0.0075 / +0.0075 (screen BLPR/BLP) - tiny static offsets, sane UVs
  (not the bogus 805.0 which corner-collapsed under CLAMP);
- front-face working-buffer sample zoom: **-0.084** (the ~8% refraction
  magnification);
- mode-3 colour: base*(e*0.4*0.6 + 0.2)*fix/128, all channels equal -
  the env layers are DIM (~9..14% of texel), which is the entire
  "subtle glass" look; the earlier misread (pi-flavoured garbage from
  0x2a709c) saturated them at 128 = the white cubes.

Also confirmed from the real dump + disasm: `OpeningInitLightsCubes`
itself writes the face colours via `sub_21b690(1.8, -16, -16, 24)` =
{112,112,152} (+128 encoding), so the static table stands; face 5 is
genuinely unwritten BSS = black. `DrawIllegalCube` (0x219748) is a
near-clone of DrawCube with its own constants - do not confuse their
callers when tracing (0x217ab0 = OpeningInitLightsCubes, not a cube
drawer). `DrawExtraBuf2` really samples the save as PSMCT24 (alpha via
the init TEXA{127,AEM,129}) - port adjusted to match.

All fixes applied; builds clean; awaiting visual test. Expected: dim
glassy cubes with the background refracted ~8% zoomed, subtle shimmer
from the +-0.004/0.0075-offset env layers, edge glow at silhouettes.

## 2026-08-26 (cont. 4): register-level diff of OUR draws vs the REAL BIOS's draws - per-instance colour bug found; textures verified byte-identical

aap's fresh GS dump of the current build enabled the ultimate check: the
same folder holds per-draw records of BOTH implementations (real BIOS =
mtime 19:39; ours = today's run, draws 1978-2937). Field-by-field
comparison of complete cube pass sequences:

**Validated exactly** (real vs ours): capture parameters; pass A colours
and fix; all blend/ALPHA/TEST/TEX0/TEXA/CLAMP registers per pass type
(TEX0 for the REF passes is bit-identical); the scroll-phase UVs
(+-0.24 and +-0.48 texels, matching +-0.00375/0.0075 x 64); and the env
UV formula (one REF pass: ours U=(-5.5..84.2) V=(0..94.2) vs real
U=(-5.6..83.6) V=(0..93.7) - different frame, same math). The
BLPR/BLP/REF textures are BYTE-IDENTICAL between ours and the real
(itexraw means match to the digit) - note BLP/BLPR are pure-black
textures with noise in the ALPHA only: those additive passes contribute
no colour, they paint the noise pattern into the destination ALPHA,
which the following REF passes pick up via their Cs*Ad blend - that's
the shimmer mechanism, and it works unchanged in the port. (Also
learned: `itex0` dump images are UV-cropped per draw - compare `itexraw`
for content questions.)

**The bug found**: real REF/colour-pass vertex minima never reach 0,
ours did. Cause: `cube_21BF88` slices the 0x27b3f0 colour table by its
INSTANCE argument (`sll s8,a2,4`, a2 = the instance), not by face - the
table is per-cube, all six faces share one colour, and entry 5 is
simply unused (five cubes). The port had read it as per-face, giving a
phantom black "face 5" (visibly dark faces + black REF verts). Fixed:
single `cubeBaseColor` {112,112,152}.

**Open, parked as cosmetic**: real pass-A blue spans 121..128 early in
the animation, which back-solves to base_b ~ 127 rather than the static
init's 152 ({112,112,152} confirmed twice: OpeningInitLightsCubes'
sub_21b690(1.8,-16,-16,24) call and the later savestate). An unnamed
second init at 0x219cb8 writes {128,128,128} with a different
half-extent global (gp-32416) - some phase logic around these writers
is still untraced. Worst-case error is ~14 on one channel of one pass.

## 2026-08-26 (cont. 5): three fixes from aap's synced A/B dumps - blue clamp, inverted refraction normal, rand-stream displacement

aap provided synced GS dumps of both implementations (same boot moment -
the cube geometry matches vertex-for-vertex), plus: "some faces too
bright... the blue gets too much... refraction isn't quite correct...
lights not starting at the right positions anymore".

1. **Blue: the [0,127] clamp.** Dense real-dump colour statistics fit
   base {112,112,127} exactly on every pass type (pass A blue spans
   121..128; REF64 12..20; REF42 8..13 - none fit 152). The mechanism
   was already in the 21BC90 trace and got dropped in porting:
   `sceVu0ClampVector(dst, src, 0.0, 127.0)` clamps the colour table on
   copy into the struct, so blue is 127 in practice. Folded into
   `cubeBaseColor`. (Side finding: `sub_219cb8` - called from the real
   sub_219f08 init chain - is the REAL InitCubes: it contains the
   `d = (i==2) ? special : (i-2)*scale` formula our port already has,
   writes the colour table to {128,128,128} via sub_21b690 with a
   half-extent from gp-32416, and inits anchor/outB/rate. Which of its
   writes survive against OpeningInitLightsCubes' {112,112,152}/1.8 is
   moot for colours - the dump pins the effective base - but its
   existence is worth knowing: 0x219cb8-0x219ea4, unnamed in the IDB.)

2. **Refraction: the offset normal points INWARD.** With matched
   geometry, per-vertex U-X offsets were equal-magnitude OPPOSITE-SIGN
   between real and port (+3.5/-14.5/-4 vs -3.1/+11.3/+3.7 per face).
   The real 21BC90 normal = OuterProduct(v1-v0, v2-v0) over the
   face-vert table = the INWARD normal (face 0 = {0,1,2,3} on the -z
   face crosses to +z). Only the projected-normal UV term cares about
   the sign (lighting/faceSign are sign-invariant); `CubeFaceSetup` now
   projects the negated normal.

3. **Lights: RandomizeTowerField was displacing the rand() stream.**
   lightsSeed is tuned to be the boot's 5th rand() call (4 burned in
   sub_215798), but RandomizeTowerField (added 2026-08-24, runs earlier
   in init) consumed a variable number of rand() calls. It now uses a
   private LCG (`towerRand`, deterministic seed - field repeatable per
   boot, keeps A/B comparisons aligned); the ELF now contains exactly 5
   libc rand() call sites: the 4-burn + lightsSeed.

All three rebuilt clean, awaiting aap's test - the reference moment is
early: "the pink [light] should split into 3 or so" through a cube.

## 2026-08-26 (cont. 6): the additive feedback loop - FBA, and the capture's missed tail

aap watched the dumper-slowed render: "first couple of frames had good
cubes and then they became too bright... accumulate the color
additively... feedback loop somewhere. wrong buffer as texture?" -
diagnosis essentially correct, the loop was real, the culprit a register
not a buffer:

- Real cube-era draws all run **FBA_1 = 0**; ours ran FBA=1 (dump
  compare). With FBA=1 every framebuffer write forces alpha >= 128, so
  the Ad-gated env layers (blend 8) add at FULL strength everywhere
  instead of being keyed by the capture/noise alpha - and since pass A
  re-samples the previous screen each frame, the additions compound:
  the runaway bloom.
- The real ROM's flow: init genuinely sets FBA=1 (real 0x218d64, the
  source of aap's pre-AI `vif1SetAD(SCE_GS_FBA_1, 1)` line - it's at
  the tail of the function that tail-calls OpeningInitLightsCubes), the
  early lights-only frames run with it - aap's "first frames good"
  window - and then `sub_21c7a8`'s TAIL (missed in the original trace,
  cut off at 0x21c8c0) restores TEXA{127,AEM,129} and writes
  **FBA_1=0** on every capture, keeping the opening at FBA=0 from the
  first cube onward. Port now does the same.

With FBA=0 the alpha story finally coheres end to end: the capture
stamps alpha=0 (colour a=0), pass A stamps the true screen alpha,
BLP/BLPR paint their noise into destination alpha, and the REF layers'
Cs*Ad blend picks up exactly that pattern - dim, textured, bounded.

## 2026-08-26 (cont. 7): the REAL last feedback loop - "-2" is the CURRENT frame, not the previous one

Post-FBA-fix, aap still saw bright-blue accumulating cubes and no light
refraction. Screenshot pixel forensics: the bright cubes have r=g
EXACTLY with b ~ 2.5-4x r - the fingerprint of content repeatedly
multiplied by the glass tint (blue gain 127/128 ~ 0.99 vs red 112/128 ~
0.875: any residual add accumulates ~125x in blue, ~8x in red). A
cross-frame loop was still alive.

Re-deriving the parity selectors carefully:
- `-1`: evenOdd&1 ? 2240 : 0  (the displayed/previous buffer)
- `-2`: evenOdd&1 ? 0 : 2240
- the capture's own screen read: same as -2
- DrawCube's screen-pass FRAME target: same as -2

All cube reads and writes use ONE parity - the current in-progress
frame buffer (which is cube-free at cube time, cubes drawing last).
`-1` is never used by the opening cubes: **the real pipeline has no
cross-frame reference whatsoever.** The port had bound -2 to the
opposite parity ("previous screen"), creating a genuine feedback loop
the real ROM never had - and mixing sources (capture=current, pass
A=previous), which is also why the lights (drawn this frame, present in
the capture but not consistently in the A-stamp) failed to refract.
One-word fix: -2 binds `cur`, `prev` deleted.

Verification note from the same round: the flipped-normal refraction UVs
now match the real draw-for-draw to +-0.06 texels (11:37 run vs the
saved real draw-28 numbers) - the refraction FORMULA is closed.

## 2026-08-26 (cont. 8): REFRACTIONS CONFIRMED WORKING by aap - then real winding culling + the current memcard field

aap: "holy shit, you did it! the refractions!" - the cube pipeline is
now visually confirmed end to end. Two follow-ups from the same message:

**1. Backface culling ("faces off a bit") - real mechanism ported.**
The real 21BC90 visibility sign is a true screen-space winding test:
`cross2d(normalize(v0-v1), normalize(v0-v2))` on the PROJECTED verts
(21bdc0..21be9c; the edges ARE normalized - so the AA1 threshold 0.005
is a small-angle cutoff, not a raw-area epsilon). The port's
dot(normal, fwdDir) approximation agrees except near edge-on faces,
where perspective makes the visibility boundary vertex-dependent -
exactly "off a bit". Ported verbatim; group convention: sign<0 = back
face (group 1), aa1 = texaFlag && (back-group || sign > 0.005).

**2. Tower lighting: the field now matches the real memcard.** Extracted
the CURRENT field from the newest real-BIOS savestate's derived arrays
(towerFlags@0x325f90 stride 80, towerC@0x3277e0 stride 56,
towerD@0x327af0, towerPos@0x326c10 stride 144 - the doc's BSS map).
Same 21 (r,c) cells as the 2023 capture, but z values shifted (the
boot-history ring advanced) - extraction was necessary, the old table
would NOT have matched. `LoadCapturedTowerField` (restored from
opening.c.ai-reference, new data) is now the default;
`RandomizeTowerField` + its LCG remain behind `TOWER_FIELD_CAPTURED 0`.
Bonus: with the captured field, the randomizer consumes nothing and the
whole boot is deterministic - libc rand() call sites in the ELF: exactly
5 (the 4-burn + lightsSeed).

**Next step for the tower-lighting question**: all real tower draws in
the shared gsdump folder have been overwritten by later runs - one
fresh A/B dump pair (real + ours, same boot moment, now with identical
tower geometry) will allow the same per-vertex diff that cracked the
cube colours. Things to compare when it lands: main-pass per-vertex
RGB vs tower position (the radial-falloff question), the side-pass
colours, towerA/towerB alpha tables, and the DrawExtraBuf2 composite's
contribution (the old "feedback glow" falloff lead, now actually
implemented in the port).

## 2026-08-26 (cont. 9): culling confirmed by aap; the "z-fighting" distant towers = a double-applied step 5

aap: "oh the culling is so much better!" - then reported z-fighting-ish
artifacts on distant towers. Investigation on the fresh dump + synced
real savestate screenshot (which conveniently captured almost the same
moment - overall the two scenes now match remarkably well):

- The artifact: bright slivers / grey panel edges visible on the dark
  distant towers, absent in the real. The stepped-Z draws crossing that
  region are the FOG layers (six additive GEQUAL quads, z ~142k..180k)
  over the far towers (z ~97k).
- Root cause, found in our own init: step 5's
  `towerPos[r][c][2] += towerC*30 - towerA` is NON-IDEMPOTENT, and the
  captured field was extracted from the real savestate's towerPos AFTER
  the real step 5 had run - so the adjustment is baked into the table,
  and running our step 5 on top applied it TWICE: every tower sank a
  further +9 (C=0.4), +15 (C=0.6) or +21 (C=0.8/1.0) into the distance.
  (This also retro-explains the "+9 shift vs the 2023 capture" seen
  during extraction - the 2023 table had been captured PRE-step-5; the
  memcard history apparently never changed at all.) The misplaced
  depths put the far towers wrongly relative to the fog layers - the
  panel/sliver artifacts. Fix: skip the pos-adjust when loading the
  captured (post-step-5) field; towerA/B/C recomputation is idempotent
  and still runs.
- Z state itself verified clean in the dump: ZBP 0x1180, Z_24
  consistently, main pass zmsk=0 / decal passes zmsk=1, GEQUAL - no
  format mixing.

**IMPORTANT WORKFLOW NOTE**: every PCSX2 folder dump OVERWRITES
osdbits/gsdump/ by draw number - the real-BIOS reference dumps from
11:27 were clobbered by later runs TWICE today. For A/B work, move or
rename the folder between runs (e.g. gsdump-real/) so both survive.
The tower-LIGHTING comparison (aap's remaining question) still needs
one surviving A/B pair: with the captured field + this depth fix the
geometry should now be identical, making a per-vertex colour diff
conclusive.

## 2026-08-26 (cont. 10): THE TOWER FALLOFF - FOUND AND CLOSED. It was towerGrid all along.

With aap's fresh real-opening dump (archived to
osdbits/gsdump-archive/real-opening-1248/ - the new workflow: aap keeps
dumping into gsdump/, we copy out what we need), the per-tower colour
comparison finally ran: REAL tower mean brightness varies ~10..72 with
position (bright central cluster, dim edges/top); OURS was flat 52..77.
The falloff is baked into the MAIN-pass vertex colours - so it lives in
the colour-scale computation, killing the old DrawExtraBuf2-composite
theory for good.

f12 = towerGrid[r+3][c+6] * (towerB/128 | towerA/30) was already
ported correctly - the culprit was **towerGrid itself**: the real grid
(read from the savestate @0x3265d0) is a rich spatial field (32..220,
bright core over the tower cluster), while our HeightGrid produced a
flat 220 everywhere (its `v *= 10.2f` saturated every cell - 10.2 is
actually one of the real centre coordinates, likely the source of the
misread). An old session had "ruled out" towerGrid - wrongly.

Real sub_217e30 (0x217e30) fully traced and verified **bit-exact
(max error 0.0 over all 400 cells)** against the savestate:

    sq = sqrtf(5202.0)
    for j in 0..19:                     # second index of towerGrid
      gy = (j-10)*5.1 + 2.55
      for i in 0..19:                   # first index
        gx = (i-10)*5.1 + 2.55
        d1 = 2*sqrt((-5.1-gx)^2 + gy^2)
        d2 = 4*sqrt((10.2-gx)^2 + (5.1-gy)^2)
        v  = (clamp(255*(sq-d1)/sq, 32, 255)
            + clamp(255*(sq-d2)/sq*0.5, 32, 255)) * 0.85
        v -= 10*((((i+j)*i)/(j+1)) % 11 - 5)    # integer maths
        towerGrid[i][j] = clamp(v, 32, 220)

Constants from gp-32508..-32488 = {5202.0, 5.1, 2.55, -5.1, 10.2,
0.85}. Traps that bit previous attempts: BOTH sqrt paths (hardware
sqrt.s and the libm-fallback) double d1 in a branch-likely delay slot;
the two "centres" are (-5.1, 0) and (10.2, 5.1) with the 5.1 reusing
the scale global as a coordinate; the hash divides by (j+1) with the
INNER index i multiplying. Also chased two false towerGrid xrefs into
libdma (lui 0x29, not 0x32 - check the lui!).

Ported verbatim; builds clean. This was the last known lighting
difference - after aap verifies, the tower falloff question that opened
this whole investigation (pre-dating even the 2026-08-24 reset) can be
marked resolved.

## 2026-08-26 (cont. 11): falloff CONFIRMED by aap ("yes! that's it!") - play mode + the top-face corner glitch triage

The tower falloff fix is visually confirmed - the original question of
this whole document is resolved. Changes for aap's "let's play" round:
`TOWER_FIELD_CAPTURED 0` (randomized field via the private LCG, now
seeded per boot from the COP0 cycle counter - variety without touching
the libc rand() stream; flip back to 1 for A/B comparisons against
real-BIOS dumps).

The suspected "z-fighting glitch in a corner of the top tower face":
first triage done - shared-corner Z consistency inside the tower main
pass is PERFECT in both implementations (0 mismatches, real and ours),
so it is not intra-tower z-fighting. Remaining candidates, both of
which would be ORIGINAL behaviour if confirmed: (a) the AA1 edge-seam
artifact (the main tower pass draws with AA1=1 in both real and ours -
AA1 on shared polygon edges is a classic PS2 crack/seam source), or
(b) a fog-layer edge crossing that specific corner. Need aap to catch
it in a savestate/dump with a pointer to the corner (rough screen
position) - then the draws covering those pixels can be diffed exactly.

## 2026-08-26 (cont. 12): the memcard boot-history mechanism - traced (fork), verified, and PORTED. The towers now really are a gaming biography.

aap: "the towers represent how many games have been played and how much
- make it more realistic." A fork traced the full mechanism; its Python
reimplementation reproduces ALL 21 cells of the real field from the
real history bytes (scratchpad/history_parse.py held the verifier; the
real history is archived at osdbits/gsdump-archive/bootHistory.bin).

**The read side (OpeningInitTowersFog 0x218e00, steps 2+4 - now REAL in
the port, no more field synthesis):**
- history: 21 entries x 22B: char name[16]; u8 launchCount, mask, own,
  pad; u16 date = ((year-2000)<<9)|(month<<5)|day.
- each history SLOT owns a fixed 6-cell neighbourhood (cell table
  @0x27a260, tiling all 126 grid cells exactly once).
- tidx = count < 14 ? count : (count-14)%10+4; towerC/towerD from the
  count tables @0x27aee0/0x27af18 ({0.2,0.4,0.6,0.8,1.0...} /
  {0.1 x5, 0.2..1.0}); cell[own] gets that tower; every other set mask
  bit renders as a maxed C=D=1.0 tower.
- positions are STATIC per cell: src table @0x27a670,
  towerPos = {(x+4.8)*4, (y-6.5)*4, (z+4)*12+150} (the old survey's
  "+0.85" was wrong - it's [gp-32468]=4.8).

**The write side (updater ~0x2019a0, file "%s/history" + history.old
in /B?DATA-SYSTEM by region):** known title -> date refresh, count++
(cap 127); at count 14, 24, 34... a random unset mask bit becomes the
new `own` (rand()%6 retry loop - another variable rand() consumer!) and
the title "sprawls" across its neighbourhood; mask full -> count to 63
then frozen with own=7 (six maxed towers). Unknown title -> evict
lowest count (tie: oldest date), fresh entry = count 1, mask 0x01,
own 0. So a big tower = a much-played game; a cluster of maxed towers =
a beloved one; the 2023-02/03 dates in aap's card decode via the
packed-date formula.

**Port** (opening.c): `GenerateTowerField()` = the real parse + step-4
positions, fed either aap's REAL history bytes
(TOWER_FIELD_CAPTURED 1 - exact real field, for A/B work) or
`SimulateBootHistory()` (default) - a synthetic card life run through
the REAL updater rules (12-title library, favourites-skewed boot picks,
40..140 boots, LCG + cycle-counter seed). RandomizeTowerField,
LoadCapturedTowerField and capturedTowers[] are gone (git has them);
step 5's pos-adjust is unconditional again (the parse emits pre-step-5
values for both modes). Both toggle branches build clean; libc rand()
sites still exactly 5. Bonus gotcha from the fork: the doc's
towerA/towerB address labels (0x3273f0/0x3275e8) are swapped relative
to the port's variable names (step-5 semantics unaffected).

## 2026-08-26 (cont. 13): argv-driven field simulation

The cycle-counter seed only produced two distinct fields under PCSX2's
deterministic boot timing (cold boot vs reset). Per aap's spec the
simulator now takes ELF launch arguments (the SDK crt0 passes the
loader's _args block to main(argc, argv) - verified in the crt0
disasm): argv[1] = seed, argv[2] = number of games, argv[3] = number of
boots (favourites-skew distribution via min-of-two draws). Missing args
fall back to cycle-counter seed / 12 games / 40..109 boots. PCSX2-Qt
passes ELF args via its -gameargs option (check --help for the exact
spelling of the installed version). main.c exports gameArgc/gameArgv;
opening.c parses with a local digits-only argInt (no libc dependency).

## 2026-08-26 (cont. 14): "always no towers" - the towerC==1.0 sentinel was mutating the PERSISTENT grid

With simulated fields, towers vanished no matter the parameters. Chain:
the count->towerC table makes any title with >= 4 launches a C=1.0
monolith; the port's C==1.0 special path wrote the -1.0 sentinel into
towerGrid itself; the grid is computed once at init; DrawTowers' <= 0
gate then skipped that cell forever - every monolith self-deleted after
one frame. Simulated cards are mostly monoliths, so almost nothing
survived to frame 2. (On aap's real card - a single monolith - the bug
was practically invisible, which is why it survived every earlier
verification round.)

Re-trace of the real DrawTowers (0x2185c8..): the -1.0 store targets a
PER-FRAME local array on DrawTowers' own stack (base sp+32, stride 36,
pointer parked at sp+544; the draw gate `c.lt.s 0.0, [s6]` reads the
same local) - the persistent towerGrid is never written after init.
Port fix: gridval = (towerC==1.0) ? -1.0 : towerGrid[r+3][c+6], passed
as a parameter into towerPatchVertices; no array mutation. Also
softened simulator defaults (24 titles, 30..69 boots) so default fields
are mostly 1..3-launch towers with the occasional monolith, like a real
card.

Meta-lesson for the notes: this is the THIRD trace error of the same
shape today (missed tail, missed branch-likely delay-slot doubling, and
now a store aimed at a stack local misread as a global array) - delay
slots and stack-relative addressing are where this codebase's traces go
wrong; always resolve the STORE TARGET REGISTER's provenance, not just
the value stored.

## 2026-08-26 (cont. 15): "flatter and brighter, no growth, no chunking" - the per-vertex far-side blackout

aap's degenerate-field report cracked the last tower-rendering
mistranslation. Re-trace of tower_218318's colour stores (218418-458):
the black-out condition is `vertex_z > 0` - PER-VERTEX, not per-tower.
Far-side (+towerA) vertices are painted {0,0,0,128}; near-side get the
lit colour; Gouraud then shades each tower from lit at the camera-facing
end to black at its tail. Combined with the banded wall texture this IS
the "column/chunk" look, and it is why growth reads as LENGTH: a taller
tower fades over a longer run. The port's old reading (`towerA <= 0` -
per-tower, never fires since towerA >= 3) left every vertex lit: tall
towers showed their far faces bright = flatter + brighter, no depth cue.

Also settled along the way:
- The real tall-tower geometry anchors the FAR end at base + C*30 and
  extends the near face toward the camera by towerA (step 5's
  `z += C*30 - A` plus the +-A vertex extent) - growth is toward the
  viewer from a common back plane.
- DrawTowers' multi-pass skeleton (pass counter at sp+552, per-frame
  local |dx|+|dy| array at sp+32 with running max, the -1.0 stomp) is
  DORMANT in the shipped ROM: the pass counter is initialised to 0
  (`move v0,zero` = N-1 with N=1), so exactly one pass runs, the
  distance gate (pass < |dx|+|dy|) is always true, and the max-distance
  scan result is dead. Looks like a chunk-by-chunk build-in animation
  that shipped disabled - port needs none of it.
- Fourth same-family trace error today: a condition OPERAND
  misattributed (vertex z read as towerA). The pattern of the day:
  resolve every operand's provenance from the disasm, not from what
  the surrounding code "obviously" means.

## 2026-08-26 (cont. 16): the monolith colour myth dies - packet forensics from the savestate

aap's '1324 7 1000' card (38 maxed towers) rendered as small flat white
squares. Reading the LAST-PATCHED VU packet straight out of the
savestate's EE RAM (towerBlocks pointers -> vertex qwords at +0x10,
colours at +0x90) showed the smoking gun: lit-vertex colours of -0.6 -
while normal towers carry f12 = towerGrid * towerB/128 ~ 60..130. The
"-1.0 sentinel" never enters the colour path in the real ROM at all:
the C==1.0 branch converges back to the common patcher call, whose f12
is loaded from towerGrid like any other tower. A maxed tower is simply
a LONGER, BRIGHTER tower (f12 = grid * towerA/30; the real monolith's
182-max colours confirm). The -1.0 store goes exclusively into the
dormant per-frame draw-once local. Port fix: the gridval override
removed; f12 always = towerGrid[r+3][c+6].

Packet forensics also confirmed working: z = +-towerA patched per
vertex, far-side colour zeroing per the sign flags, alpha 128, and the
towerBlocks pointer order (index 0 -> TowerBlock5 etc.) matches the
real's own block-pointer table - not a bug.

Fifth and final trace-error of the family today, and the wildest: an
INVENTED semantic ("sentinel colour") attached to a real instruction
whose actual target (the dormant local) had already been correctly
identified. When a store's purpose is unknown, port it as dead rather
than guessing a purpose for it.

## 2026-08-26 (cont. 17): FIELD CONFIRMED MAGNIFICENT - and the last artifact was a striped mip

aap: "lovely! this was it!... but the tower geometry is still looking
wrong... may not be z-fighting but getting the wrong texture." The
texture instinct was right. Register diff real-vs-ours: TEX0/TEX1/
MIPTBP1/CLAMP/TEXA bit-identical. Content diff via the dumps' itex
snapshots: main WAL0 identical (mean 109.8 both); OUR mip1 = horizontal
stripe garbage (mean 55.1 vs real 111.7). Distant towers sample the
mips with trilinear blending - shimmering between good near-texture and
striped far-mip reads exactly like z-fighting.

Root cause in InitTexture's mip downscaler: `dst[y*prevDim.h + x]` -
destination stride taken from the PREVIOUS level (256) while writing
the 128-wide mip -> every other row interleaved with garbage. Fixed to
`dst[y*mipDim.w + x]`; mip2 heals transitively (it downsamples mip1).
The neighbouring "picks wrong colors" quirk (box filter sampling only
even texels) is aap's own pre-AI faithful-bug annotation of the REAL
code - kept bug-for-bug. Verification: simulating the fixed filter over
the dumped real texture reproduces the real mip1 (mean 110.3 vs 111.7,
residual = CT16 quantisation through the PNG round-trip).

With this, every known divergence between the port and the real OSDSYS
opening is closed: towers (field, memcard mechanism, falloff, colours,
geometry, mips), fog, lights, the feedback composite, and the
refractive cubes. What a day.

## 2026-08-26 (cont. 18): the stale-texcache endgame

Post-mip-fix, aap still saw a wrong patch of texture and suspected the
GS texture/CLUT cache (librw experience: "a new texture needs to write
both TEXFLUSH *and* TEX0"). Forensics agreed on every level: raw VRAM
mip bytes in the savestate are clean (main 99.9% exact vs the real
savestate; mips within +-1 LSB of the real's - our "faithful" even-texel
filter is also apparently not quite the real generator, invisible but
noted), the FRESH dump shows PCSX2 sampling clean mips (111.2 vs real
111.7), yet the savestate's own screenshot still rendered the OLD
striped mip: stale renderer-side texture cache from the earlier
boot/reset chain. The real ROM never TEXFLUSHes in the opening (safe on
cold real hardware); our reset-heavy emulator workflow is not so lucky.
Fix: a TEXFLUSH at the end of every UploadImage (clearly commented as a
harness-side deviation; TEX0 is rewritten by every bind anyway). If any
staleness ever appears in the RT-sampling paths (cube work buffer /
save), the same medicine applies before those binds.

## 2026-08-27: REAL HARDWARE - the TOOL, dsnet, and four emulator-hidden bugs

The setup jumped platforms: a TOOL on the network at `devtool`, dsnet
from /usr/local/sce, `dsedb -r run main.elf` booting our port on real
silicon.  First run: an endless stream of `sceGsSyncPath: DMA Ch.1
does not terminate` with VIF1_STAT=0x0f002002.  Decoding that STAT was
the whole ballgame: bit 13 = ER1, "reserved VIFcode" - fatal on real
VIF1 (it stalls until FBRST.STC), silently ignored by PCSX2.

Workflow notes for next time (reference memory has details too):
- `dsedb -d devtool -nokbd <cmd>` for batch; ONE debugger connection
  at a time (a live `run` session blocks all others - "cannot connect").
- ELF load+start is ~5-6s.  Console printfs STOP reaching the dsedb
  log shortly after early init (cause unknown) - console silence is
  NOT evidence of a hang; two "hangs" this session were phantoms of
  this (one misdiagnosed as slow init - it is not slow).
- `dt`/`ds`/`dr`/`dw` on a fresh connection give threads/semas/regs/
  memory of the LIVE target - `dw` of uncached mirrors (0x2xxxxxxx)
  reads true RAM.  `storeimage <f> <bp> <bw> <psm> <x> <y> <w> <h>`
  (hex) dumps GS VRAM but needs the target quiet; argv[4] now runs N
  frames then Exit(0) for scripted grabs (untested end-to-end).
- In-program diagnostics beat debugger archaeology: a one-shot dump of
  VIF1_STAT/ERR/CODE/NUM + D1 regs on ER1 found bug 1 in one run
  (VIF1_CODE still held the killer code: 0x3f800000 = 1.0f).

The four bugs, all invisible under PCSX2 (instant synchronous DMA, no
EE dcache emulation), each now marked in the source:

1. towerKick register-write race (opening.c towerKick comment): the
   real per-tower kick order writes QWC=0/TADR BEFORE its sync; with
   the previous chain still active this truncates the in-flight unpack
   (VIF left owed quads), re-points the live chain at the TowerChain
   tag, and VIF pays the debt with vifcodes+data then decodes a data
   float as a vifcode: ER1 wedge on frame 1 kick 2.  The real ROM
   survives on timing margin only.  Fixed by the loop-top drain (bug
   3); the byte-faithful register order is restored.
2. dcache coherency of the blob patchers: ALL five real patchers OR
   0x20000000 into their destinations (21818c/218214/2182bc/2183e0/
   21850c - disasm) and write the chain through the UNCACHED mirror;
   our memcpy patched through the dcache and the DMA read stale RAM
   (towers absent/degenerate - the "1px strips" were probably the
   collapsed towers).  Ported faithfully with UNCACHED().
3. patch-vs-inflight-chain race: patches for tower N+1 (uncached,
   straight to RAM) overwrite the memory tower N's chain is still
   DMA-reading - flickering towers, worst after big towers (long GS
   raster).  Real ROM: same structural race, lives on margin.  Fix:
   one sceGsSyncPath at the top of the per-tower work, before the
   patchers (marked as making the real code's implicit margin
   explicit).
4. PATH3 clear race (main.c SwapBuffers/StartFrame/WaitNextFrame):
   the harness swap kicked PutDrawEnv (env + CLEAR) down PATH3 - the
   lowest-priority GIF path - and released the next frame's drawing
   immediately; early towers got erased by the late-landing clear
   (popping in/out of existence) and CubeCaptureBuffer photographed a
   half-cleared screen (near-black cube env).  Fix: sceGsSyncPath
   after PutDrawEnv, plus one at frame end in WaitNextFrame before
   the swap is signalled.  (main.c is harness code, so these three
   syncs are harness-correctness, not ROM porting.)

Verdict from aap after all four: "woah, no flickering!" - towers,
cubes and refraction all hold on real hardware.

New toy: argv ngames == 0 populates the WHOLE 14x9 field (seed-varied
heights/brightness via the memcard tables; marked NOT original).
`run main.elf 1234 0 0` - full field, default boots.

aap also identified: sub_218b20/sub_218bd0 (DrawOpeningScene tail) =
the "Sony Computer Entertainment" text overlay (at least one of them).

Open threads for next session:
- Two ~1px strips at the bottom of the screen, present even with the
  whole scene skipped - so frame-infrastructure or display-side, not
  scene geometry.  Plan: storeimage the framebuffer and compare with
  the screen; if the dump is clean the strips are display config
  (DISPLAY/MAGV/half-offset showing rows beyond the 224-line buffer -
  possibly authentic and normally hidden by TV overscan).
- The dark-quarter tower texture artifact: aap reported the SAME
  artifact on real hardware (pre-fixes) as on PCSX2, which undercuts
  the PCSX2 page-rounded-RT-cache theory from cont. 18 / the page
  guards - a shared cause (GS texture cache semantics? mip
  addressing?) needs a fresh look with real-GS ground truth
  (storeimage the texture pages).  Status post-fixes unconfirmed.
- Cube 4 (second to last) "rather black most of the time": possibly
  just the background now - verify against the original OSDSYS with
  the same seeded field on the TOOL.
- The captured-history mode (TOWER_FIELD_CAPTURED) and the real
  memcard parse path remain compile-time toggles.

## 2026-08-27 (cont.): the dark quarter SOLVED - it is authentic (CLAMP x the towerPatchST u offset)

aap flagged towerPatchST as looking different from the assembly and
found that dropping the `u` offset made the tower-texture artifact go
away.  Full re-disassembly settled all of it:

**The port's C was already semantically exact.**  tower_2184d0
(re-decoded insn by insn) writes the quad {u,u},{u+1,u},{u,u+1},
{u+1,u+1}, q=1, w=0, u=a0/256 into all 6 packets at +0xd0; the odd
register dance (u and u+1 precomputed into 4 FPRs before the loop, an
a0==0 early-out that yields the same 0.0) is compiler scheduling, not
extra semantics.  The caller's a0 (0x218a08-0x218a3c, re-verified:
s0=r+3, s2=c+6, s1=s0+s2) is exactly the ported
`(r+c+9)*(r+8)/(c+7) + (r+c+9)*(r+7)/(c+9)` (integer div, range
0..114 over the 14x9 field).

**The artifact the u offset produces is the real ROM's look, not a
bug.**  The chain of evidence:

- The towers draw with LEFTOVER clamp state: the real image has
  exactly two CLAMP_1 writers (vif1SetClamp 0x212bd0 xrefs: DrawFog =
  REPEAT, DrawLightsAndCubes = CLAMP), both AFTER DrawTowers in
  DrawOpeningScene's frame order, and NO prebuilt A+D CLAMP_1 qword
  anywhere in the 4MB image (exhaustive 16-byte-aligned scan, 0 hits).
  So from frame 2 on the tower field draws with WMS=WMT=CLAMP
  inherited from the previous frame's cube passes.
- The real-ROM GS dump proves it at the GS: gsdump-archive/
  real-opening-1248 draw 03260 is the whole tower field in one flush -
  PRIM=TRISTRIP IIP=1 TME=1 FST=0, TEX0 TW=TH=8 TBW=4 (the 256px
  wall texture), CLAMP_1 WMS=WMT=1 - with per-tower U,V spanning
  exactly [a0, a0+256] texels (observed a0 values 0..104, each high
  value = its low + 256, both axes).
- Everything past texel 255 clamps to the texture's edge row/column
  (TEXOWAL0's edges are window/grout detail, not a solid colour), so
  a u-sized fraction of each face - up to ~45% - renders as smeared
  streaks.  The same streak bands are visible on the tower faces in
  the real YouTube capture (full_opening_yt.png crops) - "the dark
  quarter".
- Dropping u eliminates the band (all towers sample [0,1]^2 exactly)
  but also eliminates the real per-tower texture variation - it is a
  visual "fix" that diverges from the ROM.

Port changes: restored the faithful u; replaced the raw
`pkt[(0xd0+v*16)/4]` byte-offset arithmetic in all three block
patchers with a proper TowerPacket struct (tag / vert / norm / colour
/ st, the real 17-qword unpack layout); DrawTowers now documents the
deliberate absence of a CLAMP_1 write.  The earlier PCSX2
RT-cache/mip theories for the dark quarter are dead: the artifact is
supposed to be there.  (Caveat: frame 1 towers sample with whatever
CLAMP_1 the boot/menu left behind - one frame, invisible.)

## 2026-08-27 (cont. 2): "why is the strip so dark for us?" - a full A/B audit finds GS-LEVEL IDENTITY in PCSX2

aap accepted the clamp-is-authentic finding but reported our strip
renders visibly DARKER than the original BIOS's.  Audited every layer
of yesterday's PCSX2 GS dump (osdbits/gsdump, 17:21 run, u active)
against the real archive (real-opening-1248) and both savestates:

- Tower-draw registers: bit-identical (CLAMP/TEX0/TEX1-incl-K/TEXA/
  MIPTBP; only half-pixel OFY + 1px scissor + unused-mip3 cosmetics).
- The strip's source data - row/col 255 of TEXOWAL0 - byte-identical
  in VRAM between our savestate and the real's.  Mips value-identical
  within +-1 LSB everywhere INCLUDING edges (the 56%-of-bytes diff is
  uniformly-distributed filter noise, not an edge artifact).
- The glow feedback pair (composite FIX=80 blend + half-width squeeze)
  register-identical to the real's (draws 03261/03262); our save chain
  verified working across frames.  There IS a ~10-frame startup
  transient (composite blends 62.5% BLACK until the save warms up,
  0.625^k decay) - but the real has the identical transient.
- Our isolated tower draw (rt1) shows the strip as midtone streaks,
  NOT anomalously dark; the conspicuous dark lengthwise bands in our
  screenshots trace to AUTHENTIC features (the f3=0.5 side faces and
  the far-end Gouraud black), verified per-triangle from the dump.

CORRECTIONS to earlier notes made while chasing this:
- Resource files: pixel data starts at byte 16 (16-byte header) and
  the file ends with 8 trailer bytes.  The morning's "texture corner
  texel is BLACK" claim came from reading the trailer as pixels with
  an assumed 24-byte header - WRONG.  The true corner texel is 0x45ED
  (bluish midtone), uploaded intact, sampled intact (verified via the
  dump's transfer record AND itexraw, both of which encode C_16 as
  raw bytes R=low/G=high - the itex0/1/2 PNGs are decoded RGB
  instead).
- PCSX2 vertex dumps print FST=0 ST as S*2^TW (texel ints) but FST=1
  UV as raw/16.0 floats - don't conflate.
- Genuinely unexplained but harmless leftover: BOTH savestates (ours
  AND the real's) show ~27 texels of the texture's bottom-right 16x8
  block differing from the resource file at rest - something writes
  that block at runtime in both, midtone values, affects only the last
  ~6% of the strip.  Parked.

Since PCSX2-level data is identical, the remaining suspects for
"ours darker" are: (a) TOOL-vs-PCSX2 sampling differences (real GS
mip/CLAMP behavior vs emulation - the TOOL was off the network when a
storeimage A/B was attempted); (b) the comparison conditions - the
full-field test mode (ngames=0) renders rows 8-13 with a0 = 48..114,
i.e. flat smear bands covering 19-45% of each face right next to
sparkly window texture, while the real memcard field + fog + glow
moments soften the same artifact (a0 map: a0 >= 64 on 37 of 126
cells, max 114 at r=13,c=0).  Next step needs aap's input: which
platform/mode/reference the dark-strip observation came from, and a
fresh same-moment A/B dump pair (MOVE the gsdump folder between runs).

## 2026-08-27 (cont. 3): THE DARK STRIP SOLVED - it was the LIGHT MATRIX, not the texture

aap's fresh same-day GS dump of the current build (no args) landed
nearly frame-synced with the real archive (tower vertices match the
real 03260 draw to half a pixel) - which finally made a per-face A/B
possible.  The verdict, from the matched a0=58 tower:

    real:  cap R=147, four side faces R = 81 / 81 / 81 / 81  (uniform 0.55x)
    ours:  cap R=230, four side faces R = 35 / 115 / 35 / 106

Two of our four side faces were lit at 0.15x cap instead of the
real's uniform 0.55x.  The "dark strip" aap kept seeing is a
DARK-LIT LONG FACE next to a bright-lit one - per-face VU lighting,
nothing to do with towerPatchST/clamp/mips (the clamp band is real
but subtle, as established in cont. 2).

Root cause, solved by back-solving the real savestate's patched param
window (0x2680f0, the tower light matrix, frozen from the tower era):

    window row0 = (0,  0.6941, -0.6941, 0)
    window row1 = (0,  0.7199, -0.7199, 0)
    window row2 = (1,  0,       0,      0)
    window row3 = (0,  0,       0,      1)

This decodes EXACTLY (4 decimals, including the sway angle
theta = -1.0452 deg: 0.7071*(cos+sin) = 0.6941, 0.7071*(cos-sin) =
0.7199) as light.row[i] = SOURCE x rotation.row[i], with the static
source matrix (real spr+0x200, set once in InitTowersFog):

    { 0, 0, 1, 0 }
    { +sqrt(.5), +sqrt(.5), 0, 0 }
    { -sqrt(.5), -sqrt(.5), 0, 0 }
    { 0, 0, 0, 1 }

Confirmed against the real DrawTowers disasm: 0x2189a4 is
sceVu0MulMatrix(spr+0x180, spr+0x200, spr+0x240) = MulMatrix(light,
SOURCE, ROTATION), and MulMatrix computes m0.row[i] = m1 x m2.row[i] -
each ROTATION row through the SOURCE matrix.

The port had this DOUBLY wrong (both from the original deepseek
trace): source vectors at magnitude 0.5 instead of sqrt(0.5), and the
ApplyMatrix operands swapped (rot x source.row[k] - the TRANSPOSED
result).  Fixed both (opening.c InitTowersFog quad[] + DrawTowers
loop); the fixed code reproduces the real window bytes exactly.
Builds clean, awaiting aap's visual test - expected: all four side
faces uniformly lit at ~0.55x cap, the dark band gone, leaving only
the subtle authentic clamp streaks.

Bonus facts from the same A/B: per-face brightness = VU lighting
(light matrix x the packet's static per-face normals), so side faces
are NOT a constant f12*0.5 - the 0.5 is f3, lighting multiplies on
top.  And the fresh dump's texture TBPs moved (0x3100 vs yesterday's
0x2d40) - allocation shift, upload/sample consistent, harmless.

## 2026-08-27 (cont. 4): light matrix round 2 - the REAL source matrix, f3=0.8, and the ApplyMatrix convention trap

aap's test of the first light-matrix fix: "not right yet, but
definitely different".  The fresh dump showed a consistent new
pattern - per tower, sides at {0.5, 2.1, 1.5, 0.5} x cap in
term-rotated order (real: all 0.55x) - and reading the FIXED build's
own patched TowerLightMatrix out of its savestate exposed the real
mistake: sceVu0ApplyMatrix(out, m, v) computes out = sum_k v[k] *
m.row[k] (row-vector x matrix), NOT rows-dot-v as the morning's
derivation assumed.  With the correct convention everything
re-solves cleanly:

- The construction is light = ROTATION x SOURCE (row-major product;
  sceVu0MulMatrix(light, spr+0x200, spr+0x240) with MulMatrix m0 =
  m2 x m1).  The operand swap from cont. 3 was accidentally right;
  the source VECTORS were wrong.
- The REAL source matrix (spr+0x200): rows { (0, s2, -s2, 0),
  (0, s2, -s2, 0), (1, 0, 0, 0), (0, 0, 0, 1) }, s2 = sqrt(.5) -
  the light direction (0,1,-1)/sqrt2 DOUBLED, then the x-axis row.
  Solved from the real window (l*(c+s) = .6941, l*(c-s) = .7199)
  where the unit-rotation check c^2+s^2 = 1.000 pins l = sqrt(.5)
  exactly; the identical doubled-row pattern appears in the cube-era
  spr+0x240 remnants.
- vucode_1's lighting (read from the .vsm, no more inference):
  f = max(0, n^T x lightmatrix) per column, colour = f * the static
  window colour qwords 8-11 = {(1,1,1,0), (.8,.8,.8,0) x2,
  (.4,.4,.4,1)} (read from the real savestate at 0x2680b0), then
  vertexcolour * that.  Packet normals (real blob): caps (0,0,1,1),
  sides (0,+-1,0,1)/(+-1,0,0,1).
- f3 (the side-face colour factor in tower_218318) is 0.8, not 0.5:
  the real load is lwc1 f3, -32484(gp) = 0x2a718c = 0.8 (verified in
  expanded.bin, next to DEG2RAD/PI_2).  The old "0x27af38 first entry
  0.5" was a misattribution - that ramp table {.5,.6,.7,.8,.9,1.0} is
  something else.
- Closed-loop check: simulating the fixed port pipeline (rot x source,
  f3=0.8, the .vsm formula, real colours) for terms 0/90/180 deg
  predicts cap=147, sides 80-81 - the real dump's exact per-face
  pattern (81 x4, cap 147; the 80s are the +-1.5% sway modulation).

Port changes: quad[] rows replaced with the real source matrix rows;
f3 = 0.8.  Builds clean, awaiting aap's test.  LESSON (again, now
with teeth): NEVER derive from an assumed sceVu0 convention - the
memory note said disassemble first, and skipping that cost a full
round trip.  Reading the port's OWN patched output back from its
savestate (TowerLightMatrix via ee-nm symbol + eeMemory.bin) is the
fastest way to see what the code actually computes vs. what you
think it computes.

## 2026-08-27 (cont. 5): DARK STRIP ROOT CAUSE - the mip generator's out-of-bounds bottom row

With the captured field enabled, the fresh dump's tower vertex colours
matched the real archive EXACTLY, tower for tower, face for face - the
light-matrix + f3 fixes are confirmed complete, and the remaining dark
strip had to be texture sampling.  aap's clue was decisive: the strip
vanishes under REPEAT or with in-[0,1] ST - both of which avoid
sampling the texture's EDGE texels.

The chain, each link verified:
- TEX1 K = -65/16 = -4.06 with tower w ~ 100-180 puts the GS LOD at
  ~2.6-3.4: THE WHOLE TOWER FIELD SAMPLES MIP2 (MXL clamps at 2).
- The T>1 clamp band smears mip2's BOTTOM ROW (dump-verified band
  orientation: the dark band sits on the V=a0+256 side, at 0.5x face
  brightness - a suspiciously structural factor).
- Positional ours-vs-real VRAM delta localization: mip2's big deltas
  (>4/31 per channel) concentrate EXACTLY in the bottom block row
  (blocks 21/23/29/31 = pixel rows 56-63; 64 texels = row 63), ours
  33% darker (25.7 vs 38.3 avg).  Mips are otherwise positionally
  near-identical (88% of texels within +-2/31) - no scrambling.
- InitTexture's downscaler reads source row 2y+2 for mip row y; on
  the LAST mip row that is one row PAST the source buffer.  The two
  OOB taps (of 4) averaged in whatever follows the buffer - zeros
  here - making every generated mip's bottom row HALF brightness.
  There is the 0.5.
Fix: clamp the second source row to the last valid row on the final
mip row.  Simulated fixed mip2 bottom row: avg 41.5 vs real 44.1
(remaining gap = the known uniform +-1 LSB filter-tap difference).

This also explains the ancient observations: the artifact appeared
identically on PCSX2 AND real hardware (it is baked into the mip
DATA, not renderer behaviour), and disappeared with REPEAT (wraps to
interior texels) or unclamped coords (never touches the edge row).

Also added: TOWER_DEBUG_SQUARE (aap's isolation idea) - five FST=0
STQ quads drawn after the tower field with the towers' texture state,
q per quad steering the GS LOD directly (mip0 / mip0-1 blend / the
far-tower operating point q=1/180 / deep mip2 / a no-clamp control),
each with the u=104/256 clamp band visible up close.  Left ENABLED
for the confirmation run - set TOWER_DEBUG_SQUARE 0 in opening.c to
turn it off.  Expected after the fix: the band on the mip2 quads
turns midtone-streaky like the mip0 quad; towers lose the dark strip,
keeping only the subtle authentic band.

## 2026-08-27 (cont. 6): dark strip CONFIRMED FIXED by aap; field-sim spread

aap: "you did it! the towers are looking great now!" - the dark-strip
saga (three stacked bugs: light-matrix source + operand order, f3=0.8,
the mip generator's OOB half-dark bottom row) is closed.
TOWER_DEBUG_SQUARE toggled back to 0.

Follow-up: `main.elf 1234 8 1000`-style simulated fields clustered all
towers in one fixed chunk of the field regardless of seed.  Cause: the
real updater's eviction scan (lowest count, oldest date) settles on
the FIRST empty slot while the card fills, so n games always occupy
slots 0..n-1 = one contiguous group of neighbourhoods; and a new
entry's home cell is always bit 0.  The simulation (only - the parse
and the full-card eviction rule are untouched, and both deviations are
marked NOT original in the code) now assigns new titles to a RANDOM
empty slot and a random initial home bit.  Verified by simulating the
exact C algorithm in Python: seeds 1234/42/99999 with `8 1000` now
produce visibly different field layouts instead of the same chunk.
