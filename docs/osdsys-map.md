# OSDSYS structural map — everything outside the opening

Scope: the whole OSDSYS EE program *except* the boot animation module
(0x211d30–0x21c910), which is already reversed and being binary-matched.

Sources, in the order I trusted them:

1. `osdsys_dump.idb` read with python-idb (1527 functions, 1156 names,
   ~1055 of them non-auto) — segments, bounds, aap's own names.
2. `ee-objdump -m mips:5900` of `/u/aap/src/osdsys/expanded.bin`
   (VA = offset + 0x200000), plus a jal call graph (3856 edges), a
   `lui`/`addiu` absolute-address reconstructor and a gp-relative
   reconstructor built on top of it.
3. Prior docs (`menu-symbols.idc`, `menu-rendering-reverse.md`,
   `OSDSYS-STRUCTURE-REPORT.txt`, `osdsys_re-crossref.md`) — treated as
   hypotheses and checked; see §5 for the verdict.

Confidence tags follow the idc convention: **[ok]** = read out of the
disassembly and cross-checked; **[tnt]** = plausible, partially traced;
**[?]** = guess, flagged as such. Unlabelled statements are [ok].

---

## 0. Corrections to previously-recorded constants

Two numbers in circulation are wrong and they matter, because everything
gp-relative depends on them.

**`gp = 0x2AF070`.** Straight out of crt0 at 0x200034:

```
00200034 lui   a0,0x2b
00200048 addiu a0,a0,-3984      ; 0x2B0000 - 0xF90 = 0x2AF070
0020005c move  gp,a0
```

and the IDB independently names `002af070 _gp_`. So:

* the task brief's `0x2AF0B0` is off by 0x40;
* `menu-symbols.idc` / `menu-rendering-reverse.md` used **`gp = 0x282E1C`**,
  which is off by **−0x2C254**. Every "absolute address" the idc derived
  from a gp offset is therefore wrong by that amount (the *offsets*
  themselves are right — see §5).

**`osdCurStringTable` is at 0x26ECE0, not 0x26EDE0.** `osdGetString`
does `lw a0,-4896(v0)` with `v0 = lui 0x27`, i.e. 0x270000 − 0x1320 =
0x26ECE0. 0x26EDE0 is inside the resource table (§4.3), so the idc name
lands on unrelated data.

---

## 1. Top-level program anatomy

### 1.1 Segments

| VA range | size | what |
|---|---|---|
| `0x1F0000`–`0x200000` | 64 KB | **low block**: globals shared between threads/modules, below the program. Dumped separately in the IDB as segment `low`. |
| `0x200000`–`0x2A7EB5` | 687,796 B | the loaded program image (`expanded.bin`). Code `0x200000`–`~0x2678F0`, then data. |
| `0x2A7F00`–`0x3F32B0` | 1.35 MB | `.bss`, cleared by crt0 (`sq zero` loop, 0x2A7F00 → 0x3F32B0). |

Within the image:

| VA range | contents |
|---|---|
| `0x200000`–`0x2678F0` | code (1527 IDB functions) |
| `0x2678F0`–`0x26B060` | the 7 VU1 microprograms (`vucode_1..7`) |
| `0x26B060`–`0x26F1F0` | link tables: language table (0x26ECC0), current-table cache (0x26ECE0), **resource table (0x26ED00, 78 entries × 16 B)** |
| `0x26F1F0`–`0x284000` | misc `.data`, per-module globals, `menuElemVertTable`-style static vertex data at 0x284100 |
| `0x295A20`–`0x2968xx` | Module-V (browser) packet contexts, element descriptors, texture descriptors |
| `0x2972A8`–`0x2A3400` | **the 8 localized string blocks + their pointer tables** |
| `0x2A3400`–`0x2A5000` | path/format strings, jump tables (0x2A3990, 0x2A4120, 0x2A46C0), config title records (0x2A4380) |
| `0x2A7080`–`0x2A7280` | opening's float literal pool (see `object-order.md`) |
| `0x2A7700`–`0x2A7EB5` | `.sdata` — the gp-addressed initialized scalars, incl. the whole browser packet state |

### 1.2 Entry and boot

`start` (0x200008) → clear bss → `InitMainThread(gp=0x2AF070, stack=-1,
size=0x18000, args=0x2B8080, root=0x2000B8)` → `InitHeap` → `main(argc,argv)`
(0x207478, 0x9F0 bytes).

`main` in order:

1. `__main` (ctors); if `argc == 0`, `ExecOSD2` (0x2073D0) — re-exec itself.
2. `sceSifInitRpc`, resolve own path from `argv[0]` (`mc`/`rom` prefixes,
   `rom0:UDNL rom0:OSDCNF`, `rom0:ATAD`, `rom0:HDDLOAD`), honour
   `-osd`/`-stat`/`SkipMc`/`SkipHdd` arguments.
3. Read `rom0:ROMVER`, parse `%c%c%c%c`, publish it.
4. `sceMcInit`, `getBRDATA_SYSTEM`, `scePadInit`, `sceCdInit(1)`.
5. `loadResources(argv[0])` (0x2052E0, 628 insns) — populates the
   78-entry resource table.
6. `getResourcePtr(0)` → `fontm_20A0C0` — font metrics setup.
7. `osdGetLanguage` → `osdSelectStringTable`, then `0x2069C8`
   (push config to kernel, §3.4), timezone read/apply.
8. `CheckMemcardHistory`.
9. **`0x2051A8` — register the 7 modules** (§1.4).
10. Create semaphores at `0x26FE20+` and the three core threads,
    then `makeThreadB(prio 32, …)` and `makeThreadA(prio 5)`.
11. Initialise the low block (`0x1F0010 = 101`, `0x1F0014 = -1`,
    `0x1F00B0 = 5`, …).
12. `InitDraw` (0x205CE0), install the VBlank INTC handler
    (`vblankHandler` 0x2068F0 on cause 2), `ChangeThreadPriority(self, 30)`.
13. Then the boot-path decision (disc / DVD / CD / `SkipMc` …), writing
    `0x1F05E8` (`systemState`) and `0x1F0018`, and finally handing control
    to the module threads.

### 1.3 Thread inventory

All from the `CreateThread` parameter blocks (`+4` func, `+8` stack,
`+12` size, `+16` gp, `+20` priority).

| thread | created by | entry | stack | size | prio | role |
|---|---|---|---|---|---|---|
| main | crt0 | `main` 0x207478 | InitMainThread | 0x18000 | — | boot, then idles at prio 30 |
| SwapThread | main | 0x205F60 | 0x2B9360 | 0x2000 | 1 | double-buffer swap, driven by vblank sema |
| ThreadX | main | 0x206E00 | 0x2BB360 | 0x2000 | 3 | pad + timer poll (`ThreadX_pad_timer?` in IDB); also reads `0x1F00A4` |
| ThreadY | main | 0x206B40 | 0x2BD360 | 0x20000 | 4 | **async command server** — memory card + config, see §1.5 |
| ThreadA | `makeThreadA` 0x20B5E0 | 0x20B830 | 0x2DDE90 | 0x2000 | 5 (arg) | `threadA_timer_cd?` — CD/timer watchdog |
| ThreadB | `makeThreadB` 0x20BE60 | 0x20F7C0 | 0x2DFEC0 | 0x20000 | 32 (arg) | large worker; set up with a 0x780000-region work buffer |
| **OpeningThread** | `makeOpeningThread` 0x211C70 | 0x211D30 | 0x3002D0 | 0x20000 | 6 | the boot animation (already done) |
| **ThreadU** | `makeThreadU` 0x21C910 | 0x21CDD8 | 0x328E00 | 0x20000 | 6 | **main menu / system configuration** |
| **ThreadV** | `makeThreadV` 0x23FAF8 | 0x2402D8 | 0x3CCC40 | 0x20000 | 6 | **browser** (memory card / disc / CD player) |
| (lib) | 0x24E050 | 0x25DF90 | 0x3FFFF8 | — | arg | newlib/SIF helper thread |

