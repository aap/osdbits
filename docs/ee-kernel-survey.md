# EE Kernel Survey — SCPH-39001 v1.60 (ROMGEN 2002-0207)

Target: scope the "XL unknown" from `docs/rom-survey.md` — a from-scratch open EE kernel
for a minimal PS2 boot ROM. Everything below is from static analysis of the `KERNEL`
file (93 256 B) extracted read-only from the local SCPH-39001 image, disassembled at
0x80000000 (`objdump -D -b binary -m mips:5900 -EL --adjust-vma=0x80000000`),
cross-checked against the PAL 30004R v1.60 image, ps2sdk (`/u/aap/othersrc/ps2sdk`,
HEAD f08e889f) and ps2tek. Addresses are facts about the layout, not Sony bytes; scratch
artifacts (extractor, analyzer, full per-syscall table `syscalls.txt`, call graph
`callers.txt`, disassembly) live in this directory only.

Bottom line up front: **the kernel is ~20 600 instructions (82 KB code + 8 KB
data/strings), of which only ~8 500 instructions (~34 KB) are the actual operating
system.** 33 % is the DECI2 debug manager (stub it), 30 % is SetGsCrt's video-mode
machinery (table-driven, well documented). The load-bearing ABI is small and now
precisely enumerable. Verdict: **L, not XL** — with the caveat that the effort lives in
behavioral fidelity, not volume.

---

## 1. Memory map (kernel's world, v1.60 USA)

### 1.1 Code/data image (copied verbatim from ROM to 0x80000000, entry 0x80001000)

| Range | Contents |
|---|---|
| 0x80000000–0x80000280 | Exception vectors: 0x000 TLB refill, 0x080 counter/perf, 0x100 debug, 0x180 common, 0x200 interrupt. 0x000 and 0x180 are the *same* 10-insn dispatcher: save t9 → 0x80015138, index table 0x80015100 by Cause.ExcCode, jump. 0x080/0x100 jump to 0x80013F3C (DECI2). 0x200 saves sp/ra/at to 0x800010C0/D0/E0, picks highest pending IP bit with `plzcw`, dispatches via 0x80015140. |
| 0x80000280 | Syscall dispatcher (see §2) |
| 0x80000380 / 0x800004C0 / 0x80000600 | INT0 (INTC) / INT1 (DMAC) / IP7 (COP0 timer) interrupt bodies |
| 0x80000740–0x80000D80 | Tiny syscalls: SetPgifHandler, SetCPUTimerHandler/SetCPUTimer, _EnableIntc/_DisableIntc/_EnableDmac/_DisableDmac, SetVTLBRefillHandler/SetVCommonHandler/SetVInterruptHandler, CpuConfig, PSMode, MachineType, GetMemorySize, GsGetIMR/GsPutIMR, Exit, peek/poke helpers, SetSyscall (0x800006C0) |
| 0x80000D80–0x80001560 | Exit, interrupt context save/restore (static area), default "handler does not exist"/panic stubs (print + hang) |
| 0x80001560–0x80002040 | undefined-syscall stub (0x80001564), SetVSyncFlag, Add/Remove/Enable/Disable-Intc/Dmac-Handler, AddSbusIntcHandler, Interrupt2Iop, user-handler eret-trampolines |
| 0x80002040–0x800028C0 | Alarm machinery (_SetAlarm 0x800022D8, _ReleaseAlarm 0x80002570, T3+INTC12), alarm reset, RFU005 resume (0x80002880), count-getter stubs |
| 0x800028C0–0x80002F80 | EnableCache/DisableCache/KSeg0 (executed via kseg1 mirror: table entries 0xA00028C0/0xA0002980/0xA0002C00), FlushCache (0x80002A40), GetCop0 (206-insn switch) |
| 0x80002F80–0x80005390 | **Threads + semaphores + scheduler** (§4): ExecPS2 0x80002F80, thread syscalls, context switch, SetupThread/SetupHeap/EndOfHeap |
| 0x80005390–0x80005C38 | EELOAD/EENULL loader-from-ROMDIR (0x80005390), LoadExecPS2 0x800055A0, _ExecOSD 0x80005990, RFU009 TLB-random-write 0x80005BD0 |
| 0x80005C38–0x80006240 | TLB: _InitTLB 0x80005C38 + tlbwi helper 0x80005C10 |
| 0x80006240–0x800073E8 | **SIF**: init 0x80006240, SifSetDChain 0x80006348, SifStopDma 0x80006380, SifSetDma 0x800067A0, SifDmaStat 0x80006958, SifSetReg 0x80006C08, SifGetReg 0x80006CC0, kprintf core 0x80006E90 |
| 0x800073E8–0x80007588 | kprintf varargs shim 0x800073E8, _print syscall 0x800074D0, ROMconf reader 0x80007508 (reads ptr at 0xBC0003C0 → SystemConfiguration @0x80022448) |
| 0x80007588–0x8000BFB8 | **GS video**: DVE/video-encoder serial helpers (0x80007588/0x7700/0x7828/0x7D60, incl. writes to 0xBF803218 gated on EE/GS revision byte), giant per-mode CRTC programmer 0x80008C30 (3 298 insns) |
| 0x8000BFB8–0x8000D798 | SetGsCrt entry 0x8000BFB8 (742), _GetGsDxDyOffset 0x8000CB50, GetGsHParam/GetGsVParam/SetGsHParam/SetGsVParam, Get/SetOsdConfigParam(2) (cache @0x800223C0/0x80022xxx), RFU059 stub |
| 0x8000D798–0x8000E000 | ResetEE 0x8000D798 + subsystem hard-init (the "# Initialize DMAC/VU1/VIF1/GIF/VU0/VIF0/IPU/GS/INTC/TIMER/FPU/User Memory/Scratch Pad" walk, "# Restart" paths), ROMDIR find 0x8000D5F0 / lookup 0x8000D688 |
| 0x8000E000–0x80014D00 | **DECI2 manager** ("EE DECI2 Manager version 0.06"), DCMP/KTTY protocols, kernel TTY, EE-UART KTTY variant, Deci2Call handler 0x80013FDC, SIF2 DMA handler 0x80013F90, default exception handler 0x80013EA0 |
| 0x80014D00–0x80015200 | **Dispatch table block** (§2.2 — the central ABI object) |
| 0x80015200–0x80016C48 | Data: CPU-timer default handler ptr @0x80015200 (=0x80001538), initial SR @0x80015204 (0x70030C13) and Config @0x80015208 (0x00073003), user-callback ptr @0x80015264, LoadExecPS2 arg area @0x80015388, TLB tables @0x80015488/558/678, TLB descriptor @0x800156F8, all strings, DECI2 tables |

