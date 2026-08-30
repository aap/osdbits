# Module U's 2D text layer, decoded and ported (2026-08-30)

Method and confidence convention as `docs/menu-draw.md`: **[ok]** =
read out of `ee-objdump -m mips:5900` of
`/u/aap/src/osdsys/expanded.bin` (VA = offset + 0x200000, gp = 0x2AF070)
and cross-checked; **[tnt]** = partial; **[?]** = guess.  Everything
below is [ok] unless marked.

Everything here is implemented in `osdbits/menutext.c` (mode `menu`),
which draws the main menu's two item labels over `menu.c`'s orb scene.

---

## 1. The headline: there is no font decoder to port

`docs/menu-draw.md` §6.1 describes `do_load_font` (0x21DBA0) as

```
0x209FD0(getResourcePtr(0) /*FONTM*/, 0x358E00, 0x800000);
0x20A3C8(clut, tex);
```

and that reads as "FONTM is decoded into VRAM".  **It is not.**  Reading
both functions:

* **`0x209FD0` decodes nothing.**  It stores the three pointers
  (`0x2DDC0C` = FONTM, `0x2DDC10` = workbuf | 0x30000000, `0x2DDC14` =
  srcbuf), grabs DMA channel 1 (`0x266F60`), clears the 72-slot glyph
  cache (`0x208C28`), and sets the engine's defaults - pen (0,0),
  per-glyph gap `0x271860 = -3` (`0x207F38(-3)`), baseline bias
  `0x271864 = -7 << 3 = -56` (`0x207F48(-7)`), colour (128,128,128,128),
  scale 1.0.  Its tail computes `0x2DDC18 = FONTM + FONTM[3]` and
  `0x2DDC1C = that + 8` - the kanji index, used lazily.
* **`0x20A3C8` is where the visible font comes from.**  It calls
  `0x20A280(slot, 0x2715E0, tbw, psm, getResourcePtr(2+slot), w, h)`
  four times:

  | slot | resource | size | TBW | TW | TH | blocks |
  |---|---|---|---|---|---|---|
  | 0 | 2 `FNTASCII` | 256x480 | 4 | 8 | 9 | 256 |
  | 1 | 3 `FNTEX000` | 512x760 | 8 | 9 | 10 | 1024 |
  | 2 | 4 `FNTEX001` | 512x760 | 8 | 9 | 10 | 1024 |
  | 3 | 5 `FNTEXOSD` | 512x80 | 8 | 9 | 7 | 128 |

  (the per-slot record is 24 bytes at **0x271578**: `+0` tbp, `+4`
  blocks, `+8` tbw, `+12` tw, `+16` th, `+20` clut tbp; the first four
  fields are static .data, the tbp/clut are filled here.  2560 blocks
  total - exactly what `0x20A3C0()` returns.)

  `0x20A280` memcpys the 64-byte CLUT at **0x2715E0** into the work
  buffer and uploads it, then memcpys `w*h/2` bytes of the resource and
  uploads that.  `w*h/2` is the giveaway: these are **plain 4-bit
  indexed bitmaps, uncompressed once the ROMDIR's LZ is undone.**
  Verified numerically: FNTASCII expands to exactly 61440 = 256*480/2,
  FNTEX000/001 to 194560 = 512*760/2, FNTEXOSD to 20480 = 512*80/2.

So the Latin glyphs are a texture.  FONTM is only consulted on the
two-byte (Shift-JIS) path, through the 72-slot LRU glyph cache at
0x271880 (zeroed by `0x208C28`, allocated by `0x208C60`), which
`0x208D40` fills by unpacking **26 rows of 13 bytes** per glyph out of
FONTM into 16-byte-strided rows of the work buffer and DMAing that to
the cache's VRAM slot.  A Latin-locale main menu never touches any of
it.

### 1.1 The ASCII page layout

97 glyphs, characters 32..128, in a grid of **8 columns x 32 px** by
**12 rows x 40 px**.  `0x2086A0`'s `div s0, 8` gives `(row, col)` and
then `u = col << 5`, `v = row * 40`.

Per-glyph metrics are static .data - **97 x `{u32 leftInset; u32
width}` at 0x26FE60** (kind 0).  `leftInset` is where the glyph starts
inside its 32-px cell, `width` is its advance.  Spot check: `'1'`
{5,23}, `'A'` {4,25}, `'W'` {0,32}, `'l'` {12,9}, `' '` {0,13}.
Rendering the page from the CLUT confirms the ink sits exactly inside
`[leftInset, leftInset+width)`.

