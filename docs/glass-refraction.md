# OSDSYS glass: transparency, refraction and bumpmap on the GS

How the refractive "glass" objects of OSDSYS are drawn — the opening's
spinning cubes, the illegal-disc cubes, and the System Configuration
screen's five cubes and twelve-rod clock. The three are **not one
renderer**: they share the idea (sample a copy of the frame by screen
position and displace the UV by the surface normal) but not the code, the
buffers, or the compositing. This doc isolates the technique so it can be
lifted into another PS2/GS project, then maps every generic step to the
exact osdbits function and retail address it came from.

Addresses are retail-image addresses (base 0x200000, gp = 0x2AF070).
Companion notes: docs/menu-config.md (the config scene), docs/menu-draw.md
and docs/menu-backdrop.md (the work-buffer plumbing), docs/towers-analysis.md
(the opening). No Sony data appears here — only GS register state, blend
math, and pass structure.

## 0. The GS primitives the whole thing is built from

Everything below is ordinary GS 2D/3D drawing; there is **no special
"refraction" hardware**. Three register groups carry the effect.

**ALPHA_1 — the blend equation.** `Cv = (A - B) * C + D`, operands
`A,B ∈ {Cs, Cd, 0}`, `C ∈ {As, Ad, FIX}`, `D ∈ {Cs, Cd, 0}`. Both engines
select it through the same `BlendModes[]` table (opening.c:372) via
`vif1SetAlphaBlend(type, mode, fix)`; `type` is PABE (1 = blend on). The
modes that matter:

| mode | A,B,C,D | equation | ALPHA hex | use |
|---|---|---|---|---|
| 4 | 0,1,0,1 | (Cs−Cd)·As + Cd | 0x44 | source-alpha blend |
| 5 | 0,2,0,1 | Cs·As + Cd | 0x48 | additive by As |
| 6 | 2,0,0,1 | Cd − Cs·As | 0x42 | subtractive by As |
| 0 | 0,2,2,1 | Cs·FIX + Cd | — | additive by FIX |
| 2 | 0,1,2,1 | (Cs−Cd)·FIX + Cd | — | blend by FIX |
| 8 | 0,2,1,1 | Cs·Ad + Cd | — | additive by dest-alpha |

**TEX0_1 / TEX1_1 / TEXA — the sampler.** TFX is always `MODULATE`
(texel × vertex colour). TEX1 forces `MMAG = MMIN = LINEAR` (0x61) for the
blits and glass — **the bilinear taps are the whole effect**; NEAREST would
make a lossy resample and nothing else. TEXA supplies the alpha for the
24-bit screen copies (`{0x7F, 1, 0x81}`). CLAMP_1 is pushed **per
primitive**: the glass overrides `sceGsSetDefTexEnv`'s CLAMP/CLAMP to
REPEAT/REPEAT so the screen-position UV biases wrap instead of clamping to
one edge row.

**TEST_1 / FRAME_1 / ZBUF_1 — depth and target.** ZMSK is **0
everywhere** in this code (FRAME and ZBUF are pushed together and the ZBUF
never masks); Z *writes* are on so an AA1 partial-coverage pixel can be
re-blended to solid by the next primitive over it. AA1 (antialias) is what
brightens the silhouette edge. `vif1SetFramebuffer` retargets FRAME between
the screen and the off-screen work buffers between passes.

**The half-pixel rule** (load-bearing, learned in commit 37efd18). A
per-field display correction (`XYOFFSET += 0.5` on one field) must ride
along on a **mesh drawn for the display** and must **not** ride on a
**buffer-to-buffer resample**. Every engine below tags each pass with its
field flag accordingly; get it wrong and a buffer-to-buffer blur lands
0.5 px off, magnified per pass, on one field only — a second ghost image
at 60 Hz.

## 1. The generic technique — refractive glass on the GS

The shared idea, independent of which engine implements it:

