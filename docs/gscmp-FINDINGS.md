# GS-dump comparison findings (2026-09-01) — retail OSDSYS vs osdbits, System Config screen

Evidence base: nine single-frame PCSX2 GS dumps (shift+F8) taken by aap this morning, all of the
System Configuration screen on Clock Adjustment. Six retail (`PS2_BIOS_(USA)_*_202609010806*`),
three of ours (`main_*_202609010805*`). All decoded in THIS directory
(`/u/aap/.claude/jobs/58e316f8/tmp/gscmp/`).

## Tooling in this directory (all working, use them)

- `decode.py` — generalized binary .gs/.gs.zst parser (based on docs/gsdump-gifdecode.py):
  decompresses (py3.14 stdlib `compression.zstd`), extracts the embedded 640x480 screenshot to
  .png, finds the packet stream by brute-force scan, walks the GIF stream with ONE GifWalker PER
  PATH (paths are independent state machines — mixing them desyncs; both sides here are ~all
  PATH3), pickles an event list per dump.
- `drawlog.py <pkl> <out.log> [v]` — per-draw state log: FRAME/TEX0/TEST/ALPHA/ZBUF/CLAMP/etc.
  writes + one `Dnnnn` line per vertex-kick run (prim flags, nverts, bbox, z range, UV bbox,
  first RGBA). With `v`: every vertex printed (XY in raw 12.4-space pixels, i.e. subtract
  XYOFFSET 1728,1936 for screen coords; z; last UV; RGBA). CAVEAT: non-FST prims use ST/Q which
  is NOT decoded — the printed uv is stale garbage there (backdrop strips, bump taps).
- `segsum.py <log> [vsync_index]` — one line per FRAME-target segment: draw/vert counts, prim
  mix, textures bound, alpha modes, bbox. CAVEAT: TEX0 written at a stage's tail (for the NEXT
  segment) gets attributed to the current segment — trust the verbose drawlog for "which pass
  samples what".
- Existing logs: `retail614.log`/`ours519.log` (compact), `retail614v.log`/`ours519v.log`
  (per-vertex). VSYNC1 frame spans lines 12346-24690 (retail) / 7550-15086 (ours) in the verbose logs.
- VRAM: each dump embeds the full 4MB GS local memory at offset `44 + serial_len + shot_size + 425`
  (serial_len at file offset 20, shot_size at 40). Decoded buffer PNGs already here:
  `v_r614_*` / `v_o519_*` (scr0, wb3, wb4, and `*_a` alpha views) via osdbits/tools/gsmem.py's
  grab/topng.

## Buffer/texture map (block addresses)

Retail: screens 0/2240, Z 4480, wb3 6720, wb4 8960, backdrop tex 11264, TEXCBUMP 11520,
orb textures 11776+11840, TEXCREFA 11712, fonts/icons 11968/12037/14341,
**unknown textures 11200 and 11584** (see finding 3).
Ours: screens 0/2240, Z 4480, wb3 7040, wb4 9600, TEXCBUMP 14372, TEXCREFA 14436,
backdrop 13728, orb 13600/13664, fonts 13984/14226/14308.

Both render 640x224 fields, XYOFFSET (1728,1936), fbw=10, PSMCT32, Z at zbp=4480.

## Finding 1 — cube black seams: ROOT CAUSE FOUND (high confidence)

**Retail draws the ENTIRE cube stage at constant flat Z = 16773136 (0xFFF010); the black mask
faces at 16773137. Our port emits real projected per-vertex Z (~5.2M-6.2M).**

