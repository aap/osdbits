# Module U — the 2D/UI draw layer

How menu items and UI elements become pixels in **Module U**
(0x21C910–0x230000, the "clock" module in HDDOSD naming: main menu +
System Configuration + Version Information + first-boot wizard).

Companion to `osdsys-map.md`. Same confidence convention:
**[ok]** = read out of the disassembly and cross-checked; **[tnt]** =
plausible, partially traced; **[?]** = guess. Unlabelled = [ok].

Ground truth: `ee-objdump -m mips:5900` of `/u/aap/src/osdsys/expanded.bin`
(VA = file offset + 0x200000), `gp = 0x2AF070`, plus the IDB for names and
the HDDOSD 1.10U symbol list for hints (all HDDOSD-derived names were
re-verified against the retail disassembly before being used).

---

## 0. Corrections to earlier documents

Three claims in circulation are wrong and they change the shape of the
problem.

**0.1 `menuElemVertTable` 0x284100 is Module V only.** `osdsys-map.md`
§5.3 says it "is referenced by ten functions across both modules, so it is
shared". It is not: an absolute-address reconstruction over the whole code
segment finds exactly ten referencing sites, and **all ten are in Module V**
— 0x237E88 (×2), 0x238228 (×3), 0x23B6F8, 0x241C78, 0x245038, 0x247E90,
0x248370, 0x248548, 0x24C070. **Module U never touches 0x284100.** Module U
has no shared static vertex table at all; its 2D geometry is generated per
draw from the sprite records described in §3.

**0.2 Module U does not use `sceVif1Pk`, and does not share Module V's
packet layer.** `menu-rendering-reverse.md` describes 0x2403C8 / 0x240438 /
0x245038 / 0x246D28 — every one of those is in **Module V** (0x231000+).
Module U has its own, structurally similar but *hand-inlined* packet layer
at **0x2293E0 / 0x2294B8** (§2), and its own texture cluster (§5). The two
modules meet only at the `sceGs*` library and at 0x230000.

**0.3 `draw_menu_item` is at retail 0x21DC88, not ~0x21DCD0.** The
HDDOSD→retail drift in this neighbourhood is **+0x8F78**, not +0x8F30
(DrawIcon 0x226508→0x21D590, draw_button_panel 0x226770→0x21D7F8,
draw_menu_item 0x226C00→0x21DC88 — all three confirmed by exact size match
and by body shape). And the HDDOSD names are misleading: the three
functions are a family of *text* helpers distinguished only by horizontal
alignment (§6.2), with no notion of a "menu item" in them.

**0.4 `0x1F0A10` is the GS double-buffer struct, not a message ring.**
`osdsys-map.md` §1.6 lists `+0xA10 dbuff (idb) — the message ring buffer
itself`. `InitDraw` (0x205CE0) passes `0x201F0A10` (= 0x1F0A10 |
0x20000000, uncached) to **`sceGsSetDefDBuff`** (0x262828) and then twice to
`sceGsSwapDBuff` (0x2627C8). The IDB name `dbuff` means *draw buffer*. Both
Module U and Module V push draw environments out of it every frame. The
message ring is only 0x1F09F8–0x1F0A0C.

---

## 1. The coordinate system and display setup

`InitDraw` (0x205CE0) [ok]:

```
0x1F0C50 (screen_width)  = 640
0x1F0C54 (screen_height) = 224 (NTSC) / 256 (PAL)     ; IsPAL = 0x204350
sceGsResetGraph(2, 1, mode = IsPAL ? 2 : 3, 1)
sceGsSetDefDBuff(0x1F0A10|uncached, psm=0, 640, h, 2, 48, 1)
sceGsSwapDBuff(dbuff, 0); sceGsSwapDBuff(dbuff, 1)
0x1F0C40 = 1                                          ; current draw buffer index
```

So **the whole OSD is drawn at 640 × 224 in one field**, PSMCT32, with
`sceGsSetHalfOffset` re-applied every frame from the field parity
(`0x1F0C44`, `evenOddField`). Everything downstream follows from this:

* UI Y coordinates run 0…223. The button hint bar sits at y = 200 (4:3) or
  182 (16:9) — near the bottom of the screen (§9.1).
* A square icon is drawn **28 px wide by 14 px tall** (`DrawIcon`, §9.1)
  because one field line covers two source rows.
* Sprite-record coordinates are in **1/16 pixel** and are pre-offset by the
  GS origin inside the packet builder (§3), so record x/16 and y/16 are
  plain screen pixels with (0,0) at the top-left.

`0x21C9D0` (per-frame setup, called from `0x21CA38` before the frame body):

```
sceGsResetPath()               ; 0x262EB0
sceDmaReset(1)                 ; 0x266E80 — VIF1 channel
0x27B440[+0x0C] = 1.0
0x27B440[+0x10] = IsPAL ? 0.47 : 0.5405      ; gp-32232 / gp-32236
```

0.5405 / 0.47 is the **pixel aspect ratio** (physical height per unit
width) of the 640×224 NTSC and 640×256 PAL field. It is used by the
letterbox code and by the button-bar Y computation.

---

## 2. The packet layer — hand-rolled VIF1 DIRECT chains in the scratchpad

Module U builds GIF packets itself, in the **scratchpad (0x70000000)**,
double buffered in two 8 KB halves, and kicks them on **VIF1 (DMA channel
1)** with `sceDmaSend`. The packet context is a 24-byte struct that always
lives on the caller's stack.

### 2.1 The packet context

| offset | field |
|---|---|
| +0x00 | `cur` — write pointer |
| +0x04 | `base` — chain head (the DMA source address) |
| +0x08 | `tag` — pointer to the open DMAtag, for QWC patch-up |
| +0x0C | `vifcode` — pointer to the open VIF1 `DIRECT` code, for its count |
| +0x10 | (unused) |
| +0x14 | `giftag` — pointer to the open GIFtag, for NLOOP patch-up |

### 2.2 `0x2293E0(pkt)` — open [ok]

```
idx  = *(u32*)(gp-30388)            ; 0x2A79BC, 0/1
base = 0x70000000 | (idx << 13)     ; 8 KB per half
pkt->cur = pkt->base = base
store DMAtag 0x10000000 (id=CNT, qwc=0) at base ; pkt->tag = base
pad to 16 B with zeros
store VIFcode 0x50000000 (DIRECT, count=0)      ; pkt->vifcode = here
*(gp-30388) = (idx + 1) & 1
```

### 2.3 `0x2294B8(pkt)` — close and kick [ok]

In order:

1. **Patch NLOOP** into the GIFtag at `pkt->giftag`:
   `n8 = (cur − giftag)/8`, `nreg = tag.NREG ? tag.NREG : 16`,
   `flg = tag & (3<<58)`; `q = flg ? n8 : n8/2`;
   `nloop += (q − 2 + nreg − 1) / nreg` — i.e. ceil-divide the payload by
   the register count. (Verified numerically against both templates: a
   32-byte PRIM+RGBAQ packet yields NLOOP 1, a 48-byte UV/XYZ packet
   yields NLOOP 2.)
2. Pad to 16 B, **patch the VIF1 DIRECT count** = qwords since the code.
3. Pad to 16 B, **patch the DMAtag QWC** = qwords since the tag − 1.
4. Append a `0x70000000` DMAtag (id = `end`), pad, patch its QWC.
5. `sceGsSyncPath(0,0)` (0x262418).
6. `chan = sceDmaGetChan(1)` (0x266F60); `chan->chcr |= 0x40` — **TTE**,
   transfer tag enable, so the VIF codes embedded in the tags are executed.
7. `sceDmaSend(chan, (pkt->base & 0x3FF0) | 0x80000000)` (0x266BD0) —
   bit 31 marks a scratchpad source address.

This is the exact analogue of the opening module's `vif1Begin`/`vif1End`
(`osdbits/opening.c`), and of Module V's `sceVif1Pk` wrappers, but written
out by hand.

Callers of the open/close pair: 0x2287E0, 0x229130, 0x2299C0, 0x22A030,
0x22CA68, 0x22CB58, 0x22D2E8, 0x22D798, 0x22D920, 0x22E428, 0x22EFF0
(+ 0x22ED10 for close only).

---

## 3. The sprite primitive — this is "how a UI element becomes pixels"

Every flat UI element in Module U (icons, panels, bars, full-screen blits,
the letterbox, the fade curtain) is drawn by **one function**, `0x2299C0`,
from **one 0x40-byte record**. This is Module U's "element".

### 3.1 The record  [ok]

Derived from `0x2297E8` + `0x2298A8` and confirmed against the static
initialiser of the icon record at 0x27B530.

| offset | type | meaning |
|---|---|---|
| +0x00 | u32 | **R** (low byte used) |
| +0x04 | u32 | **G** |
| +0x08 | u32 | **B** |
| +0x0C | u32 | **A** (0x80 = opaque) |
| +0x10 | s32 | **x0** — screen X of the top-left corner, 1/16 px |
| +0x14 | s32 | **y0** — screen Y, 1/16 px |
| +0x18 | s32 | **u0** — texture U, 1/16 texel |
| +0x1C | s32 | **v0** — texture V, 1/16 texel |
| +0x20 | s32 | **x1** — bottom-right X, 1/16 px |
| +0x24 | s32 | **y1** |
| +0x28 | s32 | **u1** |
| +0x2C | s32 | **v1** |
| +0x30 | u32 | **Z** |
| +0x34 | u32 | **ABE** — alpha-blend enable (PRIM bit 6) |
| +0x38 | u32 | **TME** — texture-map enable (PRIM bit 4) |
| +0x3C | — | padding |

The colour quadruple at +0x00…+0x0C is reused on its own as a
"colour record" for text (§6.2) and for the GS clear colour (§4.1).

### 3.2 `0x2299C0(rec)` — draw one sprite  [ok]

Two independent packets, opened and kicked separately:

```
0x2293E0(&pk); 0x2297E8(&pk, rec); 0x2294B8(&pk)   ; PRIM + RGBAQ
0x2293E0(&pk); 0x2298A8(&pk, rec); 0x2294B8(&pk)   ; UV + XYZF2 (2 corners)
```

**`0x2297E8`** copies the GIFtag template at **0x2A4B68**
(`EOP=1, FLG=REGLIST, NREG=2, REGS={PRIM(0x00), RGBAQ(0x01)}`) and appends
one data qword:

```
PRIM  = 6 | 0x100 | (rec->TME << 4) | (rec->ABE << 6)
        ; 6 = SPRITE, 0x100 = FST (use UV, not ST/Q)
RGBAQ = rec->R | rec->G<<8 | rec->B<<16 | rec->A<<24 | (1.0f << 32)
```

**`0x2298A8`** copies the GIFtag template at **0x27F2C0**
(`EOP=1, FLG=REGLIST, NREG=2, REGS={UV(0x03), XYZF2(0x04)}`) and appends
two data qwords, one per sprite corner:

```
xoff = (2048 − screen_width /2) << 4
yoff = (2048 − screen_height/2) << 4
qw0 = { UV: u0 | v0<<16 , XYZF2: (x0+xoff) | (y0+yoff)<<16 | Z<<32 }
qw1 = { UV: u1 | v1<<16 , XYZF2: (x1+xoff) | (y1+yoff)<<16 | Z<<32 }
```

The `xoff`/`yoff` term exactly cancels `sceGsSetDefDrawEnv`'s
`XYOFFSET_1 = (2048 − w/2, 2048 − h/2)`, which is why record coordinates
are plain top-left-origin screen pixels ×16.

Note what is **not** in the packet: no TEX0, no TEST, no ALPHA. Texture
binding and blend state are separate `sceGsPutDrawEnv` pushes (§4), so a
caller's shape is always *set state → fill record → `0x2299C0`*.

### 3.3 Everything drawn as a sprite

| record | drawn by | what |
|---|---|---|
| 0x27B4B0 | 0x21D0A0 | full-screen blit of the composited scene (RGB 0x37/0x28/0x3C, A 0x80) |
| 0x27B4F0 | 0x21D1F8 | the two 16:9 **letterbox bars** (drawn twice) |
| 0x27B530 | 0x21D590 `DrawIcon` | controller-button glyph, 28×14 or 25×12 |
| 0x27B7F0 | 0x21DB18 | a 2 px full-height strip at the right screen edge [tnt] |
| 0x27F630 | 0x22AFB8 | **fade-to-black curtain**, RGB 0,0,0, A = 128 − fadeCounter |
| 0x27F760 | 0x22C190 | full-screen textured blit, ABE from the argument |
| 0x27F820 | 0x22C3C0 | **zoom-blur** feedback pass, N expanding blits |
| (stack) | 0x224630, 0x225038, 0x226768, 0x22C088/100/2A0, 0x22EFF0 | wizard images, resource images, scene overlays |

---

## 4. GS state — libgraph draw environments, not custom VIF chains

