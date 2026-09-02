# padfix: the ROM pad pipeline, reversed, and pad.c brought in line

Deliverable: `padfix.diff` (pad.c only; `git apply`-checked clean against both
562e1ee and the current working tree — inc.h already declares `OsdArgInt`, so
no inc.h line was needed).  Scratch build: `make` in the copied osdbits,
`ee-gcc -O2 -Wall`, zero warnings, links.

## The reversed pipeline (all addresses = VA in expanded.bin, gp = 0x2AF070)

Two ROM stages produce the canonical word the matched menu consumers read.

### Stage 1 — the pad thread, `ThreadX_pad_timer` 0x206E00..0x2070F4

Per vsync (WaitSema loop):

* Walks all 8 port/slot combos (multitap-aware), one probe per frame:
  opens/closes via scePadPortOpen 0x24C5D8 / Close 0x24C838, DMA buffers at
  0x2DD400 + idx*0x100.
* Merge loop 0x206F7C..0x20701C: for every open pad, scePadGetState 0x24CA20 +
  scePadRead 0x24C9A0 into 0x2C92C0.  A read counts only if **state is 2
  (FindCTP1) or 6 (Stable) and the read returned 32 bytes** (0x206FD4-0x206FE8).
  Accepted pads AND their two active-low button bytes rdata[2]/rdata[3] into a
  merged buffer at 0x26FE18 (initialized 0xFF/0xFF = nothing pressed).
* **The region swap, 0x207024..0x207058**: calls `0x204318()`; if non-zero,
  the raw low byte is reshuffled `(b & 0x9F) | ((b & 0x20) << 1) |
  ((b & 0x40) >> 1)` — i.e. **circle (0x20) and cross (0x40) trade places**.
* Publishes: merged bytes to 0x1F0C58..0x1F0C5B (buttons in 5A/5B), state word
  (6 if any pad read, else 0) to 0x1F0C78.  This 0x1F0Cxx block is the shared
  mailbox the menu half reads.

`0x204318` = `max(romver_204238(), 0)`.  `romver_204238` (0x204238) opens
`rom0:ROMVER` (path string at 0x2A33D8), reads 16 bytes, and maps **character
4** (the region letter of e.g. "0100JC20000117"): `'J'` → 0 (Japan), `'A'` and
`'H'` → 1, `'E'` → 2, anything else stays -1 (clamped to 0 = no swap).  Cached
at 0x26ECE8; char 5 (console type) is stashed at 0x26ECE4.  This is the same
region signal that drives the hint-bar block choice and osdGetString's 85<->86
exchange (docs/menu-config.md §2.5) — one flag, three coordinated swaps, so
the button drawn under "Enter" is always the button that enters.

### Stage 2 — the cooker, 0x22BE30..0x22BF54 (called once per menu frame at 0x21CFB4 inside the menu tick 0x21CF20)

```
held  = ~((byte[0x1F0C5A] << 8) | byte[0x1F0C5B])   ; only if state 2 or 6, else 0
chg   = held ^ prevHeld
gp-30320 (0x2A7A00) = held                           ; held word
gp-30312 (0x2A7A08) = chg & ~held                    ; release edges
gp-30316 (0x2A7A04) = chg &  held                    ; PRESS EDGES  <- "padWord"
gp-30308 (0x2A7A0C) = repeat word, UP/DOWN only:
    per-direction counter (0x2A7FF0 up / 0x2A7FF4 down): -1 while released,
    counts up from 0 while held (0 = the press frame);
    count == 0                     -> the bit fires (press edge)
    count >= 31 && count % 3 == 0  -> the bit fires (repeat)
    => first repeat 33 frames after the press (33 is the first multiple of 3
       >= 31; 31 and 32 don't divide), then every 3 frames (20 Hz NTSC).
    Quirk: a fresh DOWN press STORES 0x4000 over the word (sw at 0x22BF20),
    clobbering an UP repeat due the same frame; fresh UP stores too (0x22BEC8)
    but that one lands on an empty word.
```

A companion zeroer 0x22BE18 (called from the menu-init 0x21CE58) clears the
press and release words and both hold counters on menu entry.

### The canonical bit layout (== libpad's, == inc.h's PAD_* enum)

| bit | meaning | | bit | meaning |
|--|--|--|--|--|
| 0x0001 | L2 | | 0x0100 | Select |
| 0x0002 | R2 | | 0x0200 | L3 |
| 0x0004 | L1 | | 0x0400 | R3 |
| 0x0008 | R1 | | 0x0800 | Start |
| 0x0010 | Triangle | | 0x1000 | Up |
| **0x0020** | **confirm** (circle slot) | | 0x2000 | Right |
| **0x0040** | **cancel** (cross slot) | | 0x4000 | Down |
| 0x0080 | Square | | 0x8000 | Left |

Truth table for the two region-dependent bits (all others map physically):