The two-byte tables sit right after it: **0x270160** (304 entries) and
**0x270AE0** (304 more, for codes >= 304) for kind 1, **0x271460** (35)
for kind 2 - same `{inset, width}` shape.

### 1.2 The CLUT

**0x2715E0**, 16 x RGBA32, a grey ramp whose *alpha* carries the
antialiasing:

```
000000/00 000000/19 000000/32 000000/4B 000000/64
333333/32 333333/4B 333333/64 333333/7D
666666/4B 666666/64 666666/7D
999999/64 999999/7D  CCCCCC/7D  FFFFFF/7D
```

Index 15 is white at alpha 0x7D (not 0x80 - the ROM's text is never
quite opaque).  TEX0 is TCC=1 / TFX=MODULATE, so the pen colour
multiplies both the RGB and that alpha.

---

## 2. The glyph emitter `0x2086A0(kind, &bound, code)`

Returns the advance in pixels; the caller adds `<< 4` to the pen.
Kind 0 = ASCII (`code -= 32`, bail if `(u32)code >= 97`); kind 1 =
two-byte, `< 304` picks table 0x270160 else 0x270AE0 with `code -= 304`;
kind 2 = table 0x271460.  `bound` is a caller-owned flag so the
texture/CLUT bind (`0x208460(kind)`) happens once per string, not per
glyph.

With `scale` = `*(0x2DDC44)`, `glyphH16` = `*(0x2DDC50)`,
`gap` = `*(0x271860)` = -3, `yBias` = `*(0x271864)` = -56,
`advMul` = `*(0x27156C)` = 1 (static .data), and the pen at
`*(0x271858)` / `*(0x27185C)` (both already << 4):

```
col = g % 8;  row = g / 8
xoff16 = ((4096 - screen_width )/2) << 4
yoff16 = ((4096 - screen_height)/2) << 4
x0 = penX + xoff16                       [+ the slant term, 0x271560 != 0]
y0 = penY + yoff16 + (int)(scale*yBias)  [+ *(0x271568), + *(0x271870)]
x1 = x0 + ((int)(scale * width*advMul) << 4)
y1 = y0 + glyphH16
u0 = col*32 + leftInset ; u1 = u0 + width
UV0 = (u0*16 + 8, row*40*16 + 8)
UV1 = (u1*16 - 8, row*40*16 + 632)
advance = (int)(scale*width*advMul) + (int)(scale*gap)
```

The `xoff16`/`yoff16` terms cancel `XYOFFSET_1 = (2048-w/2, 2048-h/2)`
exactly, so the pen is plain top-left screen pixels - the same
convention `0x2298A8` uses for the 2D sprite records
(`docs/menu-draw.md` §3.2).

`632 = 39.5*16`, i.e. the sprite samples 39 of the cell's 40 rows with a
half-texel inset at each end; horizontally it samples `width - 1`
texels.  Because `glyphH16` is 320 (20 px) at scale 1.0 and the cell is
40 rows, **the glyph is drawn at half vertical scale** - the OSD renders
one interlaced field, so one output line covers two source rows.  Same
reason `DrawIcon` halves its height (`docs/menu-draw.md` §9.1).

The packet is one PACKED GIFtag from **0x2A3C20** (or **0x2A3C30**,
picked by `*(0x271868)`, which `0x209FD0` arms to 1) - `PRE=1`,
`PRIM = SPRITE|TME|FST` plus `ABE` in the 0x2A3C30 variant, `NREG=5`,
`REGS = {RGBAQ, UV, XYZ2, UV, XYZ2}` - so exactly five data qwords per
glyph, `Z = 1`.

### 2.1 `0x207F68(scale)` - the size derivation

The truncations are load-bearing:

```
*(0x2DDC44) = *(0x2DDC48) = scale
h  = (int)(scale*32.0 * 0.5) << 4
*(0x2DDC50) = (int)(h * 1.25 * baseScale)          ; the drawn height, <<4
w  = (int)(scale*32.0 * 0.909091 * 0.5) << 4
*(0x2DDC4C) = (int)(w * baseScale)
if (*(0x271564)) { ...the escape-driven percentage size... }
else *(0x27186C) = *(0x271870) = 0
```

