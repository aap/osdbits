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

---

# The System Configuration cubes' refraction: the ROM's work-buffer chain, ported

Scratch tree `/u/aap/.claude/jobs/58e316f8/tmp/refract/osdbits`, on top of
**eba5595**.  Deliverable diff: `refract.diff` (`inc.h`, `menuback.c`,
`menuconfig.c`; +566/-137), `git apply -p1` from the repo root — checked.  No
new resource, no `res/` change, `menutext.c` and `menu.c` byte-identical to
HEAD.

Method as usual: `objdump -D -b binary -m mips:5900 -EL --adjust-vma=0x200000
/u/aap/src/osdsys/expanded.bin`, every function read to its own `jr ra`, static
data read straight out of the image, checked against headless PCSX2
software-renderer readbacks.  `gp = 0x2AF070`; addresses are retail image
addresses.  Everything below is **[ok]** unless marked.

---

## 1. The verdict, in one line

The cubes were opaque because the collapsed chain drew them on the screen while
sampling `extraBuf1` — the copy of the frame taken **before** the composite
multiplies it by `{0x37,0x28,0x3C}/128` — so every cube displayed a ~3x
brighter, un-tinted image of the backdrop *onto* the tinted backdrop.  The ROM
has no such problem because its glass never touches the screen: it is built in
the two work buffers, and the one blit that puts it back (`0x22C190(**1**)`) is
an **alpha-masked** composite, not the opaque one the docs recorded.

---

## 2. The hypothesis, measured on HEAD

`menu 12 34 56 0 1 128 145 0 1 0 10 0 1 0 1`, frame 145, `notext = 1`,
temporary RGB readback (removed from the diff).  Cube 1's projected centre is
(78, 84), half-extent 28 x 13 px.  Row y = 84, x stepping 9 px:

```
HEAD   x=      33     42     51     60     69     78     87     96    105
       bkgd  2b2640 34264e | 130f1d ... the cube ... |
       face                  495162 495061 3f4556 484f5f 474e5e 4c5363 3e4554
       bkgd  271f3b 231c34
```

i.e. face `(73,81,98)`, background `(39,31,59)` / `(35,28,52)` right beside it:
**mean luminance 84 against 43, a factor of 2.0**, and remarkably *flat* — the
same three numbers across the whole face.  Cube 0 (the cursor's, tinted toward
`{0,150,200}`) is worse: `(0,117,185)`, `(152,166,202)`, `(0,80,131)` against
`(33,26,49)` / `(31,24,47)`, i.e. **3x to 6x**.

That ratio is the reciprocal of the composite tint: `0x27B4B0` = `{0x37, 0x28,
0x3C}` = x0.43 / x0.31 / x0.47, so un-tinting a pixel multiplies it by 2.3 /
3.2 / 2.1.  **Hypothesis confirmed**, and confirmed as arithmetic rather than
as an impression: glass that shows its background 2-6x brighter than the
background reads as a solid milky block.

---

## 3. The functions the brief asked for

### 3.1 `0x22C190(abe)` — the crux

```
0x22C190(a0):
    rec = 0x27F760 | uncached
    rec.x1 = w<<4 ; rec.y1 = h<<4
    rec.u1 = (w<<4)+8 ; rec.v1 = (h<<4)+8
    0x22A0C0(1, 1)            ; ALPHA_1 = 0x44 = (Cs - Cd)*As + Cd, ZTST ALWAYS
    rec.+0x34 = a0            ; 22c1fc: sw s1,52(s0)
    0x2299C0(rec)
    tail 0x22A0C0(1, 3)
```

The record at `0x27F760` in `.data` is

```
R 128  G 128  B 128  A 128   x0 0 y0 0  u0 8 v0 8   z 0   +0x34 = 0   +0x38 = 1
```

and the record layout is settled by `0x2297E8` / `0x2298A8` (the two halves
`0x2299C0` runs):

| field | +0x00 | +0x04 | +0x08 | +0x0C | +0x10 | +0x14 | +0x18 | +0x1C | +0x20 | +0x24 | +0x28 | +0x2C | +0x30 | **+0x34** | **+0x38** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| | R | G | B | A | x0 | y0 | u0 | v0 | x1 | y1 | u1 | v1 | z | **ABE** | **TME** |

because `0x2297E8` builds `PRIM = 6 | 256 | (rec+0x38)<<4 | (rec+0x34)<<6`, i.e.
`SPRITE | FST | TME | ABE`.  So `0x22C190`'s argument **is the ABE bit**.

**Its callers.**  `0x21D0A0` calls it twice with `a0 = 0` (the two opaque
screen -> work-buffer copies).  `0x226D00`'s tail calls it with `a0 = **1**`:

```
226f44  ld ra,128(sp)
226f48  li a0,1          <-- the argument
...
226f70  j 0x22c190       (tail)
226f74  addiu sp,sp,160
```

With `ABE = 1`, `ALPHA_1 = 0x44` and `TFX = MODULATE` over a PSMCT32 work
buffer with `Af = 0x80`, `As` is the **texture's own stored alpha**, so the
blit is `screen = lerp(screen, wb3, wb3.A/128)`.  That is the difference
between "the cube stage replaces the frame" (which would erase the tunnel, the
orbs and the rods, since work buffer 3 is still the pre-object copy almost
everywhere) and "the cube stage replaces the frame exactly inside the cubes".

**`docs/menu-config.md`'s glass write-up §2d calls this "an *opaque*
full-screen blit of work buffer 3 back over the screen (`0x22C020(0,0,0)` +
`0x22C190(0)`)".  Both the argument and the conclusion are wrong**, and that
single error is what made the collapsed chain look like the only option.

### 3.2 `0x22CCE8` -> `0x22CB58` — the flat pass (`0x22D2E8` pass 4)

`0x22CCE8(face, scene)` is `0x22C888`'s twin: `sceVu0Normalize` of the face's
vertex 0 in camera space, `sceVu0InnerProduct` with the face normal at
`face+0x140`, `f12 = 1 - |d|` — the same Fresnel term — then `0x22CB58(&blk,
f12)` where the block is `{+0x00 scene, +0x04 face}` (note the order is the
mirror of `0x22C888`'s five-word block).

`0x22CB58` emits GIFtag `0x27F880` (REGLIST, NREG 6, `{PRIM, RGBAQ, XYZF2 x4}`
— byte-identical to `0x27F870`) and:

```
f20   = f12 * f12
PRIM  = 196 = TRIANGLE_STRIP | ABE | AA1        ; **TME clear**, IIP clear
RGBAQ = ( min(255, (int)(f20 * scene->+0x80)),
          min(255, (int)(f20 * scene->+0x84)),
          min(255, (int)(f20 * scene->+0x88)), 0x80 ), Q = 0
4 x XYZF2 = face.v[k].fix (+0x30/+0x34/+0x38), F = 0
```

So: untextured, flat, the scene colour scaled by the **square** of the Fresnel
term.  The `0x22BFD0(0,1,1)` that precedes it binds work buffer 4, which the
primitive then does not sample — dead state, like several others on this path.

### 3.3 `0x22CA68` — the black pass (`0x22D2E8` pass 5, `0x22D798`'s second loop)

Takes only the face.  GIFtag `0x27F870`, same NREG 6 layout:

```
PRIM  = 132 = TRIANGLE_STRIP | AA1              ; TME clear, **ABE clear**
RGBAQ = (0, 0, 0, 0x80), Q = 0x03F80000
4 x XYZF2 = (x, y, **z + 1**), F = 0            ; 22cb00: addiu a1,a1,1
```

drawn under `0x22A0C0(x, 3)` = ZTST **GREATER**.  (The `Q` is `lui a2,0x3f8` =
`0x03F80000`, a ROM slip for `1.0f`'s `0x3F800000`; dead, TME is clear.)

Two jobs, one per walk:

* in `0x22D2E8` pass 5 it primes the cube's area of work buffer 3 to **black**,
  so pass 7's refraction is all the glass shows and none of the pre-object copy
  leaks through;
* in `0x22D798` it is the whole point of the second walk — drawn into a work
  buffer 4 that `0x226D00` has just cleared to `{0,0,0,0}`, its `A = 0x80` **is
  the composite's mask**.

### 3.4 `0x22C100` — the half-width merge (`0x22D2E8` pass 6)

Record `0x27F720` = `{0x80,0x80,0x80,0x80}`, `u0/v0 = 8`, **ABE 1**, TME 1;
extents patched to `x1 = (w/2)<<4`, `y1 = h<<4`, `u1 = ((w/2)<<4)+8`,
`v1 = (h<<4)+8` — a **1:1 copy of the left half**, not a stretch
(`22c158: sll v0,v0,0x3` on `w`, i.e. `(w/2)<<4`, and the same value again in
`u1`).  Blend `0x22A0C0(0, 1)` = ALPHA_1 **0x48** = `Cs*As + Cd`, i.e. additive
and gated by the source's alpha.

Half width because the five cubes live at projected x -242..-79 of centre, i.e.
entirely in the left half of a 640-wide frame; the ROM simply does not pay for
the right half.  Its call site is `0x22BFD0(1,0,0)` (FRAME work buffer 4, TEX
work buffer 3), so it exists to show pass 7 — which samples work buffer 4 — the
silhouette passes 4 and 5 have just written into work buffer 3.

### 3.5 `0x22C2A0(n)` — the work-buffer zoom blur

Record `0x27F7E0` = `{0x80,...}`, **ABE 0**, TME 1.  Instruction for
instruction `0x22C3C0` with the same `x = 5108, y = 2388` start and the same
`-32 / -16` per pass; only the two FRAME/TEX pushes differ:

```
for(k = 0; k < n; k++) {
    0x22BFD0(1,0,0)   ; FRAME wb4, TEX wb3, NO half pixel
    0x22A0C0(1,1)
    rec.x1 = 5108-32k ; rec.y1 = 2388-16k ; rec.u1 = (w<<4)+8 ; rec.v1 = ((h-1)<<4)+8
    0x2299C0(rec)
    0x22BFD0(0,1,0)   ; FRAME wb3, TEX wb4
    0x22A0C0(1,1)
    rec.x1 = w<<4 ; rec.y1 = (h-1)<<4 ; rec.u1 = 5108-32k+8 ; rec.v1 = 2388-16k+8
    0x2299C0(rec)
}
0x22A0C0(1, 3)
```

`0x226D00` calls it with `phase < 5 ? 5 - phase : 0` off `*(gp-28844)`, which
idles at 10 — so **n is 0 on a settled screen** and the loop does not run.
(`0x2283D0`'s always-on blur keys on the same ramp the other way, `phase - 5` =
5.)  Ported anyway, for the transition case.

This also closes `docs/menu-backdrop.md` §6's "`0x22C228` is the same loop
against work buffer 3 ... not chased": the loop `0x226D00` uses is **`0x22C2A0`
with record `0x27F7E0`**.  `0x22C228` is a different thing entirely — one flat
sprite from record `0x27F7A0` (`{0,0,0,0}`, ABE 1, **TME 0**) under
`0x22A0C0(4, 1)` = ALPHA_1 `0x68` — and is still without a caller here.

### 3.6 `0x22CD78` — the one loop NOT ported

`0x22D798`'s *first* loop, `0x22CD78(face, scene, aa)`, under
`0x22AB90(**5**, 0, 1)`.  Per vertex:

```
n     = sceVu0Normalize(v.cam)
d     = 2 * sceVu0InnerProduct(n, face.normal)      ; |2d|, sign-folded
r     = 0x267710(n, 0x267050(face.normal, |2d|))    ; reflect
u     = 0x25A368((r.x + 1.0) * 512.0)
v     = 0x25A368((r.y + 1.0) * 256.0)
PRIM  = aa ? 404 : 276 ; here 404 = TRISTRIP|TME|FST|AA1
RGBAQ = scene->+0xC0 = {120,120,120,128} for the cube scene
```

— a **spherical environment map**, and TEXC slot 5 is **TEXCREFA** (the slot
order is the resource table's TEXC run: 0 FLOW, 1 KABE, 2 BUMP, 3 BINV, 4 SMOK,
**5 REFA**, 6 NAVI, 7 BLUR, 8 STSL, 9 MARU — which is what pins slot 1 = KABE
and slot 2 = BUMP, both already known).  The port ships no TEXCREFA (`res.c`
has no entry, `res/` no `.inc`), so it is not ported.

It is also, by the ROM's own ordering, invisible: `0x22CA68` redraws **the same
`cull != 0` faces** immediately afterwards with `PRIM = 132`, whose ABE is
clear, so the black overwrites it wherever the depth test lets the black
through — and the cubes' projected z is ~22000 against the tunnel's 43..526, so
`GREATER` passes everywhere.  Only AA1 edge pixels can survive.  Its colour
`{120,120,120,128}` is real and deliberate, so this may be a genuine ROM
dead-pass rather than a reading error; it is flagged, not resolved.

---

## 4. The chain as ported

`menuback.c` grew the plumbing (`MenuBackBindWork`, `MenuBackBindScreen`,
`MenuBackWorkTarget`, `MenuBackScreenTarget`, `MenuBackWorkAdd`,
`MenuBackWorkHalfAdd`, `MenuBackWorkOver`, `MenuBackWorkBlur`,
`MenuBackPhase`), `menuconfig.c` the two new emitters (`MeshEmitFlatFace`,
`MeshEmitBlackFace`), their two passes (`MeshFlatPass`, `MeshBlackPass`) and
three draw functions (`MeshDrawRod`, `MeshDrawCube`, `MeshDrawCubeMask`)
replacing the single collapsed `MeshDraw`.

`extraBuf1` = the ROM's work buffer 3, `extraBuf2` = work buffer 4.

### 4.1 `0x22D2E8`, per cube — `MeshDrawCube()`

`far` = the `cull == 0` set, `near` = `cull != 0`; the ROM always draws far
first and near last, which is what makes the pair one two-layer piece of glass
instead of two coats of the same one.

| # | ROM | target / texture | faces | emit |
|---|---|---|---|---|
| 1 | `0x22BF58(1,0,1)` + `0x22A0C0(1,1)` | FRAME wb4, TEX **the live screen** (`0x22A198`, PSMCT24) | far | `0x22C888` -> `0x22C4E0`, extra 0 |
| 2 | `0x22AB90(2,1,2)` + `0x22A0C0(0,2)` | TEXCBUMP, ALPHA 0x48 | far | `0x22C920`, ST offset (+0xB0,+0xB4) = 0.01 |
| 3 | `0x22A0C0(2,2)` | ALPHA 0x42 | far | `0x22C920`, offset (0,0) |
| 4 | `0x22BFD0(0,1,1)` + `0x22A0C0(1,1)` | FRAME wb3, TEX wb4 | far | `0x22CCE8` -> `0x22CB58` |
| 5 | `0x22A0C0(0,3)` | ZTST GREATER, ALPHA 0x48 | near | `0x22CA68` |
| 6 | `0x22BFD0(1,0,0)` + `0x22C100()` | FRAME wb4, TEX wb3 | — | half-width additive blit |
| 7 | `0x22BFD0(0,1,1)` + `0x22A0C0(1,1)` | FRAME wb3, TEX wb4 | near | `0x22C888` -> `0x22C4E0`, extra 0 |
| 8 | `0x22AB90` + `0x22A0C0(0,2)` then `(2,2)` | TEXCBUMP | near | `0x22C920` x2 |

### 4.2 `0x226D00`'s tail — `MenuConfigCubes()`

```
five x 0x22D2E8                     the table above
0x22A4C8(1, 0x27F180, field)        clear wb4 to {0,0,0,**0**}
five x 0x22D798                     near faces only, into the cleared wb4:
                                    (0x22CD78 - not ported) then 0x22CA68
0x22BFD0(0,1,0) + 0x22C088()        wb4 over wb3, ADDITIVE
0x22C2A0(phase < 5 ? 5-phase : 0)   0 passes on a settled screen
0x22C020(0,0,0) + 0x22C190(1)       wb3 back over the screen, ABE = 1
```

**The second walk is a mask generator, and `0x22C088` is how the mask travels.**
Work buffer 4 is cleared to alpha 0; the only thing painted into it is
`0x22CA68`'s black at `A = 0x80`; `0x22C088` (record `0x27F6E0`, ABE 1, under
`0x22A0C0(0,1)` = ALPHA 0x48 = `Cs*As + Cd`) therefore adds **no colour** — and
copies `As`, i.e. work buffer 4's own alpha, into work buffer 3's alpha channel,
because GS alpha blending only touches RGB and the destination alpha is written
from the source's regardless.  `0x22C190(1)` then blends by exactly that.

Measured in the port at frame 145 (temporary work-buffer readback, removed):

```
                x=140     150      160      170      180      190      200
WB4  y=50    000000/00 000000/00 000000/80 000000/3c 000000/80 000000/80 000000/80
WB3  y=50    51596d/00 5b6377/00 006eda/80 0272dd/3c 0049ab/80 0042ae/80 003fab/80
SCR  y=50    211a31/-- 211930/-- 006eda/-- 10417e/-- 0049ab/-- 0042ae/-- 003fab/--
```

- work buffer 4 black with alpha 0x80 exactly on the cube (0x3c and friends are
AA1 edge coverage), work buffer 3 carrying that alpha with the pre-object copy
`(81,89,109)` still intact outside it, and the screen equal to work buffer 3
inside the mask and to the tinted backdrop `(33,26,49)` outside.  The mechanism
is exactly as reversed.

### 4.3 The half pixel

The ROM's `field` arguments, read off the call sites:

```
0x22BF58(1,0,**1**)  pass 1      FRAME wb4     field     22d36c: 24060001 li a2,1
0x22BFD0(0,1,**1**)  pass 4/7    FRAME wb3     field     22d51c / 22d5dc: li a2,1
0x22BFD0(1,0,**0**)  pass 6      FRAME wb4     none      22d5b8: move a2,zero
0x22A4C8(1,rec,*(0x27B448))  the wb4 clear     field
0x22BFD0(1,0,**1**)  0x22D798    FRAME wb4     field     22d810: li a2,1
0x22BFD0(0,1,**0**)  the tail    FRAME wb3     none      226ef8..226f04
0x22C020(0,0,**0**)  the tail    FRAME screen  none      226f34..226f40
```

i.e. **every mesh pass carries the interlace half pixel and every
buffer-to-buffer sprite does not** — exactly 37efd18's rule, stated by the ROM
itself.  `MenuBackWorkTarget`/`MenuBackScreenTarget` take the flag per call,
which is a closer model than the old `MenuBackMeshHalfOffset` bracket; that
function is removed (see §6).

---

## 5. Evidence

Headless PCSX2, **software** renderer, Xvnc :99, `ee-gcc 2.9-ee-991111 -O2`,
clean build, no warnings.  Logs in `logs/`.  "HEAD" = eba5595 rebuilt from
`pristine/` in this tree (`headbits/main.elf`).

### 5.1 The cubes, before and after

`menu 12 34 56 0 1 128 145 0 1 0 10 0 1 0 1`, frame 145, RGB every 9 px.

**Cube 1** (`(78,84)`, an *unselected* cube, colour easing to `{100,100,100}`),
row y = 84:

```
x=        33       42       51       60       69       78       87       96      105
HEAD  2b2640  34264e  495162  495061  3f4556  484f5f  474e5e  4c5363  3e4554  8995b7
NEW   2b2640  34264e  130f1d  14101e  191423  110d1a  100c18  120e1a  8394c1  76799a
```

Background beside it is `(39,31,59)` / `(35,28,52)`.  HEAD's face is a flat
`(73,81,98)`, **2.0x the background**.  The new face is `(19,15,29)` ...
`(16,12,24)`, **0.45x the background** — a piece of glass that darkens what is
behind it, which is what `{100,100,100}/128 = 0.78` squared over two layers has
to do.

**Cube 3** (`(86,142)`), row y = 142, the same shape:

```
HEAD  ... 4a5062 454b5b 3f4554 3d4251 3f4352 404452 ...    flat (74,80,98)
NEW   ... 15111f 110c1a 120e1b 0f0b18 100d18 ...           (21,17,31)..(16,13,24)
```

**Cube 4** (`(199,170)`), row y = 170, where the structure shows best:

```
HEAD  1f182e 3e414f 363845 333642 353744 3a3c48 363945 323541 9ca0c0 1c1528
NEW   1f182e 657ba3 214569 303140 13202e 081624 172637 191e2f 557b9d 1c1528
```

HEAD: eight nearly identical numbers, spread 62..79 in R.  NEW: 8..101 in R,
19..123 in G — the refraction displacement and the emboss are visible **as
structure across the face**, and the face sits either side of the background's
own `(31,24,46)` instead of two to six times above it.

**Cube 0** is the *cursor's* cube, and `0x226BB8` eases its colour toward the
tracker `0x27EC30` -> `0x27EC10` = `{0,150,200}`; at frame 145 it is there.  It
therefore modulates by `(bright+0, bright+150, bright+200)/128` **twice**, and
comes out a saturated cyan: predicted `(5,58,179)` for a mid face at
`bright = 50` over a `(31,24,47)` background, measured `0041aa` = `(0,65,170)`.
That is the ROM's "the selected cube tints blue" effect doing exactly what its
arithmetic says, not a residue of the old bug.  (`MenuConfigSetCursor()` is
still unwired, so item 0 is always the selected one — unchanged from HEAD, one
line in `menutext.c` away.)

### 5.2 The main menu is bit-identical

`menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1`, whole-frame 8x8 map at frame 60:
`diff logs/head-mm60.map logs/final-mm60.map` is **empty**, 0 of 2240 blocks
differ.  Everything new is either behind the cube timer or a no-op refactor
(`BlurBlit` gained an `abe` argument passed 0 at both old call sites;
`MenuBackBindScreenCopy` became a wrapper on `MenuBackBindWork(0)`).

### 5.3 Enter and leave are clean

`menu 12 34 56 0 1 128 220 0 0 0 10 0 1 140 1` (enter 1, leave 140, dump 220):
`diff` against HEAD's map is **empty** — tunnel, rods, cubes all gone, black
backdrop and the orb cluster back.  The full config screen with its 2D layer
(`... 145 0 0 0 10 0 1 0 1`) still shows the three text rows intact, so the
FRAME/XYOFFSET state the stage leaves behind is right.

### 5.4 The one regression: field alternation

8x8 max-luminance maps at frames 145/146/147 (`notext = 1`), mean absolute
ramp-index difference; adjacent frames are opposite fields, frames two apart are
the same field with twice the animation, so a ratio above 1 is field-alternating
content:

| build | d(145,146) | d(146,147) | d(145,147) | ratio |
|---|---|---|---|---|
| HEAD | 0.060 | 0.059 | 0.044 | 1.36 |
| this | 0.082 | 0.087 | 0.049 | **1.72** |

This is the direct consequence of §4.3: eba5595 read `0x22BF58`'s third argument
as 0 and bracketed the whole cube stage with the half pixel **off**; the
argument is 1, so the cubes shimmer at 60 Hz exactly as the rods and the tunnel
already do — all three of which the ROM draws the same way.  The number is close
to the 1.61 that write-up itself measured for "all fixes, cubes still on the
frame's half pixel", and the port's blits are all still on the field-0 side of
37efd18's rule.  Kept faithful; if aap prefers the quieter picture, the change
is one argument (`field = 0` in `MenuConfigCubes`) and it is a deliberate
divergence, not a bug fix.

---

## 6. Corrections to `docs/menu-config.md`

The pass table in the appended glass write-up (§2c) and §5 of the original both
need edits:

1. **§2d — "`0x226D00`'s tail ends with `0x22C020(0,0,0)` + `0x22C190(0)` — an
   **opaque** full-screen blit of work buffer 3 back over the screen".**  The
   argument is **1** (`226f48: li a0,1`), it is the record's ABE bit, and the
   blit is an **alpha-masked composite**.  This is the error the whole task
   turned on: read as opaque, the chain looks like it would erase the frame, so
   binding the un-tinted work buffer as the refraction source looks forced.
2. **§5 — "the same loop again through `0x22D798` (into work buffer 3)".**
   `0x22D798` targets **work buffer 4** (`0x22BFD0(1,0,1)`), after `0x226D00`
   clears it to `{0,0,0,0}`.  It is the mask pass, not a second colour pass.
3. **§3 "What it is" and eba5595's commit message — "the cubes: `0x22BF58(1,0,0)`
   passes 0 ... the whole cube stage runs with no half pixel".**  `22d36c` is
   `24060001 li a2,1`.  The cube *meshes* pass the real field like everything
   else; only `0x22C100`, `0x22C088` and `0x22C190` pass 0 (§4.3).
4. **§2c rows 4 and 5 label both `0x22CCE8` and `0x22CA68` loops loosely as
   "front"/"back".**  Pass 4 (`0x22CCE8`) is the `cull == 0` set and pass 5
   (`0x22CA68`) the `cull != 0` set, and the ROM's order over the whole function
   is `cull == 0` **first** (passes 1-4) and `cull != 0` **last** (passes 5, 7,
   8).  The collapsed chain in HEAD drew them the other way round.
5. **§2c row 6 "wb3 -> wb4, half-width blit".**  Correct, and it is **additive**
   (`0x22A0C0(0,1)` = ALPHA 0x48) and gated by work buffer 3's alpha, so on a
   settled screen it moves only the silhouette, not the backdrop copy.
6. **§7.4's rod table, row 4 — "FRAME screen, TEX wb3 (`0x22C020(1,0,1)`)".**
   `0x22C020`'s first argument goes to `0x22A290`, so the texture is **work
   buffer 4**, not 3.  Row 2/3's order is also the reverse of the cubes':
   `0x22A0C0(**2**,2)` (subtractive) comes first and `0x22A0C0(**0**,2)`
   (additive) second, at phases `f21 + i*(gp-32032)` and
   `scene->+0xB0 + f21 + i*(gp-32028)`; both constants are 0.1.
7. **`docs/menu-backdrop.md` §6 — "`0x22C228` is the same loop against work
   buffer 3 and record `0x27F7A0` ... reached from elsewhere; not chased".**  The
   loop `0x226D00` runs is **`0x22C2A0`**, record `0x27F7E0`; `0x22C228` is a
   single flat `0x27F7A0` sprite under ALPHA `0x68` and still has no caller here
   (§3.5).
8. **§10.2's "the cubes' second pass + wb3 blur + `0x22C2A0` composite — not
   ported"** and the "Remaining deltas" bullet that calls this "the biggest
   single remaining piece" can both be struck.

---

## 7. The rods: checked, and no fix needed

The brief asked whether the same class of bug applies to `0x22D920`'s
`0x22E0EC` arm.  It does not, and the disassembly says why.  The five loops:

| # | ROM | target / texture | faces | emit |
|---|---|---|---|---|
| 1 | `0x22BFD0(1,0,1)` | FRAME wb4, TEX wb3 | far | `0x22C888`, extra 0 |
| 2 | `0x22A0C0(2,2)` | TEXCBUMP, ALPHA 0x42 | far | `0x22C920`, phase `f21 + i*0.1` |
| 3 | `0x22A0C0(0,2)` | ALPHA 0x48 | far | `0x22C920`, phase `+0xB0 + f21 + i*0.1` |
| 4 | `0x22C020(1,0,1)` | **FRAME the screen**, TEX wb4 | near | `0x22C888`, extra 0 |
| 5 | `0x22BFD0(0,1,1)` | FRAME wb3, TEX wb4 | near | `0x22C888`, extra **255** |

Pass 4 puts the rods' visible content **straight on the screen**.  There is no
work-buffer composite anywhere in their path, so there is no destination to get
wrong: in the ROM as in the port, a rod samples an un-tinted copy of the frame
and draws it onto the tinted frame.  That is why the retail rods glow and the
retail cubes do not, and it is why HEAD's rods already looked right while its
cubes did not.  `f21` is `0x22D920`'s `(float)rec->slot * *(gp-32056)` =
`slot * 0.1`.

What the port still skips for them is pass 1-3 (the far layer, into work buffer
4, which pass 4 then samples) and pass 5's `extra = 255` silhouette into work
buffer 3.  Left alone deliberately, and worth recording why: **`0x22D920` ends
with FRAME = work buffer 3 and nothing restores it.**  `0x226700`, the
depth-sorted walk, alternates `0x226360` (orb) and `0x2266E0` -> `0x22D920`
(mesh) with no drawenv of its own, and `0x226360` pushes none either (read to
its `jr ra` at `0x2266D8`) — so in the ROM an orb that sorts *after* a rod is
drawn into a work buffer rather than onto the screen.  That is a real ROM
behaviour with visible consequences and it wants its own investigation; it is
not something to port half of.

---

## 8. Left simplified, and why

| what | why |
|---|---|
| `0x22CD78`, the TEXCREFA reflection in `0x22D798` | no TEXCREFA in `res.c` / `res/`, and `0x22CA68` overdraws the same faces opaquely one loop later (§3.6) |
| the rods' five-pass chain | needs the FRAME-restore question of §7 answered first |
| `0x22D920`'s `f12 > 0` arm (the front rod's split) | unchanged from HEAD |
| `0x2267E8`'s two-pass carousel bloom | unchanged from HEAD |
| `MenuConfigSetCursor()` still unwired | unchanged from HEAD; one line in `menutext.c`, which this job may not touch |

