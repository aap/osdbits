# Module U — the high-level menu logic

Companion to `osdsys-map.md` §3.  Scope: the ThreadU loop, the screen
state machine, screen lifecycle, pad input, the main menu, and the
table-driven configuration-page engine.  **Not** covered here: the 2D
item/value rendering, the 3D clock/orb background, and the shared text
engine (0x208130–0x20B600), which stays a black box.

Everything under **VERIFIED** is read straight out of
`/u/aap/src/osdsys/expanded.bin` (loaded at 0x200000); `[tnt]` marks a
tentative reading.  gp = 0x2AF070 throughout.

---

## 0. Headline corrections to `osdsys-map.md`

These change the picture enough to state up front.

1. **Screen ids 100–116 are CDVD disc types, not UI screens.**  §3.2's
   "jump table 0x2A46C0 (11 entries, ids 106…116)" is a *disc-state*
   dispatch: `0x20B9F8` (in ThreadA) maps `*(u8*)0x1F40200F`
   — the CDVD disc-type register — onto 100…116 and writes the result to
   `0x1F0010`.  §2, §3.2 and the opening's 0x212010 all key off the same
   namespace, which is why the three modules look like they share
   "screens".  See §1.
2. **`0x21CF20` is the per-frame update/draw fan-out, not an
   initialiser.**  It is called once per frame from inside 0x21CA38's
   loop (0x21CC4C), after the disc-state dispatch and before
   `waitNextFrame`.  Likewise `0x2283F0` is the per-frame *screen* hub,
   and its eleven callees are per-frame handlers, each self-gated on its
   own screen's transition state.
3. **The record table at 0x2A4380 is not Module U's.**  It is the tail
   of a 16-byte `{x, y, w, h}` texture-rect array based at **0x2A4318**,
   consumed by the *opening* module's `0x214A60` (with the index
   `IsPAL() ? 32 : 0` selecting between paired NTSC/PAL rects) and by
   `DoSCEText` 0x214CB0 via 0x2A4578/0x2A4588.  §7's labelled gap can be
   closed.
4. **The UI-model's first two fields are swapped in §3.6.**  0x22B138
   stores `getScreenType()` at **+0** and `getSpdif()` at **+4** (the
   getter calls and the stores are one instruction apart in the delay
   slots — see §7.4).  The static item tables independently confirm it.
5. `0x21EF00` (listed in §3.2 as a config screen) is **dead code**, as
   are 15 other Module U functions — see §8.

---

## 1. The screen-id namespace = CDVD disc types  [VERIFIED]

`0x20B9F8`, reached from `ThreadA` (0x20B830), reads the CDVD disc-type
register at **0x1F40200F** and maps it:

| disc type | screen id | libcdvd 1.x name |
|---|---|---|
| 0x00 | 100 | `SCECdNODISC` |
| 0x01 | 101 | `SCECdDETECT` |
| 0x02 | 102 | `SCECdDETECT_CD` |
| 0x03 | 103 | `SCECdDETECT_DVDS` |
| 0x04 | 104 | `SCECdDETECT_DVDD` |
| 0x05 | 105 | `SCECdUNKNOWN` |
| 0x10 | 106 | `SCECdPSCD` |
| 0x11 | 107 | `SCECdPSCDDA` |
| 0x12 | 108 | `SCECdPS2CD` |
| 0x13 | 109 | `SCECdPS2CDDA` |
| 0x14 | 110 | `SCECdPS2DVD` |
| 0x20 | 111 | (undocumented) |
| 0x21 | 112 | (undocumented) |
| 0x22 | 113 | (undocumented) |
| 0xFD | 114 | `SCECdCDDA` |
| 0xFE | 115 | `SCECdDVDV` |
| 0xFF | 116 | `SCECdIllegalMedia` |
| other | 101 | — |

(0x12/0x13/0x14 share one code path at 0x20BB6C; anything in 0x15…0x1F
also lands there and resolves to 110.)

`0x1F0010` ("screen id") is written in exactly three places in the whole
image:

* `main` 0x207A48 seeds it with **101** (and 0x1F0014 = −1, 0x1F00B0 = 5);
* `0x20B9F8` 0x20BC3C, every ThreadA poll, with the table above;
* `0x20B7C0`, which forces **116** (illegal media) — called from
  `main` and from 0x2022A0.

Module U never writes it; it only reads it (0x21CA68, 0x21CAA0).  It also
keeps a shadow copy of "the id I have already reacted to" in
`gp−28884` = **0x2A7F9C**.

### 1.1 What a module does when the id changes

All three screen modules have the same tail: publish a request in the
low block and set `0x1F05E4 = 1` ("module wants out").  `main`'s loop
(0x207D2C…) then does:

```
if (*(int*)0x1F0014 != -1) {
    r = 0x202E88(...);          /* actually boot/launch the disc */
    *(int*)0x1F0014 = -1;
    switch (r) { 3: systemState = 4;
                 5: systemState = 3, bootMode(0x1F0018) = 1,
                    strcpy(0x1F001C, getString(96));
                 default: systemState = 3; }
}
0x206CC0(); 0x24DCE0(2);
WakeupThread( threadIds[ systemState==4 ? 0 : systemState==5 ? 2
                                        : systemState-1 ] );
```

