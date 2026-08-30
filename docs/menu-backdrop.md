# The menu backdrop and the frame composite (Module U, `0x21D0A0`)

New reversing done while porting the TEXCKABE backdrop and the
zoom-blur/tint composite into `osdbits` (`osdbits/menuback.c`, mode
`menu`).  Written in the style of `docs/menu-scene.md`; §§ numbered to
be merged in as a successor to that file's §10.9, with the corrections
in §7 belonging to `docs/menu-draw.md`.

Method: `objdump -D -b binary -m mips:5900 -EL --adjust-vma=0x200000` of
`/u/aap/src/osdsys/expanded.bin`, every function read to its own
`jr ra`; static data read straight out of the image; conclusions checked
against a host-side Python model of the same arithmetic and against
PCSX2 software-renderer GS dumps of the port.  Confidence tags as in
`osdsys-map.md`: **[ok]** disassembly-backed and cross-checked, **[tnt]**
plausible/partial.

---

## 1. The verdict: the "wall" is a tunnel, and the blue comes from a multiply

`docs/menu-scene.md` §10.9 called `0x2292D0`'s mesh "the deep-blue
'smoke' wall behind the orbs".  Both halves of that turn out to be
wrong. [ok]

* The mesh is a **cylinder of radius 6000 around the camera**, 16
  angular ribbons by 33 rings, running from `z = -2500` (behind the
  camera) to `z = 37500` and closed at `z = 38750` by a degenerate ring
  on the axis.  The camera sits *inside* it at `z ~ -103` looking down
  +Z, so what you see is a **tunnel** whose far end is black — not a
  plane, not a wall.
* The mesh is drawn **nearly white** (`R:G:B = 230:260:260` at the near
  end).  The blue is applied afterwards, by `0x21D0A0`'s tail, as a
  single opaque full-screen textured quad modulated by
  `{0x37, 0x28, 0x3C}` — x0.43 red, x0.31 green, x0.47 blue.  The
  texture itself is a bluish noise field (means R 160, G 165, B 200), so
  the product lands on the menu's indigo.

The stage's structure is therefore: **draw bright, copy off, draw back
dimmed**, not "draw dim".

---

## 2. `0x21D0A0` — the whole stage, decoded

```
s2 = 0x27B4B0 | uncached                 ; the full-screen composite record
rec.x1 = w<<4 ; rec.y1 = h<<4
rec.u1 = (w<<4)+8 ; rec.v1 = (h<<4)+8    ; +8 = half a texel
0x22A3B8(0x1F0A10, evenOddFrame, 0x27B4A0, *(0x27B448))
                                         ; drawenv + CLEAR to {0,0,0,0x80}
0x229358(m0, m1)                         ; the backdrop tunnel, §3
v = 0x226948()                           ; the transition ramp, gp-28844
0x22C3C0(v < 6 ? v : 10 - v)             ; the zoom blur, §6
0x22A4C8(0, 0, 0)                        ; FRAME -> work buffer 3, NO clear
0x22A198(evenOddFrame)                   ; TEX0 -> the buffer being drawn
0x22C190(0)                              ; screen -> work buffer 3
0x22A4C8(1, 0, 0)                        ; FRAME -> work buffer 4, NO clear
0x22C190(0)                              ; screen -> work buffer 4
0x22A3B8(0x1F0A10, evenOddFrame, 0, 0)   ; FRAME -> back to the screen
0x22A290(0)                              ; TEX0 -> work buffer 3, PSMCT32
0x22A0C0(0, 1)                           ; ALPHA_1 = 0x48, TEST ZTST ALWAYS
0x2299C0(0x27B4B0)                       ; the tinted full-screen quad
```

The record at **`0x27B4B0`** is `{R 0x37, G 0x28, B 0x3C, A 0x80}`,
`x0/y0 = 0`, `u0/v0 = 8`, `z = 0`, **`ABE = 0`**, `TME = 1`.  ABE clear
is the load-bearing detail: the composite **overwrites** the screen, it
does not blend onto it.  The `0x22A0C0(0, 1)` immediately before it is
dead state. [ok]

### 2.1 `0x22A198(sel)` samples the buffer being drawn, not the previous one

`0x22A198(sel)` sets `TBP0 = sel ? 0 : w*h/64`, i.e. the *complement* of
the colour buffer `0x22A3B8(dbuff, sel)` selects — because
`sceGsSetDefDBuff` gives **`draw0` the SECOND colour buffer**
(disassembled from the SDK's `graph009.o`: only `draw0.FRAME.FBP` and
`disp1.DISPFB.FBP` get patched away from 0).  So with the same `sel`
passed to both, `0x22A198` binds **this frame's own draw buffer**.  The
opening's `sub_2144c0` uses the identical formula, and
`osdbits/opening.c` already models it as
`(frame == 0 ? &db.draw0 : &db.draw1)->frame1.FBP*32`. [ok]

That settles what would otherwise be a paradox: read as "the previous
frame", the composite would erase the backdrop it had just drawn and
nothing would ever accumulate.

### 2.2 There is no frame-to-frame feedback in the menu

Nothing in this stage reads the *previous* frame.  The whole stage is
intra-frame: clear -> tunnel -> (blur) -> copy out -> copy back tinted.
The menu's "motion-blurred wash" is the tint multiply plus the orbs'
own 50-sample trails, not an accumulation buffer.  (Contrast the
opening, which really does run `DrawToExtraBuf2`/`DrawExtraBuf2` as a
feedback pair.) [ok]

### 2.3 Five screen-sized GS buffers

| words | blocks (640x224) | role | reached as |
|---|---|---|---|
| 0 | 0 | colour buffer 0 | `draw1` / `0x22A198(1)` |
| `w*h` | 2240 | colour buffer 1 | `draw0` / `0x22A198(0)` |
| `2*w*h` | 4480 | Z (PSMZ24) | `sceGsSetDefDBuff` |
| `3*w*h` | 6720 | **work buffer 3** | `0x22A4C8(0,..)` FBP `w*h*3>>11`, `0x22A290(0)` TBP `w*h*3>>6` |
| `4*w*h` | 8960 | **work buffer 4** | `0x22A4C8(1,..)` FBP `w*h>>9`, `0x22A290(1)` TBP `w*h>>4` |

`0x229698` then starts the TEXC VRAM cursor at `w*h*5` words, exactly
past buffer 4 — which is the independent confirmation that both work
buffers are permanent, not scratch. [ok]

