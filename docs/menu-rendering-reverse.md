# OSDSYS menu rendering: the VIF1/GIF layer (2026-08-22)

Reverse-engineering notes on the UNNAMED VIF1/GIF functions in the boot
OSDSYS EE program (image.bin, base 0x200000).  The opening scene's
custom packet library (spr*/vif1*/pktSet*, 0x2127a8-0x213ec0) is
opening-only, as aap observed.  The MENU renders through a different
stack, identified here.

## Summary

The menu draws by building VIF1 packets DIRECTLY with the SCE packet
library (sceVif1Pk*, linked into the binary at 0x2664xx-0x266a60) and
kicking them with sceDmaSend/sceDmaSync, plus a thin custom layer:

- packet buffers live in the SCRATCHPAD (0x70000000), double-buffered
- a custom "kick" helper sets the IRQ bit in the head tag before send
- geometry is 2D quads/sheets transformed by the SAME 0x267xxx matrix
  helpers the opening's towers use (that library is shared; the
  vif1/packet layer is not)
- textures/fonts come from files via an RPC + resource loader
  (loadImage_Resource @ 0x225038) and are uploaded with LoadImage
  packets (sceVif1PkRefLoadImage) or custom UNPACK chains (0x230000)

## Globals (gp = 0x282E1C, verified via gp-32484 = 0x27AF38 = 0.5f)

The static tables (OpeningTexList etc.) are addressed absolutely (lui
0x28...); gp-relative refs are the menu's RUNTIME state:

| gp offset | address  | use                                          |
|-----------|----------|----------------------------------------------|
| -29600    | 0x27BA7C | current packet context ptr (set by 0x2403C8) |
| -29640    | 0x27BA54 | sceDmaChan* used for the kick                |
| -29596    | 0x27BA80 | u8: packet double-buffer index (0/1)         |
| -29592    | 0x27BA84 | u32: input/pad state (0x808f checks)         |
| -29688    | 0x27BA24 | u8: packet-in-flight flag                    |
| -29672    | 0x27BA34 | u32 flag                                     |
| -29668    | 0x27BA38 | u32 counter (0..121, sub_240178)             |
| -29664    | 0x27BA3C | u32 (0/27)                                   |
| -29496    | 0x27BAE4 | element index for sub_245038                 |
| -29468    | 0x27BB00 | u32                                          |
| -29392    | 0x27BB4C | u32                                          |
| -29372..  | 0x27BB60+ | u32/float scratch (sub_245308)            |
| -28748    | 0x27BDD0 | u32: browser state machine state (0/1/2)     |

## Static tables

- 0x295AD0: 2 x 32-byte sceVif1Pk packet contexts (double buffer)
- 0x295CD0: GS register data template, 2 x 16 bytes (sub_245038)
- 0x295D00: GIF tag template qword (sub_245038)
- 0x295D10: menu element descriptors, 352 bytes each, indexed by
  *(gp-29496) (sub_245038: 244(el) = float position, 48(el) = verts)
- 0x295A20: texb_resIDs - the menu texture resource ID table
- 0x296160: texture descriptors, 28 bytes each, indexed by a1&0xFFFF
  (sub_246D28: 0 = u16, 8 = u8, 12 = u16, 24 = u16)
- 0x284100: static vertex table used by sub_245038's 2nd transform

## The packet layer (0x240000 region)

sub_2403C8 (0x6c) - OPEN a packet: reads the double-buffer byte
(gp-29596), selects packet context s0 = 0x295AD0 + idx*32 and scratch
buffer a1 = 0x70000000 | (idx << 13) (8KB per half), then
sceVif1PkInit(s0, a1), sceVif1PkReset, sceVif1PktCnt(s0, 0),
sceVif1PkOpenDirectCode(s0, 0), stores s0 to gp-29600, returns s0.

sub_240438 (0x88) - KICK: s0 = *(gp-29600);
sceVif1PkCloseGifTag/sceVif1PkCloseDirectCode/sceVif1PkEnd(s0,0)/
sceVif1PkTerminate(s0) append the chain end; sceDmaSync(ch,
0, 0); then a1 = *(s0+4) & 0x0FFFFFFF | 0x80000000 - the head tag's
QWC word with the IRQ bit set - passed as sceDmaSend(ch, a1)'s qwc
argument; clears gp-29600 and flips the double-buffer byte
(return value = old flag).  So the send always raises a DMA IRQ and
the next open uses the other scratch half.