The three prio-6, 0x20000-stack threads are the "screen modules" and are
structurally identical: sleep → per-frame loop → `SignalSema` → sleep.

### 1.4 The module registry — the real top-level architecture

This is the organising principle of the program and it was not in any
prior doc.

`osdRegisterModule` **(0x204408)** [ok] validates a 28-byte descriptor and
appends it to an array. The array base is **0x2B8B40**, limit **0x2B9240**
(64 slots), write pointer at **0x26ECF8**. Missing fields get filled with
default stubs (0x2043E8/0x2043F0/0x2043F8/0x204400).

Descriptor layout (28 B):

```
+0   setup()            creates the module's thread   (may be NULL)
+4   prepare()          default 0x2043E8
+8   getDesc(lang)      returns the module's display name
+12  getVersion(lang)   returns its version string
+16  f16()              default 0x2043F0   [tnt]
+20  f20(...)           default 0x2043F8   [tnt]
+24  f24()              default 0x204400   [tnt]
```

Accessors: `0x204618` = `getModuleDesc(i)` (calls `+8` with the current
language), `0x204660` = `getModuleVersion(i)` (`+12`), `0x2046A8` (`+16`),
`0x2046E0` (`+20`), **`0x204530` = `getModuleCount()`** (`(ptr − 0x2B8B40)/28`).

`0x2051A8` = **`osdRegisterAllModules()`**, called from `main`, registers
exactly seven, in this order:

| # | registrar | setup → thread | display name | version |
|---|---|---|---|---|
| 0 | 0x204818 | 0x2047E8 | `getString(98)` = **"Console"** | 0x204808 → 0x2B9260 (runtime ROMVER) |
| 1 | 0x211CE0 | `makeOpeningThread` → `OpeningThread` | `""` (0x2A76E8) | `"1.20"` |
| 2 | 0x21C980 | `makeThreadU` → `ThreadU` | `""` (0x2A7810) | `"1.20"` |
| 3 | 0x23FB68 | `makeThreadV` → `ThreadV` | `getString(101)` = **"Browser"** | `"1.20"` |
| 4 | 0x204898 | — | `getString(102)` = **"CD Player"** | `"1.20"` |
| 5 | 0x204938 | — | `getString(103)` = **"PlayStation® Driver"** | `"1.01"`/`"1.10"` (version-gated) |
| 6 | 0x205158 | 0x204FC8 | 0x204980 | 0x204D08 — the **DVD Player** slot [tnt] |

`0x2051F8` walks `0..getModuleCount()` collecting (name, version) pairs
into `0x1F1238 + i*12` — that table is what the **Version Information**
screen renders. It then stores `getString(100)` ("Diagnosis,Off,On\n") at
`0x1F1240` and `getString(105)` ("Disc Speed,…\nTexture Mapping,…\n") at
`0x1F1264`.

### 1.5 ThreadY: the async command server

`ThreadY` (0x206B40) is a classic command pump:

```
loop:
  CancelWakeupThread(self); SleepThread()
  cmd = *(u32*)0x1F00A4
  if (cmd-3) < 24:  jump  *(u32*)(0x2A3990 + (cmd-3)*4)
```

24-entry jump table at **0x2A3990** [ok]:

| cmd | handler | calls |
|---|---|---|
| 3–15 | 0x201038 … 0x201380 | the `sceMc*` library at 0x252898–0x2535B0 — memory-card open/close/read/write/format/getdir/getinfo |
| 16, 18, 26 | 0x203570 | `0x2033F8` — **write the console configuration** |
| 22 | 0x2051F8 | rebuild the Version Information table |
| 17,19,20,21,23,24 | 0x24D9B0 | `CancelWakeupThread` — no-op slots |

So the whole program's blocking I/O funnels through one word:
a UI thread writes `0x1F00A4 = cmd`, wakes ThreadY, and polls
`0x1F00A4 == 0` for completion; `0x1F00B0` carries the resulting
disc/tray state. Module V's memcard wrapper cluster
(**0x232F68–0x2334xx**, ~14 near-identical little functions) is nothing
but set-cmd / wake / poll pairs. This is the single most important
concurrency fact in the program.

### 1.6 The low block, 0x1F0000

Reconstructed from `main`'s initialisation plus every reader/writer found
in the disassembly. IDB-supplied names marked (idb).

| offset | use |
|---|---|
| `+0x000` | ThreadX id |
| `+0x004` | ThreadY id |
| `+0x00C` | cleared by main [tnt] |
| `+0x010` | **current/requested screen id** (main sets 101); ModU dispatches on it |
| `+0x014` | screen parameter / result (main sets −1) |
| `+0x018` | boot mode (0 normal, 1/2/3/4 for the DVD/CD/`SkipMc` paths) |
| `+0x01C` | string scratch — main `strcpy`s `getString(96)` here |
| `+0x0A4` | **ThreadY command word** (§1.5) |
| `+0x0B0` | **disc/tray state** (main sets 5; browser watches for 5 and 8) |
| `+0x0B4`, `+0x124` | cleared by main |
| `+0x138` | `history` (idb) |
| `+0x5E4` | module-exit request flag |
| `+0x5E8` | `systemState` (idb) — what to do when a module exits |
| `+0x5EC` | id of the module that requested the exit |
| `+0x9F8`/`+0x9FC`/`+0xA00` | message-ring flag / read idx / write idx (128 slots) |
| `+0xA10` | `dbuff` (idb) — the message ring buffer itself |
| `+0xC40`/`+0xC44` | `evenOddFrame` / `evenOddField` (idb) |
| `+0xC44` | also read every frame by both ModU and ModV as a frame/field input |
| `+0xC50`/`+0xC54` | `screen_width` / `screen_height` (idb) |
| `+0x1224` | `ps1drvConfig` — 15 bytes, two nibble fields each |
| `+0x1234` | a config bit fed into the settings model |
| `+0x1238` | Version Information table: 12-byte {name, version, 0} per module |
| `+0x1240`, `+0x1264` | cached `getString(100)` / `getString(105)` |
| `+0x13B8` | −1 sentinel |

### 1.7 The 20500/20501 messages

`0x200B80(id, …)` is a **varargs message post**: it spills a0..t3 into a
12-word record and appends it to the 128-deep ring at `0x1F09F8`/`0x1F0A10`.
Posters of 20500/20501:

* `0x211FD8`, `ProcessOpeningAnimation` (0x215FD0) — opening
* `ThreadU` (0x21CDD8) posts `20501,1,0,0` right after waking
* `0x21CA38` (ModU frame proc) posts 20501 three times
* `0x224288`, `0x228460` (ModU config screens) post 20500/20501