`baseScale` = `*(0x2DDC40)`, set by `0x2080D0`, which `do_load_font`
calls with **1.15 on PAL and 1.0 on NTSC**.  At scale 1.0 that gives
`0x2DDC50` = **320** (20 px) NTSC and **368** (23 px) PAL, and
`0x2DDC4C` = 224 / 257.  Note `0x2DDC4C` is *not* used for the sprite -
only `0x209970` reads it, returning `max(0x2DDC50, 0x2DDC4C) >> 4` as
the line height.  **`baseScale` only stretches text vertically**; the
horizontal size and the advance use `scale` alone, so PAL text is the
same width as NTSC text.

### 2.2 `0x209640` / `0x209998` - draw and measure

Same loop twice.  Byte classification, in the ROM's own spelling:

```
c == 7 || c == 9 || c == 10                 -> escape (0x209300)
(u8)(c + 127) < 31                          -> SJIS lead 0x81..0x9F
(u8)(c + 32)  < 16                          -> SJIS lead 0xE0..0xEF
otherwise                                   -> one ASCII glyph
```

`0x209640`'s prologue is **`0x2083D0`, which force-pushes a drawenv**
that the caller cannot override: `TEST_1 = 0x3000D` (ATE 1, ATST
GEQUAL, AREF 0, **ZTE 1, ZTST ALWAYS**) and `ALPHA_1 = 0x44` with
`FIX = 0x80` - the normal `(Cs-Cd)*As + Cd` blend.  So text is never
depth-tested, even though `0x228110` asks for `0x22A0C0(1, 2)` (ZTST
GEQUAL) immediately before.  Its epilogue is `sceGsSyncPath(0,0)`.

Measuring accumulates `0x208540(c) + (int)(scale*gap)` per glyph, where
`0x208540` is `(int)(scale * width * advMul)` - i.e. identical
arithmetic to the emitter, so a centred string's box is exact.

### 2.3 Escape sequences (`0x209300`)

Introduced by 0x07 / 0x09 / 0x0A, second byte `'a'`..`'y'` indexes the
25-entry jump table at **0x2A3CB0**.  Two seen in the shipped strings:
`\7oNNN` (three digits) and `\7rN.NN` (five characters, sets
`*(0x271564)` = a percentage size and re-runs `0x207F68`).  `\7@c`
copies glyph `c`'s width into `*(0x271560)` (fixed-width mode).
**Not ported** - `menutext.c` skips them by length (3 or 5), which is
right for the main menu's ids 89/90/91 (none contains one) and wrong
for e.g. id 93 `"PlayStation\7o004"`.  [tnt]

---

## 3. `osdGetString` (0x2041B8) and the string table

```
if (id == 86 && 0x204318()) return regionTable[85];   /* Back/Enter swap */
if (id == 85 && 0x204318()) return regionTable[86];
return ((char **)*(u32 *)0x26ECE0)[id];
```

`0x26ECE0` points at the current language's table.  The **English table
is at 0x298B08, 299 entries** (ids 0..298; entry 299 is 0, which is how
the extent was determined).  Confirms `docs/menu-logic.md` §6.1 exactly:
id 90 = `"Browser"` (0x299A50), id 91 = `"System Configuration"`
(0x299A38), id 89 = `"Version Information"`, 85 `"Back"`, 86 `"Enter"`,
87 `"Options"`.

---

## 4. The main menu

### 4.1 `0x228110` - the draw loop

```
centre = screen_height / 2                     ; 112 on NTSC
y      = centre - 14
alpha  = 0x227E18()
if (!timerIsState(0x27BEA8, 2)) return
if (!0x227FC0()) return
0x22A3B8(0x1F0A10, evenOddFrame, NULL, *(0x27B448))
0x22A0C0(1, 2)                                 ; normal blend, ZTST GEQUAL
0x207F68(1.0)
hdr = 0x27BE90
for (i = 0; i < hdr->count; i++) {
    col = (i == hdr->cursor) ? 0x27B830 : 0x27B840
    0x21DC88(430, y, col, alpha, osdGetString(hdr->items[i].strid))
    y += 16
}
```

