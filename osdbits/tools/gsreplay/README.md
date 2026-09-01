# libgpu2 as a GS-dump replay engine — survey, harness, verification

Work dir: `/u/aap/.claude/jobs/58e316f8/tmp/libgpu2/`
Date: 2026-09-01.  `/u/aap/othersrc/libgpu2` was treated **read-only**; nothing
there was modified. The only file copied out and patched is `shims.c` (one
addition, marked in the file).

**Bottom line: it works.** A captured PCSX2 GIF stream replays through Sony's
1998 GS model in ~1.1 s/frame and reproduces the retail OSDSYS System Config
screen — cubes, refraction, backdrop rays, PSMT4 text — with the 4 MB local
memory readable at any point mid-frame. VRAM seeding round-trips bit-exactly.

---

## 1. What libgpu2 is

Sony's own **behavioural reference model of the PlayStation 2 Graphics
Synthesizer**, shipped as a static i386 library for host-side tooling (it is
what the SKY EE simulator draws through). Version `Ver1.12.0`, 23 unstripped
ELF32 objects built with g++ 2.7.2.3, `ar` timestamps 1998-08-03, tree mtime
1999-09-02. Recovered from a PS2 toolchain ISO.

aap has already surveyed it in depth. Do not re-derive; read these:

- `/u/aap/othersrc/libgpu2/FINDINGS.md` — the primary survey (pipeline block
  diagram from the mangled names, MemIF back end, TXM filters, diagnostic
  strings, the `GS_SaveImage` 3-byte bug, SKY integration).
- `/u/aap/othersrc/libgpu2/port/README.md` — the 32-bit relink recipe and the
  `gpu2d` network server.
- `/u/aap/othersrc/libgpu2/re/` — mechanically extracted struct/vtable/dispatch
  evidence (`STRUCTS.md`, `classes.h`, `DISPATCH.md`).

It is **behavioural, not timing-accurate**: no cycle/clock/scanline vocabulary
anywhere, `PCRTCxif::SetSYNCH2` is a bare `return`. Pipeline is
`Pre1 → Pre3 → PCalc → DDA → TXM → MemIF → Memory`, each stage a class, with a
`gpu2vec.o` trace-instrumentation layer subclassing every stage.

### Corrections to the existing notes (measured here, 2026-09-01)

Two claims in `FINDINGS.md` §9 and `port/README.md` are **wrong**, and they are
the two that would have made the model useless for us:

1. **"PRIM must have TME=1: flat (TME=0) primitives do not rasterize."**
   False. `probe.c` draws a flat 40×20 `SPRITE` (PRIM=6, no TME) and gets
   exactly 800 pixels at the right addresses with the right colour; a flat
   TRISTRIP gives 5050 px; Gouraud interpolates. The original observation was
   an artifact of the SKY sim emitting stray `PRIM(0)` writes with no vertices
   following.
2. **"PSMCT32 pixel order is (A,B,G,R), byte-reversed relative to real GS."**
   That is true only of the `GS_SaveImage` output path (which byteswaps and
   then drops a byte). In **local memory** the model stores PSMCT32 exactly as
   the real GS does: `u32 = A<<24 | B<<16 | G<<8 | R`. Verified by writing
   RGBAQ `0x80402010` and reading `0x80402010` back.

---

## 2. Build