Module U changes GS state by mutating a handful of *static*
`sceGsDrawEnv`-shaped structs (each an array of `{u64 data; u64 addr}`
A+D pairs behind a GIFtag) and pushing them with **`sceGsPutDrawEnv`**
(0x262AD8). All of them are addressed through the uncached mirror
(`| 0x20000000`).

Two previously-unnamed `sceGs` helpers, decoded here:

* **`0x2622A8(p, pabe)`** — fills four A+D pairs:
  `ALPHA_1 (0x42) = 0x44`, `PABE (0x49) = pabe`,
  `TEXA (0x3B) = 0x81_0000807F`, `FBA_1 (0x4A) = 0`. Returns 4. [ok]
* **`0x262308(p, flush, tbp, tbw, …, tw, th, …)`** — fills four A+D pairs:
  `TEXFLUSH (0x3F)` (or the NOP register 0x7F when `flush == 0`),
  `TEX1_1 (0x14)`, `TEX0_1 (0x06)`, `CLAMP_1 (0x08) = 5` (CLAMP/CLAMP).
  Returns 4. [ok]

(The GS register numbering that makes all of this consistent —
0x40 SCISSOR_1, 0x42 ALPHA_1, 0x46 COLCLAMP, 0x47 TEST_1, 0x4C FRAME_1,
0x4E ZBUF_1 — was cross-checked against `sceGsSetDefDrawEnv` 0x262CC8,
which writes exactly {FRAME_1 76, ZBUF_1 78, XYOFFSET_1 24, SCISSOR_1 64,
PRMODECONT 26, COLCLAMP 70, DTHE 69, TEST_1 71}. `TEST` field packing is
contiguous: ATE(1) ATST(3) AREF(8) AFAIL(2) DATE(1) DATM(1) ZTE(1)
ZTST(2), so `ZTE` is bit 16 and `ZTST` bits 18:17.)

### 4.1 `0x22A3B8(dbuff, bufSel, clearColorRec, fieldParity)` [ok]

Re-points the GS drawing environment at the front or back half of the
shared `sceGsDBuff` at 0x1F0A10, using the two drawenv sub-structs at
`+80` (`bufSel == 0`) and `+320` (`bufSel != 0`). If `clearColorRec` is
non-NULL its four words are packed into the buffer's clear-colour RGBAQ.
Then `sceGsSetHalfOffset(env, 2048, 2048, fieldParity)` (0x261EB8) and
`sceGsPutDrawEnv`. Almost every drawing stage begins with
`0x22A3B8(0x1F0A10, *(0x1F0C40), …, *(0x27B448))`.

### 4.2 `0x22A0C0(blendMode, ztst)` [ok] — the blend/depth switch

Struct 0x27F2F0. Writes `TEST_1 = (ztst << 17) | 0x10000` (ZTE = 1,
ZTST = `ztst`) and `ALPHA_1` picked by `blendMode` through the 5-entry
jump table at **0x2A4B80**:

| mode | ALPHA_1 | formula |
|---|---|---|
| 0 | 0x48 | `Cs*As + Cd` — additive |
| 1 | 0x44 | `(Cs−Cd)*As + Cd` — normal alpha blend |
| 2 | 0x42 | `(0−Cs)*As + Cd` — subtractive |
| 3 | `FIX=0x28`, 0x64 | `(Cs−Cd)*40/128 + Cd` — fixed 31 % blend |
| 4 | 0x68 | `Cs*FIX + Cd` |

Observed `ztst` values are 1 (ALWAYS), 2 (GEQUAL), 3 (GREATER). 22 call
sites; the UI layer almost always uses `(1, 1)` or `(1, 2)`.

### 4.3 `0x22A4C8(psmSel, clearColorRec, fieldParity)` [ok]

`sceGsSetDefDrawEnv` + `sceGsSetDefClear` + `sceGsSetHalfOffset` +
`sceGsPutDrawEnv` on the struct at 0x27F4A0 — **clears an offscreen
render target** whose FBP is computed from `screen_w*screen_h` (×1 for
32-bit, ×3 for the wider variant). Used by 0x21D0A0 (twice per frame),
0x2267E8, 0x226D00, 0x22BF58, 0x22BFD0, 0x22EFF0.

### 4.4 `0x22A198(sel)` [ok]

Struct 0x27F350. `0x262308` + a `TEX0`/`TEX1` block and
`FRAME_1 = 0x70000` (`FBW`), used to make a render target readable as a
texture — the framebuffer→texture step behind the zoom-blur.

### 4.5 `0x22A290(sel)` — a second drawenv on 0x27F3F0, same shape.

---

## 5. Textures — the TEXC machinery

### 5.1 The GS VRAM allocator [ok]

A single word-granular bump allocator at **`gp−30384` (0x2A79C0)**.

```
0x229698()        : cursor = screen_w * screen_h * 5      ; skip the frame/Z buffers
0x2297D0(nblocks) : base = cursor; cursor += nblocks<<6; return base>>6
```

`0x2297D0` returns the base as a **GS TBP** (block = 64 words = 256 B).
Its only caller is `do_load_font` (0x21DBA0), which allocates 5 blocks for
the font CLUT and 2560 blocks for the font page.

### 5.2 The TEXC descriptor table [ok]

12-byte records at **0x27F1C0**, and a parallel array of allocated word
offsets at **0x27F280**:

| offset | field |
|---|---|
| +0x00 | pointer to the resource blob (filled at init from `getResourcePtr`) |
| +0x04 | `log2(width)` |
| +0x08 | `log2(height)` |

