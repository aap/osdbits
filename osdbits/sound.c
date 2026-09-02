/* The OSD sound path.
 *
 * The real OSDSYS plays every sound on the IOP: rom0:OSDSND is Sony's
 * rspu2_driver 1.03 (a remoted libspu2 + libsnd2, RPC server number
 * 0x80000601 - ps2sdk's iop/sound/rspu2drv is a decompilation of it
 * with the whole St* command block #if 0'd out), loaded at IOP boot by
 * OSDCNF's IOPBTCONF.  The EE side is three layers, all ported here:
 *
 *  - OSDDispatch2 (real 0x261738): the RPC caller.  fno = the message
 *    number itself; the send packet is 64 bytes at 0x3f3240 - word 0
 *    scratch, words 1..6 the args - and the 16-byte reply lands in the
 *    same buffer, word 0 = the IOP function's return value.  Message
 *    classes the real function special-cases and this port does not
 *    need: 0x6xxx receives 64 bytes into the caller's pointer
 *    (SpuGet*Attr), 0x7xxx sends 64 bytes from the caller's pointer
 *    (SpuSet*Attr structs), 0x7600 sends args[2]*64 bytes (multi
 *    voice attr), 0x8xxx installs IOP-side callbacks and mirrors the
 *    value at 0x2967f0+.  Everything the OSD proper uses is the plain
 *    class: 6 word args out, one word back.
 *  - OSDDispatch (real 0x200b80): a 128-entry mailbox (real 0x1f05f8,
 *    four u16 arrays msg/a/b/c, write index 0x1f0a00, read index
 *    0x1f09fc, lock word 0x1f09f8).  The animation threads only ever
 *    enqueue; a periodic thread (real 0x206e00) drains the ring
 *    through OSDDispatch2(1, ...) - so a sound never blocks a frame.
 *  - the init suite (real 0x2004b8/0x200250/0x261a00, called from the
 *    real main at 0x207774): bind, SpuInit, StInit, upload the SND*
 *    resources, open 2 VABs + 8 BGM sequences, volumes, SetTimer.
 *
 * The message vocabulary (from the rspu2drv decompilation; St* fnos
 * confirmed against the EE call sites):
 *    0x0001 SpuInit         0x0002 SpuSetCore     0x1031 SpuSetDigitalOut
 *    20481 0x5001 StInit          20486 0x5006 StVabOpenFakeBody(hdr,spu)
 *    20487 0x5007 StVabOpenCompleted (transfer sync)
 *    20488 0x5008 StVabClose      20489 0x5009 StBgmOpen(vab,seq)->slot
 *    20490 0x500a StSetTickMode   20491 0x500b StBgmClose
 *    20492 0x500c StSetReverbType 20493 0x500d StSetReverbDepth
 *    20496 0x5010 StGetSlotStatus 20498 0x5012 StSetMasterVol
 *    20499 0x5013 StSetBgmVol     20500 0x5014 StBgmPlay(slot)
 *    20501 0x5015 StBgmStop(slot,?,fade)  20506 0x501a StDmaWrite(iop,spu,n)
 *    20736 0x5100 SetTimer        20737 0x5101 ReleaseTimer
 *    20992 0x5200 StSePlay(1,n)   20993 0x5201 StSetSeVol(vab,vol)
 *
 * The SND* resources (SNDIMAGE, all "SS" format - parsed by the IOP
 * module, opaque blobs here).  Slot and VAB ids are 0-BASED first-free
 * indices (read straight out of the ROM OSDSND module: StBgmOpen's
 * slot loop starts at record 0 of the 24 x 68-byte table and returns
 * the index; StInit clears the whole table first):
 *    SNDBOOTH/SNDBOOTB  VAB 0 header ("SShd") / body -> SPU2 0x5010:
 *                       the bank the eight sequences play from
 *    SNDOSDDH/SNDOSDDB  VAB 1 header / body -> SPU2 0x85010: the sound
 *                       effect bank - StSePlay(1, n) names it by id
 *    the eight "SSsq" sequences, StBgmOpen'd in this order = the
 *    record numbers the OSD's play/stop messages carry:
 *      0 SNDBOOTS vol 66  started by the disc/boot logic (0x211ff0),
 *                         the opening STOPS it with a fade (20501,0)
 *      1 SNDTNNLS vol 42  the opening's boot transition (20500,1)
 *      2 SNDCLOKS vol 45  the System Config clock screen (20500,2)
 *      3 SNDTM30S vol 27  timer 30s
 *      4 SNDTM60S vol 27  timer 60s
 *      5 SNDLOGOS vol 27
 *      6 SNDWARNS vol 54  stopped at opening end (20501,6 fade 15)
 *      7 SNDRCLKS vol 54  played for boot params 108-110 (20500,7)
 *
 * NOT original in shape: the real IOP is rebooted with OSDSND resident;
 * osdbits boots bare, so the module is loaded rom0: by hand through
 * pad.c's loadfile RPC, and the forever bind loop of the real 0x261a00
 * is bounded so a BIOS without OSDSND degrades to the old printf stubs
 * instead of hanging the ELF. */

