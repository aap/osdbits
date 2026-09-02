#include <eekernel.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <libdev.h>
#include <eeregs.h>
#include <libgraph.h>
#include <libdma.h>
#include <libvu0.h>
#include <sifdev.h>
#include <sifrpc.h>

#include "inc.h"
#include "res.h"

int IsPAL(void) { return 0; }
int GetLanguage(void) { return 1; } // english

/* OSD-side glue the opening calls into, stubbed for the standalone
 * build (the real ones live in the OSD system module):
 * - discReady/discType (real 0x26ecf0/0x26ecf4, accessors 0x2043a8/
 *   0x2043b8): CDVD detection state, written by the disc-poll thread.
 * - bootLatch (real 0x26ecec; 0x204378 tests it for 0, set at
 *   0x224614, detailed meaning unknown).
 * - osdBootParam (real *(0x1f0010)): why OSDSYS was launched, values
 *   100..116 - steers the opening's boot messages and end conditions.
 *   osdBootParamC = *(0x1f000c), osdBootParam2 = *(0x1f0cf8) (the
 *   latter only checked for boot param 114).
 * - OSDDispatch (real 0x200b80) / OSDDispatch2 (real 0x261738):
 *   these turned out to be the SOUND path - the messages are RPC
 *   commands for rom0:OSDSND (20500 = StBgmPlay etc.) - and are now
 *   real, in sound.c. */
int discReady = 0;
int discType = 0;
int bootLatch = 0;
int osdBootParam = 100;
int osdBootParamC = 0;
int osdBootParam2 = 0;
int HasDisc(void) { return discReady != 0; }
int GetDiscType(void) { return discType; }
int BootLatchClear(void) { return bootLatch == 0; }

sceGsDBuff db;
int evenOddFrame;
int evenOddField;
int screenW, screenH;

int mainSema;
int waitFrameSema;
int drawEndSema;
int swapSema;
int drawStartSema;

void
InitDraw(void)
{
	screenW = 640;
	screenH = IsPAL() ? 256 : 224;
	// TODO: some unknown function

	// what is mode 2?
	sceGsResetGraph(2, SCE_GS_INTERLACE, IsPAL() ? SCE_GS_PAL : SCE_GS_NTSC, SCE_GS_FRAME);
	sceGsSetDefDBuff(UNCACHED(&db), SCE_GS_PSMCT32, screenW, screenH,
		SCE_GS_ZGEQUAL, SCE_GS_PSMZ32, SCE_GS_CLEAR);
	sceGsSwapDBuff(UNCACHED(&db), 0);
	sceGsSwapDBuff(UNCACHED(&db), 1);
	evenOddFrame = 1;
}

void
SwapBuffers(void)
{
	vif1Flush();	/* nothing may still be queued when the env flips */
	evenOddField = !((DGET_GS_CSR() >> GS_CSR_FIELD_O) & 1);
	evenOddFrame ^= 1;
	sceGsSetHalfOffset(evenOddFrame==0 ? UNCACHED(&db.draw0) : UNCACHED(&db.draw1),
		2048, 2048, evenOddField);
	sceGsSwapDBuff(UNCACHED(&db), evenOddFrame);
	sceGsPutDrawEnv(evenOddFrame==0 ? UNCACHED(&db.giftag0) : UNCACHED(&db.giftag1));
	/* drain the draw env + CLEAR before drawStartSema lets the frame
	 * draw: PutDrawEnv goes down PATH3, the LOWEST-priority GIF path,
	 * which yields at packet boundaries - on real hardware the frame's
	 * first chains (PATH1/2) overtake it, so the clear lands late and
	 * erases early draws.  (Emulators that complete DMA synchronously
	 * never show this.) */
	gsSyncPath();
}

void
SwapThread(void *arg)
{
	for(;;) {
		WaitSema(swapSema);
		SwapBuffers();
		SignalSema(drawStartSema);
	}
}

#define STACKSZ 8192
u_char SwapStack[STACKSZ] ALIGN16;
int SwapThreadID;