Accessors (aap's IDB names, all confirmed):

* `getTexC(i)` = **0x2297A0** → `0x27F1C0 + i*12`
* `getTexCOffset(i)` = **0x2297B8** → `*(u32*)(0x27F280 + i*4)` (words)
* `setTexCOffset_8b(i)` = **0x229750** →
  `0x27F280[i] = cursor; cursor += (1<<wexp) << hexp`
  — i.e. `width * height` **words**, so the textures are **PSMCT32**, not
  8-bit; the `_8b` in the name does not describe the pixel format.

`TEXTURES_229698` (**0x229698**) is a *function*, not a table: it sets the
VRAM cursor and fills slots 0…9 with `getResourcePtr(45+i)`.

### 5.3 Slot → resource map [ok]

Resource-table indices from 0x26ED00; slot = index − 45.

| slot | resource | size | role (from usage) |
|---|---|---|---|
| 0 | TEXCFLOW | 64×64 | used by 0x22E428, blend 0, ZTST 2 |
| 1 | TEXCKABE | 128×128 | the backdrop "wall" mesh (0x2292D0), additive |
| 2 | TEXCBUMP | 64×64 | 3D object surface (0x22D2E8, 0x22D920, 0x22E428) |
| 3 | TEXCBINV | 64×64 | inverted variant of the above |
| 4 | TEXCSMOK | 64×64 | (allocated, no bind site found — **gap**) |
| 5 | TEXCREFA | 64×64 | reflection pass, 0x22D798 |
| 6 | TEXCNAVI | 64×64 | **the menu navigation icons** (0x22EFF0), additive |
| 7 | TEXCBLUR | 64×64 | the glow/halo behind them (0x22EFF0), additive |
| 8 | TEXCSTSL | 64×64 | START / SELECT button glyphs |
| 9 | TEXCMARU | 64×64 | ○ × △ □ button glyphs, 2×2 in the page |

Slot 10 is declared 256×32 in the descriptor table but is not loaded by
`0x229698` — **gap**. TEXCKLGN / TEXCKLGP / TEXCKLFN / TEXCKLFP
(resources 41–44) are not in this table at all; they belong to the clock
face and are loaded elsewhere. [tnt]

### 5.4 Upload: `0x22A9B8(slot)` [ok]

Called ten times in a row from the module init chain (§7.1).

```
d = getTexC(slot)
setTexCOffset_8b(slot)              ; reserve VRAM
0x22A6D8(slot, 0x800000)            ; decode/expand the blob into the 8 MB work buffer
sceGsSetDefLoadImage(&li, getTexCOffset(slot)>>6, (1<<wexp)>>6, 0 /*PSMCT32*/,
                     0, 0, 1<<wexp, 1<<hexp)
FlushCache(0)                       ; 0x24DCE0
sceGsSyncPath(0,0)
sceGsExecLoadImage(&li, 0x800000)
```

So uploads happen **once, at module init**, straight from EE RAM through
`sceGsExecLoadImage` — not through `setTextureUpload` (0x230708) and not
through any custom UNPACK chain. (0x230708 belongs to the 0x230000 core
and is used by the *3D* path.)

### 5.5 Bind: `0x22AB90(slot, additive, ztst)` → `0x22AA88` [ok]

Builds the texture environment at **0x27F580**: `0x262308` (TEXFLUSH,
TEX1_1, TEX0_1 with `TBP = off>>6`, `TBW = width>>6`, `TW = wexp`,
`TH = hexp`, `TCC = 1`, CLAMP_1 = CLAMP/CLAMP), then overrides
`TEST_1 = (ztst<<17) | 0x10000` and `ALPHA_1 = additive ? 0x48 : 0x44`,
then `sceGsSyncPath` + `sceGsPutDrawEnv`.

Bind sites (slot, additive, ztst): (8,0,1) and (9,0,1) in `DrawIcon`;
(1,1,2) backdrop; (2,1,2)/(3,1,2)/(0,0,2) 3D objects; (5,0,1) reflection;
(6,1,1) and (7,1,1) navigation icons.

---

## 6. Text

### 6.1 The shared text engine, as a black-box API

The engine itself is 0x208130–0x20B600 and is **out of scope** for this
document. Its state lives at 0x2DDC00+ and 0x271500+. These are the entry
points Module U uses, with argument meanings read off the call sites and
the (tiny) setter bodies:

| entry | signature | effect |
|---|---|---|
| **0x208110** | `(r, g, b, a)` | set the pen colour; stores four words at 0x2DDC20 |
| **0x208130** | `(paletteIdx, a)` | look up an RGB triple in the colour table at **0x2717D0** (stride 12) and call 0x208110. 51 callers |
| **0x207E98** | `(x, y)` | set the pen position; stores `x<<4` at 0x271858, `y<<4` at 0x27185C |
| **0x207F68** | `(float scale)` | set the text scale; derives the glyph advance into 0x2DDC4C/0x2DDC50 and the tracking into 0x27186C/0x271870 |
| **0x2080D0** | `(float baseScale)` | set the base scale (0x2DDC40) and re-derive via 0x207F68 |
| **0x209640** | `(const char *s)` | draw `s` at the current pen position/colour/scale. Handles the 0x07 / 0x09 / 0x0A escapes |
| **0x209998** | `(const char *s) → int` | **measure** `s`, in pixels, honouring the same escapes. 89 callers — the widest-used entry in the program |
| **0x209DA0** | `(y, const char *s)` | draw right-aligned against the margin at 0x271874 + 0x271878 (Module V only) |
| **0x209FD0** | `(fontm, workBuf, srcBuf)` | decode the FONTM resource |
| **0x20A3B8** | `() → 5` | VRAM blocks needed for the font CLUT |
| **0x20A3C0** | `() → 2560` | VRAM blocks needed for the font page |
| **0x20A3C8** | `(clutTbp, texTbp)` | tell the engine where its font lives in VRAM |

**Width measurement and centring are done by the caller**, never by the
engine: every centred or right-aligned string in Module U goes through
`0x209998` first (§6.2).

`do_load_font` (**0x21DBA0**, HDDOSD `do_load_font`) wires it up once at
module init:

```
clut = gsVramAlloc(0x20A3B8());     ; 0x2297D0
tex  = gsVramAlloc(0x20A3C0());
0x209FD0(getResourcePtr(0) /*FONTM*/, 0x358E00, 0x800000);
0x20A3C8(clut, tex);
0x2080D0(IsPAL() ? 1.15 : 1.0);     ; gp-32212
```

### 6.2 Module U's three text drawers  [ok]

All five-argument, EABI (`a0..a3, t0`):

```
0x21DC28(int x,   int y, const u32 col[4], int alpha, const char *s)
    0x208110(col[0], col[1], col[2], alpha);
    0x207E98(x, y);
    0x209640(s);                                   ; LEFT-aligned

0x21DC88(int xc,  int y, const u32 col[4], int alpha, const char *s)
    if (alpha < 16) return;                        ; skip near-invisible text
    0x21DC28(xc − 0x209998(s)/2, y, col, alpha, s) ; CENTRED

0x21DD28(int xr,  int y, const u32 col[4], int alpha, const char *s)
    if (alpha < 16) return;
    0x21DC28(xr − 0x209998(s),   y, col, alpha, s) ; RIGHT-aligned
```

HDDOSD calls them `DrawNonSelectableItem` (0x21DC28), `draw_menu_item`
(0x21DC88) and — no name — 0x21DD28. **Those names describe usage, not
mechanism.** The only functional differences are the alignment and the
`alpha < 16` early-out (which is what makes fading text disappear cleanly
rather than dither).

`col` is a pointer to a colour record in the §3.1 layout; only the first
three words are read, the fourth (A) is ignored — the alpha comes from the
separate `alpha` argument, so one colour record can be drawn at any fade
level.

Callers: 0x21DC28 ← 0x21D7F8, 0x21DC88, 0x21DD28, 0x21DFF8, 0x21EF00,
0x221230, 0x221D78, 0x222CB0, 0x224630, 0x227560.
0x21DC88 ← 0x21FAD0, 0x221230, 0x221D78, 0x222C08, 0x224630, 0x227560,
0x228110. 0x21DD28 ← 0x21FAD0, 0x221230.

### 6.3 Static colour records  [ok]

| address | RGB(A) | role |
|---|---|---|
| 0x27B750 | 96,96,96,128 | button-bar labels |
| 0x27B830 | 30,110,156,128 | **selected** list row, variant A (blue) |
| 0x27B840 | 44,44,44,128 | unselected list row (dark grey) |
| 0x27B850 | 96,96,96,128 | selected list row variant B / value column |
| 0x27B860 | 110,110,0,128 | list title (olive) |

The values are small because the OSD text engine works in 0…128 colour
space (0x80 = full).

---

## 7. Frame composition

### 7.1 `0x21CE58` is module **init**, not a per-frame chain  [ok]

`osdsys-map.md` §3.2 lists `0x21CE58()` as the "sub-renderer chain" called
every frame. It is called **once**, from `0x21CA38` before the frame loop
is entered, and it is the module's resource/one-shot setup:

```
0x22EE88()            ; → sendDma(0x278860) — the initial VU1/renderer upload
0x22BE18()            ; clear the pad state words
0x22B838()            ; clock: read RTC, timezone, DST; ms/frame = 1000/refresh
0x22B128()            ; → 0x22B138: load the config word into the UI model @0x352880
0x22AD38()            ; frame-count constants: gp-30380 = 40*rate/60, gp-30376 = 80*rate/60
0x229698()            ; VRAM cursor = w*h*5; TEXC slots 0..9 ← resources 45..54
0x22A9B8(0) .. 0x22A9B8(9)   ; decode + upload the ten TEXC textures
0x21DBA0()            ; do_load_font
0x225998(); 0x228460(); 0x2287B0()   ; scene / timer / button-bar init
0x22ADD8(2)
*(float*)(gp-28880) = -100.0         ; camera intro offset
```

### 7.2 The real frame loop — `0x21CA38`  [ok]

```
0x21CE58();  StartFrame();                       ; once
for (;;) {
    0x27B440[+0x08] = *(u32*)0x1F0C44            ; field parity for this frame
    ... screen-id dispatch on 0x1F0010 (see osdsys-map.md §3.2) ...
    0x21CF20();                                  ; ===== the frame body =====
    waitNextFrame();                             ; 0x205F30
    0x27B440[+0x08] = *(u32*)0x1F0C44
    0x205DC0();                                  ; re-apply the display env if the
}                                                ;   video-output setting changed
```

`StartFrame` / `waitNextFrame` / `SwapThread` (0x205CE0–0x206270) own the
buffer flip; Module U never calls `sceGsSwapDBuff` itself — it only selects
which half to draw into, via `0x1F0C40` passed to `0x22A3B8`.

### 7.3 The frame body — `0x21CF20`  [ok]

Two 64-byte stack matrices (`m0` at sp+0, `m1` at sp+64) thread through the
first three stages. Stage order, top to bottom, is also the **draw order**:

| # | call | what it does |
|---|---|---|
| 1 | `0x21CFD8(m0,m1)` | camera: `sceVu0ViewScreenMatrix` (0x267068) then 0x22ED20 (unit/rot X,Y,Z/apply/`sceVu0CameraMatrix`) building the view-screen matrix from 0x27B460…0x27B490. Then decays the intro offset: `*(gp−28880) *= 0.97` |
| 2 | `0x21D0A0(m0,m1)` | **the scene**: sets the full-screen record 0x27B4B0's extents; `0x22A3B8` (drawenv); `0x229358(m0,m1)` — the animated TEXCKABE backdrop mesh; `0x22C3C0(phase)` — zoom-blur; two `0x22A4C8` offscreen clears; `0x22A198`; two `0x22C190(0)` blits; `0x22A290(0)`; `0x22A0C0(0,1)`; and finally `0x2299C0(0x27B4B0)` — **composite the offscreen target to the screen** |
| 3 | `0x2268F0(m0,m1)` | **the 3D object list** (menu icons / orbs) — see §10 |
| 4 | `0x22B020()` | steps the global transition timer 0x27F620, then draws the sweep effect (`0x22AF10` → `0x22A030`) **only when `fadeState == 0`**, and **always** draws the fade curtain `0x22AFB8` (which is invisible at alpha 0). Established while byte-matching it: the ROM's `bnez` target *is* the tail call to 0x22AFB8, and the else path falls into it |
| 5 | `0x2283F0()` | the eleven per-screen renderers (§8) |
| 6 | `0x21D368()` → `0x21D1F8()` | the **16:9 letterbox bars**, when the screen-size setting is 0 or 2 |
| 7 | `0x21D3A0()` | the **clock** (0x20A998/0x20AAA0 formatters → 0x209640) |
| 8 | `0x21DA68()` → `0x21D7F8()` | the **button hint bar** (§9.1) |
| 9 | `0x21DB18()` | the right-edge strip sprite 0x27B7F0 |
| 10 | `0x225BF8()` | advances the 12 carousel entries' animation floats |
| 11 | `0x2285C0()` | the transition-phase ramp: `*(gp−28844) = 10 − count*10/duration`, and the camera smoothing `0x27B440[0] += (target − cur)*0.1` |
| 12 | `0x2287D0()` | `*(gp−30392)++` — the module frame counter |
| 13 | `0x22B058()` | the **global fade state machine** (§8.2) |
| 14 | `0x22BB30()` | sound / RTC service |
| 15 | `0x22B588()` → `0x22B138()` | refresh the UI model from the config word |
| 16 | `0x22BE30()` | pad sampling + edge detection + UP/DOWN auto-repeat |

So the layering is: **backdrop mesh → blur/composite → 3D objects →
transition effect → 2D screens → letterbox → clock → button bar**, with
input read *after* drawing (so a press acts on the next frame).

---

## 8. Animation: the timer object and selection feedback

### 8.1 The timer  [ok]

A 16-byte object, and the single most-used abstraction in the module
(`0x22AC48` alone has 48 call sites):

```c
struct Timer { s32 duration; s32 count; s32 edge; s32 state; };
```

`state`: **0** = closed, **1** = opening, **2** = open, **3** = closing.
`edge` is set for exactly the frame on which a transition completes.

| function | meaning |
|---|---|
| `0x22AC10(t)` | `→ t->duration` |
| `0x22AC18(t)` | `→ t->count` |
| `0x22AC20(t,n)` | **`→ t->count * n / t->duration`** — the interpolator |
| `0x22AC48(t,s)` | `→ t->state == s` |
| `0x22AC58(t)` | `→ t->edge` |
| `0x22AC60(t)` | reset: state = count = edge = 0 |
| `0x22AC70(t)` | **open**: if state 0 → count = 0, state = 1 |
| `0x22AC90(t)` | **close**: if state 2 → count = duration, state = 3 |
| `0x22ACC0(t)` | **step one frame**: edge = 0; state 1 → `if (++count == duration) {edge = 1; state = 2}`; state 3 → `if (--count == 0) {edge = 1; state = 0}` |

Known instances: **0x27F190** (the backdrop-mesh fade, duration =
`gp−30380` = 40 frames @60 Hz), **0x27F620** (the global screen
transition, duration set to 80 frames-equivalent), **0x27BF50** (the
list-panel open/close), **0x27EB00** (the carousel), **0x27EC40**,
0x27BE44, 0x27BF60/70/8C, 0x27C200/258, 0x27DA70/80, 0x27DEF4, 0x27EC00.

### 8.2 Fades

**Panel alpha** — `0x221000()` [ok] is the archetype and it is a *product
of two* timers:

```
a = 0x22AC20(0x27BF50, 128);                     ; this panel's own open fade
b = 128 − 0x22AC20(*(gp−30632) + 28, 128);       ; the incoming panel's fade
alpha = (a * b + 127) >> 7;                      ; 0..128
```

That alpha is then passed as the `alpha` argument of every
`0x21DC28/88/D28` call on the panel, and the `alpha < 16` early-out in the
centred/right variants makes an outgoing panel stop drawing before it
turns muddy. **This is the whole cross-fade mechanism**: there is no
per-element opacity field; opacity is a call argument.

**Global screen fade** — `0x22B058()` [ok] (byte-matched) runs a 0…128 counter in
`gp−28824` under the state word `gp−28828` (1/2 = fade in, 3 = fade out),
and `0x22AFB8()` paints the black curtain 0x27F630 at
`A = 128 − counter`. When the counter reaches `128 − *(gp−30376)` the
state machine kicks the transition timer 0x27F620.

**Transition phase** — `*(gp−28844)` (readable via `0x226948()`) ramps
0…10 from timer 0x27EC40 and drives two zoom-blur passes:
`0x2283D0()` calls `0x22C3C0(phase − 5)` for phase ≥ 5, and `0x21D0A0`
calls `0x22C3C0(phase < 6 ? phase : 10 − phase)`.

### 8.3 The selection highlight  [ok]

Module U does **not** zoom or scale the selected item in the 2D layer.
Selection is expressed by (a) **swapping the colour record** and (b) the
3D carousel rotating the chosen object to the front (§10). In the list
renderer 0x221230, per row:

```
if (row == view->selected)  col = (aux[row].flag ? 0x27B830 : 0x27B850);
else                        col = 0x27B840;
```

0x27B830 is the blue highlight, 0x27B840 the dark unselected grey.

---

## 9. Screen-space UI structures

### 9.1 The button hint bar  [ok]

Bottom-of-screen row of "glyph + label" pairs (the ○ Enter / △ Back hints).

`0x21DA68()` — chooses which of nine hint *sets* is visible:

```
y = 0x21D9E0();                     ; 182 if uiModel[0]==2 (16:9) else 200,
                                    ; × 0.5405/0.47 when PAL
if (*(gp−30768))            0x21D7F8(8, *(gp−30772), y);   ; the caller-supplied set
else if (0x226A48())        0x21D7F8(7, 128, y);
else for (i = 0; i < 7; i++) { a = 0x228660(i); if (a > 0) 0x21D7F8(i, a, y); }
```

`0x228660(i)` dispatches through the 6-entry jump table at **0x2A4B50** to
one per-screen alpha function (0x227E18, 0x2271B8, 0x221060(1),
0x221060(0), 0x21F980, 0x226FD0) and clamps to 0 above 127 — so **several
hint sets cross-fade simultaneously** during a screen change.

`0x21D7F8(set, alpha, y)` — the drawer:

```
lang = GetLanguage();                                   ; 0x2040D0
0x22A3B8(0x1F0A10, *(0x1F0C40), NULL, *(0x27B448));
0x207F68(0.8);                                          ; gp-32216
ids  = (set == 8) ? 0x27B5D0
                  : 0x27B5E8 + set*20 + (0x204318() ? 180 : 0);
xs   = 0x27B760 + lang*16;                              ; 4 ints, per language
for (i = 0; i < 4; i++) {
    if (ids[i] == 1) continue;                          ; 1 = "no button here"
    if (i < 3) {
        DrawIcon(0x27B7E0[i], xs[i], y, alpha);
        0x21DC28(xs[i] + 28, y, 0x27B750, alpha, osdGetString(ids[i]));
    } else {                                            ; slot 3 is right-anchored
        w = 0x209998(osdGetString(ids[3])) + 24;
        DrawIcon(0x27B7E0[3], screen_w − w − 28, y, alpha);
        0x21DC28(screen_w − w, y, 0x27B750, alpha, osdGetString(ids[3]));
    }
}
0x207F68(1.0);
```

Tables:

* **0x27B5E8**, 20-byte records, **two blocks of nine** (0…8 and, 180
  bytes later, 9…17) selected by the region/version check `0x204318`.
  Fields 0…3 are string IDs (1 = empty; 85 "Back", 86 "Enter", 87
  "Options", 94, 95); field 4 is a pad-button mask (0x5000 = △|×). Set 8
  in each block is all-zero.
* **0x27B5D0** — the caller-supplied set, written by `0x21D768(a,b,c,d)`,
  which also performs the **Back/Enter (○/×) region swap**: when
  `0x204318()` is non-zero, id 85 and id 86 are exchanged between slots 1
  and 2. `0x21D758(x)` writes its fifth word, `0x21D748(f)` arms it
  (`gp−30768`) and `0x21D750(a)` sets its alpha (`gp−30772`, default 128).
* **0x27B760** — 8 languages × 4 slot X positions, e.g. EN =
  {24, 213, 335, 441}, JP = {20, 226, 332, 508}.
* **0x27B7E0** — the four glyph indices, `{2, 4, 5, 3}`.

`DrawIcon` = **0x21D590** [ok]:

```
DrawIcon(int glyph, int x, int y, int alpha)
    if (0 <= glyph < 2) { w = h = 28; bindTexture(8 /*TEXCSTSL*/, 0, 1); }
    else                { w = h = 25; bindTexture(9 /*TEXCMARU*/, 0, 1); }
    rec = 0x27B530;
    rec.x0 = x<<4;  rec.x1 = (x+w)<<4;
    rec.y0 = y<<4;  rec.y1 = (y + h/2)<<4;        ; half height: field rendering
    rec.A  = alpha;
    uv = 0x27B570 + glyph*16;                     ; {u0,v0,u1,v1} in texels
    rec.u0 = uv[0]*16+8; rec.v0 = uv[1]*16+8;
    rec.u1 = uv[2]*16+8; rec.v1 = uv[3]*16+8;     ; +8 = half-texel centring
    if (IsPAL()) rec.y1 = y0 + (y1−y0) * (0.5405/0.47);
    0x2299C0(rec);
```

**0x27B570** holds six glyph rects: `{0,0,32,32}` and `{32,0,64,32}` in
TEXCSTSL (START, SELECT), then `{0,0,32,32}`, `{32,0,64,32}`,
`{0,32,32,64}`, `{32,32,64,64}` — the four shape buttons in a 2×2 grid of
TEXCMARU.

### 9.2 The list view  [ok]

The two-column scrolling list (Version Information and friends) is
rendered by **0x221230** from a view struct at **0x27BF38**:

| offset | field |
|---|---|
| +0x00 | `const char *title` |
| +0x04 | `struct { const char *label; const char *value; } *rows` |
| +0x08 | row count |
| +0x0C | visible rows |
| +0x10 | selected row |
| +0x14 | top row (scroll) |

Layout, with `0x207F68(0.8)` and the panel alpha from `0x221000()`:

```
title   : 0x21DC88(442, y0, 0x27B860, alpha, view->title)      ; centred
per row : 0x21DD28(429, y,  col,      alpha, rows[i].label)    ; right-aligned
          0x21DC28(455, y,  col,      alpha, rows[i].value)    ; left-aligned
y += IsPAL() ? 13 : 11
```

so the list is a right-aligned label column ending at x = 429 and a
left-aligned value column starting at x = 455, centred as a unit on
x = 442. Rows `top−2 … top+visible+3` are emitted (two above and four
below the window, for the scroll animation). The auxiliary flag that
selects highlight variant A vs B is read from a 12-byte-stride table at
**0x1F1240**.

### 9.3 The letterbox  [ok]

`0x21D368()` gates on `uiModel[0]` (screen type) being 0 (4:3) or 2
(16:9) and calls `0x21D1F8()`, which draws the record at 0x27B4F0 twice:

```
content = screen_w * (9/16) * pixelAspect;      ; 640*0.5625*0.5405 = 194.6
bar     = (screen_h − content) / 2;             ; 14.7 px on NTSC
top bar : x0=0, y0=0,              x1=w*16, y1=bar*16
bottom  : x0=0, y0=(h−bar)*16,     x1=w*16, y1=h*16
```

`0.0625 * 9.0` in the code is `9/16`; `0x27B440[+0x10]` is the pixel
aspect set by `0x21C9D0`.

---

## 10. The 3D object layer — where this document stops

Stage 3 of the frame body, `0x2268F0(m0,m1)`, is the 3D scene and belongs
to the sibling investigation. For the record, its shape is:

```
0x225CF0()            ; reset the object list head 0x34E980
if (gp-32148 (0.05) < *(float*)0x34E6D0)  0x226028();   ; rebuild the 12-slot ring
0x2261B8()            ; place 7 objects using the 0x230000 matrix stack
                      ;   (0x230180 push, 0x230198, 0x230260, 0x230328, 0x230440)
0x226700()            ; walk the list: node->type==1 → 0x226360 → 0x22EFF0
                      ;                        else → 0x2266E0 → 0x22D920
0x2267E8()            ; clear + second pass: 0x22E428(node+16, 0, node+288)
```

* The **12-entry ring** at 0x34E6C0, stride 48, with `*(int*)0x34E6C0` as
  the rotation offset (`slot = (i + offset) % 12`) and per-entry floats at
  +0x10 (a 0…1 progress driven from timer 0x27EB00 by `0x225BF8`) and
  +0x14 — this is the **main-menu icon carousel**. [tnt]
* The **object list** at 0x34E980 is singly linked through +0x00, with a
  type at +0xF0, a float at +0xF4, a 4×4 matrix at +0x10 and more data at
  +0x120/+0x288.
* `0x22EFF0` (0xE94 bytes, the largest function in the module) is the
  per-icon renderer: it binds TEXCNAVI (slot 6) and TEXCBLUR (slot 7)
  additively and emits four sprites plus its own hand-built packets. So
  **the menu icons themselves are textured quads placed by a 3D matrix**,
  not by the 2D record path.

---

## 11. Verdict on `docs/menu-rendering-reverse.md`

The overriding problem with that file is **scope**: it is titled "OSDSYS
menu rendering" but almost everything it describes is **Module V (the
browser)**, which is a different module with a different packet library and
a different texture set. Applied to Module U, most of it is simply about
the wrong code. Within Module V its mechanism descriptions hold up well
(`osdsys-map.md` §2.5–2.7 already re-verified those).