Static data, read out of the image: header **0x27BE90** =
`{title 1, items 0x27BE80, count 2, rows 3, cursor 0, top 0, mode 0}`,
items **0x27BE80** = `{90, 0x2A7898}, {91, 0x2A7898}`, colours
**0x27B830** = `{30,110,156,128}` (the blue highlight) and **0x27B840**
= `{44,44,44,128}` (unselected grey).  So the two labels are centred on
**x = 430**, first baseline at **y = h/2 - 14**, 16 px apart, and
selection is *only* a colour swap - no zoom, no scale
(`docs/menu-draw.md` §8.3 confirmed at the call site).

### 4.2 `0x228460` sets the durations (correcting a gap in §7.1)

The one-shot init's `0x228460` is not only `sceVu0ViewScreenMatrix`; its
tail is the screen-timing setup:

```
rate = IsPAL() ? 50 : 60
*(gp-30400)          = rate*40/60         ; 40 on NTSC
*(0x27EC00).duration = *(gp-30400)
*(0x27BE40)          = 0
*(0x27EC40).duration = *(gp-30400) + rate/6
*(0x27BE44).duration = *(gp-30380) + *(gp-30400) + rate/6
*(gp-30396)          = rate/6             ; 10 on NTSC
*(0x27BEA8).duration = rate/6             ; the MAIN MENU's Anim
0x2217D8(); anmReset(0x27BEA8)
```

So `gp-30396` (which `docs/menu-draw.md` §8.2 does not name) is
`refreshRate/6` = **10 frames**, and it is *not* `dur80` (gp-30376).
The main menu's own fade is ten frames long.

### 4.3 `0x228050` - when the menu appears

```
if (!0x227FC0()) return;                       /* no other screen open */
if (timerIsState(mainAnim, 0)) {
    if (getFadeMode() == 2 && getFadeAlpha() == 128 - *(gp-30396))
        0x227F08();                            /* timerOpen(mainAnim) */
} else if (timerIsState(mainAnim, 2)) {
    if (getFadeMode() == 3 && getFadeAlpha() == 128)
        0x227F50(1);                           /* timerClose(mainAnim) */
}
```

The module's entry fade (`0x22ADD8(2)`, `docs/menu-scene.md` §10.7a)
runs alpha 0 -> 128 over 128 frames; at **alpha 118** the main menu's
own ten-frame fade starts, so the labels are fully up exactly as the
curtain finishes.  That is the whole "menu appears" timing.

### 4.4 `0x227E18` - the item alpha

```
a = timerScale(0x27BEA8, 128)                  /* the menu's own fade */
b = clamp(*(gp-30396) - timerCount(0x27BE44), 0, *(gp-30396))
a = a * b / *(gp-30396)                        /* SysCfg opening dims it */
a = a * (128 - timerScale(0x27BF50, 128)) / 128 /* Version Info dims it */
a = a * getFadeAlpha() / 128                   /* the global curtain */
if (!timerIsState(getClockAnim(),  0)) a = 0
if (!timerIsState(getWizardAnim(), 0)) a = 0   /* 0x224D68 */
```

With no other screen open the middle two factors are 1, so
`alpha = mainAnimCount*128/10 * fadeAlpha/128`.

### 4.5 `0x2283A0` - the per-frame slot

`0x2283F0`'s second-from-last entry:
`timerStep(0x27BEA8); 0x228050(); 0x228110(); 0x228278()` - tick, state,
draw, input.  Input (`0x228278`) is `docs/menu-logic.md` §6.2 and has no
counterpart in osdbits.

---

## 5. What the port does, and how it was checked

`osdbits/menutext.c`, hooked into `menu.c` at two points.  It uploads
FNTASCII as an ordinary `Texture` (PSMT4 + 16-entry CLUT - the same path
`opening.c` already uses for the PSMT4 logos), then reproduces
`0x2086A0`, `0x209640`, `0x209998`, `0x208540`, `0x207F68`, `0x2080D0`,
`0x207E98`, `0x208110`, `0x21DC28`, `0x21DC88`, `0x228050`, `0x227E18`
and `0x2283A0`.

`tools/extract-res.py` grew a `--tables` mode that pulls the three
static tables (0x26FE60 metrics, 0x2715E0 CLUT, 0x298B08 strings) out of
the decompressed OSDSYS module into `res/FONTDATA.inc`; the font page
comes from the already-existing `--container FNTIMAGE` path.