`threadIds` is **0x2B92E0**, filled by `0x2072E8` (called from `main`)
by walking the module registry and calling each descriptor's `setup`,
*compacting out* the modules whose setup returns < 0.  With module 0
(Console) having no thread, the array is
`[0]=OpeningThread, [1]=ThreadU, [2]=ThreadV, …`, so:

| `0x1F05E8` (systemState) | wakes |
|---|---|
| 1 | opening |
| 2 | **ThreadU** (main menu / clock) |
| 3 | ThreadV (browser) |
| 4 | opening again — the *illegal disc* scene |
| 5 | ThreadV again — the *CD player* |

`0x1F0014` is therefore a **boot/launch request** (−1 = none), and
`0x1F05E8` says which module to run when nothing is booted.

Module U's exit map (0x21CC78, dispatching on its *shadow* id):

| screen id | Module U does |
|---|---|
| 106, 107 (PS1 disc) | `0x1F0014 = 2` |
| 108, 109 (PS2 CD) | `0x1F0014 = 1` |
| 110 (PS2 DVD) | `0x1F0014 = 0` |
| 113 | `0x1F0014 = 5` |
| 115 (DVD Video) | `0x1F0014 = 3` |
| 114 (audio CD) | systemState = 5 → CD player |
| 116 (illegal) | systemState = 4 → illegal scene |
| 111, 112, < 106, else | systemState = 3 → browser |

and always `0x1F05EC = 2` (id of the requesting module) on the
systemState paths, `0x1F05E4 = 1` in all cases.

The opening's identical routine is **0x212010** (jump table 0x2A4120);
its `0x1F0014` values for 106…110/115 are the same 2/2/1/1/0/3, which is
the cross-check that these constants are *boot modes*, not UI ids.
Module V's entry (§2.3 of the map) maps the same ids to the same
internal modes 2/2/1/1/0/3.  Note the opening differs from Module U on
the exotic types: it sets `0x1F0014 = 5` for 111 and `= 4` for 112.

`0x1F0CF8`, which gates screen 114 in both the opening (0x2120A0: audio
CD → CD player if > 0, else the clock) and Module U (0x21CADC,
0x21CC30), is **never written anywhere in the image** — it must be set
by the caller/BIOS before OSDSYS runs.  **Labelled gap.**

---

## 2. ThreadU and the frame loop  [VERIFIED]

`makeThreadU` 0x21C910: `CreateThread{entry 0x21CDD8, stack 0x328E00,
size 0x20000, gp 0x2AF070, prio 6}` then `StartThread`.  Registered as
module #2 by 0x21C980 (`{setup = makeThreadU, getDesc = 0x21C8F0,
getVersion = 0x21C900}`, everything else NULL).

`ThreadU` 0x21CDD8 is as the map describes.  The real work is
**0x21CA38**, which is not a "per-frame body" but the module's *whole
run*, containing its own frame loop:

```c
/* 0x21CA38 */
curId = (*(int*)0x1F05EC == 1) ? 100 : *(int*)0x1F0010;
*(int*)0x1F05E4 = 0;
phase = 0;                        /* 0x27B444 */
0x21CE58();                       /* 0x22EE88, 0x22BE18, 0x22B838, modelReload */
StartFrame();                     /* 0x205E88 */
*(int*)0x27B448 = *(int*)0x1F0C44;        /* evenOddField */
for (;;) {
    req = *(int*)0x1F0010;
    if (phase == 0) {
        if (req != curId) {
            if (106 <= req && req <= 116) {
                /* enterTab, 0x2A46C0: id 114 -> 0x21CAD8, all else 0x21CAE8 */
                if (req == 114 && *(int*)0x1F0CF8 <= 0)
                    goto justFlag;        /* audio CD but no CD player */
                if (!0x226980()) phase = 1;   /* nothing on screen yet */
            }
            curId = req;
justFlag:   *(int*)0x1F0CEC = 1;
        }
        if (0x2269F0()) {                 /* main menu finished fading out */
            if (curId != 116) curId = 9999;
            phase = 1;
        }
    } else {
        if (0x226950()) {                 /* the first-boot wizard is up */
            if (req != curId) {
                if (106 <= req && req <= 116) goto *leaveTab[req-106]; /* 0x2A4720 */
                curId = req; *(int*)0x1F0CEC = 1;
            }
        } else if (getFadeMode()) {                       /* 0x22AD30 */
            if (getFadeAlpha() <= 0) goto leaveModule;    /* 0x22AD28 */
        } else {
            /* frameTab[curId-106], 0x2A46F0 */
            postMsg(20501, 2, 0, snd);    /* snd: 15 for 106/107/115,
                                             14 for 108/109/110, else 0 */
            0x22ADD8(3);                  /* start the fade to black */
        }
    }
    0x21CF20();                           /* the per-frame fan-out */
    waitNextFrame();                      /* 0x205F30 */
    *(int*)0x27B448 = *(int*)0x1F0C44;
    SwapBuffers();                        /* 0x205DC0 */
}
leaveModule:  /* the 0x21CC78 exit map of §1.1, then 0x1F05E4 = 1,
                 0x266108(), 0x21CFD0(), and two 64-bit patches at
                 0x1F0A60 / 0x1F0B50 (clear the low 15 bits, or in 14) */