| claim | verdict |
|---|---|
| "The menu draws by building VIF1 packets directly with `sceVif1Pk*` … plus a thin custom layer" | **WRONG for Module U.** Module U calls no `sceVif1Pk*` function at all; it hand-writes DMAtag + VIF1 DIRECT + GIFtag in the scratchpad (0x2293E0/0x2294B8). Right for Module V |
| "packet buffers live in the SCRATCHPAD (0x70000000), double-buffered" | **RIGHT**, and independently true of Module U (8 KB halves, index in `gp−30388`) |
| "a custom kick helper sets the IRQ bit in the head tag before send" | **RIGHT for Module V.** Module U's kick instead sets **TTE** (`chcr |= 0x40`) and passes `base & 0x3FF0 | 0x80000000` (the SPR source flag), not an IRQ bit |
| "geometry is 2D quads/sheets transformed by the same 0x267xxx matrix helpers" | **HALF RIGHT.** Module U's 2D quads are *not* transformed at all — the records are already in screen space (§3). The 0x267xxx helpers are used only by the camera (0x21CFD8/0x22ED20) and the 3D list |
| "textures/fonts come from files via an RPC + resource loader (`loadImage_Resource` 0x225038) and are uploaded with LoadImage packets or custom UNPACK chains (0x230000)" | **MOSTLY WRONG for Module U.** TEXC textures come from the *in-image* resource table, are expanded into a work buffer at 0x800000 and uploaded with **`sceGsExecLoadImage`** at module init (§5.4). No RPC, no UNPACK chain, no `sceVif1PkRefLoadImage` |
| `gp = 0x282E1C` and the whole gp table | **WRONG** — `gp = 0x2AF070` (already corrected in `osdsys-map.md` §0/§5.1) |
| 0x295AD0 / 0x295CD0 / 0x295D00 / 0x295D10 / 0x296160 tables | **RIGHT but Module V only** |
| "0x284100: static vertex table used by sub_245038's 2nd transform" | **RIGHT, and Module V only** — see §0.1 |
| `sub_2403C8` open / `sub_240438` kick descriptions | **RIGHT**, Module V |
| `sub_240068` browser state machine, `sub_240178` "another state machine" | **RIGHT** in mechanism (the latter is the reset watchdog — already corrected in the map) |
| `sub_245038` element renderer, 352-byte descriptors at 0x295D10 | **RIGHT**, Module V. Module U's analogue is `0x2299C0` with a **0x40-byte** record and a completely different field layout |
| `sub_246D28` / `sub_246DE8` texture upload, 28-byte descriptors at 0x296160 | **RIGHT**, Module V. Module U's are **12-byte** descriptors at 0x27F1C0 |
| "0x24DCE0 = file RPC open?" | **WRONG** — `FlushCache` (already corrected in the map) |
| "sub_224630 (0x734) = the big screen renderer" | **WRONG role** — it is the first-boot setup wizard (already corrected in the map). Its *draw* usage is right: it does call 0x21DC28/0x21DC88 and `0x2299C0` |
| "the 0x2299C0–0x22B0E8 widget cluster (per-item draws) — still to pin down" | **NOW ANSWERED.** It is not a widget cluster: 0x2299C0 is the single sprite primitive, 0x229130/0x2292D0 the backdrop mesh, 0x22A0C0/0x22A198/0x22A290/0x22A3B8/0x22A4C8 the GS state helpers, 0x22A9B8/0x22AA88/0x22AB90 texture upload/bind, 0x22AC10–0x22ACC0 the timer object, 0x22AD38–0x22B0E8 transition state |
| "0x209640 / 0x209998 as the main text entry points" | **RIGHT** — see §6.1 |
| "The flare/effect code … candidates for the config menu's background effects" | **WRONG** — 0x21A300–0x21B4C8 is inside the *opening* module (0x211C70–0x21C910). Module U's effects are `0x22AF10`/`0x22A030` (sweep), `0x22C3C0` (zoom blur) and `0x22AFB8` (fade curtain) |