Instruction budget by subsystem (trimmed of padding; total 20 618):

| Subsystem | insns | bytes | share |
|---|---:|---:|---:|
| DECI2/DCMP/KTTY/TTY + debug handlers | 6 903 | 27 612 | 33.5 % |
| GS video (SetGsCrt + DVE + CRTC + params) | 6 276 | 25 104 | 30.4 % |
| Threads/semaphores/scheduler/ctx | 2 269 | 9 076 | 11.0 % |
| SIF | ~1 100 | 4 400 | 5.3 % |
| Exception layer + INTC/DMAC/timer dispatch | ~1 000 | 4 000 | 4.9 % |
| Exec family + EELOAD loader + ROMDIR | 552 | 2 208 | 2.7 % |
| Alarms + eventflag stubs + cache ops | ~900 | 3 600 | 4.4 % |
| TLB | 386 | 1 544 | 1.9 % |
| Everything else (tiny syscalls, init, kprintf) | ~1 200 | 4 800 | 5.8 % |

### 1.2 Kernel RAM (all physical < 0x00100000; user mappings start at vaddr 0x00080000)

| Address | Object |
|---|---|
| 0x800010C0–0x800012C8 | Static interrupt context save area (sp/ra/at from vector; v0–t9, gp, s8, hi/lo/hi1/lo1/sa from 0x80001300 helper). Single buffer — interrupts never nest. |
| 0x80016C80 | MachineType value (word read from ROM 0xBFC001F8 at boot; 0 on retail) |
| 0x80016C90 | GetMemorySize value (word read from scratchpad 0x70003FF0 at boot — RESET/RDRAM leaves size there; 0x2000000 = 32 MB) |
| 0x80018CC0 | **Kernel stack base** (grows down; used by positive syscalls, INT0/INT1/IP7 dispatch). Also a small globals block: +0 misc (returned by syscall 0x53), +4/+8 VSync flag ptrs (SetVSyncFlag) |
| 0x80019AE8 | Alarm allocation bitmap (u64 → 64 alarms) |
| 0x80019AF0 | Alarm count (returned by syscall 0x52) |
| 0x80019B00 | Alarm array, 20 B/entry × 64; sorted-order byte array @0x80019FF8; user gp/handler/target fields inside |
| 0x8001A040/0x8001A050 | Saved kernel ra/sp across user-callback excursions (restored by syscall ±5) |
| 0x8001A060 | Thread count (returned by syscall 0x50) |
| 0x8001A068 | TCB free-list head (doubly linked) |
| 0x8001A070 | 129 ready-queue heads (8 B each; priorities 0–128; EENULL idle = 128) |
| 0x8001A480 | **TCB array: 256 × 0x4C bytes** (id = (node−0x8001A480)/0x4C; fields: +8 status, +0xC entry/EPC, +0x10 stack, +0x48 heap_end …) |
| 0x8001F080 | **Semaphore array: 256 × 0x20 bytes** (id = (node−0x8001F080)>>5); free head @0x8001A47C, count @0x8001A478 |
| 0x80021080 | SIF command buffer (SIF_MAINADDR written = this), SREG array @0x800210FC–0x8002117C (32 regs) |
| 0x800223C0 | OsdConfigParam cache (bit-packed ConfigParam + u16 timezone; filled via SetOsdConfigParam) |
| 0x80022448 | SystemConfiguration (0x26 bytes copied from ROMconf via pointer at 0xBC0003C0; byte 0 = EEGS revision — gates the 0xBF803218 DVE writes in SetGsCrt) |
| 0x80023050 | DECI2 stack; DECI2 BSS up to ~0x80024xxx |
| phys 0x78000–0x80000 | DECI2 work area, TLB-mapped at **0xFFFF8000** (kernel TLB entry #1) |
| 0x80074000/75000/76000 | Conventional patch-blob landing zones used by Sony SDK libkernel and ps2sdk (`osdsrc`→0x80074000, `tlbsrc`→0x80075000, `srcfile`→0x80076000) — kernel itself doesn't use them; keep them free |
| 0x00081FC0 | **EENULL** (loaded from ROMDIR by kernel init): idle loop; **0x00081FE0** = user-mode callback dispatcher (see §3.3); ps2sdk's alarm patch drops a replacement at 0x00082000 |
| 0x00082000 | **EELOAD** raw binary loaded here (entry 0x00082000, BSS to 0x95624); initial thread, priority 0, stack top 0x00081000 |

TLB layout (tables at 0x80015488 kernel[13] / 0x80015558 default[18] / 0x80015678
extended[8], descriptor {counts, wired, ptrs} at 0x800156F8): **byte-identical to the
tables in ps2sdk `ee/kernel/src/tlbfunc.c`** — scratchpad @0x70000000 (entry 0, "spad=0"),
DECI2 32 KB @0xFFFF8000→phys 0x78000, I/O 0x10000000-block, VU @0x11000000, GS
@0x12000000, ROM 32 MB @0x1E000000; user RAM 0x00080000–0x02000000 cached, mirrored
uncached @0x20000000 and UCAB @0x30100000. Wired = 31. >48 entries → "# TLB over flow"
panic. `_InitTLB` prints "# TLB spad=0 kernel=1:%d default=%d:%d extended=%d:%d".

---

## 2. The dispatcher and the table block — the heart of the ABI

### 2.1 Syscall dispatcher (0x80000280)

```
if (v1 < 0)      v1 = -v1; goto negative_path        # bltzl+negu
if (v1 == 0x7C)  j 0x80013FDC                        # Deci2Call intercepted pre-table
positive path:
  sq sp/ra/at → 0x800010C0/D0/E0
  SR &= ~0x1B (IE,EXL,KSU=kernel)                    # ints hard-off, kernel mode
  sp = 0x80018CC0 (fixed kernel stack); push ra, old sp, EPC+4
  jalr  *(0x80014D00 + (v1<<2))                      # NO bounds check
  EPC restored; SR |= 0x13; eret
negative path (0x8000032C):
  stays on CALLER's stack, SR untouched (EXL=1 masks ints)
  jalr  same table; eret
```

Load-bearing consequences:
- **One kernel stack.** A positive syscall from an interrupt handler would clobber the
  interrupt's own frames — that is *why* the i-variants exist and must be issued with
  negative numbers. An open kernel must keep the two entry flavors with exactly these
  stack/SR semantics.
- **No bounds check.** v1 beyond the installed entries jumps through zero-filled table
  slots (crash) or, ≥0x100, through the *following* tables (exception/interrupt handler
  pointers). Don't "fix" this with validation — SetSyscall-installed high entries
  (0xFC–0xFF) rely on the unchecked indexing.
- **Instruction-layout contract**: ps2sdk/FMCB-class software finds the table by reading
  the dispatcher's own code: `table = (insn[0x800002F0] << 16) | (insn[0x800002F8] & 0xFFFF)`
  (`ee/kernel/src/getkernel.c: InitSyscallTable`). So at 0x800002F0 there must be a
  `lui` with the table's hi16 in the low halfword, and at 0x800002F8 a `lw` with the
  lo16 offset — and hi16 must be the *unadjusted* upper half (lo16 < 0x8000; keep the
  table in the first 32 KB of its 64 KB lui window).
- Similarly **GetExceptionHandler** parses the *code of syscall 0x0D's handler*: hi16
  from word at handler+0x14, lo16 from handler+0x20 (→ exception table 0x80015100). The
  first 9 instructions of SetVTLBRefillHandler are ABI.

### 2.2 The dispatch-table block at 0x80014D00 (one contiguous object)

| Offset from table base | SetSyscall index | Contents |
|---|---|---|
| +0x000–0x3FC | 0x00–0xFF | Syscall pointers (0x00–0x82 populated; 0x83–0xFF zero, writable via SetSyscall) |
| +0x400–0x437 | 0x100–0x10D | Exception handlers by ExcCode (all 0x80013EA0 except [8]=0x80000280 syscall) + t9 save slot @+0x438 |
| +0x440–0x47C | 0x110–0x11F | V_INTERRUPT dispatch by IP bit: [2]=0x80000380 INT0, [3]=0x800004C0 INT1, [7]=0x80000600 timer |
| +0x480–0x4BC | 0x120–0x12F | INTC per-cause handlers (15 causes; default 0x800014D8 = print "# INTC(%d) Handler does not exist." + hang) |
| +0x4C0–0x4FC | 0x130–0x13F | DMAC per-channel handlers (default 0x80001508, same pattern) |
| +0x500 | — | CPU-timer handler ptr (0x80015200), then initial SR/Config words |

**SetSyscall (0x74) is five instructions**: `sll a0,2; lui; addu; sw a1,0x4D00(v1); jr` —
signed, unchecked, unit = 4 bytes, relative to table base. This is the entire kernel-side
patch API, and Sony's own SDK blobs use it aggressively:
- `SetSyscall(0x12C, h)` overwrites the **INTC cause-12 (TIM3) handler** — ps2sdk
  `srcfile` does exactly this for the alarm fix (so the block layout at +0x480 is ABI).
- `SetSyscall(0xFFFFC402, jal_insn)` writes a raw JAL **into ExecPS2's code** at
  table−15358×4 (on the SCPH-10000 protokernel, whose table is at **0x80011F80**, that
  lands on 0x80002F88). Gated by ROM version checks in the patchers, but it proves the
  patch model: *table-relative pokes of both pointers and instructions*.

### 2.3 Cross-version reality check (measured)

| | USA v1.60 (0160AC…0207) | PAL v1.60 (0160EC…1004) | SCPH-10000 v1.00/1.01 |
|---|---|---|---|
| Syscall table | 0x80014D00 | **0x80014E80** | **0x80011F80** (per Sony patch-blob arithmetic in ps2sdk libosd.c) |
| ExecPS2 | 0x80002F80 | 0x80002F80 | 0x80002F80 (patch target 0x80002F88 = its 3rd insn) |
| CreateThread / CreateSema / SetSyscall / SifSetDma / LoadExecPS2 | 0x80003C50 / 0x800049C0 / 0x800006C0 / 0x800067A0 / 0x800055A0 | identical | (low code historically stable) |
| ResetEE / SetGsCrt | 0x8000D798 / 0x8000BFB8 | 0x8000D900 / 0x8000BA08 | — |
| syscall 0x82 (_InitTLB) | present | **absent (0)** | absent |

The low ~30 KB of the kernel is **address-stable across builds** (USA/PAL diff in the
first 30 000 bytes: 682 bytes, essentially all relocated hi/lo fields pointing at the
moved data block); the GS-mode and DECI2 regions are what shift. That is exactly why the
homebrew world can both (a) parse the dispatcher to find the table and (b) hardcode
mid-kernel addresses on specific ROM versions. An open kernel gets maximal compatibility
by cloning the *low-region* layout of one reference version and keeping the two
instruction-parse contracts of §2.1.

---

## 3. Mechanism walkthroughs

### 3.1 Boot (entry 0x80001000)

1. Save `*(u32*)0x70003FF0` → memory size; `*(u32*)0xBFC001F8` → machine type.
2. sp = 0x80018CC0; `_InitTLB` (48-entry program from the three tables; wired=31).
3. Init INTC/DMAC handler-chain pools; hardware init walk (prints the `# Initialize …`
   sequence: DMAC, VU1, VIF1, GIF, VU0, VIF0, IPU, GS, INTC, TIMER, FPU, user memory,
   scratchpad).
4. Thread/sema pool init (256+256, free lists, 129 ready queues); load **EENULL** from
   the ROM's ROMDIR (scan 0xBFC00000–0xBFC10000 for `RESET/ROMDIR/EXTINFO`, else
   "# panic ! dir not found") to 0x00081FC0, create as priority-128 idle thread.
