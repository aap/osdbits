# Minimal-Viable Open PS2 Boot ROM — Survey & Verdict

Scope: what is actually inside a retail PS2 boot ROM, which parts a ROM whose *only*
job is "cold boot → run a retail game disc" must contain, and how much of that already
exists as open source.

Reference image: `scph39001.bin` (USA SCPH-39001, ROMVER `0160AC20020207` = v1.60,
region A = USA, C = CEX/retail, built 2002-02-07; `VERSTR` = "System ROM Version 5.0
02/07/02"), read-only from PCSX2's flatpak bios dir. Cross-checked against
`PS2 Bios 30004R V6 Pal.bin` (SCPH-30004R, `0160EC20011004`, PAL). Parser:
`romdir.py` in this dir; raw output in `inventory-usa.txt`, `inventory-pal.txt`,
`configs.txt`, `unknown-entries.txt`, `romrefs.txt`, `budget.txt`.

---

## 0. ROMDIR format (confirmed empirically)

ROMDIR table located by scanning 16-byte-aligned offsets for the triple
`"RESET"` / `"ROMDIR"` / `"EXTINFO"`. In this image it sits at file offset `0x2740`
(i.e. inside the `RESET` file, which spans `0x0000‥0x2740`+, because `RESET` is the
raw IOP/EE reset-vector blob and the directory is appended to it).

```c
struct romdir_entry {        /* 16 bytes */
    char  name[10];          /* NUL-padded; empty name terminates the table */
    u16   ext_info_size;     /* bytes consumed in the EXTINFO blob           */
    u32   file_size;         /* actual file size                            */
};
```
File data is laid out sequentially in table order; **each file's start is the running
sum of previous `file_size` values rounded up to 16** (`off += (size+15) & ~15`). Zero-size
entries (`-`, `EENULL` style padding markers) exist purely to force alignment/padding —
several entries named `-` in this image are pure padding to push the next real file to a
hardware-relevant offset (`RDRAM` @0x41000, `KROMG` @0x64000, `KROM` @0x66000,
`ROMGSCRT` @0x80000, `IOPBOOT` @0x4A000 are all forced to round addresses).

EXTINFO is a parallel byte stream, each entry consuming `ext_info_size` bytes of it,
made of records `{u16 value; u8 size; u8 type}` + `size` bytes payload:
type 1 = date string, 2 = version (value = BCD-ish `hi.lo`), 3 = description/comment,
0x7F = null/pad. That is where the build strings live, e.g.
`20020207-164243,ROMconf,PS20160AC20020207.bin,kuma@rom-server/~/g30k/g/app/rom`.

Nested ROMDIR images inside the ROM (found by re-scanning the whole file):
`EELOADCNF` @0x49190 (4 entries), `OSDCNF` @0x4B160 (4), `FNTIMAGE` @0x1CC5E0 (7),
`SNDIMAGE` @0x1E06F0 (15), `TEXIMAGE` @0x243D10 (46), `ICOIMAGE` @0x27DAD0 (16).
The first two are **IOPRP images** (RESET/ROMDIR/EXTINFO + a single `IOPBTCONF`);
the last four are OSDSYS resource archives (fonts / sounds / textures / icons), still
in ROMDIR format. There is **no second top-level ROMDIR** in either dump — i.e. no
rom1 (DVD player) in these 4 MB images; the ~300 KB tail after `KERNEL` is erased flash.

`ps2sdk/tools/romimg` already builds and unpacks these images.

---

## 1. Full inventory — SCPH-39001 v1.60 (92 entries, 3 883 407 B of 4 MiB)

Class: **A** = required to cold-boot a game disc, **B** = required for game/era
compatibility (games or the OSD load these by name, or need the newer X-variant),
**C** = droppable from a minimal ROM. "Side" = which CPU runs it.

| # | Name | Size | Side | Purpose | Class |
|---|------|-----:|------|---------|:-----:|
| 0 | RESET | 10048 | IOP+EE | Reset-vector blob at `0xBFC00000`. Contains both CPUs' bootstrap; branches on `COP0.PRid >= 0x59` (EE) vs else (IOP). Strings: "Sony Computer Entertainment Inc.", "PS compatible mode by M.T.", "KERNEL", "IOPBOOT". | A |
| 1 | ROMDIR | 1488 | data | The directory table itself | A |
| 2 | EXTINFO | 1920 | data | Extended-info blob | A |
| 3 | ROMVER | 16 | data | `0160AC20020207` — **fed to `sceCdBootCertify()`** | A |
| 4 | SBIN | 28576 | IOP | PS1-compat kernel image (COFF-ish, "PS-X Control PAD Driver Ver 3.0" strings) | C |
| 5 | LOGO | 83604 | IOP | PS1-mode boot logo/shell payload (MIPS code, `lui a0,0x8003`) | C |
| 6 | IOPBTCONF | 234 | data | Final-phase IOP module list (see §2) | A |
| 7 | IOPBTCON2 | 195 | data | First-phase (pre-UDNL) IOP module list | A |
| 8 | SYSMEM | 4625 | IOP | `System_Memory_Manager` v1.01 | A |
| 9 | LOADCORE | 9597 | IOP | `Module_Manager` v1.01 — IRX loader/linker core | A |
| 10 | EXCEPMAN | 3033 | IOP | `Exception_Manager` v1.01 | A |
| 11 | INTRMANP | 6657 | IOP | `Interrupt_Manager` v1.01, **PS1-mode** variant (no-resident-exits in IOP mode) | A* |
| 12 | INTRMANI | 7729 | IOP | `Interrupt_Manager` v1.01, IOP-mode variant | A |
| 13 | SSBUSC | 1897 | IOP | `ssbus_service` v1.01 — sub-bus controller | A |
| 14 | TIMEMANP | 3033 | IOP | `Timer_Manager` v1.01, PS1-mode | A* |
| 15 | TIMEMANI | 3113 | IOP | `Timer_Manager` v1.01, IOP-mode | A |
| 16 | DMACMAN | 14069 | IOP | `dmacman` v1.01 | A |
| 17 | SYSCLIB | 10077 | IOP | `System_C_lib` v1.01 (memcpy/strcmp/sprintf for IRXs) | A |
| 18 | HEAPLIB | 3313 | IOP | `Heap_lib` v1.01 | A |
| 19 | THREADMAN | 36225 | IOP | `Multi_Thread_Manager` v1.01 — thbase/thsemap/thevent/thmsgbx/thfpool/thvpool | A |
| 20 | VBLANK | 3465 | IOP | `Vblank_service` v1.01 | A |
| 21 | IOMAN | 8041 | IOP | `IO/File_Manager` v1.02 | A |
| 22 | MODLOAD | 9025 | IOP | `Moldule_File_loader` v1.01 (sic) | A |
| 23 | ROMDRV | 3817 | IOP | `ROM_file_driver` v1.03 — serves `rom0:` | A |
| 24 | ADDDRV | 1113 | IOP | Non-resident helper: registers rom1: via ROMDRV | B |
| 25 | STDIO | 3049 | IOP | `Stdio` v1.01 | A |
| 26 | SIFMAN | 5529 | IOP | `IOP_SIF_manager` v1.01 | A |
| 27 | SIFINIT | 1041 | IOP | Non-resident: brings up SIF (skips if DECI1) | A |
| 28 | EESYNC | 1177 | IOP | `SyncEE` v1.01 — final IOP↔EE reset handshake | A |
| 29 | EENULL | 64 | EE | EE idle thread, copied to `0x00081FC0` | A |
| 31 | RDRAM | 12308 | EE | EE-side RDRAM sizing/init + TLB setup ("Initialize memory (rev:%d.%02d, ctm:%dMhz, cpuclk:%dMhz)") | A |
| 32 | SIFCMD | 8753 | IOP | `IOP_SIF_rpc_interface` v1.01 | A |
| 33 | REBOOT | 1985 | IOP | `RebootByEE` v1.01 — SIF cmd `0x80000003` reboot server | A |
| 34 | LOADFILE | 10065 | IOP | `LoadModuleByEE` v1.01 — the LOADFILE RPC (`SifLoadModule`, `SifLoadElf`) | A |
| 35 | EELOADCNF | 412 | data | IOPRP image containing the IOPBTCONF used for the *EELOAD* reboot | A |
| 36 | TZLIST | 717 | data | Timezone table (compressed) for OSD clock | B |
| 37 | RMRESET | 2089 | IOP | `rmreset` v1.01 — IR remote reset (only useful on models with IR) | C |
| 39 | IOPBOOT | 4448 | IOP | IOP bootstrap: parses `IOPBTCONF` (`"!addr "`, `"IOPBTCONF"` strings), loads SYSMEM+LOADCORE | A |
| 40 | OSDCNF | 487 | data | IOPRP image with OSDSYS's IOPBTCONF (adds X-modules + OSDSND) | A |
| 42 | TBIN | 57200 | IOP | PS1 BIOS/shell image ("BOOT =", "No EXE-file !", "GPU_sync"), started by RESET in PS1 mode | C |
| 43 | XLOADFILE | 11161 | IOP | `LoadModuleByEE` v2.01 | B |
| 44 | SECRMAN | 17633 | IOP | `secrman_for_cex` v1.03 — MagicGate auth for memory cards / SIO2 security | B |
| 45 | SIO2MAN | 7205 | IOP | `sio2man` v1.01 | B |
| 46 | EECONF | 3905 | IOP | `EEConfig` v1.01 — reads OSD config out of the MechaCon EEPROM at IOP boot | A |
| 48 | KROMG | 7351 | data | Small kanji glyph set | C |
| 50 | KROM | 106096 | data | Kanji ROM font | C |
| 52 | VERSTR | 94 | data | Human-readable version banner | C |
| 54 | ROMGSCRT | 14488 | data | GS CRT/video-mode table blob (**USA-only in this pair**; absent from the PAL image and from SCPH-10000/15000) | C |
| 55 | MCMAN | 62605 | IOP | `mcman` v1.01 | B |
| 56 | MCSERV | 7413 | IOP | `mcserv` v1.01 — MC RPC server | B |
| 57 | PADMAN | 38733 | IOP | `padman` v1.20 | B |
| 58 | CDVDMAN | 33709 | IOP | `cdvd_driver` v1.04 — the CD/DVD driver | A |
| 59 | CDVDFSV | 33717 | IOP | `cdvd_ee_driver` v1.04 — CDVD RPC server for the EE | A |
| 60 | FILEIO | 8437 | IOP | `FILEIO_service` v1.01 — IOMAN RPC server | A |
| 61 | CLEARSPU | 7229 | IOP | Non-resident SPU reset | B |
| 62 | UDNL | 7925 | IOP | The **IOPRP merge loader** — parses image list, picks newest modules, re-resets | A |
| 63 | IGREETING | 4185 | IOP | Prints the boot banner ("PlayStation 2 ======== Hard reset boot", CPUID/ROMGEN) | C |
| 64 | EELOAD | 61968 | EE | The EE ELF loader. Loads `rom0:OSDSYS`, handles `BootBrowser`/`BootError`/`BootIllegal`, does `rom0:UDNL rom0:EELOADCNF` IOP reboots, `moduleload`/`moduleload2` | A |
| 65 | TESTMODE | 122800 | EE | Service/test mode program | C |
| 66 | TESTSPU | 26943 | IOP | SPU test helper | C |
| 67 | LIBSD | 25357 | IOP | `Sound_Device_Library` v1.04 | B |
| 68 | TSIO2MAN | 8317 | IOP | `sio2man` v2.01 for TESTMODE | C |
| 69 | TPADMAN | 40989 | IOP | `padman` v2.03 for TESTMODE | C |
| 70 | PS1DRV | 118456 | EE | PS1 driver: sets up EE GPU emulation, resets IOP into PS1 mode | C |
| 71 | FONTM | 738012 | data | The large font module (OSDSYS; a few games open `rom0:FONTM`) | C |
| 72 | FNTIMAGE | 82192 | data | OSDSYS font resource archive (FNTASCII/FNTEX000/FNTEXOSD) | C |
| 73 | SNDIMAGE | 407060 | data | OSDSYS sound archive (SNDBOOTH/SNDTNNLS/SNDCLOKS/SNDLOGOS…) | C |
| 74 | TEXIMAGE | 236980 | data | OSDSYS texture archive (TEXOPNG*/TEXOWAL0/TEXOFOG*/TEXOBLP…) — aap's project already decodes these | C |
| 75 | ICOIMAGE | 71821 | data | OSDSYS icon archive (ICOBDISC/ICOBPS2D/ICOBDVDD…) | C |
| 76 | XSIFCMD | 9449 | IOP | `IOP_SIF_rpc_interface` v2.04 | B |
| 77 | XCDVDMAN | 54181 | IOP | `cdvd_driver` v2.11 | B |
| 78 | XCDVDFSV | 53253 | IOP | `cdvd_ee_driver` v2.11 | B |
| 79 | XFILEIO | 9085 | IOP | `FILEIO_service` v2.03 | B |
| 80 | XSIO2MAN | 8317 | IOP | `sio2man` v2.01 | B |
| 81 | XMTAPMAN | 9909 | IOP | `multitap_manager` v2.02 | B |
| 82 | XMCMAN | 81765 | IOP | `mcman_cex` v2.11 | B |
| 83 | XMCSERV | 5865 | IOP | `mcserv` v2.08 | B |
| 84 | XPADMAN | 45453 | IOP | `padman` v3.06 | B |
| 85 | ATAD | 13781 | IOP | `atad` v1.01 — DEV9/ATA | C |
| 86 | HDDLOAD | 4281 | IOP | `HDD_Loader` v1.01 | C |
| 87 | OSDSND | 173981 | IOP | `rspu2_driver` v1.03 — OSD sound driver | C |
| 88 | PS2LOGO | 216260 | EE | Reads + decrypts + displays the disc's PS2 logo, then `LoadExecPS2()`s the game. References `rom0:ROMVER`, `rom0:OSDSND` | A† |
| 89 | HDDOSD | 107648 | EE | HDD Browser stub | C |
| 90 | OSDSYS | 313668 | EE | The browser/menu. **Self-compressed** (its ELF has `.text.Expand`/`.text.ExpandMain`/`.text.ExpandSetBlock`, so most strings are packed). Visible: `ExecutePs2GameDisk`, `cdrom0:\SYSTEM.CNF;`, `BOOT2`, `BootBrowser`, `DVDVIDE`, `player.elf` | A |
| 91 | KERNEL | 93256 | EE | **The EE kernel** (raw binary, copied to `0x80000000`, entry `0x80001000`). Syscall table, INTC/DMAC dispatch, threads/semaphores/alarms, TLB, `ExecPS2`/`LoadExecPS2`, GS/SIF syscalls, DECI2 manager, kernel TTY | A |

\* INTRMANP/TIMEMANP are the PS1-mode twins; they are listed in `IOPBTCONF` alongside the
I-variants and exit non-resident in the wrong mode. A minimal ROM with no PS1 support can
ship only the I-variants (rename them, since `IOPBTCONF` names, not module names, drive loading).

† PS2LOGO is not *technically* required — you can `LoadExecPS2()` the game ELF directly —
but shipping something at `rom0:PS2LOGO` is cheap and preserves expected behaviour.

**USA vs PAL diff (v1.60 both):** only `ROMGSCRT` is USA-only. Everything else is
name-identical; sizes differ slightly for `XLOADFILE`, `XCDVDMAN`, `EELOAD`, `TESTMODE`,
`PS1DRV`, `KERNEL`, and the file *order* differs (MCMAN/PADMAN/EECONF are placed
differently). So per-region divergence at a fixed ROM version is small — the divergence
that matters is **across ROM versions/models**, not across regions.

---

## 2. Boot chain, precisely

### 2.1 Reset

Both CPUs fetch from `0xBFC00000` (`RESET`). `RESET` reads `COP0.PRid` (reg 15):
`PRid >= 0x59` ⇒ EE path, else IOP path. The IOP is the one that actually owns the ROM
bus master role early; the EE spins on SIF until the IOP is up.

### 2.2 IOP path (all IOP code)

1. `RESET` (IOP half) initialises the SSBUS/hardware registers, then loads **`IOPBOOT`**
   from ROMDIR into IOP RAM.
2. `IOPBOOT` parses **`IOPBTCON2`** (first phase) — it understands `@800` (load address),
   `!addr`, and `!include` directives — locates and starts **`SYSMEM`**, then **`LOADCORE`**.
3. `LOADCORE` walks the rest of the list, IRX-loading and linking each module by resolving
   its `.iopmod` import stubs (`0x41E00000` / `0x41C00000` magic) against the export tables
   of already-resident modules.

   `IOPBTCON2` (phase 1, 24 modules) — verbatim from the ROM:
   ```
   SYSMEM LOADCORE EXCEPMAN INTRMANP INTRMANI SSBUSC DMACMAN TIMEMANP TIMEMANI
   SYSCLIB HEAPLIB THREADMAN VBLANK IOMAN MODLOAD ROMDRV ADDDRV STDIO SIFMAN
   IGREETING CDVDMAN SECRMAN SIO2MAN MCMAN
   ```
4. **`UDNL`** re-resets with a merged module list when an IOPRP image is supplied
   (see §2.5). At cold boot the final list is **`IOPBTCONF`** (29 modules):
   ```
   SYSMEM LOADCORE EXCEPMAN INTRMANP INTRMANI SSBUSC DMACMAN TIMEMANP TIMEMANI
   SYSCLIB HEAPLIB EECONF THREADMAN VBLANK IOMAN MODLOAD ROMDRV STDIO SIFMAN
   IGREETING SIFCMD REBOOT LOADFILE CDVDMAN CDVDFSV SIFINIT FILEIO SECRMAN EESYNC
   ```
5. `SIFINIT` brings up SIF; `EESYNC` performs the final handshake with the EE;
   `REBOOT` installs the SIF-cmd `0x80000003` handler and the IOP drops into its
   command loop.

### 2.3 EE path (all EE code)

1. `RESET` (EE half) measures the CPU clock, then runs **`RDRAM`** — RDRAM controller
   init/sizing, "Total accessable memory size: %d MB", basic TLB setup.
2. The **`KERNEL`** file is copied verbatim to `0x80000000`; the boot code jumps to
   `0x80001000`. The kernel installs exception vectors, the syscall table (at
   `0x80011F80` in most versions), INTC/DMAC handler dispatch, initialises
   DMAC/VU0/VU1/VIF0/VIF1/GIF/GS/IPU/INTC/TIMER/FPU/scratchpad (its own progress
   strings enumerate exactly this), copies **`EENULL`** to `0x00081FC0` as the idle thread,
   and brings up SIF DMA against the IOP.
3. Kernel loads **`EELOAD`** and enters it. `EELOAD` is the ELF loader used by
   `LoadExecPS2()`; it also owns the IOP-reboot argv convention.
4. `EELOAD` boots **`rom0:OSDSYS`** (or `rom0:TESTMODE` for service mode; the argv
   `BootBrowser` selects the browser path).

### 2.4 OSDSYS → game

`OSDSYS` reboots the IOP with `rom0:UDNL rom0:OSDCNF`, adding the X-modules and OSD
sound: `XSIFCMD XLOADFILE XCDVDMAN XCDVDFSV XFILEIO RMRESET CLEARSPU XSIO2MAN XMTAPMAN
XMCMAN XMCSERV XPADMAN OSDSND` (full OSDCNF list in `configs.txt`). Then, on "boot disc":

1. `sceCdInit()` / `sceCdDiskReady()` / `sceCdGetDiskType()`.
2. **Boot certification**: read `rom0:ROMVER`, convert `"0160AC"` → 4 bytes
   `{16, 60, 'A', 'C'}` and issue `sceCdBootCertify()` = MechaCon **S-command 0x1A**.
   This is what unlocks the drive. B-chassis and later require it; SCPH-10000/15000
   don't support it (ignore the return value).
   Reference implementation: `OSD-Initialization-Libraries/osd/common/OSDInit.c`.
3. Open `cdrom0:\SYSTEM.CNF;1`, parse the `BOOT2 = cdrom0:\SLUS_xxx.xx;1` token.
   (`OSDSYS` actually gets it via `sceCdReadKey`; parsing SYSTEM.CNF is the documented
   equivalent — see `osdinit-ps2.c` lines 236‑310 in this dir.)
4. `LoadExecPS2("rom0:PS2LOGO", 1, {boot path})`.
5. `PS2LOGO` reads the first 16 sectors (the logo), gets the disc key with
   `sceCdReadKey(0,0,0x004B,…)` / `sceCdReadKey(0,0,0x0C03,…)`, decrypts the logo
   (ROL5 + XOR with key byte 5), draws it, then `LoadExecPS2(boot path)`.
6. `LoadExecPS2` = kernel resets EE state, re-loads `EELOAD`, which `SifLoadElf`s the
   game's ELF over LOADFILE RPC and jumps to it.

### 2.5 The IOPRP mechanism (critical, easy to miss)

`SifIopReset(argv, flags)` sends SIF command `0x80000003` with an argv string of the form

```
rom0:UDNL <last image>..<first image> [-v]
```

e.g. `rom0:UDNL cdrom0:\MODULES\IOPRP243.IMG;1`. `UDNL` scans the supplied ROMDIR
images, and for every name in the final `IOPBTCONF` it picks the **newest** version found
across (images..., ROM). Retail games do this constantly with the `IOPRPxxx.IMG` shipped
on the disc (`xxx` = SDK version), which is how a 2005 game gets a 2005 CDVDMAN on a 2001
console. Consequence for a replacement ROM:

- `UDNL` must implement the version-compare/merge correctly.
- Any module a game's IOPRP does **not** contain will come from your ROM, so those ROM
  copies must be behaviourally correct, not stubs.
- Your ROM's `IOPBTCONF` name set matters: names present in the image but not in your
  `IOPBTCONF` are ignored; names in `IOPBTCONF` that you don't ship and the image doesn't
  provide will fail the boot.

---

## 3. The rom0: runtime service contract

This is the requirement most "just write a bootloader" plans forget. Retail titles open
ROM paths **by name at runtime**, both as IRX loads through LOADFILE RPC and as plain
file opens. Empirically observed in the ROM itself (`romrefs.txt`) plus well-known
practice:

**Compatibility-critical (games load these directly):**

| Path | Served by | Notes |
|------|-----------|-------|
| `rom0:SIO2MAN` | sio2man v1.01 | old games; **v1.00 exports differ** — aap's own `pad.c` documents that linking against rom0:SIO2MAN fails and XSIO2MAN is needed |
| `rom0:XSIO2MAN` | sio2man v2.01 | newer games / OSD |
| `rom0:PADMAN` / `rom0:XPADMAN` | padman v1.20 / v3.06 | X-variants have a *different* RPC/export ABI; must pair X with X |
| `rom0:MCMAN` / `rom0:XMCMAN` | mcman v1.01 / mcman_cex v2.11 | |
| `rom0:MCSERV` / `rom0:XMCSERV` | mcserv v1.01 / v2.08 | |
| `rom0:XMTAPMAN` | multitap_manager v2.02 | multitap games |
| `rom0:LIBSD` | Sound_Device_Library v1.04 | SPU2 library; many games use the disc copy but not all |
| `rom0:SECRMAN` | secrman_for_cex v1.03 | MagicGate; needed by MCMAN for MG-protected saves |
| `rom0:CDVDMAN`/`XCDVDMAN`, `CDVDFSV`/`XCDVDFSV`, `FILEIO`/`XFILEIO`, `SIFCMD`/`XSIFCMD`, `LOADFILE`/`XLOADFILE` | resident set | usually supplied *by IOPRP replacement*, but the ROM copy is the fallback |
| `rom0:ROMVER` | data | read by tons of software (PS2LOGO, TESTMODE, homebrew, some games) for model detection |
| `rom0:FONTM` | data | the ROM font — a small number of titles and much homebrew use it |
| `rom0:UDNL`, `rom0:EELOADCNF`, `rom0:OSDCNF` | reboot machinery | `rom0:UDNL` appears in every reboot argv |
| `rom0:ADDDRV` | mounts rom1: | harmless to keep even with no rom1 |

Sony's own guidance (echoed by ysai187/sp193 in OSD-Initialization-Libraries) is that
software *shouldn't* use board-specific T\*/X\*/P\* modules — but plenty of retail code
does anyway, so a replacement ROM has to serve both the plain and the X names, with the
correct, distinct ABIs. **X-modules are not drop-in newer versions with the same ABI:
"the X\* modules are just newer versions, with mostly broken compatibility"** — if a
program loads XSIO2MAN it must also get XPADMAN/XMCMAN etc.

---

## 4. Classification & size budget

Measured over the 92 entries (`budget.txt`):

| Class | Bytes | Share |
|-------|------:|------:|
| **A** — required for cold boot of a game disc | 986 711 (964 KiB) | 25 % |
| **B** — required for game/era compatibility | 439 786 (430 KiB) | 11 % |
| **C** — droppable for a minimal ROM | 2 456 910 (2 399 KiB) | 63 % |

So **~1.4 MB of the 3.9 MB is load-bearing** for the stated goal. The entire dropped
2.4 MB is: PS1 compatibility (`SBIN`+`LOGO`+`TBIN`+`PS1DRV` = 288 KB), OSD presentation
assets (`FONTM`+`FNTIMAGE`+`SNDIMAGE`+`TEXIMAGE`+`ICOIMAGE`+`OSDSND` = 1 610 KB — 41 %
of the whole ROM is OSD chrome and fonts!), service mode (`TESTMODE`+`TESTSPU`+
`TSIO2MAN`+`TPADMAN` = 199 KB), HDD (`ATAD`+`HDDLOAD`+`HDDOSD` = 126 KB), kanji ROM
(`KROM`+`KROMG` = 111 KB).

This matches what the practical custom-ROM scene does: BitBuilt-documented PS2BBL
custom ROMs strip fonts, OSD textures and OSD sounds to make room for OPL/uLE inside
the 4 MB.

Notes on borderline calls:
- `EECONF` is class A: it reads OSD/video config out of the MechaCon EEPROM during IOP
  boot, and the EE kernel's `GetOsdConfigParam` depends on it. Without it you must
  hard-code video mode and language.
- `TZLIST` is B, not A: only the clock UI needs it, but `EECONF`/OSD config paths touch it.
- `SECRMAN` is B even for a boot-only ROM: `MCMAN` imports it, so if you serve
  `rom0:MCMAN` you must serve `rom0:SECRMAN`.
- `IGREETING`, `VERSTR`, `ROMGSCRT`, `RMRESET`, `CLEARSPU` are cosmetic/board-specific;
  `CLEARSPU` is documented as crash-prone on early models anyway.
- **What aap's OSDSYS reimplementation supplies**: class-A entry `OSDSYS` (313 KB) plus,
  optionally, the whole class-C resource block (`TEXIMAGE`/`ICOIMAGE`/`SNDIMAGE`/
  `FNTIMAGE`) which his project already decodes and rebuilds. For a *minimal* ROM the
  OSDSYS slot only needs the §2.4 sequence (boot certify → disc type → SYSTEM.CNF →
  LoadExecPS2); his full towers/menu implementation is the "nice" version of the same slot.

---

## 5. Open-source coverage matrix

### 5.1 IOP side — essentially solved

`ps2sdk` (`iop/`, checked out at `/u/aap/othersrc/ps2sdk`, HEAD `f08e889f`) and its
upstream staging repo **`uyjulian/ps2iop`** (MIT, last push 2026-07-28) contain clean
reimplementations of *the entire ROM IOP module set*, with matching module names,
matching export tables, and versions tracking the **late** ROM/SDK revisions:

| ROM entry | ps2sdk path | Their IRX_ID | ROM v1.60 version | LoC |
|---|---|---|---|---|
| SYSMEM | `iop/system/sysmem` | System_Memory_Manager 2.3 | 1.01 | 760 |
| LOADCORE | `iop/system/loadcore` | Module_Manager 2.6 | 1.01 | 1586 |
| EXCEPMAN | `iop/system/excepman` | Exception_Manager 1.1 | 1.01 | — |
| INTRMANI/P | `iop/system/intrman`, `intrmanp` | Interrupt_Manager 1.1 | 1.01 | 1314 |
| SSBUSC | `iop/system/ssbusc` | ssbus_service 1.1 | 1.01 | — |
| TIMEMANI/P | `iop/system/timrman`, `timrmanp` | Timer_Manager 2.2 | 1.01 | — |
| DMACMAN | `iop/system/dmacman` | dmacman 1.1 | 1.01 | — |
| SYSCLIB | `iop/system/sysclib` (+`-full`,`-nano`) | — 1.1 | 1.01 | — |
| HEAPLIB | `iop/system/heaplib` | Heap_lib 1.1 | 1.01 | — |
| THREADMAN | `iop/system/threadman` | Multi_Thread_Manager 2.3 | 1.01 | 3882 |
| VBLANK | `iop/system/vblank` | Vblank_service 1.1 | 1.01 | — |
| IOMAN | `iop/system/ioman` (+`iomanx`) | IO/File_Manager 1.1 / IOX 2.3 | 1.02 | 616 |
| MODLOAD | `iop/system/modload` | Moldule_File_loader 2.9 | 1.01 | 2214 |
| ROMDRV | `iop/fs/romdrv` | ROM_file_driver 2.1 | 1.03 | 384 |
| ADDDRV | `iop/fs/adddrv` (+`addrom2`) | — | — | — |
| STDIO | `iop/system/stdio` | Stdio 1.1 | 1.01 | — |
| SIFMAN | `iop/system/sifman` | IOP_SIF_manager 2.5 | 1.01 | 643 |
| SIFCMD | `iop/system/sifcmd` | IOP_SIF_rpc_interface 2.8 | 1.01 / X 2.04 | 1197 |
| SIFINIT | `iop/system/sifinit` | SifInit 1.1 | — | — |
| EESYNC | `iop/system/eesync` (+`-nano`) | — | 1.01 | — |
| EECONF | `iop/system/eeconf` | EEConfig 1.1 | 1.01 | — |
| REBOOT | `iop/system/reboot` | RebootByEE 1.1 | 1.01 | — |
| LOADFILE | `iop/system/loadfile` | LoadModuleByEE 2.2 | 1.01 / X 2.01 | 1001 |
| RMRESET | `iop/system/rmreset` | rmreset 1.1 | 1.01 | — |
| UDNL | `iop/system/udnl` (+`udnl-t300`) | 1.1 | — | 1522 |
| IGREETING | `iop/system/igreeting` (+`-dtlt`) | — | — | — |
| CLEARSPU | `iop/sound/clearspu` | — | — | — |
| CDVDMAN | `iop/cdvd/cdvdman` | cdvd_driver 2.38 | 1.04 / X 2.11 | 8863 |
| CDVDFSV | `iop/cdvd/cdvdfsv` | cdvd_ee_driver 2.38 | 1.04 / X 2.11 | 3629 |
| FILEIO | `iop/fs/fileio` (+`filexio`) | FILEIO_service 1.1 | 1.01 / X 2.03 | 650 |
| SIO2MAN | `iop/sio/sio2man` (+`-nano`) | sio2man 3.17 | 1.01 / X 2.01 | 696 |
| PADMAN | `iop/input/padman` (+`-1300`,`-2000`) | padman 3.6 | 1.20 / X 3.06 | 5293 |
| MTAPMAN | `iop/sio/mtapman` (+`-1400`) | multitap_manager 2.2 | X 2.02 | 804 |
| MCMAN | `iop/memorycard/mcman` (+`-1300`,`-2000`) | 2.11 | 1.01 / X 2.11 | 8986 |
| MCSERV | `iop/memorycard/mcserv` (+`-1300`) | 2.8 | 1.01 / X 2.08 | 1204 |
| SECRMAN | `iop/security/secrman` (+`_arcade`) | 1.4 | 1.03 | 2293 |
| LIBSD | `iop/sound/libsd` (freesd) | 1.1 | 1.04 | 1857 |
| OSDSND | `iop/sound/rspu2drv` | rspu2_driver 1.3 | 1.03 | 1011 |
| ATAD | `iop/dev9/atad`, `iop/dev9/dev9` | atad | 1.01 | — |

Provenance comments in the source are explicit and reassuring:
`cdvdman.c` — "Based on the module from SCE SDK 3.1.0."; `sio2man.c` — same;
`freepad.c` — "currently is based on the last XPADMAN from BOOTROM: 0x03,0x06".
These are behaviour-matched reimplementations with real export tables
(`exports.tab` present for every module that exports anything), i.e. genuinely
drop-in for the LOADCORE stub-linker.

Also present and useful: `iop/deckard/xparam` (Deckard slim support), `iop/arcade/*`
(System 246/256), `iop/dvrp/*` (PSX DVR), and `tools/romimg` — a ROMDIR image
builder/extractor, so the packaging step is covered too.

### 5.2 EE side — the real gap

| Piece | Open status |
|---|---|
| **EE kernel** (`KERNEL`, 93 KB) | **No open reimplementation exists.** ps2sdk's `ee/kernel` is *libkernel* (syscall wrappers) plus a set of *kernel patch* modules (`ee/kernel/src/{eenull,osdsrc,tlbsrc,srcfile}`, ~750 LoC total) that get injected into the ROM kernel via the `Setup`/`Copy` syscalls (0x74/0x5A) to fix/extend `ExecPS2`, `SetOsdConfigParam(2)`, TLB and alarm functions. `jimmikaelkael/ps2-protokernel-patch` is the same idea for SCPH-10000/15000. Useful as partial code and as a *spec*, but nobody has written a standalone EE kernel. The closest full specifications are the ps2tek EE-syscall table and the HLE kernels in emulators (Play!'s `CPS2OS`, DobieStation), which are open source but host-side C++, not MIPS. |
| **EELOAD** | `OSD-Initialization-Libraries/kpatch/*/EELOAD/` contains *replacement EELOAD images* (`EELOAD.img` + source) used by the HDD-browser kernel-patch path — real, small, GPL-3.0, but scoped to patching, not a from-scratch loader. `fps2bios/kernel/eeload` is an incomplete attempt. |
| **RESET / IOPBOOT / RDRAM init** | `fps2bios` (`kernel/eestart.c`, `iopstart.c`, `romdir.c`, `kernel/iopload/`) — 3 commits, "does not work as a full replacement", no license. The Rust-console book *Writing a PlayStation 2 BIOS in Rust* documents EE boot, RDRAM init and IRX loading in detail but is explicitly WIP and emulator-targeted. ps2tek documents the EE boot path and RDRAM init registers. **Must-write.** |
| **OSDSYS** | **aap's project.** Nothing else open exists; the practical scene replaces it with PS2BBL/uLE/OPL (which are boot managers, not OSDSYS reimplementations, but do fill the same ROM slot). |
| **PS2LOGO** | `mlafeldt/ps2logo` documents the scheme; trivially re-writable (or omit). |
| **OSD init sequence** | `ps2homebrew/OSD-Initialization-Libraries` (`libosdinit`, GPL-3.0, ysai187 + sp193) — reboot-IOP, boot-certify, OSD config from EEPROM, GCONT/video setup, PS1 and PS2 disc boot. This is effectively the open spec for §2.4 and is directly reusable. |
| **PS1DRV / PS1 mode** | Nothing open. Out of scope for a minimal ROM. |