Checked headless in PCSX2 by reading the drawn buffer back with
`sceGsExecStoreImage` and printing it as a 2x2-px luminance map
(`menutext.c`'s `DumpTextAscii`, argv[9]), and by rendering the same two
strings host-side in Python from the same three tables with the same
arithmetic.  Over the 176 x 24 block window the two agree on
**97.2 %** of cells (ink vs blank) and every row's ink span agrees to
within one block (2 px); the residual is the antialiased fringe, where
the GS's bilinear filter and the host model's point sampling fall on
opposite sides of the luminance ramp's threshold, plus the orb trail
that only the emulator has.  Swapping the cursor (argv[8] = 1) inverts
which label is bright, as `0x228110`'s colour swap should.

---

## 6. Appendix: the fly-in carousel (partial, from the same session)

Reversed before this task was re-scoped; recorded so it is not lost.
All of it is [ok] unless marked.  It extends `docs/menu-scene.md` §10.9's
"the carousel" bullet.

### 6.1 The ring at 0x34E6C0

12 entries, stride 48.  Entry 0's fields double as the ring's header:

| address | field |
|---|---|
| `0x34E6C0` | `offset` - which slot is at the front, seeded `(int)clockHours() % 12` |
| `0x34E6C4` | `spinY` (s16) - eases toward `clockSeconds() * 65536/60` |
| `0x34E6C6` | `tiltZ` (s16) - eases toward `offset * 65536/12` |
| `entry+0x10` | `progress`, 0..1, driven by timer `0x27EB00` |
| `entry+0x14` | a second float, cleared when the slot is the front one |
| `entry+0x20` | a colour qword |
| `entry+0x30` | a second colour qword (i.e. the NEXT entry's +0x00 - the entries genuinely overlap, or the stride is only nominal) [tnt] |
| `0x34E910` / `0x34E920` | two shared colour qwords, initialised from `0x27EB20` |
| `0x34E930` | `1 - clockMinutes()/60` - the progress ceiling |

### 6.2 `0x225BF8` - the per-frame driver (stage 10 of 0x21CF20)

```
timerStep(0x27EB00)
0x225978()                                  ; 0x225628 then 0x225878
for (i = 0; i < 12; i++) {
    if (i != *(int*)0x34E6C0) ring[i].f14 = 0;
    else {
        if (ring[i].f14 < *(float*)0x34E930)
            ring[i].f14 = min(ring[i].f14 + *(gp-32164), *(float*)0x34E930);
    }
    ring[i].progress = timerScale(0x27EB00, 128) * 0.0078125f   ; 1/128
}
```

`0x225628` re-derives `tiltZ`/`spinY`/the orb tilt+spin
(`gp-28854`/`gp-28856`) from the clock every frame, easing each toward
its target at `*(gp-32168)` and snapping `tiltZ` when `|spinY| < 201`.
It also writes `*(gp-28852)` (`menu.c`'s `orbEaseOut`) =
`1 - clockMinutes()/60` - **so `orbEaseOut` is not 0 in the ROM**, and
the orbit radius' ease target is `clockMinutes()/60`, not 1.0.  This is
a real (small) divergence in the current `osdbits/menu.c`. [ok]

**Two divergences this uncovers in the already-ported orb scene**
(`osdbits/menu.c`), both because `0x225628` runs EVERY frame, not once:

* `menu.c` keeps `orbEaseOut` (`*(gp-28852)`) at 0, so the orbit radius
  eases toward `10 + 7.25*1.0 = 17.25`.  The ROM rewrites that global
  every frame as `1 - clockMinutes()/60`, so `0x2261B8`'s ease target
  `1 - *(gp-28852)` is **`clockMinutes()/60`** and the radius target is
  `10 + 7.25*min/60` - the ring grows over the hour and snaps back on
  the hour.  At 12:34 the target is 14.18, not 17.25.
* `menu.c` seeds `orbTiltZ`/`orbSpinY` (`gp-28854`/`gp-28856`) once in
  `InitOrbAngles`.  The ROM re-eases both toward the live clock every
  frame at `*(gp-32168)`, so the "second hand at entry" is really a
  continuously-tracked, lagging second hand.  (`docs/menu-scene.md`
  10.2's parenthetical "the second hand at entry" wants amending.)

`0x225878` animates the two colour vectors at 0x34E940/0x34E950 out of
tables at 0x27EAC0/0x27EAD0/0x27EAE0/0x27EAF0 through two 8-entry
keyframe cyclers (`0x225528`, `0x2255A8`, both stepping `0x22EC60`).

### 6.3 `0x226028` - emitting the fly-in objects

Runs from `0x2268F0` only while `ring[0].progress > *(gp-32148)` (0.05):

```
for (i = 0; i < 12; i++) {
    slot = (i + *(int*)0x34E6C0) % 12
    0x225F80(i, tiltZ, spinY)               ; build the matrix, below
    scene->slot(+0x00) = slot
    scene->f6C        = ring[slot].progress
    copy mdTop() -> scene->world (0x27E970)
    *(qword*)0x27E9D0 = *(qword*)(ring[slot] + 0x20)
    *(qword*)0x27EA10 = *(qword*)(ring[slot] + 0x30)
    if (i == 0) { scene->f90 = 200.0; 0x225DD8(scene, 0x34E910, 100, 0x34E920, ring[slot].f14) }
    else        { scene->f90 = 160.0; 0x225DD8(scene, 0x34E910,   0, 0x34E920, -1.0) }
}
```

`0x225F80(i, angZ, angY)` on the 0x230000 matrix stack:

```
unit(top); mdRotZ(angZ); mdRotY(angY)
mdRotZ((i*65536)/12 - 32768)      ; the slot's place on the ring
mdTranslatef(0, 20, 0)            ; radius 20
mdRotY(angY*4)                    ; each object spins 4x the ring
```

### 6.4 The deferred draw list (extends menu-scene.md §10)

Dummy head **0x34E980**, records at `head + n*320`, count in `gp-30452`.
`0x225DD8` (mesh) and `0x225ED0` (orb) both `recIdx++`, compute the sort
key with `0x225D18` = `MulMatrix(scene->camera(+0x64), scene->world(+0x20))[3][2]`,
copy `scene[0 .. 0xE0)` to `rec+0x10`, then insert with `0x225D40`.

| record offset | field |
|---|---|
| +0x000 | `next` |
| +0x004 | `key`, camera-space Z; list is descending (far first) |
| +0x010..+0x0EF | a 224-byte copy of the scene struct (so the world matrix lands at **+0x030**) |
| +0x0F0 | `type` - 1 = orb, 0 = fly-in mesh |
| +0x0F4 | a float (`f12`): `ring[slot].f14` for slot 0, -1.0 otherwise |
| +0x100 | qword from `0x34E910` |
| +0x110 | an int (100 for the front object, 0 otherwise) |
| +0x120 | qword from `0x34E920` |
| +0x130 | the orb index (type 1 only) |

`0x2266E0` unpacks a mesh record into
`0x22D920(rec+0x10, rec+0x100, f12 = rec->0xF4, f13 = (float)rec->0x110)`.
`0x2267E8` then draws every mesh record **twice more** with
`0x22E428(rec+0x10, pass, rec+0x120, f12 = rec->0xF4)` for pass 0 and 1,
each pass bracketed by `0x22A4C8` (clear an offscreen target),
`0x22C020(1,0,0)` and `0x226768(30)` (a full-screen additive blit of the
record at 0x27EBB0 at alpha 30) - the objects' own glow/feedback pass.

### 6.5 Not reached

`0x22D920` (0xB08 bytes) and `0x22E428` were only partly read: they walk
352-byte sub-object records at **0x3502D0**, **0x3502B0** and
**0x3555D0** (all BSS, so built at runtime by something not yet found),
bind TEXCBUMP (`0x22AB90(2,1,2)`), project with `0x22CFA8` and emit
through `0x2293E0`/`0x22ED10`.  `vucode_2` (uploaded once per module
entry by `0x22EE88` -> `0x22EE00` from 0x268860) was not extracted.

**And the practical point: on the main menu the carousel is dormant.**
`0x2268F0` gates `0x226028` on `ring[0].progress > 0.05`, and
`ring[0].progress = timerScale(0x27EB00, 128)/128`, whose timer
(`0x27EB00`) is only opened by `0x225AD0` - which `0x227268` calls when
the user *enters System Configuration*.  On the main menu the timer is
closed, progress is 0, and nothing is emitted.  So the fly-in objects
belong to the System Configuration screen, not to the menu backdrop.
