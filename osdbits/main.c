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
#include <libpad.h>

#include "inc.h"
#include "res.h"

int IsPAL(void) { return 0; }
int GetLanguage(void) { return 1; } // english

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
	evenOddField = !((DGET_GS_CSR() >> GS_CSR_FIELD_O) & 1);
	evenOddFrame ^= 1;
	sceGsSetHalfOffset(evenOddFrame==0 ? UNCACHED(&db.draw0) : UNCACHED(&db.draw1),
		2048, 2048, evenOddField);
	sceGsSwapDBuff(UNCACHED(&db), evenOddFrame);
	sceGsPutDrawEnv(evenOddFrame==0 ? UNCACHED(&db.giftag0) : UNCACHED(&db.giftag1));
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

	// TODO: wakeup a thread?

	ExitHandler();
	return 0;
}

void
StartFrame(void)
{
	SignalSema(waitFrameSema);
	WaitSema(drawStartSema);

	evenOddField = !((DGET_GS_CSR() >> GS_CSR_FIELD_O) & 1);
	sceGsSetHalfOffset(evenOddFrame==0 ? &db.draw0 : &db.draw1, 2048, 2048, evenOddField);
	sceGsPutDrawEnv(evenOddFrame==0 ? &db.giftag0 : &db.giftag1);
}

void
WaitNextFrame(void)
{
	SignalSema(drawEndSema);
	WaitSema(drawStartSema);
}

int
main()
{
	struct ThreadParam tparam;
	struct SemaParam sparam;

	LoadResources();

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
