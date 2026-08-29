# Module U's 3D background scene (the "clock" module's towers/orbs/fog analogue)

Scope: only the always-present 3D background rendered behind the main
menu / system-configuration screens (Module U, `0x21C910`-`0x230000`).
Not the menu item/widget layer, not the input/state-machine logic, not
the text engine — those are other agents' territory and are only
mentioned here where a call boundary touches them.

Method: `objdump -D -b binary -m mips:5900 -EL --adjust-vma=0x200000` of
`/u/aap/src/osdsys/expanded.bin` (identical to the disassembly the
sibling agents are using — diffed byte-identical), sliced and
call-graph-walked with small Python scripts (scratch, `/tmp`, not
committed). Cross-checked against `osdbits/opening.c`,
`docs/osdsys-map.md`, `docs/osdsys_re-crossref.md` and the live resource
table read directly out of `expanded.bin`.

Confidence tags follow `osdsys-map.md`'s convention: **[ok]** =
disassembly-backed and cross-checked, **[tnt]** = plausible/partial,
**[?]** = guess.

---

## 0. The verdict (Q1)

**Module U carries its own, independently-implemented copy of the
scene-rendering pipeline. It does not call into the opening module's
code at all.** [ok]

I extracted every `jal`/`j` target inside Module U's code range
(`0x21C910`-`0x230000`, 286 functions) and bucketed each by region:

| target region | distinct targets called |
|---|---|
| opening module (`0x211C70`-`0x21C910`) | **0** |
| Module U itself | 296 |
| "shared renderer" page (`0x230000`-`0x231000`) | 9 (see §4) |
| core (`0x200000`-`0x211C70`) | 60 |
| libraries (`0x24C068`-`0x2678F0`) | 57 |
| Module V | 0 |

Zero edges into the opening. I also checked the reverse direction (does
the *opening* call into the `0x230000` page) — also zero. So the
"shared renderer core" `osdsys-map.md` describes is shared **only**
between Module U and Module V, and even that sharing is partial (§4).

What Module U *does* share with the opening is the stuff any two
translation units linked from the same SDK would share: the `sceVu0`
matrix library (`0x267000`-`0x2678B0` — Module U calls essentially the
same ~15 entry points the opening uses: `UnitMatrix`, `ApplyMatrix`,
`MulMatrix`, `RotMatrix`, `TransMatrix`, `Normalize`, `CameraMatrix`,
`ViewScreenMatrix`, …), newlib/libm, and — concretely — **the exact same
32-bit LCG `rand()` at `0x25B478`** that the opening's `osdRand()` wraps
(`osdbits/opening.c`'s comment on `osdRandSeed`). Module U's
`0x225998` calls it directly. Same RNG stream, same address, called
from unrelated code — a real but narrow point of sharing.

Bottom line for the port: there is no shortcut. `osdbits` cannot keep
calling `DrawTowers`/`DrawLights`/`DrawCube` past the end of the opening
scene; Module U's background is a **separate implementation** that
happens to solve a similar visual problem with different textures and
(as far as traced) a different technique for at least one effect (§5).

---

## 1. The opening -> Module U handover (Q2)

### 1.1 What actually runs

`osdbits/opening.c`'s `DoOpeningIllegal()`/`ProcessOpening()`/`DrawEnd()`
are not just faithful — they are **byte-for-byte structural matches** of
the real functions, now pinned to addresses:

| osdbits function | real address | real bounds | evidence |
|---|---|---|---|
| `ProcessOpening` | **0x211E38** | 0x211E38-0x211EB4 | calls `0x215FD0` (`ProcessOpeningAnimation`), bumps a scene counter on type change, calls `0x212D40` (`vif1SetSCISSOR_1`) — line for line |
| `DrawEnd` | **0x211EB8** | 0x211EB8-0x211EF8 | calls `0x214F58` (`DoText`), conditionally `0x214790` (`DrawBlackBars`), `0x205F30` (`WaitNextFrame`), increments frame counter |
| `DoOpeningIllegal` | **0x211F00** | 0x211F00-0x211F88 | `while(openingType != 2) { ProcessOpening(); dispatch 0/1 -> 0x219250/0x219F40; DrawEnd(); openingType = nextOpeningType; }` — exact |

None of this was previously pinned to real addresses; it now is.

### 1.2 The real outer loop (not yet in osdbits — core-thread territory)

`osdbits/opening.c`'s `OpeningThread()`/`Init()`/`MakeOpeningThread()`
are a simplified stand-in for a real function at **0x211D30** that loops
forever:

```
0x211D30 loop:
    0x24D980()              ; sleep/wait primitive
    0x211F90()               ; sets fooOpeningType from systemState==4,
                              ; drawBlackBars from screenType==1
                              ; (osdbits/opening.c's Init() comment already
                              ;  named this address correctly)
    0x211FD8()               ; posts message 20500 keyed on systemState
                              ; (1 -> param 0, 4 -> param 6) via 0x200B80
                              ; (osdsys-map.md's message-ring table already
                              ;  named 0x211FD8 as a poster)
    0x211DB0()                ; ===== real Init(): InitAnimation (0x215F18),
                              ; InitTowersFog (0x218E00), sub_219F08,
                              ; initTextShit (0x214F20), StartFrame,
                              ; frameCount=evenOddFrame — matches osdbits'
                              ; Init() call order exactly, confirms every
                              ; one of those addresses =====
    0x20B780(0 or 1)           ; event-queue push (§1.9 of osdsys-map.md),
                              ; arg picks the boot-vs-illegal branch
    0x211F00()                ; ===== DoOpeningIllegal() =====
    0x212010()                ; ===== "opening_transition_to_clock" =====
    0x20B780(1)
    SignalSema(*(0x26FE20+20)) ; = 0x26FE34, same semaphore ThreadU signals
    goto loop
```

So the opening thread does not exit once — it is a normal "screen
module" thread (sleep / run / signal / sleep) like ThreadU and ThreadV,
and it re-enters `DoOpeningIllegal()` every time it's woken (e.g. if the
opening is ever shown again). `osdbits` only models one pass through
this, which is fine for the boot animation but means **the real
outer-loop plumbing (0x211D30, 0x211F90, 0x211FD8, the event-queue
pushes) is not yet ported** — flagged for whoever picks up core-thread
work, not in my scope to port.