5. SR ← 0x70030C13, Config ← 0x00073003; DECI2 init; SIF init (MAINADDR ← 0x80021080,
   MSFLAG ← 0x10000, start SIF0 chain).
6. User-callback dispatcher ptr (0x80015264) ← 0x00081FE0; VSync flags cleared.
7. Load **EELOAD** (raw binary, not ELF) from ROMDIR to 0x00082000; create thread
   (entry 0x82000, stack-top 0x81000, priority 0); mark running; `eret` into it in user
   mode. EELOAD then loads `rom0:OSDSYS` (or whatever `LoadExecPS2` asked for).

`LoadExecPS2(path, argc, argv)`: copies path+args into 0x80015388, resets the calling
thread (priority 0), re-loads EELOAD from ROMDIR to 0x82000 and enters it — EELOAD does
all actual ELF loading and the `rom0:UDNL` IOP-reboot argv convention. `ExecPS2(entry,
gp, argc, argv)` just rewrites the current context and erets. `_ExecOSD` = LoadExecPS2
("rom0:OSDSYS"); **`Exit()` boots "rom0:OSDSYS" with argv[0]="BootBrowser"**. `ResetEE`
re-runs the subsystem init walk ("# Restart…" strings) with a mask argument.

### 3.2 Interrupts

- INT0: read INTC_STAT & INTC_MASK (0xB000F000/F010), pick **highest** set cause with
  plzcw, **ack in STAT before the handler runs**, call table[+0x480] entry with a0 =
  cause. Quirk: VU0/VU1 interrupt bits (0xC0) while `cfc2 vi29` shows VPU busy (0x202)
  divert to the debug handler 0x80013EA0.