```

The three jump tables are all bounds-checked slices of the same 11
entries and all reduce to one of two stubs — `0x21CAE8`, or `0x21CAD8`
for id **114** only, which additionally requires `0x1F0CF8 > 0`
(i.e. "only treat an audio CD as a screen change if the CD player
exists").  There is no per-screen behaviour hidden in them.

`0x226980` / `0x2269F0` / `0x226950` are simple predicates over screen
transition objects (§4):

* `0x226980()` — "is anything on screen?": 0 iff 0x27BE44, 0x27BF50 and
  `getClockAnim()`'s object are *all* in state 0.
* `0x2269F0()` — "did the main menu just finish fading out?":
  `state(0x27BEA8) == 3 && dirty(0x27BEA8)`.
* `0x226950()` — `!anmIsState(getWizardAnim(), 0)`: returns 1 while the
  first-boot wizard object is on screen, which is what keeps the module
  from starting its fade-out and leaving.

`0x22ADD8(mode)` arms the whole-screen fade: it stores `mode` in
`gp−28828` (= 0x2A7FD4) and sets the alpha `gp−28824` (= 0x2A7FD8) to
0 (modes 2 and 4), 255 (mode 1) or 128 (mode 3, which also starts the
0x27F620 object), plus `0x27F650/0x27F654 = screen_w/h << 4`.
`0x22AD30` reads the mode back, `0x22AD28` the alpha; the loop above
leaves the module when a fade is running and its alpha has reached 0.

### 2.1 The per-frame fan-out `0x21CF20`

Sixteen calls, in order, with two 64-byte stack scratch buffers passed
to the first three:

```
0x21CFD8(&a,&b) 0x21D0A0(&a,&b) 0x2268F0(&a,&b) 0x22B020 0x2283F0
0x21D368 0x21D3A0 0x21DA68 0x21DB18 0x225BF8 0x2285C0 0x2287D0
0x22B058 0x22BB30 0x22B588 0x22BE30
```

`0x2283F0` is the screen hub; **`0x22BE30` (last) is the pad input
sampler** (§5); the rest is the 3D scene and 2D chrome (sibling agents).

---

## 3. The screen hub `0x2283F0`  [VERIFIED]

Eleven per-frame handlers plus a tail-jump.  Each begins by ticking its
own transition object and then self-gates, so the hub itself is
unconditional:

| handler | object it ticks | screen |
|---|---|---|
| `0x2283D0` | — | page dispatch: `if ((u32)pageIndex >= 5) 0x22C3C0(pageIndex-5)` |
| `0x226FA8` | 0x27EC00 | background / clock scene |
| `0x227198` | 0x27EC40 | background 2 |
| `0x2202C8` | `curPopup->anim` | the option **popup** (0x21FA68, 0x21FAD0, 0x220270) |
| `0x2217A8` | 0x27BF50 | **Version Information** (0x2211B0, 0x221230, 0x2215E0) |
| `0x227DE8` | 0x27BE44 | **System Configuration** (0x227390, 0x227560, 0x227D08) |
| `0x2283A0` | 0x27BEA8 | **main menu** (0x228050, 0x228110, 0x228278) |
| `0x224630` | 0x27DA70 | **first-boot wizard** panels (strings 153–160) |
| `0x221D78` | `curPage->anim` | the **config page** renderer |
| `0x2221B8` | 0x27BF60/0x27BF70/`curPage->anim` | the **config page** driver (§7.3) |
| `0x224288` | 0x27DA70 | wizard driver — opens the User Preferences page |
| `0x2236B8` (tail) | 0x27C258 | clock/time screen — opens the Options page |

`0x227FC0` is the shared "is the main menu allowed to take input?"
predicate: true only when 0x27BE44, 0x27BF50, `getClockAnim()` and
`getWizardAnim()` are all in state 0 **and** `getCurPage() == 0`.

---

## 4. The transition object  [VERIFIED, byte-matched]

A 16-byte struct that every screen, popup, page and value-carousel owns.
This is the single most reused primitive in Module U.

```c
typedef struct Anim {
	int duration;   /* +0  frames                        */
	int timer;      /* +4  0..duration                   */
	int dirty;      /* +8  set on the frame state changed */
	int state;      /* +12 0 hidden, 1 in, 2 shown, 3 out */
} Anim;
```

| addr | name | body |
|---|---|---|
| 0x22AC18 | `anmTimer` | `a->timer` |
| 0x22AC20 | `anmScaled` | `a->timer * n / a->duration` |
| 0x22AC48 | `anmIsState` | `a->state == s` |
| 0x22AC58 | `anmDirty` | `a->dirty` |
| 0x22AC60 | `anmReset` | timer = dirty = state = 0 |
| 0x22AC70 | `anmFadeIn` | if state == 0 → timer = 0, dirty = 1, state = 1 |
| 0x22AC90 | `anmFadeOut` | if state == 2 → timer = duration, dirty = 1, state = 3 |
| 0x22ACC0 | `anmTick` | dirty = 0; state 1: ++timer, on == duration → dirty, state 2; state 3: −−timer, on 0 → dirty, state 0 |

All eight are reconstructed and **byte-match** (see §10).

The Module U screen objects, and the header each one hangs off:

| header | object | screen |
|---|---|---|
| 0x27BE28 | 0x27BE44 (+0x1c) | System Configuration |
| 0x27BE90 | 0x27BEA8 (+0x18) | main menu |
| 0x27BF38 | 0x27BF50 (+0x18) | Version Information |
| 0x27C200 | 0x27C258 | clock / time screen (`getClockAnim` 0x223790) |
| 0x27DED8 | 0x27DA70 | first-boot wizard (`getWizardAnim` 0x224D68) |
| 0x27F620 | 0x27F620 | the whole-screen fade/curtain |
| 0x27EC00, 0x27EC40 | | 3D background |
| 0x27BF60, 0x27BF70 | | the config page's cursor-move animation pair |

---

## 5. Pad input  [VERIFIED]

`ThreadX` (0x206E00) polls the pads, ANDs the raw button bytes across
all connected ports, and copies `scePadRead`'s four leading bytes into
the low block at **0x1F0C58…0x1F0C5B** (0x20707C–0x207094), with
`0x1F0C78` = the port state.  Immediately before the copy (0x207024) it
calls the region/version check **`0x204318`** and, if it returns
non-zero, **swaps bits 0x20 and 0x40 of byte 3** — the CIRCLE/CROSS
exchange.  The application therefore always sees one fixed meaning for
those two bits regardless of the console's region; the same 0x204318
gates the "Back"/"Enter" label swap in `osdGetString` (map §4.1) and in
`0x21D768` (§6.2).

Module U's sampler is **`0x22BE30`**, the last call in the per-frame
fan-out:

```c
prev = padCur;  padCur = 0;                       /* gp-30320 = 0x2A7A00 */
if (*(int*)0x1F0C78 == 2 || *(int*)0x1F0C78 == 6)
	padCur = ~((*(u8*)0x1F0C5A << 8) | *(u8*)0x1F0C5B);