Buffer 4 is the shared scratch: the zoom blur (§6) ping-pongs the screen
through it, and `0x2267E8`'s carousel bloom (§8) renders into it.

---

## 3. `0x229358` — the gate

```
0x22ACC0(0x27F190)                 ; step the backdrop timer
if (!0x22AC48(0x27F190, 0))        ; state != 0
    0x229278()                     ; bgFade0/1/2 = 0x22AC20(timer, 40)
if (0x22AD30() != 0) return;       ; the module-wide fade is running
tail 0x2292D0(m0, m1)
```

**The tunnel is not drawn while `fadeMode != 0`.** [ok]  Since
`0x21CE58` ends with `0x22ADD8(2)` and mode 2 runs for 128 frames
(`menu-scene` §10.7a), the backdrop is absent for the first ~2.1 s of
every menu entry and fades in only as the black curtain lifts — i.e.
the wall appears *after* the orbs have flown in.

`bgFade0/1/2` (`gp-28840/-28836/-28832`) are added to every vertex
colour; nothing on the always-on path opens timer `0x27F190`, so they
are 0 in the idle menu and `0x2287B0` zeroes them at entry.

---

## 4. `0x2292D0` / `0x229130` — sixteen ribbons

```
0x22AB90(1, 1, 2)                  ; bind TEXCKABE, "additive", ztst 2
for (a = 0; a <= 0xFFFF; a += 0x1000) {
    0x2287E0()                     ; PRIM + CLAMP_1, one REGLIST packet
    0x229130(m0, m1, a, a+0x1000, *(gp-32096))
}
```

`0x2287E0` emits the GIFtag template at **`0x27F1A0`** (REGLIST, NREG 2,
REGS `{PRIM, CLAMP_1}`) with

* **`PRIM = 28`** = TRIANGLE_STRIP, `IIP = 1`, `TME = 1`, **`ABE = 0`**,
  `FST = 0` (ST/Q, not UV);
* **`CLAMP_1 = 0`** = REPEAT/REPEAT.

Re-emitting PRIM per segment is what breaks the strip between ribbons.
ABE clear means the tunnel is **opaque**, so the `additive` argument
`0x22AB90` was given is dead state. [ok]

`0x229130` opens one packet, copies the GIFtag template at
**`0x27F1B0`** (REGLIST, NREG **3**, REGS `{RGBAQ, ST, XYZF2}` — 24
bytes per vertex, 48 per emitted pair) and calls `0x2288C0` (the 33
rings) then `0x228E78` (the cap), then kicks.

---

## 5. `0x2288C0` — one ribbon, ring by ring

Constants, all plain floats in `.data`:

| address | gp offset | value | role |
|---|---|---|---|
| `0x2A7310` | `gp-32096` | **6000.0** | tunnel radius |
| `0x2A72F8` / `0x2A7308` | `gp-32120` / `gp-32104` | **65535.0** | angle -> S divisor |
| `0x2A72FC` / `0x2A7304` | `gp-32116` / `gp-32108` | **0.0002** | T scroll per frame |
| `0x2A7300` | `gp-32112` | **0.05** | radius wobble amplitude |
| `0x2A72F4` | `gp-32124` | **2050.0** | camera-space near cull |
| `0x2A730C` | `gp-32100` | **38750.0** | z of the cap ring |

```
e = (R*mdSin(ang), R*mdCos(ang), 0, 1)          ; per ribbon edge
s = ang / 65535                                 ; UNSIGNED angle, 0..0.9375
tint = (int)((mdCos(ang) + 1) * 10)             ; 0..20, per edge
scroll = (frameCounter % 5000) * 0.0002
for i = 0..32:
    scale = 1 + 0.05*mdSin((short)(frameCounter*100 + i*5120))
    p = ScaleVectorXYZ(e, scale)                ; w stays 1
    p.z = i*1250 - 2500
    p = ApplyMatrix(p, m0)                      ; m0 = the camera matrix
    if (p.z < 2050) continue                    ; both edges must pass
    q = 0x228838(m1, p, p)                      ; project, p /= w, q = 1/w
    if (|p.x - 2000| > 1000) continue           ; both edges, x then y
    if (|p.y - 2000| > 1000) continue
    ST   = (s*3*q, (i/32 + scroll)*3*q)         ; 0x228898, the 3.0 is literal
    XYZF = sceVu0FTOI4(p)                       ; x16 fixed, Z NOT shifted back
    pp = ((32-i)^3) >> 10
    RGB  = ((pp*230)>>5, (pp*260)>>5, (pp*260)>>5) + bgFade + tint
    A    = 0x40
```

Notes worth keeping: [ok]

* The **radius wobble is a travelling wave** — `i*5120` over 32 rings is
  2.5 turns of phase down the tunnel, at `frameCounter*100` per frame
  (a full cycle every 655 frames).  That, plus the T scroll, is the
  entire "smoke" animation; the texture is static.
* The **brightness ramp is cubic** in the distance from the near end,
  so `(32-i)^3 >> 10` is already 0 by ring 22 — the last third of the
  tunnel is pure black regardless of the texture.  That black disc is
  what reads as the vanishing point.
* Nothing clamps `RGB`.  `(pp*230)>>5` peaks at 230 at ring 0; with
  `bgFade` at its maximum 40 and `tint` at 20 that is 290, which would
  spill red into green.  It never happens in practice because ring 0 is
  always culled and the fade timer is never opened.
* The **cull rejects a whole pair**, which shortens the strip rather
  than holing it, because only the leading (nearest, off-screen) rings
  ever fail.
* `0x228838` = `ApplyMatrix` by `m1` (the view-screen matrix), then
  `ScaleVector` by `1/w` on **all four** components, returning `1/w`.
  That `q` becomes both the vertex's Q and the pre-multiplier on its ST,
  which is what makes the wrapping perspective-correct.
* `0x267050` = `sceVu0ScaleVectorXYZ` (xyz only, w preserved);
  `0x267668` = `sceVu0FTOI4`; `0x2676E0` = `sceVu0ScaleVector` (xyzw).
* `0x230018` (`mdSin`) reads a 16384-entry quarter-wave float table at
  **`0x3581F0`**; `0x230068` (`mdCos`) is `mdSin(a + 0x4000)`.  Both
  take the angle as a **signed 16-bit** value; the S coordinate,
  however, uses the raw **unsigned** argument.

### 5.1 `0x228E78` — the cap