- INT1: same with D_STAT (0xB000E010), stat & (mask|0x8000); **channel 7 (SIF2) is
  DECI2's own** and diverts to 0x80013F90 before the generic path; DMA-error bit 0x8000
  included.
- IP7: COP0 Count/Compare — Count ← 0, Compare rewritten to ack; calls ptr @0x80015200
  (SetCPUTimerHandler/SetCPUTimer syscalls 0x6C/0x6D manage this; default handler prints
  "# INT: CPU Timer" and hangs).
- After any handler: if reschedule flag 0x80015374 was set (by iWakeupThread etc.),
  fall into the thread dispatcher 0x8000363C instead of eret.
- Handler *chains*: AddIntcHandler/AddDmacHandler allocate nodes (next/handler/arg/gp)
  into per-cause linked lists; Enable/DisableIntcHandler toggle without removing;
  kernel-side trampoline invokes **user-mode** handlers via §3.3.

### 3.3 User-mode callback contract (EENULL's second half)

The kernel never calls user handler code in kernel mode. It saves kernel ra/sp
(0x8001A040/50), sets EPC = *0x80015264 (default 0x00081FE0), puts the handler address
in **v1** and args in a0…, and erets to user mode. The 16-byte dispatcher at 0x81FE0:
`sp = 0x00081FC0; jalr v1; v1 = -5; syscall` — i.e. return to kernel via **syscall −5**
(RFU005/ResumeIntrDispatch, 0x80002880 restores ra/sp and resumes interrupt exit).
ps2sdk's replacement (`ee/kernel/src/eenull`, installed at 0x00082000 by the alarm
patch) is identical except it returns via **syscall −8** and its handler is installed as
syscall 8 by the patch. So: 0x00081FC0 idle / 0x00081FE0 dispatcher / syscall ±5
resume / v1-carries-callback are all ABI, and syscalls 5 and 8 must exist as
resume-points (ROM v1.60 has 5; 8 is the RFU the patches fill).

