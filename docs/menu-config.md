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