chg = padCur ^ prev;
padUp   = chg & ~padCur;                          /* gp-30312 = 0x2A7A08 */
padDown = chg &  padCur;                          /* gp-30316 = 0x2A7A04 */
padRep  = 0;                                      /* gp-30308 = 0x2A7A0C */
/* auto-repeat, identically for 0x1000 and 0x4000: */
n = (padCur & BIT) ? ++counter : -1;              /* gp-28800 / gp-28796 */
if (n == 0)        padRep |= BIT;                 /* first frame */
else if (n >= 31 && n % 3 == 0) padRep |= BIT;    /* then every 3rd frame */
```

So: a half-second hold, then 20 Hz repeat, on exactly two bits.

**Button constants** (derived from the auto-repeat pair being the two
cursor keys, and from the CIRCLE/CROSS swap position):

```
0x0001 L2        0x0100 SELECT
0x0002 R2        0x0200 L3
0x0004 L1        0x0400 R3
0x0008 R1        0x0800 START
0x0010 TRIANGLE  0x1000 UP     <- auto-repeat
0x0020 "Enter"   0x2000 RIGHT
0x0040 "Back"    0x4000 DOWN   <- auto-repeat
0x0080 SQUARE    0x8000 LEFT
```

0x0020/0x0040 are the CIRCLE/CROSS pair after ThreadX's regional swap;
every Module U handler treats **0x20 as accept and 0x40 as cancel**
(0x228328/0x22837C, 0x22245C/0x222524, 0x224438).  `gp−30316`
(newly-pressed) is what every handler reads; `gp−30308` (repeat) is used
only by the dead 0x21E870 and by the clock-digit spinners.

---

## 6. The main menu  [VERIFIED]

### 6.1 The item list

The header at **0x27BE90** is static initialised data in the image:

```
0x27BE90:  title=1  items=0x27BE80  count=2  rows=3  cursor=0  top=0  modal=0
0x27BEA8:  Anim
```

and the two 8-byte items at 0x27BE80 are

| # | string id | text |
|---|---|---|
| 0 | 90 | "Browser" |
| 1 | 91 | "System Configuration" |

(the second word of each record is 0x2A7898 for both — not read by the
logic path; it belongs to the draw side.)

`0x228110` is the draw loop and shows the model: for `i < hdr->count`,
`osdGetString(items[i].strid)` is drawn at `y = centre−14 + 16*i` via
`0x21DC88(430, y, palette, …)`, with palette 0x27B830 for
`i == hdr->cursor` and 0x27B840 otherwise.

### 6.2 Input — `0x228278`

Guarded by `anmIsState(0x27BEA8, 2) && 0x227FC0() && *(int*)0x27B444 == 0`.

```
0x1000 UP    : if (--cursor < 0) cursor unchanged
0x4000 DOWN  : if (++cursor >= count) cursor unchanged;  both play snd 6
0x0020       : cursor 0 and no fade running -> 0x227F50(0): fade the main
                 menu out; the module then exits to the browser
               cursor 1                     -> 0x227268(): open System
                 Configuration (0x27BE44 fade-in, 0x22AEC8, 0x2291E8,
                 0x225AD0, snd 4)