1. **Keep a copy of the frame the glass will refract.** Either a
   pre-object copy of the whole screen (config path), the live composited
   screen sampled directly (config cubes' outer layer), or a per-object
   snapshot captured just before the object draws (opening path). This copy
   is the refraction source texture.
2. **For each face, compute a Fresnel term** = `1 − |normalize(v.cam) ·
   faceNormal_cam|`: 0 head-on, 1 edge-on. The glass is drawn *bright only
   at its silhouette*, so a face contributes a rim of light and is nearly
   invisible face-on. (opening.c uses a per-vertex lighting variant of the
   same dot; menuconfig.c's `MeshFresnel`, 0x22C888/0x22CCE8 head.)
3. **Draw each face as a textured tristrip whose UV is the vertex's own
   screen position, displaced by the surface normal.** Sampling the frame
   copy at `(screenPos + k·normal)` instead of at `screenPos` is the
   refraction: the surface shows the background shoved sideways by an amount
   proportional to how much the face tilts. The displacement scales with
   `1/w` (screen-space, so far objects bend less) and is taken about a
   shrunk-toward-centre point so the glass also acts as a weak lens.
4. **Layer front and back faces through an off-screen buffer for a
   two-depth look.** Back (far) faces are drawn into a work buffer
   refracting the frame copy; front (near) faces then refract *that work
   buffer*. Two stacked refractions read as a solid piece of glass with a
   visible interior rather than a single tinted sheet.
5. **Add surface detail** — either scrolling environment textures
   (opening) or a two-tap emboss bumpmap (config) — over the refraction.
6. **Composite back to the screen through an alpha mask**, so the glass
   replaces the frame only inside its own silhouette and leaves everything
   else (other layers, tint) untouched (config cubes). Objects that instead
   draw their visible pass *straight onto the screen* (config rods, opening
   cubes) need no mask but must therefore refract an **un-tinted** copy, or
   they glow — see §5.

The rest of this doc is which osdbits function does each of these, and
where the two engines make opposite choices.

## 2. Opening cubes — per-object screen capture (opening.c)

Structure: `DrawCube` (0x217520) once per instance, five back-face passes
into a work buffer then five front-face passes onto the screen, driven off
a 10-entry pass table `cubePasses[]` (opening.c:1641). `DrawIllegalCube`
(0x219748) is the same machinery with `illegalCubePasses[]` — a red colour
variant, zoom 0.8 instead of 1.0, env offsets zeroed.

- **Refraction source = a fresh per-cube snapshot.** `CubeCaptureBuffer`
  (real sub_21c7a8) stamps the current screen (towers + composite + fog, as
  drawn *so far this frame*) into the shared work buffer `extraBuf2`, as
  PSMCT24. Each cube captures immediately before it draws, so the pipeline
  has **no cross-frame reference** — selector −2 = the in-progress frame,
  −3 = that capture; the displayed/previous buffer is never sampled.
- **UV builders** (`CubeComputeUV`, real cube_21B798 / cube_21BBE0 /
  cube_21BA08):
  - mode 0 (the refraction pass): `uv = screenPos − screenOrigin +
    screenNormal·(screen/2)·uvArg2·q·4 + (screenPos − center)·uvArg`,
    clamped to the screen rect. The `screenNormal` term is the normal-driven
    displacement; the `(pos − center)·uvArg` term is the centre-relative
    zoom — `uvArg = −0.084` on the front-face working-buffer sample is the
    ~8 % refraction lens.
  - mode 1 (cube_21BBE0): a scrolled unit quad — the env layers.
  - mode 2 (cube_21BA08): a fake spherical env map (view dir + scaled
    projected normal, xy-swapped, 0.5-centred).
- **The 10 passes** (`DrawCubePass`, real DrawTexturedQuad 0x21c560, one
  packet per group). Back group (targetWork = 1) then front group
  (targetWork = 0), each: **[screen sample]** → **BLPR** → **REF** →
  **BLP** → **REF**. The screen-sample pass has `abe = 0` (opaque write of
  the refracted frame copy); the four env layers are real texture resources
  scrolled at `±0.00375 / ±0.0075` and blended additively (mode 5) and by
  dest-alpha (mode 8). `blendFix` doubles as a colour-brightness scale
  (`CubeVertexColor` `s = fix/128`). Lighting is fixed **grayscale**
  (`cubeColor1/2 = {0.5}`), i.e. a reflective look, not a coloured one.
- **No bumpmap.** The opening cube's surface detail is *scrolling env
  textures*, not an emboss. There is no TEXCBUMP anywhere in the opening.
- **No masked composite.** The front passes target the real framebuffer
  directly and the sequence ends on screen passes, so the opening never
  needs a FRAME restore and never builds an alpha mask. It gets away with
  this because each cube refracts its **own capture of the finished
  frame** and simply blends over it.

So the opening cube is: *snapshot the frame, draw the back faces
refracting it into a scratch buffer, draw the front faces refracting the
scratch buffer onto the screen, scrolling four env layers over each.*

## 3. Config cubes — shared work buffers + emboss + masked composite (menuconfig.c)

This is the elaborate one, and the one commit 509a89b fixed. The scene has
**five work-buffer-sized surfaces** in GS memory (two colour, one Z, two
work buffers `wb3`/`wb4`); osdbits borrows the opening's `extraBuf1` =
**wb3** and `extraBuf2` = **wb4**. Before the object list runs,
`MenuBackdrop` (0x21D0A0, menuback.c) has already taken two copies of the
frame into wb3 and wb4 and re-tinted the screen (see §5).

The geometry pipeline is one transform (`MeshTransform`, 0x22CFA8) into a
per-face record, then one emit per pass. Five emit kernels, each a distinct
GS PRIM:

| emit | real | PRIM | what |
|---|---|---|---|
| `MeshEmitFace` | 0x22C4E0 | 276/404 TME+FST | **the refraction** — UV = screen pos |
| `MeshEmitBumpFace` | 0x22C920 | 84 TME+ABE, ST | **the emboss** — model UV over TEXCBUMP |
| `MeshEmitFlatFace` | 0x22CB58 | 196 ABE+AA1, no TME | colour × fres² |
| `MeshEmitBlackFace` | 0x22CA68 | 132 AA1, no TME/ABE | black, **A = 0x80 alpha-only** |
| `MeshEmitReflFace` | 0x22CD78 | 276/404 TME | spherical env map (TEXCREFA) |

**The refraction UV** (`MeshEmitFace`): the vertex's own projected screen
position, shrunk 5 % toward the object's screen centre (`meshRefX/Y`), then
pushed by the **camera-space face normal** scaled `×1000` horizontally and
`×500` vertically **times 1/w**:

```
nx = normal.x * 1000 * q;   ny = normal.y * 500 * q;
u  = (proj.x − 2048 − refX)*0.95 + refX − nx + screenW/2 + 1024;   clamp u ≥ 1024
v  = (proj.y − 2048 − refY)*0.95 + refY − ny + screenH/2 + 256 − field*0.5;
```

The `+1024 / +256` biases are undone by the per-primitive CLAMP_1→REPEAT
override; the refraction magnitude is `normal·1000/500·q`, i.e. inversely
proportional to the object's `w` (which is exactly why running these cubes
through the wrong camera made them look flat as well as small — see
`cubeCamera`, §5). The face is bright at the silhouette via the Fresnel
`size·10·fres⁴` term with a cosine roll-off past `fres > 0.9`.

**The bumpmap** (`MeshEmitBumpFace`, the "bumpmapped" look). A separate
64×64 grey page, **TEXCBUMP** (TEXC slot 2, decoded by the grey expander
0x22A720 as `b|b<<8|b<<16|0x7F000000`). It is emitted **twice per face
set** at two slightly different model-UV offsets: additive (mode 5, ALPHA
0x48) at offset `+0.01` and subtractive (mode 6, ALPHA 0x42) at offset `0`.
The difference of one texture against itself at two offsets is a **classic
emboss** — `∂texture/∂uv` in the offset direction — capped by the
`{8,8,8,128}` MODULATE colour to a ±15-level relief on top of the
refraction. **TEXCBINV** (slot 3) is the exact bitwise complement of
TEXCBUMP, which is what makes the two walks of the deferred bloom a genuine
emboss *pair* (§4).

**The eight passes** (`MeshDrawCube`, 0x22D2E8) — none draw on the screen;
`far` = the cull==0 winding set, `near` = cull!=0, far always first:

```
1  FRAME wb4, TEX the LIVE tinted screen (0x22A198, PSMCT24)   far   refraction
2  TEXCBUMP additive  far  emboss +0.01
3  TEXCBUMP subtractive far emboss 0
4  FRAME wb3, TEX wb4   far   flat colour×fres²
5  ZTST GREATER         near  black (primes wb3's cube area)
6  wb3 → wb4 additive, LEFT HALF only (0x22C100)  — so pass 7 can sample it
7  FRAME wb3, TEX wb4   near  refraction  (near glass refracting wb4)
8  TEXCBUMP ×2          near  emboss
```

Pass 1's texture is **the live, composited, tinted screen**, so the outer
glass layer refracts the finished frame; wb4 collects the far glass, wb3
collects a black cube silhouette with the near glass refracting wb4 painted
over it.

**The mask + composite** — the reason the whole chain exists. After all
five cubes run the eight passes, `0x226D00`'s tail does a **second walk**
(`MeshDrawCubeMask`, 0x22D798) into a wb4 freshly cleared to **alpha 0**
(clear record 0x27F180 `{0,0,0,0}`): a TEXCREFA spherical-env reflection
(`MeshEmitReflFace`) for colour, then `MeshEmitBlackFace` whose `A = 0x80`
is the **mask**. Then:

```
0x22C088  wb4 → wb3 additive   — colour adds ~nothing (wb4 is black),
                                  but As (the 0x80 silhouette) is copied
                                  into wb3's ALPHA channel
0x22C190(abe=1)  wb3 → screen, ALPHA 0x44 = (Cs−Cd)·As + Cd
```

`0x22C190(1)` blends wb3 over the screen **by that stamped alpha**, so the
cube stage replaces the frame *exactly inside the cubes* and leaves the
tunnel, orbs, rods and tint untouched everywhere else. Without this masked
composite the cube stage would wipe the rest of the frame.

**Why the collapsed version looked opaque** (the bug 509a89b fixed). An
earlier port (eba5595) collapsed the eight loops to two, drawn straight on
the screen, and bound wb3 as the refraction source because binding the
screen "measurably killed" the cubes. Both were the same mistake: **wb3 is
the pre-tint copy** — `MenuBackdrop` copies the frame into wb3/wb4 *before*
the composite multiplies the screen by `{0x37,0x28,0x3C}/128`
(`compositeColor`, menuback.c:866). So the collapsed chain painted a ~3×
brighter, un-tinted backdrop onto the tinted backdrop: measured 3–6× too
bright, exactly the reciprocal of the tint — glass that glows reads as a
solid milky block. The ROM has no such problem because pass 1's texture
**is** the tinted screen and the result reaches the frame only through the
masked composite.

## 4. Config clock — the twelve glass rods (menuconfig.c)

The clock is a ring of twelve hexagonal-prism rods (`rodModel`, 16 faces),
the same geometry renderer as the cubes over a different scene struct. It
is **config-menu only** — there is no clock in the opening or illegal
scenes. Two differences from the cubes make the rods **glow** where the
cubes do not:

- **The rods draw their visible pass straight on the screen.**
  `MeshDrawRod` (0x22D920, the `0x22E0EC` arm) runs five passes:

  ```
  1  FRAME wb4, TEX wb3   far   refraction   (far glass refracting wb3)
  2  TEXCBUMP subtractive far   emboss (phase = slot·0.1 + faceIdx·0.1)
  3  TEXCBUMP additive    far   emboss (offset −0.008 + phase)
  4  FRAME the SCREEN, TEX wb4  near  refraction  ← the only visible pass
  5  FRAME wb3, TEX wb4    near  refraction, washed out (extra 255)
  ```

  Pass 4 puts the rod on the screen with **no work-buffer composite and no
  mask**, so it legitimately samples the **un-tinted** wb3/wb4 chain and
  draws it onto the tinted frame — that is *why retail's rods glow and its
  cubes do not*. Note the emboss order (subtractive then additive) is the
  **mirror** of the cubes', and the rod's emboss offset is **−0.008** vs
  the cube's **+0.01**; the refraction centre shrinks by **0.9** vs the
  cube's **0.35**.

- **The front rod is split along Y** (`MeshDrawRodSplit`, 0x22D9D4): a lower
  segment growing at 0.004/frame toward `minutes/60` and an upper segment
  translated up by `26·progress·split` (26 = the rod's model height), so the
  cut is seamless and the bright lower segment reads as the clock's minute
  progress bar. Same five passes, each walking both face banks (lower faces
  8..15, upper all but 8–9), with the upper piece's emboss T continuing
  `+2·progress·split` where the lower's ends.

- **The deferred bloom** (`MeshFlushSplit` / real 0x22E428, simple arm
  0x22E9A8) is the clock's sparkle and the last render stage of the screen.
  Per rod, two walks composited additively at alpha 30: a TEXCFLOW
  spherical-env rod (the same `reflect()` env map as the cube mask, ABE/AA1
  clear) with a subtractive emboss over it, the two walks differing **only**
  in which half of the emboss pair they use — TEXCBUMP on one, TEXCBINV
  (its bitwise complement) on the other. Two near-cancelling layers whose
  surviving difference is the highlight.

## 5. Where the paths diverge, and where they share

**Shared:** the *concept* (screen-position UV displaced by the normal, an
edge-brightening Fresnel/lighting term, back-then-front layering for
two-depth glass, LINEAR taps, ZMSK 0). Both use the same `BlendModes[]`
table and the same `extraBuf1/extraBuf2` GS buffers.

**Divergent — they share almost no code:**

| | opening cubes (opening.c) | config cubes (menuconfig.c) | config rods (menuconfig.c) |
|---|---|---|---|
| driver | `DrawCube` 0x217520 | `MeshDrawCube` 0x22D2E8 | `MeshDrawRod` 0x22D920 |
| refraction source | per-cube snapshot (sub_21c7a8) of the finished frame | **live tinted screen** (far) + wb4 (near) | **un-tinted** wb3 (far) + wb4 (near) |
| camera | frame camera | **identity** — positions are camera-space | frame camera |
| surface detail | 4 scrolling env textures | TEXCBUMP two-tap emboss | TEXCBUMP emboss + TEXCFLOW bloom |
| UV builders | cube_21B798/BBE0/BA08 | `MeshEmitFace` 0x22C4E0 | `MeshEmitFace` 0x22C4E0 |
| reaches screen via | front passes draw on screen | **masked composite** 0x22C190(1) | pass 4 draws on screen |
| refraction centre | `uvArg −0.084` zoom | `×0.35` | `×0.9` |

The single most important divergence is #5/#6 of §1: **config cubes reach
the frame only through an alpha-masked composite of a work buffer, so they
refract the tinted screen; config rods and opening cubes draw their visible
pass straight onto the screen, so they must refract an un-tinted copy.**
Mixing those up is precisely the "opaque milky cubes" bug.

**Config cubes have no camera.** `cubeCamera` (0x352840) is set to the
**identity** by the per-screen init 0x228460 and never touched again, so
the five cubes' table positions are already camera-space (47.5 units from
the eye). The rods, by contrast, get the frame's live camera written into
their scene struct every frame (0x2268F0). Running the cubes through the
rod camera made them 3.2× too small and cut the 1/w refraction offset by
the same factor.

## 6. Lifting it into another project

Minimum viable refractive glass on the GS, mapped to the osdbits reference:

1. Reserve one screen-sized PSMCT32 work buffer (two for a two-depth look).
   → `gsAllocBuffer` / `extraBuf1,2` (opening.c:336).
2. Each frame, copy the finished, *display-tinted* frame you want to
   refract into the source texture — and remember **which copy is tinted**.
   → `MenuBackdrop` 0x21D0A0 (menuback.c:964).
3. Transform the object; per face compute the camera-space normal and a
   Fresnel term `1 − |normalize(v.cam)·n|`.
   → `MeshTransform` 0x22CFA8, `MeshFresnel` 0x22C888.
4. Emit each face as a MODULATE tristrip, **UV = the vertex's projected
   screen position**, displaced by `n·k·(1/w)` about a centre-shrunk point,
   CLAMP_1 forced to REPEAT, LINEAR sampling, blend mode 4.
   → `MeshEmitFace` 0x22C4E0.
5. (Optional) emboss: emit the same faces twice over a grey bump page at
   two small model-UV offsets, additive then subtractive.
   → `MeshEmitBumpFace` 0x22C920, TEXCBUMP decode 0x22A720.
6. Layer far faces into a work buffer, then near faces refracting that work
   buffer; if you draw to a work buffer rather than the screen, build an
   `A = 0x80` silhouette mask in a second buffer, copy its alpha into the
   colour buffer with an additive blit, and composite over the screen with
   `(Cs−Cd)·As + Cd`.
   → passes of `MeshDrawCube` 0x22D2E8 + `MeshDrawCubeMask` 0x22D798 +
   `0x22C088` + `0x22C190(1)`.
7. Tag display meshes with the field half-pixel and buffer-to-buffer blits
   without it. → `MenuBackWorkTarget` / `BackHalfOffset` (menuback.c),
   commit 37efd18.

The simpler recipe (no mask) is the opening's: snapshot the frame per
object, draw back faces refracting it into a scratch buffer, draw front
faces refracting the scratch buffer straight onto the screen. It costs a
per-object full-screen capture but needs no masked composite.