### 1.3 The actual handover: `0x212010`, not `0x211FB0`

`docs/osdsys_re-crossref.md`'s candidate `opening_transition_to_clock ~
0x211FB0` lands mid-instruction (inside `0x211F90`'s body) — an artifact
of the HDDOSD offset drifting, as the crossref doc itself warns. The
real function playing that role is **`0x212010`-`0x2121B8`** (called
right after `DoOpeningIllegal()` returns, i.e. once `openingType==2`):

* It writes `0x1F0014` (screen-id result param, the same low-block cell
  the browser writes on exit — `osdsys-map.md` §1.6/§2.3).
* It calls two module-registry version-check helpers (`0x204378`,
  `0x2043A8`/`0x2043B8`, adjacent to `osdsys-map.md`'s `0x204318` version
  check) to decide between a couple of fixed outcomes, or —
* falls into a computed dispatch keyed on **`lastBootParam`**
  (`gp-30928` = **0x2A77A0**, exactly the address `osdbits/opening.c`
  already documents as `lastBootParam`): `(lastBootParam - 106)`,
  bounds-checked `<11`, indexes the jump table at **0x2A4120** — the
  *opening's own* 11-entry screen-id table (`osdsys-map.md` names this
  table as the opening's `0x211C70`-side dispatch). Each arm sets
  `0x1F05E8` (`systemState`) to a small fixed code (2, 3, 4, 5, or a
  computed one for the "disc present" case at `0x1F0CF8`) and/or
  `0x1F0014`.
* It finishes with a **second** call into the version-check pair and a
  short tail that ends up back at the module-registry's return.

**No scene state crosses this boundary.** The only things written are
the low-block screen-id/systemState cells (`osdsys-map.md` §1.6) — the
same generic mechanism every module uses to hand off. I did not find a
single write from this function (or from `0x211D30`'s loop) to any
address inside Module U's own scratch-data area (§3), and conversely
none of Module U's traced functions read any of the opening's globals
(`position`, `rotation`, `lightsSeed`, `fogAnimation`, the `cube*`
tables — all in the `0x2A70xx`-`0x2A7Cxx` gp-relative range documented
in `osdbits/opening.c`). Module U initialises a **completely separate**
scratch area (§3) from scratch every time it's entered.

Practically: **camera position/velocity, orb/light state, fog
animation phase, and the RNG seed do NOT persist from the opening into
the menu.** What *does* persist, indirectly, is the *choice of which
config screen to open first* — carried through `lastBootParam`, not
through any 3D state.

### 1.4 Correction to `osdsys-map.md` §3.2

Module U's screen dispatcher (`0x21CA38`, see §2) does not have *one*
11-entry jump table — it has **three**, all indexed the same way
(`id-106`, bounds `<11`, `sll 2` + base):

| table | address | reached from |
|---|---|---|
| first dispatch (documented) | `0x2A46C0` | the "screen just changed" branch, id 116 special-cased |
| second dispatch (new) | `0x2A46F0` | the steady-state branch (after `0x226950`/`0x22AD30` gate) |
| third dispatch (new) | `0x2A4720` | a sub-branch of the second, when the "already handled" check fails |

All ultimately funnel into writing `0x1F05E8`/`0x1F05EC` with a small
set of literal codes (1-5) and re-arming the "screen changed" flag —
this is UI/state-machine plumbing, not scene code, and I did not chase
its 33 combined table entries further (sibling agent's territory).

---

## 2. ThreadU's per-frame shape (Q3, partial)

```
0x21CDD8  ThreadU loop:
    SleepThread (0x24D980)
    0x200B80(20501,1,0,0)          ; scene-enter message
    0x230090()                     ; one-time renderer reset (§4)
    0x21CE40()                     ; *(float*)0x27B440 = 1.0  (already in osdsys-map.md)
    loop:
        0x21C9D0()                 ; sceGsResetPath, 0x266E80(1), IsPAL
        0x21CA38()                 ; ===== module body (own frame loop), see below =====
        SignalSema(*(0x26FE34))
        SleepThread

0x21CA38 (module body) — has its OWN internal frame loop:
    compute "requested" screen id (0x1F0010, or 100 if the exiting
        module was the opening — 0x1F05EC==1)
    0x21CE58()                     ; ===== one-shot module init, §2.1 =====
    StartFrame (0x205E88)
    inner loop (head 0x21CA98; single backedge b 0x21CA98 @0x21CC70):
        [screen-id dispatch: the three tables from §1.4, sets
         0x1F05E8/0x1F05EC/0x1F0014 when the id leaves 106-116]
        0x21CF20()                 ; ===== draw/update hub, §2.2 =====
        WaitNextFrame (0x205F30)
        loop again UNLESS requested id left [106,116]
```

So `0x21CE58` runs **once per menu-module entry** (the loop backedge at
0x21CC70 targets 0x21CA98, which is AFTER the 0x21CE58 call at 0x21CA7C —
an earlier draft of this doc had it inside the loop), and **`0x21CF20`
runs every single displayed frame** regardless of which config screen is
showing — so 0x21CF20 is where the always-present 3D background lives.

### 2.1 `0x21CE58` — one-shot module init

```
0x22EE88   sendDma-clone, VU1 defensive re-arm (§2.1.1)
0x22BE18   calls sceMc-library functions (0x253xxx) + newlib (0x257xxx)
           — NOT scene code; likely memory-card polling housekeeping
0x22B838   tiny (26 insns); calls 0x206818/0x206880 (vblank/timer-adjacent
           CORE functions) — NOT scene code