### 3.4 Threads and semaphores

Priority scheduler, 129 queues (0–128), current tid @0x8001536C, scan-start hint
@0x80015370. Context switch = full 128-bit GPR set + f0–f31 + hi/lo/hi1/lo1/sa pushed as
a 640-byte frame on the *thread's own stack* (0x80003680 save / 0x80003800 restore /
0x80001460 zero-all for fresh threads). Positive thread syscalls run the "may reschedule
now" path; i-variants only queue state changes and set 0x80015374. Several i-variants
are literally the same pointer as the plain call (iReferThreadStatus, iSuspendThread,
iCancelWakeupThread, iPollSema, iReferSemaStatus); others are distinct entries
(iWakeupThread, iSignalSema, iChangeThreadPriority, iRotateThreadReadyQueue,
iReleaseWaitThread, iTerminateThread, iResumeThread, iDeleteSema).

Known behavioral warts to preserve:
- **SuspendThread does not force a reschedule** — a thread suspending itself keeps
  running (ps2tek documents this).
- **iWakeupThread is buggy** enough that Sony worked around it with negative
  GetThreadId (`__NR__iGetThreadId = -0x2f`, per ps2sdk syscallnr.h comment).
- **DisableDispatchThread/EnableDispatchThread**: print "not supported in this version"
  and return the current thread id. Not implemented, and must not be.
- **SetupHeap(HEAP_RELATIVE)**: prints "# SetHeap: HEAP_RELATIVE is not supported…".
- ReferThreadStatus copies out of the live TCB; TCB/sema id ↔ array-index equations
  (§1.2) leak into ReferSemaStatus/ReferThreadStatus results.
- No JoinThread: 0x3B is an RFU that (bizarrely) reads video-encoder register 0x87.

### 3.5 Alarms

_SetAlarm(ticks16, handler, arg) (0x18; −0x1E for i): 64-slot bitmap, EE **Timer 3**
(0xB0001800 block) as the time base, insertion-sorted pending list, INTC cause 12
(TIM3) fires the walk, callbacks via §3.3. The high-numbered aliases **0xFC/0xFD/0xFE/
0xFF (SetAlarm/iSetAlarm/ReleaseAlarm/iReleaseAlarm) do not exist in ROM v1.60** —
they're zero slots that Sony's later libkernel and ps2sdk's `InitAlarm()` fill at app
startup (`srcfile` blob → 0x80076000 + SetSyscall 0xFC…0xFF, 0x12C, 0x08), because
"ReleaseAlarm is unable to correctly release alarms in all CEX/DEX EE kernels" (ps2sdk
alarm.c). T3_MODE bit 8 doubles as "ROM alarm subsystem active" — the patch refuses to
install if T3 already runs. An open kernel: implement 0x18/0x19 bug-for-bug *or* ship
correct 0x18/0x19 plus the 0xFC-family natively — but never break SetSyscall-based
replacement, since every SDK-built game and every ps2sdk homebrew will still install
its own.

### 3.6 SIF

Init: MAINADDR (0xB000F200) ← 0x80021080, MSFLAG (0xB000F220) ← 0x10000 (SIFINIT flag),
0xB000F260 ← 0xFF, clear SIF0/SIF1 CHCR, start SIF0 receive chain. SifSetDma builds
DMA tags and manages a software queue (uses D_ENABLER/W 0xB000F520/F590 around SIF1
CHCR); SifDmaStat polls completion by transfer id; SifSetReg/SifGetReg: 1=MAINADDR,
2=SUBADDR, 3=MSFLAG, 4=SMFLAG, negative = 32-entry software SREG array
(0x80000000|n convention — n≥0x80000000 indexes SREG). The id format returned by
SifSetDma and the exact SREG semantics are relied on by sifcmd/sifrpc in every game;
ps2sdk's `ee/kernel/src/sifcmd.c` + iop-side counterparts are the reference.

### 3.7 DECI2 / TTY (the stub-me third)

Whole block 0x8000E000–0x80014D00 (+ the 0xFFFF8000 work area + own stack): DECI2
manager 0.06 with DCMP and KTTY protocols over SIF2 (DMAC ch.7, hooked ahead of the
generic DMAC path), plus an EE-UART KTTY for TOOL hardware, plus the default
exception/interrupt reporters ("# INT: …", "# Syscall: undefined (%d)", register dumps)
and `Deci2Call` (0x7C, intercepted in the dispatcher; ±number split like syscalls;
manipulates state through the 0xFFFF8000 mapping). Kernel printf (0x800073E8 → core
0x80006E90) funnels here — on retail hardware output goes nowhere. Retail games don't
depend on DECI2 beyond `Deci2Call` returning sanely and `_print` (0x75) not crashing.

### 3.8 GS video (the other stub-me-almost third)