0x0010 TRI   : 0x2210C8(): open Version Information — only if
                 0x1F00B0 == 5 and 0x1F00A4 == 0, then post ThreadY
                 command 22 ("rebuild the Version Information table",
                 map §1.5) and fade 0x27BF50 in
```

Cursor motion here is **clamped, not wrapped** (unlike the config pages,
§7.3).

### 6.3 The legend bar

Both the main menu and every config page publish their button hints
through one shared block at **0x27B5D0**:

* `0x21D768(a, b, c, d)` — writes the four `osdGetString` ids for the
  SQUARE / "Back"-slot / "Enter"-slot / TRIANGLE hints.  If `0x204318()`
  is non-zero it **exchanges the ids 85 ("Back") and 86 ("Enter")**
  between the middle two slots, so the printed label follows the same
  regional convention as the physical button.
* `0x21D758(mask)` → 0x27B5E0, the pad-button mask that decides which
  arrows/labels are visible.
* `0x21D750(v)` → gp−30772, `0x21D748(v)` → gp−30768: two more legend
  flags.

### 6.4 Version Information

Header 0x27BF38 (`title = 89`), its item array pointer is filled at
runtime (0x34E5B8), cursor at +16, count at +8.  Input is `0x2215E0`:
UP/DOWN over the module list, `0x0010` on a module that has an option
string opens its popup (`0x21F9A8`), `0x0040` leaves.

The per-module option string comes from the **Version Information table
at 0x1F1238**, whose record is 12 bytes `{name, version, optionString}`
— `0x1F1240` is `record[0].optionString`, which is why
`osdRegisterAllModules`'s epilogue stores `getString(100)`
("Diagnosis,Off,On\n") there and `getString(105)` ("Disc Speed,…") at
`0x1F1264` = `record[3].optionString`.  (Map §1.6 lists those two as
independent scratch words; they are fields of this table.)  0x2215E0
reads `*(u32*)(0x1F1240 + cursor*12)` to decide whether to show the
"Options" hint (string 87) at all.

`0x21F9A8(i)` sets `curPopup = 0x34DBE0 + i*84` and fades that popup in
— the popup descriptors are the same 84-byte page struct as §7, one per
module, built in bss.

---

## 7. The configuration-page engine  [VERIFIED]

This is the "table-driven config screens" the map flagged.  It is one
generic engine driven by two globals:

* `gp−30624` = **0x2A78D0** = `curPage`, the open configuration page
  (accessor `0x221908`, cleared by `0x221B68`);
* `gp−30632` = **0x2A78C8** = `curPopup`, the option popup opened on top
  of a list (Version Information's "Options").

Both point at the same struct type.

### 7.1 `Page` — 84 bytes

```c
typedef struct Page {
	int   title;      /* +0x00  osdGetString id                    */
	Item *items;      /* +0x04                                     */
	int   nitems;     /* +0x08                                     */
	int   rows;       /* +0x0c  visible rows                       */
	int   cursor;     /* +0x10                                     */
	int   top;        /* +0x14  first visible row                  */
	int   mode;       /* +0x18  0 browse 1 edit 2 commit 3 step    */
	Anim  anim;       /* +0x1c                                     */
	int   f2c, f30;   /* −1 on the two wizard-style pages          */
	int   scroll;     /* +0x34  += 310/frame, wraps: a blink phase */
	int   slide;      /* +0x38  ±150, the value-carousel slide     */
	int   buttons;    /* +0x3c  accepted-button mask               */
	int   lblSquare;  /* +0x40  hint string ids, one per button    */
	int   lblCross;   /* +0x44  (the 0x40 slot)                    */
	int   lblCircle;  /* +0x48  (the 0x20 slot)                    */
	int   lblTriangle;/* +0x4c                                     */
	Vt   *vt;         /* +0x50                                     */
} Page;
```

`Vt` is a table of optional overrides; a non-zero return means "handled,
skip the default":

```
+0x00 open()            +0x14 up(Page*)        +0x28 input(Page*, int *pressed, int *mask)
+0x04 close()           +0x18 down(Page*)      +0x2c frame(Page*)
+0x08 enter()           +0x1c activate()
+0x0c square()          +0x20 left()
+0x10 cancel()          +0x24 right()
```

### 7.2 `Item` — 56 bytes, `Option` — 48 bytes

```c
typedef struct Item {
	int   label;      /* +0x00  osdGetString id                     */
	int   nvalues;    /* +0x04  option count (0 = built at runtime) */
	int   value;      /* +0x08  selected option index               */
	int   field;      /* +0x0c  WORD index into the UI model        */
	Option *values;   /* +0x10  stride 48                           */
	void (*activate)();/* +0x14  0x2229B8; NULL -> modelFreeze()    */
	void *h18, *h1c;
	void *h20, *h24;  /* left / right overrides (0x222A60/0x222B40) */
	void (*cursorHook)(Item*, int);  /* +0x28  0x222798             */
	void (*init)(Item*);             /* +0x2c  0x2228F0             */
	void (*changed)(Item*, int);     /* +0x30  0x2228B0             */
	void *h34;        /* +0x34  edit-mode input filter (0x222558)   */
} Item;