Nothing in the cube chain itself is simplified: passes 1-8, the wb4 clear, the
second walk's black pass, `0x22C088`, `0x22C2A0` and `0x22C190(1)` are all in.

---

## 9. Deliverables and cleanup

* `refract.diff` — `inc.h`, `menuback.c`, `menuconfig.c`; `git apply -p1` from
  the repo root, checked with `git apply --check`.
* `notes.md` — this file.
* `logs/` — every run above.  `headbits/` is the eba5595 reference build (from
  `pristine/`), `pristine/` the untouched HEAD sources the diff is against.
* `ascii.sh` — the harness, with one change from the `config/` copy: it kills
  only the `pcsx2-qt` whose command line names an `osdbits/main.elf`, and takes
  `ELF=` from the environment so `headbits/main.elf` runs through the same path.
  The owner's own interactive pcsx2 (`-elf ./main.elf`, cwd the real tree) came
  up mid-job and was correctly waited out, not killed.
* All temporary readbacks (`RGBgrid`/`CUBEn`/`WB3`/`WB4` in
  `menu.c:DumpFrameAscii`) are removed; `menu.c` is byte-identical to HEAD.
* Xvnc :99 started by this job and killed by it; no pcsx2-qt left running that
  this job started; `ulimit -c 0` throughout, no cores.  No PCSX2 setting was
  changed (no `-setting`, no ini edit); the only deltas the ini shows against
  this job tree's Aug-30 backup are window geometry, `HeaderState` and one pad
  binding, which `-batch -nogui` writes back on exit and which predate this
  job's runs.
* The real tree `/u/aap/src/ps2rev/osdsys` is untouched and clean at eba5595;
  nothing was committed.

---

# The OSDSYS "chrome", reversed and ported (2026-08-31)

Confidence convention as `docs/menu-draw.md`: **[ok]** = read out of
`ee-objdump -m mips:5900` of `/u/aap/src/osdsys/expanded.bin`
(VA = offset + 0x200000, gp = 0x2AF070) and cross-checked against the
retail screenshots or a headless run; **[tnt]** = partial; **[?]** =
guess.  Everything below is [ok] unless marked.

Scope: stages **6, 7 and 8** of the frame body `0x21CF20` - the parts
that sit on top of every screen - plus the second font page the config
screen's page marker comes off.

```
| # | call                    | what                          |
|---|-------------------------|-------------------------------|
| 5 | 0x2283F0()              | the per-screen 2D renderers   |  <- already ported
| 6 | 0x21D368() -> 0x21D1F8() | the letterbox bars           |  <- new
| 7 | 0x21D3A0()              | the date/time header          |  <- new
| 8 | 0x21DA68() -> 0x21D7F8() | the button hint bar          |  <- new
| 9 | 0x21DB18()              | a right-edge strip            |  not ported
```

---

## 0. Two corrections to the brief before anything else

**FNTEXOSD does not power the hint bar.**  The brief calls it "the
symbol font that powers it".  It does not: `0x21D7F8` draws its coloured
button glyphs through **`DrawIcon` (0x21D590)**, which binds texture
slot 8 or 9 - **TEXCSTSL** or **TEXCMARU** - and emits a plain sprite.
FNTEXOSD is the text engine's *kind-2* glyph page, reached only through
the `\7oNNN` escape, and on these two screens it draws exactly one
thing: the System Configuration **page marker**.  Both are ported; they
are simply unrelated mechanisms.

**`0x228660` is not a clamp.**  `docs/menu-draw.md` §9.1 reads its tail
as "clamps to 0 above 127", and concludes "several hint sets cross-fade
simultaneously".  The instruction pair is

```
2286f4: slti v1, v0, 128        ; v1 = (alpha < 128)
2286fc: movn v0, zero, v1       ; if (alpha < 128) alpha = 0
```

i.e. it **zeroes anything below 128**.  The hint sets never cross-fade:
a set is drawn only while its screen is *fully* up, which is why
`0x21DA68`'s loop over seven sets emits at most one.  §9.1 wants
amending.

---

## 1. FNTEXOSD - the kind-2 glyph page

### 1.1 It is the ASCII font again, with one number changed

`0x2086A0(kind, &bound, code)` has three arms and they differ in almost
nothing:

| kind | binds | metrics table | columns | bounds check |
|---|---|---|---|---|
| 0 | slot 0 FNTASCII, **only if `!*bound`**, then sets it | 0x26FE60, 97 entries | `div 8` | `(u32)(c-32) < 97` |
| 1 | slot 1/2 FNTEX000/001, always | 0x270160 / 0x270AE0 | `div 16` | none |
| 2 | **slot 3 FNTEXOSD, always** | **0x271460, 35 entries** | **`div 16`** | none |

Everything after the arm is shared: `col = g % cols`, `row = g / cols`,
`u0 = col*32 + inset`, `u1 = u0 + width`, `v = row*40`, the same
half-texel insets (`+8` / `+632`), the same `0x2DDC50` drawn height.  So
FNTEXOSD is **the same 32x40 cell as FNTASCII, laid out 16 columns
wide instead of 8**, and there is no second decoder to write.

Page record, slot 3 of the four-record table at **0x271578** (stride 24:
`+0` tbp, `+4` blocks, `+8` tbw, `+12` tw, `+16` th, `+20` clut tbp):
`{ -, 128, 8, 9, 7, - }` - 512x80, TBW 8, TW 9, TH 7, 128 blocks.
`0x208460` hardcodes **PSM = 0x14 (PSMT4)**, TCC 1, TFX MODULATE,
CLD 1, CSM1/PSMCT32 CLUT, TEX1 = 97 (LINEAR/LINEAR).  The CLUT is
**0x2715E0 - the very same 16-entry grey ramp FNTASCII uses**; nothing
on the page is coloured, the pen colour supplies that.

The resource expands to exactly **20480 = 512*80/2**, confirming plain
uncompressed 4-bit indexed.

### 1.2 The page, rendered

16 columns x 2 rows, glyph `g` at `(g%16)*32, (g/16)*40`:

```
row 0:  0 down-arrow   1 right-arrow  2 left-tri    3 right-tri
        4 (R) large    5 (R) small    6 up-tri      7 down-tri
        8 left-tri sm  9 right-tri sm 10..13 four grey button blobs
       14 cycle-arrow 15 up/down chevrons
row 1: 16 "(PS2)" (60 px, spans two cells)          17 dead (overlaps 16)
       18 filled square  19 brightness sun
       20 UP/DOWN CHEVRONS  <- the page marker
       21 up/down chevrons (variant)   22..34 all {0,13}, unused
```

Verified numerically: the ink runs measured off the decoded page agree
with the 0x271460 metrics cell for cell, e.g. glyph 20 `{3, 26}` ->
u 131..157, ink 132..155.  Entry **17 `{0,30}` is dead data** - its cell
is the right half of glyph 16's "(PS2)".

Consumers in the shipped Latin strings:

* `gp-30416` = `0x2A79A0` = `"\7o020"` - the config page marker.
* string 93 `"PlayStation\7o004"` - the (R).
* `0x2A4750` = `"\7r0.88\7o019\7r0.00"` - the clock's optional
  brightness prefix, used when `0x203928()` (bit 29 of `0x2A8700`) is
  set.  Not reachable in the port.

### 1.3 Two escapes are now obeyed, not skipped

`0x209300`'s 25-arm jump table (0x2A3CB0).  The port previously skipped
every arm by length; two are now executed, because Module U's own data
needs them:

* **`\7oNNN`** (`0x2094AC`, 5 bytes) - three decimal digits; **returns**
  the glyph, which `0x209640` emits with `0x2086A0(2, ...)` and
  `0x209998` measures with `0x208610` (the same expression as
  `0x208540` against 0x271460).  So `"\7o020"` measures **23**
  (26 - 3 gap), not 0 - `docs/menu-config.md`'s note that "its width is
  0 here" no longer applies, and both of `0x227560`'s right-margin
  clamps now carry the term the ROM gives them.
  After emitting one, `0x209640` **clears the caller's `bound` flag**
  (the delay slot at 0x2096C8), so the next Latin glyph rebinds
  FNTASCII.  Ported literally.
* **`\7rN.NN`** (`0x209400`, 6 bytes) - digits at `s[1] s[3] s[4]`, into
  `0x271564`, then `0x207F68(*(0x2DDC48))` - **the saved scale**, so
  percentages do not compound.  `0x207F68`'s second half then splits the
  percentage into a scale for the advance (`0x2DDC44`), a height
  (`0x2DDC50`) and two shift terms (`0x27186C`, `0x271870`) that keep the
  shrunken glyph on the baseline; only `0x271870` reaches the sprite.
  Note the emitter takes the **y bias from `0x2DDC48`** (unscaled) and
  the **width from `0x2DDC44`** (scaled) - that asymmetry is load-bearing.

  This matters for exactly one string: id 111
  `"\7r0.90DIGITAL OUT (OPTICAL)\7r0.00"`.  Without it that label
  measures 355 instead of **322** and, being the widest of the five, it
  is what `0x228708` puts in the header's `+0x0C` - so the page marker
  would sit ~16 px too far right.

### 1.4 Pipeline

`tools/extract-res.py --tables` grew the 0x271460 table
(`fontOsdMetrics[35][2]` in `res/FONTDATA.inc`); the page itself comes
from the existing `--container FNTIMAGE` path.  `res.c` wires
`RESID_FNTEXOSD`.  Upload is the plain `InitTexture()` path FNTASCII
already uses (PSMT4 + the same 16-entry CLUT).

---

## 2. The button hint bar - `0x21DA68` / `0x21D7F8`

### 2.1 Layout, exact

```
y    = 0x21D9E0() = (uiModel[0] == 2) ? 182 : 200,  * 0.5405/0.47 on PAL
scale = *(gp-32216) = 0.8
ids  = (set == 8) ? 0x27B5D0 : 0x27B5E8 + set*20 + (0x204318() ? 180 : 0)
xs   = 0x27B760 + GetLanguage()*16
for (i = 0; i < 4; i++) {
    if (ids[i] == 1) continue;                 /* 1 = no button here */
    if (i < 3) { DrawIcon(0x27B7E0[i], xs[i], y, alpha);
                 0x21DC28(xs[i] + 28, y, 0x27B750, alpha, osdGetString(ids[i])); }
    else       { w = 0x209998(s) + 24;         /* slot 3 is right-anchored */
                 DrawIcon(0x27B7E0[3], screenW - w - 28, y, alpha);
                 0x21DC28(screenW - w, y, 0x27B750, alpha, s); }
}
```

* **`0x27B7E0` = `{2, 4, 5, 3}`** - square, cross, circle, triangle.
* **`0x27B750` = `{96, 96, 96, 128}`** - the label colour, shared with
  the clock.
* **`0x27B760`** English row = `{24, 213, 335, 441}`.  The fourth column
  of every language row is **dead data** - slot 3 never reads it.
* Icon at `xs[i]`, label at `xs[i] + 28`.

Measured against `ss-real1.png` (640x480, i.e. a 640x224 field scaled
2.1428x vertically, 1:1 horizontally):

| | ROM says | retail ink | our build |
|---|---|---|---|
| square icon | sprite x 24..49 | 27..46 | 27..46 |
| "Display" | pen x 52 | 55..135 | 54..132 |
| cross icon | sprite x 213..238 | 216..235 | 216..234 |
| "Enter" | pen x 241 | 244..302 | 244..302 |
| circle icon | sprite x 335..360 | 338..357 | 338..354 |
| "Back" | pen x 363 | 365..421 | 364..412 |
| triangle icon | `640 - 113 - 28` = 499 | 502..521 | 502..518 |
| "Options" | `640 - 113` = 527 | 529..616 | 528..616 |

