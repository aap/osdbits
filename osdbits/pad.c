/* NOT original: the port-0 controller.  The retail OSDSYS never touches
 * a pad library from Module U - its own pad thread (0x206E00) reads all
 * eight port/slot combinations through XPADMAN, merges the active-low
 * button bytes, applies the region swap (below), and publishes them to
 * 0x1F0C58..5B plus a state word at 0x1F0C78.  Each menu frame the tick
 * (0x21CF20) then calls the cooker 0x22BE30, which builds the canonical
 * word `~((b[0x1F0C5A]<<8) | b[0x1F0C5B])` - exactly libpad's bit
 * layout, the PAD_* enum in inc.h - and derives four globals from it:
 * held (gp-30320), press edges (gp-30316, the word 0x228278 and every
 * other menu consumer reads), release edges (gp-30312), and an UP/DOWN
 * repeat word (gp-30308, the clock editor's up/down).  UpdatePad below
 * reproduces that pipeline: pad.press == gp-30316, pad.dirPress ==
 * gp-30308 for up/down plus gp-30316's press edges for left/right.
 * osdbits boots as a bare ELF with nothing running on the IOP but the
 * boot ROM's own modules, so the whole IOP side has to be brought up
 * here.
 *
 * Lifted from ~/src/xtc/src/joy.c, aap's known-working pad layer, with
 * the module bookkeeping rewritten (see LoadPadModules).
 *
 * Two traps this path exists to avoid:
 *  - sceSifLoadModule cannot load out of the boot ROM.  libkernl asks the
 *    loadfile server for its version over a one-off fno 0xff RPC and
 *    refuses to go on unless the reply matches the SDK's own tag; the
 *    ROM's LOADFILE does not implement fno 0xff, replies 0, and every
 *    sceSifLoadModule against it comes back -SCE_EVERSIONMISS.  Same
 *    server, same wire protocol, no handshake: fno 0, arg length in word
 *    0, path at byte 8 (romLoadModule below).
 *  - the ROM's XPADMAN is padman 3.6 and imports sio2man 1.02, so it
 *    needs rom0:XSIO2MAN (rom0:SIO2MAN is 1.00 and the link fails), and
 *    the EE side must be libopad.a - the SDK 2.0 libpad, the last one
 *    that accepts a padman with major version 3.  Linking the current
 *    libpad.a here gives a library-version error at scePadInit. */

#include <stdio.h>
#include "inc.h"

#include <libopad.h>

Pad pad;

/* Honest state, kept apart on purpose (joy.c folds all three into one
 * optimistic flag): the modules being resident does not mean the pad
 * library came up, and neither means a pad is plugged in - that is
 * scePadGetState's answer and it changes every frame. */
static int padModules;		/* XSIO2MAN + XPADMAN are on the IOP */
static int padOpen;		/* scePadInit + scePadPortOpen said yes */
static int padModeSet;		/* analog asked for on this connection */

static u_long128 padDmaBuf[scePadDmaBufferMax] __attribute__((aligned(64)));

/* ===================== IOP modules (rom0:) ===================== */

#define LOADFILE_RPC_ID 0x80000006

static sceSifClientData loadfileCd __attribute__((aligned(64)));
static unsigned char loadfileBuf[512] __attribute__((aligned(64)));
static int boundLoadfile;

void
rpcDelay(int n)
{
	volatile int i;

	for(i = 0; i < n; i++);
}

/* the module id (>= 0), or a status word - see LoadPadModules.
 * Shared with sound.c, which pulls rom0:OSDSND through the same
 * server. */