Emits one more pair, **both vertices at `(0, 0, 38750)`** on the tunnel
axis, ST `(ang/65535 * 3 * q, (1 + scroll) * 3 * q)`, colour `bgFade`
only (so: black), no cull.  It closes the strip to a point and puts a
black cap over the vanishing point. [ok]

---

## 6. `0x22C3C0` — the zoom blur, and when it runs

```
for (k = 0; k < n; k++) {
    0x22BF58(1,0,0)     ; = 0x22A198(evenOddFrame) + 0x22A4C8(1,0,0)
                        ;   TEX = the screen, FRAME = work buffer 4
    0x22A0C0(1,1)
    rec(0x27F820).x1 = 5108 - 32k ; .y1 = 2388 - 16k
    rec.u1 = (w<<4)+8 ; rec.v1 = ((h-1)<<4)+8
    0x2299C0(rec)       ; the screen, shrunk to ~319 x 149
    0x22C020(1,0,0)     ; = 0x22A290(1) + 0x22A3B8(dbuff, evenOddFrame,0,0)
                        ;   TEX = work buffer 4, FRAME = the screen
    0x22A0C0(1,1)
    rec.x1 = w<<4 ; .y1 = (h-1)<<4 ; .u1 = 5108-32k+8 ; .v1 = 2388-16k+8
    0x2299C0(rec)       ; and stretched back full screen
}
0x22A0C0(1, 3)
```

`n = phase < 6 ? phase : 10 - phase` where `phase` is `gp-28844`, which
`0x2285C0` drives as `10 - count*10/duration` off the global transition
timer.  **In the idle menu `phase` is 10, so `n` is 0 and the blur does
not run at all** — it is a screen-transition effect, not the always-on
wash `menu-scene` §10.9 assumed.  Its record `0x27F820` is
`{0x80,0x80,0x80,0x80}`, `ABE = 0`, `TME = 1`: an opaque resample. [ok]

The down-scale is deliberately non-uniform (x0.499 horizontally,
x0.669 vertically), so it squashes as much as it blurs.

`0x22C228` is the same loop against work buffer 3 and record `0x27F7A0`
(`0x22BFD0` + `0x22C088`), reached from elsewhere; not chased. [tnt]

---

## 7. Corrections to `docs/menu-draw.md`

All [ok], all from reading the functions to their `jr ra` plus the
SDK's own `graph009.o` / `0x262308` / `0x2622A8`:

1. **§4.1 / §4.3 — the patched field is the GIFtag's NLOOP, not
   `FRAME.FBP`.**  `0x22A3B8` and `0x22A4C8` write `8` or `14` into the
   low 15 bits of the drawenv's **GIFtag** (`&~0x7FFF`, and 0x8000 = EOP
   stays set), choosing between "draw environment only" (8 A+D pairs)
   and "draw environment + clear" (14).  `sceGsSetDefDBuff` does exactly
   the same thing to the same words.  §13's "those two pages are only
   48 KB apart" puzzle dissolves.
2. **§4.3 — `0x22A4C8` does not always clear.**  Its second argument is
   a *colour record pointer*; `0x21D0A0` passes NULL for both work
   buffers, which selects NLOOP 8 and skips the clear.  Only
   `0x2267E8`'s two calls (record `0x27EBF0` = black) actually clear.
3. **§4.4 — `0x22A198` does not write `FRAME_1`.**  The `0x70000` it
   stores goes to register **71 = `TEST_1`** (`ZTE = 1`, `ZTST = 3`),
   not 76 = `FRAME_1`.  Neither `0x22A198` nor `0x22A290` ever touches
   FRAME; they are pure texture environments.
4. **§5.5 — `0x22AB90` binds with `CLAMP_1 = REPEAT/REPEAT`, not
   CLAMP/CLAMP.**  `sceGsSetDefTexEnv` writes 5 (CLAMP/CLAMP) and
   `0x22AA88` then overwrites that word with 0.  The backdrop needs it:
   its S runs 0..3.
5. **§5.5 — `0x22AB90`'s `ztst` argument is OR'd, not assigned.**
   `0x22AA88` computes `TEST_1 = (ztst << 17) | 0x30000`, so the low
   ZTST bit is always set: the `2` (GEQUAL) every caller passes arrives
   at the GS as **3 (GREATER)**.  `0x22A0C0` uses `| 0x10000` and does
   not have the bug.
6. **`0x262308` is `sceGsSetDefTexEnv(env, flush, tbp0, tbw, psm, tw,
   th, tfx, cbp, cpsm, cld, filter)`** — the SDK 3.0.3 header's
   prototype, with `w`/`h` actually being the **exponents**.  `TCC` is
   hard-wired to 1 inside it; `filter` sets `MMAG = filter & 1` and
   `MMIN = filter`.  `0x2622A8` is `sceGsSetDefAlphaEnv(env, pabe)`.
   The EE ABI here passes eight arguments in `a0-a3, t0-t3`.

And one to `docs/menu-scene.md`:

7. **§10.6 — the `Q` the ROM builds is exactly `1.0f`, not "1.0f
   shifted right by four bits".**  `li r,0xfe00; dsll32 r,r,0xe` is
   `0xFE00 << 46 = 0x3F80 << 48`, whose RGBAQ Q field (bits 32..63) is
   `0x3F800000`.  The same idiom appears in `0x22A3B8`'s clear colour
   and throughout `0x22EFF0`.

---

## 8. `0x2267E8` — the deferred list's second pass

```
0x22A4C8(1, 0x27EBF0, *(0x27B448))     ; work buffer 4, CLEARED to black
walk 0x34E980: node->type != 1 -> 0x22E428(node+16, 0, node+288, node->f4)
0x22C020(1, 0, 0)                      ; TEX = work buffer 4, FRAME = screen
0x226768(30)                           ; additive full-screen blit, A = 30
0x22A4C8(1, 0x27EBF0, *(0x27B448))     ; clear it again
walk again with the mode argument = 1
0x22C020(1, 0, 0)
0x226768(30)                           ; tail
```

`0x226768(a)` patches `rec(0x27EBB0).a = a` and draws it: `{0x80, 0x80,
0x80, a}`, `ABE = 1`, `TME = 1`, through `0x22A0C0(0, 1)` (ALPHA_1 =
0x48).  So this is a **two-pass additive bloom for the non-orb (carousel)
objects only** — they are rendered off-screen and added back at 23 %
twice.  With no carousel records in the list both walks are empty, the
buffer stays black and the two additive blits contribute nothing, so
the stage is correctly *inert* rather than missing in an orbs-only
port. [ok]