(the last two follow from `osdTextWidth("Options") = 89` at scale 0.8;
the readback's right edges run a block or two short because the 2x2
block takes the max of four pixels and the antialiased tail falls below
the ramp's first step.)

### 2.2 The glyphs are TEXCMARU sprites, and the colour is in the texture

`DrawIcon` = `0x21D590`:

```
if (0 <= glyph < 2) { w = h = 28; 0x22AB90(8, 0, 1); }   /* TEXCSTSL */
else                { w = h = 25; 0x22AB90(9, 0, 1); }   /* TEXCMARU */
rec = 0x27B530                       /* RGB 128,128,128, alpha = argument */
rec.x0 = x<<4 ; rec.x1 = (x+w)<<4
rec.y0 = y<<4 ; rec.y1 = (y + h/2)<<4       /* half height: one field */
uv = 0x27B570 + glyph*16 ; rec.u = uv*16 + 8 (all four)
if (IsPAL()) rec.y1 = y0 + (y1-y0)*0.5405/0.47
0x2299C0(rec)
```

The slot numbers resolve through `0x2297B8`/`0x2297A0`: slot record
`0x27F1C0 + slot*12` = `{ptr, log2w, log2h}`, VRAM base
`0x27F280[slot]`, and `0x229698` fills slot *i*'s pointer from
**resource `45 + i`** - so slot 8 = **53 TEXCSTSL**, slot 9 =
**54 TEXCMARU**, both declared 64x64 and both **PSMCT32**.  TEXCMARU
expands to exactly **16384 = 64*64*4**; TEXCSTSL to 8192, i.e. it is
really 64x32 (its two glyph rects both live in rows 0..32).

**Answering the brief's question directly: the colours are neither a
per-glyph CLUT nor a vertex colour - they are RGBA texels.**  TEXCMARU's
2x2 grid, sampled at each quadrant centre:

```
0x27B570 rects        TEXCMARU 64x64 PSMCT32, alpha 0x80 = opaque
                                                  brightest texel RGB
glyph 2 {0,0,32,32}   top-left      PINK square      (249, 138, 202)
glyph 3 {32,0,64,32}  top-right     GREEN triangle   ( 26, 211, 111)
glyph 4 {0,32,32,64}  bottom-left   BLUE cross       (140, 149, 252)
glyph 5 {32,32,64,64} bottom-right  RED circle       (255,  90,  90)
```

Each is a coloured ring/outline on a dark (40,40,40) disc; the retail
screenshot's per-hint average colours - (163,97,135), (95,100,156),
(174,71,71), (32,138,81) - are those four rings averaged against their
discs, in the same left-to-right order the `{2,4,5,3}` glyph table gives.

The record's RGB is a flat 128,128,128 and MODULATE passes the texel
through unchanged; only the alpha is the caller's.  Only TEXCMARU is
uploaded here - nothing on these two screens asks for START or SELECT,
and `DrawIcon` returns early for glyphs 0..1 with a comment saying so.

### 2.3 Which set each screen shows

`0x21DA68`:

```
if (*(gp-30768))      0x21D7F8(8, *(gp-30772), y);   /* caller-supplied */
else if (0x226A48())  0x21D7F8(7, 128, y);           /* *(0x27BE40) == 1 */
else for (i = 0; i < 7; i++) { a = 0x228660(i); if (a > 0) 0x21D7F8(i, a, y); }
```

`0x228660(set)` dispatches through the 6-entry table at 0x2A4B50 -
`set 1 -> 0x227E18` (main menu), `2 -> 0x2271B8` (System Configuration),
`3 -> 0x221060(1)`, `4 -> 0x221060(0)`, `5 -> 0x21F980`,
`6 -> 0x226FD0` - and then zeroes anything under 128 (§0).  Set 0 is
unreachable (the table is indexed by `set-1`, bounded at 6).

The nine 20-byte records at **0x27B5E8**, `{4 string ids, pad mask}`,
and the second block 180 bytes on:

```
      block A (0x204318 == 0)          block B (0x204318 != 0)
set 0 {  1,  1,  1,  1, 0x0000 }       {  1,  1,  1,  1, 0x0000 }
set 1 {  1,  1, 86, 95, 0x5000 }       {  1, 85,  1, 95, 0x5000 }   main menu
set 2 { 94, 85, 86,  1, 0x5000 }       { 94, 85, 86,  1, 0x5000 }   SysConfig
set 3 {  1, 85,  1, 87, 0x5000 }       {  1,  1, 86, 87, 0x5000 }
set 4 {  1, 85,  1,  1, 0x5000 }       {  1,  1, 86,  1, 0x5000 }
set 5 {  1, 85, 86,  1, 0x5000 }       {  1, 85, 86,  1, 0x5000 }
set 6 { 94,  1,  1,  1, 0x0000 }       { 94,  1,  1,  1, 0x0000 }
set 7 {  1, 85, 86,  1, 0x5000 }       {  1, 85, 86,  1, 0x5000 }
set 8 {  0,  0,  0,  0, 0x0000 }       {  0,  0,  0,  0, 0x0000 }   (never read)
```

ids: 85 "Back", 86 "Enter", 87 "Options", 94 "Display", 95 "Version".

So **the main menu does have a hint bar**: circle (or cross, per region)
"Enter" and triangle "Version".  It has no clock (§4).

### 2.4 Where the fourth hint comes from - a real find

The retail config screen shows **four** hints (Display / Enter / Back /
Options).  **No set in the table has four.**  Set 2 has three; slot 3 is
empty.

The fourth comes from the *caller-supplied* set 8 at **0x27B5D0**,
armed by **`0x21EB80`** - and `0x21EB80` is **Clock Adjustment's
`+0x28` focus callback**, the only one of the five config items whose
`+0x28` is not the bare `jr ra` at `0x21F160`:

```
0x21EB80(item, focused):
    if (!focused) { 0x21D748(0); return; }        /* disarm */
    0x21D768(94, 85, 86, 87);                     /* Display/Back/Enter/Options */
    0x21D758(0x5000);                             /* pad mask TRIANGLE|CROSS */
    0x21D748(1);                                  /* arm */
```

It is fired by **`0x227D08`**, the config screen's tail, which keeps a
latch in `gp-30404`:

```
open = timerIsState(0x27BE44, 2) && timerIsState(0x27EC40, 0)
if (open)  { if (!*(gp-30404)) { items[cursor].fn28(&items[cursor], 1); *(gp-30404) = 1; }
             hdr->mode == 0 ? 0x2279B8() : 0x227BE8(); }
else       { if ( *(gp-30404)) { items[cursor].fn28(&items[cursor], 0); *(gp-30404) = 0; } }
```

and again, twice, by `0x2279B8` on every cursor move (off the old item,
onto the new one - `lw v1, 40(a0); jalr v1` at 0x2279F0/0x227A28 for UP
and 0x227A78/0x227AB0 for DOWN).

**Consequence, and it is testable: move the cursor off Clock Adjustment
and the triangle/"Options" hint disappears**, leaving set 2's three.
Reproduced - see §7.3.

### 2.5 The region swap

`0x204318()` (via `0x204238`, a region/version word) selects both the
hint block *and* `osdGetString`'s own 85/86 exchange.  `0x21D768` adds a
third twist:

```
0x27B5D0[0] = a;  0x27B5D0[3] = d
if (0x204318()) { 0x27B5D0[1] = (c == 86) ? 85 : c;      /* cross slot  */
                  0x27B5D0[2] = (b == 85) ? 86 : b; }    /* circle slot */
else            { 0x27B5D0[1] = b; 0x27B5D0[2] = c; }
```

It puts **argument c on the cross slot and argument b on the circle
slot**, renaming 86->85 and 85->86 so that `osdGetString`'s exchange
(which fires under the same flag) undoes the renaming.  Net, with the
usual `(b, c) = (85, 86)`:

* flag clear -> cross "Back", circle "Enter"  (the Japanese arrangement)
* flag set   -> cross "Enter", circle "Back"  (what the screenshots show)

The port has no region word.  `textRegionSwap` is `OsdArgInt(16, 1)` -
default 1, the retail arrangement - and it is the only hook.  Both
values verified (§7.4).

---

## 3. The page marker

`0x227560`'s middle block, now that FNTEXOSD exists:

```
markW = 0x209998(gp-30416)       /* "\7o020"  -> 23 */
gap   = 0x209998(0x2A79A8)       /* " "       -> 10 */
x = 430
if (x + markW + gap + hdr->maxw/2 >= screenW - 24)
        x -= x + markW + gap + hdr->maxw/2 + 24 - screenW
if (hdr->mode != 1 && screen fully open)
        0x21DC28(x + hdr->maxw/2 + gap, labelY, 0x27B850,
                 |(int)(128 * sinf(hdr->+0x34 / 10000.0))|, gp-30416)
```

`hdr->+0x34` is a sawtooth `0x227390`'s tail steps by **310** a frame
and folds at **+-refreshRate*31400/60** (+-31400 NTSC, ~203 frames a
lap); `0x21EE50` zeroes it on entering an item.

With `maxw = 322` (id 111 at its 90 % size), `right = 430+23+10+161 =
624 >= 616`, so **the clamp fires**: `x = 422` and the marker lands
left-aligned at **593**.  Retail ink measured 594..617 in `ss-real2.png`
(the glyph is 26 px wide with a 1 px ink inset).  Ours: 592/594.

Note the two clamps still disagree by design - the marker's is computed
off the header's *widest* label and each label's off *its own*.

---

## 4. The date/time header - `0x21D3A0`

```
y = (uiModel[0] == 2) ? 32 : 14                    /* * 0.5405/0.47 on PAL */
0x22A3B8(0x1F0A10, evenOddFrame, 0, field)
0x207F68(*(gp-32220) = 0.83)
0x208110(96, 96, 96, 0x226A60())
0x207E98(22, y);            0x209640(0x20A998(Y, M, D))
prefix = 0x203928() ? "\7r0.88\7o019\7r0.00" : ""
sprintf(buf, "%s %s", prefix, 0x20AAA0(h, m, s))
0x207E98(screenW - 0x209998(buf) - 22, y);  0x209640(buf)
```

* the date format is `0x2039A8()` = the low 2 bits of `0x2B8704`: 0 ->
  `"%04d/%02d/%02d"` (Y/M/D), 1 -> M/D/Y, 2 -> D/M/Y.  Only 0 is ported.
* the clock face is `0x203968()` = bit 30 of `0x2B8700`: 24-hour is
  `"\7p@0%2d\7p00:%02d:%02d"` (the `\7p` fixed-width bracket is a no-op
  for the Latin face - '0'..'9' are all `{5,23}`); 12-hour appends
  `" \7r0.80\7p@AA\7p00M\7r0.00"`.  Only 24-hour is ported.
* the `"%s %s"` with an empty prefix gives the time string a **leading
  space**, which is measured, so the pen lands 10 px further left than a
  bare "18:27:45" would.  Reproduced.

**Which screens show it.**  `0x226A60`:

```
a = 128
if (timerIsState(0x27C258, 0)) {                  /* that screen closed */
    if (!timerIsState(0x27DA70, 0)) return 0;     /* the wizard */
    v = clamp(timerCount(0x27BE44) - (dur40 + dur80), 0, dur10)
    a = (v << 7) / dur10
}
return a * getFadeAlpha() / 128
```

so on the **bare main menu the header is invisible** (0x27BE44's count is
0) and it **fades up over the last ten frames of System Configuration's
130-frame entry**.  That ramp is byte for byte the port's existing
`MenuConfigAlpha()`, so `DrawTopBar` just calls it.

Retail measurements (`ss-real1.png`, "2026/08/31" / "18:27:45"):
date ink from x 26 (pen 22, matching '2' inset 4); time right edge 617
against `screenW - 22 = 618`.  Ours: date 26, time ink from 510 against
retail 510.

**Data source.**  The ROM reads `uiModel[6..11]` (the RTC snapshot).
osdbits has `hh:mm:ss` from argv and no date, so the port draws
`cfgClockDate` = **2000/01/01** - the same fixed date the Clock
Adjustment value row already shows.  That is the one visible divergence
from retail in the header.

---

## 5. The letterbox - `0x21D368` -> `0x21D1F8`

```
0x21D368: t = uiModel[0]; if (t == 0 || t == 2) 0x21D1F8()
```

so the bars are drawn in **4:3 (0) and 16:9 (2)**, and only "Full" (1)
loses them.

```
0x22A3B8(0x1F0A10, evenOddFrame, 0, 0);  0x22A0C0(1, 1)
content = screenW / *(0x27B44C) * 0.0625*9.0 * *(0x27B450)
bar     = (screenH - content) * 0.5
top:    (0,0) .. (screenW<<4, (int)(bar*16))
bottom: (0, (int)((screenH - bar)*16)) .. (screenW<<4, screenH<<4)
record 0x27B4F0, RGBA (0,0,0,128), untextured
```

`0x21C9D0` writes `0x27B44C = 1.0` unconditionally and
`0x27B450 = 0.5405` (PAL) or **`0.47`** (NTSC) - `docs/menu-draw.md`
§9.3 used the PAL constant for its NTSC worked example and got 14.7 px.
The right answer on a 640x224 NTSC field is

```
content = 640 * 0.5625 * 0.47 = 169.2
bar     = (224 - 169.2)/2     = 27.4
```

Checked against `ss-real1.png`: the top black band is rows 0..57 of 480,
i.e. 58 / 2.1428 = **27.1** field lines.  Our readback puts the top bar
at y 0..27 and the bottom edge at y ~196.6, both exact.

Note the ROM does **not** compute the two bars symmetrically - the top
truncates `bar*16` and the bottom truncates `(screenH - bar)*16` - so
they can differ by 1/16 px.  Reproduced rather than tidied.

---

## 6. What the port does

All of it is in `osdbits/menutext.c` (plus two lines of `res.c` and one
table in `tools/extract-res.py`); **no other file is touched**, and the
merge needs **no hook in `menu.c`** - the three stages are called from
the tail of `MenuTextFrame()`, which `menu.c` already calls at exactly
the right point in the frame (after the 2D screens, before the swap), so
the ROM's stage order 5-6-7-8 comes out right for free.

New/changed, in the order they appear:

| | real |
|---|---|
| `textRegionSwap`, `osdGetString`'s 85/86 exchange | 0x204318, 0x2041B8 |
| `fontOsdTexture`, `GlyphFont`, `fontAscii` / `fontOsd` | 0x271578 slots 0 and 3 |
| `osdTextSetScale` second half (the `\7r` percentage) | 0x207F68 |
| `textBound`, `osdBindFont`, `osdDrawGlyph(font, g)` | 0x208460, 0x2086A0 |
| `osdEscape()` replacing `osdEscapeLen()` | 0x209300, 0x2094AC, 0x209400 |
| `osdTextWidth` / `osdTextDraw` kind-2 arms | 0x209998 / 0x209640 |
| `configMenu.phase`, `cfgPhaseFold`, `cfgMarker` | 0x27BE28+0x34, 0x227390, 0x2A79A0 |
| `DrawConfigMenu`'s marker block and `markw` clamps | 0x227560 |
| `ConfigMenuInput`'s focus pair | 0x2279B8 |
| `osdScreenType`, `osdFlatRect`, `DrawLetterbox` | 0x22B0E8(0), 0x2299C0, 0x21D368/0x21D1F8 |
| `cfgFmtClockLine`, `DrawTopBar` | 0x20A998/0x20AAA0, 0x21D3A0 |
| `maruTexture`, `iconUV`, `DrawIcon` | slot 9, 0x27B570, 0x21D590 |
| `hintSet`, `hintX`, `hintGlyph`, `hintTextCol`, `hintCustom` | 0x27B5E8, 0x27B760, 0x27B7E0, 0x27B750, 0x27B5D0 |
| `HintSetCustom`, `ConfigItemFocus`, `ConfigFocusNotify` | 0x21D768, 0x21EB80, 0x227D08 |
| `HintSetAlpha`, `DrawHintSet`, `HintBarY`, `DrawHintBar` | 0x228660, 0x21D7F8, 0x21D9E0, 0x21DA68 |
| `MenuTextDumpBand` + two chrome bands | not original, diagnostic |

`osdScreenType()` returns `configItems[1].value` - the Screen Size item's
own `+0x08`, which *is* `uiModel[0]` in the ROM (`0x21EDB8` re-syncs one
from the other every frame).  So the letterbox, the header's row and the
hint bar's row all follow that item, as they should.

### Resources

`res/FNTEXOSD_EXP.inc` (126 KB) and `res/TEXCMARU_EXP.inc` (101 KB) are
**left in the scratch `osdbits/res/`** for the session owner to copy, as
with MENUGEOM/TEXCBUMP.  `res/FONTDATA.inc` was regenerated and gains
`fontOsdMetrics[35][2]`.  To reproduce:

```
python3 tools/extract-res.py <bios.bin> res --tables
python3 tools/extract-res.py <bios.bin> /tmp/x --container FNTIMAGE   # FNTEXOSD
python3 tools/extract-res.py <bios.bin> /tmp/x --container TEXIMAGE   # TEXCMARU
cp /tmp/x/FNTEXOSD_EXP.inc /tmp/x/TEXCMARU_EXP.inc res/
```

Both `scph39001.bin` and `PS2 Bios 30004R V6 Pal.bin` in the PCSX2 bios
directory give byte-identical output for all four resources, and
FNTASCII from either matches the committed `res/FNTASCII_EXP.inc`
(md5 33e67de9c2feea76779df069bb9ee3d0).

---

## 7. Verification

Headless PCSX2, software renderer, on the already-running Xvnc `:99`
(not started by me, not killed).  Clean `-Wall` build, no warnings.
`chrome.diff` `git apply -p1 --check`s and round-trips.

The readback (`MenuTextDump`) now prints the old item band **with its
exact old window** plus four new half-width chrome bands - full width
would exceed the emulator log's 254-character line limit and lose rows.

Full transcripts: `evidence-cfg.txt`, `evidence-main.txt`,
`evidence-cfg-cursor1.txt`, `evidence-cfg190.txt`,
`evidence-cfg-noswap.txt`; raw logs in `logs/`.

### 7.1 The main menu is bit-identical, and gains chrome

`menu 12 34 56 200 1 0 0 0 0 140 10 0 0 0 1` against a pristine build of
HEAD 509a89b (`../base/`):

```
main140 text band: IDENTICAL
```

New on the main menu, both from the ROM:

* the **letterbox** (invisible in the readback there - the backdrop is
  black at the top and bottom of the main menu at frame 140, so the bars
  land on black; the config run below proves the geometry);
* a **two-hint bar**: cross "Enter" at x 216/244 and, right-anchored,
  triangle "Version" at 501/529.

```
chrome botL (x 0..320)                       chrome botR (x 320..640)
 ......      ++++++.                          ..:...     .+:   -+.        +-
.+*=..**:    #+.... --=+=  =#+-  -++.  -.+-  ...===...    ##   #*   ++=  --=+  -++-  -.  -++: .-:++:
..-****...   #*==== *#=:#* :#=..##.-#*.##-.  ..==.==:..   .#* *#  +#=.##.*#*: =#=.#- #- #*:-#*.#*:+#.
.:*****+..   #=     *#  #*  #- .#=====.#:    .==----=-.    -#-#-  *#====.*#   +*=*## #- #+  #*.#. .#:
 *+...:*-    ##****:*#  #*  #*- -#+**..#:     ::::::::      *#+    *#+*+ *#   .#+=#= #- -*+** .#. .#:
   ^cross           E n t e r                    ^triangle    V e r s i o n
```

**No clock on the main menu** - the whole `chrome top` band is black,
as `0x226A60` says it should be.  This is a real change to the main
menu's rendered frame; the *item band* used for regression is untouched.

### 7.2 The config screen: all four pieces at once

`menu 18 27 45 0 1 128 0 0 0 145 10 0 1 0 1 0`, readback at frame 145.
Against the pristine build the item band **differs only by the page
marker** (14 lines, all in columns 180..200 of the band = x 584..624):

```
chrome top (letterbox + header)
|                                                       |  y 0..15  black bar
|             *#***#. -#***#-  *#**#*  -#***#- ...      |  "2000/01/01" from x 26
|             ======.  .===.    -==-    .===.  ...      |
                              (topR, x 320..640)
|          -+##   +#*+*#- .-  *#***#. *****#+ .-    ### |  "18:27:45" from x 510
|          ::*#   +#*=##= -+. *+  +#-    *#=  -+. -***# |
|            -=    -===.      ======.  :=.           == |

text band, label row, columns 180..200        frame 145      frame 190
                                              ::::---::*###+   :::::--:-=====-:
                                              ::::::=**#####*+ ::::-:-========-
                                              :::::::.....:::  ::::---:::::::::
                                              :::::.:*#######= --::::-========-
                                              :::::::.+#####=: --:::::::=====:
```

- the marker is the **up/down chevron pair**, ink from x 592, exactly
  where §3's arithmetic puts it;
- its **alpha pulses**: bright at frame 145 (`#`, predicted 125) and
  dim at frame 190 (`=`, predicted 48) - the 310/frame sawtooth folded
  at +-31400, through `|128 sin(phase/10000)|`.

The bottom bar carries all four hints, at the columns tabulated in §2.1.

### 7.3 Moving the cursor off Clock Adjustment drops "Options"

Same run from a build whose only change is `OsdArgInt(15, 1)` (argv slot
16 is unreachable - see §8), i.e. cursor on Screen Size:

```
chrome botL   square "Display"   cross "Enter"
chrome botR   circle "Back"      <nothing right-anchored>
```

exactly set 2's three hints, confirming §2.4.

### 7.4 The region hook, both ways

Same run from a build with `textRegionSwap` defaulted to 0:

```
swap = 1 (default)   cross x213 "Enter"   circle x335 "Back"     <- retail
swap = 0             cross x213 "Back"    circle x335 "Enter"
```

---

## 8. Remaining gaps

* **argv slot 16 is unreachable.**  PCSX2 delivers at most 16 `argv`
  entries, so `OsdArgInt(15)` (`cfgCursor`) and `OsdArgInt(16)`
  (`textRegionSwap`) always take their defaults - the same trap
  `cfgtext/notes.md` hit.  Both §7.3 and §7.4 therefore came from
  one-line temporary builds, reverted before the final build.  If either
  is wanted at runtime the argv list needs a slot freed (or packing two
  flags into one token).
* **The date is 2000/01/01**, not the RTC - the port has no `uiModel`.
  Consistent with the Clock Adjustment value row, which already does
  this.  Wiring both to one clock source is a merge-time job.
* **Date order and the 12-hour face** (`0x2039A8`, `0x203968`) are not
  ported: no settings to select them.  Their format strings are in §4 if
  they are ever wanted.
* **`0x203928()`'s clock prefix** (`"\7r0.88\7o019\7r0.00"`, the
  brightness sun, glyph 19) is never emitted - the flag has no
  counterpart.  The glyph itself is on the uploaded page and would draw.
* **TEXCSTSL is not uploaded**; `DrawIcon` returns early for glyphs
  0 and 1.  Neither screen asks for START or SELECT.  Add the resource
  and drop the early-out if a screen that does gets ported.
* **Escapes other than `\7o` and `\7r`** still only advance the pointer.
  The remaining arms of `0x209300` set the colour (`\7c`), the fixed
  width (`\7p`), and so on; `\7p` is a genuine no-op for the Latin face,
  the others are unused by the strings these screens draw.
* **`0x21DB18`** (stage 9, the right-edge strip sprite at 0x27B7F0) is
  not ported - it draws nothing on either screen, but it was not read.
* **Hint sets 3..6** are in the table but their alpha functions
  (`0x221060`, `0x21F980`, `0x226FD0`) belong to screens the port does
  not have; `HintSetAlpha` returns 0 for them.
* **The `0x226A48()` arm** of `0x21DA68` (set 7 at a flat 128) tests
  `*(0x27BE40)`, a mode flag `0x228460` clears and nothing on these two
  screens sets.  Left out with a comment.
* The letterbox reads `configItems[1].value`, which is always 0 in the
  port (no settings block), so only the 4:3 branch is ever exercised at
  runtime.  The 16:9 branch (bars + header at y 32 + hint bar at y 182)
  is ported but untested.

---

# Three glass problems on System Configuration, round two

Scratch tree `/u/aap/.claude/jobs/58e316f8/tmp/glass2/osdbits`, on top of
**509a89b**.  Deliverable diff: `glass2.diff` (`menu.c`, `menuconfig.c`;
+309/-35), `git apply -p1` from the repo root — checked with
`git apply --check`.  `inc.h` and `menuback.c` are byte-identical to HEAD and
not in the diff; `menutext.c`, `res.c` and `tools/extract-res.py` were never
touched (the parallel workstream moved all three in the real tree while this
job ran, so the diff deliberately excludes them).

One new **untracked** resource: `res/TEXCREFA_EXP.inc`.  Regenerate it with the
existing tool, unchanged —
`python3 tools/extract-res.py <bios.bin> osdbits/res` — exactly as
`res/TEXCBUMP_EXP.inc` is regenerated today (verified: the tool's TEXCBUMP
output is byte-identical to the copy already in the tree).

Method: `objdump -D -b binary -m mips:5900 -EL --adjust-vma=0x200000
/u/aap/src/osdsys/expanded.bin`, every function read to its own `jr ra`;
**live retail EE RAM and live retail GS local memory** out of the two
savestates; headless PCSX2 software-renderer readbacks of the port, including
a temporary full-frame RGB download so port and retail could be compared as
images rather than as ASCII.  `gp = 0x2AF070`.  Everything below is **[ok]**
unless marked.

---

## 0. The verdict in five lines

| the owner's words | root cause | status |
|---|---|---|
| "there are black seams now" | `0x22CFA8`'s winding test was **inverted**, so every pass drew on the wrong face set | fixed |
| "the refraction seems a bit backwards, as if the faces were rendered in the wrong order" | the same one line | fixed |
| "the clock currently has no refraction either" | the rods' five-pass chain was collapsed to two passes on the screen | fixed (`MeshDrawRod`) |
| the clock ring's orientation | **not a bug** — the ring is time-driven and the two screenshots were 2.4 s apart | proved, no change |
| the big DARK sphere where retail glows | the same inverted winding | fixed |

Two more found on the way, both quantified against retail's GS memory: the
cubes were ~3.5x too dark (the un-ported `0x22CD78` reflection, now ported),
and `0x22D920` leaves FRAME on work buffer 3 — which is safe, because every
later stage pushes its own (§5).

---

## 1. The tools that made this tractable

Both are new here and worth keeping.

### 1.1 Live EE RAM

`zipfile.ZipFile(p).read('eeMemory.bin')` — 32 MB, virtual addresses below
`0x2000000` map 1:1 to file offsets.  `ram.py` in this directory wraps it.
What it settled, at the exact instant of `ss-real1.png` (18:27:45):

```
0x34E6C0  carousel header   offset 6   spin -15635   tilt -32768
0x34E6D0 + slot*48          progress 1.0, split 0.5373 on slot 6 only
0x27E950  rod scene         +0x80 {45,96,102,128}  +0x90 160  +0xA0 {8,8,8,128}
                            +0xB0/+0xB4 -0.0080     +0xB8 1.0  +0xC0 {60,60,60,128}
0x27EFB0  cube scene        +0x60 0x352800  +0x64 0x352840 (identity, confirmed)
                            +0x80 {100,100,100,128} +0x90 200 +0xA0 {8,8,8,128}
                            +0xB0/+0xB4 +0.0100     +0xC0 {120,120,120,128}
0x27F090  cube table        cursor cube {0,150,200,128} alpha 128, the other four
                            {100,100,100,128} alpha 0 - i.e. 0x226BB8 as ported
0x3529D0  face bank         cube 4's six faces, cull = 0,1,0,0,1,1
0x34E980  the sorted list   19 records: 12 type-0 rods at |t| = 20.00 exactly,
                            7 type-1 orbs at |t| = 13.34
```

The last line is the one that settled §4: **the port's `CarouselMatrix` already
reproduces the ROM's rod world matrices to three decimals.**

### 1.2 Live GS local memory

`GS.bin` in the savestate is 4194813 bytes: PCSX2's register block, then the
**4 MB of GS local memory at offset 425**, then a short tail.  `gsmem.py`
de-swizzles PSMCT32 (page 8192 B / 64x32 px, `blockTable32`, `columnTable32`)
and dumps any TBP as a PNG.  The offset was found by scanning 0..509 for the
one that makes TBP 0 decode as a smooth image and match `ss-real1.png`
pixel-for-pixel (offset 425: 5 sample points agree to a mean of 5 levels,
including the alpha channel; 424 and 426 are byte-rotations of it).

The buffer layout falls straight out of `0x22A198`/`0x22A290`
(`w*h/64`, `3*w*h/64`, `w*h/16` in blocks) and is `2240`-block strides:

| TBP | what |
|---|---|
| 0 | screen buffer 0 |
| 2240 | screen buffer 1 |
| 4480 | the Z buffer (all zeros this frame — nothing writes Z) |
| **6720** | **work buffer 3** |
| **8960** | **work buffer 4** |

`gs_wb3.png` and `gs_wb4.png` in this directory are retail's two work buffers
at 18:27:45.  Being able to read them is what turned three of the findings
below from argument into arithmetic.

---

## 2. Issue 2 — "the refraction seems backwards": the winding test was inverted

**One line, and it is the biggest fix in the diff.**

`0x22CFA8`'s tail, read instruction by instruction:

```
22d1e4  a1 = *(sp+216)              ; v[2].proj   (22d0f0: v0 = s2+192)
22d1ec  jal 0x2676f8 SubVector(sp+176, v[2].proj, v[0].proj)   ; e2
22d1f4  a1 = *(sp+212)              ; v[1].proj   (22d0e0: v0 = s2+112)
22d1fc  jal 0x2676f8 SubVector(sp+192, v[1].proj, v[0].proj)   ; e1
22d214  f1 = e2.x * e1.y
22d21c  f0 = e2.y * e1.x
22d220  f1 = f1 - f0
22d224  c.lt.s  f4(0.0), f1         ; cc = (0 < cross)
22d22c  bc1f    0x22d238            ; NOT LIKELY - the delay slot always runs
22d230  li      v0,1                ;   (delay)
22d234  move    v0,zero             ; reached only when cc is TRUE
22d24c  sw      v0,336(v1)          ; face->+0x150
```

`bc1f` has no likely bit, so `li v0,1` executes unconditionally and the branch
skips `move v0,zero`.  Therefore

```
cull = !(e2.x*e1.y - e2.y*e1.x > 0)
```

and HEAD had `f->cull = (e2x*e1y - e2y*e1x) > 0.0f` — the exact opposite.

**Confirmed against the live face bank.**  `0x3529D0` holds cube 4's six faces
at the moment of `ss-real1.png`.  Their stored `+0x150` and the cross product
of their own stored `+0x20` projected positions:

| face | normal (camera space) | cross | ROM's cull | port's old rule |
|---|---|---|---|---|
| 0 | (0.103, 0.990, −0.093) | **+260** | 0 | 1 |
| 1 | (−0.103, −0.990, 0.093) | **−95** | 1 | 0 |
| 2 | (−0.104, 0.103, 0.989) | **+1417** | 0 | 1 |
| 3 | (−0.989, 0.093, −0.114) | **+295** | 0 | 1 |
| 4 | (0.104, −0.103, −0.989) | **−104** | 1 | 0 |
| 5 | (0.989, −0.093, 0.114) | **−125** | 1 | 0 |

Six for six inverted.  And the geometry agrees with the ROM, not with HEAD:
the cube's centre is at camera-space (−11.25, 11.5, 47.5), so the faces whose
normal points *away* from the eye — 0, 2, 3 — are the back set, and those are
exactly the ones the ROM marks `cull == 0`.  `cull == 0` **is** the far layer,
which is what `menuconfig.c` always claimed.

### What it was doing to the picture

Every pass in both walks keys on that flag, so all of them were on the wrong
set at once:

* `0x22D2E8` passes 1–4 (the far glass into wb4, its emboss, the flat layer)
  ran on the **near** faces;
* passes 5, 7, 8 (the silhouette, the near glass on the screen-facing layer,
  its emboss) ran on the **far** faces.

So the port drew the *back* of every cube on top and refracted the *front* into
the buffer nobody sees — "as if the faces were rendered in the wrong order",
which is precisely what it was.  The rods were saved by a second error
cancelling this one: `MeshDrawRod` drew `back = 1` first and `back = 0` last,
so with the flag inverted the last (visible) pass still landed on the right set.
That is why HEAD's rods looked plausible while its cubes did not, and it is why
the rod path had to be rewritten in the same commit (§4).

### And it is where the black seams came from

`ss-ours.png` and a HEAD dump at 18:27:45 (`base145.png`) show the same thing:
a dashed dark diagonal across every cube face and dark outlines on the edges.
With the flag corrected they are gone — compare `cubecmp2.png` (left: the port
right after this one line; right: retail).  Nothing else changed in that build.

The mechanism: with the sets swapped, the alpha/silhouette pass and the visible
glass pass no longer partition the same silhouette the same way, so the darker
layer's own edges — including each quad's tristrip diagonal — ran across the
middle of the visible faces instead of hiding underneath them.

**The dark sphere in the middle of the ring goes away with the same line.**
It was the orb cluster showing through a wrongly-ordered glass stack; retail's
glowing orbs and the port's now agree (`cmpcull.png`).

---

## 3. Issue 3a — the clock ring's orientation is **not** a bug

`ss-ours.png` shows a face-on twelve-spoke fan where `ss-real1.png` shows a
tight near-vertical bundle.  That difference is entirely the **clock**:

* `0x225628` drives the ring's spin from the *seconds* hand.  At 45 s the target
  is `45 * 65536/60 = 49152`, i.e. **−90°**: the ring is edge-on.  The live
  retail value is `*(short*)0x34E6C4 = **−15635** = −85.9°`, and the tilt
  `*(short*)0x34E6C6 = **−32768** = 180°`, which is `(hour%12) << 16 / 12` for
  hour 18 — both exactly what `CarouselClock()` computes.
* `ss-ours.png` was taken at a different second, i.e. tens of degrees away from
  edge-on, and near 90° the projected ring width changes very fast: the ring's
  world x extent is `20·cos(spin)`, which is 1.4 units at retail's −85.9° and
  4.6 at −76.6°.

Re-run with the clock seeded so the dumped frame lands at the same second
(`menu 18 27 43 ... 145`, which reaches 45.4 s and spin −16125) and the port's
ring is the same tight vertical bundle — `cmpcull.png`, `final-vs-retail.png`.

**And the matrix chain itself is confirmed exact.**  `0x225F80` was re-read
(`unit; mdRotZ(tilt); mdRotY(spin); mdRotZ((i<<16)/12 − 32768);
mdTranslatef(0,20,0); mdRotY(spin*4)`, `0x230180` being *get* mdTop, not push)
and `0x226028` copies mdTop into the scene's `+0x20`.  The live sorted list at
`0x34E980` holds all twelve rod records; their translations are

```
(0,±20,0)  (±0.700,±17.319,∓9.976)  (±1.213,±10.001,∓17.277)  (±1.400,0,∓19.951)
```

— all `|t| = 20.00`, and slot 5's `(0.700, 17.319, −9.976)` is reproduced by the
port's chain as `(0.714, 17.320, −9.974)`.  Nothing structural is missing.

The camera is right too: the port's `menuCamPos (10.436,0,−103)` /
`menuCamRot (0.031, 0.145, 0)` maps the front rod's tip (world `(0, 46.39, 0)`)
to camera `(−10.44, 46.36, 101.6)`, against retail's own transformed copy in the
second face bank `0x3555D0`: `(−12.38…−8.10, 46.09…46.23, 99.2…103.9)`.