typedef struct Option { int value; int label; /* + 40 unused bytes */ } Option;
```

`Option.value` is the raw number written into the UI model;
`Option.label` its `osdGetString` id.  The 40 trailing bytes are zero in
every static table and are presumably per-option geometry used by the
renderer.

**`0x222920` — the default item initialiser** (called by `0x2228F0`
when `item->init` is NULL) is the piece that ties an item to the model:

```c
cur = *uiModelField(it->field);            /* 0x22B0E8 */
if (it->values[it->value].value == cur) return;
for (i = 0; i < it->nvalues; i++)
	if (it->values[i].value == cur) { it->value = i; break; }
```

i.e. *the option list is the source of truth for the values, and
`item->field` is a word index into the model at 0x352880* — there is no
per-field getter/setter callback pair at all.  The HDDOSD names
`clock_config_change_cb_aspect_ratio` etc. do **not** correspond to a
per-field callback table in retail 1.20; the closest thing is
`Item.activate`/`Item.changed`, which most items leave NULL.

### 7.3 Behaviour

`0x2219E0(page)` — **open**:

```c
curPage = page;
0x27BF60 = 0x27BF70 = (IsPAL()?50:60)*16/60;      /* anim durations */
gp-28872 = page->cursor;
anmReset(0x27BF60); anmReset(0x27BF70); anmFadeIn(0x27BF70);
pageResetScroll();                                 /* 0x2219D0 */
for (i = 0; i < page->nitems; i++) 0x2228F0(&page->items[i]);
gp-28876 = page->mode ? 1 : 0;   /* "wizard mode": step through items */
if (page->mode) 0x2229B8();
if (page->vt && page->vt->open && page->vt->open()) return;
if (anmIsState(&page->anim, 0)) anmFadeIn(&page->anim);
```

`0x2221B8` — the per-frame driver.  It ticks the two cursor-move anims
and the page anim, advances `page->scroll += 310` (wrapping), calls
`vt->frame(page)`, and then dispatches on `page->mode`:

| mode | handler | meaning |
|---|---|---|
| 0 | `0x2223D8` | browse: move the cursor between rows |
| 1 | `0x222558` | edit: change the focused row's value |
| 2 | inline | commit — waits for `0x1F00B0 == 8`/`5`, then clears `0x1F00A4` |
| 3 | inline | wizard step — advance to the next item, or close on the last |

**Browse mode `0x2223D8`:**

```c
pressed = padPressed;               /* gp-30316 */
mask    = page->buttons & ~0xA000;  /* LEFT and RIGHT are not cursor keys here */
if (vt && vt->input && vt->input(page, &pressed, &mask)) return;
0x21D768(mask&0x80 ? page->lblSquare  : 1, mask&0x40 ? page->lblCross    : 1,
         mask&0x20 ? page->lblCircle  : 1, mask&0x10 ? page->lblTriangle : 1);