Bottom line: the file is a decent Module V write-up with a misleading
title. It should be renamed `browser-rendering-reverse.md`, its gp table
regenerated, and its "menu" claims about Module U dropped in favour of this
document.

---

## 12. Binary matching

`matching/src/menudraw.c` + `matching/menudraw-functions.txt` reconstruct
the leaf functions of this layer. **38 of 43 attempted functions match
byte-exact** against the retail image with ee-gcc 2.9-ee-991111 -O2; the
five misses are instruction-scheduling / basic-block-order ties (same
instructions, different slots), annotated at their definitions.

Matched: the whole timer object (0x22AC10–0x22ACC0), the three text
drawers (0x21DC28/88/D28), `drawSprite` (0x2299C0), the TEXC accessors
and the VRAM allocator (0x229750/0x2297A0/0x2297B8/0x2297D0),
`transStep` (0x22B020), `drawLetterbox` (0x21D368), and eighteen small
accessors and wrappers.

Two reusable codegen findings came out of it and are written up in the
file's header:

* **Store order.** gcc 2.9 -O2 rotates a run of independent stores to
  the same struct rather than emitting them in source order — `(s3, s1,
  s2)` for a straight-line run ending in the epilogue, `(s1, s3, s2)`
  for a run inside a branch. Reading the order off the ROM and writing
  it into the source gets it wrong every time; brute-forcing the
  permutations is the only reliable route.
