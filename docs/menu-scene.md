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