#include <stdio.h>
#include <string.h>
#include <sif.h>	/* sceSifSetDma - the EE->IOP copies */
#include "inc.h"
#include "res.h"

#define OSDSND_RPC_ID 0x80000601

/* SPU2 RAM addresses the two VAB bodies live at (real 0x2004b8) */
#define SPU_VB1 0x5010
#define SPU_VB2 0x85010

/* one IOP heap block holds everything resident (real 0x26e084, size
 * 0x10000): +0 VAB1 header, +0x1000.. the sequences, +0x6000 VAB2
 * header; the same block is the staging buffer for the SPU uploads
 * during init, in 0x10000 chunks */
#define IOPBUFSZ 0x10000
#define CHUNKSZ 0x10000

static sceSifClientData sndCd __attribute__((aligned(64)));
/* real 0x3f3240: send 64 bytes, receive 16, word 0 = result */
static u32 sndPkt[16] __attribute__((aligned(64)));

static int sndBound;	/* the RPC server answered the bind */
static int sndUp;	/* the full init ran: banks uploaded, VABs open */

static u32 sndIopBuf;	/* the IOP heap block (real gp-8060 = 0x26e084) */

static int sndVab1, sndVab2;	/* real 0x26e09c / 0x26e09e */
static int sndBgm[8];		/* real 0x26e08c..0x26e09a */

/* .inc resource arrays carry no alignment promise and SIF DMA wants a
 * qword-aligned EE address, so every transfer bounces through here */
static u8 sndStage[CHUNKSZ] ALIGN16;

int soundSema = -1;

/* ================== the RPC (real 0x261738/0x261a00) ================== */

/* the plain message class: up to 3 word args, one word back.  mode 0 =
 * blocking - the real x=1 path, which is what every OSD call uses (x=0
 * would be NOWAIT with the end-callback at 0x2967c0; nothing ported
 * needs it). */
static int
SndRpc(int msg, int a, int b, int c)
{
	sndPkt[0] = (u32)sndPkt;	/* scratch, as the real one writes it */
	sndPkt[1] = a;
	sndPkt[2] = b;
	sndPkt[3] = c;
	sndPkt[4] = 0;
	sndPkt[5] = 0;
	sndPkt[6] = 0;
	if(sceSifCallRpc(&sndCd, msg, 0, sndPkt, 64, sndPkt, 16, nil, nil) < 0)
		return -1;
	return sndPkt[0];
}

void
OSDDispatch2(int x, int msg, int a, int b, int c)
{
	printf("osd: dispatch2(%d, %d, %d, %d, %d)\n", x, msg, a, b, c);
	if(sndBound)
		SndRpc(msg, a, b, c);
}

/* real 0x261a00: sceSifInitRpc + bind 0x80000601 (forever) + SpuInit.
 * InitPad already ran sceSifInitRpc; the retry here is bounded. */
static int
SndBind(void)
{
	int i;

	for(i = 0; i < 100; i++) {
		if(sceSifBindRpc(&sndCd, OSDSND_RPC_ID, 0) < 0)
			return 0;
		if(sndCd.serve)
			break;
		rpcDelay(0x10000);
	}
	if(sndCd.serve == nil)
		return 0;
	sndBound = 1;
	FlushCache(0);
	SndRpc(1, 0, 0, 0);	/* SpuInit */
	return 1;
}

/* ================= the mailbox (real 0x200b80/0x200a50) ================= */

#define SNDQLEN 128

/* real 0x1f05f8: four parallel u16 arrays, stride 8 */
static short qMsg[SNDQLEN], qA[SNDQLEN], qB[SNDQLEN], qC[SNDQLEN];
static volatile int qRead, qWrite;	/* real 0x1f09fc / 0x1f0a00 */
/* volatile or gcc deletes the set/clear pair as dead stores - the
 * whole point is that SoundThread sees it mid-update */
static volatile int qLock;		/* real 0x1f09f8 */

void
OSDDispatch(int msg, int a, int b, int c)
{
	int i;

	printf("osd: dispatch(%d, %d, %d, %d)\n", msg, a, b, c);
	qLock = 1;
	if(qWrite - qRead < SNDQLEN) {
		i = qWrite % SNDQLEN;
		qMsg[i] = msg;
		qA[i] = a;
		qB[i] = b;
		qC[i] = c;
		qWrite++;
	}
	qLock = 0;
}