int
romLoadModule(const char *module)
{
	if(!boundLoadfile) {
		for(;;) {
			if(sceSifBindRpc(&loadfileCd, LOADFILE_RPC_ID, 0) < 0)
				return -1;
			if(loadfileCd.serve)
				break;
			rpcDelay(0x10000);
		}
		boundLoadfile = 1;
	}

	memset(loadfileBuf, 0, sizeof(loadfileBuf));
	strncpy((char*)loadfileBuf+8, module, 252);
	if(sceSifCallRpc(&loadfileCd, 0, 0, loadfileBuf, 512, loadfileBuf, 8, nil, nil) < 0)
		return -2;
	return *(int*)loadfileBuf;
}

/* The loadfile reply is only half-diagnostic.  A fresh load returns the
 * module id, which is >= 0; the IOP's own failures come back as the
 * loadcore codes in kerror.h - KE_LINKERR (-200) for an unresolved
 * import (rom0:SIO2MAN in front of XPADMAN lands exactly here),
 * KE_ILLEGAL_OBJECT (-201), KE_UNKNOWN_MODULE (-202), KE_NOFILE (-203),
 * KE_FILEERR (-204), KE_NO_MEMORY (-400).
 *
 * The ambiguity: a module that is ALREADY resident does not fail, but
 * what the ROM's LOADFILE returns for it is not documented in the SDK
 * headers - the residency codes there (NO_RESIDENT_END = 1, sifdev.h)
 * are the module's own start-function results, not loadfile's, and
 * whether they reach the EE unchanged is unverified.  So: >= 0 is a
 * success, the known-fatal codes above are failures, and anything else
 * negative is reported and treated as "the module is there" rather than
 * bailing out - the pad library's own answer (scePadInit /
 * scePadPortOpen) is the real proof, and that is what InitPad returns. */
int
romLoadFatal(int r)
{
	return r == -1 || r == -2 ||		/* our own RPC failures */
	       r == -200 || r == -201 || r == -202 ||
	       r == -203 || r == -204 || r == -400;
}

static int
LoadPadModules(void)
{
	int r;

	/* order matters: XPADMAN imports sio2man 1.02, which only
	 * XSIO2MAN provides */
	r = romLoadModule("rom0:XSIO2MAN");
	printf("pad: rom0:XSIO2MAN -> %d\n", r);
	if(romLoadFatal(r))
		return 0;
	if(r < 0)
		printf("pad: XSIO2MAN status not a known error, assuming resident\n");

	r = romLoadModule("rom0:XPADMAN");
	printf("pad: rom0:XPADMAN -> %d\n", r);
	if(romLoadFatal(r))
		return 0;
	if(r < 0)
		printf("pad: XPADMAN status not a known error, assuming resident\n");

	return 1;
}

/* ========================= the pad itself ========================= */

#define PADDEADZONE 0.2f

static float
padAnalog(int x)
{
	float f = (x/255.0f - 0.5f)*2.0f;

	if(f > PADDEADZONE) return (f-PADDEADZONE)/(1.0f-PADDEADZONE);
	if(f < -PADDEADZONE) return (f+PADDEADZONE)/(1.0f-PADDEADZONE);
	return 0.0f;
}

/* Switch the pad into analog mode ourselves so nobody has to reach for
 * the ANALOG button.  scePadSetMainMode is an RPC to padman that is
 * refused (returns 0) until the pad state is Stable/FindCTP1, so it
 * cannot go in InitPad right after PortOpen - the SDK samples spin on it
 * with a vsync per retry.  UpdatePad runs every frame anyway, so poll
 * the state instead: on the first stable frame ask once - offs 1 picks
 * the analog entry of the mode table, lock 0 keeps the ANALOG button
 * alive.  One shot per connection: a refusal while stable means a pad
 * with no analog mode, and the ExecCmd/Stable dance during the switch
 * itself must not retrigger us.  Disconnect re-arms, so a re-plugged pad
 * is switched too. */
static void
SetAnalogMode(int state)
{
	if(state == scePadStateDiscon)
		padModeSet = 0;
	if(!padModeSet && (state == scePadStateStable || state == scePadStateFindCTP1)) {
		scePadSetMainMode(0, 0, 1, 0);
		padModeSet = 1;
	}
}