---

## 4. Issue 3b — the rods' real five-pass chain, ported

HEAD drew a rod as two untargeted passes on the screen, both sampling work
buffer 3.  That is one layer of the same picture twice: soft, featureless
sticks.  `0x22D920`'s `f12 <= 0` arm (`0x22E0EC`, which all twelve rods take —
`0x226028` passes `f12 = −1.0` for every object but the front one) is five
loops, and the whole crystalline look is in the first four:

| # | ROM | target / texture | faces | emit |
|---|---|---|---|---|
| 1 | `0x22BFD0(1,0,1)` + `0x22A0C0(1,1)` | FRAME wb4, TEX wb3, field; ALPHA 0x44, ZTST ALWAYS | far (`cull==0`) | `0x22C888`, extra 0 |
| 2 | `0x22AB90(2,1,2)` + `0x22A0C0(**2**,2)` | TEXCBUMP; ALPHA **0x42 subtractive**, ZTST GEQUAL | far | `0x22C920`, ST offset `f21 + i*(gp-32032)` |
| 3 | `0x22A0C0(**0**,2)` | ALPHA **0x48 additive** | far | `0x22C920`, offset `scene->+0xB0 + f21 + i*(gp-32028)` |
| 4 | `0x22C020(1,0,1)` + `0x22A0C0(1,2)` | FRAME **the screen**, TEX wb4, field; ALPHA 0x44, ZTST **GEQUAL** | near (`cull!=0`) | `0x22C888`, extra 0 |
| 5 | `0x22BFD0(0,1,1)` + `0x22A0C0(1,2)` | FRAME wb3, TEX wb4, field | near | `0x22C888`, extra **255** (22e3c4: `li a2,255`) |

`f21` is `0x22D920`'s head: `(float)scene->+0x00 * *(gp-32056)` = **slot × 0.1**,
and `i` in passes 2/3 is the face *index*, incremented for skipped faces too
(22e1ec `mtc1 s1,f12`).  Both `(gp-32032)` and `(gp-32028)` are 0.1.

Two constants HEAD had wrong, both read out of the image:

* **the refraction centre.**  `22d9c0/22d9c4` multiply `0x22CFA8`'s outX/outY by
  `*(gp-32052) = 0.9` before any pass, exactly as `0x22D2E8` multiplies them by
  `*(gp-32064) = 0.35`.  The port passed them through unscaled — and
  `docs/menu-config.md`'s glass write-up says in as many words that the rod path
  "hands `0x22C888` outX/outY unchanged".  It does not.
* **the rod scene's emboss offset is −0.008**, not the cube's +0.01
  (`0x27EA00 = bc03126f bc03126f` in the live image), and passes 2/3 are the
  **mirror** of the cubes' order (subtractive first).

All five loops are now in `MeshDrawRod`, with `MeshBumpPass` gaining the
per-face phase step the rods need and the cubes pass as 0.

**Measured.**  Frame 145 at 18:27:43+2.4 s, `final145_480.png` against
`ss-real1.png`, 12x12-px means:

```
                     HEAD            after the chain      retail
rod, upper bundle    ( 21, 42, 62)   ( 3,142,215)         ( 20,118,156)
rod, lower bundle    ( 44, 57, 83)   (25,141,184)         ( 35, 96,118)
```

HEAD's rods are grey-blue mush; the ported chain puts them in retail's
saturated cyan band with the same internal shard structure (`cmprod.png`).
They now overshoot slightly in G/B and undershoot in R — see §8.

---

## 5. The `FRAME = wb3` question, resolved

`0x22D920` really does return with FRAME pointing at work buffer 3 (its pass 5)
and `0x226700` really does walk orbs and rods alternately with no drawenv of its
own.  **Nothing needs to restore it: every stage that draws afterwards pushes
its own FRAME first.**

```
0x226360 (orb)  -> 0x22FEC0 trail, then 0x22EFF0
                   0x22F0CC: 0x22A3B8(0x1F0A10, *(0x1F0C40), 0, *(0x27B448))
0x2267E8 (the carousel bloom, 0x2268F0's tail)
                   0x22681C: 0x22A4C8(1, 0x27EBF0, field)   clear wb4
                   0x226864: 0x22C020(1,0,0)                FRAME the screen
0x226D00 (cubes)   0x22D2E8's 0x22BF58(1,0,1)
the 2D layers      0x227560's / 0x2283A0's own 0x22A3B8
```

A whole-image caller scan for `0x22A3B8`/`0x22A4C8`/`0x22BFD0`/`0x22BF58`/
`0x22C020` finds `0x22F0CC` inside the orb draw, which is the piece that makes
the interleaving safe.  The port had no such push (nothing moved FRAME before),
so porting the rods' chain required adding it — one call at the head of
`DrawOrb`, and the main menu is still bit-identical afterwards (§7).

The one thing that *does* land in a work buffer is the orb **trail**:
`0x226360` calls `0x22FEC0` before `0x22EFF0`, and only `0x22EFF0` re-aims.  A
trail that sorts after a rod is drawn into wb3, and retail's wb3
(`gs_wb3.png`) shows exactly that — a dark torus of trail around the ring
centre that is on no screen buffer.  The port draws its trail from inside
`DrawOrb`, after the push, so it does not reproduce that; it is invisible on
the screen either way.  Noted, not ported.

---

## 6. New: the cubes were 3.5x too dark, and `0x22CD78` is why

### 6.1 The measurement

`ss-real1.png` at (85,300): retail's cube face is `(62,57,70)`; the port's, with
everything above fixed, was `(17,13,26)`; the background beside both is
`(25,19,35)` in *both* to within a level.  A flat offset of **+44/+44/+44**.

Retail's GS memory says where it comes from:

```
              screen            wb3               wb4
(160,140)  (26,19,36, 0)   (67,69,84,  0)    (  0, 0, 0,  0)   background
( 85,140)  (61,56,69,128)  (61,56,69,128)    ( 44,44,44,128)   a cube face
( 60, 90)  (33,29,44,128)  (30,26,41,128)    (  5, 5, 5,128)
(120,150)  (135,113,167,128)(142,122,176,128)( 31,31,31,128)
```

`0x226D00`'s tail adds wb4 over wb3 (`0x22C088`, additive) and then composites
wb3 onto the screen through the alpha mask, so `screen = wb3_before + wb4`.
`61 − 44 = 17`, `56 − 44 = 12`, `69 − 44 = 25` — **the port's own wb3 value**.
The chain `menuconfig.c` already had was arithmetically right; the whole
deficit was wb4.

And wb4 is not black.  It carries smooth per-face grey over every cube
(`gs_wb4.png`) — which is `0x22CD78`, the TEXCREFA spherical environment map,
the one loop the previous write-up declined to port.

### 6.2 Why it survives the black pass — the GS's AA1 rule

`docs/menu-config.md`'s last write-up says `0x22CD78` is "by the ROM's own
ordering, invisible: `0x22CA68` redraws the SAME `cull != 0` faces immediately
afterwards with `PRIM = 132`, whose ABE is clear, so the black overwrites it".
**Retail's wb4 disproves that**, and the reason is the AA1 rule: with AA1 set
the GS blends regardless of ABE, and `0x22CA68` runs under `0x22A0C0(0,3)` =
ALPHA_1 `0x48` = `Cs*As + Cd` with `Cs = 0`.  It adds no colour at all.  It is
an **alpha-only pass** — that is how it makes the mask, and it is why the
reflection is still underneath it.

The same correction applies to `0x22D2E8` pass 5: it does not "prime the cube's
area of work buffer 3 to black", it stamps `A = 0x80` there and leaves the
colour alone.

### 6.3 As ported

`0x22CD78(face, scene, aa)`, read to its `jr ra`, is per **vertex**:

```
n = sceVu0Normalize(v.cam)                          ; 0x2677E0
d = 2 * sceVu0InnerProduct(n, face.normal)          ; 0x267818 then f0+f0
if(d < 0) d = -d                                    ; 22ce80..22ceb4
r = sceVu0AddVector(n, ScaleVector(normal, d))      ; 0x267710 is ADD, 0x267050 SCALE
UV   = ((r.x + 1) * 512, (r.y + 1) * 256)           ; 0x25A368, 1/16 units
PRIM = aa ? 404 : 276     (0x22D798 passes aa = 1)
RGBAQ= scene->+0xC0 = {120,120,120,128}
XYZ2 = v.fix, no z + 1
```

`r = n + N|2(n·N)|` is `reflect(n, N)` written for a normal that faces the eye.
TEXC slot 5's descriptor `0x27F1C0 + 5*12` is `{0x01E3BCB0, 6, 6}` — a 64x64
page — and the decoder table `0x2A4BA0[5]` is `0x22A720`, **the same grey
expander slot 2 (TEXCBUMP) uses**, so one decode serves both.  The bind is
`0x22AB90(5,0,1)` (ALPHA 0x48, ZTST ALWAYS).

Result at the same probe: the port's cube face goes `(17,13,26)` →
`(64.6, 59.9, 73.4)` against retail's `(61.8, 57.0, 70.1)` — a 4 % match, with
the cubes at different spin phases so exact equality is not expected.
`cubecmp3.png` is the before/after against retail.

**Where the resource lives.**  `res.c` already has the
`{ "TEXCREFA", nil, 0, RES_COMPSUBFILE }` row and `res.h` already has
`RESID_TEXCREFA`; only the `.inc` include and the two
`resources[RESID_TEXCREFA].data/.size` lines are missing, and `res.c` belongs to
the parallel workstream.  So `menuconfig.c` includes `res/TEXCREFA_EXP.inc`
itself and uses the array directly, with a comment saying so.  **Moving those
three lines into `res.c` and calling `GetResourceData(RESID_TEXCREFA)` is the
tidy version and is a five-minute merge.**

---

## 7. Verification

Headless PCSX2 2.6.3, **software** renderer, Xvnc :99, `ee-gcc 2.9-ee-991111
-O2`, clean build, no warnings.  Logs in `logs/`.

* **Main menu bit-identical.**  `menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1`,
  whole-frame 8x8 map at frame 60, against a build of HEAD's own sources made
  in this tree: **0 of 2296 blocks differ** (`logs/final-mm60.log` vs
  `logs/head2-mm60.log`; `logs/new-mm60.log` is the same check one edit
  earlier).  Also 0 against the real tree's current `main.elf`.
  Everything new is behind the cube/carousel timers except `DrawOrb`'s FRAME
  push, which is a no-op while nothing else moves FRAME.
* **Leave path unchanged.**  `... 220 0 0 0 10 0 1 140 1` (enter 1, leave 140,
  dump 220): **0 of 2296 blocks differ** from HEAD.
* **Config screen.**  `menu 18 27 43 0 1 128 145 0 0 0 10 0 1 0 1`,
  `logs/new-cfg145.log` — tunnel, edge-on rod bundle, five cubes and all three
  text rows.  The colour side-by-side is `final-vs-retail.png`
  (`final145_480.png` is the port alone, `final145.png` the raw 640x224).