| physical button | Japan ('J', flag 0) | world ('A'/'H'/'E', flag != 0) |
|--|--|--|
| Circle | 0x20 (confirm) | 0x40 (cancel) |
| Cross  | 0x40 (cancel)  | 0x20 (confirm) |

Consumers verified against this: MainMenuInput 0x228278 (matched, mmi.c) reads
gp-30316 and tests 0x1000 up / 0x4000 down / **0x20 confirm** / 0x10 leave
(Triangle); config screen 0x2279B8 tests 0x1000, 0x227BE8/0x227C20 confirm on
0x20; the clock editor 0x21E870 reads gp-30316 for left/right (press only —
**left/right never repeat anywhere in the ROM**) and gp-30308 for up/down
(press + repeat).

## What was wrong in pad.c

1. **No region swap at all** — `pad.press` bit 0x20 was always physical
   Circle.  A consumer testing 0x20 the way the matched ROM code does would
   confirm on Circle on every console — the Japanese arrangement — while the
   text layer (`textRegionSwap`, default 1) draws "Enter" on Cross.  This is
   exactly aap's "kinda wrong - could be japanese vs world?".  (The current
   menutext.c consumers paper over it by accepting `PAD_CROSS|PAD_CIRCLE` for
   confirm; once consumers use the canonical 0x20/0x40 they now just work.)
2. **Repeat was guessed** (its comment said so): delay 20 / rate 5, applied to
   all four directions.  Real ROM: **up/down only**, first repeat 33 frames
   after press, then every 3.
3. Minor: reads were accepted in any pad state; the ROM gates on
   Stable/FindCTP1.

## What changed (all in osdbits/pad.c)

* `btns` gets the ROM's exact bit shuffle when the region flag is set
  (swapping the composed active-high word commutes with the inversion —
  verified exhaustively, below).  Region proxy: `OsdArgInt(16, 1)`, the same
  argv slot menutext.c's `textRegionSwap` uses (default 1 = world), read
  lazily on the first UpdatePad because InitPad runs before ParseArgs has
  located the numeric args.  So `pad.press`/`pad.btns` bit 0x20 now always
  means "the confirm button", 0x40 "the cancel button", region-correctly and
  in agreement with the hint bar.
* The repeat logic is now the ROM's, verbatim: two counters (-1 released,
  0 on press), fire at `count == 0` and at `count >= 31 && count % 3 == 0`,
  up/down only, including the store-over-OR quirk on a fresh DOWN press.
  `pad.dirPress` = gp-30308 for up/down + gp-30316's press edges for
  left/right — which is precisely the word each ported consumer models
  (ConfigValueStep/ClockEditInput left-right = gp-30316 reads, clock up/down
  = gp-30308 reads).  Left/right no longer auto-repeat, matching retail.
* Read acceptance gated on `scePadStateStable || scePadStateFindCTP1`
  (0x206FD4's gate).
* The stick remains a port amenity (merged into `dirs` before the counters,
  so a held stick repeats like a held dpad); the retail thread reads only the
  raw button bytes.

Contract for the consumer files (owned by the parallel agent): a function
matching a ROM read of gp-30316 should test `pad.press`; one matching
gp-30308 should test `pad.dirPress` (up/down).  `pad.press & PAD_CIRCLE`
(0x20) = region-correct confirm, `pad.press & PAD_CROSS` (0x40) = cancel,
`PAD_TRIANGLE` (0x10) = the leave/options button.  Note the ROM's main-menu
cursor does NOT repeat (0x228278 reads press edges only); menutext.c currently
uses `pad.dirPress` for it, which now adds the retail up/down repeat feel —
if strict parity is wanted there, that consumer should switch to `pad.press`.

## Verification

* Clean build: `make` in the scratch osdbits, `ee-gcc -O2 -Wall`, no
  warnings, links to main.elf.
* `swapcheck.c` (in this directory): all 65536 (hi, lo) raw-byte pairs — the
  ROM's raw-byte shuffle + inversion == the port's word-level shuffle.
* `repcheck.c`: 400-frame scripted + randomized hold pattern — the port's
  dirPress equals the ROM cooker's `(press edges & LEFT|RIGHT) | repeat word`
  on every frame, including the down-press-clobbers-up-repeat quirk; up hold
  fires the press edge at frame 0, first repeat at frame 33, then every 3.
* `git apply --check` of padfix.diff passes against the real tree (pad.c is
  identical at 562e1ee and current HEAD).

**Live button-press verification is deferred to aap on hardware/emulator** —
the headless PCSX2 harness has no controller injection.  One-line test plan:
boot to the menu and confirm an item with Cross (default argv, world
arrangement) and with Circle after passing `... 0` in argv slot 16 (Japan) —
the working button must always be the one the hint bar labels "Enter", and a
held Up/Down should step once, pause about half a second, then run at 20
steps/s while Left/Right never auto-repeat.