Given the `SND*` resource set and that the posts bracket scene changes,
these are **sound/scene-transition commands** [tnt]; I did not locate the
ring's consumer (nothing in the app region reads `0x1F09FC`/`0x1F0A00`
through a form my scanner recognised), so the consumer is either in
`ThreadA`/`ThreadB` via a base pointer, or on the IOP side. **Labelled gap.**

### 1.8 Module boundaries — the address map

Derived by BFS over the call graph from each thread root and intersecting
(the partition is remarkably clean: only one 4 KB page, 0x230000, is
genuinely shared between U and V).

| VA range | bytes | funcs | subsystem |
|---|---|---|---|
| `0x200000`–`0x200C90` | | | crt0, message post (0x200B80), sound init (0x200250) |
| `0x200C90`–`0x201000` | | | `Expand*` — the decompressor (`ps2expand.c`) |
| `0x201008`–`0x2034xx` | | | **CDVD / memcard / NVM command implementations** — the bodies ThreadY jumps into; `0x203390`/`0x2033F8` = config read/write |
| `0x203570`–`0x203A10` | | ~20 | **console configuration accessors** (§3.3) |
| `0x203A10`–`0x204100` | | | memcard history (`radMemcardHistory1`, `memcardHistory2`, `CheckMemcardHistory`) |
| `0x2040D0`–`0x204240` | | | `GetLanguage`, `osdSelectStringTable`, `osdGetString`, ROMVER |
| `0x204318`–`0x204560` | | | version check, **module registry** |
| `0x204618`–`0x205158` | | | module accessors + the Console / CD Player / PS1 Driver / DVD Player module descriptors |
| `0x2051A8`–`0x2052E0` | | | `osdRegisterAllModules`, Version-Information table builder |
| `0x2052E0`–`0x205CE0` | | | `loadResources` + `getResourcePtr`/`getResourceSize` |
| `0x205CE0`–`0x206270` | | | `InitDraw`, `StartFrame`, `waitNextFrame`, `SwapThread`, `SwapBuffers` |
| `0x206270`–`0x207478` | | | `TZLIST` handling, `vblankHandler`, ThreadY, ThreadX |
| `0x207478`–`0x208130` | | | `main`, `ExecOSD2`, `checkpathThing`, `mcstuff` |
| `0x208130`–`0x20B600` | | ~80 | **text / font engine** — colour table (0x2717D0) indexed by `0x208130` → `0x208110`, `0x209998` string measure (handles the 0x07/0x09/0x0A escapes via `0x209300`), `0x209DA0` string draw, `0x209640` (11 callers) and `0x209998` (30 callers) as the main entry points, `texUpload_208E48`, `fontm_20A0C0`, `FONT_20A3C8`, date/time formatters (0x20A998–0x20AC10, one per notation bit) |
| `0x20B5E0`–`0x211C70` | | | `makeThreadA`/`ThreadA`, `BootIllegal`, `makeThreadB`/`ThreadB` (0x20BE60–0x211680, the biggest core block — disc/title boot machinery), **the event queue + handler registry (0x2116D8–0x211C50)** |
| `0x211C70`–`0x21C910` | 0xACA0 | 118 | **opening module** — DONE |
| `0x21C910`–`0x230000` | 0x13708 | 286 | **Module U — main menu + system configuration** |
| `0x230000`–`0x231000` | | ~17 | **shared renderer core** — `sendDma` (0x22EE00), `setScreenMatrix` (0x230478), `setLightMatrices` (0x230580), `setTextureUpload` (0x230708), `SetWorldMatrix` (0x230898); custom VIF1 UNPACK chains via D1 MMIO |
| `0x231000`–`0x24C068` | 0x1C050 | 396 | **Module V — browser / memory card / CD player** |
| `0x24C068`–`0x2678F0` | | 425 | libraries: `scePad`, EE kernel syscall wrappers, `sceCd`, `sceMc`, libm, newlib (`malloc`, `printf`, `dtoa`), `sceGs`, **SIF RPC** (0x263F30–0x264DB8), `sceVif1Pk` (0x266400–0x266C00), `sceVu0*` matrix library (0x267000–0x2678B0) |

### 1.9 The event queue (0x2116D8–0x211C50)

Separate from the message ring. A ring of 20 8-byte events plus a
20-entry handler registry, all inside one context struct addressed
through `gp-31140` and `gp-31136`:

* `0x2119A8` — init (also called by `ThreadV`)
* `0x2116D8` — push event `{type:u8, code:u8, param:u16}`
* `0x211800` — pop event
* `0x211888` — register/lookup a handler (12-byte entries)
* `0x211BB0` — find handler by key
* five typed push wrappers with validation:
  `0x211A28`(t=1, code<17, param<257), `0x211A78`(t=2, code<3),
  `0x211AC0`(t=3, code<3), `0x211B08`(t=4, code<4),
  `0x211B50`(t=5, code ∈ {11,12,17,18,255})

Context fields: `+0xA0` write idx, `+0xA4` read idx, `+0xA8` count,
`+0xAC` sequence number, `+0xB0` handler table.

---

## 2. The browser / main menu (Module V)

Registered as module #3, display name `getString(101)` = "Browser".
Code region **0x231000–0x24C068**, 396 functions, ~114 KB.
Texture resources are the `TEXB*` set; `getTexB` (0x23FA98, aap's IDB
name) reads the resource-ID table at **0x295A20**.

### 2.1 Entry path

`main` → `osdRegisterAllModules` → `0x23FB68` registers
{setup = `makeThreadV`}. Nothing calls `makeThreadV` directly; it is
invoked through the descriptor's `+0` slot when the OSD decides to bring
the browser up. `makeThreadV` creates ThreadV at prio 6 and starts it;
ThreadV immediately `SleepThread()`s and is woken by whoever wants the
browser on screen.

### 2.2 ThreadV main loop (0x2402D8)

```
SleepThread()
outer:
  0x23FE48                      ; module init
  0x23E8F0(screen_id)           ; screen_id = 90 or 75 depending on gp-29624
  StartFrame (0x205E88)
  0x2119A8(0x780000)            ; event-queue init on a 0x780000 arena
  inner:
    *(u32*)0x10000000 = 0       ; VIF/GIF reg poke
    0x23FE78                    ; ===== the per-frame body =====
    0x240178                    ; reset-combo watchdog + "Reset" overlay
    ++(s64)*(gp-29648)          ; frame counter, saturates at 0
    waitNextFrame (0x205F30)
    while *(gp-29632) != 0 and the 76/91 screen-id window holds
  0x23FFF8                      ; module exit: publish 0x1F05E4/5E8/5EC/0x1F0014
  SignalSema(*(0x26FE34)); SleepThread()
  goto outer
```

`0x23FE48` (init) = `0x23FBB8` (reads `0x1F05E8`) → `0x23FCC0`
(resources, `getResourcePtr`) → `0x23FD78` → `0x244B00`.

`0x23FE78` (frame) = `0x23DB90` → `0x23E330(0,3,1)` → `0x23E3E8` →
`0x23EE10` → `0x244CA8` → **`0x2449E0`** → `0x23E298(*(0x1F0C44))` →
tail `0x23EB38`.