### 5.3 Prior open-BIOS attempts — what's real

- **`fps2bios`** (AKuHAK's fork of the old ps2dev project): the only project that ever
  aimed at a whole replacement boot ROM. Directories `kernel/{eeload,iopload}`, `loader`,
  `intro`, `doc`. README: *"an open source (but 'unfree' because of no licenses) and
  incomplete implementation of the PS2's boot ROM… currently, does not work as a full
  replacement."* Effectively abandoned; useful as documentation, unusable as a base
  (no license).
- **`uyjulian/ps2iop`** + **ps2sdk `iop/`**: real, maintained, MIT/AFL-2.0, complete for
  the IOP module set. This is the strongest existing asset.
- **`ps2homebrew/OSD-Initialization-Libraries`**: real, maintained, GPL-3.0, covers the
  cold-boot init sequence.
- **Rust-console `ps2-bios-book`**: real documentation effort (CC-BY-SA text, GPLv3+
  code), WIP, no working ROM.
- **PS2BBL-based custom ROMs** (israpps' PlayStation2-Basic-BootLoader, BitBuilt
  threads): real and shipping — but these are *modified retail ROMs*: they keep Sony's
  RESET/KERNEL/EELOAD/IOP modules and only replace the OSDSYS slot and strip assets.
  Proof that the ROM slot mechanics work; not a clean-room replacement.
- **PCSX2 project statement**: no open BIOS alternative exists.

---

## 6. Verdict

### 6.1 Minimal-viable ROM parts list

| # | Part | Side | Status | Effort |
|---|------|------|--------|--------|
| 1 | `RESET` — dual-CPU reset vector, PRid dispatch | both | **must-write** (fps2bios/ps2tek/Rust-book as docs) | M |
| 2 | `RDRAM` — RDRAM controller init + sizing | EE | **must-write** (ps2tek `RDRAM_Init` documents registers) | M |
| 3 | `KERNEL` — EE kernel: exceptions, syscall table, INTC/DMAC, threads/sema/alarm, TLB, ExecPS2/LoadExecPS2, GS/SIF syscalls | EE | **must-write** — *the single biggest item* | **XL** |
| 4 | `EELOAD` — ELF loader + IOP-reboot argv handling | EE | **must-write** (partial refs exist) | M |
| 5 | `EENULL` — idle thread | EE | must-write (trivial; ps2sdk has `eenull.s`) | XS |
| 6 | `IOPBOOT` + `IOPBTCONF`/`IOPBTCON2` | IOP | **must-write** (small: config parser + two loads) | S |
| 7 | IOP kernel set: SYSMEM, LOADCORE, EXCEPMAN, INTRMAN, SSBUSC, TIMEMAN, DMACMAN, SYSCLIB, HEAPLIB, THREADMAN, VBLANK, IOMAN, MODLOAD, ROMDRV, STDIO | IOP | **exists-open** (ps2sdk / ps2iop) | S (integration) |
| 8 | SIF set: SIFMAN, SIFCMD/XSIFCMD, SIFINIT, EESYNC, REBOOT, LOADFILE/XLOADFILE, FILEIO/XFILEIO | IOP | **exists-open** | S |
| 9 | UDNL + IOPRP merge | IOP | **exists-open** (`iop/system/udnl`) — verify version-compare semantics | S–M |
| 10 | CDVDMAN/XCDVDMAN + CDVDFSV/XCDVDFSV | IOP | **exists-open** (v2.38, ahead of ROM) | S |
| 11 | SECRMAN | IOP | **exists-open** (`secrman` 1.4) | S |
| 12 | EECONF (MechaCon EEPROM config) | IOP | **exists-open** | S |
| 13 | SIO2MAN/XSIO2MAN, PADMAN/XPADMAN, XMTAPMAN | IOP | **exists-open** | S |
| 14 | MCMAN/XMCMAN, MCSERV/XMCSERV | IOP | **exists-open** | S |
| 15 | LIBSD | IOP | **exists-open** (freesd) | S |
| 16 | `ROMVER` (+ `VERSTR`) | data | trivial — but the *content* matters (boot certify) | XS |
| 17 | `OSDSYS` slot: boot-certify → disc-type → SYSTEM.CNF/BOOT2 → LoadExecPS2 | EE | **aap's-osdsys**; minimal path also fully covered by `libosdinit` | S (minimal) / done (full) |
| 18 | `PS2LOGO` | EE | must-write or omit | S |
| 19 | ROM image packaging | host | **exists-open** (`ps2sdk/tools/romimg`) | XS |

Rough effort ranking (descending): **EE kernel ≫ RESET+RDRAM+IOPBOOT bring-up >
EELOAD > UDNL validation > OSDSYS-minimal > everything else (integration only)**.

Bluntly: **the IOP half of a minimal open ROM is already written.** What is missing is
a ~90 KB EE kernel, a few KB of dual-CPU reset/bring-up code, an ELF loader, and the
boot-decision program — plus the very large amount of on-hardware validation that
"boots *this* game" requires.

### 6.2 Top technical risks

1. **The EE kernel is the whole project.** ~100 syscalls, an exception/interrupt
   dispatcher, a thread/semaphore scheduler with exact priority and `iXxx`-from-interrupt
   semantics, TLB management, `ExecPS2`/`LoadExecPS2` state reset, and — worst —
   *bug-compatibility*. Games are shipped against specific kernel behaviours and several
   ship **kernel patches of their own** (the whole `ee/kernel/src/{osdsrc,tlbsrc}` patch
   machinery, and FMCB's "mark kernel as fully patched" logic, exist because programs poke
   the syscall table at `0x80011F80` and expect a known layout). A clean reimplementation
   with a *different* internal layout will break anything that patches by address rather
   than by `SetSyscall`. Mitigating: keep the syscall table at the canonical address and
   keep the documented syscall numbering; expect to have to match some internal offsets.
2. **CDVD / MechaCon boot certification.** `sceCdBootCertify()` (S-cmd 0x1A) takes the 4
   bytes derived from `ROMVER` and *binds the ROM to the MechaCon*. Your ROM must ship a
   `ROMVER` the console's MechaCon accepts, and the acceptable value is per-chassis:
   B-chassis and later require certify; SCPH-10000/15000 don't support it; DTL-H301xx
   OSDSYS hard-codes 1.10; from MechaCon fw 50000 an unlock pair (0x03 0x46 / 0x03 0x47)
   is needed first; Dragon-series (SCPH-50000+) MechaCons **hang on any blocking command
   when the tray is empty**, which is why OSDSYS checks `sceCdDiskType()` first. Get any
   of this wrong and the drive simply never unlocks. Good news: **the game data itself is
   not encrypted** — the only crypto in the boot path is the trivial ROL5+XOR of the
   16-sector logo, and the disc/region check lives in the MechaCon, not in the ROM. There
   is no signature check on the boot ROM by hardware.
3. **IOPRP merge fidelity.** Every retail game reboots the IOP with its own
   `IOPRPxxx.IMG`. `UDNL`'s "pick the newest version across images then ROM" logic, the
   `@800`/`!addr`/`!include` config directives, and the module-name/version conventions
   must be exact. A subtle mistake here produces "works on 200 games, hangs on 30".
4. **Timing-sensitive IOP bring-up.** SSBUS setup, SPU/CDVD reset delays, the SIF
   handshake sequence (`SIFINIT` → `EESYNC`), and the EE's spin on SIF registers are the
   classic sources of "boots in PCSX2, hangs on hardware" bugs. This is exactly the
   class of bug aap's savestate-bisection method is bad at and the dsedb/dsnet hardware
   loop is good at — budget for hardware-first bring-up.
5. **Per-model divergence, not per-region.** USA↔PAL at the same ROM version differs by
   one file (`ROMGSCRT`). What actually diverges is generation:
   - SCPH-10000/15000 "protokernel": no boot certify, no `TZLIST`/`ROMGSCRT`, buggy
     `ExecPS2`, no `SetOsdConfigParam2`.
   - SCPH-70000+: `NCDVDMAN` (crippled), universal ROMs with per-region
     `DVDID*/DVDPL*/DVDVER*/EROMDRV*` sets, rom1 present.
   - SCPH-7500x+ "Deckard": the IOP is *emulated* on a different core; ROM carries
     Deckard-specific modules (`XPARAM` etc.). A hand-written IOP kernel may or may not
     survive the IOP emulator. Also: controller ports aren't powered at cold boot there.
   - SCPH-50009 (China): `ADDROM2` + `GB18030` replacing `FONTM`.
   - PSX/DESR: `P*` module family; DVR ROM.
   Targeting one generation (e.g. fat SCPH-3x/5x) first is the sane scope cut.
6. **X-module ABI split.** Serving `rom0:SIO2MAN` and `rom0:XSIO2MAN` means shipping two
   incompatible ABIs and making sure a program that loads one doesn't get the other's
   dependencies. ps2sdk has both families, but the pairing rules are on you.
7. **Physical deployment.** The stock ROM is generally **not reflashable in place**; the
   custom-ROM scene desolders the SOP44 mask/flash and fits an `MX29LV320`/`MX29LV640`,
   programmed with an STM32F407/RP2040 rig. Practical bring-up therefore wants a
   PCSX2 loop plus a socketed console — or, easier, iterate entirely under PCSX2 first,
   since PCSX2 will happily boot an arbitrary 4 MB ROM image.
8. **Licensing hygiene.** ps2sdk is AFL-2.0, ps2iop is MIT, OSD-Initialization-Libraries
   is GPL-3.0. Mixing the last one into a redistributable ROM makes the whole image GPL-3
   unless it's used only as a reference. `fps2bios` has **no license at all** — do not
   copy from it.

### 6.3 Bottom line

A minimally-viable open PS2 boot ROM is **feasible and much closer than it looks**,
because the two-thirds of the ROM by risk (the entire IOP module ecosystem, including
CDVDMAN/CDVDFSV/MCMAN/PADMAN/SECRMAN/UDNL) is already reimplemented, maintained, and
version-ahead-of-ROM in ps2sdk and uyjulian's ps2iop, and the ROM-packaging tool ships
with ps2sdk. The critical path is a **~90 KB EE kernel** plus a few KB of dual-CPU reset
bring-up and an ELF loader — none of which any open project has completed — and the
OSDSYS slot, which is exactly what aap's project already occupies. The hard *engineering*
risk is the EE kernel's bug-compatibility and syscall-table layout; the hard *systems*
risk is MechaCon boot certification and the IOPRP merge; the hard *practical* risk is
hardware-only timing bugs and the fact that deployment means replacing a soldered flash.

---

## Sources

- ps2tek (israpps fork) — BIOS section index: <https://israpps.github.io/ps2tek/PS2/BIOS/>
- ps2tek — Boot Process: <https://israpps.github.io/ps2tek/PS2/BIOS/BootProcess.html>
- ps2tek — File structure (ROMDIR): <https://israpps.github.io/ps2tek/PS2/BIOS/File_structure.html>
- ps2tek — IOP REBOOT / SIF reboot server / UDNL & IOPRP: <https://israpps.github.io/ps2tek/PS2/BIOS/IOP_REBOOT.html>
- ps2tek — EE Syscalls: <https://israpps.github.io/ps2tek/PS2/BIOS/EE_Syscalls.html>
- ps2tek — EE RDRAM initialization: <https://israpps.github.io/ps2tek/PS2/EE/RDRAM_Init.html>
- ps2tek (original, PSI-Rockin): <https://psi-rockin.github.io/ps2tek/>
- uyjulian — "PS2 BIOS ROM contents" gist: <https://gist.github.com/uyjulian/25291080f083987d3f3c134f593483c5>
- AKuHAK — "PS2 BIOS ROM contents" gist: <https://gist.github.com/AKuHAK/db60caf94425654864d0a5d60f323294>
- uyjulian/ps2iop (MIT, IOP software reimplementation): <https://github.com/uyjulian/ps2iop>
- ps2dev/ps2sdk (local checkout `/u/aap/othersrc/ps2sdk`, HEAD f08e889f): <https://github.com/ps2dev/ps2sdk>
- ps2sdk `tools/romimg`: <https://github.com/ps2dev/ps2sdk/tree/master/tools/romimg>
- ps2sdk libcdvd (`sceCdBootCertify`, `sceCdReadKey`, `sceCdDecSet`): <https://github.com/ps2dev/ps2sdk/blob/master/common/include/libcdvd-common.h>
- ps2homebrew/OSD-Initialization-Libraries (GPL-3.0, ysai187/sp193): <https://github.com/ps2homebrew/OSD-Initialization-Libraries>
- ysai187 — Initializing the PS2/PSX: <https://sites.google.com/view/ysai187/home/projects/initializing-the-ps2psx>
- AKuHAK/fps2bios: <https://github.com/AKuHAK/fps2bios>
- Writing a PlayStation 2 BIOS in Rust: <https://rust-console.github.io/ps2-bios-book/print.html>
- mlafeldt/ps2logo — PS2 disc security explanation: <https://github.com/mlafeldt/ps2logo/blob/master/Documentation/PS2_Disc_Security_explanations.htm>
- TCRF — PlayStation 2/Security Implementations: <https://tcrf.net/PlayStation_2/Security_Implementations>
- jimmikaelkael/ps2-protokernel-patch: <https://github.com/jimmikaelkael/ps2-protokernel-patch>
- israpps/PlayStation2-Basic-BootLoader (PS2BBL): <https://github.com/israpps/PlayStation2-Basic-BootLoader>
- BitBuilt — Creating custom ROMs for the PS2: <https://bitbuilt.net/forums/threads/creating-custom-roms-for-the-ps2.6503/>
- ps2-home — PS2 BIOS Modules (Definitions): <https://www.ps2-home.com/forum/viewtopic.php?t=423>
- ps2-home — MechaCon functions (S-cmd 0x1A boot certify): <https://www.ps2-home.com/forum/viewtopic.php?t=8695>
- PCSX2 BIOS docs (no open alternative exists): <https://pcsx2.net/docs/setup/bios/>
- psdevwiki — IRX Files / IOPRP images: <https://www.psdevwiki.com/ps2/IRX_Files>