---

## 9. TEXCKABE's decoder (`0x22A7F4`, TEXC slot 1)

Slot 1 is the only TEXC slot with its own decoder; the per-slot jump
table is at **`0x2A4BA0`** (slots 0/2/3/5 -> `0x22A720`, the grey
expander; 4/6/7 -> `0x22A790`, `opening.c`'s "format 3"; 8 -> `0x22A8BC`;
9 -> `0x22A930`).

The blob is **64x64 packed RGB triples** (12288 bytes).  `0x22A7F4`
expands each source pixel to `R | G<<8 | B<<16 | 0x7F000000` and writes
it **four times**, tiling the 64x64 image **2x2 into the 128x128 page**
the descriptor at `0x27F1C0 + 12` declares (`wexp = hexp = 7`).  The
tiling is load-bearing, not decorative: the mesh's S runs 0..3 over the
declared page under REPEAT, so a 64x64 texture with the same UVs would
wrap half as often. [ok]

Content: a low-contrast bluish noise field — R mean 160 (137..184), G
mean 165 (143..190), B mean 200 (177..224).  Multiplied by the near-white
vertex colour and then by the composite's `{0x37,0x28,0x3C}`, the final
ratio is about **R 0.54 : G 0.45 : B 0.83** — the menu's indigo. [ok]

---

## 10. What the port does, and how it was checked

`osdbits/menuback.c` implements §§2-6 and §9; §8 is documented but not
wired up (it is inert without carousel records).

### 10.1 Against the retail console's own GS stream  [ok]

A single-frame GS dump of the **real BIOS main menu** (PCSX2 Shift+F8 on
`scph39001`, decoded with a resync walker over the raw `.gs`) confirms
every register of both stages, and every one of them matches what the
port emits:

| what | retail dump | port |
|---|---|---|
| mesh PRIM | `0x01c` TRISTRIP, IIP 1, TME 1, ABE **0**, FST 0 | identical |
| mesh TEX0 | TBP 11264, TBW 2, PSMCT32, TW 7, TH 7, TCC 1, MODULATE | identical (own TBP) |
| mesh TEX1 / CLAMP / TEXA | `0x61`; REPEAT/REPEAT; `{TA0 0x7F, AEM 1, TA1 0x81}` | identical |
| mesh TEST / ALPHA | ZTE 1, ZTST 3 GREATER; `0x48` | identical |
| ribbons per frame | 16 (`64` mesh GIFtags across 4 buffered frames) | 16 |
| screen FRAME | FBP 70 / FBP 0 on alternate frames | identical |
| work buffers | FBP **210** and **280**; TBP **6720** and **8960** | extraBuf1/2 |
| copy | FRAME 210, TEX0 TBP 2240 **PSMCT24** TW 10 TH 8, sprite (0,0)-(640,224), UV 0.5-640.5/224.5, RGBAQ 128,128,128,128, ABE 0 | identical |
| composite | FRAME 70, TEX0 TBP 6720 PSMCT32 TW 10 TH 8, CLAMP/CLAMP, ALPHA `0x48`, sprite full screen, **RGBAQ 55,40,60,128**, ABE 0 | identical |

Vertex data matches arithmetically, not just structurally.  Three
consecutive retail vertices of ribbon 0 read
`(R 170, G 190, B 190, A 64)`, `(169,189,189,64)`, `(156,174,174,64)` at
screen `(240.5, 703.5)`, `(663.1, 592.8)`, `(242.0, 513.9)`.  Feeding
i = 4, 5 and ang = 0, 4096 into §5's formula gives
`p = 28^3>>10 = 21`, `rr = 21*230>>5 = 150`, `gg = 21*260>>5 = 170`,
`tint(0) = 20`, `tint(4096) = 19` -> **170/190, 169/189, 156/174** —
exact, including the truncations.  The port's own dump reproduces the
same identity at its own ring (`p = 9`, `rr = 64`, `gg = 73`,
`tint = 19` -> R 83, G 92).

The retail dump also caught `0x22C3C0` running live with **n = 5**
(five screen -> work-buffer-4 -> screen ping-pongs), which is what §6
predicts for a transition ramp of 5, and confirms the blur is a
transition-only effect.

### 10.2 Against a host-side model  [ok]

A Python re-implementation of the camera (`menu.c`'s, already validated
against the orbs), the ribbon transform, both cull tests and the
projection puts the tunnel's vanishing point at screen `(245, 119)`;
PCSX2's `DumpFrameAscii` readback of the port puts the black disc's
centre in the same 8x8 block, and a rasterised version of the model
reproduces the port's ASCII luminance map to within 1-3 blocks.  One
sampled port vertex (ribbon 0, ring 12) lands at `(244.69, 233.94)`
against the model's `(244.73, 234.55)`.

---

## 11. THE ONE OPEN DISCREPANCY: the retail menu *looks* black

Every draw call matches, and yet the two do not look the same.

* The port renders a deep violet-blue tunnel filling the screen around
  the orb ring (PCSX2 screenshot, software renderer).
* The **retail** menu, rendered by the same PCSX2 build from the same
  draw calls, has a background that measures **exactly `(0,0,0)`** in
  every corner and along both side edges; only a faint bluish glow
  (region mean `(9, 16, 22)`) survives around the orb ring, which is
  consistent with the orbs' own halos.

This is not a "the mesh is not drawn" case: the very GS dump taken from
the same run, three seconds after the black screenshot, contains all 16
ribbons with the bright colours above, followed by the copy and the
tinted composite.  Nor is it a fade gate: `0x22AD30` returns 0 (the
curtain in that frame is `A = 0`), and the mesh packets are present in
all four dumps taken at 80, 110, 140 and 170 s.

Ruled out by inspection:

* **Buffer parity.**  Per frame the dump shows `FRAME 70` with the copy
  reading `TBP 2240` (= page 70), and on the next frame `FRAME 0` with
  the copy reading `TBP 0`.  The composite always samples the buffer
  just drawn, so it cannot be an inter-frame feedback that decays to
  black.  (This also rules out the tempting reading in which the
  composite overwrites the wall with the previous frame.)
* **A black texture.**  TEXCKABE's blob is a bright bluish noise field
  and `0x22A7F4` covers all 128x128 texels; the decoder's four writes per
  source pixel tile it exactly.
* **A later overdraw.**  The only full-screen primitives after the
  composite are the fade curtain (`A = 0` in that frame) and the
  carousel bloom's additive `A = 30` blit.