* **The RGB readback** used for every image here was a temporary extension of
  `DumpFrameAscii` (`debugFrame + 10000/20000/30000` selecting the screen /
  work buffer 4 / work buffer 3).  It is **removed from the diff**; `menu.c`'s
  only change is the eleven-line `DrawOrb` block.  If you want it back: PCSX2
  truncates each EE `printf` at ~256 bytes, so the dump has to emit ≤ 32 pixels
  (192 hex chars) per line — at 80 px/line exactly half of every line was lost
  silently.  `mkpng.py` rebuilds the PNG from the log (and has to re-join
  PCSX2's wrapped log entries first).

---

## 8. Remaining deltas on this screen

In rough order of visible effect.

1. **A 1-px dashed dark line along each cube face's tristrip diagonal**, in the
   software renderer.  Much smaller than HEAD's seams (which were the §2 bug)
   but real: a 40–64 level dip on a ~62 face, one pixel wide, dashed.
   Bisected, each with its own run (`logs/`):

   | switched off | seam |
   |---|---|
   | AA1 in `MeshEmitFace` (passes 1/7) | unchanged |
   | AA1 in `MeshEmitFlatFace` (pass 4) | unchanged |
   | the black pass `0x22CA68` (pass 5) | unchanged |
   | the TEXCBUMP emboss (passes 2/3/8) | unchanged |
   | `0x22C100` (pass 6) | unchanged — **it is a no-op in the port**, wb3's alpha is still 0 when it runs |
   | `0x22CCE8`/`0x22CB58` (pass 4, the flat far layer) | **mostly gone**, and the cubes turn much brighter |
   | `meshTex = 0` (all texturing) | **gone completely** |

   So it is not antialiasing, not the silhouette, not the emboss, and not a
   rasterisation gap (untextured cubes are clean, `wb3notex.png`); it lives in
   the textured far layer.  It is **not** pose-dependent either: re-rendered at
   retail's own cube rotation (`cubeSpin = 38628`, frame 1288, solved from
   `0x27EFB0`'s live world matrix) the cubes show retail's two-face pose and the
   lines are still there (`pose_zoom.png`).  Retail's own work buffer 3
   (`retail_wb3_zoom.png`) is clean at the same stage, so one detail of the
   far-layer path is still wrong.  **Next measurement:** a mid-frame GS dump of
   retail's wb4 (before `0x226D00` clears it) — the savestates were both taken
   after the clear, so the far glass layer is the one thing the ground truth
   here cannot show.
2. **The rods are over-cyan.**  Ours `(3,142,215)` against retail's
   `(20,118,156)`.  Two known contributors: `0x2267E8`'s two-pass additive
   carousel bloom (`0x22E428` into a cleared wb4, then wb4 over the screen at
   alpha 30, twice) is still not ported and would lift R the most; and the ring
   colour cyclers `0x225528`/`0x2255A8` are not ported, so the port uses
   `0x27EAC0`'s idle `{45,85,102}` where the live retail value at 18:27:45 is
   `{45,**96**,102}`.
3. **`0x22D920`'s `f12 > 0` arm** — the front rod splitting along Y.  Still not
   ported, and it *is* active in retail: the live ring has
   `split = 0.5373` on slot 6 (`splitMax = 1 − 27/60 = 0.55`, still growing).
   One visible shard of retail's bundle is missing from the port.  The split
   *rate* is now the ROM's `*(gp-32164) = 0.004` (it was 0.02).
4. **`0x2267E8`**, as above — also the only consumer of the second face bank
   and of `0x22E428`.
5. **`MenuConfigSetCursor()` is still unwired** (`menutext.c`'s one line);
   unchanged from HEAD.
6. **The tunnel is smoother than retail's** (issue 4).  Not chased, as asked.
   The one thing that stands out from the GS dump: retail's wb3 shows the
   un-tinted tunnel with much more mottling than the port's at the same
   `backPhase`, and the port's `DrawKabe` is the only place a scroll phase could
   differ — worth one look at `0x229358`'s T advance before anything else.

---

## 9. Corrections to `docs/menu-config.md`

Numbered against the three appended write-ups.

1. **The glass write-up §2c/§5 and the refraction write-up §4.1 both describe
   `cull == 0` as the far set and `cull != 0` as the near set.**  That is
   correct — but the *port* computed the flag inverted, and neither write-up
   states the rule as the ROM has it.  It is
   `cull = !(cross > 0)`, i.e. **1 when the cross product is ≤ 0** (§2), and
   the live face bank at `0x3529D0` is the check.
2. **The refraction write-up §3.3 — "`0x22CA68` … in `0x22D2E8` pass 5 it
   primes the cube's area of work buffer 3 to black".**  It does not.  AA1
   forces blending regardless of ABE and the pass runs under ALPHA `0x48`
   (`Cs*As + Cd`) with `Cs = 0`, so it adds nothing and only writes
   `A = 0x80`.  It is an alpha-only pass in **both** walks.
3. **The refraction write-up §3.6 / §8 — "`0x22CD78` … is also, by the ROM's own
   ordering, invisible".**  It is not: retail's work buffer 4 carries its grey
   over every cube face (§6.1), it is the cubes' only colour in that buffer, and
   `0x22C088` adds it to the frame.  Not porting it made the cubes 3.5x too
   dark.
4. **The refraction write-up §7 — "`f21` is `0x22D920`'s
   `(float)rec->slot * *(gp-32056)`"** is right, but the same section says the
   rod path "hands `0x22C888` outX/outY unchanged".  `22d9c0`/`22d9c4` scale
   both by `*(gp-32052)` = **0.9**.
5. **The refraction write-up §7 — "`0x22D920` ends with FRAME = work buffer 3
   and nothing restores it … that is a real ROM behaviour with visible
   consequences and it wants its own investigation".**  Answered (§5): the orb
   draw pushes its own FRAME at `0x22F0CC`, and so does every later stage.  The
   only thing that really lands in a work buffer is the orb *trail*
   (`0x22FEC0`, called before the push), and retail's wb3 shows it.
6. **The rod table in the refraction write-up §7, rows 4 and 5** — both run
   under `0x22A0C0(1,**2**)`, i.e. ZTST **GEQUAL**, not ALWAYS.
7. **The original §10.2 — "`0x225628`'s tilt/spin easing rate … hard-coded 0.1
   **[tnt]**".**  `*(gp-32168)` is indeed **0.1**; the tag can be dropped.  But
   the ROM's steady-state lag behind the seconds hand is bigger than the port's
   (retail spin −15635 where the target is −16384, i.e. 749 units; the port
   settles ~170 behind), so something else in `0x225628` still differs.  It is
   worth a look only because near 90° the ring's projected width is very
   sensitive to it.
8. **The original §10.2's split rate** — `*(gp-32164)` is 0.004 and the port now
   uses it.
9. **New, for `docs/menu-scene.md`**: the GS buffer map is
   `0 / 2240 / 4480(Z) / 6720(wb3) / 8960(wb4)` in blocks for a 640x224 screen,
   straight out of `0x22A198`/`0x22A290`, and the Z buffer is all zeros in a
   settled config frame — nothing on this screen writes Z.

---

## 10. Deliverables and cleanup

* `glass2.diff` — `osdbits/menu.c`, `osdbits/menuconfig.c`; `git apply -p1`
  from the repo root, checked.
* `notes.md` — this file.
* `final-vs-retail.png` — the config screen at 18:27:45, port | retail.
  `cmp145.png` (HEAD), `cmpcull.png` (after §2), `cmprod.png` (after §4),
  `cmprefl.png` (after §6) are the four steps.
* `gs_wb3.png` / `gs_wb4.png` / `gs_scr0.png` — retail's live GS buffers;
  `wb3cmp.png` / `wb4cmp.png` the port beside them.
* `ram.py`, `gsmem.py`, `mkpng.py`, `fn.sh` — the four throwaway tools
  (savestate EE RAM, savestate GS memory, log-to-PNG, function printer).
* `logs/` — every run above.  `ascii.sh` is the `config/` harness with `ELF=`
  taken from the environment so a reference build runs through the same path.
* Xvnc `:99` was started and killed by this job (it died once mid-job and was
  restarted); no `pcsx2-qt` left running; `ulimit -c 0` throughout, no cores.
  **No PCSX2 setting was changed** (no `-setting`, no ini edit) and the two
  savestates were only ever read.
* The real tree `/u/aap/src/ps2rev/osdsys` was not touched and nothing was
  committed.


---

# System Configuration screen: cube depth, orbs in work buffer 3, and the
# deferred rod-bloom stage (2026-09-01)

Companion to `docs/gscmp-FINDINGS` (the nine-dump comparison).  Everything here
was settled against three independent kinds of ground truth: the real ROM
disassembly (`objdump -D -b binary -m mips:5900 -EL --adjust-vma=0x200000
expanded.bin`), the live retail EE/GS state in savestate `20020207-164243
(00000000).04.p2s`, and PCSX2 GS dumps replayed through Sony's own software GS
model (the `libgpu2` harness).

---

## 1. The cube stage's flat Z - what it actually is

The dumps show retail drawing the entire cube stage at a constant
`z = 0xFFF010` (the black mask faces at `0xFFF011`) while this port emitted
real projected Z around 5.2M-6.2M.  The previous write-up proposed hardcoding
the constant.  **It is not a constant in the ROM, and it does not need to be
hardcoded** - it falls out of one number in the cube placement table.

### The chain

`sceVu0ViewScreenMatrix` (0x267068) builds, after the perspective divide,

    Z = m[2][2] + m[3][2]/z_cam
      = (zmin*farz - zmax*nearz)/(farz - nearz)  +  farz*nearz*(zmax - zmin)/(farz - nearz) / z_cam

Both the frame's own matrix (0x21CFD8) and the cube scene's (0x228460 ->
0x352800) are built from *identical* arguments - `scrz 512, ax/ay 0x27B44C/
0x27B450, cx/cy 2048, zmin 1, zmax 16777215, nearz 1, farz 65536`; the two
`gp`-relative zmax constants (`gp-32228` and `gp-32132`) both hold
`0x4B7FFFFF` = 16777215.0.  So `m[2][2] = -255.0039` and `m[3][2] = 16777470`
in both, and `menuViewScreen` really does serve for both scenes.

The difference is the **w** of the cube's world matrix.

`0x226D00` places each cube by calling **`0x2303E8` (mdTranslate) directly with
a pointer to the table entry at 0x27F090**, not the three-float wrapper
`0x230440` (mdTranslatef).  0x2303E8 does `ApplyMatrix(tmp, top, v)` and then
stores the **whole quadword** into the matrix's row 3 (`sq` at 0x230428) - w
included.  Every one of the five table vectors carries `w = 0`:

    cube 0  (-11.50, -11.50, 47.5, 0)
    cube 1  (-22.50,  -5.50, 47.5, 0)
    cube 2  (-10.75,  -0.25, 47.5, 0)
    cube 3  (-21.75,   6.00, 47.5, 0)
    cube 4  (-11.25,  11.50, 47.5, 0)

(`res/MENUGEOM.inc` has already been carrying those zeros; the port threw them
away by going through `mdTranslatef`, which forces `w = 1`.)

With `world[3][3] = 0` the camera-space w of every cube vertex is 0, so
0x22CFA8's second transform gives

    proj.z = m[2][2]*z_cam + m[3][2]*0 = -255.0039 * z_cam
    proj.w = z_cam

and the divide leaves `proj.z = m[2][2] = -255.0039` for **every vertex of
every cube**.  `sceVu0FTOI4` (0x267668, a bare `vftoi4`) turns that into
`-4080 = 0xFFFFF010`; the black pass's `+1` gives `0xFFFFF011`.  A dump decoder
that masks the GS Z field to 24 bits prints those as 16773136 / 16773137.

x and y are untouched, because `m[3][0] = m[3][1] = 0` in the view-screen
matrix - which is exactly why this port's cubes were in the right place and at
the right size but at the wrong depth.

### Verified

Live retail face bank at 0x3529D0 (savestate `.04.p2s`, face record stride
352, vertex stride 80: `+0x00` cam, `+0x10` uv, `+0x20` proj, `+0x30` the
`vftoi4` triple, `+0x40` q):

    f0 v0  cam (-8.6401, 14.1428, 50.1668, 0.0000)  proj (1959.82, 2115.84, -255.0039, 1.0)  fix z -4080
    f0 v1  cam (-13.8629, 14.6318, 49.5670, 0.0000) proj (1904.80, 2119.03, -255.0039, 1.0)  fix z -4080
    f1 v0  cam (-9.1858, 8.9140, 50.6559, 0.0000)   proj (1955.16, 2090.35, -255.0038, 1.0)  fix z -4080

- `cam.w = 0` on every vertex; camera-space z legitimately varies 44.3..50.7.
- The rod scene's world matrix (0x27E950+0x20) has `row3 = (..., 1)` and its
  vertices keep real per-vertex Z.  **Rods must not be changed.**

After the fix, a one-shot debug print of the first face's vertex 0 at the debug
frame (removed again before the diff) gives:

    rods   camw 1.0000  projz 113648.8 .. 274767.4   fix z 1818380 .. 4396278
    cubes  camw 0.0000  projz -255.0039              fix z -4080  (= 0xFFFFF010)

i.e. bit-identical to retail, with the rods untouched.

**Fix:** add `mdTranslate(const float *v4)` (the real 0x2303E8) next to
`mdTranslatef` (the real 0x230440, which is 0x2303E8 with w = 1), and call it
with `menuCubePos[i]` in both cube walks.

---

## 2. ZBUF: PSMZ32 and, more importantly, ZMSK = 0

Two ZBUF divergences, found by counting every ZBUF write in both dumps:

    retail:  416 x  "zbp=4480 psm=0"                (PSMZ32, ZMSK clear)
    ours:      8 x  "zbp=4480 psm=0"
               4 x  "zbp=4480 psm=1"                (PSMZ24)
             124 x  "zbp=4480 psm=1 ZMSK"           (PSMZ24 + Z writes masked)

**The ROM never masks Z anywhere in the menu.**  0x22BF58 / 0x22BFD0 /
0x22A4C8 / 0x22A3B8 push FRAME and ZBUF together and the ZBUF they push always
has ZMSK = 0.  `vif1SetZWrite(0)` in the mesh paths was an addition of ours.

The psm is also wrong: `main.c`'s `sceGsSetDefDBuff` allocates the Z buffer as
**PSMZ32**, and every retail ZBUF write agrees, but `vif1SetZWrite` was writing
PSMZ24.  That was harmless while every Z in this port fitted in 24 bits; with
the cube stage now emitting `0xFFFFF010` it is not (PSMZ24 masks the value on
both the write and the compare).

### Why ZMSK matters: the AA1 crack repair needs BOTH halves

Measured in the libgpu2 replay harness on the wb4 alpha mask (the mask the
cube stage composites by), classifying non-{0, 0x80} alpha pixels into
*interior* (all four neighbours non-zero - a crack inside a face) and
*silhouette* (unavoidable AA on the outline):

| stream | solid 0x80 | interior cracks | silhouette |
|---|---|---|---|
| retail, as dumped | 11377 | **0** | 960 |
| ours, as dumped | 11434 | **1020** | 1017 |
| retail, cube-stage Z forced down to ours (0x55AECD) | 10404 | **941** | 984 |
| ours, cube-stage Z forced up to 0xFFF010 | 11434 | **1020** | 1017 |

Rows 3 and 4 are the whole story:

- Giving **retail** our Z reproduces our artifact exactly (0 -> 941 interior
  cracks).  So the flat near-max Z genuinely is what keeps retail's mask solid
  - Finding 1's mechanism is confirmed, not just correlated.
- Giving **ours** retail's Z fixes nothing, because our ZBUF has ZMSK = 1.
  With Z writes masked, nothing is ever stored, every primitive compares
  against whatever the pre-cube scene left, and the repair cannot happen at
  all.  Retail's AA1 partial-coverage pixels skip the Z write while fully
  covered ones store the flat Z, so the *next* primitive over a crack pixel
  wins its depth test against the stale value and re-blends the coverage up to
  solid.

So the flat Z is necessary but not sufficient: **both** the `w = 0` translate
and `ZMSK = 0` are required.  A bisect with the harness's `-x` knob also showed
the crack pixels are written by the black pass (0x22CA68), not the reflection
pass - masking cube 1's whole reflection pass left the final mask
byte-identical, i.e. the black pass overwrites everything the reflection pass
put down.

**Fix:** `vif1SetZWrite` now writes `SCE_GS_PSMZ32`, and `MeshDrawCube` /
`MeshDrawCubeMask` call `vif1SetZWrite(1)` (ZMSK 0).

**Left alone deliberately:** `MeshDrawRod` and every other menu call site still
carry `vif1SetZWrite(0)`.  Retail has ZMSK = 0 there too, so those are also
divergences, but the rod path draws straight to the visible buffer and changing
its depth behaviour is a separate, separately-verifiable change.  Worth doing
next; see "open" below.

---

## 3. The orbs were missing from work buffer 3

0x22EFF0 draws every orb **twice** - once on the visible buffer and once,
identically placed, into work buffer 3 - and the port only had the first walk.
Retail's per-orb pattern in the dumps is `LINESTRIP+AA1 x1 + SPRITE+TME+ABE+FST
x2` to FB = 0/2240 immediately followed by the same three to FB = 6720,
interleaved per orb with the rods.

Work buffer 3 feeds the frame-start tint, the rods' passes 1 and 5, and the
whole cube chain, so without the twin everything that refracts the scene
refracted a version of it with no orbs in it.

The second walk begins with `0x22F798: 0x22A4C8(0, NULL, *(0x27B448))` - FRAME
= work buffer 3, no clear, the module's own field snapshot (the same half pixel
the screen walk carries; this is a mesh-style draw, not a buffer-to-buffer
blit).

The two walks are **not** identical.  Both sprite colour records differ:

| | halo (TEXCBLUR, tbp 11840) | core (TEXCNAVI, tbp 11776) |
|---|---|---|
| walk 1 (screen) | the orb's own trail colour `{0x30,0x62,0x80,0x3C}` | `*0x27F930 = {0x80,0x80,0x80,0x80}` |
| walk 2 (wb3) | the same RGB with **alpha 0x80** | `*0x27F940 = {0xFF,0xFF,0xFF,0x80}` |

Confirmed both in the ROM and per-vertex in the dump (`rgba=3062803c` /
`80808080` on the screen, `30628080` / `ffffff80` into wb3).

The halo's alpha is not a separate record: 0x22EFF0 keeps the colour on its own
stack at sp+48 and the sprite code overwrites the quadword's fourth word with
`TimerInterp(orbTrailTimer, <that word>)` each time it draws (0x22F54C/
0x22F558), so walk 1 feeds it the orb's 0x3C while walk 2's head
(0x22F788/0x22F794: `li v1,128; sw v1,60(sp)`) resets it to 128 first.  With the
trail timer open both come out unchanged, i.e. exactly the port's `baseAlpha`.

**Fix:** `DrawOrb` split into `DrawOrbPass(o, zscale, baseAlpha, blurCol,
coreCol)` called twice, with `MenuBackWorkTarget(0, 0, MenuBackField())` between
them, plus `orbCoreColorWork = {0xFF,0xFF,0xFF,0x80}` (the real 0x27F940).

`MenuFrame` also re-aims FRAME at the screen after `SceneWalk()`, because the
last orb now leaves it on work buffer 3 (as the last rod already did).  The ROM
gets this from 0x2267E8's own FRAME push, which this port still defers; without
it, an `argv[10]` that drops the transition phase to 5 makes `ZoomBlur` run zero
passes and the 2D text would land in a work buffer.

---

## 4. The "full-screen distortion stage" is 0x2267E8, the deferred rod bloom

FINDINGS called this "2 x 86 quads, full-screen bbox, two unknown textures
(tbp 11200 and 11584), very likely the TEXCKABE tunnel family".  It is none of
those things.  It is **0x2267E8**, the two-pass flush that
`menu.c`'s `SceneWalk` comment already says is deferred, and its 86 quads are
small and scattered - the bbox is full-screen only because the rods are.

`86` is exactly the sum of the twelve carousel rods' *near*-face counts in that
frame (5+5+5+7+7+7+8+6+8+9+8+11), which is what identified it.

### The two unknown textures

Both were resolved by dumping the retail dump's VRAM and matching the decoded
texels against the live TEXC slot table at **0x27F1C0** (stride 12, first word =
the decoded texel buffer):

- **tbp 11200 = TEXC slot 0 = `TEXCFLOW`** - 64x64, greyscale, alpha 0x7F
  throughout (the same `b | b<<8 | b<<16 | 0x7F000000` decoder as TEXCBUMP).
- **tbp 11584 = TEXC slot 3 = `TEXCBINV`** - 64x64, and it is the **exact
  bitwise complement of TEXCBUMP**: comparing the two VRAM images gives 0 RGB
  mismatches and 0 alpha mismatches over all 4096 pixels.

Slot numbering is `slot = <index in the TEXC resource group> - 4`, which the
known bindings confirm: slot 1 = TEXCKABE, 2 = TEXCBUMP, 5 = TEXCREFA,
6 = TEXCNAVI, 7 = TEXCBLUR.

The VRAM map in blocks is therefore
`11200 TEXCFLOW, 11264 TEXCKABE (128x128), 11520 TEXCBUMP, 11584 TEXCBINV,
11712 TEXCREFA, 11776 TEXCNAVI, 11840 TEXCBLUR`.

`tools/extract-res.py` already extracts both (`TEXCFLOW 4556 -> 4096`,
`TEXCBINV 4452 -> 4096`); only TEXCFLOW needs adding to `res/` and `res.c`,
because TEXCBINV can be produced in `DecodeBump()` as `~TEXCBUMP`.

### The stage, from the disassembly

`0x2268F0` ends with `j 0x2267E8`, immediately after `0x226700` (the sorted
walk).  0x2267E8 is:

    0x22A4C8(1, 0x27EBF0, field)      ; FRAME = wb4, CLEAR to {0,0,0,0x80}
    for each record with type != 1:   ; meshes only, orbs skipped
        0x22E428(rec+0x10, f12 = rec->0xF4 (split), a1 = 0, a2 = rec+0x120)
    0x22C020(1, 0, 0)                 ; TEX = wb4, FRAME = screen, NO half pixel
    0x226768(30)                      ; full-screen additive composite, alpha 30
    0x22A4C8(1, 0x27EBF0, field)      ; again
    for each record with type != 1:
        0x22E428(..., a1 = 1, ...)
    0x22C020(1, 0, 0)
    0x226768(30)

`0x22E428`'s `a1` selects the walk; `0x22E428` branches on `split > 0` into a
split-rod arm (not needed - the port only has the one-piece rod) and, at
**0x22E9A8**, the simple arm.  Guarded by `0 <= a1 < 2`, the simple arm is:

    0x22CFA8(...)                     ; the same MeshTransform, scene = rec+0x10
    outX *= 0.9; outY *= 0.9          ; *(gp-32020); unused by both emits here
    f21 = (float)rec->index * 0.1     ; *(gp-32024)

    ; pass A, both walks
    0x22AB90(0, 0, 2)                 ; TEXCFLOW
    0x22A0C0(1, 1)                    ; ALPHA_1 0x44, ZTST ALWAYS
    for each face with cull != 0:     ; NEAR faces
        0x22CD78(face, scene, aa = 0) ; PRIM 276 (TRISTRIP|TME|FST, no AA1, no
                                      ; ABE) - the same spherical env map the
                                      ; cube mask walk uses, colour scene+0xC0
    0x22ED10                          ; flush

    ; pass B
    if(a1 == 1)  0x22AB90(2, 1, 2)    ; TEXCBUMP,
                 ST offset = (scene+0xB0 + i*0.1, scene+0xB4 + i*0.1)
    else         0x22AB90(3, 1, 1)    ; TEXCBINV,
                 ST offset = (f21 + i*0.1, f21 + i*0.1)
    0x22A0C0(2, 1)                    ; ALPHA_1 0x42 SUBTRACTIVE, ZTST ALWAYS
    for each face with cull != 0:
        0x22C920(face, scene+0xD0)    ; PRIM 84, colour {0x28,0x28,0x28,0x80}
    0x22ED10

Cross-checked against the dump, which shows exactly this and settles which
texture goes with which walk:

    walk a1 = 0:  TEX0 tbp=11200 (ALPHA 0101) x N faces, then tbp=11584 (ALPHA 2001) x N
    walk a1 = 1:  TEX0 tbp=11200 (ALPHA 0101) x N faces, then tbp=11520 (ALPHA 2001) x N

So it is the classic emboss pair - TEXCBUMP and its exact complement TEXCBINV,
one per walk, subtractively over an environment-mapped rod - and the reason the
two walks look almost identical in a segment summary.

Constants read out of the ROM:

    0x27EBF0  the wb4 clear record       {0, 0, 0, 0x80}
    scene+0xC0                            the record's colA (dump: {0x3c,0x3c,0x3c,0x80})
    scene+0xD0                            {0x28, 0x28, 0x28, 0x80}  (static)
    scene+0xB0/+0xB4                      -0.008 (the rod scene's own bump offset)
    gp-32020 = 0.9, gp-32024 = 0.1, gp-32000 = 0.1, gp-31996 = 0.1

`0x226768(a)` patches the sprite record at **0x27EBB0** - colour
`{0x80,0x80,0x80, a}`, `x0/y0 = 0,0`, `u0/v0 = 8,8`, extents `(w<<4, h<<4)` and
`((w<<4)+8, (h<<4)+8)`, `+0x34 = 1` (ABE) and `+0x38 = 1` (TME) - then does
`0x22A0C0(0, 1)` (ALPHA_1 0x48, additive, ZTST ALWAYS) and draws it via
0x2299C0.  In the dump: `SPRITE+TME+ABE+FST`, full screen, `uv (0,0)-(640,224)`,
`rgba = 8080801e` (alpha 30 = the argument).

### To port it

1. `tools/extract-res.py <bios.bin> osdbits/res` already writes
   `TEXCFLOW_EXP.inc`; add the `#include` and the `{ "TEXCFLOW", ... }` row to
   `res.c` and `RESID_TEXCFLOW` is already in `res.h`.
2. In `menuconfig.c`'s `DecodeBump`, decode TEXCFLOW like TEXCBUMP/TEXCREFA,
   and build TEXCBINV as `~bumpTexels` (RGB complement, alpha kept at 0x7F) -
   no new resource.  Add `MenuConfigBindFlow()` / `MenuConfigBindBinv()`
   alongside the existing binders.
3. Give `MeshEmitReflFace` an `aa` parameter (PRIM `aa ? 404 : 276`); the cube
   mask walk passes 1, this stage passes 0.  `MeshReflPass` gains the same.
4. Add `MenuConfigFlushMesh(SceneRec *rec, int walk)` implementing 0x22E428's
   simple arm above, and a `SceneFlush()` in `menu.c` implementing 0x2267E8,
   called from `MenuFrame` right after `SceneWalk()` (replacing the explicit
   screen-target restore added in section 3, whose job 0x2267E8 does).
5. `menuback.c`'s `BlurBlit` is the right shape for `0x226768` but hardcodes
   RGBAQ 0x80808080 - it needs an alpha argument (or a small sibling).
6. `MenuBackWorkTarget(1, 1, field)` already models `0x22A4C8(1, rec, field)`,
   but `WorkClear` writes `{0,0,0,0}`; this stage's record 0x27EBF0 is
   `{0,0,0,0x80}`, so the clear colour has to become a parameter.

Not implemented in this pass - see "open".

---

## 5. What changed, and how it was verified

`gsfix.diff` (applies cleanly to the tree at `d68fbdf`):

    osdbits/inc.h          3 +      declare mdTranslate
    osdbits/menu.c       144 +-     mdTranslate/mdTranslatef split; DrawOrbPass;
                                    the wb3 orb twin; FRAME restore after SceneWalk
    osdbits/menuconfig.c  34 +-     mdTranslate(menuCubePos[i]) in both cube walks;
                                    vif1SetZWrite(1) in the two cube drawers
    osdbits/opening.c     10 +      vif1SetZWrite -> PSMZ32

Verification (windowless PCSX2 on Xvnc :97, `DumpFrameAscii` 8x8 luminance maps
extracted with `grep -o '|.*|'`):

- **Cube Z, direct:** a temporary debug print of the first face's vertex 0 at
  the debug frame gave `camw 0.0000 projz -255.0039 fix z -4080` for all five
  cubes and `camw 1.0000 projz 113648..274767` for all twelve rods - retail's
  values exactly, rods untouched.  Print removed before the diff.
- **Main-menu regression** (`menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1`): the
  ASCII map is **byte-identical** to an unmodified baseline build.  Expected:
  on the main menu nothing samples work buffer 3 after the scene walk, so the
  orb twin has no visible consumer there, and there are no cubes.
- **Config screen** (`menu 18 27 45 0 1 128 145 0 0 0 10 0 1 0 1`): the map
  changes only in the cube/clock region and only by getting brighter.  Four
  builds were compared to split the credit:

      baseline                     9c736bd4672afb2442e09bb37f0b1d61
      orb wb3 twin only            06e637469a057639728d2650d0b9fd2d
      + cube flat Z                0524efab56a7823ab6db207447a39fb7
      + ZMSK 0 / PSMZ32 (final)    (differs again, in the cube columns)

  Almost all of the visible change is the orbs reaching work buffer 3 - exactly
  what Finding 2 predicted (`.:+*#######` -> `.-+#%%%####` across the clock).
  The flat Z alone moves one 8x8 block, because a 1 px seam barely survives a
  block-max luminance map.  Adding ZMSK = 0 changes the five cube columns again
  (`-%%` -> `-@@`, `+%@@@*` -> `+%@@@%`), which is the masked composite finally
  replacing the frame solidly inside the cubes instead of leaking the dark
  background through the seams.  The changed block columns (9-10 and 24-25)
  match the cubes' own projected centres from the mesh debug print
  (screen x 78, 86, 196, 199, 204).
- **The cracks themselves** were measured in the libgpu2 replay harness rather
  than on screen, because 1 px seams do not survive the ASCII map.  See the
  table in section 2: forcing our Z into the retail stream reproduces our
  artifact (0 -> 941 interior crack pixels), which is the pixel-exact proof of
  the mechanism.

### Open

- **Pixel proof of the fixed build.**  The harness needs a PCSX2 GS dump, and
  PCSX2's dump trigger is a hotkey - not reachable from the headless recipe.
  A fresh shift+F8 dump of the fixed build replayed through
  `gsreplay <dir> -e draw:<mask-walk-end> -s ...` and scored with the
  interior/silhouette classifier should read `interior = 0`; that is the one
  outstanding check for sections 1 and 2.
- **ZMSK on the rod path and elsewhere.**  Retail has ZMSK = 0 for all 416 of
  its ZBUF writes; we still set it everywhere except the two cube drawers.  The
  rod path is the next one worth changing, but it draws to the visible buffer,
  so it wants its own before/after run.
- **Section 4 (0x2267E8) is specified but not implemented.**  It is a real,
  visible stage (two full-screen additive composites at alpha 30 over an
  embossed environment map of the rods) and its absence is why the retail clock
  has a glow this port does not.
- **The refraction-pass ALPHA in the cube mask walk.**  Retail runs 0x22CD78
  there under `ALPHA_1 0x44`; we use `0x48`.  It does not affect the mask (the
  black pass overwrites it) but it does affect the colour the reflection
  contributes, so it is worth aligning.


---

# The System Configuration clock's entry/exit animation

Symptom (aap): in the port the twelve-rod clock carousel visibly **grows in**
on entry and **shrinks out** on exit; on retail it just fades in.

Result: root-caused to a single mis-identified gp-relative global, fixed with a
21-line behavioural change in `osdbits/menuconfig.c` (`clockanim.diff`).
Everything below is from the ROM disassembly plus live retail memory; nothing
is guessed.

## 1. The rod scale really is the carousel's progress (not the bug)

Confirmed independently of the parent session's trace:

* `0x226028` (the ring emitter) loads `ring[slot].progress` from `+0x10` and
  stores it into the scene struct's `+0x6C` — `0x2260b4: lwc1 $f0,16(s1)` /
  `0x2260bc: swc1 $f0,108(s4)`, `s4 = 0x27E950`.
* `0x22D920` (the rod draw) reloads `+0x6C` at `0x22d974` (the `< 0` skip) and
  at `0x22da6c`/`0x22daa8` (the split arm's two halves get `progress*split` and
  `progress*(1-split)`), and `0x22CFA8` consumes it as the Y scale.
* `0x225BF8` (stage 10) recomputes it every frame:
  `progress = cfgInterp(carouselTimer, 128) * 0x3C000000f` (= `*1/128`) —
  `0x225c9c: jal 0x22AC20` with `a1 = 128`, `0x225cb4: mul.s $f0,$f0,$f20`.

So the ramp exists in retail too and must **not** be removed. What differs is
how long it lasts.

## 2. Root cause: `gp-30380` is NOT `dur80`

The port's `InitMenuConfig` had

```c
cfgDur80 = rate*80/60;                  /* real: *(gp-30380) */
carouselTimer.duration = cfgDur80;      /* real: *(gp-30372), 0x225998 */
```

Both mappings are wrong. `gp = 0x2AF070`, so the four words in play are
`0x2A79B0` (gp-30400), `0x2A79B4` (gp-30396), `0x2A79C4` (gp-30380),
`0x2A79C8` (gp-30376) and `0x2A79CC` (gp-30372). Two *different* initialisers
write them, and `0x21CE58` (the module init) calls them in this order:

| addr | writes | value |
|---|---|---|
| `0x22AD7C` (in `0x22AD38`) | `gp-30380` | `rate*40/60` — a **second** forty-frame leg |
| `0x22ADA4` (in `0x22AD38`) | `gp-30372` | `li v0,1` — the literal **1** |
| `0x22ADC4` (in `0x22AD38`) | `gp-30376` | `rate*80/60` — the only real "dur80"; it is `0x27F620`'s duration, which no config-screen timer uses |
| `0x22850C` (in `0x228460`) | `gp-30400` | `rate*40/60` (dur40) |
| `0x228568` (in `0x228460`) | `gp-30396` | `rate/6` (dur10) |

`0x225998` then does `0x2259B4: sw v1,-5376(v0)` with `v0 = 0x280000` and
`v1 = *(gp-30372)`, i.e. **`0x27EB00.duration = 1`**. With a duration of 1 the
timer's very first step (`0x22ACC0`) takes `count` straight from 0 to 1 = the
duration, so `progress` goes 0 → 1.0 in a **single frame**. Retail's rods are
at full height the first time the emitter's `progress > 0.05` gate
(`0x226914`, `*(gp-32148)`) lets them through: the growth is real but it is
never on screen for more than one frame.

The port had given that timer 80 frames, which is the 80-frame grow-in aap saw.

### Live retail confirmation

`eeMemory.bin` from `20020207-164243 (00000000).04.p2s` and `.05.p2s`
(both retail BIOS, NTSC, sitting on the config screen), read at the exact
addresses above:

```
gp-30400 dur40 @0x2A79B0 = 40      anim   0x27BE44 = [90, 90, 0, 2]
gp-30396 dur10 @0x2A79B4 = 10      carous 0x27EB00 = [ 1,  1, 0, 2]
gp-30380       @0x2A79C4 = 40      cube   0x27EC00 = [40, 40, 0, 2]
gp-30376       @0x2A79C8 = 80      backfd 0x27F190 = [40, 40, 0, 2]
gp-30372       @0x2A79CC = 1       (struct = duration,count,edge,state)
```

The Anim's duration is **90** (`= 40+40+10`, `0x227290`), not the port's 130,
and the carousel's is **1**. `menuback.c` already had `gp-30380` right
(`bgTimer.duration = 40`); only `menuconfig.c` was wrong.

## 3. The exit: `0x225B68`, and the state machine's staggered close

`0x225B68` is the carousel timer's only closer. Besides `0x22AC90` it walks the
ring and writes `0` into every slot's `+0x10` by hand (`0x225be0: sw zero,16(v0)`)
— retail deliberately snaps the rods away instead of letting them ramp down.

It is called from exactly one place: `0x22749C`, the **last** edge of
`0x227390`'s closing arm. The whole machine (run from `0x227DE8` immediately
after the Anim's own step, `0x227DF4` → `0x227DFC`):

```
opening (state 1)  count == *(gp-30380) (40)      -> 0x226B28  open cube timer
closing (state 3)  duration-count == dur10 (10)   -> 0x226B70  close cube timer
closing            count == gp-30380+dur40 (80)   -> 0x22AE80  reopen 0x27F620 (not ported)
closing      else  count == *(gp-30380) (40)      -> 0x229230  close backdrop fade
closing      else  count == *(gp-30372) (1)       -> 0x225B68  close carousel
```

(the last three are a real `else if` chain in the ROM, and are gated on
`0x223790`'s timer — `0x27C258`, the module-level screen state — being idle;
the port has no such timer so that gate is dropped.)

So on the way out retail keeps the clock at full height for **89 of the 90**
exit frames and then drops it in one. The port instead closed the carousel,
the cube timer and the backdrop fade all at once from `MenuLeaveConfig`, which
with an 80-frame carousel duration produced the 80-frame shrink-out.

## 4. What changed (`clockanim.diff`, `osdbits/menuconfig.c` only)

1. `cfgDur80` → `cfgDur40b = rate*40/60` (= `*(gp-30380)`), and a new
   `cfgCarouselDur = 1` (= `*(gp-30372)`). This fixes `carouselTimer.duration`
   (the actual bug), `cfgAnim.duration` (130 → 90) and `MenuConfigAlpha`'s
   threshold (`count-120` → `count-80`, i.e. the config item list's fade-up is
   the last dur10 frames of a 90-frame Anim, as in retail).
2. New `CarouselClose()` = `0x225B68`.
3. New `MenuConfigStateMachine()` = `0x227390`, called from `MenuConfigStep()`
   right after the Anim's step, exactly as `0x227DE8` does. This removes the
   previously-documented cube-timer divergence for free (the cubes now start
   growing at Anim count 40 rather than at frame 0).
4. `MenuEnterConfig` no longer opens the cube timer; `MenuLeaveConfig` is now
   just `cfgClose(&cfgAnim)` (retail's `0x227C20` TRIANGLE arm) — everything
   else closes on its own edge.

Ordering is preserved: `MenuConfigStep()` (state machine) runs before
`MenuConfigCarousel()` (stage 10) in `MenuFrame()`, the same as
`0x2283F0`'s sixth slot running before `0x225BF8`.

## 5. Evidence

`menuconfig.c`'s existing `MeshDebug()` prints one line per drawn mesh on the
frame given by `argv[6]`, including the Y scale it was handed — so the entry
envelope can be read straight out of the log. Entry argv
`menu 18 27 45 0 1 128 N 0 0 0 10 0 1 0 1` (config entered at frame 1):

| dump frame N | pristine rods | pristine cubes | fixed rods | fixed cubes |
|---|---|---|---|---|
| 2  | *(none — under the 0.05 gate)* | 5 @ 0.05 | **12 @ 1.00** | none |
| 3  | none | 5 @ 0.08 | 12 @ 1.00 | none |
| 5  | none | 5 @ 0.12 | 12 @ 1.00 | none |
| 10 | 12 @ **0.11** | 5 @ 0.25 | 12 @ 1.00 | none |
| 20 | 12 @ **0.23** | 5 @ 0.50 | 12 @ 1.00 | none |
| 40 | 12 @ **0.48** | 5 @ 1.00 | 12 @ 1.00 | none |
| 41 | 12 @ 0.50 | 5 @ 1.00 | 12 @ 1.00 | **5 @ 0.03** |
| 60 | 12 @ **0.73** | 5 @ 1.00 | 12 @ 1.00 | 5 @ 0.50 |
| 90 | 12 @ 1.00 | 5 @ 1.00 | 12 @ 1.00 | 5 @ 1.00 |

The fixed build's rods are at 1.00 on the first frame they exist and never
move; the cubes now hold off until Anim count 40 and reach full size at
count 80, i.e. the retail staging.

Exit. Fixed build, leave at frame 100 (`argv[13]=100`, Anim in state 2 by then):

| N | 105 | 110 | 150 | 185 | 188 | 189 | 190 |
|---|---|---|---|---|---|---|---|
| rods  | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | gone | gone |
| cubes | 1.00 | 0.98 | gone | gone | gone | gone | gone |

(cube timer closes 10 frames into the close, backdrop fade at count 40 = frame
150, carousel at count 1 = frame 188 → rods gone on 189.)

Pristine build for the same shape — it needs `leave 140` because its Anim is
130 frames long, so a leave at 100 is silently ignored (`state != 2`), which is
itself a symptom of the wrong duration:

| N | 145 | 160 | 180 | 200 | 218 |
|---|---|---|---|---|---|
| rods | **0.94** | **0.75** | **0.50** | **0.25** | gone |

That is the shrink-out aap reported, measured.

ASCII maps confirm it visually too (`grep -o '|.*|'`): at N=10 the pristine
build shows the five bright cube blobs on the left and almost nothing in the
ring, the fixed build shows no cubes and a full-height rod column through the
middle rows.

### Regression

Main menu, `menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1`, pristine vs fixed:
the 28-line ASCII frame map is **byte-identical**, and every `osdsys:`/`mesh `
program line is identical. (The only log differences are environmental — ELF
size/CRC, mmap addresses, host path, and the pad handshake's `state 5`/`state 7`
ordering, which is a wall-clock artefact of the IOP RPC and is unrelated.)
This is expected: `cfgAnim` is in state 0 on the main menu, so
`MenuConfigAlpha` returns 0 either way and the state machine returns
immediately.

## 6. Two things worth recording

* **Retail has no alpha envelope on the rods.** `0x22C4E0` writes the RGBAQ
  alpha as a hard-coded `0x80` (`t2 = 0x8000 << 16`, `0x22c6a8: or v0,v0,t2`);
  the RGB is `ring[slot].col0` (scene `+0x80`) plus a Fresnel rim term
  `trunc(scene->+0x90 * 10 * (1-|dot|)^4)` plus the pass's `extra` (0 or 255).
  Nothing there is enveloped by the entry. So the "fade" aap sees is not the
  rods' own alpha — it is the glass being nearly invisible until the backdrop
  behind it brightens: `MenuBackFadeOpen` (`0x2291E8`) starts `0x27F190`, whose
  40-frame ramp feeds `bgFade0/1/2` (gp-28840/-28836/-28832) into every
  TEXCKABE vertex colour, and the rods refract that live picture. No extra
  mechanism is needed or was added.
* **`0x22AE80`/`0x22AEC8` (`0x27F620`, duration 80 = gp-30376) are still not
  ported.** The entry closes that timer and the state machine reopens it at
  Anim count 80 on the way out. It is the main-menu screen's own timer; nothing
  in the port reads it, so the corresponding arm of the state machine is an
  empty statement with the ROM address in a comment.


---

# The deferred rod bloom (0x2267E8) ported, plus XYZF2 and the ZMSK audit
# (2026-09-01, second pass)

Implements section 4 of the previous write-up ("the deferred rod-bloom
stage") and closes the two register-fidelity items it left open.  Ground
truth throughout: the ROM disassembly (`objdump -D -b binary -m mips:5900
-EL --adjust-vma=0x200000 expanded.bin`), the retail live state in
savestate `20020207-164243 (00000000).04.p2s`, and the decoded retail GS
dump `retail614.log` / `retail614v.log`.

---

## 1. What 0x2267E8 actually is, confirmed instruction by instruction

The previous pass identified the stage and left a six-step plan.  Every
constant in it was re-read from the image before use; the plan was right
except for one argument (noted in 1.3).

`0x2268F0` ends with `j 0x2267E8` right after the sorted walk 0x226700, so
this is the tail of the object-list stage and it runs on **every** screen.
Written out longhand in the ROM (not looped), it is:

    0x2267e8  0x22A4C8(1, 0x27EBF0, *(0x27B448))   FRAME wb4, CLEAR, field
    0x226824  for rec = *(0x34E980); rec; rec = rec->next:
    0x226838      if(rec->0xF0 == 1) continue          ; orbs skipped
    0x226848      0x22E428(rec+0x10, 0, rec->0xF4, rec+0x120)
    0x226864  0x22C020(1, 0, 0)                    TEX wb4, FRAME screen,
                                                   NO half pixel
    0x22686c  0x226768(30)
    0x226880  0x22A4C8(1, 0x27EBF0, *(0x27B448))   ... and again, with
    0x2268a8      0x22E428(..., **1**, ...)            walk = 1
    0x2268c4  0x22C020(1, 0, 0)
    0x2268e8  j 0x226768   (a0 = 30)

`0x22E428` branches on the record's split (+0xF4): `> 0` takes the
split-rod arm at 0x22E4D0, everything else the **simple arm at 0x22E9A8**.
This port has no split rod (neither does `MeshDrawRod`), so all twelve rods
come through the simple arm.

### 1.1 The simple arm, 0x22E9A8

Guarded by `0 <= walk < 2` (22e9a8 `slti v0,s8,2` / 22e9b4 `bltz s8`); an
out-of-range walk falls through to 0x22EB98, a lone pass A that nothing in
the image calls.  Before the arm, 0x22E428's head does

    22e484  if(scene->+0x6C < 0) return             ; the fly-in progress
    22e490  f21 = (float)scene->+0x00 * *(gp-32024) ; slot * 0.1
    22e4ac  0x22CFA8(&outX, &outY, &outZ, 0x3529D0, scene)   the transform
    22e4c4  outX *= *(gp-32020) = 0.9; outY *= 0.9

and then

    ; pass A, identical in both walks
    0x22AB90(0, 0, 2)                   TEXCFLOW
    0x22A0C0(1, 1)                      ALPHA_1 0x44, ZTST **ALWAYS**
    for each face with cull != 0:
        0x22CD78(face, scene, aa = 0)   PRIM **276** - TRISTRIP|TME|FST,
                                        no AA1, no ABE; RGBAQ = scene+0xC0
    0x22ED10                            flush

    ; pass B
    walk 1: 0x22AB90(2, 1, 2)   TEXCBUMP, ST offset f21 + i*0.1 + scene->0xB0
    walk 0: 0x22AB90(3, 1, 2)   TEXCBINV, ST offset f21 + i*0.1 + 0
    0x22A0C0(2, 1)                      ALPHA_1 0x42 SUBTRACTIVE, ZTST ALWAYS
    for each face with cull != 0:
        0x22C920(face, scene+0xD0)      PRIM 84; RGBAQ {0x28,0x28,0x28,0x80}
    0x22ED10

`i` in the ST offset counts **every** face index, culled ones included
(22eaac `mtc1 s2,f0` where s2 is the outer counter), exactly as 0x22E0EC
does for the visible rod.  The two per-face steps are `*(gp-32000)` and
`*(gp-31996)`, both 0.1.

### 1.2 Constants, all re-read from the image

| where | value | how |
|---|---|---|
| `0x27EBF0` | `{0, 0, 0, **0x80**}` | the wb4 clear record (0x226800/0x226810 build the pointer; bytes at 0x27EBF0) |
| `0x27EBB0` | `{0x80,0x80,0x80, A}`, x0/y0 = 0, u0/v0 = 8, +0x34 = 1 (ABE), +0x38 = 1 (TME) | 0x226768's sprite record; A patched from a0 at 0x2267B8 |
| `scene+0xC0` | the ring slot's own `col1` | 0x226028 at 0x226114/0x22611C copies `ring[slot]+0x30` into `0x27EA10` |
| `scene+0xD0` | `{0x28,0x28,0x28,0x80}` | static .data at 0x27EA20 |
| `scene+0xB0/+0xB4` | `-0.008` (`0xBC03126F`) | static .data at 0x27EA00 |
| `gp-32020` | 0.9 | 0x2A735C |
| `gp-32024`, `gp-32000`, `gp-31996` | 0.1 | 0x2A7358 / 0x2A7370 / 0x2A7374 |
| `0x226768`'s alpha | 30 | `li a0,30` at 0x226870 and 0x2268D0 |

`0x22AB90(slot, additive, ztst)` -> `0x22AA88`: `additive` picks ALPHA_1
0x48 vs 0x44 (22ab44..22ab58) and `ztst` becomes `(ztst<<17) | 0x30000`, so
its `2` arrives as GREATER.  Both are then overwritten by the `0x22A0C0`
that immediately follows in every case, which is why the port's binders
push texture state only.

`0x22A0C0(mode, ztst)`: mode 0/1/2 -> ALPHA_1 0x48 / 0x44 / 0x42,
TEST_1 = `(ztst<<17) | 0x10000` (no off-by-one here), so `(x, 1)` is ZTST
ALWAYS.

### 1.3 One correction to the previous plan

The plan wrote walk 0's bind as `0x22AB90(3, 1, **1**)`.  It is
`0x22AB90(3, 1, **2**)`: `a2` is set in the branch **delay slot** at
0x22EA48 (`li a2,2`), which runs for both arms.  Immaterial - 0x22A0C0(2,1)
overrides TEST_1 either way - but the disassembly says 2.

### 1.4 The two textures

Both were confirmed from the ROM rather than inferred:

- the TEXC slot descriptor table at `0x27F1C0` gives slot 0 and slot 3 as
  `wexp = hexp = 6` (64x64), and the per-slot decoder table `0x2A4BA0`
  sends **both** to `0x22A720` - the same grey expander slots 2 (TEXCBUMP)
  and 5 (TEXCREFA) use.  So one decode serves all four.
- extracted from `scph39001.bin`'s TEXIMAGE: `TEXCFLOW 4556 -> 4096`,
  `TEXCBUMP 4452 -> 4096`, `TEXCBINV 4452 -> 4096`, and **`TEXCBINV` is the
  exact bitwise complement of `TEXCBUMP`** - checked byte for byte over the
  whole 4096-byte expansion, 0 mismatches.  `DecodeBump()` derives it as
  `~src[i]` through the same expander, so no new resource file.

`tools/extract-res.py` already extracts TEXCFLOW with everything else in
TEXIMAGE; `res/` is gitignored, so regenerate with

    python3 tools/extract-res.py <bios.bin> osdbits/res

before building.  `res.c` gains the `#include` and the two assignments;
`RESID_TEXCFLOW` and `RESID_TEXCBINV` were already in `res.h`.

### 1.5 What it looks like

Each walk re-renders every rod's NEAR faces into a freshly cleared work
buffer 4 as a spherical environment map (the same `reflect()` 0x22CD78 the
cube mask walk uses, but with AA1 **and** ABE clear - PRIM 276 - so it is a
flat opaque write, alpha included) with a subtractive emboss over it, then
adds the whole buffer over the finished frame at alpha 30.  The two walks
differ only in which half of one emboss pair they use, so what survives the
two additions is the *difference* between TEXCBUMP and its complement: a
thin sparkle along the rods.  That is the glow retail's clock has.

Because pass A has ABE clear, the alpha it writes into wb4 is the record's
own `col1[3]`, and that alpha is exactly what the composite modulates by -
which is why 0x2267E8's clear record carries **0x80** where the cube
stage's 0x27F180 carries 0.  A 0 there would make the whole stage
invisible.

---

## 2. Nit 1: the flat cube Z was a wrong GS REGISTER, not a missing mask

The port emitted `0xFFFFF010` where retail carries `0x00FFF010`.  Nothing
in the ROM masks: `sceVu0FTOI4` (0x267668) is a bare `vftoi4` and 0x22C4E0
pushes the whole sign-extended word (22c82c `lw a0,56(s0)` / 22c838
`dsll32`).  The difference is the register.

Every one of the ROM's REGLIST templates names GIF register **4 = XYZF2**,
not 5 = XYZ2:

| template | NREG | regs |
|---|---|---|
| 0x27F870, 0x27F880 | 6 | PRIM, RGBAQ, XYZF2 x4 |
| 0x27F890 | 12 | PRIM, CLAMP_1, CLAMP_1, RGBAQ, (UV, XYZF2) x4 |
| 0x27F8A0 | 10 | PRIM, RGBAQ, (UV, XYZF2) x4 |
| 0x27F8B0 | 14 | PRIM, CLAMP_1, (ST, RGBAQ, XYZF2) x4 |

XYZF2's Z field is 24 bits wide where XYZ2's is 32, so the GS truncates
`-4080` to `0x00FFF010 = 16773136` - exactly what the dump shows.  The
dump's own event counts confirm the register: retail's mesh vertices are
`RL XYZF2` (22984 of them in one frame), never `XYZ2`.

Fixed by a shared `MeshEmitXYZ(f, k, zbias)` used by all five emits, which
pushes `SCE_GS_XYZF2` with `z & 0xFFFFFF` and F = 0.  F is dead: none of
the five PRIMs (276, 404, 84, 132, 196) sets FGE.  The rods are unaffected
(their z is 1.8M..4.4M, well inside 24 bits); only the cube stage's flat
`0xFFFFF010` / `0xFFFFF011` changes, to retail's `0x00FFF010` /
`0x00FFF011`.  `vif1SetZWrite`'s PSMZ32 stays as it is - retail's 416 ZBUF
writes are all `psm=0` - but its comment about PSMZ24 masking the port's
0xFFFFF010 is now moot and has been rewritten.

---

## 3. Nit 2: the ZMSK audit

Retail: **416 of 416** ZBUF writes in a config-screen dump are
`zbp=4480 psm=0` with ZMSK clear.  Before this patch the port still masked
Z in five places.  All five are now `vif1SetZWrite(1)`:

| file | function | ZTST there | effect |
|---|---|---|---|
| `menuconfig.c` | `MeshDrawRod` | passes 2..5 run **GEQUAL** | the real one - the rods were testing against whatever the previous frame's cube stage left behind, and never storing |
| `menu.c` | `DrawOrbPass` | ALWAYS | orbs now store z (trails real, sprites 0), which is what the interleaved rods then test against |
| `menu.c` | `DrawFadeCurtain` | ALWAYS | none |
| `menuback.c` | `ZoomBlur` | ALWAYS | none |
| `menuback.c` | `MenuBackWorkBlur` | ALWAYS | none |
| `menuback.c` | `MenuBackdrop`'s two screen copies | ALWAYS | none |

Retail's own orb pattern in the dump is the same shape: `LINESTRIP+AA1`
under `ztst=3` at real z, then the two `SPRITE+TME+ABE+FST` under `ztst=1`
at **z = 0**, all with ZMSK clear (retail614.log D1461-D1463).  So retail
really does stamp z = 0 through every orb halo, and its rods' GEQUAL passes
run against that.

`menutext.c` has no `vif1SetZWrite` call at all, so nothing to align there.

---

## 4. The diff

`rodflush.diff`, `git apply` from the repo root.  Written against
`9d1fd93`; checked to apply cleanly against `3ee8cae` (current tip) as
well - `729d76a` touched `menuconfig.c` but not in any hunk this patch
needs.

    osdbits/inc.h           4 +-   MenuConfigFlushMesh, MenuBackFlushOver,
                                   MenuBackWorkTarget's clear-record arg
    osdbits/menu.c         ~70 +   SceneFlush (0x2267E8); the two ZMSK
                                   alignments; MenuFrame calls SceneFlush
                                   instead of the explicit screen restore
    osdbits/menuback.c    ~130 +-  WorkClear takes a colour record AND
                                   pushes its own ZTST (see 5.1); BlurBlit
                                   takes an alpha; MenuBackFlushOver
                                   (0x22C020(1,0,0) + 0x226768); three ZMSK
    osdbits/menuconfig.c  ~190 +-  TEXCFLOW + TEXCBINV decode/bind;
                                   MeshEmitXYZ (XYZF2); MeshEmitReflFace and
                                   MeshReflPass gain `aa'; MenuConfigFlushMesh
                                   (0x22E9A8); rod-path ZMSK; cubeMaskClear
    osdbits/res.c           8 +    TEXCFLOW wired in

`res/TEXCFLOW_EXP.inc` is NOT in the diff - `res/` is gitignored.  Run the
extractor first (1.4).

`MenuFrame`'s explicit `MenuBackScreenTarget(MenuBackField())` after
`SceneWalk()` is gone: 0x2267E8's own `0x22C020(1,0,0)` does that job, as
the previous write-up predicted.  One behavioural consequence, documented
rather than papered over: 0x22C020's third argument is 0, so the stage
leaves XYOFFSET without the interlace half pixel, and it is `ZoomBlur`'s
tail (`BackHalfOffset(1)`) that puts it back.  With the default ramp
(`argv[10]` = 10) that always runs.  If `argv[10]` is dropped below 5 the
blur runs zero passes and the 2D layer draws without the half pixel - the
ROM does not have that gap because its 2D layers push their own 0x22A3B8;
`menutext.c` pushes no FRAME at all.

---

## 5. Verification

Windowless PCSX2 on Xvnc `:96` (started and left running by this job, no
`pcsx2-qt` left behind, `ulimit -c 0`, no cores, no PCSX2 setting touched).
`DumpFrameAscii` 8x8 luminance maps extracted with `grep -o '|.*|'`.
Baseline = an unmodified build of `9d1fd93` in `base/osdbits`.

- **Main-menu regression** (`menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1`):
  the ASCII map is **byte-identical** to the baseline
  (md5 `37786007ddc6534efaff9528f3ba8c11` both).  Expected, and it is a
  real check of three separate things at once: the flush runs there too
  (the record list is all orbs, which it skips, so what runs is the wb4
  clear plus two additive composites of a black buffer - exactly as in the
  ROM, and exactly as invisible); the ZMSK changes are colour-neutral where
  every ZTST is ALWAYS; and XYZF2 is a no-op for z that already fits in 24
  bits.

- **Config screen** (`menu 18 27 45 0 1 128 145 0 0 0 10 0 1 0 1`, frame
  145): see the map diff below.

### The map results

| build | config-screen map md5 | main-menu map md5 |
|---|---|---|
| baseline (`9d1fd93`) | `08a25058c842a3cfd14056d5977d6888` | `37786007ddc6534efaff9528f3ba8c11` |
| flush + XYZF2 only (ZMSK reverted) | `ed3cc7d236adb9eb4688eb81b65f828e` | - |
| full patch, before the WorkClear fix | `1df6bc1a659b1deb62daaae4d3908fa2` | `37786007ddc6534efaff9528f3ba8c11` |
| **final** (full patch + WorkClear fix) | `1df6bc1a659b1deb62daaae4d3908fa2` | `37786007ddc6534efaff9528f3ba8c11` |

Two things worth recording about the method:

- **The config map is reproducible run to run.**  Two independent PCSX2
  runs of the same build gave a byte-identical map, so `evenOddField`
  (which `main.c` reads from the live GS CSR, and which therefore *could*
  have varied with emulation speed) did not vary here, and every difference
  below is a real consequence of the change rather than run noise.  Worth
  re-checking if a future comparison ever looks noisy.
- **The bisect earned its keep** and is what found the `WorkClear` bug
  (section 5.1).  Without it the "flush + ZMSK" pair would have gone in as
  one change that happened to work.

### What changed on the config screen

Everything above the clock ring is within a ramp step or two of the
baseline; the clock region is unambiguously brighter, which is the bloom:

    row 13  base `.-+#%%%####`   ->  `.-*#%%%%###`
    row 21  base `.-----*+=:`    ->  `.----=##*:`
    row 23  base `==--+*++=`     ->  `++=-*%##=`
    row 24  base `-------@@=-+#+*+`  ->  `-------@@==*%%%*-`
    row 25  base `:=-+#+**-`     ->  `:-=+%%%%=`

The change is not confined to the rods' own pixels - the top and bottom
chrome bars and the item rows move by a ramp step too.  That is expected
rather than suspicious: `MenuZoomBlur` runs `ZoomBlur(5)` on **every**
frame (the ramp idles at 10), five bilinear shrink/stretch round trips over
the whole frame buffer, so any brightness the bloom adds is spread a long
way before the 2D layer goes on top.  The bloom is added before the blur in
the ROM too (0x2267E8 is the object list's tail, 0x2283D0's blur is the
next stage).  Confirming that in detail is a job for the GS dump in
section 6, where the stage is two unmistakable segments rather than an 8x8
luminance map.

## 5.1 A real bug the bisect found: WorkClear had no depth test

The "flush + XYZF2 only" build above lifted **every** background block of
the config screen by about two ramp steps - a second, ~23 % copy of the
whole frame laid over the frame.  The cause was not the new stage's
arithmetic but `menuback.c`'s `WorkClear`, which emitted its clear sprite
under whatever TEST_1 was live.

`sceGsSetDefDrawEnv2` does not: a retail dump shows it bracketing the clear
sprite with its own pushes (retail614.log around D1759) -

    TEST ZTE ztst=2      the drawenv's own
    TEST ZTE ztst=1      the clear's - ALWAYS
    PRIM SPRITE ... z=(0,0) rgba=00000080
    TEST ZTE ztst=2      restored

The clear sprite is at z = 0, so a caller arriving with ZTST GEQUAL and a
non-zero Z buffer got **no clear at all**.  `0x226D00`'s call site happens
to set ZTST ALWAYS just before, so the cube mask clear was always fine;
`0x2267E8`'s follows `MeshDrawRod`'s pass 5, which leaves GEQUAL, so the
rod bloom's clear was skipped and work buffer 4 still held
`MenuBackdrop`'s copy of the whole screen.

`WorkClear` now pushes ALWAYS before its sprite and GEQUAL after, as the
library does.  With the ZMSK alignment also in place the map does not move
(the two pre-scene screen copies now write z = 0 across the buffer, so the
clear was passing anyway) - `1df6bc1a...` before and after the fix - but
the stage no longer depends on that coincidence, and neither does anything
else that ever clears a work buffer.


---

## 6. What aap should look for in the next shift+F8 snap

Pixel-exact proof needs a fresh interactive GS dump, which the headless
recipe cannot trigger.  In the next config-screen dump of this build,
decoded with `tools/gsdump-decode.py` + `-drawlog.py`, the things to check
against `retail614.log` / `retail614v.log`:

1. **Two new 173-draw segments into FB = wb4**, between the last rod/orb
   and the zoom blur, each `1 SPRITE + 86 TRISTRIP+TME+FST + 86
   TRISTRIP+TME+ABE`.  Retail's are at retail614.log **L9632** and
   **L10335**.  Our wb4 is at block 9600, not retail's 8960, so match on
   the shape, not the fbp.
   - the leading sprite must be full-screen `z=(0,0) rgba=00000080` (the
     0x27EBF0 clear).  `rgba=00000000` would mean the wrong clear record.
   - the 86 `+TME+FST` draws must carry `rgba=3c3c3c80` (the ring slots'
     `col1`) under `TEST ZTE ztst=1` and `ALPHA 0101`, sampling our
     TEXCFLOW page.  See the caveat in 7 about one rod.
   - the 86 `+TME+ABE` draws must carry `rgba=28282880` under `ztst=1` and
     `ALPHA 2001` (0x42, subtractive), sampling TEXCBINV in the first
     segment and TEXCBUMP in the second - the segment-boundary caveat in
     `gsdump-segsum.py`'s header applies, so read the verbose log for which
     tbp actually feeds which draw.
2. **The two composites**: right after each segment, one
   `SPRITE+TME+ABE+FST` onto the screen buffer with
   `uv=(0,0)-(640,224)` and **`rgba=8080801e`**, under `ALPHA 0201`
   (0x48) and `ztst=1`, with `XYOFF (1728.0,1936.0)` - no half pixel.
   Retail's are D1932 and D2106.  A different alpha there means
   `MenuBackFlushOver`'s argument did not reach `BlurBlit`.
3. **ZBUF**: every ZBUF write in the whole frame must now read
   `zbp=4480 psm=0` with **no** `ZMSK` suffix, matching retail's 416/416.
   `grep -o 'ZBUF.*' ours.log | sort | uniq -c` should show exactly one
   line.
4. **The cube stage's Z**: `z=(16773136,16773136)` on the cube draws and
   `16773137` on the black mask pass, where the previous build reported
   `4294963216` / `4294963217`.
5. **The crack classifier** (the outstanding check from the previous
   write-up) can be run on the same dump: replay through
   `tools/gsreplay/` with `-s draw:<mask-walk-end>` and score wb4's alpha -
   `interior` should read 0.

---

## 7. Left open

- **The front rod's `col1`.**  Retail's live ring (savestate `.04.p2s`,
  0x34E6C0 + slot*48 + 0x30) gives the front slot `{0x80,0x80,0x80,0x1E}`
  and the other eleven `{0x3C,0x3C,0x3C,0x80}` - which is exactly what
  `InitMenuConfig` sets up, and 0x225318's argument list (a3 = 0x27EAD0 for
  the plain slots, t1 = 0x27EAF0 for the front) confirms the targets.  Yet
  all **86** of retail's flush draws are `rgba=3c3c3c80`.  The reason is
  that retail's front rod has a non-zero `split` and therefore goes through
  0x22E428's *split* arm (0x22E4D0), which passes 0x22CD78 a stack-built
  scene copy (22e6c8 `move a1,sp`) rather than the record's.  This port has
  no split rod, so its front rod comes through the simple arm with its own
  `col1` - it will bloom at RGB 0x80 and, because pass A writes alpha too,
  composite at As ~ 7 instead of 30 over its own faces.  Porting the split
  arm is the real fix; until then this is one rod out of twelve, and it is
  the same divergence the visible rod path already has.
- **The split rod (0x22E4D0) itself**, still not ported anywhere - see
  `MeshDrawRod`'s notes.
- **The cube mask walk's refraction ALPHA** (retail 0x44 vs our 0x48 for
  0x22CD78 there) - untouched, still open from the previous pass.
- **Pixel proof**, section 6.


---

# "The config menu switches between two resolutions"

Diagnosis, fix and evidence.  Deliverables in this directory:

* `interlace.diff` — git-apply-able against `6425e1c` (checked with `git apply --check`)
* `fix/osdbits/` — the patched tree it came from (builds clean, `main.elf` present)
* `base/`, `fixm/` — throwaway instrumented trees used for the measurements
* `pristine/osdbits/` — an unmodified copy of the tip, for the A/B runs

---

## Verdict: **hypothesis 1 (frame pacing).  Not a field/offset bug.**

The config screen's frame no longer fits in one 16.7 ms field.  It straddles
the vsync, so the port renders **one field for every two displayed**, and
because a field-rendered scene picks its XYOFFSET half-pixel from the *current*
CSR FIELD bit, rendering every second field means it always sees the **same
parity**.  The even field is then never drawn fresh — the display shows the
odd-field image on both fields, which is exactly a halving of vertical
resolution.  When the frame happens to squeak in under budget it flips back to
proper 60 Hz field alternation.  That flapping is what reads as "two
resolutions", and it is worse on the heaviest item (cursor 0, Clock
Adjustment).

The display circuit is **not** involved: SMODE2 (`INT=1 FFMD=1`), DISPLAY2
(`DX=636 DY=50 MAGH=4 MAGV=1 DW=2560 DH=448`) and DISPFB2's FBW/PSM are
byte-identical in every privileged-register snapshot of both dumps, of the
pre-bloom dump and of retail.

---

## Evidence from the two dumps

`osdbits/tools/gsdump-decode.py` throws the 8 KB privreg blocks away
(`('REGS',)`); `decode2.py` here is the same decoder with the blob kept, and
`priv.py` prints PMODE / SMODE1 / SMODE2 / DISPFB1,2 / DISPLAY1,2 / CSR.

### 1. DISPFB2 does not advance every field in the bad dump

| dump | vsync 1 | vsync 2 | vsync 3 | vsync 4 | verdict |
|---|---|---|---|---|---|
| `...192536` ("bad")  | FBP 0 | FBP 0 | FBP 70 | FBP 70 | **30 Hz** |
| `...192542` ("good") | FBP 70 | FBP 0 | FBP 70 | FBP 0 | 60 Hz |
| `gscmp2/...155131` (pre-bloom) | 0 | 70 | 0 | 70 | 60 Hz |
| `gscmp/...080614` (retail) | 0 | 70 | 0 | 70 | 60 Hz |

CSR FIELD alternates 1/0/1/0 in all four — the GS keeps interlacing; it is the
*app* that only produces a new buffer every other field.

### 2. Both rendered frames of the bad dump use the SAME half-pixel

Per-vsync XYOFFSET tally from `gsdump-drawlog.py`:

```
192536 (bad):   block0: 78x (1728.0,1936.5) + 11x (1728.0,1936.0)   <- ODD
                block1: (nothing - only the 12-draw tail of block0 spilling past vsync)
                block2: 78x (1728.0,1936.5) + 11x (1728.0,1936.0)   <- ODD AGAIN
                block3: (nothing)

192542 (good):  block0: 89x (1728.0,1936.0)                          <- EVEN
                block1: 78x 1936.5 + 11x 1936.0                      <- ODD
                block2: 89x (1728.0,1936.0)                          <- EVEN
                block3: 78x 1936.5 + 11x 1936.0                      <- ODD
```

The 11 draws at 1936.0 inside an odd frame are the buffer-to-buffer blits, which
correctly carry field 0 (commit 37efd18's rule).  So the *offsets themselves are
right everywhere* — hypothesis 2 is disproved.  What is wrong is that the bad
dump never renders an even field at all.

Confirmed live, without a dump: with the field parity traced straight out of
`SwapBuffers` for 40 consecutive frames starting at frame 120,

```
baseline: field parity: n=40 [1111111111111111111111111111111111111111]
fixed:    field parity: n=40 [0101010101010101010101010101010101010101]
```

### 3. Where the vsync falls inside the frame

In the bad dump the "empty" field is not empty: it holds ~12 draws of caption
text (font sprites at x 2091..2346, y 2133..2148) — the *tail* of the previous
frame spilling past the vsync marker.  The frame is only marginally over one
field, which is why the two dumps differ at all (2057 vs 2002 packets/frame).

### 4. We are not drawing more than retail — we are packaging it far worse

Per frame, from the decoded streams:

| | GIF packets | qwords | draws | verts |
|---|---|---|---|---|
| retail `080614`            | 1029 | 12997 | 1189 | 5942 |
| ours, pre-bloom `155131`   | 1587 | 15763 |  826 | 4506 |
| ours, with bloom `192542`  | 2002 | 20385 | 1136 | 5792 |
| ours, with bloom `192536`  | 2057 | 20582 | 1150 | 5818 |

The bloom brought our draw count to *retail parity* (the wb4 segment goes
411 → 749 draws against retail's 771) — it is not drawing anything twice, and
deleting it was never the answer.  The cost is entirely in packaging:

* every one of our GIFtags is `(flg=PACKED, nreg=1, reg=A+D)` — **2002 of them**;
  retail mixes `PACKED nreg=1/3/5` with `REGLIST nreg=2/3/6/10/12/14`, 1029 tags,
  and REGLIST puts two registers in one qword where A+D puts one.
* the per-FRAME-segment breakdown matches retail's pass structure exactly, but
  every segment costs 2.5–4.7× the packets: e.g. the glass chain is
  retail `2240:4 / 6720:4 / 8960:9` against ours `0:10 / 7040:11 / 9600:42`.

---

## Root cause in the source

`opening.c`'s packet layer gave **every single GS register write its own DMA**:

```c
void vif1SetAD(u32 a, u64 d)
{ vif1Begin(); pktSetAD(a, d); vif1End(); }
```

with `vif1Begin()` = grab a scratchpad chain buffer, `PkCnt` + `OpenDirectCode`
+ `OpenGifTag`, and `vif1End()` = `CloseGifTag` + `CloseDirectCode` + pad +
`PkEnd` + `PkTerminate` + **`sceDmaSync`** (blocks on the previous kick) +
`sceDmaSend`.  Measured in-emulator on the tip:

```
baseline, config screen: gif tags=1921  kicks=1921  DMA qwords=27273   (per frame)
```

1921 fully serialized DMA kicks per frame, ~9600 qwords of which are nothing but
per-packet framing (CNT tag, GIFtag, END tag, `vif1Pad` padding).

## The fix

Every one of those packets has the *identical* shape — one PACKED A+D GIFtag —
so consecutive ones concatenate exactly.  Keep **one** tag open in the chain
buffer and only close/kick it when the buffer runs out or when something outside
the layer needs the GS to have caught up:

* `vif1Open()` — the old `vif1Begin` body (new buffer, CNT, DIRECT, GIFtag).
* `vif1Begin()` — reuse the open tag if at least `VIF1_CHAIN_RESERVE` (224)
  qwords are free, else flush and open a new one.  Guaranteeing the reserve up
  front means **no begin/end block is ever split**, so no strip is ever cut in
  half and no new PATH3 interleave hazard is created.
* `vif1End()` — only tracks `vifChainHigh`, the largest block seen, which is
  what justifies the 224: measured **182** across the whole config screen.
* `vif1Flush()` — close the tag and kick.  `pktSetAD` carries a hard overrun
  guard that splits as a last resort; `vifChainHigh` says it never fires.
* `gsSyncPath()` = `vif1Flush()` + `sceGsSyncPath(0, 0)`; every `sceGsSyncPath(0, 0)`
  in the port now goes through it, so a drain can never be declared while
  register writes are still sitting in scratchpad.  Additional explicit flushes
  before `sceGsExecLoadImage` (UploadImage), before `SwapBuffers`/`StartFrame`
  touch the draw env, and before `sendDma`/`towerKick` reprogram D1 by hand.

The GS sees the same register writes in the same order — only the packaging
changes.  Both ASCII regression maps are byte-identical (below).

### One trap worth recording

`static const u64 vif1GifTag[2]` at file scope lands 8-byte aligned, and
`sceVif1PkOpenGifTag` takes the tag *by value* — it is loaded with `lq`, which
ignores the low four address bits.  The first build read the eight bytes
*before* the array as the tag's low half; the GIF then sat mid-packet on PATH2
forever and every later drain reported `sceGsSyncPath: DMA Ch.2 does not
terminate`.  `ALIGN16` on the array is load-bearing (the old code got away with
a stack local by luck).  The comment in the diff says so.

---

## Verification

All runs: windowless PCSX2 on its own `:94` Xvnc, one instance at a time.

### Pacing (config screen, cursor 0 = the worst item)

`-gameargs "menu 18 27 45 <N> 1 128 0 0 0 0 10 0 1 0 1"`

| | frames | fields consumed | 1-field frames | 2-field frames |
|---|---|---|---|---|
| baseline, N=300 | 300 | **507** | 93 | 205 |
| fixed,    N=300 | 300 | **302** | 298 | 0 |
| baseline, N=200 | 200 | **312** | 88 | 110 |
| fixed,    N=200 | 200 | **202** | 198 | 0 |

68 % of baseline frames were taking two fields.  None do now.

### GIF traffic (per frame, config screen)

| | GIFtags | DMA kicks | DMA qwords |
|---|---|---|---|
| baseline | 1921 | 1921 | 27273 |
| fixed    | **75** | **75** | **17692** |

25.6× fewer packets and kicks, 35 % less DMA volume.  Main menu, same run
shape: 273 → **7** kicks/frame, 3025 → **1576** qwords/frame (it already had
headroom — 202 fields for 200 frames before and after — which is why aap only
saw this on the config screen).

### Field parity, frames 120–159

```
baseline [1111111111111111111111111111111111111111]   locked odd, even field never drawn
fixed    [0101010101010101010101010101010101010101]   proper interlace
```

### Output regressions — all byte-identical to the pristine tip

* main menu, `menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1` → 28-line ASCII map identical
* config,    `menu 18 27 45 0 1 128 145 0 0 0 10 0 1 0 1` → 28-line ASCII map identical
* opening,   `boot 12 34 56 0` → identical printf stream, `osd: dispatch(20500, 1, 0, 0)`,
  zero `does not terminate` errors (this exercises `sendDma` / `towerKick` /
  `vu1Wait`, the raw-D1 paths the flush had to be threaded through)

---

## Left on the table (not needed for this bug)

We are still at 2002 GIFtags/frame against retail's 1029 and 20385 GIF-payload
qwords against 12997, because every register still goes as A+D.  Moving the
vertex streams to REGLIST (retail's `nreg=14/12/10/6`) would roughly halve the
payload and is the obvious next step if the frame ever gets tight again — but
after this change the config screen has ~1/3 of a field of slack, so it is not
urgent.


---

# The front rod's split arm, the vignette, and the config item editors

Deliverables in this directory:

* `splitarm.diff` — git-apply-able against `f9a8b46` (checked with
  `git apply --check` on a clean worktree of that commit)
* `osdbits/` — the patched tree it came from (builds clean, `main.elf` present)
* `pristine/osdbits/` — an unmodified copy of the tip, used for every A/B run
* `test_p2/` — a throwaway instrumented copy (scripted pad injection) used to
  exercise the phase-2 interactive loop headlessly; NOT part of the diff
* `dis_*.txt`, `base_*.log`, `new_*.log`, `p2_*.log`, `final_*.log` — the
  disassembly extracts and the PCSX2 run logs cited below

Everything below is from the ROM disassembly (`expanded.bin`, VAs = file
offset + 0x200000, gp = 0x2AF070), live retail memory (the
`20020207-164243 (00000000).0N.p2s` savestates), and the decoded retail GS
dump `retail614.log`/`retail614v.log` in `tmp/gscmp/`.  Nothing is guessed;
the few places where the port simplifies are called out as such.

---

# Phase 1a — the front rod's split arms (0x22D920's f12 > 0 arm, and 0x22E4D0)

## What the split IS

The front ring slot is the clock's **minute progress bar**.  Its `split`
(record +0xF4) climbs from 0 by `*(gp-32164)` = 0.004/frame toward
`1 - minutes/60` (0x225BF8), and once it is > 0 the rod is drawn as TWO
pieces:

* the **lower** piece at Y scale `progress * split`, in a bright cyan-white;
* the **upper** piece at Y scale `progress * (1 - split)`, in the rod's own
  body colour, its world matrix translated by `(0, 26 * progress * split, 0)`
  **in the rod's local frame** (26 = the model height, so it sits exactly on
  the cut).  The ROM does the translate by loading the matrix into the
  0x230000 stack top (0x230180), calling 0x230440 (`mdTranslatef`) and
  reading it back — the port does literally that with `mdTop`.

Both `0x22D920` (the visible rod) and `0x22E428` (the deferred bloom) branch
on `0 < split` at their heads (22d9b4/22d9cc and 22e4b8/22e4d0); the split
arms are 0x22D9D4 and 0x22E4D0.  Each makes two 224-byte stack copies of the
scene struct (`move a1,sp` — the "stack-built scene copy" the old Left-open
note saw), patches them as below, transforms the lower into the face bank at
**0x3529D0** and the upper into the second bank at **0x3555D0**
(= 0x3529D0 + 32*352), and then runs the normal pass bodies with each pass
walking both banks, lower first.

## The face-index skips

The rod model's sixteen faces are: 0–1 the flat top cap (y 26.39), 2–7 the
bevel ring (26..26.39), 8–9 the bottom cap (y 0), 10–15 the six shaft sides
(0..26).  Every split-arm loop skips fixed indices per piece:

* lower: only faces **8..15** (`slti v0,s2,8` skips — bottom cap + shaft);
* upper: everything **except 8 and 9** (`addiu v0,s2,-8; sltiu v0,v0,2` —
  top cap + bevel + shaft).

I.e. exactly "no cap at the cut".  Port: `meshFaces2[]` + a per-pass
`meshBank`/`meshMask` pair (`MESH_LOWER` = 0xFF00, `MESH_UPPER` = ~0x0300)
threaded through the five pass walkers; the per-face ST-phase step still
counts the raw index i, culled and masked faces included, as the ROM does.

## What the record feeds the two arms

`0x225DD8` (the record appender) lays the record out as: +0x10 the 224-byte
scene copy, +0xF4 split, +0x100 = `*(0x34E910)`, +0x110 = the aux int
(100 for the front slot, 0 otherwise — 0x226028 passes `li a2,100`),
+0x120 = `*(0x34E920)`.  `0x2266E0` then calls
`0x22D920(rec+0x10, rec+0x100, f12=rec->0xF4, f13=(float)rec->0x110)` and
`0x2267E8` calls `0x22E428(rec+0x10, walk, f12=rec->0xF4, a2=rec+0x120)`.

**The two colour qwords were mis-valued in the port.**  0x225878/0x225318
ease 0x34E910 toward **0x34E940 = {167, 217, 255, 0}** (the fixed cyan-white,
written in immediates at 22d8c8..22d8e8 — the same colour whose average with
the body colour makes the front slot's col0) and 0x34E920 toward
**0x27EAE0 = {0x3C, 0x3C, 0x3C, 0x80}**.  Live retail memory confirms both
settled values in savestates .04 and .05 (they are zero in .02/.03 — the
ring isn't built until the config screen runs).  The port had them as
0x27EAE0/0x27EAF0's values; harmless before (nothing read `colA`/`colB`),
load-bearing now.

## The visible split arm (0x22D9D4), pass by pass

Same five passes as the one-piece arm — same binds, targets and blend modes
(22db44..22e0e0) — with each pass walking lower then upper.  The deltas
against the one-piece pass bodies:

* copy1 (lower): `+0x6C = progress*split` (22da74), `+0x80 = *(rec+0x100)`
  = the bright {167,217,255,0} (22da7c), `+0x90 = f13 = (float)rec->0x110`
  = **100** (22da70) — the lower piece's Fresnel size is half the upper's 200.
* copy2 (upper): `+0x6C = progress*(1-split)` (22dab0); world translated as
  above (22dabc..22db20); keeps the scene's own colour and size.
* both pieces shrink toward the WHOLE rod's 0.9 refraction centre — every
  0x22C888 call in both loops gets f12/f13 = the head's outX/outY
  (sp+448/452).  (The origin is scale-invariant, so the port computes it
  from the lower transform.)
* the upper piece's emboss **T offset gains `2 * progress * split`**
  (22dd58 `add.s f13,f13,f13` on copy1's +0x6C): the model V spans 0..2
  scaled by +0x6C, so this makes the upper piece's TEXCBUMP phase continue
  exactly where the lower piece's ends.  S is NOT offset.
* the per-face ST steps are gp-32048/-32044/-32040/-32036 — read from the
  image, all 0.1, same as the one-piece arm's gp-32032/-32028.

Retail GS-dump evidence: in `retail614.log`'s object walk, D0066
(`rgba=cfffff80`) and D0116 (`dfffff80`) are the lower piece's on-screen
draws — 0xCF = 167 + a Fresnel bright of 40 with G and B saturated, which
only the {167,217,255} base produces (every other rod saturates to
`ffffff80`).

Port: `MeshDrawRodSplit()` in menuconfig.c; `MenuConfigDrawMesh` dispatches
on `rec->f12 > 0`.

## The flush split arm (0x22E4D0) — the bloom-colour fix

Same two copies, same skips, same upper-piece translate and T continuation
(22e850/22e974), with three deltas of its own:

1. **BOTH copies' +0xC0 — the colour 0x22CD78 stamps — become
   `*(rec+0x120)` = {0x3C,0x3C,0x3C,0x80}** (22e580/22e5c0), not the slot's
   own col1.  This is the whole answer to the Left-open item: retail's 86
   flush draws are all `rgba=3c3c3c80` (retail614.log L9655..: 5–8 FST draws
   per rod, 86 total, every one 3c3c3c80) even though the front slot's col1
   is {0x80,0x80,0x80,0x1E} — because the front rod goes through THIS arm.
   Since pass A (PRIM 276, no ABE) writes its alpha flat into wb4, the front
   rod now also composites at As 0x80 instead of 0x1E — the "bloom at RGB
   0x80, composite at As ~7" divergence is gone.
2. pass A runs under `0x22A0C0(0,1)` = ALPHA_1 0x48 (22e674/22e688) where
   the simple arm pushes (1,1) = 0x44 — both dead (PRIM 276 has ABE clear),
   mirrored anyway.
3. **the emboss textures are SWAPPED against the simple arm**: walk 1 binds
   slot 3 = TEXCBINV (22e74c `li a0,3`) and walk 0 slot 2 = TEXCBUMP
   (22e884 `li a0,2`), while the ST offset stays with the walk (walk 1 gets
   scene->+0xB0 = -0.008, walk 0 gets 0) exactly as in the simple arm.
   Net: the front rod uses the emboss pair in the opposite order — the sum
   is the same, the sign of the surviving difference is not.

0x22E4D0 takes no size argument and neither copy keeps the record's col1,
so only progress, split, world and colB come out of the record.  The
per-face steps gp-32016/-32012/-32008/-32004 are all 0.1.

Port: `MeshFlushSplit()` in menuconfig.c, dispatched from
`MenuConfigFlushMesh`.

---

# Phase 1b — the state-machine arm: 0x27F620 is the menu VIGNETTE

The empty `count == cfgDur40b + cfgDur40` branch calls **0x22AE80**, which
opens the timer at **0x27F620** (duration rate*80/60, set by 0x22AD38 with
the other config durations; 0x22AD38's tail 0x22AC60 RESETS it to state 0).
Its counterpart **0x22AEC8** — called at 0x2272B0, the second call of "enter
System Configuration" — closes it.  Those are the only two edges in the
image.

What the timer gates: **0x22AF10**, run every frame from 0x22B020 (the
0x21CF20 slot right after the object list 0x2268F0 and before the 2D hub
0x2283F0 — the same function whose other half, 0x22AFB8, is the fade curtain
the port already had).  0x22AF10:

* writes the timer's interp (0x22AC20, 0..128) into the record at 0x27F670
  as its alpha, rewrites the record's y centre as `(screenH/2)<<4` (its x
  centre 4256 = 266 px and radii 1184/592 = 74/37 px are .data), and returns
  without drawing while the timer is state 0;
* otherwise draws a 16-segment TRISTRIP ring (PRIM 76 = TRISTRIP|IIP|ABE via
  one REGLIST {PRIM} packet, template 0x27F2D0; then sixteen {RGBAQ,XYZF2}
  REGLIST packets, template 0x27F2E0, six vertices each) of **BLACK**:
  the inner pair on the (74,37) ellipse at alpha 0, the pair at 1.5x those
  radii at the timer's alpha, and an outer pair at 10x, clamped to the frame
  rectangle (0x229BC8/0x229C30 -> 0x229A70).  X uses sin, Y cos, angles
  seg*4096 as shorts (0x229AA8/0x229B38 — centre + sin*r*scale + the
  XYOFFSET-relative origin, one trunc).
* state: ALPHA_1 0x44, TEST ZTST **GEQUAL**, Z = 0 (rec +0x14), ZMSK 0,
  FRAME **inherited** — no drawenv push; it draws into whatever 0x2267E8's
  tail left bound, the screen with no half pixel.

Under 0x44 that multiplies everything outside the ellipse toward black — a
**vignette around the menu's centre** at (266, h/2).  The GEQUAL-with-Z=0
test is NOT much of a gate in practice: SceneFlush's full-screen clears and
composites (all ZTST-ALWAYS sprites at z=0 with ZMSK 0) have already reset
the Z buffer by this point in the frame, so the vignette lands on the whole
scene, rods and orbs included.  It is skipped while a module fade runs
(0x22AD30's word — the same gate the curtain uses on the other branch).

The choreography this completes:

* fresh boot -> main menu: timer state 0, **vignette never drawn** (the
  backdrop is black anyway);
* enter System Configuration: 0x22AEC8 -> the (open, if it was) vignette
  lifts over 80 frames as the tunnel fades in;
* leave: at Anim count 80 the state machine's 0x22AE80 re-opens it — the
  tunnel (which takes another 40+40 frames to fade out) is masked back down
  to black around the menu centre while the carousel folds;
* thereafter the main menu keeps the vignette at 128, i.e. the post-config
  main menu is "black with a pool of light at the centre".

Port: `MenuVignette()`/`DrawVignette()`/`MenuVignetteOpen`/`MenuVignetteClose`
in menu.c (the timer lives with the fade code, as in the ROM);
`MenuEnterConfig` calls the close, the state-machine branch the open; the
draw sits between `SceneFlush()` and `MenuZoomBlur()` — the ROM's slot, so
the blur softens the vignette's edge as retail does.  The fade curtain keeps
its documented (commutative) late position.

**Verification** (windowless PCSX2, :93, headless ASCII maps):

* main-menu regression `menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1`:
  28-line map **byte-identical** to the pristine f9a8b46 build (the vignette
  timer is state 0 on that path, the split never grows there).
* config regression `menu 18 27 45 0 1 128 145 0 0 0 10 0 1 0 1`: the map
  changes ONLY in the front rod's cells (rows 21–25, cols ~33–38): the lower
  ~55 % of the rod brightens (`##*` -> `%%#`/`@%%`) — the split's bright
  lower piece plus the 4x-stronger bloom, exactly the localized change the
  port was missing.  `base_config.log` vs `final_config.log`.
* vignette, exercised with the argv pad stand-ins (enter frame 1, leave
  frame 100, dump frame 160 — a leave at 60 does nothing, the Anim is still
  opening; it must be state 2, i.e. past frame 91): pristine shows the
  still-fading tunnel across the whole map; patched shows it masked to black
  everywhere outside an ellipse around (266, 112), the centre surviving
  through the alpha ramp.  `base_leave2.log` vs `new_leave2.log`.
* zero `does not terminate` / VIF errors in any run.

Left open from phase 1 (small, listed for honesty):

* the vignette record's static x centre 266 is left-of-centre on a 640-wide
  frame (the ROM hardcodes `li a2,4256` while computing y from the live
  height) — reproduced verbatim, worth eyeballing against real hardware;
* the keyframe cyclers 0x225528/0x2255A8 that animate 0x27EAC0 and 0x34E940
  around their idle values are still not ported (pre-existing gap; the live
  .04 ring shows body G mid-swing at 0x60 vs the idle 0x55);
* 0x22D920's `MeshDebug` diagnostic only reports the lower bank in the split
  path.

---

# Phase 2 — the config item editors (mode 1)

## The ROM mechanism

The 56-byte item records at 0x27BD10 carry SIX callbacks:

| offset | clock (item 0) | items 1–3 | item 4 (Language) | role |
|---|---|---|---|---|
| +0x14 | 0x21DF28 | 0x21EE50 | 0x21EE50 | confirm-OPEN (mode 0 -> 1) |
| +0x18 | 0x21E350 | 0x21EE78 | 0x21EE78 | mode-0 value row |
| +0x1C | 0x21EA20 | 0x21F080 | 0x21F168 | mode-1 widget (draw AND pad) |
| +0x20 | 0x21EAE0 | 0x21EBC8/0x21EC08/0x21EC98 | 0x21ECD8 | confirm-APPLY |
| +0x24 | 0x21EB30 | 0x21F158 (nop) | 0x21F158 | cancel |
| +0x28 | 0x21EB80 | 0x21F160 (nop) | 0x21F160 | focus notify |

0x227D08 (the 0x227DE8 tail) dispatches the pad by the header's mode:
0x2279B8 for 0, 0x227BE8 for 1.  Mode 0's confirm (edge bit 0x20) runs the
+0x14, zeroes the marker phase (0x27BE5C) and sets mode 1.  Mode 1's confirm
writes `*(gp-32136)` = **-0.1 into the cursor's 0x27F090 cube entry at
+0x20** (the pressed cube shrinks 10 % and the 0.95 decay regrows it), runs
the +0x20 and drops to mode 0 (a special `*(0x1F00B0)==5 && *(0x1F00A4)==18`
arm — "launched from a game to set the clock" — picks mode 2 instead; not
ported); cancel (bit 0x40) runs the +0x24.  The **left/right value stepping
is NOT in the pad handler** — each +0x1C widget polls gp-30316 itself, from
inside the draw path, gated on the 0x27EC40 timer being idle (never opened
by anything the port models; gate dropped).

The settings live in the block at **0x352880** (`0x22B0E8(n)` returns
`&block[n]`): 0 screen size, 1 digital out, 2 component out, 3 language,
6..11 the clock's Y/M/D/h/m/s.  The mode-1 widgets write the live setting
**every frame while scrolling** — a value is previewed the moment it is
highlighted, and the value items' +0x24 cancel is a genuine nop, so a
previewed value survives cancel (re-opening re-syncs the index from the live
setting via 0x21EDB8).  The +0x20 callbacks compare the live value against
the persisted copy (0x203690/0x203658/0x2036F8/0x2040D0) and call 0x22B3F8 —
the NVRAM save — only on a change; digital out additionally posts 20/21 into
`*(0x1F00B4)` (the IOP S/PDIF message).

The value list drawer 0x21EF00 draws ALL of an item's values on one row,
centred as a whole, separated by the " " at 0x2A7888, the current one in
0x27B830 (the selected blue) and the rest in 0x27B840 (the dark grey).

**Language** has no list in .data: **0x21ED18**, called from the per-screen
init 0x228460 (0x2284D4), wires the region's table into items[4] —
region 0 gets 0x27B8C0 ({Japanese, English}, count 2), regions 1/2 the seven
European languages at 0x27B920/0x27BA70 ({English..Portuguese}, values 1..7).
Its widget 0x21F168 shows only the current name and slides the old one out
(gp-30676, ±150 px decaying a sixth per frame, drawn by 0x21F3A8).

**Clock Adjustment**: +0x14 (0x21DF28) fades the orb trails down (0x22EF90
closes the 0x27F900 trail timer — a teleporting orb with a live trail would
smear), freezes the tick (gp-30328 via 0x22B960), snapshots the RTC into
settings 6..11 and seeds the soft clock from them (0x22B8E8).  The mode-1
widget 0x21EA20 draws the row via 0x21E3B0 (= the 0x21DFF8 drawer with
edit=1: the field under the cursor in 0x27B830, the rest in 0x27B840), runs
0x21E870 — left/right move the field cursor (kept in the item's +0x08!),
up/down (from gp-30308, the held-repeat word) step the field through its
0x27B870 {setting, min, max} range ({6,2000,2099} {7,1,12} {8,1,31} {9,0,23}
{10,0,59} {11,0,59}) with wraparound, 0x21E3B8 re-clamping the calendar
after every step (epoch conversion between 2000-01-01 and 2099-12-31 via
0x20ABB0/0x20B028) — and reseeds the soft clock EVERY frame, which is what
makes the carousel follow the edit live.  Apply (0x21EAE0) calls **0x22B2A8,
the CDVD RTC write**, unfreezes, fades the trails back up (0x22EF30) and
restores the hint bar; cancel (0x21EB30) re-reads the real RTC (0x22B838)
instead.  During the edit the hint bar is 0x21D768(1, 85, 86, 1) with mask
0xF000 (the outer slots carry the empty string 1).

## What the port does (menutext.c, + small hooks in menu.c/menuconfig.c)

The full minimal loop — enter item -> change value -> confirm/cancel — for
all five items, structured like the ROM (ConfigItem grows the +0x1C `edit`
member; the widgets own their pad input; ConfigMenuInput carries only the
two handlers' confirm/cancel arms):

* `cfgSettings[16]` stands in for 0x352880, `cfgPersisted[4]` for the NVRAM
  copy; `ConfigItemApply` prints `config save: setting N = V ("...")
  (stubbed)` on a change (plus `digital out message 20/21 (stubbed)` for
  item 2) — **no hardware side effects**;
* `EditItemValue` = 0x21EF00+0x21F080 (full row, selected blue vs dark grey,
  live setting write, dirPress left/right whose held-repeat matches the
  ROM's gp-30316);
* `EditItemLang` = 0x21F168 minus the slide (current name only, centred);
  the seven-language table is wired in InitMenuText as 0x21ED18 does for
  regions 1/2;
* the clock editor: `ClockEditOpen/Input/Apply/Cancel` + `DrawClockRow`'s
  edit colours.  **The RTC write is a printf stub**
  (`clock adjustment: set YYYY/MM/DD hh:mm:ss (RTC write stubbed)`) — aap
  decides about hardware side effects; the soft clock keeps the edited time
  (and the port's stand-in date cell) either way.  Cancel restores the
  pre-edit snapshot (the port has no RTC to re-read).  The calendar clamp is
  simplified to days-in-month (+leap): the day wraps against the real month
  length and shrinks when a month/year step leaves it over — same visible
  behaviour as the ROM's epoch clamp inside the 0x27B870 ranges;
* menu.c grows `MenuClockHold/Set/Get` (gp-30328 / 0x22B8E8's h-m-s third)
  and `MenuOrbTrailFade` (0x22EF90/0x22EF30, including the turn-a-close-
  into-an-open arm); menuconfig.c grows `MenuConfigCubeKick` (the -0.1 cube
  press kick, now actually fired by the confirm).

Verified headlessly with a scripted-pad throwaway build (`test_p2/`,
inject.c — presses at fixed frames; not in the diff): open/apply/cancel on
value items and the clock, cursor wrap, live-preview-survives-cancel
(re-open resyncs to the previewed value, as in the ROM), the German apply
(`config save: setting 3 = 4 ("German")`), the clock freeze (apply printed
18:27:47 = the argv clock + 2 s of pre-open runtime, frozen through the
edit), and the Feb-2000 day wrap (1 -> down -> 29 -> up -> 1).  Both
regression maps re-checked byte-identical after phase 2 (`p2_mainmenu.log`,
`p2_config.log`, `final_*.log`).

## Phase-2 polish not ported (each small, none load-bearing)

* the language slide animation (0x21F3A8: ±150 px, marker glyph 61);
* the 0x27EC40 gate and mode 2 (the "launched to set the clock" path);
* gp-30352 / 0x22B138 (the hint-refresh word behind 0x22B100/0x22B108/
  0x22B118) — the port's widgets set their hints directly;
* gp-30428 (0 while editing the clock, float 1.0 after — some drawer-side
  blink/factor; consumer not traced);
* 0x21DDC0 (the idle re-sync of settings 6..11 from the soft clock — the
  port's mode-0 row reads the clock directly);
* the ROM's mode-0 leave oddity: 0x2279B8's bit-0x10 arm closes the Anim
  only when the CURSOR is 0 (`lw v1,16(s0); bnez v1,skip`), and bits
  0x40/0x80 jump to 0x227338/0x227028 (other top-level screens).  The port
  keeps its existing leave-on-TRIANGLE-from-anywhere.


---

# Config-mode + clock-adjust fix (bugs A and B of 6f72b1c's phase 2)

Scratch tree: `tmp/clockfix/osdbits` = HEAD **fb7a2f5** + this fix.
`clockfix.diff` is git-apply-able against fb7a2f5 (checked with
`git apply --check`).  Files touched: `menuconfig.c`, `menutext.c`,
`menu.c`, `inc.h` (declarations only, away from the `osdBootSound`
line).  `verify-old/` / `verify-new/` are throwaway builds (baseline /
fixed + a scripted-press injector and two debug printfs, marked
`VERIFY ONLY`) used for the evidence below - not part of the diff.

## Bug A - entry fell straight into the item editor

### The real mechanism (0x227D08)

`0x227DE8` (the screen's per-frame slot) ends in `0x227D08`, and THAT
is where the pad enters the config screen:

    0x227D08:
      if(!0x22AC48(0x27BE44, 2))  ->  focus-off path, NO pad dispatch
      if(!0x22AC48(0x27EC40, 0))  ->  focus-off path, NO pad dispatch
      ...focus-on dance (gp-30404 latch, the cursor item's +0x28)...
      mode 0 -> tail-jump 0x2279B8;  mode 1 -> tail-jump 0x227BE8

(the second call's `a1 = 0` sits in the first branch's delay slot at
0x227D2C - it checks the value sub-screen timer for IDLE, not open).
So the ROM reads **no pad at all** unless the screen's Anim `0x27BE44`
is in state 2 - fully open.  The whole 90-frame opening choreography,
and the closing one, are input-dead.  In particular the confirm press
that *entered* the screen (main menu handler `0x228278` runs the same
frame the Anim leaves idle) can never fall through into the item list.

### What phase 2 got wrong

`ConfigMenuInput()` gated on `MenuConfigOpen()` = "Anim not idle".  On
a real pad, the X that selects "System Configuration" in the main menu
was still set in `pad.press` when `ConfigMenuInput` ran later the same
frame, its mode-0 confirm arm fired, and the cursor item's editor
opened - on the default cursor that is Clock Adjustment, whose open
callback grabs the soft clock and fights the opening animation.

### The fix

* `menuconfig.c`: new `MenuConfigFullyOpen()` = `cfgIsState(&cfgAnim,
  2)`, the exact 0x227D08 head (the 0x27EC40 half of the gate is
  always true here - the port never opens that timer - and stays
  dropped, documented).
* `menutext.c`: `ConfigMenuInput()` and `ConfigFocusNotify()` now gate
  on it (the ROM's focus dance sits under the same gate, not under an
  alpha threshold).

While rewriting the handler I also matched the arms of
`0x2279B8`/`0x227BE8` bit for bit against the canonical pad word the
new pad.c (fb7a2f5) publishes - both handlers read gp-30316, the
press-EDGE word (`pad.press`), and every taken arm tail-jumps out, so
they are exclusive and in ROM order:

    mode 0 (0x2279B8):  up 0x1000 / down 0x4000  wrap + the two +0x28
                          focus calls + click 5  (edges - the ROM's
                          config list does NOT auto-repeat)
                        confirm 0x20 = PAD_CIRCLE  item's +0x14, mode 1,
                          click 4  (0x21DF28 clock / 0x21EE50 rest)
                        0x80 -> 0x227028  version sub-screen, not ported
                        cancel 0x40 = PAD_CROSS -> 0x227338 = close the
                          Anim + click 10 - the real way back to the
                          main menu (the old port had confirm on
                          CROSS|CIRCLE and leave on TRIANGLE)
                        triangle 0x10  only from cursor 0 (the ROM's
                          `lw v1,16(s0); bnez` oddity - the hint bar
                          only offers "Options" there): click 4, close
                          the Anim, 0x223658 (the Options screen switch,
                          not ported, so it leaves like cancel)
    mode 1 (0x227BE8):  confirm 0x20  cube kick (*(gp-32136) = -0.1),
                          item's +0x20, mode 0, click 4, phase 0
                        cancel 0x40   item's +0x24 (0x21EB30 for the
                          clock, nop for the rest), mode 0, click 10
                        (mode 2, the "launched from a game to set the
                        clock" arm at 0x227C44, stays unported)

Entry state: `0x227268` (enter) touches neither mode nor cursor; mode
is 0 whenever the screen closes, so entry = the item LIST, and an
item's `+0x1C` editor is reached only through the mode-0 confirm.

## Bug B - the clock-adjust screen

### The real display path, function by function

**Entry, `0x21DF28`** (the clock item's +0x14), in order:

1. `0x21DDC0` - selects the 12/24-hour field table and re-syncs the
   six settings `0x22B0E8(6..11)` from the soft clock block.
2. `sw zero, -30428(gp)` - the **orb scale target** (see below).
3. `0x22EF90` - closes the 0x27F900 trail timer (trails fade down).
4. `0x22B960` - writes 0 into gp-30328.  **This word is write-only in
   the whole image** (the only accesses are the stores in 0x22B950/
   0x22B960) - it is NOT what freezes the clock; see the reseed below.
5. `*(0x22B0E8(11)) = 0` - **the SECONDS are zeroed** (0x21DF68).
6. `0x22B8E8(Y,M,D,h,m,s)` - seed the soft clock from the six fields,
   ms fraction = 0.
7. tail `j 0x21E3B8` - the calendar clamp.

**Per frame, `0x21EA20`** (the +0x1C widget): draw the row in edit
colours (`0x21E3B0`), run the pad half `0x21E870` if 0x27EC40 is idle
(left/right = field cursor in the item's +0x08, from gp-30316 edges;
up/down = value step with wrap through the 0x27B870 ranges, from
gp-30308, the repeat word), then **tail-jump `0x22B8E8` again - the
soft clock is reseeded from the six raw fields EVERY frame**.  That
per-frame reseed IS the freeze: it re-pins h/m/s and zeroes the ms
fraction faster than the frame-body tick `0x22BB30` can advance them
(it also rewrites gp-28816, so the tick's RTC-drift resync never
fires).

**Why that is a FIXED pose.**  The carousel pose is derived every
frame by `0x225628` (from `0x225BF8` -> `0x225978`) purely from the
soft-clock accessors:

    ringOffset (0x34E6C0+0) = trunc(hours 0x22B720) % 12
    spin  (0x34E6C4)  eases toward seconds * 65536/60  at *(gp-32168)=0.1
    tilt  (0x34E6C6)  SNAPS to (offset<<16)/12 while |spin| < 201,
                      eases at *(gp-32172)=0.1 otherwise
    0x34E930 = 1 - minutes/60   (the front rod's split target)
    gp-28854/-28856  the orbs' hour/second angles, same eases

With the clock pinned and the seconds zeroed at entry, spin eases to
**0** and parks, tilt then satisfies |spin| < 201 and **snaps** to the
hour slot's angle: the canonical presentation pose, rotation stopped.
The front slot (`i == ringOffset` in the colour spreader `0x225318`
and slot 0 of the emitter `0x226028`) is the ring's highlighted rod -
it carries the bright col1 `0x27EAF0` {80,80,80,1E}, the front body
blend, and the minute split.

**The highlight while editing** is the same machinery reacting to the
edited VALUES (the reseed passes the raw fields - there is no
field-cursor-driven code anywhere in the path):

* stepping the HOUR moves `ringOffset` - the ring re-fronts onto the
  new hour's rod instantly (tilt snaps, spin is ~0);
* stepping the MINUTE resizes the front rod's bright split -
  `0x225BF8`'s front-slot arm both grows the split at *(gp-32164) =
  0.004 AND (the `bc1fl` arm at 0x225C6C) **snaps it DOWN** to the new
  `1 - minutes/60` when the max shrinks;
* stepping the SECOND swings spin to the new angle and parks again.

**The orbs disappear into the centre.**  gp-30428 (0x2A7994, .data
1.0) is the target of a per-frame ease in `0x2285C0`'s head:

    *(0x27B440) += (*(gp-30428) - *(0x27B440)) * *(gp-32128 = 0.1)

and the orb emitter `0x2261B8` computes the orbit radius as

    radius = (gp-28848 * 7.25 + 10.0) * *(0x27B440)     (0x22626C)

`0x22625C` is the only reader of 0x27B440 in the image.  So writing 0
into gp-30428 on entry sends the whole orbit radius to 0 over ~1s -
the orbs sink into the centre - and the exits restore it:
**Apply `0x21EAE0`**: `0x22B2A8` (CDVD RTC write), gp-30428 = 1.0f
(0x21EAFC), `0x22EF30` (trails back up), `0x22B950`, hint restore.
**Cancel `0x21EB30`**: `0x22B838` (re-read the real RTC, which never
stopped), then the same 1.0f / trails / hints.
(The browser transition writes 0x27B440 and gp-30428 too - 0x224278 /
0x2245F0 - and 0x21CE40 snaps 0x27B440 to 1.0 at module start; neither
is this screen's business.)

### What phase 2 got wrong

* No orb-scale machinery at all: `menuScale` (the port's 0x27B440) was
  a constant 1.0, so the orbs stayed on the ring through the edit and
  every up/down press flung them around (`RotY(minutes*65536/60*1100)`
  and `RotX((i+21)*seconds*65536/60)` are direct clock terms - in the
  ROM they are invisible during the edit because the radius is 0).
* The seconds were NOT zeroed at entry, so the pose froze wherever the
  live second hand happened to be instead of parking at spin 0 with
  the tilt snapped - and the ROM's open-time seed (`0x22B8E8` at
  0x21DFC4) was skipped, leaving the first frame on the old clock.
* The front rod's split never shrank (the 0x225C6C snap-down arm was
  missing), so a minute step UP left the bright segment too long.
* The doc's phase-2 write-up read the per-frame reseed as "what makes
  the carousel follow the edit live" - it is the opposite: it is the
  freeze (gp-30328 being write-only, the reseed is the ONLY freeze),
  and the pose only moves when a field value actually changes.

### The fix

* `menu.c`: `menuScaleTarget` (real gp-30428) + exported setter
  `MenuOrbScaleTarget()`; the 0x2285C0 ease
  `menuScale += (menuScaleTarget - menuScale)*0.1f` in `MenuFrame`,
  in the frame-body slot right after `MenuConfigCarousel()` (the ROM
  runs 0x2285C0 right after 0x225BF8).  Init target = 1.0 (the .data
  value), live = 1.0 (0x21CE40's snap), so the idle menu is
  bit-identical to before.  Also documented that gp-30328 is
  write-only in the image and the port's `clockHold` is the
  one-frame-exact stand-in for the ROM's reseed-pinned clock.
* `menutext.c` `ClockEditOpen`: reordered to 0x21DF28's exact order -
  sync, `MenuOrbScaleTarget(0)`, trail fade, hold, **`cfgSettings[11]
  = 0`**, the open-time `MenuClockSet` seed, `ClockClampDay()` (the
  0x21E3B8 tail).  The cancel snapshot keeps the real seconds (the
  ROM's cancel re-reads the RTC, which never stopped - the port has no
  RTC, documented).
* `menutext.c` `ClockEditApply`/`ClockEditCancel`: +
  `MenuOrbScaleTarget(1.0f)` (0x21EAFC / 0x21EB4C).
* `menuconfig.c` `MenuConfigCarousel`: the front slot's split snap-down
  arm (`else split = max`, real 0x225C6C's bc1fl).
* `EditItemClock` keeps the per-frame reseed (it is real) with the
  corrected comment.

## Matching status

These functions are C reconstructions in osdbits, not byte-matched
TUs - none of menuconfig.c/menutext.c/menu.c is in `matching/`'s bound
tables, so "matching" here means instruction-level behavioural
equivalence, verified arm by arm against the disassembly:

| real | port | status |
|---|---|---|
| 0x227D08 head | MenuConfigFullyOpen + ConfigFocusNotify + the ConfigMenuInput gate | exact (0x27EC40 half of the gate constant-true, documented) |
| 0x2279B8 | ConfigMenuInput mode 0 | all six arms, ROM order/exclusivity/click ids; 0x80 (version screen) and 0x223658 (Options screen) unported - both documented, triangle leaves instead |
| 0x227BE8 | ConfigMenuInput mode 1 | exact incl. cube kick and click ids; mode 2 arm unported |
| 0x21DF28 | ClockEditOpen | exact order incl. the seconds zero and open seed; RTC snapshot is the port's argv clock |
| 0x21EA20/0x21E870 | EditItemClock/ClockEditInput | unchanged from HEAD (already matched: gp-30316 edges for left/right, gp-30308 repeat for up/down, per-frame reseed) |
| 0x21EAE0/0x21EB30 | ClockEditApply/Cancel | + the gp-30428 = 1.0 write; RTC write/re-read remain printf/snapshot stubs |
| 0x2285C0 head | the menuScale ease | exact (rate 0.1 read from gp-32128 in the image) |
| 0x2261B8's radius | UpdateOrbs (unchanged) | already multiplied by menuScale; only the factor was dead |
| 0x225BF8 front-slot split | MenuConfigCarousel | exact (grow 0.004, clamp, snap-down) |

Left alone, pre-existing and documented in the sources: the orbs'
gp-28854/-28856 eased hour/second angles (port seeds once - invisible
during the edit, radius is 0), the 0x27EC40 sub-screen timer, the
gp-30352 hint-refresh word, DrawConfigMenu's `alpha >= 128` marker
guard.

## Verification (windowless PCSX2, display :93, llvmpipe)

Logs live next to this file.  All runs used the full --nosocket
wrapper; the two verify builds add a scripted-press injector
(confirm at frames 1 and 130, RIGHT at 150/155/160 = field cursor to
hour, UP at 170 = hour 18 -> 19) plus per-50-frame `carousel ...` /
`orbscale ...` printfs - `VERIFY ONLY`, not in the diff.

**Main-menu regression** - argv
`menu 12 34 56 0 1 128 60 0 0 0 10 0 0 0 1`, pristine fb7a2f5 build
(`base_mainmenu.log`) vs fixed build (`fix_mainmenu.log`): the
28-line frame-60 luminance maps (`grep -o '|.*|'`) are
**byte-identical** (base_map.txt == fix_map.txt).

**Config entry + clock adjust** - argv
`menu 18 27 45 0 1 128 260 0 0 0 10 0 1 0 0` (clock 18:27:45,
cfgEnter frame 1, debug/exit frame 260, meshTex 0, cursor default 0 =
Clock Adjustment; note the last token lands on OsdArgInt(14) =
meshTex - cfgCursor would be a 17th token, over the 16-token cap, so
the cursor rides on its default).

`old_config.log` (baseline + injector) - both bugs on display:

    [0.8823] osdsys: enter System Configuration
    [0.8869] osdsys: config item 0 ("Clock Adjustment") opened   <- SAME frame: bug A
    [1.70]   carousel off=6 spin=-16304 tilt=-32768 ... ; orbscale 1.0000
    [3.03]   ...set 2000/01/01 18:27:45 (RTC write stubbed)      <- frame-130 press APPLIES
    [3.36..5.03] spin=-16159 -> -15271 -> -14360 ; orbscale 1.0000

i.e. entry dropped straight into the editor; inside it the pose froze
mid-swing at the live seconds (spin -16304, not 0), the orbs never
left the ring (orbscale pinned 1.0), and after the accidental apply
the carousel is visibly rotating again by the dump frame.

`new_config.log` (fixed + injector):

    [1.0083] osdsys: enter System Configuration      <- frame-1 confirm IGNORED
    (frames 10..120: item list; spin eases with the live clock,
     orbscale 1.0 - normal list mode through the opening)
    [3.1617] osdsys: config item 0 ("Clock Adjustment") opened   <- the frame-130
             confirm, first frame the Anim is fully open is 91
    [3.49]   carousel off=6 spin=-1552  tilt=-32768 splitmax=0.550 ; orbscale 0.1094
    [4.33]   carousel off=7 spin=-4     tilt=-27307 splitmax=0.550 ; orbscale 0.0006
    [5.16]   carousel off=7 spin=0      tilt=-27307               ; orbscale 0.0000

* entry lands in the item LIST (no editor until the fully-open
  confirm) - bug A fixed;
* in the editor the spin parks at exactly **0** (seconds zeroed) and
  the tilt **snaps** to the hour slot - after the hour edit,
  -27307 = (7<<16)/12 as s16, off=7 = 19%12: the ring re-fronted onto
  the new hour's rod, i.e. the edited rod is the highlighted one -
  and splitmax snapped to 1 - 27/60 = 0.550;
* orbscale 1.0 -> 0.0000: the orbs sink into the centre.

The frame-260 MeshDebug rod lines show the pose itself: the fixed
build's twelve rods sit in a symmetric, level ring (front rod at
y +0.2, pairs at y ±22.5/23.0, ±39.6/40.2, ±46.4/47.0, ±40.9/41.1,
±24.0/23.8, back rod y -0.2 - spin 0, edge-on), where the old build's
are bunched mid-swing (x -73..-21, y asymmetric) and still moving.
The five cube lines are identical between the two runs (the cube
stage is untouched).

## Loose ends for aap

* The prescribed verification argv's last token is OsdArgInt(14) =
  meshTex, not cfgCursor - see above; nothing to fix, just naming.
* Mode-0 leave is now the CANCEL button (0x40 -> 0x227338), exactly as
  retail; TRIANGLE only acts on item 0 (retail sends it to the Options
  screen 0x223658, which the port lacks, so it leaves too).  If the
  triangle-from-anywhere leave was a UX preference, it is a two-line
  revert in ConfigMenuInput's last arm.
* The config item list no longer auto-repeats up/down (gp-30316 is the
  edge word; only the clock editor's up/down, gp-30308, repeats) -
  ROM-exact, but worth knowing it is deliberate.
* Retail's 0x27B440 starts at .data 0.0 and 0x21CE40 snaps it to 1.0
  at module start; the port never sees the 0.0 (init order), matching
  the settled state.  The browser transition's writes (0x224278/
  0x2245F0) remain unported.