0x22B128   reads 8 config-word getters (screenType/language/timezone/
           DST/date-notation/time-notation) + the 4 clock-field
           functions (0x22B720/790/7A0/7B0) into the UI model struct —
           this IS osdsys-map.md's §3.6 "read config into UI model"
           (documented there as 0x22B138 — off by the usual drift).
           NOT scene code (config-screen sibling's territory, or none).
0x22AD38   99 insns, almost no calls — looks like more bit-twiddling,
           not classified
0x229698   "TEXTURES_229698" (osdsys-map.md's own IDB name): fetches
           10 resource pointers, ids 45-54, into a table at 0x27F1C0
0x22A9B8 x10 (a0=0..9)  per-slot texture (re)upload: calls "getTexC"
           (0x2297A0) / "setTexCOffset_8b" (0x229750) + FlushCache +
           sceGs image-load helpers — MINE, see §2.1.2
0x21DBA0   43 insns; touches FONT_20A3C8-adjacent functions (0x20A3B8/
           0x20A3C0) and getResourcePtr — looks like TEXT/font setup,
           likely sibling (2D widget) territory, not scene
0x225998   186 insns; calls the opening's own rand() (0x25B478,
           confirmed §0) plus GS-register helpers (0x22AC48/70/90/C0
           family) — randomised INIT (runs once per menu entry). MINE,
           unclassified (candidate: initial orb/twinkle placement)
0x228460   73 insns; calls 0x267068 (sceVu0ViewScreenMatrix!) and five
           of Module U's own per-screen-init functions (0x2283D0,
           0x2283A0, 0x228110, 0x228050, 0x227DE8, 0x227198) — mixed
           scene/UI, not classified further
0x2287B0   42 insns; calls 0x2293E0/0x2294B8 (the vif1Begin-style
           packet-open family, §2.1.2) — MINE, unclassified
