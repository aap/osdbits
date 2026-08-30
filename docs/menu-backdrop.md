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