SetGsCrt(interlace, mode, ffmd) = 742-insn front end + 3 298-insn per-mode CRTC
programmer (SMODE1/SMODE2/SRFSH/SYNCH1/SYNCH2/SYNCV per NTSC/PAL/480p/1080i/VESA…) +
bit-banged video-encoder (DVE) helpers with model checks on the EEGS revision byte
(0x80022448, from ROMconf @0xBC0003C0) including writes to 0xBF803218 with spin delays.
GetGsHParam/GetGsVParam/SetGsHParam/SetGsVParam/_GetGsDxDyOffset tweak the same state;
GsGetIMR/GsPutIMR are 4/7-insn CSR pokes. All of this is table-driven register
programming, fully documented across ps2tek GS docs, sp193's OSD init code, and gsKit —
tedious, not risky. PAL/NTSC differences here are why the two v1.60 builds diverge.

---

## 4. Named syscall table (v1.60 USA; handler / direct insns / with-callees insns)

Full machine-generated version in `syscalls.txt`. Ratings: **T**rivial (<20 insns),
**S**mall (<100), **M**edium (<300), **L**arge.

| nr | name | handler | insns | closure | rating |
|---|---|---|---:|---:|---|
| 0x00,0x03,0x08,0x3F,0x54–0x5B,0x7C(table) | RFU → undefined-stub 0x80001564 | prints "# Syscall: undefined (%d)" then wedges | 5 | 399 | T |
| 0x01 | ResetEE | 0x8000D798 | 72 | 936 | M (subsystem re-init walk) |
| 0x02 | SetGsCrt | 0x8000BFB8 | 742 | 1 492* | L (*closure excludes the 3.3 K mode-setter reached indirectly) |
| 0x04 | Exit | 0x80000D80 | 2 | — | T (tail into ExecOSD-BootBrowser) |
| 0x05 | RFU005/ResumeIntrDispatch | 0x80002880 | 11 | 11 | T |
| 0x06 | LoadExecPS2 | 0x800055A0 | 146 | 3 560 | M |
| 0x07 | ExecPS2 | 0x80002F80 | 16 | 3 619 | S |
| 0x09 | RFU009 (TLB-write-random for vaddr 0x04xxxxxx) | 0x80005BD0 | 15 | 15 | T |
| 0x0A/0x0B | Add/RemoveSbusIntcHandler | 0x80001E78/0x80001ED0 | 12 | 28 | T |
| 0x0C | Interrupt2Iop | 0x80001F70 | 12 | 40 | T |
| 0x0D/0x0E/0x0F | SetVTLBRefill/VCommon/VInterruptHandler | 0x800009C0/A00/A40 | 12 | 12 | T (**instruction layout is ABI**, §2.1) |
| 0x10–0x13 | Add/Remove Intc/Dmac handler | 0x80001A30… | 16–96 | ≤758 | S |
| 0x14–0x17 (=0x1A–0x1D) | _En/DisableIntc/Dmac | 0x800008C0… | 12 | 12 | T |
| 0x18/0x19 (=0x1E/0x1F) | _SetAlarm/_ReleaseAlarm | 0x800022D8/0x80002570 | 166/158 | 174 | M (buggy by spec, §3.5) |
| 0x20–0x3A | thread family | 0x80002FC0–0x80004970 | 4–108 | ≤842 | the whole subsystem is M/L in aggregate |
| 0x3B | RFU059 (reads DVE reg 0x87) | 0x8000CE50 | 8 | 416 | T |
| 0x3C/0x3D/0x3E | SetupThread/SetupHeap/EndOfHeap | 0x80005198/0x800052A0/0x800052D8 | 66/14/46 | ≤440 | S |
| 0x40–0x49 | semaphore family | 0x800049C0… | 14–96 | ≤842 | M aggregate |
| 0x4A/0x4B | Set/GetOsdConfigParam | 0x8000D310/0x8000D260 | 44 | 44 | T (bit-packed cache @0x800223C0) |
| 0x4C–0x4F | Get/SetGsH/VParam | 0x8000CBC8… | 22–164 | ≤4 912 | S–M |
| 0x50/0x51/0x52/0x53 | undocumented count-getters: thread count / sema count / alarm count / word@0x80018CC0 | | 4 | 4 | T |
| 0x5C–0x5F | En/DisableIntc/DmacHandler | 0x80001FA0… | 9 | 9 | T |
| 0x60/0x61/0x62 | KSeg0/EnableCache/DisableCache | **kseg1 entries** 0xA0002C00/0xA00028C0/0xA0002980 | 9–41 | | S (must run uncached) |
| 0x63(=0x67) | GetCop0 | 0x80002C40 | 206 | 206 | M (big switch) |
| 0x64(=0x68) | FlushCache | 0x80002A40 | 16 | 40 | S |
| 0x65(=0x69) | FlushCache-variant | 0x80002B40 | 40 | 40 | S |
| 0x66(=0x6A) | CpuConfig | 0x80000A80 | 55 | 55 | S |
| 0x6B | SifStopDma | 0x80006380 | 12 | 12 | T |
| 0x6C/0x6D | SetCPUTimerHandler/SetCPUTimer | 0x800007C0/0x80000800 | 4/45 | | T/S |
| 0x6E/0x6F | Set/GetOsdConfigParam2 | 0x8000D1F0/0x8000D158 | 28/38 | | T |
| 0x70/0x71 | GsGetIMR/GsPutIMR | 0x80000D00/0x80000D40 | 4/7 | | T |
| 0x72/0x73 | SetPgifHandler/SetVSyncFlag | 0x80000740/0x80001588 | 5/6 | | T |
| 0x74 | **SetSyscall** | 0x800006C0 | 6 | 6 | T (**semantics are the whole patch ABI**) |
| 0x75 | _print | 0x800074D0 | 14 | 14 | T |
| 0x76–0x7A | SifDmaStat/SetDma/SetDChain/SetReg/GetReg | 0x80006958… | 12–116 | ≤360 | M aggregate |
| 0x7B | _ExecOSD | 0x80005990 | 20 | 3 580 | T |
| 0x7C | Deci2Call | 0x80013FDC (dispatcher intercept) | — | — | subsystem |
| 0x7D/0x7E/0x7F | PSMode/MachineType/GetMemorySize | 0x80000B80/BC0/C40 | 6/21/46 | | T |
| 0x80 | _GetGsDxDyOffset | 0x8000CB50 | 30 | 30 | T |
| 0x82 | _InitTLB | 0x80005C38 | 122 | 528 | M (absent in PAL v1.60 and protokernel!) |
| 0x83–0xFF | zero (0xFC–0xFF conventionally patched in; 0x83+/0x85+ only exist on later/DESR kernels) | | | | |