0x22ADD8(2) 7 insns, trivial wrapper
```

#### 2.1.1 The VU1 re-arm is a private copy of the opening's exact idiom

`0x22EE00` (called via the `0x22EE88` wrapper, which hardcodes the
target address) reproduces, instruction-for-instruction, the *one-time
VU1 microcode upload* sequence `osdbits/opening.c`'s `sendDma` comment
documents for `OpeningInitTowersFog` (`0x218FF0`-`0x219044`): mask the
address to `&0x0FFFFFFF`, `QWC=0` at `0x10009020`, `TADR=addr` at
`0x10009030`, `*(0x1000E010)=2`, `FlushCache(0)`, `CHCR=325` at
`0x10009000`. Same magic constants, same order, same instructions —
but it's a **separate copy of the code** at a different address, and it
targets **`0x268860`**, not the opening's **`0x2678E0`** (the start of
the VU microcode blob, `vucode_1`, per `osdsys-map.md` §1.1). `0x268860`
is `0xF80` bytes further into the same shared VU-microcode data region
(`0x2678F0`-`0x26B060`), so Module U arms a **different** compiled VU1
program — not the opening's tower program — using the exact
same low-level kick recipe. (Whether that program is towers-shaped,
orb-shaped, or something else is not determined; extracting and
disassembling it as VU1 code is future work, same technique as
`towerchain.dsm`.)

Like the opening's, this upload happens **once per module entry** (it
sits in the one-shot init `0x21CE58`, before the frame loop — see §2's
correction), which is also when protection against another module
having clobbered VU1 memory is actually needed: ThreadU, ThreadV and
the opening are all prio-6 threads sharing the same VU1/VIF1 hardware.

#### 2.1.2 The texture reload is the atmosphere/background texture set

Reading the live resource table (`0x26ED00`, entry stride 16 bytes)
directly out of `expanded.bin`, resource ids 45-54 — exactly the ten
ids `0x229698` loads — are:

```
45 TEXCFLOW   46 TEXCKABE   47 TEXCBUMP   48 TEXCBINV   49 TEXCSMOK
50 TEXCREFA   51 TEXCNAVI   52 TEXCBLUR   53 TEXCSTSL   54 TEXCMARU
```

("KABE" is Japanese for "wall"; "MARU" is Japanese for "circle/round".)
This set is uploaded to GS local memory **once per menu entry** via
`0x22A9B8` (from the one-shot init `0x21CE58`). Compare to the opening's texture set (`TEXOWAL0` = the
tower wall texture, `TEXOBLP`/`TEXOBLPR`/`TEXOREF` = the cube
refraction layers, `TEXOFOG0-4` = the fog quads): the naming maps
almost one-to-one —

| opening texture | role | Module U texture | likely role |
|---|---|---|---|
| `TEXOWAL0` | tower wall | `TEXCKABE` ("wall") | background wall/plane |
| `TEXOBLP`/`TEXOBLPR`/`TEXOREF` | cube refraction (screen-capture based) | `TEXCBUMP`/`TEXCBINV`/`TEXCREFA` | refraction, but via a **bump map pair** instead of screen capture |
| `TEXOFOG0-4` | fog quads | `TEXCSMOK`/`TEXCFLOW` | atmosphere, but as scrolling textures, not a vertex-coloured quad grid |
| (no equivalent) | — | `TEXCMARU` ("circle") | plausibly the orb/light sprite |
| (no equivalent, UI) | — | `TEXCNAVI`, `TEXCSTSL` | **flag for the 2D-widget sibling** — "navi(gation)" and a status/slider-shaped name look like UI, not scene, even though they share this texture-reload table |
| (no equivalent) | — | `TEXCBLUR` | a screen/element blur mask |

This is the strongest evidence for Q5 (§5): the menu background is
conceptually the same idea (wall/tower plane + refractive floating
elements + atmosphere), reimplemented with different techniques for at
least the refraction (bump-mapping instead of the opening's
screen-capture-and-recomposite cube trick) and the atmosphere (scrolling
textures instead of a 17x17 vertex-coloured fog grid).

Every-frame re-upload of all ten textures (rather than uploading once
at screen-init) is consistent with these being **procedurally animated
on the CPU side** each frame (a moving bump/flow pattern) rather than
static image data — this is speculative [tnt], not confirmed by reading
the upload's source buffer.

### 2.2 `0x21CF20` — the draw/update hub

Calls, in order: `0x21CFD8`, `0x21D0A0`, `0x2268F0`, `0x22B020`,
`0x2283F0`, `0x21D368`, `0x21D3A0`, `0x21DA68`, `0x21DB18`, plus (per
`osdsys-map.md`) `0x225BF8`, `0x2285C0`, `0x2287D0`, `0x22B058`,
`0x22B588`, `0x22BB30`, `0x22BE30`.

I traced two of these concretely:

* **`0x21D0A0`** calls `sceVu0ViewScreenMatrix` (`0x267068`, the same
  entry point `ProcessOpeningAnimation` calls) — screen-projection setup
  runs here, every frame, same as the opening does every frame
  regardless of whether the projection ever changes.
* **`0x2283F0`** is **not** scene code — it's the "screen-init hub"
  `osdsys-map.md` §3.2 already documents (eleven per-screen
  initialisers, tail-jumping to `0x2236B8`, which is a small UI
  navigation/wizard-flow state machine, confirmed by reading it: it
  branches on a state word and calls `0x221908`/`0x22AC18`/`0x22AC48`
  before jumping into further UI-flow code at `0x2219E0`/`0x221B68`/
  `0x223680`). This confirms `0x21CF20` mixes real scene work with pure
  UI-flow dispatch in the same per-frame call list — there is no clean
  "3D scene subsystem" vs "menu subsystem" split at this level; they're
  interleaved function calls in one thread's frame body.

The other eight callees of `0x21CF20` were not individually classified
— **labelled gap**, priority for follow-up (see §7).

---

## 3. Module U's own scratch-data area

Every scratch address I encountered while tracing (§2) lands in one of
these ranges, none of which overlap any address `osdbits/opening.c`
documents for the opening's own state:

* `0x27B440` — a single float flag, set to 1.0 at ThreadU init
  (`osdsys-map.md`, confirmed).
* `0x27E950` — a small struct `0x2268F0` writes two words into
  (candidate: a per-call "threshold/state" pair, unclassified).
* `0x27F1C0` — the 10-entry texture-pointer table `0x229698` builds.
* `0x27F2D0` — a fixed qword template `0x2299C0` copies into new
  packets (candidate: a GIF-tag template, analogous to the opening's
  `giftag` constant in `vif1Begin`).
* `0x352880` — the UI settings model (`osdsys-map.md` §3.6 — confirmed
  config/UI territory, not scene).

None of this is the opening's `0x2A70xx`-`0x2A7Cxx` block. This is
corroborating (not exhaustive) evidence for §1.3's "no scene state
persists" conclusion — I did not check all 296 functions' scratch
addresses, only the ones on the always-on per-frame path.

---

## 4. The "shared renderer core" page is two unrelated clusters

`osdsys-map.md` describes `0x230000`-`0x231000` as one shared block. It
is actually two, sharing only a 4 KB page by coincidence of layout:

| sub-range | called by | role |
|---|---|---|
| `0x230000`-`0x230440` | **Module U only** (9 functions: `0x230018/068/090/180/198/260/328/3E8/440`) | a fixed-point angle-wrap + table-lookup helper (`0x230018`, cousin of a sin/cos LUT), `InitRenderer` (`0x230090` — resets a 16-slot packet-ring counter and calls `sceVu0UnitMatrix`), and the ring-slot acquire/release pair with a busy-loop panic at slot 16 (`0x2300B8`/`0x230138`) |
| `0x230478`-`0x230FC8` | **Module U: never** (checked — zero call sites); **Module V only** | `setScreenMatrix` (0x230478), `setLightMatrices` (0x230580, 4 sites), `setTextureUpload` (0x230708), `SetWorldMatrix` (0x230898), plus three more (`0x2307F8`, `0x230958`, `0x230A08`, `0x230CC0`, `0x230FC8`) — confirmed shared with Module V by direct call-site check |

So Module U's matrix/packet plumbing for its OWN scene is **entirely
private** (its own `0x22A0C0`-style AD-register emitter, its own
`0x2299C0`-style packet-begin, direct `sceVu0` calls) — it does not even
use the genuinely-shared matrix setters that Module V uses. The map's
"`sendDma` (0x22EE00)" entry under this section header is also outside
the `0x230000` range; that's a pre-existing documentation slip, not
something I'm fixing here (out of scope — `osdsys-map.md` is not one of
my writable files).

---

## 5. What's genuinely new vs. the opening (Q5)

Given §0's zero-code-reuse verdict, **all of Module U's scene code is a
separate implementation** by definition. The more useful question is
where it *diverges in technique*, which §2.1.2's texture-name mapping
answers concretely:

* **Refraction**: opening captures the live screen into a working
  buffer and re-samples it per cube face (`CubeCaptureBuffer` in
  `osdbits/opening.c`); Module U's texture set (`TEXCBUMP`/`TEXCBINV`/
  `TEXCREFA`) names a bump-map pair instead — a cheaper, static-normal-map
  style fake refraction rather than a screen-capture one. [tnt] —
  inferred from texture naming, not from reading the actual draw
  function (not located with confidence, see §7).
* **Atmosphere**: opening draws a 17x17 grid of vertex-coloured,
  UV-scrolling quads (`DrawFog`); Module U has `TEXCSMOK`/`TEXCFLOW`,
  names that suggest pre-authored scrolling texture(s) rather than a
  procedural vertex grid. [tnt]
* **RNG**: identical mechanism (same `rand()` address, §0), reused for
  presumably the same kind of purpose (randomising a decorative
  element's phase/position) — the one point of literal, verified reuse
  beyond the standard library.
* **VU1 upload idiom**: byte-identical *recipe*, different *program*
  (§2.1.1) — reused technique, not reused content.

I could not conclusively identify a single "this is the tower-plane
draw call" or "this is the orb draw call" function — see §7's gaps.
Two large, packet-emission-heavy candidates were found
(`0x22C0D0`, ~2.3 KB, and `0x22E5A8`, ~2.1 KB, the latter confirmed to
call `sceVu0CameraMatrix`) but **both are reached only through indirect
calls** (no `jal`/`j` call site found anywhere in the image) — almost
certainly invoked through one of the per-screen/per-item callback
tables the config-menu logic uses, which puts identifying their trigger
condition outside what I could resolve without also reversing that
table (sibling territory).

## 6. The menu camera (Q4, incomplete)

Confirmed: Module U builds a camera the same way the opening does —
`0x22E5A8` calls `sceVu0CameraMatrix` (`0x267298`) and `0x21D0A0` calls
`sceVu0ViewScreenMatrix` (`0x267068`), both the exact library entry
points `ProcessOpeningAnimation` uses. **Not** called anywhere in
Module U: `sceVu0NormalLightMatrix` (`0x2671D8`) — the opening's
per-frame 3-light setup is absent, meaning Module U's lighting model
(if it has per-vertex lighting at all) is built differently, most
likely the fixed/precomputed `sceVu0LightColorMatrix`-style setup the
opening's own cubes use (that function's address wasn't independently
re-derived here).

I could not determine, within budget, whether the camera **drifts**
(continuous integration like `ProcessOpeningAnimation`'s
position/speed/accel chain) or is **fixed** (a constant eye point,
possibly with only a spin/sway term like `DrawTowers`' single
per-frame `towerSway` angle). The `sceVu0CameraMatrix` call in
`0x22E5A8` takes a position argument that is itself computed earlier in
the *same* function (not read from a small number of persistent
speed/accel globals the way the opening's does), which argues mildly
against a from-scratch Euler-integrated flight path and mildly for
either a fixed eye point or a purely rotational (sin/cos-driven)
animation — Module U does call the same trig library entry points
(`0x257658`/`0x257918` — likely `sinf`/`cosf`, by address proximity to
the opening's own trig calls in this region) but I did not pin down
which function uses them for what. **Labelled gap.**

I found no evidence either way for "does entering a config sub-screen
dim/blur/hide the background" — `TEXCBLUR` (§2.1.2) exists and is
reloaded every frame regardless of which sub-screen is active, which is
at least consistent with an always-applied (not screen-conditional)
blur layer, but this is weak evidence.

---

## 7. Port impact — what osdbits needs to render past the opening

Concretely, continuing the port past `sceneState`'s end requires **new
code, not reuse**:

1. **A second VU1 program upload/kick path.** The exact recipe is
   known (§2.1.1, identical to the opening's `sendDma`/`towerKick`), but
   the *program itself* (bytes at `0x268860` onward in the shared
   `0x2678F0`-`0x26B060` vucode blob) has not been extracted or
   disassembled — same technique as `towerchain.dsm`, unattempted here.
2. **The 10-texture atmosphere pipeline** (§2.1.2): resource ids 45-54,
   `TEXCFLOW/KABE/BUMP/BINV/SMOK/REFA/NAVI/BLUR/STSL/MARU`, reuploaded
   every frame via a new `getTexC`/`setTexCOffset_8b`-based loader —
   structurally similar to the opening's `InitTexture`/`UploadImage`
   but keyed to a different resource range and re-run every frame
   instead of once.
3. **A private packet-builder layer** (Module U's own
   `pktSetAD`/`vif1Begin`-equivalents, `0x22A0C0`/`0x2299C0` and the
   `0x22AC18`-`0x22AD30` helper family) — cannot reuse osdbits'
   existing `vif1*`/`pkt*` functions as-is; they're a different, if
   structurally similar, implementation, so this is new C to write
   (following the same gcc-2.9-ee -O2 shape rules `matching/README.md`
   documents), not a call-through.
4. **The handover itself needs no new state machinery** beyond what the
   opening already has: it's the same `0x1F0010`/`0x1F05E8`/`0x1F0014`
   low-block screen-id convention (`osdsys-map.md` §1.6) the opening's
   own dispatch already uses. `lastBootParam` (`0x2A77A0`, already a
   named global in `osdbits/opening.c`) decides which of the 106-116
   screen ids to request first; no 3D state needs to be carried across.
5. **Not required**: nothing from `osdbits/opening.c`'s `sprMatrices`/
   `sprVertices`/`textures[]`/cube or light state needs to survive past
   the opening — Module U initialises all of its own equivalents fresh
   (§3).

---

## 8. Open questions / labelled gaps

* Which specific function(s) actually issue the tower/wall-plane and
  orb/circle draw calls — candidates `0x22C0D0` and `0x22E5A8` are
  packet-emission-heavy and one confirmed to build a camera matrix, but
  both are reached only by indirect call, so their trigger/owner
  (per-screen table? per-item callback?) is unresolved.
* The remaining eight callees of `0x21CF20` are not classified.
* Whether the camera drifts or is fixed (§6).
* Whether entering a config sub-screen changes the background scene at
  all (no evidence found either way).
* The exact role of `0x22AD38`, `0x225998` (beyond "calls rand()"),
  `0x2287B0`, `0x228460` within the always-on preamble.
* `0x21DBA0`'s FONT-adjacent calls suggest it's actually 2D-widget/text
  territory swept into the same per-frame chain — flagged, not chased.
* `TEXCNAVI`/`TEXCSTSL` likewise flagged as probable 2D-widget/sibling
  ownership despite living in the same per-frame texture-reload table
  as the atmosphere textures.
* The VU1 program at `0x268860` (what it draws) is unextracted.

---

## 9. Matching

Not attempted. Everything found this session is call-graph/architecture
level (which function calls what, which globals, which resource ids) —
I do not yet have a confident line-level C reconstruction of any single
Module U function to compile and diff. Writing speculative C against
`0x22C0D0`/`0x22E5A8` without first resolving how they're invoked (§7)
would risk a source shape that can't be correctness-checked even if it
happened to byte-match by luck. Left for a follow-up pass once the
indirect-call sites are resolved.

---

## 10. The orb scene, decoded and ported (2026-08-30)

This section supersedes §§5-8's "labelled gaps" for the always-on part
of the background: the seven glowing orbs. Everything below is
disassembly-derived and is implemented in `osdbits/menu.c` (run it with
`main.elf menu`). Confidence **[ok]** unless marked.

### 10.1 The scene is a clock

The orbit is driven entirely by the **real-time clock**, not by a frame
counter. `0x2261B8` reads two of the clock accessors that live next to
the config-model getters (§2.1's `0x22B720/790/7A0/7B0` family), and
they are literal clock hands over the block at `0x352980`:

| address | field |
|---|---|
| `0x352980` | milliseconds (float) |
| `0x352984` | seconds |
| `0x352988` | minutes |
| `0x35298C` | hours |
| `0x352990/4/8` | day / month / year (the leap-year code at `0x22B7C0` confirms) |

* `0x22B5E8` (= `0x22B590`, a duplicate) → `sec + ms/1000`
* `0x22B6B0` (= `0x22B640`) → `min + sec/60`
* `0x22B720` → `hour + min/60`

`0x22BB30` (per-frame, from `0x21CF20`) integrates the milliseconds at
`*(gp-30324)` = 1000/59.94 per frame and resyncs against the low-block
RTC copy at `0x1F0CB8` whenever the two drift more than 3 s apart.

### 10.2 `0x2261B8` - the orbit, fully decoded

```
rateX = clockSeconds() * 65536/60      ; a full turn per minute
rateY = clockMinutes() * 65536/60      ; a full turn per hour
t      = *(float*)(gp-28848)           ; the entry ease, 0 at module entry
radius = (t*7.25 + 10.0) * *(float*)0x27B440
*(gp-28848) = t + ((1.0 - *(gp-28852)) - t) * *(gp-32152)      ; 0.005/frame
for i = 0..6:
    identity
    RotZ(*(short*)(gp-28854))          ; the hour hand, see 10.3
    RotY(*(short*)(gp-28856))          ; the second hand at entry
    RotZ(-32768)                       ; 180 degrees
    RotY(trunc(rateY * *(gp-32156)))   ; *(gp-32156) = 1100.0, a CONSTANT
    RotX(trunc((i+21) * rateX))
    Translatef(0, radius, 0)
    RotY(8192)                         ; 90 degrees
    copy top -> 0x27E970 ; 0x225ED0(0x27E950, i)