/* the four directions from the dpad and from the left stick, so both
 * drive the menu (the stick is a port amenity - the retail pad thread
 * reads the two raw button bytes only).  The stick threshold sits well
 * past PADDEADZONE: a resting stick must never step the cursor. */
#define PADSTICKDIR 0.5f

/* Read out of the retail image now (0x22BE98..0x22BF50, the tail of the
 * cooker 0x22BE30): only UP and DOWN repeat - left and right are press
 * edges only, everywhere in the menu.  Each of the two keeps a frame
 * counter that sits at -1 while released and counts up from 0 (the
 * press frame - count 0 IS the repeat word's press edge) while held; a
 * repeat fires on every count >= 31 that is divisible by 3, so the
 * first lands 33 frames after the press (~0.55 s NTSC) and then every
 * 3 (20 Hz). */
#define PADREPEATFIRST 31
#define PADREPEATMOD 3

static int padUpCount = -1;
static int padDownCount = -1;

/* the confirm/cancel region convention, the retail pad thread's publish
 * step (0x207024..0x207058): when 0x204318() says the console is not
 * Japanese (rom0:ROMVER's region letter - 'J' = 0, 'A'/'H' = 1,
 * 'E' = 2, negative/unknown treated as 0), the circle (0x20) and cross
 * (0x40) bits are exchanged in the raw button byte before the menu ever
 * sees it.  Bit 0x20 of the canonical word therefore always means "the
 * confirm button" - physical circle in Japan, physical cross everywhere
 * else - and 0x40 "the cancel button", which is what every matched menu
 * consumer tests (0x227BE8 confirms on 0x20).  osdbits has no ROMVER to
 * read; the region proxy is argv slot 16, the same one menutext.c's
 * textRegionSwap uses for the hint bar and osdGetString's 85<->86
 * exchange, so the button under the "Enter" hint is always the button
 * that enters.  Default 1 = the world arrangement.  Read lazily on the
 * first UpdatePad - InitPad runs before opening.c's ParseArgs has
 * located the numeric args. */
static int padRegionSwap = -1;

/* diagnostics: every frame while the pad is settling, then only when
 * something changes.  Kept in the build - a run with no pad and a run
 * with a pad that never leaves FindPad look identical otherwise. */
#define PADLOGFRAMES 240

static int padLogLeft = PADLOGFRAMES;
static int padLogState = -1;
static int padLogMode = -1;
static int padLogConn = -1;

static void
PadLog(int state, int mode)
{
	/* the warm-up logs every frame for PADLOGFRAMES - fine for a debug
	 * run, but on PCSX2 a per-frame SIF-console printf wrecks the frame
	 * rate, so gate on the debug arg (OsdArgInt(6), 0 on a normal boot).
	 * InitPad's one-time status lines are not gated and still print. */
	if(!osdTrace)
		return;
	printf("pad: state %d mode 0x%02x conn %d btns %04x dirs %04x "
		"L %d,%d R %d,%d\n",
		state, mode, pad.connected, pad.btns, pad.dirs,
		(int)(pad.lx*100.0f), (int)(pad.ly*100.0f),
		(int)(pad.rx*100.0f), (int)(pad.ry*100.0f));
}