* **The low block must be one symbol.** The ROM reaches `evenOddFrame`,
  `screen_width` and `screen_height` through a single `lui r,0x1f` with
  three displacements. Separate `extern int x[]` declarations emit a
  `%hi` each; a literal pointer cast folds when read once but
  materialises `lui+ori` when read twice. Only one
  `extern int lowBlock[];` indexed by constants reproduces the ROM.

---

## 13. Open questions / labelled gaps

* **`0x22A6D8(slot, 0x800000)`** (0x2E0 bytes) — the TEXC blob decoder. Not
  disassembled here; presumably the same TIM2/RLE format the opening uses.
* **TEXC slot 4 (TEXCSMOK)** is uploaded but I found no `0x22AB90` bind
  site for it, and **slot 10** (256×32) has a descriptor but no resource.
  Both may be bound through a computed slot index I did not trace.
* **TEXCKLGN / TEXCKLGP / TEXCKLFN / TEXCKLFP** (resources 41–44) — the
  clock-face textures. Not in the 0x27F1C0 table; loader not located.
* **`0x21DB18`** draws a 2 px-wide, full-height sprite at the right screen
  edge from record 0x27B7F0 whose colour is black/α128 and whose UVs are
  never set. Role unclear (scrollbar? seam cover?).
* **`0x22A3B8`'s FRAME patch** writes FBP = 8 or 14 into the selected
  drawenv. Those two pages are only 48 KB apart, far less than a
  640×224×32 buffer, so either the OSD's buffers are much smaller than the
  declared draw size or I have mis-attributed the qword. Unresolved.
* The **0x27F2F0 / 0x27F350 / 0x27F3F0 / 0x27F4A0 / 0x27F580 drawenv
  structs' GIFtags** are never written by the code I read; they must be
  initialised by an earlier `sceGsSetDef*` call on the same memory.
  Not confirmed.
* **`0x22ED20`** (the camera-matrix builder) and the `0x230000` matrix
  stack (0x230180/0x230198/0x230260/0x230328/0x230440) are described only
  by their `sceVu0*` callees; the 3D sibling should own these.
* Where the **item strings** for the config screens come from: the
  computed-ID screens (0x21D7F8's callers aside, 0x227560, 0x228110,
  0x221D78, 0x222CB0) resolve their IDs from tables I did not chase —
  that is the screen-logic sibling's territory.