```

The three floats an earlier draft left unidentified are plain constants
in .data: `gp-32156` = **1100.0** (not a frame counter), `gp-32152` =
**0.005** (the ease rate), `gp-32148` = **0.05** (the carousel gate in
`0x2268F0`). So the radius eases 10 -> 17.25 over ~600 frames.

Consequences worth noting for the look: `rateY*1100` turns the ring
about once every 3.3 s, and each orb takes a *different* multiple
(21..27) of the second hand, so the seven shear along the ring instead
of moving rigidly - they bunch up and spread out again on a one-minute
cycle. The ease target is `1 - *(gp-28852)`, so the browser transition
can pull the cloud back in by driving that global up.

**Matrix order matters**: `mdRotX/Y/Z` (0x230198/0x230260/0x230328) do
`sceVu0MulMatrix(top, top, R)` - **post**-multiply - whereas the camera
builder `0x22ED20` uses `sceVu0RotMatrix{X,Y,Z}(m, m, a)`, which
**pre**-multiplies (`m = R x m`). Swapping the two silently mirrors the
scene.

### 10.3 `0x225998` - the one-shot init

```
*(int*)0x27EB00 = *(gp-30372)          ; the carousel timer's duration
0x225420()                             ; the clock-seeded angles, below
0x225978() -> 0x225628(), 0x225878()
[the 12-slot carousel ring at 0x34E6C0 is rebuilt here]
for i = 0..6:  0x34E960[i] = rand() % 65536      ; rand() = 0x25B478
                                                ; (the shared LCG, so the
                                                ;  phases depend on how many
                                                ;  times the OSD called rand()
                                                ;  before the menu started -
                                                ;  same caveat as the
                                                ;  opening's lightsSeed)
0x22FE88()                             ; zero ten 1616-byte orb structs
0x22EE98()                             ; the trail timer, below
```

`0x225420` seeds the cloud's orientation from the clock:

```
*(int*)0x34E6C0     = (int)clockHours() % 12                 ; carousel offset
*(short*)(gp-28854) = clockHours()   * 65536/12              ; the hour hand
*(short*)(gp-28856) = clockSeconds() * 65536/60              ; the second hand
*(short*)0x34E6C6   = (carouselOffset << 16) / 12
*(short*)0x34E6C4   = clockSeconds() * 65536/60
```

`0x22EE98` (tail-called from `0x225998`) sets the timer at `0x27F900` to
`(fps<<8)/60` frames (256 on NTSC) and opens it; `0x22EFF0` steps it
once **per orb**, so it actually fills in ~37 frames.

`0x21CE58`'s tail sets `*(gp-28880) = -100.0` - the camera's fly-in.

### 10.4 The camera (closes §6)

Fixed, not drifting. `0x21CFD8` per frame:

```
sceVu0ViewScreenMatrix(m1, scrz=512, ax=*(0x27B44C), ay=*(0x27B450),
                       cx=2048, cy=2048, zmin=1, zmax=16777215,
                       nearz=1, farz=65536)
0x22ED20(m0, pos, 0x27B470, 0x27B480, 0x27B490)
*(gp-28880) *= *(gp-32224)             ; 0.97 - the fly-in decay
```

with `ax/ay` from `0x21C9D0` (1.0 and **0.47** on NTSC / 0.5405 PAL -
note the opening uses 0.457627 for the same aspect) and

| address | value | role |
|---|---|---|
| `0x27B460` | `{10.436, 0, -103, 0}` | rest position (z += `*(gp-28880)`) |
| `0x27B470` | `{0, 0, 1, 1}` | forward |
| `0x27B480` | `{0, 1, 0, 1}` | up |
| `0x27B490` | `{0.031, 0.145, 0, 0}` | fixed Euler angles |

`0x22ED20` = unit; `RotMatrixX(0.031)`; `RotMatrixY(0.145)`;
`RotMatrixZ(0)`; rotate all three vectors by it; `sceVu0CameraMatrix`.
`sceVu0CameraMatrix`'s `zd` argument is a **direction** (0x267298
normalises it and crosses it with `yd`), not a look-at point - so the
camera sits ~203 units back at module entry, eases to 103, and looks
along a fixed axis that passes ~10 units to the right of the origin.

The scene struct at `0x27E950` keeps `m1` (viewscreen) at **+0x60** and
`m0` (camera) at **+0x64** - that ordering is easy to get backwards.
`0x225D18`'s sort key is `(camera x world)[3][2]`, i.e. camera-space Z.
`0x225D40` walks on while the new key is *smaller* than the node's, so
the list comes out **descending** - farthest first, which is the right
order for the depth test the flush uses.

### 10.5 `0x22FEC0` and the 1616-byte orb struct at `0x27F950`

Ten structs, seven used, zeroed by `0x22FE88`:

| offset | field |
|---|---|
| +0x00 | `head` - trail write index, 0..49 |
| +0x04 | `sub` - 0..2 subframe counter |
| +0x08 | `wrapped` - set once the ring has filled |
| +0x10 + i*0x20 | screen position (4 floats) for trail sample i |
| +0x20 + i*0x20 | colour (4 ints) for trail sample i |

`0x22FEC0(i, pos, col)` - note the **third argument is passed in a2 and
is invisible at the call site** (`0x226360` leaves `sp+16` there from
the colour block above it) - writes the current sample, then advances
`head` only every third call, copying the sample into the new slot too.
So a full trail spans 150 frames (2.5 s).

### 10.6 `0x22EFF0` - the orb renderer

Not "the menu navigation icons" (§10 of `menu-draw.md`): the two TEXC
slots it binds are the two halves of one glowing dot. Both decode to
white-with-alpha (decoder `0x22A790`, the same expansion `opening.c`
calls format 3); TEXCBLUR is a soft radial falloff and TEXCNAVI a solid
disc.

Per orb, everything additive, and the whole sequence run **twice** -
once into the visible buffer, once into the offscreen buffer the
zoom-blur composite samples:

1. **the trail** - `PRIM = 0x82` = LINE_STRIP with AA1, ABE and IIP off
   (the same "weird setting" `opening.c`'s `DrawLights` uses for its
   light trails - the AA1 edge blend is what draws it), then 49
   `(RGBAQ, XYZF2)` pairs in one REGLIST GIF packet. Sample `k` reads
   ring index `(head + 100 - k) % 50` with a fade `f = max(128-3k, 0)`
   and colour
   `B = b*f/128`, `G = g*f^2/2^14`, `R = (((r*f^2/128)*f/128)*f)/2^14`,
   `A = f/2` - so red dies fastest and the tail fades to blue. When the
   ring has not filled yet the age index is stretched: `f = 128 - 3*(k*50/(head-1))`.
2. **the halo** - `0x22AB90(7,1,1)` (TEXCBLUR, additive, ZTST ALWAYS)
   then one sprite of half-width `depth * 6.5e-06 * 30.0`, half-height
   half of that (one NTSC field line = two source rows), coloured from
   the trail head's stored colour.
3. **the core** - `0x22AB90(6,1,1)` (TEXCNAVI) and the same sprite at
   `4.5` instead of `30.0`, coloured `{128,128,128,128}` (`0x27F930`).

`depth` is `0x22CFA8`'s third output: the projected Z **after** the w
divide, i.e. `A + B/z` over `z`. At the menu's camera distance that is
~150000, which makes the halo about 60x30 px and the core about 9x5.

The trail's base RGBAQ is `0x22AC20(0x27F900, 128) << 24 | Q`, where the
`Q` field is built as `0xFE00 << 42` = `0x03F80000` - that is `1.0f`
shifted right by four bits, a ROM quirk that is invisible because the
line strip is untextured.

### 10.7 The colour tables

* `0x27EB30` = `{0x30, 0x62, 0x80, 0x3C}` - the colour **every** orb has
  in the idle menu (a dim blue at alpha 60).
* `0x27EB40`, 7 x 4 ints - the per-orb colours: blue, green, cyan, red,
  magenta, orange, white, all at alpha 60.

`0x226360`'s lerp runs per-orb -> base as the fade alpha goes 0 -> 128;
with no fade running it multiplies the per-orb table by **0** and the
base table by 128, so all seven orbs are the same blue. The individual
colours are only visible while a fade is in progress.

### 10.7a The entry animation is the screen fade

The two words `0x226360` keys on are **not** a browser transition -
`0x22AD30`/`0x22AD28` are `getFadeMode`/`getFadeAlpha` over `gp-28828`
and `gp-28824` (`menu-logic.md` 2 names them). `0x22ADD8(mode)` arms
them, `0x22B058` runs the counter, `0x22AFB8` paints the curtain from
the record at `0x27F630` at `A = 128 - alpha`:

| mode | colour | alpha | meaning |
|---|---|---|---|
| 1 | white | 0 -> 128 | flash |
| 2 | black | 0 -> 128, then mode = 0 | **fade UP from black** |
| 3 | black | 128 -> 0 | fade DOWN, then leave the module |
| 4 | black | 0 | instant |

**The module's one-shot init `0x21CE58` ends with `0x22ADD8(2)`.** So
mode 2 runs for the first 128 frames (~2.1 s) of every menu entry, and
during it `0x226360` throws each orb out along its random phase angle
by half a screen, pulling it back as `1 - sin(alpha*128)` closes, while
the colour lerps from the orb's own colour to the shared blue. That is
the menu's entry fly-in - the seven-orb version only when
`*(int*)0x1F05EC == 1` ("the module we came from was the opening"),
otherwise orb 0 alone. Mode 3 is the mirror image on the way out, orb 0
only, with the extra `sp = sp*alpha/128 + offset*1.5` zoom.

An earlier reading of this section called it a browser hand-off; it is
not - it is the always-on entry animation, and `osdbits/menu.c` ports
it (`FadeArm`/`FadeStep`/`DrawFadeCurtain`).

### 10.8 A freesce SDK bug that breaks this (and the opening)

While bringing the port up, `sceVu0ViewScreenMatrix` and the rotations
returned garbage. Cause: **freesce's `libvu0.a` has lost the
instruction out of several branch delay slots**, checked instruction by
instruction against the retail image's copies:

| function | ROM | freesce |
|---|---|---|
| `sceVu0MulMatrix` (0x267860) | `bne ...; addi a0,a0,16` | `bne ...; nop` then `addi` after the loop |
| `sceVu0RotMatrixX/Y/Z` | same loop, plus `j ...; li a3,1` | same loop bug; `li a3,1` unreachable |
| `sceVu0ViewScreenMatrix` | `jal MulMatrix; swc1 f20,56(sp)` | `jal; nop` then the `swc1` after |
| `sceVu0NormalLightMatrix`, `sceVu0LightColorMatrix` | args set in the `jal` delay slots | `nop`, args set after the call |
| `sceVu0RotTransPers/N`, `sceVu0Normalize` | - | same class of difference |

`sceVu0MulMatrix` alone is fatal: without the `addi` all four result
columns are written to column 0, so **every** matrix product is wrong.
`sceVu0ApplyMatrix`, `UnitMatrix`, `CopyMatrix`, `TransMatrix`,
`ScaleVector`, `OuterProduct`, `InversMatrix`, `TransposeMatrix`,
`ClampVector`, `AddVector`, `SubVector` and `ScaleVectorXYZ` are
byte-identical to the ROM.

The original SDK's copy is fine: `/usr/local/sce_24/ee/lib/libvu0.a`
has the `addi` in the `bne` delay slot and the `li a3,1` in the `j`
delay slot, exactly like the ROM. So the two ways out are (a) take
`libvu0.a` from `$(SCETOP)/lib` in `osdbits/Makefile` instead of
`$(LIBDIR)`, or (b) put the instructions back in freesce's assembly.

`osdbits/menu.c` sidesteps it by doing its matrix arithmetic in plain C.
This is very likely also why the opening looks wrong in freesce-linked
builds - `opening.c` calls `sceVu0MulMatrix` from `DrawLights`,
`DrawCube` and the flare/fog code, and with the broken version every
one of those transforms collapses onto column 0.

Method, if the same audit is wanted for `libgraph`/`libdma`: disassemble
the freesce archive, locate each function in the retail image by its
first four instruction words (the SDK code in the ROM is the same
build), and diff word by word. Only `libvu0` is hand-written assembly,
so nop delay slots in the compiled-C libraries are normal and not by
themselves evidence of anything.

### 10.9 Neighbours worth naming (not ported)

* **The backdrop mesh** - `0x21D0A0` calls `0x229358(m0, m1)`, which
  steps timer `0x27F190` and, **only while the transition state is 0**,
  tail-calls `0x2292D0`: bind TEXCKABE (`0x22AB90(1,1,2)`), then sixteen
  `0x229130(m0, m1, a, a+0x1000)` calls sweeping a full 16-bit turn.
  That is the deep-blue "smoke" wall behind the orbs, and it is the
  single biggest thing still missing from `osdbits`' menu mode.
* **The composite** - `0x21D0A0`'s tail: two `0x22A4C8` clears, two
  `0x22C190(0)` full-screen blits, `0x22A198`/`0x22A290` to make the
  offscreen target sampleable, then `0x2299C0(0x27B4B0)` - a full-screen
  additive blit at RGB `{0x37,0x28,0x3C}`, alpha 0x80. This is what
  gives the menu its motion-blurred wash; `0x2267E8`'s two-pass flush
  and `0x226768(30)` feed the same buffer.
* **The carousel** - the 12-entry ring at `0x34E6C0` (stride 48,
  rotation offset at `0x34E6C0` seeded from the hour) driven by
  `0x225BF8` from timer `0x27EB00`; when its progress passes
  `*(gp-32148)` = 0.05, `0x226028` appends the non-orb records that
  `0x2266E0` -> `0x22D920` and `0x22E428` draw. These are the fly-in
  objects, and the only part of the scene that needs VU1 (`vucode_2`,
  uploaded by `0x22EE88` -> `0x22EE00` from the chain at `0x268860`).
  The orbs need no VU1 at all - `0x22EFF0` emits GIF packets directly.

### 10.10 What the port renders, and how it was checked headless

`osdbits/menu.c` (mode `menu`) draws the seven orbs with their trails,
halos and cores, the entry fly-in and the fade-up curtain. Verified in
PCSX2 with no window: the EE prints each orb's projected screen
position, depth and colour for the first two frames, and
`DumpFrameAscii()` reads the drawn buffer back with
`sceGsExecStoreImage` and prints it as an 8x8-block luminance map (the
`debugFrame` argument). Two checks passed:

* the per-orb depths the EE printed (80345, 81859, 83526, 85056, 86152,
  86585, 86260 at 12:34:56, frame 1) reproduce a host-side model of the
  same arithmetic to the digit, so the camera, the orbit chain and the
  projection all agree with the disassembly;
* the readback shows the orb cluster and its trails exactly where those
  coordinates put them - at frame 50 with the entry skipped, and at
  frame 120 with the entry running, where the seven orbs have converged
  from their scattered start into the ring.

Performance note: with PCSX2's **software** renderer (`Renderer = 13`)
the scene runs at about **1 fps**, against 60 fps for the opening in the
same build. It is not a hang and it is not a coordinate blow-up - the
settled scene, whose vertices all sit well inside the screen, is just as
slow as the fly-in. The cost is 7 x 49 = 343 AA1 line segments per
frame, which the software rasteriser draws with per-pixel coverage
blending; the opening's light trails are the same idiom but only ~64
segments. Worth a check on a hardware renderer and on real hardware
before treating it as a problem - the ROM does twice this work, drawing
every orb into two buffers.