sub_240068 (0x110) - browser state machine: reads *(0x1F0000A4) and
*(0x1F0000B0) (the 0x1F0000 "low" segment = system state globals),
checks the value 5/8 in 0x1F0000B0 (disc states), calls 0x2036B0 /
0x23BFD0 / 0x23BFF0, transitions gp-28748 (0/1/2).  Logic, not
rendering: disc-tray/drive state handling.

sub_240178 (0x160) - another state machine: watches *(gp-29592) for
0x808F (a pad button code) and 2048, counts gp-29668 up to 121,
writes gp-29664 = 27 - looks like an animation/timer or input-echo
state.  Logic.

sub_2404C0 / sub_2407A8 / sub_2409A0 - helper layer between open and
kick (end-tag append + sync-wait variants; to be detailed on demand).

## The element renderer: sub_245038 (0x29C)

sub_245038(a0, a1) - draw one menu element:
1. v1 = *(gp-29496); el = 0x295D10 + v1*352 (element descriptor).
2. Builds a model matrix on the stack: reads 244(el) (a float - the
   element's angle/pos), calls 0x267630, then 0x267370 (RotMatrix),
   0x2676B0 (translate, row 3), 0x267860 (transform) - the SAME
   0x267xxx matrix library the towers use (0x2676B0 = the row-3
   translation helper ported for the towers).
3. Transforms the element's verts: 0x267860(dst, el+48, 0x284100)
   (element verts x static table) and scales by s1 via 0x23F6A0
   (matrix -> screen-space helper).
4. Opens the packet (sub_2403C8), then sceVif1PkOpenGifTag(pkt, tag)
   with the tag qword lq'd from 0x295D00.
5. Streams GS data with sceVif1PkAddGsData: a first qword, then a
   loop of 2 entries reading 4 u32s each from 0x295CD0 + 16*i,
   packing them (v1<<16 | v0<<8 | base | 0x80000000-bit s7) - fixed
   GS register state per element.
6. Adds the transformed vertices (XYZF2-style fixed point: x|y<<16,
   z rounded (z+15)>>4) as one more AddGsData qword.
7. sub_240438 (kick).

Callers: sub_238228 (x3), sub_2437A0 (x4), sub_248370 - the menu
SCREEN renderers (each screen draws its elements through this).

## The texture uploader (0x246xxx)

sub_246D28(a0, a1, a2, a3, t0, t1) - upload a menu texture:
entry = 0x296160 + (a1 & 0xFFFF)*28 (28-byte descriptor: u16/u8/u16
fields at 0/8/12/24), reads the source struct a0 (fields at 0/4/8/
12/16/20 = data ptr, size, CLUT info...), and calls sub_246DE8 twice
- once for the CLUT, once for the image (a1=2/a2=4 style params).

sub_246DE8 - the actual upload: calls 0x24DCE0(0) (file RPC open?),
then sceVif1PkInit/sceVif1PkReset, sceVif1PkRefLoadImage, PkEnd,
PkTerminate, sceDmaSync, sceDmaSend - a standalone LoadImage packet
per texture/CLUT.  Driver: sub_247070.  The resource IDs come from
texb_resIDs at 0x295A20.

## The resource loader (0x225xxx)

loadImage_Resource (0x225038, 0x2D4 - aap's name) + ~40 helpers
(0x225310-0x226FD0): the menu's image resource loading (TIM2-style
parse/decompress), which ends in setTextureUpload (0x230708) - the
0x230000 cluster (aap names: setTextureUpload/setScreenMatrix/
setLightMatrices; unnamed: sub_2307F8 and friends, using D1VIF1 MMIO
directly = custom UNPACK chains, no libpkt).

## Architecture sketch

```
menu screen renderers          texture loading
 sub_238228 / sub_2437A0       sub_247070
 sub_248370                    sub_246D28 (CLUT + image)
     |                             |
     v                             v
 sub_245038 element renderer   sub_246DE8 uploader
 (0x295D10 desc, 0x267xxx        (file RPC 0x24DCE0 +
  matrix helpers,                 sceVif1PkRefLoadImage)
  0x295D00 tag, 0x295CD0 data)
     |                             
     v                             
 sub_2403C8 packet open             
 (SCRATCHPAD 0x70000000,          
  sceVif1Pk* context @0x295AD0)    
     |                             
     v                             
 sub_240438 kick                    
 (IRQ bit in head tag,             
  sceDmaSend/Sync, double-buffer   
  flag flip)
```

Separate track: 0x225038 loadImage_Resource -> setTextureUpload
(0x230000) -> custom VIF1 UNPACK chains (the "renderer core" the
structure report flagged as missing from osdbits).

## The localization system and the configuration menu (2026-08-22)

Found via the string tables, as suggested.

### String data layout

Each language = a pointer table + string block pair:

- JP table 0x2972A8 (strings 0x297xxx, Shift-JIS - not ASCII-scannable)
- EN table 0x298B08 (strings 0x299000-0x29A100)
- FR table 0x29A2A4 (strings 0x29A5A0+)
- + 5 more tables: 0x29A0E8, 0x29B9D8, 0x29D1D0, 0x29EA88, 0x2A0250,
  0x2A1B30 (per-language: DE/IT/ES/PT/NL...)

The language selector = a 9-entry pointer array at 0x26ECC0:
0x26ECC0[lang] = the language's string table.

### The string API (0x203xxx-0x204xxx)

- 0x2037B8 = GetLanguage()
- sub_204170(lang) = "select string table": caches 0x26ECC0[lang]
  into 0x26EDE0.
- sub_2041B8(id) = "get string": returns *(0x26EDE0)[id] - the
  pointer to string ID `id` in the currently selected language table.
  Special cases: id 85/86 gated on 0x204318 (version check), then
  read through a struct at +340/+344.
- The resource table accessors (aap's res.c) = getResourcePtr /
  getResourceSize at 0x26ED00+.

### Config-menu string IDs (EN table)

  89 Version Information      97 DVD Player
  90 Browser                 102 CD Player
  91 System Configuration    105 Disc Speed,Standard,Fast
  92? (toplevel entry)       106 Clock Adjustment
                             107 Screen Size
                            114 Component Video Out
                            140 Time Format   143 Date Format
                            151 Time Zone     152 User Preferences

Per-language title records at 0x2A4380: 32-byte entries
{0x100, x, titleID(+0x100 flag), subID} - e.g. {0x100, 0x40, 0x5B
(91 System Configuration), 0x66 (102)} repeated ~30 times with the
bit-8 flag alternating (per screen/state variants).

### The config UI code (string consumers)

- sub_224630 (0x734) = the big screen renderer: 15 getString + 5
  selectString calls, 3x loadImage_Resource (0x225038, screen
  images), 9x sub_22AC20 + 3x sub_22AC48 (the 0x2299C0-0x22B0E8
  widget cluster - per-item draws), 6x sub_21DC88, and 0x209640/
  0x209998 helpers.
- The other consumers cluster in: 0x21d7f8/0x21ef00/0x21ff88/
  0x220950/0x220df8/0x221230/0x2215e0/0x2217d8/0x2219e0
  (0x21dxxx-0x222xxx, just above the resource loader),
  0x241f18/0x242828/0x2429c8/0x242c38/0x2435b0/0x2443c8/0x244d70
  (the packet-layer region), and 0x24a610/0x24ac08/0x24b1d8
  (0x24Axxx - the two 20-string functions).
- sub_2051f8 = passes Disc Speed / Diagnosis IDs directly.

The flare/effect code sits right below this cluster: FlareThing
0x21a300, flare_21A6D8/21AA50/21AF18, DrawRedFlare 0x21b4c8 (aap
names) - candidates for the config menu's background effects.

### Still to pin down

- Which of the consumer clusters is the config screen vs browser vs
  memory-card screens (the 0x2A4380 title table is reached through a
  struct base pointer, not directly).
- What the 0x2299C0-0x22B0E8 widget cluster does (per-item draws for
  sub_224630).
- The fancy effects themselves: who calls FlareThing/DrawRedFlare in
  the config screens, and what 0x209640/0x209998 do.

## Open items

- sub_2404C0/2407A8/2409A0 exact roles (end-tag/sync variants).
- The element descriptor (0x295D10, 352B) and texture descriptor
  (0x296160, 28B) field layouts.
- What the 0x230000 UNPACK chains upload (CLUTs? textures?) and how
  they differ from the 0x246xxx LoadImage path.
- Which channel gp-29640 holds (probably D8 fromSPR; TTE flag?).
- The screen renderers 0x238228/0x2437A0/0x248370 (what each screen
  draws, the element index sequence).
- 0x1F0000 low-segment globals (system state shared with the kernel).