* **The screen-space cull.**  The retail vertices themselves span
  x = -720..1127 and y = -415..738 in screen pixels: the retail mesh
  covers the whole frame and beyond, exactly as the port's does.

So the difference lies somewhere *outside* the GIF stream — most likely
in how the emulator resolves the render-target-as-texture copy chain for
the two address layouts (the ROM's work buffers sit immediately after
the Z buffer at pages 210/280; the port's `extraBuf1/2` are separately
allocated with page guards), or in a GS behaviour the ROM relies on that
one of the two paths exercises and the other does not — note that
`0x22A198` deliberately passes `flush = 0`, so the retail copy samples a
just-rendered page **without a TEXFLUSH** (the NOP register `0x7F` is
written in its place, visible in the dump), whereas the port emits
TEXFLUSH before every bind.

**Tried and eliminated:** dropping the port's TEXFLUSH so its copies
sample the just-rendered page exactly as the ROM's do (the port now
matches the ROM here and emits TEXFLUSH only for the TEXC bind).  The
port's picture is unchanged — PCSX2's software renderer reads GS local
memory directly and has no texture cache to go stale, so this cannot be
the mechanism on the emulator side.  (It could still be the mechanism on
real hardware, where the GS's texture cache is real.)

**Recommendation:** treat `menuback.c` as correct at the draw-call level
(that much is proven) and treat "why the retail frame comes out black"
as an open question for a follow-up on a real console, or with a
GS-level step debugger that can dump work buffer 3 immediately after the
copy.  The single most informative next measurement is the content of
GS local memory at page 210 in the middle of a retail frame.

---

## 12. Open questions

* §11 above.
* Who, if anyone, opens the backdrop's fade timer `0x27F190`
  (`0x2291E8`/`0x229230`).  Until someone does, `bgFade0/1/2` are dead
  and the wall never brightens.
* `0x22C228`/`0x22BFD0`/`0x22C088` — the work-buffer-3 twin of the zoom
  blur.  Callers not chased.
* `0x22A6D8`'s remaining slot decoders (`0x22A8BC` for TEXCSTSL,
  `0x22A930` for TEXCMARU) are unread — both are 2D-widget textures.

---

## §11 resolution + real-console ground truth (aap, 2026-08-30)

The §11 discrepancy is RESOLVED: aap confirms the real main menu's
background is black - **the TEXCKABE tunnel is only visible in the
System Configuration screen**, not the main menu.  So the port's
draw-call-level match was real, and the missing factor is a per-screen
gate (prime suspect: the never-opened backdrop fade timer 0x27F190,
presumably opened by the System Config entry path exactly like the
carousel's 0x27EB00).  osdbits now defaults the backdrop OFF in menu
mode (argv `back=1` shows it).

More aap observations of the real System Configuration screen, for the
eventual port of that screen:
- the 12 carousel fly-in objects are a **glass clock: 12 rods arranged
  around the orbs** (the scene really is a clock, hour markers and all);
- the **5 glass cubes** (the config items) have a bumpmap-like surface
  effect - relevant to the vucode_2 lit-mesh path;
- the orbs in the real menu are **blurrier** than the port's - see
  menu-scene.md §10: the unported second orb pass into the offscreen
  buffer (+ its composite) is the standing suspect.

---

# Why the retail menu's orbs are blurrier: `0x2283D0`'s always-on zoom blur

Written in the style of `docs/menu-scene.md`; numbered to be merged in as
a successor to that file's §10, with the corrections in §7 belonging to
`docs/menu-backdrop.md`.

Method: `objdump -D -b binary -m mips:5900 -EL --adjust-vma=0x200000` of
`/u/aap/src/osdsys/expanded.bin`, every function read to its own `jr ra`;
static data read straight out of the image; checked against a decoded GIF
stream of the **retail BIOS's own menu** (`real1.pkl`, three consecutive
frames from a PCSX2 `.gs` dump of `scph39001`) and against PCSX2
`DumpGSData` folder dumps of the port.  Confidence tags as in
`osdsys-map.md`: **[ok]** disassembly-backed and cross-checked, **[tnt]**
plausible/partial.

---

## 1. The verdict

The orbs are not drawn differently.  Every register of the trail, the
halo and the core matches the port already.  What the port was missing is
a **whole-screen post-process that runs after the 3D scene and before the
2D text, on every single idle frame**:

```
0x21CF20 (Module U's frame hub), stage 5:
    0x2283F0
      -> 0x2283D0:  phase = *(gp-28844);
                    if (phase >= 5) 0x22C3C0(phase - 5);
      -> the 2D layer (0x226FA8, 0x227198, ..., 0x2283A0 = the menu text)
```

`*(gp-28844)` is **10** whenever no screen transition is running (§2), so
this is `0x22C3C0(5)`: **five bilinear shrink-and-stretch round trips
through work buffer 4** over the entire frame buffer.  The 3D layer -
orbs, trails, halos, backdrop - goes through all five; the text and the
rest of the 2D layer are drawn afterwards and stay crisp.  That, and
nothing else, is the softness aap sees on the console. **[ok]**