`0x2449E0` → `0x2437A0` → **`0x245038`** (the element renderer) is the
draw path.

### 2.3 Screen states

`0x23FED8` and `0x23FF38` are the two "enter the browser in mode N"
entry points. They map the incoming screen id to an internal mode in
`gp-29616` and set `gp-29632 = 1` (loop-run flag):

| screen id | internal mode |
|---|---|
| 110 | 0 (and `gp-29620 = 0`) |
| 108, 109 | 1 |
| 106, 107 | 2 |
| 115 | 3 |
| anything else | −1 (and `gp-29620 = id`) |

Both then call `0x2418B8(2)`, `0x240038` (clear `gp-29672`), `0x23D688`,
and `0x23E918(90 or 75)`.

On exit `0x23FFF8` calls `0x235188` then publishes
`0x1F05E8 = gp-29620`, `0x1F05EC = 3` (module id 3 = Browser),
`0x1F0014 = gp-29616`, `0x1F05E4 = 1` — i.e. "browser finished, here is
why and with what parameter".

### 2.4 The reset watchdog / "Reset" overlay (0x240178)

Fully decoded, and it is *not* general input handling:

* while `gp-29664 == 0`: if the pad state (`gp-29392`) equals **0x808F**
  it counts `gp-29668` up to 121 frames (~2 s) and then raises
  `gp-29672 = 1` (prompt visible). If the pad then reads **2048**
  (START), it sets `gp-29664 = 27` and calls `0x240040` (browser reset).
* while `gp-29664 != 0`: it counts down from 27, computes an alpha with
  `cosf`, calls `0x208110(114, 39, 39, alpha)` (text colour [tnt]) and
  `0x23F2D8(186, "Reset")` — the literal string `"Reset"` at 0x2A7C98,
  verified — then runs the disc-state machine `0x240068`.

  `0x23F2D8` itself is a **PAL coordinate fixup**: if `gp-29624`
  (= 0x2A7CB8, the PAL flag) is set it scales the x argument by the float
  at `gp-31684`, then tail-jumps to the shared text engine at
  **`0x209DA0`**. The neighbouring `0x23F308` does the same with the
  factor at `gp-31680`. Module V is full of these little wrappers, which
  is why so many of its "draw" functions look empty in a call graph.

### 2.5 The packet/DMA layer (0x2403C8 / 0x240438)

Confirmed exactly as the prior write-up described (only the absolute
addresses of the gp globals were wrong):

* **`0x2403C8` open**: `idx = *(u8*)(gp-29596)`;
  ctx = `0x295AD0 + idx*32`; scratch = `0x70000000 | (idx << 13)`;
  `sceVif1PkInit` (0x266A60), `PkReset` (0x266A50), `PktCnt(ctx,0)`
  (0x266990), `PkOpenDirectCode(ctx,0)` (0x266848); stores ctx to
  `gp-29600` (= **0x2A7CD0**).
* **`0x240438` kick**: `PkCloseGifTag` (0x266760), `PkCloseDirectCode`
  (0x266818), `PkEnd(ctx,0)` (0x2668A0), `PkTerminate` (0x2669E8),
  `sceDmaSync(chan,0,0)` (0x266B08); then
  `qwc = (head_tag.word1 & 0x0FFFFFFF) | 0x80000000` and
  `sceDmaSend(chan, qwc)` (0x266BD0) — the IRQ bit is set on every kick;
  clears `gp-29600` and flips `gp-29596`.
* Sync/close variants `0x2404C0`, `0x2407A8`, `0x2409A0` operate on
  contexts at 0x295AC0/0x295A90 (different packet pools).
  `0x240B48` streams GS data (contexts at 0x295AA0), used by six callers
  in the 0x239–0x23B range.
* `0x240E68` and `0x240040`/`0x246F30` have **no direct callers** — either
  dead code or reached indirectly. **Labelled gap.**

### 2.6 The element renderer (0x245038)

Confirmed: `el = 0x295D10 + *(gp-29496)*352`; builds a matrix with the
shared `sceVu0`/0x267xxx helpers (0x267630, 0x267370, 0x2676B0, 0x267860);
static vertex table at **0x284100**; GIF tag template at **0x295D00**;
GS register data at **0x295CD0**; kicks via 0x240438.
**Only one caller: `0x2437A0`, which calls it 4×** (0x2437EC, 0x243800,
0x243830, 0x243840). The idc's claim that 0x238228 and 0x248370 call it
is wrong (§5).

### 2.7 Textures and icons

* `0x247070` ← `0x23FD78` (module init) is the upload driver; it walks the
  28-byte descriptors at **0x296160**.
* `0x246D28(src, id, …)` picks `0x296160 + (id & 0xFFFF)*28` and calls
  `0x246DE8` twice (CLUT then image).
* `0x246DE8` does `FlushCache(0)` (0x24DCE0 — note: the IDB names this
  `FlushCache`, *not* a file RPC as the prior doc guessed), then
  `sceVif1PkInit`/`PkReset`/`PkRefLoadImage`/`PkEnd`/`PkTerminate`,
  `sceDmaSync`, `sceDmaSend`.
* Icon resources: `ICOIMAGE` archive with `ICOBDISC`, `ICOBCDDA`,
  `ICOBPS1M`, `ICOBPS2M`, `ICOBPKST`, `ICOBPS1D`, `ICOBPS2D`, `ICOBDVDD`,
  `ICOBYSYS`, `ICOBFSCE`, `ICOBFNOR`, `ICOBFBRK`, `ICOBQUES`.

### 2.8 Memory-card enumeration

The browser never calls `sceMc*` itself. It goes through the ThreadY
command word (§1.5) via the wrapper cluster **0x232F68, 0x232FE0,
0x233008, 0x233038, 0x233068, 0x233098, 0x233120, 0x2331E8, 0x233278,
0x2332E8, 0x233388, 0x2333A8, 0x2333F0, …** — each is
`store cmd → wake ThreadY → poll 0x1F00A4`. Directory results land in a
Module-V buffer that the file-list screens (0x235EB8, 0x236348, 0x236440,
0x236550, 0x2375B8, 0x237870) render. **I did not trace the result buffer
layout — labelled gap.**

Browser screen functions identified by the string IDs they request:

| function | strings | screen |
|---|---|---|
| 0x235EB8 | 45,46,47 | free-space / "Unformatted" line |
| 0x236348 | 48,49,50 | "Are you sure? / Yes / No" confirm |
| 0x236440 | 24 | Delete |
| 0x236550 | 23 | Copy |
| 0x2375B8 | 61,64,65,71,72,73 | file properties (size, protection) |
| 0x237870 | 61,62,64,65 | size display |
| 0x23CC18 | 57 | "Corrupted data" |
| 0x241F18 | 45,46,47,130,131,133,134,135 | card list + play-mode overlay |
| 0x242828 | 49,50,74,75,76,77,78,79 | **format flow** |
| 0x2429C8 | 80,82 | "No data" / "Now loading…" |
| 0x242C38 | 88 | "Reading disc…" |
| 0x2443C8 | 83 | "Disc read error." |
| 0x244D70 | 21,136,137 | device labels ("PocketStation®/", "Memory Card/") |
| 0x239470, 0x249CC8, 0x24C070 | 2 | "Track %d" — **CD player** |
| 0x23A890 | 3,4,5 | "-", "min.", "sec." |
| 0x24A610 | 127,129,130,131 | Play Mode / Normal / Program / Shuffle |
| 0x24AC08 | 128,132,133,134 | Repeat / Off / All / 1 |
| 0x24B1D8, 0x24B408 | 126,127,128 | Play Options |