Mechanism (proven from the dumps):
- The refract pass is AA1. On AA1 partial-coverage pixels (face outlines AND tristrip
  diagonals — a strip's two triangles each treat the shared diagonal as an edge) the GS writes
  coverage as alpha and does NOT write Z.
- The bump taps that follow are NOT AA1 and redraw the same quads full-coverage — they normally
  REPAIR those crack pixels back to alpha 0x80. But they run ztst=2 (GEQUAL); at the crack
  pixels the stored Z is stale (never written by the AA1 pass), so with our real Z the repair
  loses wherever stale Z is higher, and the crack survives.
- Retail's flat near-max Z makes the repair unconditionally win. That is WHY the ROM uses flat
  Z there (the cube stage has no camera and relies on painter's order anyway).
- The alpha-masked composite (0x22C190) then leaks dark background through every crack.

Proof artifacts: `v_r614_wb4_a.png` = five SOLID grey silhouettes; `v_o519_wb4_a.png` = the full
cube wireframe cracked into the mask (crack pixels measured alpha 2..58 = unrepaired coverage
values; retail has essentially zero such pixels). `crack_o519.png`/`crack_r614.png` are 3x crops.

This also explains the previously-unresolved "1px dashed dark line along tristrip diagonals"
from the last session (same mechanism, milder when stale Z only partially wins).

**Fix: in the cube stage only (menuconfig.c cube emit path), emit constant z=0xFFF010
(16773136), black pass z=0xFFF011. Rods KEEP real Z** — retail rods measured real per-vertex z
1.79M-2.19M, same scale as ours (1.85-1.89M). Everything else about the cube chain (pass
structure, winding split counts, shared-vertex bit-exactness, tbp bindings, the half-width
wb3->wb4 additive sprite uv(0.5..320.5) x(1728..2048), cube2+ far pass sampling tbp=0 psm=1 =
the screen) VERIFIED IDENTICAL retail-vs-ours already — don't touch those.

Open sub-question worth one check: our ZBUF writes show `psm=1` (PSMZ24) + ZMSK in places where
retail has psm=0 (PSMZ32) throughout the cube/rod stages; harmless for values <2^24 but match
retail while in there.

## Finding 2 — orbs are missing from wb3 in ours

Retail draws EVERY orb twice, interleaved in global z-order with the rods: the LINESTRIP trail +
2 textured sprites go to the screen (FB=0/2240) AND then identically into wb3 (FB=6720).
Ours draws orbs to the screen only (verified: our orb segments have no wb3 twin).
wb3 feeds the frame-start tint, the rod wb3 passes (self-sampling), and the cube-stage
composites — so everything that refracts/samples wb3 lost the orbs. This is the most likely
main cause of "no refraction visible when orbs pass behind the clock/cubes".
Retail reference: verbose log VSYNC1 region, orb pattern = `LINESTRIP+AA1 x1 + SPRITE+TME+ABE+FST x2`
to FB=0 then the same to FB=6720, tex=11776,11840, ALPHA 0201.

## Finding 3 — missing full-screen distortion stage (2 x 86 quads) between rods and blur

Retail, after the last rod/orb and before the 5-pass zoom blur, renders TWICE:
`FB=8960: 173 draws = 1 SPRITE + 86 TRISTRIP+TME+FST + 86 TRISTRIP+TME+ABE` (bump-pair style),
full-screen bbox, sampling wb4 self + TEXCBUMP(11520) + **tbp 11200 + tbp 11584** (textures we
never bind), each pass followed by a full-screen composite sprite to the screen (FB=0,
SPRITE+TME+ABE+FST, and a second 2-sprite composite after the repeat). Ours: nothing between
rods and blur. This is very likely the config-only backdrop shimmer ("TEXCKABE tunnel" family —
the thing we gated off). Retail log landmarks (compact retail614.log): L9632 and L10335 (d=173
segments), L10323/L11026 composites.
TODO for the fix: identify 11200/11584 by dumping them from the retail dump's VRAM (grab() with
the right tw/th from the TEX0 lines) and matching against the ROM resources (TEXCKABE? TEXCSTSL?);
find the ROM function (osdsys_dump.idb, python-idb venv at /u/aap/src/osdsys/.venv) and port it.

## Finding 4 — backdrop segment differences (minor)

Retail backdrop: 1 sprite + 16 IIP tristrips (60 verts each, ST/Q coords, additive rays fading
along their length, vertex alpha 0x40), segment alphas 0101 AND 0201. Ours: 2 sprites + 16
strips, alpha 0201 only. Worth a close diff of the sprite(s) and blend modes (could relate to
the reported background flicker).

## Finding 5 — rod (clock) passes are structurally IDENTICAL; differences are data-level

Per-rod: 11ish quads bump(2 taps, subtractive 2001 first, then 0201)+refract(AA1+FST, ALPHA
0101, sampling wb4) into wb4 -> ~5 near quads to screen sampling wb4 -> same into wb3 sampling
wb3. Counts, ALPHA/ztst sequences, colour scheme (cap faces dark teal 2d5f66/33656c/407279,
side faces white), CLAMP writes — all match. So "clock has no refraction" is NOT a missing pass.
Remaining candidates, in order: (a) Finding 2 (orbs absent from the sampled buffers); (b) exact
refraction UV displacement — spot readings: retail rod cap face UV-vs-pixel offset ~(-1.0,-0.8),
ours ~(+1.0,-2.9) at a roughly similar pose — same ballpark but possibly wrong sign/scale;
verify properly by re-running our build at a pose matched to a retail dump (clock argv) and
comparing displacement residuals per face; (c) whether the same AA1/stale-Z repair mechanism
from Finding 1 is also nibbling the rod bump taps (rods use real Z legitimately — check where
each side CLEARS Z within the frame; grep the logs for big Z-writing clears).

## PCSX2-dump reading gotchas (learned the hard way)

- Retail uses REGLIST-format GIF packets for vertices; ours is all A+D. Both decode fine.
- One GifWalker per path; a single stray path-0 transfer desyncs everything otherwise.
- The backdrop/bump passes use ST (not UV): drawlog's uv column is stale there.
- The clock-time argv seeds our pose; retail dumps are at 7:06:0x real time — our known-good
  config argv: `menu 18 27 45 0 1 128 145 0 0 0 10 0 1 0 1` (OsdArgInt(n) = token n+1).
