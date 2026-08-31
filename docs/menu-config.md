# The System Configuration screen, decoded and ported (2026-08-31)

Written in the style of `docs/menu-scene.md`; numbered to be merged in
as a successor to `docs/menu-text.md` §6 (the carousel groundwork), with
the corrections in §9 belonging to `docs/menu-scene.md` and
`docs/menu-backdrop.md`.

Method: `objdump -D -b binary -m mips:5900 -EL --adjust-vma=0x200000` of
`/u/aap/src/osdsys/expanded.bin`, every function read to its own `jr ra`;
a whole-image `jal`/`j` index for the call-graph claims; static data read
straight out of the image; checked against PCSX2 frame readbacks of the
port.  Confidence tags as in `osdsys-map.md`: **[ok]** disassembly-backed
and cross-checked, **[tnt]** plausible/partial, **[?]** guess.
Everything is **[ok]** unless marked.

---

## 1. The verdict

Entering System Configuration opens **four** timers, and every visible
difference between that screen and the main menu is one of them:

| timer | opened by | what it switches on |
|---|---|---|
| `0x27BE44` | `0x227268` itself | the screen's own Anim - gates the 2D item list, and its *count* is the clock every other stage reads |
| `0x27F190` | `0x2291E8`, from `0x2272B8` | the backdrop fade - the **TEXCKABE tunnel** |
| `0x27EB00` | `0x225AD0`, from `0x2272C0` | the carousel - the **12 glass rods** |
| `0x27EC00` | `0x226B28`, from `0x2273D0` (the state machine, at count == dur80) | the **5 glass cubes** |

So aap's console description is exactly the four timers: tunnel, orbs,
a twelve-rod clock ring, five bumpmapped cubes.

And the rods and cubes are **the same renderer over two static meshes**
that are plain `.data` inside OSDSYS - no model loading, no resource, and
(§8) **no VU1 at all**.

---

## 2. `0x227268` - "enter System Configuration"

Read to its `jr ra`.  It is the whole entry point; the only caller is
`0x22836C`, inside the main menu's pad handler.

```
0x227268:
    if(!timerIsState(0x27BE44, 0)) return;           /* 0x22AC48 */
    0x27BE44->duration = *(gp-30380) + *(gp-30400) + *(gp-30396);
    timerOpen(0x27BE44);                             /* 0x22AC70 */
    0x22AEC8();                                      /* timerClose(0x27F620) */
    0x2291E8();                                      /* timerOpen(0x27F190) */
    0x225AD0();                                      /* the carousel, §4 */
    tail 0x2287A8(20992, 1, 4);                      /* = 0x200B80, the UI click */
```

The three durations come from `0x228460` (`docs/menu-text.md` §4.2):
`*(gp-30400)` = `rate*40/60` = **40**, `*(gp-30396)` = `rate/6` = **10**,
and `*(gp-30380)` = **80** on NTSC, so the Anim runs **130 frames** and
its count splits into three legs, 80 / 40 / 10.  Each leg is a cue:

* count == 80 → `0x2273D0` opens the cube timer;
* count 120..130 → `0x226A60`'s alpha ramps 0 → 128 (the item list);
* on the way out, `0x227390`'s closing arm fires `0x226B70` (cubes) at
  10 from the end, `0x229230` (backdrop) at 80 and `0x225B68`
  (carousel) at `*(gp-30372)`.

### 2.1 `0x228278` - how the main menu gets there

The ROM's own main-menu pad handler, and it settles what the two items
do.  `s0 = 0x27BEA8` (the Anim), so `s0-24` is the page header `0x27BE90`
and `s0-8` is its **cursor**:

```
if(!timerIsState(0x27BEA8, 2)) return;      /* the menu is not open yet */
if(!0x227FC0()) return;                     /* no other screen is open */
if(*(0x27B444) != 0) return;
pad = *(gp-30316);
if(pad & 0x1000)      cursor--  (clamped at 0), click 0x2287A8(20992,1,6)
else if(pad & 0x4000) cursor++  (clamped at count), same click
else if(pad & 0x20)   /* CIRCLE */
        if(cursor == 0 && getFadeMode() == 0) 0x227F50(0);   /* Browser */
        if(cursor == 1)                       0x227268();    /* SysConfig */
else if(pad & 0x10)   /* TRIANGLE */          0x2210C8();    /* Version Info */
```

which matches the item list at `0x27BE80` (`{90 Browser}, {91 System
Configuration}`) exactly.  `osdbits`' `MenuSelectItem()` hook is now
wired to this.

### 2.2 `0x227FC0` - "no other screen is open"

`timerIsState(0x27BE44, 0) && timerIsState(0x27BF50, 0) &&
timerIsState(clockAnim(0x223790), 0) && timerIsState(wizardAnim(0x224D68), 0)
&& 0x221908()`.  So opening `0x27BE44` is what silences the main menu's
input **and** its labels (`0x228110` calls it too).

---

## 3. The System Configuration item list

Static data, read out of the image.  Header **`0x27BE28`** =
`{title 91, items 0x27BD10, count 5, rows 0, cursor 0}`, items at
**`0x27BD10`**, stride **56**, with the string id at +0x00, a per-item
kind at +0x04, a sub-list pointer at +0x10 and six callbacks at
+0x14..+0x28.  The five ids resolve in the English table (0x298B08) to

| n | id | string |
|---|---|---|
| 0 | 106 | `Clock Adjustment` |
| 1 | 107 | `Screen Size` |
| 2 | 111 | `\7r0.90DIGITAL OUT (OPTICAL)\7r0.00` |
| 3 | 114 | `Component Video Out` |
| 4 | 117 | `Language` |

i.e. the retail System Configuration menu, and **five items for five
cubes**.