/* real 0x2009b0, called with 60 after every drain: fade the reverb
 * depth in over the first 60 ticks (by QUEUEING StSetReverbDepth for
 * both cores - the messages go out on the following drains) */
static int rampCount;	/* real gp-8064 = 0x26e080 */

static void
SndReverbRamp(int fps)
{
	int depth, core;

	if(rampCount >= fps)
		return;
	depth = rampCount * 0x9ffec / 127 / fps;
	for(core = 0; core < 2; core++)
		OSDDispatch(20493, core, depth, depth);	/* StSetReverbDepth */
	rampCount++;
}

/* real 0x200a50: drain the ring through the RPC.  Runs in SoundThread
 * (the real one runs in the periodic thread at 0x206e00) so a blocking
 * RPC never stalls a render thread. */
static void
SoundUpdate(void)
{
	int i, j, w;

	if(!sndUp || qLock)
		return;
	w = qWrite;
	for(i = qRead; i < w; i++) {
		j = i % SNDQLEN;
		if(qMsg[j]) {
			OSDDispatch2(1, qMsg[j], qA[j], qB[j], qC[j]);
			qMsg[j] = 0;
			qA[j] = 0;
			qB[j] = 0;
			qC[j] = 0;
		}
	}
	qRead = w;
	SndReverbRamp(60);
}

/* ==================== data upload (real 0x200250) ==================== */

/* real 0x2000c0: FlushCache + sceSifSetDma + poll sceSifDmaStat -
 * a synchronous EE -> IOP copy of the (aligned) staging buffer */
static void
SndStageToIop(u32 dst, int size)
{
	sceSifDmaData dd;
	unsigned int id;

	dd.data = (u32)sndStage;
	dd.addr = dst;
	dd.size = size;
	dd.mode = 0;
	FlushCache(0);
	id = sceSifSetDma(&dd, 1);
	while(sceSifDmaStat(id) >= 0);
}

static void
SndCopyToIop(u8 *src, u32 dst, int size)
{
	int n;

	while(size > 0) {
		n = min(size, CHUNKSZ);
		memcpy(sndStage, src, n);
		SndStageToIop(dst, n);
		src += n;
		dst += n;
		size -= n;
	}
}

/* real 0x200130: a VAB body goes EE -> IOP staging -> SPU2 RAM in
 * 0x10000 chunks; each chunk is StDmaWrite + StVabOpenCompleted (the
 * transfer-done sync).  The real code moves a FULL chunk even for the
 * tail (reads past the source and writes stale staging bytes past the
 * sample); this port pads the tail chunk with zeroes instead - the
 * overshoot region is dead space either way (VB2's chunk ends well
 * before 0x95010, and VB1's six chunks end at 0x65010 < VB2). */
static void
SndSpuUpload(u8 *src, u32 iopBuf, u32 spuAddr, int size)
{
	int n;

	while(size > 0) {
		n = min(size, CHUNKSZ);
		memcpy(sndStage, src, n);
		if(n < CHUNKSZ)
			memset(sndStage+n, 0, CHUNKSZ-n);
		SndStageToIop(iopBuf, CHUNKSZ);
		OSDDispatch2(1, 20506, iopBuf, spuAddr, CHUNKSZ);	/* StDmaWrite */
		OSDDispatch2(1, 20487, 0, 0, 0);	/* StVabOpenCompleted */
		src += n;
		spuAddr += n;
		size -= n;
	}
}

static u8*
sndData(int resid)
{
	return GetResourceData(resid);
}

/* real 0x200250: the two VAB bodies to SPU2 RAM, then the headers and
 * sequences into the resident IOP block.  Order kept. */
static int
SndLoadData(void)
{
	static const struct {
		int resid;
		u32 off;
	} iopres[] = {
		{ RESID_SNDBOOTH, 0x0000 },	/* VAB 1 header */
		{ RESID_SNDBOOTS, 0x1000 },
		{ RESID_SNDTNNLS, 0x2000 },
		{ RESID_SNDCLOKS, 0x3000 },
		{ RESID_SNDTM30S, 0x4000 },
		{ RESID_SNDTM60S, 0x5000 },
		{ RESID_SNDLOGOS, 0x7000 },
		{ RESID_SNDWARNS, 0x8000 },
		{ RESID_SNDRCLKS, 0x9000 },
		{ RESID_SNDOSDDH, 0x6000 },	/* VAB 2 header */
	};
	int i;

	sceSifInitIopHeap();
	sndIopBuf = (u32)sceSifAllocIopHeap(IOPBUFSZ);
	if(sndIopBuf == 0) {
		printf("sound: no IOP heap\n");
		return 0;
	}
	if(sndData(RESID_SNDBOOTB) == nil || sndData(RESID_SNDOSDDB) == nil) {
		printf("sound: SND resources not loaded\n");
		return 0;
	}

	SndSpuUpload(sndData(RESID_SNDBOOTB), sndIopBuf, SPU_VB1,
		GetResourceSize(RESID_SNDBOOTB));
	SndSpuUpload(sndData(RESID_SNDOSDDB), sndIopBuf, SPU_VB2,
		GetResourceSize(RESID_SNDOSDDB));
	for(i = 0; i < 10; i++)
		SndCopyToIop(sndData(iopres[i].resid),
			sndIopBuf + iopres[i].off,
			GetResourceSize(iopres[i].resid));
	return 1;
}