So the "CD Player" module (#4, no `setup`) has no thread of its own —
**its UI lives inside Module V**.

---

## 3. The system configuration menu (Module U)

Registered as module #2 with an empty display name.
Code region **0x21C910–0x230000**, 286 functions, ~79 KB.
Textures are the `TEXC*` set (`getTexC` 0x2297A0, `setTexCOffset_8b`
0x229750, `TEXTURES_229698` — aap's own IDB names).

`osdsys_re` calls this the "clock" module (HDDOSD naming). That is
consistent: it renders the main-menu scene (the clock/orb background)
*and* hosts the System Configuration, Version Information and
first-boot-wizard screens.

### 3.1 ThreadU loop (0x21CDD8)

```
SleepThread()
0x200B80(20501, 1, 0, 0)        ; scene-enter message
0x230090()                      ; renderer init (shared core)
0x21CE40()                      ; *(float*)0x27B440 = 1.0
loop:
  0x21C9D0()                    ; per-frame setup: sceGsResetPath, 0x266E80(1), IsPAL
  0x21CA38()                    ; per-frame body — the screen state machine
  SignalSema(*(0x26FE34))
  SleepThread()
```

### 3.2 The screen state machine (0x21CA38)

```
requested = *(u32*)0x1F0010          ; screen id
current   = *(gp-28884)              ; = 0x2A7F9C
0x21CE58()                           ; sub-renderer chain (0x22EE88, 0x22BE18, 0x22B838, 0x22B128)
StartFrame()
if (requested != current) and 106 <= requested <= 116:
    jump *(u32*)(0x2A46C0 + (requested-106)*4)
```

Jump table at **0x2A46C0** (11 entries, ids 106…116) — ten of the eleven
point at the same stub `0x21CAE8`, only id **114** goes to `0x21CAD8`
(which additionally gates on `0x1F0CF8 > 0`). Both stubs go through
`0x226980`, then `0x2269F0`, `0x226950`, `0x22AD30`. The screen id 116
and the sentinel 9999 get special treatment. This mirrors exactly the
opening's 11-type dispatch (jump table 0x2A4120, ids 106–116 = 'j'…'t'),
so **the OSD uses one global screen-id namespace across all modules**:
main seeds it with 101, the browser reacts to 75/90/106–110/115, ModU to
106–116.

`0x2283F0` is Module U's **screen-init hub** — it calls eleven per-screen
initialisers in a row and then tail-jumps to `0x2236B8`:

```
0x2283D0, 0x226FA8, 0x227198, 0x2202C8, 0x2217A8, 0x227DE8,
0x2283A0, 0x224630, 0x221D78, 0x2221B8, 0x224288  →  j 0x2236B8
```

Its only caller is `0x21CF20`, which is called from `0x21CA38` and fans
out to sixteen sub-initialisers (0x21CFD8, 0x21D0A0, 0x21D368, 0x21D3A0,
0x21DA68, 0x21DB18, 0x225BF8, 0x2268F0, 0x2283F0, 0x2285C0, 0x2287D0,
0x22B020, 0x22B058, 0x22B588, 0x22BB30, 0x22BE30).

Identified screens by string usage:

| function | strings | screen |
|---|---|---|
| 0x220DF8 | 89, 103 | **Version Information** |
| 0x224630 | 153–160 | **first-boot setup wizard** ("Select language.", "Select time zone.", "Is daylight savings time in effect?", "Settings can be adjusted later in \"System Configuration\".") |
| 0x223790 | 148 + 10 computed | "(Summer Time)" / DST screen |
| 0x21D7F8, 0x21EF00, 0x221D78, 0x222C08, 0x222CB0, 0x2235C8, 0x224D68, 0x227560, 0x228110, 0x228708 | all computed IDs | table-driven config item screens |

The screens with *computed* string IDs are the config menus proper —
their item lists come from tables, not from literal `li a0,N`. The record
table at **0x2A4380** is one of them: **16-byte** records
`{flags=0x100, x=0x40/0x49, id1, id2}` where `id1` carries a 0x100 flag
bit over a small string id, repeating with the flag alternating. (The idc
said 32 bytes; the observed stride is 16 — see §5.) It has **no direct
xref**: it is reached through a base pointer, so I could not confirm its
consumer. **Labelled gap.**

### 3.3 The settings themselves — the config word

The console configuration lives in a packed 32-bit word at **0x2A8700**
(with a second block at 0x2A86F0). Twenty accessor functions in
**0x203570–0x203A10** get/set its fields; the bit layout falls straight
out of their shifts and masks:

| bits | getter | setter | meaning |
|---|---|---|---|
| 0 | 0x203658 | 0x203668 | S/PDIF mode |
| 1–2 | 0x203690 | 0x2036B0 | **screen type** (0 = 4:3, 1 = Full, 2 = 16:9) — strings 107–110 |
| 3 | 0x2036F8 | 0x203710 | **video output** (RGB / Component) — strings 114–116 |
| 4–8 | 0x203738 (via `GetLanguage` 0x2040D0) | 0x2037B8 | **language** (5 bits) — strings 117–125 |
| 9–19 | 0x203848 | 0x203860 | **timezone offset**, 11-bit *signed* (minutes) |
| 20–28 | 0x203890 | 0x2038C0 | **timezone index**, 9-bit, clamped to 127 — indexes the country/city strings 161–297 |
| 29 | 0x203928 | 0x203940 | **daylight saving** — strings 147–150 |
| 30 | 0x203968 | 0x203980 | **time notation** (12/24 h) — strings 140–142 |
| (2 bits, second word) | 0x2039A8 | 0x2039C0 | **date notation** (MM/DD/YYYY, DD/MM/YYYY, YYYY/MM/DD) — strings 143–146 |

`0x203570` = write-config (calls `0x2033F8`), `0x2035D0` = read-config
(calls `0x203390`).

### 3.4 Persistence — it is CDVD NVM, over SIF RPC

`0x203390` (read) and `0x2033F8` (write) work on a handle struct at
**0x2A8680** and delegate to `0x203220`, which calls three libcdvd
S-command RPCs:

| function | SCMD id | role |
|---|---|---|
| 0x251670 | 20 | open config |
| 0x251780 | 21 | close config |
| 0x251840 | 22 | read config |
| (next) | 23 | write config |

all through `sceSifCallRpc` (0x263F30) after `_sceCd_scmd_prechk`
(0x24FA38). So: **settings persist in the console's NVM/EEPROM via
cdvdman on the IOP; there is no file and no memory card involved.** The
write is triggered asynchronously through ThreadY command 16/18/26 (§1.5).

### 3.5 Publishing the config to the kernel

`0x2069C8` (called by `main`) is the converter from OSDSYS's internal
word to the **standard PS2 `ConfigParam` layout**, which is why the
layouts differ:

```
bit 0      = spdifMode      (0x203658)
bits 1-2   = screenType     (0x203690)
bit 3      = videoOutput    (0x2036F8)
bit 4      = japLanguage    (forced 1)
bits 5-12  = ps1drvConfig   (*(u8*)0x1F1224 << 5)
bits 13-15 = version        (forced 1)
bits 16-20 = language       (GetLanguage)
bits 21-31 = timezoneOffset (0x203848)
```

This matches PS2SDK's `ConfigParam` exactly and is strong independent
confirmation of the field semantics above.

### 3.6 The UI model struct

`0x22B138` = **read config into the UI model**; `0x22B3F8` = **commit the
UI model back** (it is the only function that calls all ten setters, and
it also drives ThreadY command 26). The model lives at **0x352880**
(in bss):

| offset | field |
|---|---|
| +0 | spdifMode |
| +4 | screenType |
| +8 | videoOutput |
| +12 | language |
| +16 | bit 0 of `0x1F1234` |
| +24, +28, +32, +36 | clock fields (0x22B7B0, 0x22B7A0, 0x22B790, 0x22B720) |
| +52 | time notation |
| +56 | date notation |
| +60 | timezone index |
| +64 | daylight saving |
| +68…+188 | PS1 driver config: 15 entries × {b & 7, (b & 0x70) >> 4} unpacked from `0x1F1224` |

Only five functions touch it: `0x22B0E8`, `0x22B138`, `0x22B2A8`,
`0x22B3F8`, `0x22C2A0` — a small, well-bounded surface.

### 3.7 The clock

`0x22B720`/`0x22B790`/`0x22B7A0`/`0x22B7B0` feed the clock fields, and
the date/time *formatters* live in the shared text region
(0x20A998, 0x20AAA0, 0x20ABB0, 0x20AC10) — each consumes exactly one of
the notation/DST bits. Clock Adjustment is string 106.

---

## 4. Localization and string machinery

### 4.1 The API — verified

* **`osdGetString(id)` = 0x2041B8** [ok]. Returns
  `((char**)*(u32*)0x26ECE0)[id]`. Special-cased: ids **85** and **86**
  ("Back", "Enter") are, when the version check `0x204318` returns
  non-zero, read from the *table struct* at `+340` and `+344` instead —
  i.e. the button labels swap for the Japanese/regional button layout.
* **`osdSelectStringTable(lang)` = 0x204170** [ok]. Caches
  `0x26ECC0[lang]` into `0x26ECE0`. Note it indexes with **its own
  argument**, and separately calls `0x2037B8(lang)` for the side effect of
  writing the language field of the config word.
* **`0x2037B8` is a language *setter*, not a getter** — it writes bits 4–8
  of 0x2A8700, clamping the value against the version check (`< 2` or
  `< 8` depending on `0x204318`). The idc's name `osdGetLanguage` is
  misleading. The real getter is **`GetLanguage` = 0x2040D0** (aap's own
  IDB name), which tail-calls `0x203738`.

### 4.2 The tables

`osdLanguageTables` at **0x26ECC0**, 9 entries; slot 8 aliases slot 1.
Each table is 299 pointers (ids 0…298); index 299 is NULL, which is what
terminates them.

| lang | table | verified by string 91 |
|---|---|---|
| 0 | 0x2972A8 | Japanese — **Shift-JIS**, `シ ス テ ム 設定` |
| 1 | 0x298B08 | English — "System Configuration" |
| 2 | 0x29A0E8 | French — "Configuration du Système" |
| 3 | 0x2A1B30 | Spanish — "Configuración del Sistema" |
| 4 | 0x29D1D0 | German — "Systemkonfiguration" |
| 5 | 0x29EA88 | Italian — "Configurazione di Sistema" |
| 6 | 0x29B9D8 | Dutch — "Systeemconfiguratie" |
| 7 | 0x2A0250 | Portuguese — "Rotina de pesquisa" (Browser) |
| 8 | 0x298B08 | English (alias) |

This is exactly PS2SDK's `LANGUAGE_*` enum order
(JAPANESE=0, ENGLISH, FRENCH, SPANISH, GERMAN, ITALIAN, DUTCH,
PORTUGUESE), which the string list itself confirms (ids 118–125).

### 4.3 Encoding and markup

Latin-language strings are **not plain Latin-1**. Two escape mechanisms:

* **`0x87` + byte** = extended glyph (accented characters). E.g.
  `Syst\x87Qme` = "Système", `Configuraci\x87pn` = "Configuración",
  `S\x87^` = "Sí", `Configura\x87M\x87Io` = "Configuração".
  Presumably indexes the `FNTEX000`/`FNTEX001`/`FNTEXOSD` extension fonts. [tnt]
* **`0x07` + directive** = inline markup, handled by the text engine
  (`0x209998` treats 0x07, 0x09, 0x0A specially and calls `0x209300` for
  the escape body). Observed forms: `\x07o004` (symbol glyph — the ® in
  "PlayStation®"), `\x07o016`, `\x07r0.90` … `\x07r0.00` (horizontal
  scale ratio, used to squeeze long country names such as
  "\x07r0.80Petropavlovsk-Kamchatsky\x07r0.00"), `\x07p…` (used inside
  the `1-\x07p@DA\x07p00` disc-track labels). The exact grammar is
  **not fully decoded — labelled gap.**

Some strings are also **comma/newline-packed option lists**, expanded by
the caller: id 100 = `"Diagnosis,Off,On\n"`, id 105 =
`"Disc Speed,Standard,Fast\nTexture Mapping,Standard,Smooth\n"`.

### 4.4 String-ID map (English), by block

```
  0-  8  numeric/units scaffolding ("Track %d", "min.", "sec.", "PS2")
  9- 16  PS1 memory-card block labels (1-A … 2-D)
 17- 22  disc types (PlayStation® DISC, PlayStation®2 DISC, DVD Video, Audio CD)
 23- 50  memory-card operations: Copy/Delete/Property, progress and error text
 51- 73  file types, sizes, dates, protection flags
 74- 83  format flow + "No data" / "Now loading..." / "Disc read error."
 84- 92  top-level: CD Playback, Back, Enter, Options, Reading disc...,
         Version Information, Browser, System Configuration, Diagnosis
 93-105  module names and versions, Diagnosis/Disc Speed option lists
106-116  Clock Adjustment, Screen Size (4:3/Full/16:9), DIGITAL OUT,
         Component Video Out (RGB / Y Cb/Pb Cr/Pr)
117-125  Language + the 8 language names
126-139  CD player Play Options / Play Mode / Repeat, device path labels
140-160  Time Format, Date Format, Daylight Savings, Time Zone,
         User Preferences, and the first-boot wizard prompts
161-298  the timezone country/city list (Afghanistan … Taiwan, Unknown)
```

### 4.5 Resources (0x26ED00, 78 entries × 16 B)

`{name*, ptr, size, flag}`; `getResourcePtr(i)` = `+4`,
`getResourceSize(i)` = `+8`. Flag: 1 = standalone, 2 = archive,
4 = member of the preceding archive.

```
  0        FONTM                          (flag 1)
  1- 5     FNTIMAGE + FNTASCII FNTEX000 FNTEX001 FNTEXOSD
  6-18     SNDIMAGE + SNDBOOTH/B/S SNDTNNLS SNDCLOKS SNDTM30S SNDTM60S
           SNDOSDDH/B SNDLOGOS SNDWARNS SNDRCLKS
 19-62     TEXIMAGE + TEXOPNG*/TEXOWAL0/TEXOBLP/TEXOCRLE/TEXOFOG0-4/
           TEXOREF/TEXOBLPR/TEXOFLAR/TEXOSCE/TEXOCRBL   ← opening
           TEXCKLGN/P TEXCKLFN/P TEXCFLOW TEXCKABE TEXCBUMP TEXCBINV
           TEXCSMOK TEXCREFA TEXCNAVI TEXCBLUR TEXCSTSL TEXCMARU ← Module U
           TEXBNAV1/2 TEXBARRW TEXBBTTN TEXBCPAR TEXBCDPB TEXBOVAL
           TEXBICHI                                       ← Module V
 63-76     ICOIMAGE + ICOBDISC ICOBCDDA ICOBPS1M ICOBPS2M ICOBPKST
           ICOBPS1D ICOBPS2D ICOBDVDD ICOBYSYS ICOBFSCE ICOBFNOR
           ICOBFBRK ICOBQUES
 77        TZLIST                         (flag 1)
```

The `TEXO`/`TEXC`/`TEXB` prefixes map one-to-one onto
opening / Module U / Module V, which independently corroborates the
module partition in §1.8 and aap's own `getTexB`/`getTexC` names.

---

## 5. Verdict on `docs/menu-symbols.idc`

Short version: **the mechanism descriptions are good, the addresses are
not.** Whoever wrote it read the disassembly properly — every function
body I re-derived matched their prose. But three systematic errors mean
the file should not be applied to the IDB as-is.

### 5.1 Systematic error #1 — wrong gp (affects 13 of the ~50 entries)

The idc's "menu globals (gp-relative, absolute addrs)" section used
`gp = 0x282E1C`. The real gp is 0x2AF070 (§0). **All thirteen absolute
addresses in that section are wrong by −0x2C254**, and each one lands in
unrelated `.data`. The gp *offsets* in the comments are correct, so the
fix is mechanical:

| idc name | idc address | **correct address** |
|---|---|---|
| menuCurPacket (gp−29600) | 0x27BA7C | **0x2A7CD0** |
| menuDmaChan (gp−29640) | 0x27BA54 | **0x2A7CA8** |
| menuPktBufIdx (gp−29596) | 0x27BA80 | **0x2A7CD4** |
| menuInputState (gp−29592) | 0x27BA84 | **0x2A7CD8** |
| menuPktBusy (gp−29688) | 0x27BA24 | **0x2A7C78** |
| menuFlag1 (gp−29672) | 0x27BA34 | **0x2A7C88** |
| menuCounter121 (gp−29668) | 0x27BA38 | **0x2A7C8C** |
| menuFlag27 (gp−29664) | 0x27BA3C | **0x2A7C90** |
| menuElemIndex (gp−29496) | 0x27BAE4 | **0x2A7D38** |
| menuFlag2 (gp−29468) | 0x27BB00 | **0x2A7D54** |
| menuFlag3 (gp−29392) | 0x27BB4C | **0x2A7DA0** |
| menuState (gp−28748) | 0x27BDD0 | **0x2A8024** |

### 5.2 Systematic error #2 — "menu" is really two modules

The idc's `menu*` prefix lumps together Module U (main menu / system
configuration, 0x21C910–0x230000) and Module V (browser, 0x231000–0x24C068).
They are separate registered modules with separate threads, separate
texture sets (`TEXC*` vs `TEXB*`) and an almost disjoint call graph. Every
`menuPkt*`/`menuDrawElement`/`menuUploadTexture` symbol belongs to
**Module V only**; `menuScreenRenderer` (0x224630) and the widget cluster
belong to **Module U only**. Names should say `browser*` / `config*`.

### 5.3 Spot-check table

Fourteen entries checked against the disassembly.

| idc entry | tier | verdict |
|---|---|---|
| `osdGetString` 0x2041B8 | [ok] | **RIGHT** — including the id 85/86 special case at +340/+344 |
| `osdSelectStringTable` 0x204170 | [ok] | **RIGHT** in mechanism; its comment names the wrong cache address (0x26EDE0 → **0x26ECE0**) |
| `osdGetLanguage` 0x2037B8 | [ok] | **WRONG NAME** — it is a language *setter* that writes the config word; the getter is `GetLanguage` at 0x2040D0 (already named in the IDB) |
| `osdLanguageTables` 0x26ECC0 | [ok] | **RIGHT** (9 entries, slot 8 = slot 1) |
| `osdCurStringTable` 0x26EDE0 | [ok] | **WRONG ADDRESS** — should be 0x26ECE0; 0x26EDE0 is inside the resource table |
| `osdStringsEN` 0x298B08, `osdStringsJP` 0x2972A8, `osdStringsFR` 0x29A2A4 | [ok] | tables **RIGHT** (0x298B08, 0x2972A8); "FR" is at **0x29A0E8**, not 0x29A2A4 (that is inside the FR string block, not its pointer table). The `osdStringsTbl2..7` [tnt] names are all real tables but the index→language mapping is now known: 2=FR, 3=ES, 4=DE, 5=IT, 6=NL, 7=PT |
| `menuPktOpen` 0x2403C8 | [ok] | **RIGHT**, comment and all |
| `menuPktKick` 0x240438 | [ok] | **RIGHT**, comment and all (IRQ bit, buffer flip) |
| `menuPktFlagReset` 0x240038 | [ok] | **RIGHT** (`sw zero, gp−29672`) |
| `menuBrowserState` 0x240068 | [ok] | **RIGHT** — 0x1F0000A4 / 0x1F0000B0, states 5/8, gp−28748 0/1/2. Minor: it calls 0x2036B0 and 0x23BFD0; 0x23BFF0 is called from 0x240040, not here |
| `menuBrowserInput` 0x240178 | [ok] | **mechanism RIGHT, role WRONG** — it is the reset-combo watchdog and the "Reset" text overlay (string at 0x2A7C98), not general input |
| `menuDrawElement` 0x245038 | [ok] | **body RIGHT**; the caller list is **wrong** — the only caller is 0x2437A0 (4 call sites). 0x238228 and 0x248370 do not call it |
| `menuUploadTextureLoadImage` 0x246DE8 | [ok] | **RIGHT** except "file RPC 0x24dce0" — 0x24DCE0 is `FlushCache` (named in the IDB) |
| `menuScreenRenderer` 0x224630 | [ok] | **body RIGHT, role WRONG** — it is the **first-boot setup wizard** (strings 153–160: "Select language.", "Select time zone.", …), called once from the init hub 0x2283F0, not a general screen renderer |
| `configTitleRecords` 0x2A4380 | [tnt] | table is real; **stride is 16 bytes, not 32**, and it has no direct xref (reached via a base pointer), so its role stays unconfirmed |
| `menuPktCtxs` 0x295AD0, `menuElemGsData` 0x295CD0, `menuElemGifTag` 0x295D00, `menuElements` 0x295D10, `menuTexDescs` 0x296160, `menuElemVertTable` 0x284100 | mixed | **all RIGHT** (0x284100 is referenced by ten functions across both modules, so it is shared, not V-only) |
| `menuPacketInit` 0x240040, `menuPktBuilder` 0x240E68, `menuUploadTexturePair` 0x246F30 | [tnt] | functions exist but have **no callers** in the jal graph — dead or indirect; unverifiable as named |
| `menuScreenDraw1/2/3` 0x238228 / 0x2437A0 / 0x248370 | [tnt] | 0x2437A0 is genuinely the element-draw caller; the other two are **not** — they are separate draw paths that never reach 0x245038 |
| `menuOptionsStrings` 0x2051F8 | [tnt] | **RIGHT and then some** — it is the Version Information table builder, also reachable as ThreadY command 22 |

### 5.4 Overall

Roughly: of the entries I checked, **~60 % are correct as written**,
**~25 % are correct in substance but wrongly addressed or wrongly
named**, and **~15 % are unverifiable or wrong**. The prose in
`menu-rendering-reverse.md` is more reliable than the `.idc`, because the
prose quotes offsets and the idc converts them. Recommendation: keep the
write-up, regenerate the `.idc` with gp = 0x2AF070, split the `menu`
prefix into `cfg`/`browser`, and drop the four entries with no callers.

---

## 6. Reversing roadmap

### 6.1 Sizes

| module | funcs | code bytes | status |
|---|---|---|---|
| core (0x200000–0x211C70) | 302 | ~0x11C70 | partly named by aap |
| opening (0x211C70–0x21C910) | 118 | 0xACA0 | **done, matching in progress** |
| Module U — menu/config (0x21C910–0x230000) | 286 | 0x13708 | unnamed |
| shared renderer (0x230000–0x231000) | ~17 | 0x1000 | 5 named by aap |
| Module V — browser (0x231000–0x24C068) | 396 | 0x1C050 | unnamed |
| libraries (0x24C068–0x2678F0) | 425 | 0x1B888 | mostly named (SCE/newlib) |

### 6.2 Dependency order

```
                    ┌────────────────────────────────┐
                    │ core: config accessors,        │
                    │ string API, resources, modules │  ← everything needs this
                    └───────────┬────────────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
   text/font engine       event queue +           shared renderer
   (0x208130-0x20B600)    msg ring (0x2116D8)     (0x230000)
        │                       │                       │
        └───────────┬───────────┴───────────┬───────────┘
                    │                       │
              Module U (menu/config)   Module V (browser)
                                            │
                                       sceVif1Pk packet layer
                                       (0x2403C8 / 0x240438)
```

### 6.3 Recommended next targets, in order

1. **The config accessor cluster, 0x203570–0x203A10 (~0x4A0 bytes, 20
   tiny functions).** *Do this first.* It is completely self-contained
   (pure bit twiddling on one word plus two RPC calls), the semantics are
   already proven two ways (the accessors themselves and the
   `ConfigParam` converter at 0x2069C8), and every function is 6–24
   instructions. It is the cheapest possible win and it unblocks the
   config menu. Add `0x203390`/`0x2033F8`/`0x203220` (~0x180 bytes) for
   the NVM path.

2. **The module registry, 0x204408–0x204560 + 0x204618–0x2046FF +
   0x2051A8 (~0x300 bytes).** Also tiny, also self-contained, and it is
   the program's spine — having it named makes every subsequent module
   readable. Together with (1) this is a single small TU, call it
   `osdconf.c` / `module.c`.

3. **The string API + resource table, 0x2040D0–0x204240 and
   0x2052E0–0x205CE0.** `osdGetString`/`osdSelectStringTable`/
   `getResourcePtr`/`getResourceSize` are trivial; `loadResources` is 628
   instructions but is straight-line file/archive parsing with no
   graphics. Verifiable against the string tables dumped in §4.

4. **The event queue, 0x2116D8–0x211C50 (~0x578 bytes, 10 functions).**
   Self-contained ring buffer + registry, no hardware, no floats.
   Needed by both remaining modules.

5. **Module V's packet layer, 0x2403B8–0x240B48 (~0x790 bytes).** The
   `sceVif1Pk` wrappers are thin and already understood; this is the
   analogue of the opening's `spr*`/`vif1*` layer and is the gateway to
   any browser rendering work. GS-dump-verifiable the same way the
   opening was.

6. **The text/font engine, 0x208130–0x20B600 (~0x3500 bytes, ~80
   functions).** Shared by everything, and until it exists no menu screen
   can be shown. This is the first genuinely hard piece: escape parsing
   (§4.3), the `FNTEX*` extension fonts, and the texture upload path
   through 0x208E48. Budget accordingly.

7. **Module U's config screens.** Start at the model struct 0x352880 and
   its five toucher functions (0x22B0E8/138/2A8/3F8, 0x22C2A0), then walk
   outward through 0x2283F0's eleven initialisers.

8. **Module V's browser proper.** Start at the memcard command wrappers
   0x232F68–0x2334xx (mechanical, ~14 clones), then the file-list screens.

### 6.4 Expected hard parts

* **The font renderer** — the `0x87` extended-glyph escape and the `0x07`
  markup grammar are both undecoded, and the glyph data lives in
  compressed `FNT*` archives. Likely the single biggest time sink outside
  the renderer.
* **The 0x230000 renderer core** — custom VIF1 UNPACK chains poked
  straight at D1 MMIO, no libpkt. Same class of problem as the opening's
  VU1 pipeline, and it is shared by both modules so it cannot be skipped.
* **SIF RPC** is *not* a hard part: it is stock `sceSifCallRpc` and the
  library is already named. Same for `sceMc*` — the browser never touches
  it directly, only through ThreadY's command word.
* **The memcard "FS"** is likewise not a filesystem implementation, just
  RPC marshalling; the real work is the *result buffer layout*, which is
  still unmapped (§2.8).
* **The screen-id namespace** is global and shared between modules
  (106–116 in both the opening and Module U, plus 75/90 in Module V).
  Any port has to honour it or the modules will not hand off correctly.

### 6.5 Verification strategy

The existing pattern (port → GS dump / savestate diff → binary match)
transfers, with one addition: for the config subsystem the verification
target is not a GS dump but the **`ConfigParam` word produced by
0x2069C8** — it can be read out of a savestate and compared bit-for-bit
against a reference console's NVM, which is a far tighter check than any
visual diff.

---

## 7. Open questions / labelled gaps

* Consumer of the 20500/20501 message ring (§1.7).
* Roles of module-descriptor fields `+16`, `+20`, `+24` — all seven
  registered modules leave them NULL, so only the default stubs run.
* The `0x2A4380` record table's consumer and true record size.
* Module V's memcard directory result-buffer layout (§2.8).
* The `0x07` markup grammar and the `0x87` glyph-escape mapping (§4.3).
* Which of `0x240040`, `0x240E68`, `0x246F30` are dead code vs. reached
  through function pointers.
* `ThreadA` (0x20B830) and `ThreadB` (0x20F7C0) bodies — ThreadB is the
  largest single block in the core region (0x20BE60–0x211680) and is
  almost certainly the disc/title boot machinery, but I only confirmed
  its creation and that `main` passes it a 0x780000 work arena.
* The DVD Player module (#6, registrar 0x205158, setup 0x204FC8) — it has
  a `setup` but I did not trace what thread, if any, it creates.