`0x227D08` (the screen's draw slot) calls `items[cursor].fn(+0x28)` with
0 or 1 - a focus-change callback - and then tail-jumps to `0x2279B8`
(mode 0) or `0x227BE8` (mode 1), which are the screen's own **pad**
handlers (UP/DOWN over the same header, CIRCLE into `items[cursor].fn
(+0x14)`).  The item *rows* themselves are drawn by each item's own
widget code at `0x21DF28`-`0x21F168`, reached from the other slots of
`0x2283F0`; **none of that is ported.**

---

## 4. The carousel: twelve glass rods

### 4.1 `0x225AD0`, the only opener of `0x27EB00`

```
if(!timerIsState(0x27EB00, 0)) return;
0x22AC60(0x27EB00); timerOpen(0x27EB00);
for(i = 0; i < 12; i++) ring[(i + *(int*)0x34E6C0) % 12].progress = 0;
```

### 4.2 `0x225BF8` and `0x225F80`

`docs/menu-text.md` §6.2/§6.3 already have these and both check out; the
matrix chain confirmed instruction for instruction, including that the
translation is `mdTranslatef(0, 20, 0)` (`0x41A0` = 20.0) and the last
term `mdRotY(angY*4)` is a `sll 0x12` / `sra 0x10`, i.e. the 16-bit
angle times four.

### 4.3 `0x226028` - the emitter, corrected in one place

`docs/menu-text.md` §6.3 has the shape right.  Two details it does not
record: the "size" it writes into `scene+0x90` is **200.0** for the front
object and **160.0** for the rest (`0x4348`/`0x4320` << 16), and the
`f12` argument is `-1.0` for every object but the front one - which
matters, because `0x22D920`'s very first branch is `if(!(f12 > 0)) goto
0x22E0EC`, a **completely different, simpler draw path**.  Eleven of the
twelve rods always take it, and so does the front one until its `split`
has grown past 0.

---

## 5. The five cubes: `0x226D00`, stage 2 of `0x2283F0`

Reached as `0x226FA8` (`timerStep(0x27EC00); 0x226CF8(); 0x226D00()`),
which `0x2283F0` calls immediately after the always-on zoom blur
(`0x2283D0`) and before every 2D layer.

```
if(timerIsState(0x27EC00, 0)) return;          /* the config-only gate */
t = timerCount(0x27EC00) / (float)*(gp-30400)  /* 0 -> 1 over 40 frames */
for (i = 0; i < 5; i++) {                       /* 0x27F090, stride 48 */
    ang = (short)(*(u16*)(gp-30432) + i*7000)
    scene(0x27EFB0)->f68 = f6C = f70 = t + entry[i].bias
    scene->colour(+0x80)             = entry[i].colour
    unit(mdTop); mdTranslate(entry[i].pos)
    mdRotX(ang); mdRotY(ang); mdRotZ(ang)
    copy mdTop -> scene->world(+0x20)
    0x22D2E8(scene)                             /* to the screen */
}
... the same loop again through 0x22D798 (into work buffer 3), then
    0x22BFD0(0,1,0) + 0x22C088() + 0x22C2A0(phase) + 0x22C020 + 0x22C190
```

`*(gp-30432)` is bumped by **30 every frame** by `0x2285C0` and zeroed by
`0x228460`, so a cube turns once every 2184 frames and the five are
7000 units of angle apart.

The placement table at **`0x27F090`** (5 × 48: `float pos[4]`, `int
colour[4]`, `float bias`):

```
(-11.50, -11.50, 47.5)   {128,128,128,128}   0.0
(-22.50,  -5.50, 47.5)   {128,128,128,128}   0.0
(-10.75,  -0.25, 47.5)   {128,128,128,128}   0.0
(-21.75,   6.00, 47.5)   {128,128,128,128}   0.0
(-11.25,  11.50, 47.5)   {128,128,128,128}   0.0
```

- a zig-zag column on the left of the frame, in front of the orb ring.
The second render pass through `0x22C228`'s work-buffer-3 twin of the
zoom blur is exactly the caller `docs/menu-backdrop.md` §12 could not
find: **this screen is where work buffer 3 has a live consumer**, and it
closes that open question.

---

## 6. The two meshes, and where the geometry lives

`0x22CFA8`'s per-face loop reads its model out of the **scene struct
itself** - both structs are static `.data`:

| field | rod `0x27E950` | cube `0x27EFB0` |
|---|---|---|
| +0x04 face count | 16 | 6 |
| +0x08 vertices (4 × 16 B per face) | `0x27E050` | `0x27EC50` |
| +0x0C face normal (16 B per face, w = 0) | `0x27E850` | `0x27EDD0` |
| +0x10 vertex UV (4 × 16 B per face) | `0x27E450` | `0x27EE30` |
| +0x60 / +0x64 | the view-screen / camera matrix pointers | ditto (statically `0x352800`/`0x352840`) |
| +0x68/+0x6C/+0x70 scale | 1, 1, 1 - **+0x6C is overwritten with the fly-in progress** | 1, 1, 1 - all three overwritten with the cube timer's ramp |
| +0x80 colour | `{0,0,0,0x80}`, overwritten per slot | overwritten per cube |
| +0x90 size | 128.0, overwritten (200/160) | 200.0 |
| +0xB8 | 1.0, the refraction scale | 1.0 |

* **the rod** is a **hexagonal prism**: circumradius 2.6 (vertices
  `(±1.3, ±2.2516)` and `(±2.6, 0)`), **26 units tall** from y = 0, with
  a bevelled cap - a second hexagon of radius 2.34 at y = 26.39 - and
  16 quads: 2 top cap, 6 bevel, 2 bottom cap, 6 sides.  Twelve of these
  on a radius-20 ring: aap's "12 rods arranged around the orbs", hour
  markers and all.
* **the cube** is a cube, half-extent **2.64**, six quads, unit axis
  normals, and its UVs run 0..1.5 in a 2×2-ish atlas (the TEXCBUMP
  layout).

`tools/extract-res.py --tables` now also writes `res/MENUGEOM.inc` with
both meshes and the cube placement table, so the repo still ships no
Sony data.

---

## 7. The glass: `0x22CFA8` + `0x22C888` + `0x22C4E0`

### 7.1 `0x22CFA8(outX, outY, outZ, dst, scene)` - transform

```
m = MulMatrix(scene->camera(+0x64), scene->world(+0x20))
p = ApplyMatrix(m, 0x27F860); p = ApplyMatrix(scene->viewscreen(+0x60), p)
p = ScaleVector(p, 1/p.w)
*outX = p.x - 2048; *outY = p.y - 2048; *outZ = p.z
if(dst) for each face f:
    dst[f].normal(+0x140) = ApplyMatrix(m, model.norm[f])   /* w = 0 */
    for k = 0..3:
        v = model.vert[f][k] * (scene->f68, scene->f6C, scene->f70), w = 1
        dst[f].v[k].cam (+0x00) = ApplyMatrix(m, v)
        dst[f].v[k].proj(+0x20) = ApplyMatrix(viewscreen, cam) / w
        dst[f].v[k].q   (+0x40) = 1/w
        dst[f].v[k].fix (+0x30) = FTOI4(proj)
        dst[f].v[k].uv  (+0x10) = (model.uv[k].x, model.uv[k].y * scene->f6C)
    e2 = v[2].proj - v[0].proj ; e1 = v[1].proj - v[0].proj
    dst[f].cull(+0x150) = (e2.x*e1.y - e2.y*e1.x) > 0
```

per-vertex stride 80, per-face record 352.  The ROM keeps the bank at
**`0x3529D0`** and a second at **`0x3555D0` = `0x3529D0 + 32*352`**,
which is why the two look like separate arrays: they are one array, and
the second bank holds the split copy of the front rod.

### 7.2 `0x22C888` - the Fresnel term

```
n = Normalize(face.v[0].cam)                    /* 0x2677E0 */
d = InnerProduct(n, face.normal)                /* 0x267818 */
f12 = 1.0 - |d|
0x22C4E0(args, f12)
```

so 0 head-on and 1 edge-on - a plain Fresnel rim term.  **This is the
whole lighting model**; Module U calls no `sceVu0*Light*` function at
all.

### 7.3 `0x22C4E0` - the emit

```
bright = scene->size(+0x90) * 10.0 * f12^4
if(f12 > 0.9)  bright = (int)(bright * (1 - mdCos((1-f12)*32768/0.1)) * 0.5)
PRIM = (f12 > 0.99) ? 404 : 276           /* TRISTRIP|TME|FST, +AA1 */
CLAMP_1 = 0x01000000                       /* WMS = WMT = REPEAT */
RGBAQ = clamp(bright + scene->colour[i] + extra, 255), A = 0x80, Q = 0
for k = 0..3:
    x = (v[k].proj.x - 2048 - outX)*0.95 + outX - face.normal.x*1000*v[k].q*scene->f B8
    y = (v[k].proj.y - 2048 - outY)*0.95 + outY - face.normal.y* 500*v[k].q*scene->fB8
    UV  = FTOI4( max(w/2 + x + 1024, 1024),  h/2 + y + 256 - field*0.5 )
    XYZ = v[k].fix
```

Three things worth keeping:

* **the UV is the vertex's own SCREEN position**, so the surface samples
  the copy of the frame taken before the object list ran (work buffer 3),
  displaced by the camera-space face normal over w.  That is the "bumpmap
  effect" aap sees on the cubes: a screen-space fake refraction, not a
  normal map.  The `TEXCBUMP` pass (§7.4) is on top of it.
* **the two biases are one wrap period each** - 1024 = 2^TW for U, 256 =
  2^TH for V - and the `CLAMP_1 = 0x01000000` the ROM re-pushes with
  *every primitive* (`WMS = WMT = REPEAT`, the `MINV` bit it also sets is
  dead in REPEAT mode) is what makes them cancel.  This is load-bearing:
  `sceGsSetDefTexEnv` left CLAMP/CLAMP behind, under which every V ≥ 256
  clamps to one row and the whole surface comes out flat.  The port hit
  exactly that and the meshes rendered black until the CLAMP_1 override
  went in.
* the GIFtag template is **`0x27F890`**: REGLIST, NREG **12**,
  `{PRIM, CLAMP_1, CLAMP_1, RGBAQ, (UV, XYZ2) × 4}` - six qwords, which
  is exactly what `0x22C4E0` writes.  `0x27F8B0` (NREG 14,
  `{PRIM, CLAMP_1, ST, RGBAQ, XYZ2, ...}`) is the TEXCBUMP pass's.

### 7.4 `0x22D920`'s five passes

For `f12 <= 0` (the arm at `0x22E0EC`, i.e. all twelve rods in practice):

| # | target / texture | faces | via |
|---|---|---|---|
| 1 | FRAME wb4, TEX wb3 (`0x22BFD0(1,0,1)`) | front | `0x22C888` |
| 2 | FRAME wb4, TEX **TEXCBUMP** (`0x22AB90(2,1,2)`) | front | `0x22C920`, phase `slot*0.1 + i * *(gp-32032)` |
| 3 | same, `0x22A0C0(0,2)` | front | `0x22C920`, offset by `scene->+0xB0` |
| 4 | FRAME screen, TEX wb3 (`0x22C020(1,0,1)`) | back | `0x22C888`, extra 0 |
| 5 | FRAME wb3, TEX wb4 (`0x22BFD0(0,1,1)`) | back | `0x22C888`, extra **255** |

and `0x2267E8` then clears wb4, re-runs every mesh record through
`0x22E428`, and adds wb4 back over the screen at alpha 30, twice
(`docs/menu-backdrop.md` §8 - **not inert on this screen**).

---

## 8. vucode_2 is uploaded and never used  [ok]

`docs/menu-scene.md` §7.1/§10.9 assume the fly-in objects are "the only
part of the scene that needs VU1 (`vucode_2`)".  They do not.

* The chain at **`0x268860`** is `DMAcnt QWC=2` (STCYCL/STMASK/STMOD/
  BASE/OFFSET) then `DMAcnt QWC=0x2D` carrying `MPG NUM=0x59 addr=0`
  - 89 instructions = **712 bytes at `0x2688A8`**, byte-identical to the
  repo's `vucode_2`.  It is kicked once per module entry by
  `0x21CE60 → 0x22EE88 → 0x22EE00`, the opening's exact `sendDma` idiom.
* **Nothing ever runs it.**  A whole-image scan finds no `MSCAL`/`MSCNT`
  vifcode constant anywhere in Module U, and every one of the 13 callers
  of the packet kick `0x2294B8` builds its packet through `0x2293E0`,
  which writes a **`0x50000000` DIRECT** vifcode into a double-buffered
  scratchpad packet at `0x70000000`/`0x70002000` and patches its
  immediate at kick time.  DIRECT is PATH2 - straight to the GIF, VU1
  bypassed.
* So the upload is defensive housekeeping (ThreadU, ThreadV and the
  opening share VU1), not a renderer.  Module U's entire 3D output is
  EE-built GIF packets.

`vucode_2.vsm` (aap's 2023 reconstruction, already tracked in the repo)
was re-verified with the towerchain workflow anyway: a minimal
`.dsm` (`DMAcnt * / MPG 0,* / .include "vucode_2.vsm" / .EndMPG /
.EndDmaData / DMAend`) assembled with `ee-dvp-as` gives a 752-byte
`.vutext` whose bytes 16..727 are **byte-identical** to the ROM's blob at
`0x2688A8`.  No `.dsm` is added to the port, because nothing would call
it.

---

## 9. Corrections to the docs

1. **`docs/menu-scene.md` §10.9 and §7.1** - the carousel objects do not
   need VU1; `vucode_2` is dead on this screen (§8).
2. **`docs/menu-scene.md` §8** - "which function issues the tower/orb
   draw calls" is closed for the mesh half: `0x22D920` (rods, via
   `0x2266E0`) and `0x22D2E8`/`0x22D798` (cubes, via `0x226D00`), both
   over `0x22CFA8` + `0x22C888` + `0x22C4E0`.  The two "reached only by
   indirect call" candidates `0x22C0D0`/`0x22E5A8` are still unresolved.
3. **`docs/menu-backdrop.md` §12** - "who opens `0x27F190`" is closed:
   `0x2291E8` from `0x2272B8` (enter System Configuration) and
   `0x229230` from `0x227478` (its leave path), one call site each in
   the whole image.
4. **`docs/menu-backdrop.md` §12** - "`0x22C228`/`0x22BFD0`/`0x22C088`,
   callers not chased / which screen" is closed: the cube stage
   `0x226D00`, i.e. System Configuration (§5).
5. **`docs/menu-text.md` §6.3** - add the `scene+0x90` size (200 front /
   160 others) and note that `f12 = -1` sends every non-front object
   down `0x22D920`'s second arm.
6. **`docs/menu-draw.md` §5.5's `0x22AB90` CLAMP note** - the mesh path
   overrides CLAMP_1 *per primitive*, from inside `0x22C4E0`, not from
   the texture bind (§7.3).

### 9.1 The one thing still unexplained

`0x229358` draws the tunnel whenever `getFadeMode() == 0`, on **every**
screen - the call from `0x21D0A0` is unconditional, and a retail GS dump
of the main menu really does contain all sixteen bright ribbons
(`docs/menu-backdrop.md` §10.1).  Yet the retail main menu measures
`(0,0,0)` and aap confirms the tunnel is System-Configuration-only.
Nothing in the ROM gates the mesh.  The port therefore keys it on the
ROM's own per-screen signal - the `0x27F190` timer of §1 - and
`menuback.c`'s `MenuBackdropVisible()` says so in as many words.  The
mechanism that blacks it out on the real main menu is still open; the
most informative next measurement is still GS local memory at page 210
mid-frame on a console.

---

## 10. What the port does

New file **`osdbits/menuconfig.c`**, plus hooks.  Everything is behind
the same timers as the ROM, so the main menu is bit-for-bit what it was.

### 10.1 Ported

* `MenuEnterConfig()` (**0x227268**) - the Anim's duration, `timerOpen`,
  the backdrop fade opener, the carousel opener, the click message.
  Callable at any frame; `MenuSelectItem(1)` (the pad layer's hook in
  `menu.c`) now calls it.
* `MenuLeaveConfig()` - the closing arm of **0x227390**.
* `MenuConfigStep()` (**0x227DE8**), `MenuConfigOpen()` (**0x227FC0**),
  `MenuConfigAlpha()` (**0x226A60**).
* the carousel: the 12-slot ring, `0x225AD0`, `0x225BF8` (with
  `0x225628`'s clock easing), `0x225F80`, `0x226028`, and `0x2268F0`'s
  `progress > 0.05` gate.
* the mesh renderer: `0x22CFA8`'s transform and winding test,
  `0x22C888`'s Fresnel, `0x22C4E0`'s brightness, rim rolloff, PRIM
  selection, CLAMP_1 override, 0.95 shrink, normal-scaled refraction
  offset and screen-space UVs - all arithmetically verbatim.
* the five cubes: `0x226D00`'s placement, spin and scale ramp, over the
  `0x27F090` table and the `0x27EFB0` mesh.
* `menuback.c`: `MenuBackFadeOpen/Close` (**0x2291E8**/**0x229230**),
  `MenuBackdropVisible()` (the §9.1 gate), `MenuBackBindScreenCopy()`
  (**0x22A290(0)**, work buffer 3 = the port's `extraBuf1`).
* `menutext.c`: the five item strings and their colours; the main menu's
  labels and pad handler now yield to the config screen the way
  `0x227FC0` makes them.

### 10.2 Approximated or stubbed - and where

| what | real | state |
|---|---|---|
| the five draw passes of `0x22D920` | §7.4 | collapsed to **two** untextured/refraction passes (back faces, then front) straight onto the screen.  The TEXCBUMP passes (`0x22C920`) and the wb3/wb4 ping-pong are **not ported**. |
| `0x2267E8`'s two-pass bloom for mesh records | `docs/menu-backdrop.md` §8 | **not ported** (it was already inert with orbs only; it is not inert here) |
| `0x22D920`'s `f12 > 0` arm (the front rod splitting in two along Y) | 0x22D920 head | **not ported**; `MenuConfigDrawMesh` always takes the one-piece arm |
| the cubes' second pass + wb3 blur + `0x22C2A0` composite | `0x226D00`'s tail | **not ported**; the port draws the first pass only |
| the ring's colour cyclers `0x225528`/`0x2255A8` | `0x225878` | **not ported**; the four `.data` tables' idle values are used directly |
| the cube timer's opening cue | `0x2273D0`, at Anim count == 80 | opened by `MenuEnterConfig` instead, so the cubes start growing at frame 0 of the entry rather than frame 80 |
| the item **labels'** positions | each item's widget, `0x21DF28`+ | **not the ROM's**: hung off each cube's projected position |
| the config screen's own confirm button | `items[cursor].fn(+0x14)` | **not wired** - each item is a whole sub-screen |
| `0x225628`'s tilt/spin easing rate | `*(gp-32168)` | hard-coded 0.1 (the constant was not read back out) **[tnt]** |

### 10.3 Files touched (merge awareness)

| file | what |
|---|---|
| `osdbits/menuconfig.c` | **new** |
| `osdbits/res/MENUGEOM.inc` | **new**, generated, gitignored with the rest of `res/` |
| `osdbits/tools/extract-res.py` | `--tables` also writes `MENUGEOM.inc` (`write_menu_geometry`) |
| `osdbits/Makefile` | `menuconfig.o` |
| `osdbits/inc.h` | `SceneRec` moved here and extended; `SceneAddMesh`; the `MenuClock*` and `mat*`/`md*` exports; the `menuconfig.c` and four new `menuback.c` prototypes |
| `osdbits/menu.c` | clock accessors renamed `MenuClock*`; `mat*`/`md*`/`menuCamera`/`menuViewScreen`/`mdTop` un-`static`ed; `SceneRec` removed (now in `inc.h`), `MAXRECS` 16 → 32; `SceneAddMesh`; `SceneWalk`'s mesh branch; `MenuFrame` gains the emitter gate, `MenuConfigCubes`, `MenuConfigStep`, `MenuConfigCarousel` and the new backdrop gate; `InitMenuScene` calls `InitMenuConfig`; `DoMenuScene` gains argv[12]/[13]; **`MenuSelectItem` filled in** |
| `osdbits/menuback.c` | `MenuBackFadeOpen/Close`, `MenuBackdropVisible`, `MenuBackBindScreenCopy`; the screen-copy bind uses REPEAT |
| `osdbits/menutext.c` | the config item list, `DrawConfigMenu`, `ConfigMenuInput`; `MainMenuInput`/`DrawMainMenu` yield to `MenuConfigOpen()`; argv[15] |

Rebased onto **9c53e9d** (the pad commit), so `pad.c` and its hooks are
untouched and `MenuSelectItem(1) -> MenuEnterConfig()` is the merge the
hook's own comment asked for.

### 10.4 New argv

`main.elf menu hh mm ss framelimit fromOpening fadeAlpha debugFrame
cursor notext textDump backPhase back cfgEnter cfgLeave meshTex
cfgCursor` - `cfgEnter`/`cfgLeave` are frame numbers (0 = never) for
headless runs with no pad; `meshTex` 0 draws the meshes untextured
(geometry only), 1 is the ROM's screen-space refraction; `cfgCursor`
picks the highlighted config item.

---

## 11. Verification

Headless PCSX2 (Xvnc :99, software renderer), `DumpFrameAscii` at 8×8
blocks.  Full logs in `logs/`.

**Main menu unchanged** - `menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1 0`,
frame 60: black everywhere but the orb cluster, identical to the
pre-change build.

```
|                               .--=:                                            |
|                            .-###*#-                                            |
|                           .=##*-:.                                             |
|                           :#%-.                                                |
```

**Config mode**, `menu 12 34 56 0 1 128 145 0 0 0 10 0 1 0 0 2` (entered
at frame 1, meshes untextured so the geometry reads clearly), frame 145:
the TEXCKABE tunnel fills the frame, twelve rods stand in a ring around
the orbs, the five cubes are the bright cluster left of centre, and the
five item labels are the `****` band on the right.

```
|:::..==-----..........::-::.....+++=....::---:.........::::::::::::::-:::-------|
|.........::::---:::...  .:---.  *##=  ++---......::-----::::::::::::::----------|
|.................  .:::.  @@@@.********** *******+*************:::::::::--------|
|.:::::::::::::.......@@@+.++@@-**********-*********************::::::::::-------|
|=-----------++=======@@++%@@@@--:-----------%@-----------------:-:---:----------|
|+=-------------------.@@=@@++@--------------%@----------------------------------|
|.....................@@@*%@@+=-----------------:----------.-----:-::::::::------|
|:......-++++++-----.....***==..:+++-..#**+=....-+++++===-::::::::::::::::-------|
|:...:*++*+-----:.......*#*==-..-#+++..=##+++......:=+++-:::::::::::::---:-------|
```

With `meshTex 1` (the ROM's refraction) the same frame shows the rods
sampling the tunnel behind them, dimmer and modulated - the glass look.

**Geometry cross-check** - the per-mesh diagnostic prints each object's
projected origin at the debug frame.  At 12:34:56, frame 145, the twelve
rods land on an ellipse around the orb ring:

```
mesh 16 faces at 37.1 0.5 ... 25.9 -21.0 ... 26.1 22.0 ... -5.2 -37.7
        ... -50.3 -45.3 ... -98.6 -40.9 ... -136.5 -24.6 ... -151.2 -0.6
mesh 6  faces at -97.0 -16.0, -135.4 -6.7, -94.7 1.8, -133.1 11.9, -96.6 20.6
```

which is the `0x27F090` table's zig-zag column for the cubes and a
radius-20 ring for the rods, both where the ASCII map puts them.

**The back path** - `... 220 ... 1 140 ...` (enter at 1, leave at 140,
dump at 220): tunnel, rods, cubes and labels all gone, black backdrop and
the orb cluster back, i.e. `MenuLeaveConfig()` returns the screen to the
main menu's look.

**vucode_2** - `ee-dvp-as` round trip byte-identical (§8).

**Build** - clean, no warnings, `ee-gcc 2.9-ee-991111 -O2`, freesce +
sce_24 as before.

---

# The System Configuration screen's 2D layout, decoded and ported

Written to fold into `docs/menu-config.md` as a replacement for its §3 and
for the "item **labels'** positions" and "the config screen's own confirm
button" rows of §10.2; §5 below also corrects `docs/menu-text.md` §4.1.

Method as `docs/menu-config.md`: `objdump -D -b binary -m mips:5900 -EL
--adjust-vma=0x200000` of `/u/aap/src/osdsys/expanded.bin`, every function
read to its own `jr ra`, static data read straight out of the image,
checked against PCSX2 readbacks of the port.  Everything is **[ok]**
unless marked.

---

## 1. The verdict: three rows, one item at a time

`docs/menu-config.md` §3 names `0x227D08` "the screen's draw slot".  It is
not.  **`0x227D08` draws nothing** - it is the focus notify plus the
dispatch to a pad handler:

```
0x227D08:
    hdr = 0x27BE28                                  /* s0 = 0x27BE44, s0-28 */
    if(!timerIsState(0x27BE44, 2) || !timerIsState(0x27EC40, 0)) {
        if(*(gp-30404)) { hdr->items[hdr->cursor].fn28(item, 0);
                          *(gp-30404) = 0; }
        return;
    }
    if(!*(gp-30404)) { hdr->items[hdr->cursor].fn28(item, 1);
                       *(gp-30404) = 1; }
    if(hdr->mode == 0) tail 0x2279B8                 /* the item-list pad   */
    if(hdr->mode == 1) tail 0x227BE8                 /* the value-list pad  */
```

The drawer is **`0x227560`**, the third of the four calls `0x227DE8`
makes: `timerStep(0x27BE44)`, `0x227390` (the state machine),
**`0x227560`**, tail `0x227D08`.

And what it lays down is **three fixed rows**, all centred on x = 430 like
the main menu's:

| row | y (NTSC) | y (PAL) | colour | what |
|---|---|---|---|---|
| title | **88** | 101 | `0x27B860` = {110,110,0,128} | `osdGetString(hdr->title)` = 91 "System Configuration" |
| label | **112** | 128 | `0x27B830` = {30,110,156,128} | the item's own string |
| value | **130** | 149 | `0x27B850` = {96,96,96,128} | the item's widget (§4) |

Three rows for five items, because **the screen shows exactly one item at
a time**.  Each item's alpha is

```
a = (*(int*)(0x27F090 + i*48 + 0x24) * pageAlpha) >> 7
```

and `0x226BB8` (the cube stage's first half, `0x226FA8 -> 0x226CF8`)
ramps that field **+8 a frame toward 128 for the item under the cursor
and -8 toward 0 for the other four**.  It lives in the cube placement
table because it is the same number that drives each cube's colour ease:
the label is a cube's *caption*, not a row in a list.

That is exactly what the port had wrong.  The stopgap drew all five
labels at full alpha, hung off `MenuConfigItemPos()`'s projected cube
positions, so they piled up wherever the cubes clustered - aap's "the
text is all on top of each other".

### 1.1 The y's

```
0x227560:
    y = 0x204350() == 1 ? 101 : 88               ; 0x204350 = IsPAL
    labelY = (int)((double)y + (IsPAL ? *(double*)0x2A4B40 : 24.0))
    valueY = (int)((double)y + (IsPAL ? *(double*)0x2A4B48 : 42.0))
```

`0x2A4B40` = **27.6** and `0x2A4B48` = **48.3**, i.e. 24 and 42 times the
1.15 base scale `do_load_font` hands `0x2080D0` on PAL (`docs/menu-text.md`
§2.1), and 101 ≈ 88 × 1.15.  The ROM adds them as doubles and truncates,
so the port's `(int)` casts are load-bearing.

---

## 2. `0x227560` in full

```
alpha = 0x2271B8()                                  /* the page alpha, §3 */
if(timerIsState(0x27BE44, 0)) return                /* screen closed      */
if(timerIsState(0x27EC40, 2)) return                /* a sub-screen is up */
0x22A3B8(0x1F0A10, *(0x1F0C40), 0, *(0x27B448))     /* aim at the visible buffer */
0x22A0C0(1, 2)                                      /* normal blend, ZTST GEQUAL */
0x207F68(1.0)                                       /* scale 1 for the whole page */
hdr = 0x27BE28 ; x = 430
0x21DC88(430, titleY, 0x27B860, alpha, osdGetString(hdr->title))
0x228708()                                          /* hdr->+0x0C = widest label, §5 */
markW = 0x209998(gp-30416)                          /* "\7o020", the page marker  */
gap   = 0x209998(0x2A79A8)                          /* " "                        */
if(430 + markW + gap + hdr->maxw/2 >= screenW - 24)
        x -= 430 + markW + gap + hdr->maxw/2 + 24 - screenW
if(hdr->mode != 1 && timerIsState(0x27BE44, 2) && timerIsState(0x27EC40, 0)) {
        k = (IsPAL() ? 50 : 60) * 31400 / 60
        p = hdr->+0x34 * 31400 / k
        0x21DC28(x + hdr->maxw/2 + gap, labelY, 0x27B850,
                 |(int)(128.0 * sinf(p / *(gp-32140)))|, gp-30416)
}
for(i = 0; i < hdr->count; i++) {
        a = (*(int*)(0x27F090 + i*48 + 0x24) * alpha) >> 7
        s = osdGetString(hdr->items[i].strid)
        ix = 430
        if(430 + markW + gap + 0x209998(s)/2 >= screenW - 24)
                ix -= 430 + markW + gap + 0x209998(s)/2 + 24 - screenW
        if(a == 0) continue
        if(hdr->mode == 1) {
                0x21DC88(ix, labelY, 0x27B850, a, s)
                (i == hdr->cursor ? items[i].fn1C : items[i].fn18)
                        (&items[i], 430, valueY, a)
        } else {
                0x21DC88(ix, labelY, 0x27B830, a, s)
                items[i].fn18(&items[i], 430, valueY, a)
        }
}
```

Three details worth keeping:

* **only the label's column is clamped.**  The title and the value row
  keep the literal 430; the marker's column is clamped off the header's
  *widest* label and the label's off *this item's*, so the two clamps do
  not agree when the widest item is not the selected one.
* **the marker.**  `gp-30416` = `0x2A79A0` holds `"\7o020"` - escape `'o'`
  (`0x2094AC`, 5 bytes) emits glyph **20** of the kind-2 table
  (`0x271460`, 35 entries, the FNTEXOSD page).  `0x2A79A8` holds a single
  space, whose measured width is the gap between label and marker.  The
  marker's alpha is `|128 sin(phase/10000)|` where `phase` is the
  header's **+0x34**, a sawtooth `0x227390`'s tail steps by **310** a
  frame and folds at ±`refreshRate*31400/60` (±31400 NTSC, ~203 frames a
  lap); `0x21EE50` (the items' +0x14) zeroes it on entering an item.
  `*(gp-32140)` = **10000.0**, `0x253C08` = `sinf`.
* `hdr->mode` is +0x18, 0 for the item list and 1 for one item expanded
  into its value list.  Mode 1 dims the label to `0x27B850` and hands
  the cursor item's row to the **+0x1C** widget instead of +0x18.

---

## 3. `0x2271B8` - the page alpha

```
c = timerCount(0x27BE44)
v = clamp(c - (*(gp-30400) + *(gp-30380)), 0, *(gp-30396))    /* dur40+dur80, dur10 */
a = (v << 7) / *(gp-30396)
return a * clamp(*(gp-30396) - timerCount(0x27EC40), 0, *(gp-30396)) / *(gp-30396)
```

i.e. `MenuConfigAlpha()` as already ported (the port's extra `fadeAlpha`
factor is not the ROM's, and the `0x27EC40` factor has no counterpart).
Nothing is drawn for the first **120** frames of the entry; the whole
page fades up over the last **10**.

---

## 4. The item records and their widgets

`0x27BD10 + n*56`, with the fields the draw path touches:

| n | +0x00 | +0x04 | +0x08 | +0x0C | +0x10 values | +0x14 enter | **+0x18 draw** | +0x1C mode 1 | +0x28 focus |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 106 `Clock Adjustment` | 6 | 0 | 0 | 0 | 0x21DF28 | **0x21E350** | 0x21EA20 | 0x21EB80 |
| 1 | 107 `Screen Size` | 3 | 0 | 0 | 0x27BC20 | 0x21EE50 | **0x21EE78** | 0x21F080 | 0x21F160 |
| 2 | 111 `\7r0.90DIGITAL OUT (OPTICAL)\7r0.00` | 2 | 0 | 1 | 0x27BCB0 | 0x21EE50 | **0x21EE78** | 0x21F080 | 0x21F160 |
| 3 | 114 `Component Video Out` | 2 | 0 | 2 | 0x27BBC0 | 0x21EE50 | **0x21EE78** | 0x21F080 | 0x21F160 |
| 4 | 117 `Language` | 0 | 0 | 3 | **0** | 0x21EE50 | **0x21EE78** | 0x21F168 | 0x21F160 |

+0x04 is the sub-entry count (six clock *fields* for item 0), +0x08 the
current index, +0x0C a **setting id**: `0x21EDB8` re-syncs +0x08 from
`*(int*)0x22B0E8(+0x0C)` every draw, and `0x22B0E8(n)` is just
`0x352880 + n*4`, the settings array in BSS.

The value lists are 48-byte records of which the draw path reads +0x00
(the setting value) and +0x04 (its string id):

| list | entries |
|---|---|
| `0x27BC20` Screen Size | {0, 108 `4:3`}, {1, 109 `Full`}, {2, 110 `16:9`} |
| `0x27BCB0` Digital Out | {0, 112 `On`}, {1, 113 `Off`} |
| `0x27BBC0` Component | {1, 116 `Y Cb/Pb Cr/Pr`}, {0, 115 `RGB`} - stored in that order, RGB second |

**Language's list pointer is 0 in `.data` and nothing in Module U fills
it in** (`0x21F168`, its +0x1C widget, reads the same field), so its
value row has no source inside this module.  [ok]

### 4.1 `0x21EE78` - the generic value row

```
0x21EDB8(item)
tail 0x21DC88(x, y, 0x27B850, alpha,
              osdGetString(item->values[item->+0x08].strid))
```

### 4.2 `0x21E350 -> 0x21DFF8` - the clock row

`0x21E350` is `0x21DDC0(item); 0x21DFF8(item, x, y, alpha, edit = 0)`.
(`0x21E3B0` is the same with `edit = 1`, which splits the colours into
`0x27B830` for the field being edited and `0x27B840` for the rest - that
is the Clock Adjustment *editor*, not this screen.  With `edit = 0`
every field draws in `0x27B850`.)

```
0x21DFF8:
    x -= 0x209998(0x203968() == 1 ? 0x2A47C8 : 0x2A47F0) / 2
    for(n = 0; n < item->+0x04; n++) {
        kind = *(int*)(0x27B870 + n*12)
        0x209640("\7p@0")                        /* fixed width on '0' */
        sprintf(buf, fmt[kind], *(int*)0x22B0E8(kind))
        0x21DC28(x, y, col, alpha, buf) ; x += 0x209998(buf)
        0x209640("\7p00")                        /* fixed width off */
        separator[n]
    }
```

* the centring template is `0x2A47F0` `"0000/00/00 00:00:00"`, or
  `0x2A47C8` `"0000/00/00 00:00:00 \7r0.66AM\7r0.00"` when `0x203968()`
  says twelve-hour.
* `0x27B870` is six 12-byte `{kind, min, max}`: `{6,2000,2099}` year,
  `{7,1,12}` month, `{8,1,31}` day, `{9,0,23}` hour, `{10,0,59}` minute,
  `{11,0,59}` second.  The ranges belong to the editor.
* formats (`0x2A4840`'s arms): year `"%04d"` (0x2A7858), month/day/minute/
  second `"%02d"` (0x2A7860), **hour `"%2d"`** (0x2A7868) - space-padded,
  and in twelve-hour mode `h = h%12 ? h%12 : 12`.
* separators (`0x2A4860`'s arms), by field index: 0 and 1 `"/"`, **2 a
  space that `0x21E27C` measures and advances over but never draws**, 3
  and 4 `":"`, 5 the twelve-hour tail `" AM"` / `" PM"` at `\7r0.66`
  (`0x2A4808` / `0x2A4820`), drawn without advancing.
* the `\7p@0` / `\7p00` brackets are a no-op for the Latin face:
  `'0'`..`'9'` are all `{5, 23}` in the `0x26FE60` metrics.

---

## 5. `0x228708` - the header's +0x0C is a WIDTH, not a row count

```
best = 0
for(i = 0; i < 0x27BE28->count; i++)
    best = max(best, 0x209998(osdGetString(items[i].strid)))
0x27BE28->+0x0C = best
```

Hard-coded to the System Configuration header and re-run by `0x227560`
every frame.  **This corrects `docs/menu-text.md` §4.1**, which reads
`0x27BE90`'s +0x0C as "rows 3" - it is the same field, and the 3 in
`.data` is only an initial value.  Nothing in Module U reads the main
menu header's +0x0C at all.

---

## 6. `0x2279B8` - the item-list pad handler

Read for the parts the port can honour:

```
0x1000 (up)    items[cursor].fn28(item,0); if(--cursor < 0) cursor = count-1;
               items[cursor].fn28(item,1); 0x2287A8(20992,1,5)
0x4000 (down)  items[cursor].fn28(item,0); if(++cursor >= count) cursor = 0;
               items[cursor].fn28(item,1); 0x2287A8(20992,1,5)
0x0020         items[cursor].fn14(item); hdr->mode = 1;
               cursor == 0 ? 0x22B108() : 0x22B100(); 0x2287A8(20992,1,4)
0x0080         tail 0x227028
0x0040         tail 0x227338    /* timerClose(0x27BE44) + click 10 - leave */
0x0010         only when cursor == 0: click 4, timerClose(0x27BE44),
               tail 0x223658
```

**The cursor wraps at both ends here**, unlike the main menu's
(`0x228278`, which clamps).  Ported.

---

## 7. Does the main menu's "System Configuration" label morph?

No, and nothing cross-fades:

* `0x228110` (the main menu's labels) bails on `0x227FC0`, which goes
  false the instant `0x27BE44` leaves state 0 - the same frame `0x227268`
  opens it.  The two labels are **cut**, not faded.
* `0x227560` bails while `0x27BE44` *is* state 0, so the two screens
  never draw together for even one frame.
* the page title then fades in **120 frames later** (§3) and at a
  different place: (430, 88) centred, against the main menu's item row at
  (430, 114).  Different y, different colour ({110,110,0} vs
  {30,110,156}), no shared state.

Nothing to port; the port already had this right through
`MenuConfigOpen()`.

---

## 8. What the port now does

One file, `osdbits/menutext.c`.  Functions touched:

| function | state |
|---|---|
| the file header comment | updated (what is and is not ported) |
| `colDim` / `colTitle` | **new** - `0x27B850` / `0x27B860` |
| `MenuTextDump` | diagnostic only: the readback window opens to x 224..640, y 80..152 while the config screen is up so all three rows fit.  The main menu's window is unchanged, so its dumps still compare byte for byte with older builds. |
| `ConfigValue` / `ConfigItem` / `configItems` / `configMenu` | **new** - the `0x27BD10` records, the three value lists and the `0x27BE28` header, with `configCursor` now a `#define` onto `configMenu.cursor` |
| `configItemAlpha[5]` | **new** - `*(0x27F090 + i*48 + 0x24)` |
| `cfgClockField` / `cfgClockSep` / `cfgClockDate` / `cfgFmtNum` | **new** - `0x27B870`, the `0x2A4860` separators, and a small `%0Nd` |
| `DrawItemClock` | **new** - `0x21E350` -> `0x21DFF8`, 24-hour arm |
| `DrawItemValue` | **new** - `0x21EE78` |
| `ConfigMenuWidest` | **new** - `0x228708` |
| `ConfigMenuStepItems` | **new** - `0x226BB8`'s item-alpha share |
| `DrawConfigMenu` | **rewritten** - `0x227560`; the `MenuConfigItemPos()` hack is gone |
| `ConfigMenuInput` | the ROM's wrap added |
| `InitMenuText` | derives `cfgTitleY` / `cfgLabelY` / `cfgValueY`, clears `configItemAlpha`, clamps `configCursor` against `configMenu.count` |
| `MenuTextFrame` | calls `ConfigMenuStepItems()` first (the ROM runs `0x226BB8` a hub slot earlier than either 2D layer, and unconditionally) |

`menuconfig.c` and `menuback.c` are untouched, as asked.
`MenuConfigItemPos()` is now unused but still exported.

### 8.1 Not ported, and where it would attach

| what | real | note |
|---|---|---|
| the page marker and its pulse | `gp-30416` `"\7o020"`, `0x227560`'s middle block, `0x227390`'s +0x34 sawtooth | `\7o` emits a **kind-2 (FNTEXOSD)** glyph and the port uploads only FNTASCII, so there is nothing to draw.  Its measured width is 0 here, which is the only reason the port can leave the `markW` term out of the two right-margin clamps; at screenW 640 neither clamp fires for any of the five labels either way. |
| the mode-1 arm (one item expanded into its value list) | `hdr->+0x18`, `0x21F080` / `0x21F168` / `0x21EA20` | not ported; `DrawConfigMenu` only has the mode-0 arm.  This is where the +0x1C widgets attach. |
| the items' confirm callbacks | +0x14: `0x21DF28` (clock editor) and `0x21EE50` | not wired; `0x2279B8`'s CIRCLE arm would set `hdr->mode = 1` and call these. |
| the focus callbacks | +0x28, latched through `*(gp-30404)` by `0x227D08` | not ported (all five are `0x21EB80`/`0x21F160`, and `0x21F160` is a bare `jr ra`). |
| `0x21EDB8`'s setting sync | `*(int*)0x22B0E8(item->+0x0C)` | no settings in the port; every item keeps its `.data` value index 0, so the value rows read `4:3`, `On`, `Y Cb/Pb Cr/Pr`. |
| the Language value row | item 4's +0x10 is NULL | drawn as nothing.  Whatever fills that pointer is outside Module U. |
| the twelve-hour face and the date | `0x203968()`, `0x22B0E8(6..8)` | the port always draws the 24-hour face, and has no RTC date - `cfgClockDate` is the PS2's own epoch, `2000/01/01`.  Only hh:mm:ss are live (from `MenuClock*`). |
| the `\7r` / `\7p` escapes | `0x209300` | still skipped by length, as before.  Harmless for these five items: `\7p` is a no-op on fixed-width digits, and id 111's `\7r0.90` only means DIGITAL OUT draws at full size instead of 90%. |

### 8.2 For the menuconfig.c merge (do NOT apply here)

Reading `0x226BB8` for the label ramp turned up two things about the
**cubes** that `menuconfig.c` currently approximates:

1. the cube colours are **not** the constant `{128,128,128,128}` the
   `0x27F090` table holds.  `0x226BB8` eases each entry's +0x10 qword
   through `0x22EC60` at rate 7 toward `0x27EC30` for the item under the
   cursor and `0x27EC20` = **{100,100,100,128}** for the others, and
   `0x27EC30` = {128,128,128,128} is itself eased at rate 1 toward
   `0x27EC10` = **{0,150,200,128}**.  So the selected cube drifts blue
   and the rest sit at flat grey.
2. each entry's +0x20 bias is multiplied by `*(gp-32144)` = **0.95**
   every frame.  All five are 0.0 in `.data`, so nothing visible today,
   but `MenuConfigCubes()` treats `menuCubeBias[i]` as a constant.

Also: `0x226BB8`'s loop bound is a literal **5**, not `hdr->count`, and
`configItemAlpha[]` really belongs next to the cube table - the two
should become one array in the merge.

---

## 9. Verification

Headless PCSX2 (Xvnc :99, software renderer), `ee-gcc 2.9-ee-991111 -O2`,
clean build, no warnings.  Logs in `logs/`.  "new" = this build, "ref" =
the committed `896dc63` build at
`/u/aap/src/ps2rev/osdsys/osdbits/main.elf`.

### 9.1 The fix, before and after

Both the known-good line `menu 12 34 56 0 1 128 145 0 0 0 10 0 1 0 1`
(enter at frame 1, whole-frame 8x8 dump at 145), rows 12..19 of 28:

**ref** - the five labels crushed into two 8-px rows:

```
|.................   ...   ====.********** *******+*************:::::::::--------|
|.::::::::.......     ===:.--==-**********-*********************::::::::::-------|
|---------:::--:::::::==:-%@+==--:-----------++-----:-:---------:-:---:----------|
|==::::::::::..........===@@--=--------------++:---------------------------------|
|...............      +++-%@@+=-----------------:----------.-----:-::::::::------|
|................ ..::-:::=%@#%-------------------.........::::::::::::::--------|
```

**new** - the ROM's three rows, title / label / value:

```
|.................   ...   ====.:::-+++++++++++++.+++++++++++++++=+++++++++------|
|.::::::::.......     ===:.:===....:+++++++++++++.+++++++++++++++++++++++++------|
|---------:::--:::::::==:-%@+==....          ++:::::::::----::::::::::::::-------|
|==::::::::::..........===@@#:*        **********::********************::--------|
|...............      +++-%@@#=.       **********-*********************::::------|
|................ ..::-:::=%@#%-. #######*##=###*##.###*##..#-##+#*#*#+#+*##-----|
```

### 9.2 The rows, read as glyphs

`menu 12 34 56 160 1 128 0 0 0 145 10 0 1 0 0` (`textDump` at 145), the
2x2-px band, cursor on item 0.  Trimmed at both ends; the `-`/`:` field
is the TEXCKABE tunnel behind the text.

Title row, y 88 - **System Configuration**:

```
::::::::: ++.   ++. --   .=: .====.  =++=.+=-===:::-=:==::==- ......-++....-+= . -===.. -=-==-..:=++- -= ..-==--- -=.:.-= :=:==...-===:::.=++=..
......... -+=+++=:  =+-  ++  ++..++  :+=: .++-:=+= =+=:=+=:-++ .... =+.........:++:.=+= ++=:-++ .=+-. =+ .++--=++ =+.:.=+.-++=:. =+:.=+-:.:+=:.
.........:=: ..-+++  =+:++:  -++-++.  +=  -+=---++ =+. :+:  ++ .... =+:.... =- =+... ++ ++ . ++ .-+:. =+ -+:.. ++ =+.:.=+.-+-.:: ++-==+-:: +=.:
......... ++=:.:=++   +++-  .+=. -+=  ++  .++: -+- =+. :+:  ++ ......+++:.-++= -++..-++ ++ . ++ .-+:. =+ .++=-=++ =+-.-++.-+-.:::+=.:=+-::.++.:
.........  -++++=:    .+=    :++++=   -++:  =+++-  =+  .+:. += .......:=+++=.....=+++-. =+ ..+= .:+:. =+  ----:++  =++==+ -+::::.-+++-++.:.=++:
```

Label row, y 112 - **Clock Adjustment**:

```
                         +******- =*-         .-*%@%*-:**.-------------.***.--------:+*.:.*+.-----------:::... *+.:::::::::::::::::::::
                        **+   :**:=*-  -+**=  ..-***=:.**.:.++.-------.**-**.---.=**=+*.:.+= =+:-:=+.::+**+.. +**+. +-+*+.=**:::.-**+::.
                       :**        =*- **+:=**:.**+:+*+ **:***:.:::::::=*+ **+:-.**=:+**...** **.-:**..**::** .-**:..**=-**+-**-.**=:+** 
                       :**     :+-=*-.*+   .*+=*- .    +****:........:*******..=*-...+*.. ** **...**. +**+**:..** ..*+..**..-*==**===** 
                        +**+::+**.=*- **= :**-.**-.-** +*. **+ ..... **-...:** :**-.=**.. ** **+.=**.:**..=**..**...*+..**..-*=:**-.=*+ 
                          =****=  =*-  =****.  .+***+  +*.. +*-.....-*+.....+*:..****=*...**  ****+*..-****+.:.+**-.*=..**.:-*-:.+***=. 
```

Value row, y 130 - **2000/01/01 12:34:56** (the clock, `hh:mm:ss` from
argv 12/34/56, date the epoch placeholder of §8.1):

```
    +#+=*#-  .*#++#*.  .*#++#*....*#++#*..-*#%++#::.*#++#*.......+#-.......+#:..*#++#*.......+#-............+#-:::.+#+=*#=:::::.+*+++*+.
   =#+  .##  *#-  -#*  *#-  -#*  *#-  -#*.*%%++#*:.*#-::-#* ...+*##-..... +#*  *#-..-#* ...+*##-..........+*##-:::=#+...## .++ .#+ . *# 
       :*#* .##    ##..## .. ##..## .. ##.%%++#+:-.##:-=:##.:::. =#-.... +#+...##....##..... =#-........:::.=#-:::.. .:*#*.::::::::***+ 
    :***+:  .##    ##..## .. ##..## .. ##.%*+#+:--.##:--:##.----.=#-....+#+....## ...##..... =#-........:::.=#-:::::***+::::::.--...:*#=
   =#*:::::  *#+::*#*  *#+::*#*  *#+::*#*.-+#+-++=:*#+::*#*:====:=#-::.+#+.... *#+::*#* .....=#-........:::.=#-:::=#*:::::..** +##-:=##=
   +******* . -****- .. -****- .. -****- ..==:=++++--****-:----=-=*---.==.......-****-.......=*:.....::::::.=*:::.+******* ::::::+***+.:
```

The title is visibly dimmer than the other two, which is `0x27B860`'s
{110,110,0} (mean 73) against {30,110,156} (99) and {96,96,96} (96).

### 9.3 A different item

Same run with the cursor on item 3.  Only one item is ever up, and both
its rows change together - label **Component Video Out**, value
**Y Cb/Pb Cr/Pr**:

```
                  =**:   +*+  .=**+:  =+-**-:+*+  ++-**+.:+*==**+:-.++=**+.--:+**=:--+-+**-:.+**+:-----.**---.**: =+ ..=**=+*...-**+::::
                  **.        -**-:*** ***:+**-=** ***:-**- -**-:*** ***-=** ***:-**:=**=-**-.-**-.-----:.**..+*+: **..**=:+**..**=:+** 
...
                                  . =#* .. *#-*#%#*=.+*+****- .#+ ..........+#:.##+++**- +#.............-****+*+
                                  .. +** :##=+%%%#+.*#+:--:##:.#++++= .... +#* .#*....## +#:**+:.......=##:.:.=#+ =+=*+.
```

**Caveat, and a real find:** `cfgCursor` is `OsdArgInt(15)`, i.e. gameargs
token **16** - and PCSX2 delivers **at most 16 argv entries**.  A 17-token
`-gameargs` arrives as `argc = 16` with the last token dropped, so
`OsdArgInt(15)` always takes its default and **argv slot 16 is
unreachable**.  This run was therefore made from a build whose only
change was `OsdArgInt(15, 3)`; the delivered build has `OsdArgInt(15, 0)`
back.  (`docs/menu-config.md` §11's `... 0 2` config run has the same
problem - it was really a cursor-0 run.)  Fixing it means freeing a slot
in `main.c`, which is outside this task's file budget.

### 9.4 The main menu is unchanged

* `menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1` (the known-good line), whole
  8x8 frame at 60: **`diff` of new vs ref is empty**.
* `menu 12 34 56 200 1 0 0 0 0 140 10 0 0 0 1` (fade allowed to run, so the
  labels are actually up; 2x2 text band at 140): **`diff` of new vs ref is
  empty**.  Both show the two labels, `Browser` bright and
  `System Configuration` dim:

```
                                                           ********.
                                                           **    +*- ****+  -**+*+  +*. :*+  ** :**+**.  +*+**- -*+**.
                                                           **===**+  **+.  =*+  -** :** ***:-*+ +*+:.+= **-..**==**:
                                                           **    :** **    +*.   **  +*-*-****  .-***** *******==*=
                                                           ********+ **    :**+=***   *** +**-  ***=+** +**++**:=*=
            :-------                    -:                             :--::---.                   .-:: :-
            --.   ::. --   .-. ::::::  :--:. .:::::  :-::::::--.      :--    .--  .:::-:  --::--. .---: :-  .:::::- :-   :- .-:-:  .:::::
            .:--::-:  .-:  --  --. ::   -:  .--  .-- :-. .-:  --      :-         :-:  .-- --.  --  .-.  :- .--  .-- :-   :- .--.   --  :-:
           .-:    :--  :-:--    ::-:-.  -:  :-:::::: :-  .-.  --      :-:     -- :-    -- --   --  .-.  :- .-:   -- :-   :- .-.   .-::::-:
            ---:::--:   :--.  .--:.:-:  --:  ---::-: :-  .-.  --       :---::--. .--:.--: --   --  .-.  :-  ----:-- :--::-- .-.   .--:::--
```

### 9.5 The leave path

`menu 12 34 56 0 1 128 220 0 0 0 10 0 1 140 1` (enter 1, leave 140, dump
220): tunnel, rods, cubes and all three text rows gone, black backdrop
and the orb cluster back - the same end state
`docs/menu-config.md` §11 records.

---

## 10. Deliverables

* `cfgtext.diff` - `diff -ru` of `/u/aap/src/ps2rev/osdsys/osdbits`
  against `cfgtext/osdbits`, excluding `*.o *.elf res gsdump* core* *.png
  *.orig *.ai-reference *.annotated *.map`.  **Only `menutext.c`.**
* `ascii.sh` - the config harness with one change: it kills only the
  `pcsx2-qt` whose command line names our own ELF (a parallel agent had
  one of its own running), and takes `ELF=` from the environment so the
  committed build can be run through the same path.
* `logs/` - every run above.

---

# Three glass problems on the System Configuration screen

Scratch tree `/u/aap/.claude/jobs/58e316f8/tmp/glass/osdbits`, rebased onto
**b5d7163** (the parallel menutext.c commit landed mid-job and was pulled in;
`menutext.c` is byte-identical to the real tree and not in the diff).
Deliverable diff: `glass.diff` (`inc.h`, `menuback.c`, `menuconfig.c`,
`res.c`; +446/-42), `git apply -p1` from the repo root - checked.
`res/TEXCBUMP_EXP.inc` is new but untracked like the rest of `res/`; regenerate
it with the existing `python3 tools/extract-res.py <bios.bin> osdbits/res`
(no change to the tool was needed).  Method as usual: `objdump -D -b binary -m mips:5900 -EL
--adjust-vma=0x200000 /u/aap/src/osdsys/expanded.bin`, every function read to
its `jr ra`, static data read straight out of the image, checked against
headless PCSX2 software-renderer readbacks.

---

## 1. The cubes were too small and too close together

**Root cause: the two meshes do not share a camera, and the port gave both of
them the orb camera.**

`0x22CFA8`'s first act is `MulMatrix(scene->+0x64, scene->+0x20)` - the scene
struct's *own* camera pointer, not a global.  The two scene structs get theirs
from different places:

| | rod `0x27E950` | cube `0x27EFB0` |
|---|---|---|
| `+0x60` view-screen | `0` in `.data`, written every frame | `0x352800` in `.data` |
| `+0x64` camera | `0` in `.data`, written every frame | `0x352840` in `.data` |

* `0x2268F0`'s head is `sw a1,96(v0); sw a0,100(v0)` on `0x27E950` - the frame's
  view-screen and camera matrices, straight off `0x21CF20`'s stack.  So the rods
  really are seen through the orb camera.
* `0x27EFB0` is mentioned **exactly once in the whole image**, at `0x226D78`
  inside `0x226D00`, and that function never writes `+0x60`/`+0x64`.  The only
  writer of either target is the per-screen init `0x228460`:

```
  22847c: addiu a0,a0,10240      ; a0 = 0x352800
  ...     f12 = 512.0, f13/f14 = *(0x27B44C)/*(0x27B450), f15/f16 = 2048.0,
          f17 = f19 = 1.0, f18 = *(gp-32132) = 16777215.0, sp+0 = 65536.0
  2284c0: jal 0x267068           ; sceVu0ViewScreenMatrix
  2284cc: jal 0x267630           ; sceVu0UnitMatrix
  2284d0: addiu a0,a0,10304      ; a0 = 0x352840
```

`0x267068` is given *exactly* the arguments `0x21CFD8` gives the frame's own
view-screen matrix, so `0x352800` and `menuViewScreen` are numerically the same
matrix and the port can keep using `menuViewScreen`.  `0x352840` is the
**identity**, and nothing ever writes it again (a whole-image scan for
`lui 0x35` + `addiu ...,10304` finds the one site above).

So the five cubes are drawn with **no camera at all**: the `0x27F090` table's
positions are already camera space.  The consequences, all three of them
reported by the owner:

* the camera sits at `z = -103`, so running the cubes through it put them at
  `w = 150.5` instead of `w = 47.5` - **3.17x too small**;
* the same factor squeezed the zig-zag column from 118 px wide to 39 px and
  from 117 px tall to 37 px - "too close together";
* the refraction offset is `normal * 1000 (500 in y) * q` with `q = 1/w`, so it
  was **3.17x too weak as well** - the "not refracting much" half of issue 2 is
  the same bug.

Measured, `menu 12 34 56 0 1 128 145 0 1 0 10 0 1 0 1`, the per-object
diagnostic (`mesh 6 faces at X Y ... v0 X Y`):

```
before                                after
-97.9 -16.2  v0 -90 -12               -124.0 -58.3  v0  -93 -43
-136.7  -6.7 v0 -129  -7              -242.5 -27.9  v0 -205 -27
-95.6   1.9  v0  -84  -3              -115.9  -1.3  v0  -79 -15
-134.4 12.0  v0 -118  12              -234.4  30.4  v0 -187  30
-97.5  20.8  v0  -87  25              -121.3  58.3  v0  -86  68
```

which is `512 * x/47.5` and `0.47*512 * y/47.5` for the table's
`(-11.5,-11.5,47.5) … (-11.25,11.5,47.5)`, to the tenth of a pixel.  Half-extent
2.64 now projects to 28.5 x 13.4 px, i.e. a 57 x 27 px cube, against 18 x 8
before.  Frame 145, `notext=1`, 8x8-block max luminance, left half only:

```
before                                     after
|.........:::::::......  .....   :--:  |  |....:#%%%%#%%:::...---::+++++   ---: |
|.............:........    ...         |  |....%%%%%##%%#:.......::++=..        |
|........................         ..:. |  |....%%#-----##*.........            .|
|.................   ...   ====.:::--: |  |....-=------::**.  ...=======..:::--:|
|.::::::::.......     ===:.:===......  |  |.:::::==-------+.....++========..... |
|---------:::--:::::::==:-%@+==....    |  |-------=+=-:::::....::.....-==....   |
|==::::::::::..........===@@#:*        |  |+=:::::::::::.......::::....==       |
|...............      +++-%@@#=.       |  |.:::::.+##%##.......::::.:..+=.      |
|................ ..::-:::=%@#%-.      |  |......#%#%#%###* ..::::::-+@@%-.     |
|:..............:----::....:-+#-.      |  |:...+%%%#---+###*---::....-+##-.     |
|:..........:-==--:.......::...........|  |:...:@%%------:--:.......--..........|
```

(the `@@@` column that stays put in both is the rod ring; the new bright blocks
on the left are the five cubes.)

### Not the cause, checked anyway

* **The timer ramp.** `0x226D00`'s `t = timerCount(0x27EC00)/ *(gp-30400)` is a
  plain linear `count/40` and the timer saturates at its duration, so `t` tops
  out at exactly 1.0.  The port already had this.
* **`scene+0x68/+0x6C/+0x70`.** All three get the same `t + bias` and multiply
  the model vertices once, in `0x22CFA8`'s per-vertex loop
  (`f0 = *(s3+0) * *(scene+0x68)` &c.).  No second scale anywhere.
* **The anim phase.** The port opens the cube timer at entry, the ROM at Anim
  count == `dur80`.  That is timing, not size, as the brief guessed.

### One thing that *is* different, from the menutext agent's handoff

`0x226BB8` (reached as `0x226CF8` from `0x226FA8`, between the timer step and
`0x226D00`) rewrites the table every frame, and the port treated it as static:

* `+0x10` **colour** eases (`0x22EC60`, rate 7) toward `0x27EC20 =
  {100,100,100,128}` for the unselected cubes and toward the tracker `0x27EC30`
  - itself easing at rate 1 toward `0x27EC10 = {0,150,200,128}` - for the
  cursor's.  The `{128,128,128,128}` in `0x27F090` is only where they start.
* `+0x20` **size bias** is `*= *(gp-32144) = 0.95` every frame.  It is a *kick*,
  not a constant: `0x227C20` (the config screen's CIRCLE arm) writes
  `*(gp-32136) = **-0.1**` into the cursor's entry, so the pressed cube shrinks
  10 % and springs back.  The static value is 0.0 and the port's confirm button
  is unwired, so the *resting* size was right; the mechanism was missing.

Both are now ported (`MenuConfigCubeState`, `cfgEase`).  `+0x24` (the label
alpha) is deliberately **not** touched here - menutext.c owns that half of
`0x226BB8` as of b5d7163.  The cursor lives in menutext.c's item header, so
menuconfig.c exposes `MenuConfigSetCursor(int)` and defaults to item 0 until
someone wires it: **one line in `ConfigMenuInput`/`InitMenuText` finishes it**,
and until then the cursor cube tints as if item 0 were selected.

---

## 2. The refraction was too weak

Three separate causes; the first is much the biggest.

### 2a. `q` (see §1) - fixed by the camera

`1000 * q` went from 6.6 px to 21 px horizontally and 3.3 to 10.5 vertically.

### 2b. `ALPHA_1` was the additive blend, and the `AA1` bit was inverted

`0x22C4E0`'s PRIM select, read instruction by instruction:

```
22c5b0: lwc1 $f0,-32072(gp)   ; 0.99
22c5b4: c.lt.s $f0,$f21       ; cc = (0.99 < fres)
22c5bc: bc1f 0x22c5cc         ; cc FALSE -> 22c5cc
22c5c4: b 0x22c5d0
22c5c8: li v0,276             ; (delay) cc TRUE  -> 276 = TRISTRIP|TME|FST
22c5cc: li v0,404             ;         cc FALSE -> 404 = the same + AA1 (0x80)
```

so **AA1 is on for the ordinary face and off for the near-edge-on one**, the
opposite of what the port did.  Left inverted, AA1 flickered on and off per face
as a spinning rod's faces crossed 0.99.

Putting AA1 back where the ROM has it immediately exposed a second bug: the port
pushed `vif1SetAlphaBlend(1, **5**, 128)` = `BlendModes[5]` = `0x48` = `Cs*As +
Cd`, the **additive** blend, where `0x22A0C0(1,1)` picks entry 1 of the jump
table at `0x2A4B80` (`0x22A124`, `li v0,68`) = **`0x44`** = `(Cs-Cd)*As + Cd`.
With PRIM's ABE clear and AA1 almost never set that was dead state; with AA1 on
the GS blends, and `0x48` turned every glass face into an additive wash - the
whole screen went `@@@@` (logs/fix-cfg145.log).  With `0x44` and the emit's
`A = 0x80` the blend is the identity, i.e. an opaque face, which is what it has
to be.  Both are fixed; they cancel visually and leave the antialiased edge the
ROM asks for.

### 2c. The TEXCBUMP emboss - the actual "bumpmap" - was missing

`0x22D2E8` runs **eight** loops over the face bank, five for the front faces and
three for the back:

| # | state | faces | emit |
|---|---|---|---|
| 1 | `0x22BF58(1,0,0)` FRAME wb4, TEX **the screen**; `0x22A0C0(1,1)` | front | `0x22C888` -> `0x22C4E0`, extra 0 |
| 2 | `0x22AB90(2,1,2)` TEXCBUMP; `0x22A0C0(0,2)` = ALPHA `0x48` | front | `0x22C920`, offset `(+0xB0,+0xB4)` = (0.01, 0.01) |
| 3 | `0x22A0C0(2,2)` = ALPHA **`0x42`** | front | `0x22C920`, offset (0, 0) |
| 4 | `0x22BFD0(0,1,1)` FRAME wb3, TEX wb4 | front | `0x22CCE8` |
| 5 | `0x22A0C0(0,3)` | back | `0x22CA68` |
| 6 | `0x22BFD0(1,0,0)` + `0x22C100()` | - | wb3 -> wb4, half-width blit |
| 7 | `0x22BFD0(0,1,1)`; `0x22A0C0(1,1)` | back | `0x22C888` |
| 8 | as 2 and 3 | back | `0x22C920` x2 |

Loops 2/3 and 8 are the bumpmap, and they are a plain **emboss**: the same
64x64 TEXCBUMP page drawn twice over the mesh's own UVs, once ADDITIVE at a
+0.01 UV offset and once SUBTRACTIVE at zero offset.  `0x22C920`'s primitive is
`PRIM = 84` = `TRISTRIP | TME | ABE` with **FST clear** (ST/Q, model UVs, not
screen position), colour `scene+0xA0 = {8,8,8,128}`, `Q = q` per vertex, GIFtag
template `0x27F8B0`.  With TFX MODULATE each pass caps at `8*255/128 = 15`
levels, so the relief is +-15 on top of the refraction - subtle, and exactly
what a "bumpmap-like surface effect" looks like.

Ported as `MeshEmitBumpFace` / `MeshBumpPass`, for the cubes only (the rods'
`0x22D920` arm has the same loops with a per-face phase `f21 + i*(gp-32032)`;
left stubbed as before, and noted in the code).

**TEXCBUMP is TEXC slot 2**: descriptor `0x27F1C0 + 2*12` says `wexp = hexp = 6`
(a real 64x64 page) and the decoder table `0x2A4BA0[2]` is `0x22A720`, the grey
expander (`b | b<<8 | b<<16 | 0x7F000000`).  `tools/extract-res.py` **already
emits `res/TEXCBUMP_EXP.inc`** with the rest of TEXIMAGE (4452 -> 4096 bytes) -
no tool change was needed, only the two lines in `res.c` that every other
resource has.  The decode/upload is `DecodeBump` + `InitTexture`, the bind
`MenuConfigBindBump` (`0x22AB90(2,1,2)` -> `0x22AA88`, TEX1 `0x61`, CLAMP_1 back
to REPEAT/REPEAT, TEXA `{0x7F,1,0x81}`).

### 2d. The refraction SOURCE - deliberately *not* changed, with evidence

The brief asked me to confirm what `0x22A290(0)`'s bind really points at in the
cube path.  It is not `0x22A290` at all: loop 1 uses `0x22BF58(1,0,0)`, whose
first act is `0x22A198(*(0x1F0C40))` - **TEX0 = the live screen** (PSMCT24), by
then carrying the tunnel, the composite, the orbs, the rods and `0x2283D0`'s
five zoom-blur round trips.  Its FRAME, though, is work buffer 4, and
`0x226D00`'s tail ends with `0x22C020(0,0,0)` + `0x22C190(0)` - an **opaque**
full-screen blit of work buffer **3** back over the screen.  Neither work buffer
is ported, and both of them still hold `0x21D0A0`'s copy of the *bright,
un-tinted* tunnel.

So binding the screen in a port that also *draws* onto the screen gets the
texture right and the destination wrong.  Measured both ways, frame 145,
`notext=1`, the cube column:

```
TEX = a fresh copy of the screen        TEX = extraBuf1 (work buffer 3)
|....-----:::-::...........   |         |....%%%%%##%%#:.......::++=..|
|....--:.....:::..........    |         |....%%#-----##*.........     |
|......::::-::::: ..:::..=*#@ |         |......#%#%#%###* ..::::::-+@@|
```

The cubes all but vanish against the backdrop.  So the port keeps `extraBuf1`
for both meshes (no code change), and the divergence is now written down in
`MeshDraw`'s header comment instead of being silent.  Porting loops 4-8 and the
`0x22C2A0` composite would make the ROM's own source correct; that is the next
piece of work on this screen, not a bug fix.

`MenuBackCaptureScreen()` was written, measured and then removed - it is not in
the diff.

---

## 3. The flickering "clock background"

### What the measurements say

Headless SW renderer, one frame per run.  Two metrics: the existing 8x8-block
max-luminance map, and (for the investigation only, reverted before the diff) a
16x16-block **mean** luminance map, compared as mean absolute block difference.
Adjacent frames are opposite fields; frames two apart are the same field and
have twice the animation, so `d(N,N+1) > d(N,N+2)` is the signature of
field-alternating content.

| build | d(145,146) | d(146,147) | d(147,148) | d(145,147) | d(146,148) | ratio |
|---|---|---|---|---|---|---|
| HEAD | 0.491 | 0.593 | 0.471 | 0.445 | 0.454 | 1.22 |
| all fixes, cubes still on the frame's half pixel | 0.661 | 0.677 | 0.652 | 0.416 | 0.396 | **1.61** |
| all fixes + cube half-pixel off (delivered) | 0.500 | 0.530 | - | 0.416 | - | 1.24 |

Controls, same metric:

| scene | adjacent | 2 apart | ratio |
|---|---|---|---|
| main menu, orbs + blur only | 0.050 / 0.089 | 0.089 | 0.8 (none) |
| main menu + tunnel + composite forced on (`back=1`) | 0.173 / 0.218 | 0.134 | 1.46 |
| config (HEAD) | 0.491 / 0.593 | 0.445 | 1.22 |

### Bisection

Turning the two config meshes off one at a time (temporary edits, reverted),
8x8 metric:

| | d(145,146) | d(145,147) |
|---|---|---|
| rods + cubes (HEAD) | 6.3 % | 3.8 % |
| rods only | 6.0 % | 3.8 % |
| **cubes only** | **1.7 %** | **1.5 %** |
| meshes untextured (`meshTex=0`) | 5.8 % | 4.0 % |

so the alternation follows the **rods**, and it survives `meshTex=0`, i.e. it is
in the rasterisation, not in the refraction sampling.  The `back=1` control
above adds the tunnel to that list.

### What it is

The interlace half pixel, and everything that carries it in the port carries it
in the ROM too - `0x22A4C8`/`0x22A3B8` take `field` per call site:

* the tunnel: `0x21D0A0`'s head passes `*(0x27B448)`, the real field. Faithful.
* the rods: all three of `0x22D920`'s FRAME pushes (`0x22BFD0(1,0,1)`,
  `0x22C020(1,0,1)`, `0x22BFD0(0,1,1)`) pass `a2 = 1` = the real field.
  Faithful.
* **the cubes: `0x22BF58(1,0,0)` passes 0**, and nothing in `0x226D00` puts it
  back - the whole cube stage, down to the blit that puts its result on the
  screen, runs with **no** half pixel.  The port had them inheriting the
  frame's.  **This is the one real bug in issue 3** and it is fixed
  (`MenuBackMeshHalfOffset`, the same bracket 37efd18 gave the blur).

Its practical importance is that the correct cubes are 3.2x bigger: the middle
row of the table above shows what the size fix alone would have done to the
flicker (1.22 -> 1.61).  With the bracket the ratio is back to HEAD's 1.24 with
three times the cube area on screen.

Also fixed, though too small to measure: `MeshEmitFace` read the **live**
`evenOddField` for `0x22C4E0`'s `- field*0.5` on the V.  `SwapBuffers` flips
that on the swap thread, so two objects of one frame could disagree - the exact
race class of 37efd18.  It now uses menuback.c's per-frame snapshot
(`MenuBackField()`), which is what the ROM's `*(0x27B448)` is.

### What it is *not*

* **The composite / blit chain (candidate b).**  `back=1` on the main menu -
  tunnel, both `FullScreenBlit` copies and the tinted composite, no meshes -
  shows no odd/even asymmetry in the 8x8 metric at all (1.2 / 1.5 / 1.2 %).
  `BackHalfOffset(0)` already brackets every buffer-to-buffer blit; the only
  consumer of `MenuBackBindScreenCopy` is a texture bind, which has no XYOFFSET.
* **Z-state leakage (candidate c).**  `DrawKabe` is the only thing that writes Z
  (`vif1SetZWrite(1)`, ZTST GREATER); the meshes and orbs all run ZWrite off /
  ZTST ALWAYS, and `sceGsSetDefDBuff(..., SCE_GS_CLEAR)` clears Z to 0 each
  frame.  The tunnel's projected z is 43..526 in 1/16 units against the cubes'
  ~22000, so even the ROM's GEQUAL for the bump passes would pass everywhere.
* **AA1.**  Forcing AA1 off entirely gives 0.620 / 0.664 / 0.402 - the same 1.60
  ratio as with it on.  Not a factor either way.
* **Tunnel aliasing (candidate d).**  Present but small and symmetric: the
  `back=1` control's 0.173 / 0.134 is the wobble and the T scroll drifting, plus
  the same half pixel the ROM applies.

### The residue, and the hardware renderer

After the fix the config screen's frame-to-frame difference is 0.50 mean block
luminance against 0.42 for pure animation - a half-pixel-sized residue on the
rods and the tunnel, both of which the ROM draws exactly the same way.  On the
software renderer there is nothing left to fix without diverging from the ROM.

The owner watches in the **hardware** renderer, and I could not reproduce that
here: PCSX2 with `EmuCore/GS/Renderer=12` (OpenGL) never gets far enough on this
Xvnc display to open its log file (no GL/GLX), so every number above is
software-renderer only.  What is left as the standing hypothesis for a
hardware-only flicker, in order:

1. **the deinterlacer.**  Everything above is a genuine, ROM-faithful half-pixel
   field offset on a full-screen high-frequency texture.  A blend/bob
   deinterlacer turns that into visible shimmer where an interlaced CRT would
   not.  Worth asking the owner to try `GS > Deinterlacing = Weave` or
   `Progressive` and report back - that is a one-setting experiment that would
   settle it.
2. **the render-target/texture chain.**  Config mode adds, per frame: 16 tunnel
   ribbons, two full-screen RT->RT copies, a tinted composite, five zoom-blur
   ping-pongs, and then ~19 meshes that sample `extraBuf1` (a render target)
   while drawing to the screen.  PCSX2's HW renderers resolve that with
   heuristics the SW renderer does not need.  The main menu, which the owner does
   *not* report as flickering, has only the five blur ping-pongs.

---

## Remaining deltas on this screen

Unchanged from `docs/menu-config.md` 10.2 unless listed:

* `0x22D2E8` loops 4-8 (`0x22CCE8`/`0x22CA68`, the wb3/wb4 ping-pong,
  `0x22C100`'s half-width blit) and `0x226D00`'s tail (`0x22C088`, the
  `0x22C2A0` work-buffer-3 blur, `0x22C190(0)`'s opaque blit back).  Porting
  these is what would let the refraction sample the screen the way the ROM does
  (§2d), and it is the biggest single remaining piece.
* `0x22D920`'s TEXCBUMP loops for the **rods** (the per-face phase
  `f21 + i * *(gp-32032)`, and the second one's `+ scene->+0xB0 = -0.008`).
* `0x2267E8`'s two-pass additive bloom for the carousel records.
* `0x22D920`'s `f12 > 0` arm (the front rod splitting along Y).
* `0x226BB8`'s cursor: `MenuConfigSetCursor()` is exported and unused; menutext.c
  needs one call for the selected cube to tint.
* `0x227C20`'s CIRCLE arm, which is what would seed the -0.1 size kick the decay
  now models.
* `0x225BF8`'s split rate: the port uses 0.02, `*(gp-32164)` is **0.004**.
  Untouched (it is the front rod's split, which feeds the unported arm above).
* The port pushes `CLAMP_1 = CLAMP/CLAMP` once per frame in `MenuFrame` and the
  glass overrides it to REPEAT per primitive, so an orb that sorts *behind* a rod
  is sampled with REPEAT.  That is **not** a bug: `0x22AB90` -> `0x22AA88` forces
  CLAMP_1 to 0 for the orbs' own binds too (docs/menu-backdrop.md 7 correction
  4), so REPEAT is the ROM's state.  The once-per-frame CLAMP/CLAMP is the
  divergence, and it is on the main menu's bit-stable path - left alone.

---

## Build and verification

`ee-gcc 2.9-ee-991111 -O2`, freesce + sce_24 as before, clean, no warnings.

* **Main menu bit-stable**: `menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1`,
  frame 60, 8x8 map identical to HEAD's - **0/2240 blocks differ**
  (`logs/base-mm60.log` vs `logs/W-mm60.log`).  Everything new is behind the
  cube timer or the `bumpCol != nil` argument, which only the cube path passes.
* **Config, 3D layer**: `menu 12 34 56 0 1 128 145 0 1 0 10 0 1 0 1` - the
  before/after maps in §1.
* **Config, full**: `... 145 0 0 0 10 0 1 0 1` with b5d7163's 2D layer -
  `logs/V-cfg145.log`.
* **Leave path**: `... 220 0 0 0 10 0 1 140 1` - tunnel, rods and cubes gone,
  black backdrop and the orb cluster back (`logs/V-leave220.log`).
* **Flicker**: the five-run frame matrices in §3, plus the two control scenes.

Logs in `logs/`; `cmpmap.py` (8x8 map differ) and `meandiff.py` (mean-block
metric) are the two throwaway comparators.

Cleanup: Xvnc `:98` killed, no pcsx2-qt left running, no cores, `PCSX2.ini`
never touched (the hardware attempt used `-setting` only).