/* ====================== init (real 0x2004b8) ====================== */

static u_char SoundStack[4096] ALIGN16;
static int SoundThreadID;

static void
SoundThread(void *arg)
{
	for(;;) {
		WaitSema(soundSema);
		SoundUpdate();
	}
}

/* called from the vblank handler - same refer/signal guard as the
 * draw semaphores in main.c */
void
SoundVblank(void)
{
	struct SemaParam sp;

	if(soundSema < 0)
		return;
	iReferSemaStatus(soundSema, &sp);
	if(sp.currentCount < sp.maxCount)
		iSignalSema(soundSema);
}

int
SoundInit(void)
{
	/* the eight sequences by IOP offset, in StBgmOpen (= slot) order:
	 * BOOTS TNNLS CLOKS TM30S TM60S LOGOS WARNS RCLKS */
	static const u32 seqoff[8] = {
		0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x7000, 0x8000, 0x9000
	};
	static const int seqvol[8] = { 66, 42, 45, 27, 27, 27, 54, 54 };
	struct ThreadParam tparam;
	struct SemaParam sparam;
	int r, i;

	/* rom0:OSDSND through the same loadfile RPC as the pad modules;
	 * requires InitPad's sceSifInitRpc to have run */
	r = romLoadModule("rom0:OSDSND");
	printf("sound: rom0:OSDSND -> %d\n", r);
	if(romLoadFatal(r)) {
		printf("sound: no OSDSND, dispatches stay printf-only\n");
		return 0;
	}
	if(r < 0)
		printf("sound: OSDSND status not a known error, assuming resident\n");

	if(!SndBind()) {
		printf("sound: rpc 0x%08x did not bind\n", OSDSND_RPC_ID);
		return 0;
	}

	SndRpc(20481, 0, 0, 0);		/* StInit */
	if(!SndLoadData())
		return 0;
	SndRpc(2, 1, 0, 0);		/* SpuSetCore(1) */
	SndRpc(20492, 0, 4, 0);		/* StSetReverbType(core, 4) */
	SndRpc(20492, 1, 4, 0);
	sndVab1 = SndRpc(20486, sndIopBuf, SPU_VB1, 0);	/* StVabOpenFakeBody */
	sndVab2 = SndRpc(20486, sndIopBuf+0x6000, SPU_VB2, 0);
	SndRpc(20490, 60, 0, 0);	/* StSetTickMode */
	for(i = 0; i < 8; i++)
		sndBgm[i] = SndRpc(20489, sndVab1, sndIopBuf+seqoff[i], 0);	/* StBgmOpen */
	for(i = 0; i < 8; i++)
		SndRpc(20499, sndBgm[i], seqvol[i], 0);	/* StSetBgmVol */
	SndRpc(20993, sndVab2, 28, 0);	/* StSetSeVol */
	SndRpc(20498, 0, 0x3fff, 0x3fff);	/* StSetMasterVol */
	SndRpc(20498, 1, 0x3fff, 0x3fff);
	SndRpc(20736, 0, 0, 0);		/* SetTimer - the sequencer tick */

	/* a healthy run prints vab 0/1 and bgm 0 1 2 3 4 5 6 7 - the
	 * 0-based first-free ids the IOP allocates; anything else means
	 * an open failed (-1) and the play/stop record numbers the OSD
	 * sends will hit the wrong sequences */
	printf("sound: up - vab %d/%d, bgm %d %d %d %d %d %d %d %d\n",
		sndVab1, sndVab2,
		sndBgm[0], sndBgm[1], sndBgm[2], sndBgm[3],
		sndBgm[4], sndBgm[5], sndBgm[6], sndBgm[7]);

	sparam.initCount = 0;
	sparam.maxCount = 1;
	soundSema = CreateSema(&sparam);

	tparam.entry = SoundThread;
	tparam.stack = SoundStack;
	tparam.stackSize = sizeof(SoundStack);
	tparam.initPriority = 2;	/* below the render threads: runs when
					 * they block, sleeps in the RPC */
	tparam.gpReg = &_gp;
	SoundThreadID = CreateThread(&tparam);
	StartThread(SoundThreadID, nil);

	sndUp = 1;
	return 1;
}