Prereqs on this Void host (already installed):
`glibc-devel-32bit`, `libX11-32bit`, clang (Void's gcc is not multilib).

    ./build.sh          # builds probe, swz, fmt, regprobe, gsreplay

The recipe is aap's from `port/build.sh`: clang cross-compiles to i386, GNU `ld`
links by hand against `/usr/lib32` crt files, `port/libgpu2-patched.a`, 32-bit
libX11/libm/libc. Everything here links the **patched** archive (4-byte
`GS_SaveImage` pixels); we never call `GS_SaveImage` anyway, we read memory
directly.

`shims.c` is `port/shims.c` plus **one change**: `__builtin_new` records every
allocation ≥ 4 MB into `lg2_bigalloc[]`/`lg2_bigsize[]`. `GPU2::GPU2` makes
exactly one (`0x4001c8` = the `Memory` object), and **VRAM is the first 4 MB of
it**, so the harness gets a raw pointer to GS local memory with no exported API
at all. This is the single most important discovery for our purposes: reads
*and writes* of any buffer at any instant, no BitBLT, no readback path.

Endianness/bitfields: the objects are i386 little-endian and must stay 32-bit
(`long long` args are passed on the stack in the 1998 cdecl ABI). No bitfield
assumptions leak through the public API — everything is `(int addr, long long
data)`.

---

## 3. API cheat-sheet

The whole public surface is eight functions (`gpu2/i386-pc-linux-gnu/include/libgpu2.h`):

```c
void GS_InitSim(void);
void GS_OpenSim(char *title, int w, int h, int disp_on, int field);
void GS_CloseSim(void);
void GS_PutPort(int addr, long long data);      /* drawing registers 0x00-0x7f */
int  GS_PutCtlPort(int addr, long long data);   /* 0x12000000-0x120000c0 */
int  GS_SaveImage(char *filename);
void GS_SetSaveImageArea(FRAME_BUFFER *);
void GS_GetSaveImageArea(FRAME_BUFFER *);
```

Practical notes measured here:

- `GS_OpenSim(title, 640, 480, 0, 0)` is **headless**: no X connection, works
  with `DISPLAY` unset. Pass `disp_on=1` for a window (and then write register
  `0x7f` to make it repaint — the model only paints on `GS_REFRESH`).
- **There is no VRAM accessor.** Use the `lg2_bigalloc[0]` pointer.
- **Error contract is fatal**: an unknown register makes the model
  `fprintf(stderr,...)` then `exit(0)` — silently truncating a replay with a
  *zero* exit status. `regprobe.c` measured the accepted set by forking per
  address. Accepted: `0x00-0x0a, 0x0c, 0x0d, 0x11-0x1c, 0x22, 0x34-0x37, 0x3b,
  0x3d, 0x3f-0x54, 0x7f`. **Refused (fatal): `0x60 SIGNAL`, `0x61 FINISH`,
  `0x62 LABEL`**, plus every reserved address. `gsreplay` gates on this list.
  (Neither OSDSYS dump uses SIGNAL/FINISH/LABEL — `skipped-regs=0` on both.)
- Completeness, measured (`probe.c`, all PASS): flat + Gouraud prims, sprites,
  tristrips, `SCISSOR`, `TEST/ZTST=GEQUAL`, `ALPHA` blending, **AA1
  partial-coverage alpha**, PSMCT32/PSMCT16/PSMZ32 addressing, PSMT4 textures
  (proven end-to-end: the OSDSYS font text renders correctly).

---

## 4. THE IMPORTANT GOTCHA — the model's VRAM swizzle is not retail's

**The 1998 model uses a different (pre-production) local-memory address
mapping from the retail GS.** Page geometry is identical (8 KB pages,
`page = bp/32 + (y/32)*bw + x/64` for PSMCT32) but the *in-page* layout differs.

Measured two independent ways (`swz.c`: BitBLT upload, and 2048 one-pixel
sprite draws) — both agree exactly:

| | model | retail (gsmem.py) |
|---|---|---|
| block index | `by1 ^= bx2` | `bx0\|by0<<1\|bx1<<2\|by1<<3\|bx2<<4` |
| column index | `cx2 ^= cy1` | `cx0\|cy0<<1\|cx1<<2\|cx2<<3\|cy1<<4\|cy2<<5` |

Expressed on the retail dword index `i` within a page, the whole difference is

```c
pi(i) = i ^ (bit10(i) << 9) ^ (bit4(i) << 3);
```

`bit10` is `x&32`, `bit4` is `y&2`; `pi` touches only bits 9 and 3, so **`pi` is
an involution** — one routine converts both directions.

Verified against retail formats (`fmt.c`): PSMCT32, PSMCT16 and PSMZ32 all
match `pi(realGS)` with **zero** mismatches; retail `blockTable32Z` (= 32-table
XOR 24) is confirmed to apply to the Z formats. PSMT4 is verified end-to-end
(fonts render). PSMT8 was not verified — my transcribed retail `columnTable8`
is probably wrong; OSDSYS does not use PSMT8 so this was left open.

Why this doesn't hurt: the permutation is page-local and format-independent for
everything we use, so buffer aliasing (wb3/wb4 self-sampling, reading the
framebuffer as a texture at a different `tbw`, PSMCT24 views of PSMCT32) behaves
identically to real hardware. `gsreplay` applies `pi` per 8 KB page when seeding
and again when snapshotting, so **every file it writes is in real-GS physical
layout and `osdbits/tools/gsmem.py` reads it unchanged**.

---

## 5. The harness

### `gsprep.py <dump.gs|.gs.zst> <outdir>`

Unpacks a PCSX2 binary GS dump into `outdir/`:

| file | contents |
|---|---|
| `vram.bin` | 4 MB GS local memory, real-GS layout (`44+serial_len+shot_size+425`) |
| `stream.bin` | flat record stream, `"GSR1"` + count + `{u8 type, u8 path, u16 _, u32 nqw, data}` |
| `pre_state.bin` | 425 bytes of GS register state preceding VRAM (**not used yet — see §7**) |
| `post_state.bin` | bytes between VRAM and the packet stream (GIF path state) |
| `shot.png` | the dump's embedded screenshot |

Record parsing is `gscmp/decode.py`'s, unchanged (brute-force stream start, then
PCSX2's own framing).

### `gsreplay <dir> [options]`

Seeds VRAM, walks the GIF stream (PACKED / REGLIST / IMAGE, one state machine
per PATH, correct PACKED unpacking incl. the internal `Q` latched by `ST` and
the `ADC` bit selecting XYZ3/XYZF3), and snapshots the 4 MB local memory.

```
-s SPEC   snapshot at SPEC (repeatable)   -e SPEC   stop there
          end | vsync[:N] | xfer:N | reg:N | prim:N | draw:N | frame
-o PREFIX snapshot prefix (default <dir>/snap)
-l        log a line at every FRAME fbp change  (this is how you find ranges)
-v        log every register write to stderr    -q quiet   -n don't seed
-w        open the model's X window, GS_REFRESH each vsync
-Z A:B:HEXZ  force the Z of vertex kicks A..B    (hypothesis test)
-x A:B       make vertex kicks A..B write nothing (bisect a pass)
```

Snapshots are 4 MB raw files, real-GS layout.

### `topng.py <vram.bin> [tbp:bw:w:h[:name] ...]`

Renders buffers to PNG (RGB and alpha, 2× vertical) exactly like `gsmem.py`.
Defaults to the retail OSDSYS set (scr0 0, scr1 2240, wb3 6720, wb4 8960, all
640×224 PSMCT32). `topng.py -c A.bin B.bin [spec...]` prints a numeric diff.

### Diagnostics

`probe` (behaviour suite, 0 failures), `swz` (derive the in-page mapping),
`fmt` (per-format layout vs `pi(realGS)`), `regprobe` (which registers are fatal).

### Usage

```sh
./build.sh
python3 gsprep.py '/u/aap/.claude/jobs/58e316f8/tmp/gscmp/PS2_BIOS_(USA)_..._080614.gs' r614
./gsreplay r614 -s vsync                       # snapshot each frame
python3 topng.py r614/v000_vsync.bin           # -> PNGs
./gsreplay r614 -l -q -e vsync:1 -o /tmp/x     # list the frame's segments
```

Cost: ~4.5 s for a 4-frame dump (≈1.1 s/frame, 4116 transfers / 82064 register
writes / 23792 vertex kicks); 4 MB per snapshot.

---

## 6. Verification result

Retail dump `PS2_BIOS_(USA)_20020207-164243_20260901080614.gs`, replayed with
`gsreplay r614 -s vsync`. Reference: `gscmp/v_r614_scr0.png`, the decode of the
dump's own VRAM scr0 — i.e. the frame PCSX2 rendered *before* the capture. The
scene animates, so a perfect model still drifts from it; the animation cost is
the baseline row.

```
BASELINE: PCSX2 scr0 vs PCSX2 scr1             mean|d|  3.889  >8: 11.21%
seed round-trip (scr0 before any draw) vs ref  mean|d|  0.000  >8:  0.00%   <-- BIT EXACT
replayed final scr0 vs ref (4 frames of drift) mean|d|  2.155  >8:  7.78%

static regions, final replayed scr0 vs ref  (baseline in brackets)
  top status bar                  0.352   [3.304]
  bottom button bar               0.155   [4.975]
  "System Configuration" caption  1.274   [10.145]
```

Reading:

1. **Seeding is exact.** At the first vsync `scr0` has not been drawn into yet,
   and it is byte-identical to `vram.bin`'s `scr0` — the `pi` permutation
   round-trips perfectly and nothing else corrupted memory.
2. **Rendering is faithful.** In regions that do not animate, the replayed
   frame matches PCSX2's render to 0.15–1.3 / 255, an order of magnitude below
   the frame-to-frame animation baseline. The whole-screen 2.155 is animation
   drift (four frames), not model error; it grows monotonically frame by frame
   (1.462 → 1.512 → 2.108 → 2.155).
3. **Visually it is the real thing.** `r614/snap000_end_scr0.png` is a complete
   correct System Config screen: five glass cubes with refraction, the rod
   clock, backdrop rays, orb glows, yellow/blue captions, the date and time,
   and the button bar — all rendered by a 1998 software model.
4. **The debugging signal survives.** `r614/v000_vsync_wb4_a.png` shows the
   five **solid** grey silhouettes that `gscmp/FINDINGS.md` Finding 1 cites as
   the retail signature; our port's dump replayed through the same harness
   (`o519/base000_stop_wb4_a.png`) shows the **cracked** mask with the tristrip
   diagonals cut into it. The harness reproduces the exact artifact under
   investigation.

Our port's dump (`main_[?]_main_20260901080519.gs`) also replays cleanly
(5939 transfers, `skipped-regs=0`, 4.5 s) and renders correctly, with the cube
seams plainly visible in `o519/v003_vsync_scr0.png`.

---

## 7. Caveats and open items

- **Register state is not seeded.** `gsreplay` starts the model at defaults and
  relies on the captured frame writing everything it needs. That held for both
  OSDSYS dumps (the ROM re-emits full state each frame), and the bit-exact seed
  check plus the static-region match prove nothing important was missing. But
  `pre_state.bin` (425 bytes, PCSX2's `GSState` freeze of PRIM/TEXA/CTXT/…) is
  extracted and unparsed; if a future dump starts mid-frame, parse it and
  replay it as register writes first.
- **PSMT8 layout unverified** (see §4). PSMT4 is verified only end-to-end.
- **`-Z` / `-x` ranges are in vertex-kick units**, which is *not* `drawlog.py`'s
  `Dnnnn` draw-run numbering. Anchor them with `gsreplay -l` (which prints
  `draw=`, `prim=`, `xfer=`, `vsync=` at every FRAME change) or with `-v`.
- **My one experiment was inconclusive** and should not be believed: I forced
  flat Z over draws 3345–3921 of our dump expecting the Finding-1 crack to
  close, and nothing changed — because by draw 3922 the cube stage has not run
  yet (wb4 differs from the seed by only 7.6% there; the cracked mask I was
  looking at was mostly leftover previous-frame content). The knobs themselves
  are verified working (both change output; a no-knob run is byte-identical to
  the earlier baseline, so there is no regression). Locate the real cube-stage
  range first.
- The model is not cycle- or hardware-exact. It is a *golden functional model*
  and a 1998 one; where it and PCSX2 disagree, neither is automatically the PS2.
  Residual differences here were below the animation floor, but for a
  sub-pixel/AA-coverage argument treat it as a strong second opinion, not
  ground truth. Real hardware (the dsedb/dsnet TOOL workflow) remains the
  arbiter.
- No git commits, nothing published. `/u/aap/othersrc/libgpu2` untouched.

---

## 8. How to use this for the cube/rod refraction work

The thing this buys that savestates and dumps cannot: **any buffer, at any
instant, on demand, and the ability to change one thing and re-render.**

1. **Locate the stage.** `./gsreplay o519 -l -q -e vsync:1 -o /tmp/x` prints
   every FRAME target change with `draw=` / `prim=` / `xfer=` counters.
   fbp is in pages; the line also prints the block address, so retail wb3/wb4 =
   6720/8960 and ours = 7040/9600 are easy to spot. Cross-reference with
   `gscmp/drawlog.py` output for the semantic labels.
2. **Watch a buffer evolve inside one frame.** `-s draw:N` repeatedly, then
   `topng.py`. This is the direct answer to "what does wb4 look like *after the
   AA1 pass but before the bump taps*" — a question the PCSX2 dump cannot
   answer at all.
3. **Test Finding 1 properly.** Find the cube-stage vertex-kick range, then
   `-Z A:B:FFF010` and compare `wb4`'s alpha mask against the un-forced run
   with `topng.py -c`. If the partial-coverage pixel count collapses, the
   flat-Z fix is confirmed *before* touching `menuconfig.c`.
4. **Bisect a pass.** `-x A:B` masks a range's writes. Kill the bump taps, the
   refract pass, or the composite in turn and see which one owns the artifact.
5. **Test Finding 2 (orbs missing from wb3) the other way round.** Replay the
   *retail* dump with `-x` over the orb→wb3 twin draws; if retail then looks
   like ours, that closes the loop on the diagnosis without a rebuild.
6. **Diff retail vs ours at the same stage.** Both dumps replay into identical
   file formats, so `topng.py -c r614/…bin o519/…bin <tbp>:10:640:224:name`
   gives a per-buffer numeric diff at matched points in the frame.

Fastest path to value: step 1 then step 3 — the flat-Z hypothesis is a
five-minute experiment now, and it is currently the top open item in
`gscmp/FINDINGS.md`.