int MainThreadID;

int vblankCount;

int
vblankHandler(int id)
{
	struct SemaParam sp;

	vblankCount++;

	if(iPollSema(waitFrameSema) == waitFrameSema) {
		iReferSemaStatus(drawStartSema, &sp);
		if(sp.currentCount < sp.maxCount)
			iSignalSema(drawStartSema);
	}

	if(iPollSema(drawEndSema) == drawEndSema) {
		iReferSemaStatus(swapSema, &sp);
		if(sp.currentCount < sp.maxCount)
			iSignalSema(swapSema);
	}

	SoundVblank();	/* real: the drain runs off a periodic thread
			 * (0x206e00); here the vblank wakes it */

	// TODO: wakeup a thread?

	ExitHandler();
	return 0;
}

void
StartFrame(void)
{
	SignalSema(waitFrameSema);
	WaitSema(drawStartSema);

	vif1Flush();	/* ditto - the draw env goes down PATH3 */

	evenOddField = !((DGET_GS_CSR() >> GS_CSR_FIELD_O) & 1);
	sceGsSetHalfOffset(evenOddFrame==0 ? &db.draw0 : &db.draw1, 2048, 2048, evenOddField);
	sceGsPutDrawEnv(evenOddFrame==0 ? &db.giftag0 : &db.giftag1);
	gsSyncPath();		/* same PATH3 drain as SwapBuffers */
}

void
WaitNextFrame(void)
{
	/* drain PATH1/2/3 before declaring the frame done: the vblank-side
	 * SwapBuffers kicks the next draw environment (FRAME/ZBUF + CLEAR)
	 * via PATH3, and on real hardware the frame's rendering is
	 * otherwise still in flight at that point - late primitives land
	 * in the swapped buffer and the clear races the render. */
	gsSyncPath();
	SignalSema(drawEndSema);
	WaitSema(drawStartSema);
}

/* ELF launch arguments (crt0 passes the loader's _args block through) -
 * consumed by opening.c's SimulateBootHistory: argv[1] = seed,
 * argv[2] = number of games, argv[3] = number of boots. */
int gameArgc;
char **gameArgv;

int
main(int argc, char *argv[])
{
	struct ThreadParam tparam;
	struct SemaParam sparam;

	gameArgc = argc;
	gameArgv = argv;

	{
		int i;
		printf("osdsys: argc = %d\n", argc);
		for(i = 0; i < argc && i < 16; i++)
			printf("osdsys: argv[%d] = \"%s\"\n", i, argv[i] ? argv[i] : "(null)");
	}

	LoadResources();

	/* before the opening thread exists: InitPad brings up the SIF RPC
	 * layer and waits on the IOP's loadfile server, and nothing that
	 * runs after this point should be doing that mid-frame */
	InitPad();

	/* the real main inits sound early too (0x207774), well before the
	 * opening runs - the boot jingle message (20500) must find the
	 * banks already on the SPU */
	SoundInit();

	MakeOpeningThread();	// this is not quite accurate

	sparam.initCount = 0;
	sparam.maxCount = 1;
	mainSema = CreateSema(&sparam);
	waitFrameSema = CreateSema(&sparam);
	drawEndSema = CreateSema(&sparam);
	swapSema = CreateSema(&sparam);
	drawStartSema = CreateSema(&sparam);

	MainThreadID = GetThreadId();

	tparam.entry = SwapThread;
	tparam.stack = SwapStack;
	tparam.stackSize = STACKSZ;
	tparam.initPriority = 1;
	tparam.gpReg = &_gp;
	SwapThreadID = CreateThread(&tparam);
	StartThread(SwapThreadID, nil);

	InitDraw();

	AddIntcHandler(INTC_VBLANK_S, vblankHandler, -1);
	EnableIntc(INTC_VBLANK_S);
	ChangeThreadPriority(MainThreadID, 30);

	WaitSema(mainSema);
	for(;;)
		printf("main\n");

	return 0;
}