void
UpdatePad(void)
{
	unsigned char rdata[32];
	int state, mode;
	u16 btns, dirs, rep;

	if(padRegionSwap < 0)
		padRegionSwap = OsdArgInt(16, 1);

	state = padOpen ? scePadGetState(0, 0) : scePadStateClosed;
	if(padOpen)
		SetAnalogMode(state);

	btns = 0;
	mode = 0;
	pad.lx = pad.ly = pad.rx = pad.ry = 0.0f;
	/* a pad is only really there once padman answers a read; the state
	 * alone still says Stable for a frame or two after an unplug.  The
	 * state gate is the retail one (0x206FD4): a read only counts while
	 * the pad is Stable or FindCTP1. */
	pad.connected = 0;
	if(padOpen &&
	   (state == scePadStateStable || state == scePadStateFindCTP1) &&
	   scePadRead(0, 0, rdata) > 0) {
		pad.connected = 1;
		mode = rdata[1];
		btns = 0xFFFF ^ ((rdata[2] << 8) | rdata[3]);
		/* the region swap - the retail thread's exact bit shuffle
		 * ((b & 0x9f) | (b & 0x20) << 1 | (b & 0x40) >> 1), done on
		 * the composed word instead of the raw active-low byte, which
		 * commutes with the inversion */
		if(padRegionSwap)
			btns = (btns & ~(PAD_CIRCLE|PAD_CROSS)) |
				((btns & PAD_CIRCLE) << 1) |
				((btns & PAD_CROSS) >> 1);
		/* 0x73 = analog mode, 16 bytes: the two sticks follow the
		 * button word.  0x41 (digital) has no stick bytes at all. */
		if(mode == 0x73) {
			pad.rx = padAnalog(rdata[4]);
			pad.ry = padAnalog(rdata[5]);
			pad.lx = padAnalog(rdata[6]);
			pad.ly = padAnalog(rdata[7]);
		}
	}

	pad.press = btns & ~pad.btns;
	pad.release = pad.btns & ~btns;
	pad.btns = btns;

	dirs = btns & (PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT);
	if(pad.ly < -PADSTICKDIR) dirs |= PAD_UP;
	if(pad.ly > PADSTICKDIR) dirs |= PAD_DOWN;
	if(pad.lx < -PADSTICKDIR) dirs |= PAD_LEFT;
	if(pad.lx > PADSTICKDIR) dirs |= PAD_RIGHT;
	/* the retail repeat word, bit for bit (see PADREPEATFIRST above);
	 * the counters key off the merged dirs so the stick repeats too.
	 * A fresh DOWN press STORES over an UP repeat due the same frame
	 * rather than ORing - the ROM's own quirk (sw at 0x22BF20), kept. */
	rep = 0;
	if(dirs & PAD_UP) padUpCount++; else padUpCount = -1;
	if(padUpCount == 0)
		rep = PAD_UP;
	else if(padUpCount >= PADREPEATFIRST && padUpCount % PADREPEATMOD == 0)
		rep |= PAD_UP;
	if(dirs & PAD_DOWN) padDownCount++; else padDownCount = -1;
	if(padDownCount == 0)
		rep = PAD_DOWN;
	else if(padDownCount >= PADREPEATFIRST && padDownCount % PADREPEATMOD == 0)
		rep |= PAD_DOWN;
	pad.dirPress = (dirs & ~pad.dirs & (PAD_LEFT|PAD_RIGHT)) | rep;
	pad.dirs = dirs;

	if(padLogLeft > 0 || state != padLogState || mode != padLogMode ||
	   pad.connected != padLogConn || pad.press || pad.release) {
		if(padLogLeft > 0)
			padLogLeft--;
		PadLog(state, mode);
	}
	padLogState = state;
	padLogMode = mode;
	padLogConn = pad.connected;
}

int
InitPad(void)
{
	/* nothing else in osdbits speaks to the IOP, so the RPC layer comes
	 * up here; a second call from a future caller is harmless */
	sceSifInitRpc(0);

	padModules = LoadPadModules();
	if(!padModules) {
		printf("pad: no IOP modules, running without a pad\n");
		return 0;
	}
	if(scePadInit(0) == 0) {
		printf("pad: scePadInit failed\n");
		return 0;
	}
	if(scePadPortOpen(0, 0, padDmaBuf) == 0) {
		printf("pad: scePadPortOpen(0,0) failed\n");
		return 0;
	}
	padOpen = 1;
	printf("pad: port 0 open\n");
	return 1;
}
