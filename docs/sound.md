# OSDSYS sound path — reverse notes + port (2026-09-02)

Deliverables in this directory:
- `sound.diff` — git-apply-able against the working tree (verified with
  `git apply --check` at 11:35; the tree was moving while this ran — the
  hunks are all small and additive, trivial to rebase if it drifts).
- `osdbits/` — the scratch tree the diff was cut from, already built
  (`main.elf`), with the regenerated `res/SND*_EXP.inc` files.
- `full.dis` — objdump of expanded.bin (base 0x200000), the source of
  every address below.
- `extract/` — the SND* blobs decompressed from the BIOS, plus
  `OSDSND.irx` and its disassembly `osdsnd.dis` (scratch only; no Sony
  bytes leave this directory).

**res/*.inc are NOT in the diff.** Regenerate them in osdbits/ with the
unmodified extractor (it already handles the SNDIMAGE container):

    python3 tools/extract-res.py <bios.bin> res --container SNDIMAGE

(cut from `/u/aap/src/ps2rev/firmware/scph39001.bin` here; 12 files,
`SNDBOOTH_EXP.inc` … `SNDRCLKS_EXP.inc`).

## The reversed path

OSDSYS plays ALL sound on the IOP. `rom0:OSDSND` = `rspu2_driver` 1.03
(rspu2drv), a remoted libspu2 + libsnd2 behind **SIF RPC server
0x80000601** (ps2sdk `iop/sound/rspu2drv/src/rsd_com.c` is a decomp of
it — `#define sce_SPU_DEV 0x80000601` — but with the entire `St*`
0x5xxx command block `#if 0`'d, so ps2sdk/audsrv is NOT a substitute
for the OSD messages; only the real ROM module implements them. The
extracted ROM module confirms it: the full `_SsSnd*` sequencer is
statically linked, and its RPC dispatch (osdsnd.dis 0xbf4..) routes
every St fno — 20481→0x2d38, 20486→0x2dbc, 20489→StBgmOpen 0x1abf8,
20500→StBgmPlay 0x1b5bc, 20501→0x308c, 20506→0x31ec, 20736→0x31b4,
20992→StSePlay 0x1be88). It is self-contained: imports only
sysmem/intrman/loadcore/sifcmd/sifman/sysclib/thbase/timrman —
**LIBSD.IRX is not needed**. The real IOP gets OSDSND at boot from
OSDCNF's IOPBTCONF.

EE side, three layers:

### OSDDispatch2 = the RPC caller (real 0x261738; bind at 0x261a00)

`sceSifCallRpc(cd @0x2967c8, fno = the message number, mode, send =
0x3f3240 (64 B), 16-B reply into the same buffer)`. Packet: word 0
scratch (written with the packet's own address; the reply's word 0 = the
IOP function's return value), words 1..6 = the varargs. First arg x:
x!=0 → mode 0 (blocking; every OSD call), x==0 → mode 1 NOWAIT with the
end-callback global at 0x2967c0.

Special message classes (none needed by the OSD's own sound use):
- `0x6xxx` — recv 64 B into args[1] (SpuGet*Attr; low bits = struct size)
- `0x7xxx` — send 64 B from args[1] (SpuSet*Attr structs)
- `0x7600` — send args[2]*64 B from args[1] (multi SpuSetVoiceAttr),
  preceded by scePrintf of the string at 0x2a6728
- `0x8100/0x8200/0x8600` — install IOP-side callbacks, cache the arg at
  0x2967fc/0x296800/0x296804; `0x8300/0x8400/0x8500` — set the EE
  callback globals 0x2967f0/f4/f8 without any RPC

Bind function 0x261a00: sceSifInitRpc(0); loop { sceSifBindRpc(cd,
0x80000601, 0); delay; } until cd.serve != 0 (forever, printing the
string at 0x2a6720 on bind error); then FlushCache and OSDDispatch2(1,
**1**) = SpuInit. Returns 1.

### OSDDispatch = a mailbox, not an RPC (real 0x200b80)

128-entry ring of four parallel u16 arrays at 0x1f05f8 (stride 8:
msg/a/b/c), write index 0x1f0a00, read index 0x1f09fc, lock word
0x1f09f8. Writer sets the lock, drops the entry (or returns -1 when
full), clears the lock. Animation/menu threads only ever enqueue.

Drain (real 0x200a50, run from the periodic thread at 0x206e00): if
lock set, skip; else for read..write, each entry with msg != 0 goes out
as OSDDispatch2(1, msg, a, b, c), entry zeroed; read = write; then
0x2009b0(60): for the first 60 ticks queue StSetReverbDepth(core,
depth, depth) for both cores with depth = count*0x9ffec/127/60 — a
reverb fade-in (counter at gp-8064 = 0x26e080).

### The init suite (real 0x2004b8, called from main at 0x207774)

Helpers: 0x2000c0 = FlushCache + sceSifSetDma{src,dst,size,0} + poll
sceSifDmaStat (synchronous EE→IOP copy); 0x200130 = chunked SPU upload:
per 0x10000 chunk: copy to the IOP staging buffer, OSDDispatch2(1,
20506 StDmaWrite, iopBuf, spuAddr, 0x10000), OSDDispatch2(1, 20487
StVabOpenCompleted) as the transfer sync. (The real tail chunk is a
FULL chunk — reads past the source and writes stale bytes past the
sample; the port zero-pads instead, the overshoot region is dead space
either way.)

0x200250 loader: sceSifInitIopHeap (server 0x80000003) +
sceSifAllocIopHeap(0x10000) → the block at gp-8060 = 0x26e084; then via
resource table 0x26ed00 (16 B/entry: name,data,size,type — indices ==
osdbits res.h RESID exactly):

| res | name | goes to |
|---|---|---|
| 8  | SNDBOOTB | SPU2 RAM **0x5010** (VAB 1 body, chunked) |
| 15 | SNDOSDDB | SPU2 RAM **0x85010** (VAB 2 body, chunked) |
| 7  | SNDBOOTH | IOP buf +0x0000 (VAB 1 header) |
| 9  | SNDBOOTS | +0x1000 |
| 10 | SNDTNNLS | +0x2000 |
| 11 | SNDCLOKS | +0x3000 |
| 12 | SNDTM30S | +0x4000 |
| 13 | SNDTM60S | +0x5000 |
| 16 | SNDLOGOS | +0x7000 |
| 17 | SNDWARNS | +0x8000 |
| 18 | SNDRCLKS | +0x9000 |
| 14 | SNDOSDDH | +0x6000 (VAB 2 header) |

Then 0x2004b8 continues (values in decimal are the RPC fnos):
SpuSetCore(2: 1); StSetReverbType(20492: core 0, 4) and (1, 4);
vab1 = StVabOpenFakeBody(20486: iopBuf, 0x5010) → stored 0x26e09c
(returns 0); vab2 = StVabOpenFakeBody(iopBuf+0x6000, 0x85010) →
0x26e09e (returns 1); StSetTickMode(20490: 60); eight StBgmOpen(20489:
vab1, iopBuf+off) in the table order above → records 0..7, stored
0x26e08c..0x26e09a; per-record
StSetBgmVol(20499): **66, 42, 45, 27, 27, 27, 54, 54**;
StSetSeVol(20993: vab2, 28); StSetMasterVol(20498: core, 0x3fff,
0x3fff) both cores; SetTimer(20736) — starts the IOP-side sequencer
tick. (Shutdown suite at 0x2007c0: StBgmStop(slot,·,1) ×8, StBgmClose
(20491) ×8, StVabClose(20488) ×2, ReleaseTimer(20737) — not ported,
osdbits never exits.)

### Message vocabulary (fno = message; St* names from the ps2sdk
`#if 0` block, arg slots confirmed against the EE call sites)

    0x0001 SpuInit                  0x0002 SpuSetCore(n)
    0x0006 SpuSetReverb             0x000a SpuSetReverbModeDepth
    0x0020 SpuSetMute               0x1020/1021/1022 SpuAutoDMA write/stop/status
    0x1031 SpuSetDigitalOut         0x7128/0x7240/0x7314 SpuSet*Attr (struct)
    20481 0x5001 StInit             20486 0x5006 StVabOpenFakeBody(hdr,spuaddr)->vab
    20487 0x5007 StVabOpenCompleted 20488 0x5008 StVabClose(vab)
    20489 0x5009 StBgmOpen(vab,seq)->slot   20490 0x500a StSetTickMode(60)
    20491 0x500b StBgmClose(slot)   20492 0x500c StSetReverbType(core,4)
    20493 0x500d StSetReverbDepth(core,l,r) 20496 0x5010 StGetSlotStatus
    20498 0x5012 StSetMasterVol(core,l,r)   20499 0x5013 StSetBgmVol(slot,vol)
    20500 0x5014 StBgmPlay(slot)    20501 0x5015 StBgmStop(slot,?,fade)
    20506 0x501a StDmaWrite(iop,spu,n)      20507 0x501b StDmaRead
    20736 0x5100 SetTimer           20737 0x5101 ReleaseTimer
    20992 0x5200 StSePlay(1,n)      20993 0x5201 StSetSeVol(vab,vol)

### What the OSD actually plays (all call sites decoded)

**Slot and VAB ids are 0-BASED** — settled by disassembling the ROM
OSDSND module itself (extract/osdsnd.dis): StInit (0x1a46c) memsets the
whole 24 x 68-byte BGM record table at IOP 0x22210; StBgmOpen (0x1abf8)
checks the `SSsq` magic at seq+12, scans records from 0 and **returns
the first free index**; StBgmPlay (0x1b5bc) indexes `0x22210 + n*68`
directly with the message's argument. StSePlay (0x1be88) is
`(vab_id, se)` — the OSD's constant `StSePlay(1, n)` names VAB id 1 =
the SECOND opened VAB = SNDOSDD, so StVabOpenFakeBody allocates 0-based
too. The eight opens therefore land on records 0..7 in table order,
and every literal in the OSD's messages reads coherently:

- **0 = SNDBOOTS** — started by the disc/boot system logic (the
  `li a0,20500 / a1=0` pair at 0x211ff0/0x211ffc, next to the
  systemState logic opening.c already cites); the opening's
  `OSDDispatch(20501, 0, 0, 15/17)` is *stop record 0 with a fade* —
  fading the boot sound out, not a stop-all.
- **1 = SNDTNNLS** — the opening's boot transition:
  `OSDDispatch(20500, 1, 0, 0)` on a clear boot latch / default params.
- **2 = SNDCLOKS** — the System Configuration screen (the glass-clock
  carousel — the name fits the screen): menu plays/stops it at
  0x224494/0x224520/0x224608, stops with fades 14/15/0 at 0x21cb8c...
- 3/4 = SNDTM30S/TM60S (timers), 5 = SNDLOGOS,
- **6 = SNDWARNS** — stopped with fade 15 at opening end
  (`OSDDispatch2(1,20501,6,0,15)` — the one direct blocking call).
- **7 = SNDRCLKS** — played for boot params 108-110 (`20500,7` +
  stop-record-0 fade 17). Also `20501, 1` (stop TNNLS) at 0x21cdfc.

SEs — always `StSePlay(1, n)` = (VAB id 1 = SNDOSDD, SE n; the IOP
indexes the SE table by n*64), via per-module jump stubs to
OSDDispatch (0x21f978, 0x2287a8, 0x23fa68, …): n = 0..6 and
8..13 all occur. Known: **4 = confirm/click** (enter System Config,
0x2287a8 tail), 6 = cursor move (arrow-repeat sites), 10 = back/cancel,
others per-screen (9/11/12/13 in the clock/timer editors, 0..3 in the
0x227058 block, 5, 8). The port forwards them all; naming the rest is
listening work.

Not ported (documented for later): SpuSetDigitalOut(0/1) at
0x2078bc/0x20d7d0 (SPDIF config item); the SpuAutoDMA PCM-streaming
path in the CD-player screens (0x20c388-0x210670); the mute/common-attr
pause block at 0x201d40; the 0x2007c0 shutdown.

### SND* resource formats (sizes = expanded; bytes stay out of the repo)

All COMPSUBFILEs in the SNDIMAGE nested ROMDIR, standard "expand" LZ.
The "SS" (libsnd2 PS2) format family, parsed entirely IOP-side —
opaque blobs to the EE:
- Headers (`SShd` at +12): SNDBOOTH 1228 B, SNDOSDDH 1056 B. Word 1 =
  the body's size (0x56f10 / 0xd480 — matches exactly); the rest is the
  program/tone attribute table (ADSR, pitches, SPU offsets).
- Bodies (SPU2 ADPCM, leading silence block): SNDBOOTB 356112 B,
  SNDOSDDB 54400 B.
- Sequences (`SSsq` at +12): BOOTS 460, TNNLS 420, CLOKS 1612, TM30S/
  TM60S 304, LOGOS 304, WARNS 1616, RCLKS 420 B.

## The port (all in sound.diff)

- **sound.c** (new): the whole path above, 1:1 where it matters —
  `SndRpc` (the generic packet: 64 B out / 16 B back, word 0 = result),
  `OSDDispatch` ring + `SoundUpdate` drain + the reverb-ramp arithmetic
  verbatim, `SndLoadData`/`SndSpuUpload`/`SndStageToIop`
  (FlushCache + sceSifSetDma + DmaStat poll, chunked StDmaWrite +
  StVabOpenCompleted), `SoundInit` with the exact real message order,
  volumes, SPU addresses and IOP-buffer layout.
- Deliberate deviations, each commented in the file: rom0:OSDSND is
  loaded by hand through pad.c's loadfile RPC (bare-ELF boot has no
  OSDCNF); the bind retry is bounded (100) so a BIOS without OSDSND
  degrades to the previous printf-stub behaviour instead of hanging;
  the drain runs in its own prio-2 thread woken from the vblank handler
  (the real one lives in the periodic thread 0x206e00) so the blocking
  RPC never stalls a render thread; every transfer bounces through a
  16-byte-aligned staging buffer (the .inc arrays carry no alignment
  promise; the real code DMA'd straight out of its resource heap); the
  tail chunk is zero-padded, not stale-byte-padded; `qLock` is volatile
  (gcc otherwise deletes the set/clear pair as dead stores — verified
  in the disassembly, then fixed).
- **main.c**: printf stubs removed (OSDDispatch/2 now real, still
  printf-logging every message), `SoundInit()` after `InitPad()` (the
  real main inits sound early too), `SoundVblank()` in the vblank
  handler.
- **pad.c/inc.h**: romLoadModule/romLoadFatal/rpcDelay exported (shared
  loadfile RPC); sound declarations.
- **res.c**: the 12 SND includes + LoadResources wiring.
- **Makefile**: sound.o.

Nothing is stubbed at the RPC level — every message the OSD sends goes
out for real. What is *not* implemented: the 0x6xxx/0x7xxx struct
classes and NOWAIT mode (nothing ported uses them), and the not-ported
feature blocks listed above.

## Verification — done here vs deferred to aap

Done (emulator-free, per the rules — PCSX2 untouched):
- `make clean && make` clean, no warnings, from the current tree +
  regenerated .inc (ELF 1 805 972 B).
- `sound.diff` passes `git apply --check` against the real tree.
- Own-ELF disassembly (`ee-objdump -d main.elf`): `SndRpc` compiles to
  `sceSifCallRpc(&sndCd, msg, 0, pkt, 64, pkt, 16, 0, 0)` — the real
  0x261738 generic path's exact shape; `SndStageToIop` to the real
  0x2000c0 shape (stack {src,dst,size,0} → FlushCache → sceSifSetDma →
  DmaStat poll); `SoundInit` to romLoadModule → bind → StInit → load →
  SpuSetCore → the full message order; the qLock stores exist.
- Module-load path mirrors pad.c's proven romLoadModule pattern (same
  server, same fatal-code handling).

**Audible verification is deferred to aap on PCSX2/real hardware.**
Checklist for that run:
1. Console: `sound: rom0:OSDSND -> <id>` (>= 0), then the init
   dispatch2 log lines, then **`sound: up - vab 0/1, bgm 0 1 2 3 4 5 6
   7`** — the exact expected values (0-based first-free allocation,
   proven from the module's own disassembly, see above). Any -1 = an
   open failed (bad upload, VAB not open) and the OSD's literal
   record numbers will hit the wrong sequences. No translation layer
   exists or is needed — the port reproduces retail's opens in
   retail's order, so retail's literals hit the same records.
2. Opening: boot jingle when the boot message fires (log
   `dispatch(20500, 1, …)` already appears today); menu: click on
   entering System Configuration (`20992,1,4`), tunnel music
   (`20500,2`) inside it, reverb fading in over the first second.
3. PCSX2 needs a real BIOS (it has rom0:OSDSND; module loads like
   XPADMAN already do). On the TOOL, check whether ITS rom0 has OSDSND
   (`dsedb` run will print the loadfile result; the DTL boot ROM may
   differ) — if absent the port degrades cleanly and the next step is
   loading the module image from host0:.
4. If SPU state from a previous program pollutes (garbage notes on
   warm restarts): the real boot list runs CLEARSPU before OSDSND;
   `romLoadModule("rom0:CLEARSPU")` before OSDSND in SoundInit is the
   one-line fix (left out — SpuInit should cover a cold boot).