---

## 5. Must-preserve ABI (the bug-compat question, answered concretely)

**Hard ABI — break any of these and shipped software breaks:**

1. **Syscall numbering 0x00–0x82** incl. the negative-number convention and the
   stack/SR split of §2.1, RFU no-op behavior for 0x00/0x03/0x08/0x3F/0x54–0x5B (they
   must exist and return; note Sony's stub *prints and wedges* — a benign
   return-0 is the safe open choice, since old crt0.s files call 3 and 63 at startup),
   and i-variant aliasing.
2. **SetSyscall(0x74)**: signed 4-byte-unit table-relative unchecked write. Plus
   FlushCache(0)/(2) and GetMemorySize working before any patch runs (every `_InitSys`
   calls them).
3. **The dispatch-table block layout**: syscalls +0x000, exception +0x400, interrupt
   +0x440, INTC +0x480 (index 0x12C = TIM3!), DMAC +0x4C0, CPU-timer ptr +0x500. All
   SetSyscall-index arithmetic in Sony/ps2sdk patch blobs assumes it.
4. **Instruction-parse contracts**: `lui` @0x800002F0 + `lw` @0x800002F8 encoding the
   table address; SetVTLBRefillHandler's `lui` @+0x14 + `sw` @+0x20 encoding the
   exception-table address.
5. **User-callback protocol**: eret-to-*(0x80015264) in user mode with callback in v1,
   default dispatcher at **0x00081FE0** (EENULL blob at 0x00081FC0), return via syscall
   −5; syscall 8 patchable as alternate resume; kernel must tolerate the dispatcher
   pointer/blob being replaced (patch writes new code at 0x00082000 and beyond).
6. **Patch landing zones free**: 0x80074000–0x80078000 must be unused kernel RAM;
   0x00082000+ becomes patch/dispatcher space after LoadExecPS2 hands off (EELOAD is
   dead by then).
7. **Alarm contract**: 0x18/0x19 on T3+INTC12; T3_MODE bit 8 as the "alarm active"
   sentinel; 0xFC–0xFF *empty* (or at minimum SetSyscall-replaceable); INTC-12 chain
   slot replaceable via index 0x12C.
8. **Memory map**: user RAM visible at 0x00080000–0x02000000 cached + 0x20000000
   uncached + 0x30100000 UCAB; scratchpad at 0x70000000; kernel owns phys <0x00080000;
   EENULL/EELOAD placement 0x81FC0/0x82000; GetMemorySize == 0x02000000 on retail.
9. **Boot handoff**: LoadExecPS2 semantics (arg copy, EELOAD reload, IOP-reboot argv
   conventions downstream), Exit → OSDSYS BootBrowser, ExecPS2 register/mode contract.