0x21D758(mask); 0x21D750(128); 0x21D748(1);
switch (pressed & mask) {
  0x1000: 0x221C38();   /* cursor up   */
  0x4000: 0x221BD8();   /* cursor down */
  0x0020: 0x221CD0();   /* accept: activate the focused row */
  0x0080: 0x221D40();   /* vt->square()                     */
  0x0040: 0x221B70();   /* back: vt->close(), then fade out */
  0x0010: 0x221C98();   /* vt->enter()                      */
}
```

Cursor motion here **wraps**: `0x221BD8` sets `cursor+1`, or 0 past the
end; `0x221C38` sets `cursor−1`, or `nitems` when negative (clamped by
`0x2226B8`).  `0x2226B8(new)` snapshots the old cursor in `gp−28872`,
restarts the 0x27BF60/0x27BF70 slide pair, fires `Item.cursorHook` on
the old (arg 0) and new (arg 1) rows, and plays sound 5.

`0x221CD0` ("accept") is a no-op while `page->mode != 0`; otherwise it
runs `vt->cancel()` and then `0x2229B8()`:

```c
pageResetScroll();
if (vt && vt->activate && vt->activate()) return;
it = &page->items[page->cursor];
if (it->activate) it->activate(); else modelFreeze();   /* 0x22B100 */
page->mode = 1;                      /* enter edit mode */
page->slide = 0;
page->buttons &= ~0x0090;            /* SQUARE and TRIANGLE go away */
```

**Edit mode `0x222558`** is the same shape with `mask = page->buttons &
~0x5000` (UP/DOWN removed), the per-item filter at `Item+0x34` instead
of `vt->input`, and

```
0x8000 LEFT  : 0x2227E0()  prev option
0x2000 RIGHT : 0x222848()  next option
0x0020       : 0x222A60()  vt->left()  / Item+0x20, else prev
0x0040       : 0x222B40()  vt->right() / Item+0x24, else next
```

`0x2227E0` / `0x222848` decrement/increment `Item.value` modulo
`Item.nvalues`, set `page->slide = ∓150` (the carousel slide), fire
`Item.changed(it, previousIndex)` through `0x2228B0`, and play sound 6.

### 7.4 The UI model

`0x22B0E8(i)` is simply `&((int*)0x352880)[i]`, so `Item.field` is a
word index.  `0x22B138` refills the model from NVM but only when
`gp−30352` == 1; `0x22B100/08/18/28` set that flag to 0 / 0xFFFFFFFF /
1 / 1-and-reload.  `0x21CE58` calls `0x22B128` once at module entry, and
`0x2229B8` calls `0x22B100` when a row is activated — i.e. **activating a
row freezes the model so the user's in-progress edit is not overwritten
by the next NVM refresh**; `0x22B3F8` commits it back.

Corrected field map (from 0x22B138's store order):

| word index | byte offset | field |
|---|---|---|
| 0 | +0 | **screenType** (0x203690) |
| 1 | +4 | **spdifMode** (0x203658) |
| 2 | +8 | videoOutput (0x2036F8) |
| 3 | +12 | language (`GetLanguage` 0x2040D0) |
| 4 | +16 | bit 0 of 0x1F1234 |
| 6–9 | +24…+36 | clock fields |
| 13 | +52 | time notation |
| 14 | +56 | date notation |
| 15 | +60 | timezone index |
| 16 | +64 | daylight saving |
| 17+ | +68… | ps1drv config, 15 × 2 words |

---

## 8. The concrete pages and their tables  [VERIFIED]

All of these are *static initialised data* in the image, which is why
their string ids looked "computed" from the code side.

### System Configuration — header 0x27BE28, items 0x27BD10 (5 × 56)

| # | label | id | model field | options |
|---|---|---|---|---|
| 0 | "Clock Adjustment" | 106 | — | none; `activate` = 0x21DF28, six sub-handlers at +0x18…+0x28 (the six date/time digits) |
| 1 | "Screen Size" | 107 | 0 screenType | 0x27BC20, 3: 0→108 "4:3", 1→109 "Full", 2→110 "16:9" |
| 2 | "DIGITAL OUT (OPTICAL)" | 111 | 1 spdifMode | 0x27BCB0, 2: 0→112 "On", 1→113 "Off" |
| 3 | "Component Video Out" | 114 | 2 videoOutput | 0x27BBC0, 2: 1→116 "Y Cb/Pb Cr/Pr", 0→115 "RGB" |
| 4 | "Language" | 117 | 3 language | `values` NULL, count 0 — the language list is built at runtime (the version check 0x204318 limits it to 2 or 8 entries), so the default initialiser 0x222920 bails out and the item's own hooks (0x21EE50/0x21EE78/0x21F168/0x21ECD8/0x21F158/0x21F160) drive it |

Note item 2's value 0 is *"On"*: the config-word bit is "S/PDIF
disabled".  Item 3's options are stored in the order 1, 0.

This screen has its own handlers (`0x227560`, `0x227D08`, `0x227390`)
rather than the generic 0x2223D8, but uses the same 56-byte records.

### Main menu — header 0x27BE90, items 0x27BE80 (2 × 8)   →  §6.1

### Version Information — header 0x27BF38 (title 89), items built at 0x34E5B8

### Clock "Options" — header 0x27C200 (title 139), items 0x27C0F0 (4 × 56)

`buttons = 0xF060` (UP|DOWN|LEFT|RIGHT|0x20|0x40), `lblCross = 85`
("Back"), `lblCircle = 86` ("Enter"), vtable 0x27C1D0 (all NULL).

| # | label | id | model field | options |
|---|---|---|---|---|
| 0 | "Time Format" | 140 | 13 | 0x27BFA0, 2: 1→141 "12 hour clock", 0→142 "24 hour clock" |
| 1 | "Date Format" | 143 | 14 | 0x27C000, 3: 1→144 MM/DD/YYYY, 2→145 DD/MM/YYYY, 0→146 YYYY/MM/DD |
| 2 | "Time Zone" | 151 | 15 | 0x27C270 + the hook set 0x223938/0x223A88/0x223C10/0x223878/**0x2237A0**(init)/0x223940/0x223A00 — the 138-entry country list needs custom paging |
| 3 | "Daylight Savings Time" | 147 | 16 | 0x27C090, 2: 0→149 "Standard (Winter Time)", 1→150 "Daylight Savings (Summer Time)" |

Opened by `0x2236B8` (the hub's tail) when the clock screen object
0x27C258 is fading in and its timer reaches `gp−30400`; closed by the
same function when the object returns to state 0.

### User Preferences (first-boot wizard) — header 0x27DED8 (title 152), items 0x27DE00 (3 × 56)

`mode` is forced to 1 by `0x224288` at 0x2244D8 *before* the open call,
so the wizard starts directly in edit mode and walks the rows through
`mode == 3`.  `buttons = 0xA060` — LEFT|RIGHT|0x20|0x40 only, no
UP/DOWN, because the user is stepped through the items rather than
choosing among them.  vtable 0x27DEA8.

| # | label | id | model field | notes |
|---|---|---|---|---|
| 0 | "Language" | 117 | 3 | hooks 0x224E48/0x224E78/0x224FF0, init 0x224E28, cursor 0x224D78 |
| 1 | "Time Zone" | 151 | 15 | same hook set as the Options page |
| 2 | "Daylight Savings Time" | 147 | 16 | options 0x27DDA0 |

matching the wizard prompts drawn by `0x224630` (strings 153–160).

### Dead code

Sixteen Module U functions have **no caller and no reference anywhere in
the image** (checked by call-graph, by 32-bit word scan for the address,
and by lui/addiu materialisation scan):

```
0x21DDC0 0x21E3B0 0x21E870 0x21EDB8 0x21EF00 0x21F3A8 0x21FF88 0x220118
0x227028 0x227338 0x22B108 0x22B2A8 0x22B950 0x22B960 0x22EF30 0x22EF90
```

`0x21EF00` is on the map §3.2 list of config screens — it is a leftover.
`0x21E870` and `0x220118` are complete list-input handlers that were
replaced by the generic engine.

---

## 9. Open questions

* `0x1F0CF8` — read by the opening, Module U and Module V to decide
  whether an audio CD goes to the CD player; **written nowhere**.
* The trailing 40 bytes of every `Option` record are zero in all five
  static tables; presumably renderer geometry.  Same for `Item+0x18`,
  `+0x1c`, `+0x24`, and `Page+0x2c`/`+0x30` (−1 on the two wizard-style
  pages, 0 elsewhere).
* Disc types 0x20/0x21/0x22 → screen ids 111/112/113 are not in any
  libcdvd header I can check; the opening and Module U disagree about
  what to do with them, which suggests they were never exercised.
* The two 64-bit patches at `0x1F0A60` / `0x1F0B50` in the module-exit
  epilogue (`x = (x & ~0x7FFF) | 14`) sit inside the message ring
  (`dbuff`, 0x1F0A10); the ring's consumer is still the map's §1.7 gap,
  so their meaning is unresolved.
* `Page.vt` is all-NULL in every static page; the Version-Information
  popups at 0x34DBE0 are built in bss and may install one.
* HDDOSD's `clock_config_change_cb_*` names do not map onto anything in
  retail 1.20 — the retail data model has no per-field callback table
  (§7.2).  Either HDDOSD 1.10 restructured this, or the names are
  guesses.

---

## 10. Binary matching

`matching/src/menu.c` + `matching/menu-functions.txt`.

    ee-gcc -O2 -c src/menu.c -o build/menu.o
    EETOOLS=/usr/local/sce/ee/gcc/bin python3 check.py build/menu.o \
        /u/aap/src/osdsys/expanded.bin menu-functions.txt

**33 / 36 attempted functions match byte-exact**: the whole `Anim` API
(0x22AC18–0x22ACC0), the fade accessors (0x22AD28/0x22AD30), the model
mode setters (0x22B0E8–0x22B128), the legend setters
(0x21D748/50/58), the popup helpers (0x21F980, 0x21FA18), the page
accessors (0x221908, 0x2219D0, 0x221B68), the page cursor and item hooks
(0x221BD8, 0x221C38, 0x222798, 0x2228B0, 0x2228F0), the screen-object
getters (0x223790, 0x224D68, 0x226948, 0x226A48) and 0x2283D0.

Residuals, documented in the source and not ground on:

* `pageEnter` 0x221C98 and `pageSquare` 0x221D40 — 12/14, the ROM loads
  `curPage` into `$a0` and dereferences into `$v0`, we use `$v0` twice.
  Known allocator tie class.
* `pageClose` 0x221B70 — 22/25, purely a branch-likely/delay-slot
  scheduling difference (`beqzl` + hoisted `addiu` vs `beqz` + `nop`).

New codegen findings for the ee-gcc 2.9 law list are recorded at the top
of `menu.c`: the store-order permutation inside an `if` body, the
`goto`-to-labels-after-`return` trick that reproduces the ROM's
out-of-line branch bodies (0x22ACC0), and the fact that
`lui 0xffff / ori 0xffff` for −1 needs the *global's* type to be
unsigned (0x22B108).