Measured point spread of the five passes (a host-side model of the exact
GS mapping, `orb/`'s throwaway script): an isolated bright pixel comes
out with **31 % of its peak and sigma 1.96 px horizontally**, **15 % of
its peak and sigma 1.81 px vertically**.  The orb core sprite is only
about 9 x 5 px and the halo 60 x 30 px, so the core all but dissolves
into the halo - exactly the reported look.

The five passes also **shift the 3D layer up and left**, by 1.5 px per
pass horizontally and 1.25 px vertically = **7.8 x 6.2 px** over the
five.  That is not a porting slip, it is what the record's `u0 = 0.5`
against `x0 = 0` does (§3); the port reproduces it because it emits the
same numbers.  It is invisible in practice because the offset is constant
and applied to a freshly composed image, and because the text is drawn
after it.

## 2. Why `phase` is 10, not 0

`0x2285C0` (stage 12 of `0x21CF20`) writes the ramp:

```
v = timerCount(0x27EC40)                  ; 0x22AC18 = *(t+4)
if (v >= *(gp-30396))  *(gp-28844) = 0
else                   *(gp-28844) = 10 - v*10 / *(gp-30396)
```

`*(gp-30396)` is set to **10** by the per-screen init `0x228460`
(`(isPAL ? 50 : 60)/6`), and nothing on the idle path ever opens the
timer at `0x27EC40`, so `v` stays 0 and the ramp sits at its maximum
`10 - 0 = 10`.  (It is `0` only for the one frame after a transition has
run to completion.) **[ok]**

The two blur call sites are complements over that ramp:

| site | argument | idle (phase 10) | mid-transition (phase 5) |
|---|---|---|---|
| `0x21D0A0` (before the scene) | `phase < 6 ? phase : 10 - phase` | **0** | 5 |
| `0x2283D0` (after the scene) | `phase >= 5 ? phase - 5 : 0` | **5** | 0 |

So the blur does not switch on and off during a transition - it *migrates*
from after the scene to before it, keeping five passes' worth in flight
the whole way down to phase 5, then fading out below that.

## 3. `0x22C3C0` exactly as the GS sees it

`0x22C3C0(n)`, record `0x27F820` = `{0x80,0x80,0x80,0x80}`, `x0/y0 = 0`,
`u0/v0 = 8` (i.e. 0.5 texel), **`ABE = 0`**, `TME = 1`:

```
x = 5108; y = 2388;                       ; 1/16 pixel units
for (k = 0; k < n; k++) {
    0x22BF58(1,0,0)   ; TEX0 = the screen  (0x22A198: TBP = this frame's
                      ;                     draw buffer, PSM = PSMCT24,
                      ;                     TW 10 TH 8, filter = 1)
                      ; FRAME = work buffer 4 (0x22A4C8(1,...): FBP 280)
    0x22A0C0(1,1)     ; ALPHA_1 = 0x44, TEST_1 = ZTE 1 / ZTST 1 ALWAYS
    rec.x1 = x ; rec.y1 = y
    rec.u1 = (w<<4)+8 ; rec.v1 = ((h-1)<<4)+8
    0x2299C0(rec)     ; SPRITE, FST, ABE 0
    0x22C020(1,0,0)   ; TEX0 = work buffer 4 (0x22A290(1): TBP 8960,
                      ;                       PSM = PSMCT32)
                      ; FRAME = the screen  (0x22A3B8)
    0x22A0C0(1,1)
    rec.x1 = w<<4 ; rec.y1 = (h-1)<<4
    rec.u1 = x+8 ; rec.v1 = y+8
    0x2299C0(rec)
    x -= 32; y -= 16;
}
0x22A0C0(1, 3)
```

so the five destination rectangles are **319.25 x 149.25, 317.25 x
148.25, 315.25 x 147.25, 313.25 x 146.25, 311.25 x 145.25**, always from
the full `640 x 223` screen, and each is stretched straight back.

**The whole effect is `sceGsSetDefTexEnv`'s `filter` argument.**  Both
`0x22A198` and `0x22A290` pass `filter = 1`, which sets `MMAG = MMIN =
LINEAR` (`TEX1 = 0x61`).  With NEAREST this loop would be a lossy
resample and nothing more; with LINEAR each round trip is a ~2 px box
followed by a ~4 px triangle, and five of them compound. **[ok]**

The half-texel bookkeeping is what produces §1's shift: the destination
*corner* `x = 0` is mapped to source `u = 0.5` (the centre of source
texel 0), not to `u = 0`, in both directions of the round trip, so each
pass moves the picture by half a *source* texel of the down-scale
(`0.5 * 640/319.25 = 1.0` px) plus half a destination texel
(`0.5` px) = 1.5 px.

## 4. The retail GS stream, one whole frame

Decoded from `real1.pkl` (retail BIOS, PCSX2 `.gs`, frame between the 1st
and 2nd VSYNC).  141 draws.  Order, with what each one is:

| # | FRAME | what |
|---|---|---|
| 1 | 70 | full-screen `TME 0` clear |
| 2-17 | 70 | 16 x 60-vertex TRISTRIP, TEX0 TBP 11264 TW7 TH7 = TEXCKABE, A 64 - the backdrop tunnel |
| 18 | 210 | copy screen (TBP 2240, **PSMCT24**) -> work buffer 3 |
| 19 | 280 | copy screen -> work buffer 4 |
| 20 | 70 | composite: TEX0 TBP **6720** (work buffer 3), RGBAQ **(55,40,60,128)**, ABE 0 |
| 21-41 | 70 | **7 orbs, visible pass**: trail, halo, core |
| 42-62 | 210 | **7 orbs, offscreen pass** into work buffer 3 |
| 63,65 | 280 | `0x2267E8`'s two clears of work buffer 4 to black |
| 64,66 | 70 | `0x226768(30)` additive blits of the (black) work buffer 4 |
| 67 | 70 | a 96-vertex IIP TRISTRIP (2D-ish, unidentified) |
| 68 | 70 | the fade curtain, `A = 0` |
| **69-78** | **280 / 70** | **the five zoom-blur pairs** |
| 79-141 | 70 | the menu text (TEX0 TBP 12037, TW 8 TH 9) |

The five pairs, verbatim:

```
FRAME 280 <- TEX0 {TBP 2240, TBW 10, PSM 1(CT24), TW 10, TH 8}  TEX1 0x61
            ALPHA 0x44  PRIM SPRITE/TME/FST, ABE 0  RGBAQ 128,128,128,128
            XY (0,0)-(319.25,149.25)   UV (0.5,0.5)-(640.5,223.5)
FRAME  70 <- TEX0 {TBP 8960, TBW 10, PSM 0(CT32), TW 10, TH 8}  TEX1 0x61
            XY (0,0)-(640,223)         UV (0.5,0.5)-(319.75,149.75)
   ... then 317.25/148.25 & 317.75/148.75
   ... then 315.25/147.25 & 315.75/147.75
   ... then 313.25/146.25 & 313.75/146.75
   ... then 311.25/145.25 & 311.75/145.75
```

(XY quoted after subtracting `XYOFFSET_1` = 1728, 1936 = `(2048-w/2)<<4`,
`(2048-h/2)<<4`.)

`osdbits`' `ZoomBlur()` now emits exactly these ten sprites, with the same
TEX0/TEX1/ALPHA/PRIM and the same 1/16-pixel extents. **[ok]**

### 4.1 The orb draws already matched

From the same frame, per orb (all `ALPHA_1 = 0x48` = `Cs*As + Cd`,
`TEX1 = 0x61`):

| primitive | retail | `osdbits/menu.c` |
|---|---|---|
| trail | `PRIM` LINE_STRIP, IIP 0, TME 0, ABE 0, **AA1 1**, 49 vertices | identical |
| halo | SPRITE, TME 1, ABE 1, FST 1; TEX0 TBP **11840** TBW 1 TW 6 TH 6 (TEXCBLUR, 64x64); UV (0,0)-(63,63); RGBAQ **(48,98,128,60)** | identical (`0x27EB30` = `{0x30,0x62,0x80,0x3C}`) |
| core | same PRIM; TEX0 TBP **11776** (TEXCNAVI); UV (0,0)-(63,63); RGBAQ **(128,128,128,128)** | identical (`0x27F930`) |

Sprite extents agree too: a retail halo is
`(1982.19,2060.19)-(2041.75,2090.00)` = 59.6 x 29.8 px, a core
`(2007.50,2072.88)-(2016.44,2077.31)` = 8.9 x 4.4 px, which is
`depth * 6.5e-06 * {30, 4.5}` with the vertical half of that - the
formula `menu.c` already implements.

So suspects 2 (halo/sprite parameters), 3 (texture content / a missing
particle texture) and 4 (the wobble phases) are all **negative**:

* only TEXC slots 6 and 7 are ever bound by `0x22EFF0`; the slot table at
  `0x27F1C0` (stride 12, `{ptr, wexp, hexp}`) gives both of them
  `wexp = hexp = 6`, i.e. genuine 64x64 pages, and the retail TEX0s
  confirm `TW 6 TH 6`.  No third texture participates. **[ok]**
* `res/TEXCBLUR_EXP` and `res/TEXCNAVI_EXP` are 4096 bytes each (64x64
  8-bit) and decode through `0x22A790` (`opening.c`'s "format 3"), which
  is what `menuTextures[]` declares.  TEXCBLUR is a soft radial falloff
  filling the middle ~34 texels, TEXCNAVI a solid disc across the whole
  page - the ROM's own content.
* `0x34E960`'s phases only feed the entry scatter, which the port uses
  the same way.

## 5. So what *is* the second orb pass for?

`0x22EFF0` really does run the whole orb twice - at `0x22F784` it points
`FRAME` at **work buffer 3** (`0x22A4C8(0, NULL, field)` -> FBP 210, no
clear) and repeats the trail, halo and core - and it never puts `FRAME`
back; `0x2267E8` is the next thing to set it.

**In the idle menu that second pass is dead, and the retail GS stream
proves it.**  In the whole 141-draw frame above, exactly **one** draw
samples TBP 6720 (= work buffer 3): the composite at #20, which runs
*before* the orbs.  The next frame's `0x22C190(0)` copy is an **opaque**
sprite (`rec(0x27F760)+0x34 = ABE = 0`) covering the full screen, so it
overwrites work buffer 3 wholesale before anything can read the orbs back
out of it. **[ok]**

That resolves the tension `docs/menu-backdrop.md` left open.  The two
"inert" readings were both right; what was wrong was the assumption that
*something* must consume the offscreen copy.  Nothing does, on this
screen.  Work buffer 3 has a live consumer elsewhere - `0x22C228` /
`0x22BFD0` / `0x22C088`, the work-buffer-3 twin of the zoom blur, called
from the stage at `0x226D80`-`0x226F74` (a different scene: 12 objects
transformed through `0x22D2E8`/`0x22D798`, then the wb3 blur, then
`0x22C2A0`) - so the second pass is presumably live on *that* screen and
is simply carried along, unread, by the main menu.

Two details of the second pass that are only visible because it exists,
and that pin down `0x22EFF0`'s stack layout:

* its core sprite uses the colour record at **`0x27F940` =
  `{0xFF,0xFF,0xFF,0x80}`** where the visible pass uses `0x27F930` =
  `{0x80,0x80,0x80,0x80}` - and the retail dump shows exactly that,
  `(255,255,255,128)` on FRAME 210 against `(128,128,128,128)` on
  FRAME 70;
* its halo sprite comes out at `A = 128` where the visible one is
  `A = 60`, because `0x22F794`'s `sw v1(=128), 60(sp)` lands **inside**
  the halo's colour quadword: `sp+48..63` is the copy of the trail head's
  colour and `sp+60` is its alpha.

## 6. One real quirk the port does *not* have (deliberately)

That same aliasing means the visible pass's halo and core alphas are
**timer-scaled**: `0x22F54C` reads `sp+60` (the halo colour's alpha, 60),
passes it through `0x22AC20(0x27F900, a)` and writes the result back into
`sp+60` before the sprite is drawn; `0x22F678` does the same to `sp+28`
(the core's 128).  `0x27F900` is the trail timer that `0x22EE98` opens for
`(fps<<8)/60` = 256 counts and `0x22EFF0` steps once per orb, so both
alphas ramp 0 -> 60 / 0 -> 128 over the first ~37 frames of a menu entry
and are constant thereafter.  `osdbits` passes the raw record colours, so
it matches from frame 37 on and is slightly brighter before that.  Not
ported: it is invisible under the 128-frame fade-up that runs over the
same window.  Listed here as **approximated**.

## 7. Corrections to the docs

1. **`docs/menu-backdrop.md` §6 - "the blur is a screen-transition
   effect, not the always-on wash" is WRONG.**  It is *both*, and in the
   idle menu it is at **full strength**.  §6 read only `0x21D0A0`'s call
   site.  The always-on one is `0x2283D0`, reached from stage 5 of
   `0x21CF20`, and at `phase = 10` it runs `0x22C3C0(5)`.
2. **`docs/menu-backdrop.md` §10.1 mis-attributes its own evidence.**  It
   records that "the retail dump also caught `0x22C3C0` running live with
   n = 5 ... which is what §6 predicts for a transition ramp of 5".  The
   dump was of an *idle* menu; n = 5 is the idle value, from `0x2283D0`.
   That observation was the answer to this whole question, sitting in the
   notes already.
3. **`docs/menu-scene.md` §10.6's aside about the second pass** - "the
   offscreen buffer the zoom-blur composite samples" - is wrong twice
   over: the buffer is work buffer **3**, and on this screen nothing
   samples it (§5).  The zoom blur ping-pongs through work buffer **4**
   and reads the *screen*.
4. **`docs/menu-backdrop.md` §2.2 "there is no frame-to-frame feedback in
   the menu"** stands, and is now the reason the second pass is dead
   rather than an unexplained coincidence.
5. `docs/menu-backdrop.md` §12's open question "`0x22C228`/`0x22BFD0`/
   `0x22C088` - callers not chased" is partly closed: `0x226F00`/
   `0x226F08` in the stage at `0x226D80` (see §5).  Which screen that
   stage belongs to is still open. **[tnt]**

## 8. What the port does now

`osdbits/menuback.c`:

* **`BlurBlit(tbp, psm, x1, y1, u1, v1)`** - new.  Emits `0x2299C0`'s
  sprite straight in 1/16-pixel units (TEX1, TEX0, CLAMP_1, PRIM, RGBAQ,
  UV/XYZ2 x2).  The old code went through `Rect`/`pktSetTexRect`, which
  is whole-pixel only and shrinks the far corner by 1/16 - it could not
  express 319.25 x 149.25.  **Exact.**
* **`ZoomBlur(n)`** - rewritten on top of `BlurBlit`; same loop and same
  constants as before, now with the ROM's exact extents, the ROM's PSMs
  (PSMCT24 reading the screen, PSMCT32 reading work buffer 4) and the
  ROM's `ALPHA_1 = 0x44` / ZTST ALWAYS around it.  **Exact.**
* **`MenuZoomBlur()`** - new, `real: 0x2283D0`.  `if (backPhase >= 5)
  ZoomBlur(backPhase - 5)`.  **Exact** (given `backPhase`, see below).
* **`MenuBackFrameStart()`** - new, **NOT original**.  Snapshots
  `evenOddFrame` and the draw buffer's TBP once per frame, so
  `MenuBackdrop()` and `MenuZoomBlur()` cannot straddle a swap-thread
  flip and address different buffers.  `MenuBackdrop()` used to do this
  itself; it now uses the snapshot.
* `backPhase` (argv[11], default 10) is now documented as the transition
  ramp `*(gp-28844)`.  It still defaults to 10 = idle, which is what
  makes the blur on by default.  `backPhase = 5` turns the post-scene
  blur off without touching anything else, which is the A/B switch used
  below.  **Approximated**: the port has no transition timer, so the ramp
  is a constant instead of `0x2285C0`'s output.

`osdbits/menu.c`:

* `MenuFrame()` calls `MenuBackFrameStart()` first, and `MenuZoomBlur()`
  between `SceneWalk()` (stage 3, `0x2268F0`) and `MenuTextFrame()`
  (stage 5's 2D layer).

`osdbits/inc.h`: two declarations.

**Not ported / stubbed**: the second orb pass into work buffer 3 (§5 -
dead on this screen, and porting it would cost a second full orb render
for nothing); `0x22C3C0`'s transition-side call from `0x21D0A0` still
exists but is still driven by the same fixed `backPhase`; the halo/core
alpha ramp of §6.

### 8.1 Ordering note

The ROM's stage order is scene -> fade curtain (`0x22B020`) -> blur
(`0x2283D0`) -> text.  The port draws the curtain *after* the text.  The
blur was inserted before the text anyway, so the port's order is
scene -> blur -> text -> curtain.  That is safe: the curtain is a uniform
full-screen alpha blend and the blur is linear, so
`blur(lerp(scene, black, a)) == lerp(blur(scene), black, a)`.

## 9. How it was checked

**Register level.**  `real1.pkl` (retail BIOS `.gs`, decoded with a GIF
walker) against a PCSX2 `DumpGSData` folder dump of the patched port
(`menu 12 34 56 160 1 128 0 0 1 0 10 0`, SW renderer, one frame).  The
port's ten blur sprites, verbatim from the dump:

| # | FRAME FBP | TEX0 | TEX1 | ALPHA | XY | UV |
|---|---|---|---|---|---|---|
| 04556 | 0x2580 (extraBuf2) | 0x8c0 tbw10 **PSM 1** tw10 th8 | 1/1 | 0,1,0,1 | (0,-0.5)-(**319.25**,148.75) | (0.5,0.5)-(640.5,223.5) |
| 04557 | 0x8c0 (screen) | 0x2580 tbw10 **PSM 0** tw10 th8 | 1/1 | 0,1,0,1 | (0,-0.5)-(640,222.5) | (0.5,0.5)-(**319.75**,149.75) |
| 04558/9 | | | | | 317.25/147.75 | 317.75/148.75 |
| 04560/1 | | | | | 315.25/146.75 | 315.75/147.75 |
| 04562/3 | | | | | 313.25/145.75 | 313.75/146.75 |
| 04564/33 | | | | | 311.25/144.75 | 311.75/145.75 |

against the retail rows in §4: identical PRIM (SPRITE, TME 1, ABE **0**,
FST 1), identical TEX0 (TBW 10, TW 10, TH 8, PSMCT24 reading the screen /
PSMCT32 reading the work buffer), identical TEX1 (`0x61`), identical
ALPHA (`0x44`), identical ZTST (1 = ALWAYS), identical RGBAQ
(128,128,128,128), identical UVs, identical widths.

The **0.5 px offset in Y** is not a mismatch: `XYOFFSET_1.OFY` is
`1936.5` in the port's dumped frame and `1936.0` in the retail one -
field parity.  Both write `y0 = 0` into the record and let the draw
environment's per-field OFY place it (`0x22A3B8`'s fourth argument is the
field; `vif1SetXYOffset`'s `halfpx` is the port's equivalent), so the two
behave the same way; the dumps just caught opposite fields.  X matches to
the 1/16, including the 0.25 fractional widths.

**Pass counts.**  Retail: 7 orbs x 3 primitives x 2 passes = 42 orb draws,
then 10 blur sprites, per frame.  Port: 7 x 3 x 1 = 21 orb draws (the
second pass is deliberately absent), then the same 10 blur sprites.

**Visually**, `DumpFrameAscii` at frame 90, orbs only
(`menu 12 34 56 0 1 128 90 0 1 0 <phase> 0`), 8x8-block max luminance:

```
phase 5  (no post-scene blur = the old behaviour)   phase 10 (the ROM's idle)
|                  ...                           |  |                 :*##=.   |
|                .=@@@*.                         |  |               .:*@@@#:   |
|              :-+#@@@#:                         |  |               ..=%@@*.   |
|             .::--*@@+.                         |  |                   ..     |
|            .::                                 |  |                          |
|            :.                                  |  |                          |
|             .                                  |  |                          |
```

(columns trimmed to the interesting 40).  The cluster stays where it was,
the hard `@@@` cores keep their peak but lose their edges, the dim trail
tail is smeared below the ramp's first level, and the whole thing moves
one 8-px block left and about six rows' worth up - the §1 shift, to the
block.  Nothing else in the map changed.