10. **SIF**: MSFLAG bit 0x10000 init handshake, SifSetDma id format + queueing,
    SifSetReg/GetReg register map incl. 0x80000000|n software registers, SifStopDma.
    (The IOP side of the handshake is already fixed by ps2sdk's modules.)
11. **SetGsCrt** mode results (games chain into GsPutIMR + their own GS setup; the
    *visible* CRTC output matters, per-instruction fidelity doesn't).

**Soft/nice-to-have (needed only for version-sniffing software):** the stability of
low-region function addresses (ExecPS2 0x80002F80 etc.) — FMCB-era tools mostly parse
rather than hardcode, but protokernel-targeted game patches poke absolute addresses
after checking ROM version; an open ROM should report a ROMVER those patchers *don't*
match, which sidesteps the issue entirely.

**Explicitly NOT ABI** (free to relayout): everything above 0x80005E00 code-wise
(GS/DECI2/ResetEE region — it moves between Sony builds anyway), the exact table
address (parsers follow the instructions — but keep it stable once chosen), internal
TCB/sema field layouts beyond what Refer*Status returns, string addresses, kernel stack
placement, DECI2 internals.

---

## 6. Effort & implementation order

| # | Subsystem | Size in ROM | Effort | Rationale |
|---|---|---|---|---|
| 1 | Exception/dispatch layer + table block + SetSyscall | ~1.3 K insns | **S** | Fully understood, mostly mechanical; the two instruction-parse contracts are the only subtlety. Do first — everything hangs off it. |
| 2 | Tiny-syscall bag (0x0A–0x17, 0x5C–0x73, 0x7D–0x80, GsIMR, CpuConfig, GetCop0, caches) | ~1.5 K | **S** | Registers + one big COP0 switch; EnableCache/DisableCache must run from kseg1. |
| 3 | TLB (_InitTLB + tables + RFU009) | 0.4 K | **S** | Tables already public in ps2sdk tlbfunc.c (byte-identical). Optionally implement 0x55–0x5B natively (tlbsrc is the spec) while keeping them patchable. |
| 4 | Threads + semaphores + scheduler + context switch | 2.3 K | **L** | The real work: exact ready-queue/priority semantics, i-variant deferral via 0x80015374, single-kernel-stack discipline, 640-byte frames, the documented warts (SuspendThread no-dispatch, iWakeupThread bug, missing JoinThread). Emulator HLE kernels (Play! CPS2OS) + ps2tek are the spec; needs a game-driven test matrix. |
| 5 | INTC/DMAC handler chains + user-callback trampoline + EENULL | 0.7 K | **M** | Protocol is pinned (§3.3); ack-before-handler ordering and highest-bit priority matter for timing-sensitive games. |
| 6 | Alarms (0x18/0x19 on T3/INTC12) | 0.5 K | **M** | Small but semantics-dense (16-bit wrap, sorted list, in-callback Release). Decide early: bug-compatible vs. correct+patch-tolerant. |
| 7 | SIF syscalls | 1.1 K | **M** | Must interlock with ps2sdk IOP modules; SifSetDma queue/id semantics need hardware validation (classic "works in PCSX2, hangs on HW" territory). |
| 8 | Exec family + ROMDIR loader + boot init | ~1.2 K | **M** | LoadExecPS2/ExecPS2/Exit/_ExecOSD + EENULL/EELOAD placement; co-designed with the (separate) EELOAD deliverable; libosdinit already documents the downstream sequence. |
| 9 | SetGsCrt + GS params | 6.3 K | **M** | Bulky but table-driven; ps2tek GS + sp193 OSD-init + gsKit cover every mode. DVE/0xBF803218 quirks only matter on specific board revisions — gate like Sony does on the ROMconf EEGS byte. |
| 10 | DECI2/KTTY/TTY | 6.9 K | **S (stub)** | Replace with: Deci2Call returning error codes, _print/kprintf → EE UART or /dev/null, default handlers that dump state and hang. Full DECI2 only ever mattered on TOOLs. |
| 11 | OSD config params (0x4A/0x4B/0x6E/0x6F) + ROMconf read | 0.2 K | **S** | Bit-packing already public in ps2sdk osd_config.h / osdsrc. |

Aggregate: **~9–10 K instructions of real kernel to write** (rest is stubs/tables).
As a C-plus-asm project: the asm core (vectors, dispatchers, context switch, cache/TLB
ops) is ~1.5 K lines; the rest is ordinary C against a fixed data layout.

**Recommended order**: 1 → 2 → 3 → 8(boot half) → 4 → 5 → 6 → 9(minimal NTSC/PAL modes)
→ 7 → 10(stub) → 11 → then breadth-test: ps2sdk homebrew first (it exercises the patch
ABI hardest via `_InitSys`), then retail games era-by-era. PCSX2 boots arbitrary 4 MB
ROMs, so the whole loop runs emulator-first; the SIF and DVE bits are the two places
that must be re-validated on the TOOL.

**Biggest de-risking insight from this survey**: the bug-compat surface is *not* "match
90 KB of Sony code" — it's §5's eleven bullet points. Sony themselves treated the kernel
as patchable firmware (every SDK-era game boots by rewriting syscalls through
SetSyscall), which means the *patch mechanism*, not the patched code, is the contract.
An open kernel that gets §5 exactly right can be internally clean.

---

## 7. Sources

- ps2sdk (local: /u/aap/othersrc/ps2sdk): `ee/kernel/include/syscallnr.h` (numbering),
  `src/getkernel.c` (table-parse contract), `src/tlbfunc.c` (TLB tables, 0x80075000),
  `src/alarm.c` + `src/srcfile/` (alarm patch, 0x80076000, index 0x12C, syscall 8),
  `src/libosd.c` + `src/osdsrc/` (protokernel table 0x80011F80, ExecPS2 patch at
  0x80002F88, OSD config), `src/eenull/` (0x81FC0/0x81FE0 dispatcher contract) —
  <https://github.com/ps2dev/ps2sdk>
- ps2tek EE Syscalls (negative-syscall semantics, SuspendThread bug):
  <https://israpps.github.io/ps2tek/PS2/BIOS/EE_Syscalls.html>
- ps2tek BIOS/Boot Process/EE docs: <https://israpps.github.io/ps2tek/> and
  psi-rockin original: <https://psi-rockin.github.io/ps2tek/>
- OSD-Initialization-Libraries (boot sequence downstream of the kernel):
  <https://github.com/ps2homebrew/OSD-Initialization-Libraries>
- jimmikaelkael/ps2-protokernel-patch (SCPH-10000 patch practice):
  <https://github.com/jimmikaelkael/ps2-protokernel-patch>
- Play! emulator HLE kernel (behavioral spec for threads/semas):
  <https://github.com/jpd002/Play-/blob/master/Source/ee/PS2OS.cpp>
- rom-survey notes: `docs/rom-survey.md` in the osdsys repo (ROMDIR format, boot chain,
  IOP coverage).
