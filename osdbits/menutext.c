/* Reconstruction of the OSDSYS main menu's 2D text layer: the shared OSD
 * text engine (0x207E00-0x20B600) far enough to draw ASCII, Module U's
 * three alignment wrappers on top of it (0x21DC28/0x21DC88/0x21DD28),
 * and the main-menu item list itself (0x228110) with its fade-in and
 * its selection highlight.
 *
 * This is a SECOND, unrelated engine to menu.c's 3D scene.  The scene
 * builds GIF packets out of matrices; this one is pure screen space: a
 * pen position, a colour, a scale, and one textured SPRITE per glyph
 * out of a font page that lives in GS VRAM.  Nothing here transforms
 * anything (docs/menu-draw.md 3, 6).
 *
 * Where the font actually comes from (this was the surprise):
 * `do_load_font` (0x21DBA0) hands 0x209FD0 the FONTM resource, but that
 * is only the *kanji* path - the glyphs OSDSYS draws in a Latin locale
 * come from 0x20A3C8, which uploads resources 2..5 (FNTASCII, FNTEX000,
 * FNTEX001, FNTEXOSD) straight into VRAM as plain 4-bit indexed
 * bitmaps with one shared 16-entry CLUT.  FNTASCII is 256x480, a grid
 * of 8 columns x 12 rows of 32x40 cells holding characters 32..128, and
 * the per-glyph {left inset, advance} pairs are static .data at
 * 0x26FE60.  So there is no font decoder to port at all - it is a
 * texture and a table.  tools/extract-res.py --tables writes both out
 * (res/FONTDATA.inc), and --container FNTIMAGE writes the page.
 *
 * What is here: the ASCII half of 0x209640 (draw) and 0x209998
 * (measure), the glyph emitter 0x2086A0, the scale/pen/colour setters,
 * the centred wrapper 0x21DC88, the main menu's draw loop 0x228110,
 * its open/close state machine 0x228050 and its alpha 0x227E18.
 * What is NOT: the two-byte (Shift-JIS) path and its VRAM glyph cache
 * (0x208460's kinds 1..3, 0x208C60's 72-slot LRU), the 0x07 escape
 * sequences beyond skipping them, the button hint bar (0x21D7F8), the
 * clock (0x21D3A0), and every screen other than the main menu.
 *
 * Extra argv (menu mode), continuing menu.c's list:
 *     main.elf menu hh mm ss framelimit fromOpening fadeAlpha
 *                   debugFrame [cursor [notext]]
 * cursor (default 0) picks the item the highlight STARTS on; the pad
 * (pad.c, MainMenuInput below) moves it from there.  notext = 1
 * suppresses the whole 2D layer (for the orbs-only regression run);
 * textDump is a frame number at which to read the item band back out of
 * GS memory and print it at 2x2 px per character, fine enough to read
 * the glyph shapes (menu.c's DumpFrameAscii is 8x8, too coarse for
 * text).  Both are diagnostics, not in the ROM. */

#include <stdio.h>
#include "inc.h"
#include "res.h"

/* the text engine's static tables, read out of the retail image by
 * tools/extract-res.py --tables:
 *   fontAsciiMetrics[97][2]  real 0x26FE60
 *   fontClut[16]             real 0x2715E0
 *   osdStringTable[299]      real 0x298B08 (the English table; the real
 *                            osdGetString 0x2041B8 indexes whichever
 *                            language table 0x26ECE0 points at) */
#include "res/FONTDATA.inc"

/* real: 0x2041B8, minus its two regional swaps (ids 85/86 trade places
 * when 0x204318() says so) */
static const char *
osdGetString(int id)
{
	if((u32)id >= (u32)osdStringTableLen || osdStringTable[id] == nil)
		return "";
	return osdStringTable[id];
}

/* ===================== the font page (0x20A3C8) =====================
 *
 * Slot 0 of the four-page table at 0x271578: FNTASCII, 256x480, PSMT4,
 * TBW 4, TW 8, TH 9, CLUT PSMCT32/CSM1 - which is exactly what
 * osdbits' InitTexture()/vif1SetTexture() already build for the
 * opening's PSMT4 logos, so the whole upload path is reused.  The ROM
 * binds it with TCC 1, MODULATE, and TEX1 = 97 (LCM 1, MMAG/MMIN both
 * LINEAR); vif1SetTextureMIP's non-mipmap path is the same pair. */

#define FONTW 256
#define FONTH 480
#define CELLW 32		/* real: the `col << 5' in 0x2086A0 */
#define CELLH 40		/* real: the `row * 40' in 0x2086A0 */
#define CELLCOLS 8		/* real: the divisor 8 for kind 0 */
#define NASCII 97		/* real: the `(u32)(c-32) < 97' bound */

static Texture fontTexture = {
	nil, RESID_FNTASCII, fontClut, 3, { 0, 0, FONTW, FONTH },
	0, 0, SCE_GS_PSMT4, 0, { 0 }
};

/* ==================== the text engine's pen state ====================
 *
 * Real addresses, all in the engine's own scratch (0x271xxx / 0x2DDCxx):
 *   0x271858 / 0x27185C  pen x / y, stored << 4          (0x207E98)
 *   0x2DDC20..0x2DDC2C   pen colour r,g,b,a              (0x208110)
 *   0x2DDC40             base scale                      (0x2080D0)
 *   0x2DDC44 / 0x2DDC48  the scale                       (0x207F68)
 *   0x2DDC50             the glyph's drawn height << 4   (0x207F68)
 *   0x271860             per-glyph gap, set to -3 by 0x209FD0
 *   0x271864             baseline bias, set to -7<<3 = -56 by 0x209FD0
 *   0x27156C             advance multiplier, 1 in .data
 *   0x271560 / 0x271568 / 0x271870  the escape-sequence overrides
 *                        (fixed width, y shift, y bias) - always 0 here */
static int textPenX, textPenY;			/* real 0x271858/0x27185C */
static int textColR, textColG, textColB, textColA;	/* real 0x2DDC20.. */
static float textBaseScale = 1.0f;		/* real 0x2DDC40 */
static float textScale = 1.0f;			/* real 0x2DDC44/0x2DDC48 */
static int textGlyphH16;			/* real 0x2DDC50 */
static int textGap = -3;			/* real 0x271860 */
static int textYBias = -7*8;			/* real 0x271864 */
static int textAdvMul = 1;			/* real 0x27156C */

/* real: 0x207F68.  The ROM's chain of truncations matters - the height
 * is (int)(scale*16) rounded to a whole pixel BEFORE the 1.25 and the
 * base scale are applied, so 1.0 gives exactly 320 (20 px) on NTSC and
 * 368 (23 px) on PAL, not 400 and 460. */
static void
osdTextSetScale(float scale)
{
	int h;

	textScale = scale;
	h = (int)(scale*32.0f*0.5f) << 4;
	textGlyphH16 = (int)((float)h * 1.25f * textBaseScale);
}

/* real: 0x2080D0 - set the base scale, then re-derive from the current
 * scale.  do_load_font calls it with 1.15 on PAL, 1.0 on NTSC, which
 * stretches the glyphs vertically only (the width uses `scale' alone). */
static void
osdTextSetBaseScale(float base)
{
	textBaseScale = base;
	osdTextSetScale(textScale);
}

/* real: 0x208110 */
static void
osdTextSetColor(int r, int g, int b, int a)
{
	textColR = r; textColG = g; textColB = b; textColA = a;
}

/* real: 0x207E98 */
static void
osdTextSetPos(int x, int y)
{
	textPenX = x << 4;
	textPenY = y << 4;
}

/* real: 0x208540 - one glyph's advance, in whole pixels.  (The ROM
 * indexes the table unguarded here, unlike the emitter 0x2086A0 which
 * bounds-checks; a byte outside 32..128 that is not a Shift-JIS lead
 * therefore reads past the table in the ROM.  Guarded here.) */
static int
osdGlyphAdvance(int c)
{
	int w;

	if((u32)(c-32) >= (u32)NASCII)
		return 0;
	w = fontAsciiMetrics[c-32][1];
	return (int)(textScale * (float)w * (float)textAdvMul);
}

/* ================== the glyph emitter (0x2086A0) ==================
 *
 * The ROM emits one PACKED GIF packet per glyph, PRE=1 with
 * PRIM = SPRITE|TME|FST|ABE and REGS = {RGBAQ, UV, XYZ2, UV, XYZ2}
 * (templates at 0x2A3C20 / 0x2A3C30, the latter chosen because
 * 0x209FD0 arms the blend flag at 0x271868).  osdbits' packet layer
 * writes A+D pairs instead; same registers, same order, one more qword.
 *
 * Coordinates: the pen is already in 1/16 px, and the ((4096-w)/2)<<4
 * term the ROM adds is exactly the XYOFFSET_1 the draw environment
 * subtracts, so screen (0,0) is the top-left.  The vertical half-texel
 * insets (+8 / +632 over a 40-row cell) are the ROM's, not mine. */
static void
osdDrawGlyph(int c)
{
	int g, col, row;
	int x0, y0, x1, y1;
	int u0, u1;
	int xoff16, yoff16;
	int w16;

	g = c - 32;
	if((u32)g >= (u32)NASCII)
		return;
	col = g % CELLCOLS;
	row = g / CELLCOLS;

	xoff16 = ((4096 - screenW)/2) << 4;
	yoff16 = ((4096 - screenH)/2) << 4;

	x0 = textPenX + xoff16;
	y0 = textPenY + yoff16 + (int)(textScale * (float)textYBias);
	w16 = (int)(textScale * (float)(fontAsciiMetrics[g][1]*textAdvMul)) << 4;
	x1 = x0 + w16;
	y1 = y0 + textGlyphH16;

	u0 = col*CELLW + fontAsciiMetrics[g][0];
	u1 = u0 + fontAsciiMetrics[g][1];

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(textColR, textColG, textColB, textColA, 0x3f800000));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(u0*16 + 8, row*CELLH*16 + 8));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(x0, y0, 1));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(u1*16 - 8, row*CELLH*16 + 632));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(x1, y1, 1));
	vif1End();
}

/* ===================== the string walkers =====================
 *
 * 0x209640 (draw) and 0x209998 (measure) are the same loop twice over,
 * classifying each byte: 0x07/0x09/0x0A open an escape sequence
 * (0x209300 - colour, scale, fixed width, ...); a Shift-JIS lead byte
 * (0x81..0x9F or 0xE0..0xEF - the ROM spells those as
 * `(u8)(c+127) < 31' and `(u8)(c+32) < 16') takes the two-byte path;
 * everything else is one ASCII glyph.  Only the ASCII arm is ported,
 * and the escapes are skipped rather than obeyed - see the header. */

#define ISSJISLEAD(c) ((u8)((c)+127) < 31 || (u8)((c)+32) < 16)
#define ISESCAPE(c) ((c) == 7 || (c) == 9 || (c) == 10)

/* real: 0x209300.  0x09 and 0x0A are newline and tab (one byte each,
 * handled at 0x2095A8); 0x07 introduces a letter that indexes the
 * 25-entry jump table at 0x2A3CB0, and each arm consumes a different
 * number of bytes.  The lengths below are read off each arm's final
 * `*(s) = ...' - they are exact; what the arms DO (set the colour, the
 * size, the fixed width, or emit a kind-2 button glyph) is not ported.
 * Unlisted letters fall through to 0x2095A0 and consume nothing but
 * the introducer. */
static int
osdEscapeLen(const char *s)
{
	if(*s != 7)
		return 1;		/* 0x09 newline / 0x0A tab */
	switch(s[1]) {
	case 'g': case 's':	return 2;	/* 0x2094EC / 0x209558 */
	case 'c': case 'w':	return 3;	/* 0x20936C / 0x209584 */
	case 'p':		return 4;	/* 0x20939C */
	case 'a': case 'o':
	case 'y':		return 5;	/* 0x209514 / 0x2094AC / 0x209454 */
	case 'r':		return 6;	/* 0x209400 */
	}
	return 1;
}

/* real: 0x209998 */
static int
osdTextWidth(const char *s)
{
	int total = 0;

	while(*s) {
		if(ISESCAPE(*s)) {
			s += osdEscapeLen(s);
			continue;
		}
		if(ISSJISLEAD(*s)) {
			s += 2;
			continue;
		}
		total += osdGlyphAdvance((u8)*s) + (int)(textScale * (float)textGap);
		s++;
	}
	return total;
}

/* real: 0x209640.  Its prologue (0x2083D0) pushes a fixed drawenv -
 * TEST_1 with ZTE 1 / ZTST ALWAYS and ALPHA_1 = 0x44 (normal blend,
 * FIX 0x80) - overriding whatever the caller set, which is why the
 * text is never depth-tested even though 0x228110 asks for GEQUAL. */
static void
osdTextDraw(const char *s)
{
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);
	vif1SetTexture(&fontTexture);
	/* the ROM's CLAMP_1 comes with the bind (0x262308 always writes
	 * CLAMP/CLAMP); osdbits' vif1SetTexture leaves the GS default */
	vif1SetCLAMP_1(1, 1, 0, 0, 0, 0);

	while(*s) {
		if(ISESCAPE(*s)) {
			s += osdEscapeLen(s);
			continue;
		}
		if(ISSJISLEAD(*s)) {
			s += 2;
			continue;
		}
		osdDrawGlyph((u8)*s);
		textPenX += (osdGlyphAdvance((u8)*s) + (int)(textScale * (float)textGap)) << 4;
		s++;
	}
	sceGsSyncPath(0, 0);		/* real: 0x262418, the same tail */
}

/* =============== Module U's three alignment wrappers ===============
 * real 0x21DC28 / 0x21DC88 / 0x21DD28.  `col' is a colour record in the
 * 0x40-byte sprite layout; only R/G/B are read, the alpha is the
 * separate argument, which is what lets one record be drawn at any
 * fade level (docs/menu-draw.md 6.2). */

static void
drawTextL(int x, int y, const int *col, int alpha, const char *s)
{
	osdTextSetColor(col[0], col[1], col[2], alpha);
	osdTextSetPos(x, y);
	osdTextDraw(s);
}

static void
drawTextC(int x, int y, const int *col, int alpha, const char *s)
{
	if(alpha < 16)
		return;
	drawTextL(x - osdTextWidth(s)/2, y, col, alpha, s);
}

/* ================= the animation timer (0x22AC10) ================= */

typedef struct MtTimer MtTimer;
struct MtTimer
{
	int duration, count, edge, state;
};

static int
mtScale(MtTimer *t, int n)	/* real: 0x22AC20 */
{
	return t->duration ? t->count*n/t->duration : 0;
}

static int
mtIsState(MtTimer *t, int s)	/* real: 0x22AC48 */
{
	return t->state == s;
}

static void
mtOpen(MtTimer *t)		/* real: 0x22AC70 */
{
	if(t->state == 0) {
		t->count = 0;
		t->edge = 1;
		t->state = 1;
	}
}

static void
mtClose(MtTimer *t)		/* real: 0x22AC90 */
{
	if(t->state == 2) {
		t->edge = 1;
		t->count = t->duration;
		t->state = 3;
	}
}

static void
mtStep(MtTimer *t)		/* real: 0x22ACC0 */
{
	t->edge = 0;
	if(t->state == 1) {
		if(++t->count == t->duration) {
			t->edge = 1;
			t->state = 2;
		}
	} else if(t->state == 3) {
		if(--t->count == 0) {
			t->edge = 1;
			t->state = 0;
		}
	}
}

/* ======================= the main menu =======================
 *
 * The page header at 0x27BE90 is static data: title id 1, two 8-byte
 * items at 0x27BE80 (string ids 90 "Browser" and 91 "System
 * Configuration"), 3 visible rows, cursor 0.  Its Anim sits at +24
 * (0x27BEA8) and 0x228460 gives it duration = refreshRate/6 - ten
 * frames on NTSC.  The colour records are 0x27B830 (the blue selected
 * highlight) and 0x27B840 (the dark unselected grey); Module U does not
 * scale or zoom the selected row, it only swaps the record
 * (docs/menu-draw.md 8.3). */

typedef struct MenuItem MenuItem;
struct MenuItem
{
	int strid;		/* real +0x00 */
	void *aux;		/* real +0x04, unused by the draw path */
};

/* real: 0x27BE80 */
static MenuItem mainMenuItems[2] = { { 90, nil }, { 91, nil } };

/* real: 0x27BE90 */
static struct
{
	int title;		/* +0x00 */
	MenuItem *items;	/* +0x04 */
	int count;		/* +0x08 */
	int rows;		/* +0x0C */
	int cursor;		/* +0x10 */
	int top;		/* +0x14 */
	int mode;		/* +0x18 */
} mainMenu = { 1, mainMenuItems, 2, 3, 0, 0, 0 };

static MtTimer mainMenuAnim;	/* real: 0x27BEA8 */

/* real 0x27B830 / 0x27B840 - selected (blue) and unselected (grey) */
static const int colSelected[4] = { 30, 110, 156, 128 };
static const int colUnselected[4] = { 44, 44, 44, 128 };

/* real *(gp-30396) = refreshRate/6, set by 0x228460 alongside the Anim's
 * duration - both 10 on NTSC.  0x228050 uses it as the fade offset at
 * which the menu starts appearing. */
static int mainMenuDur;

static int menuTextEnable = 1;	/* NOT original: argv[8] */
static int menuTextDumpFrame;	/* NOT original: argv[9] */

/* NOT original: read the item band back out of the drawn buffer and
 * print it at 2x2 px per character - the headless way to actually LOOK
 * at the glyphs.  Same sceGsExecStoreImage trick as menu.c's
 * DumpFrameAscii, but windowed on the text and four times finer. */
#define TEXTFBDUMP ((u128*)0x1400000)
#define TDX0 256
#define TDX1 608
#define TDY0 88
#define TDY1 136

/* called from DoMenuScene right after WaitNextFrame (swap-thread quiet
 * window - see menu.c); par = the parity of the buffer just drawn */
int
MenuTextDumpFrame(void)
{
	return menuTextDumpFrame;
}

void
MenuTextDump(int par)
{
	static const char ramp[] = " .:-=+*#%@";
	sceGsStoreImage si;
	u32 *px;
	int x, y, i, j, l, best;
	char line[(TDX1-TDX0)/2 + 2];

	sceGsSyncPath(0, 0);
	sceGsSetDefStoreImage(&si, par == 0 ? 0 : (screenW*screenH)/64,
		screenW/64, SCE_GS_PSMCT32, 0, 0, screenW, screenH);
	FlushCache(0);
	sceGsExecStoreImage(&si, TEXTFBDUMP);
	sceGsSyncPath(0, 0);
	sceDevVif1Reset();	/* clear the reversed FIFO - see DumpFrameAscii */

	px = UNCACHED(TEXTFBDUMP);
	printf("text band x %d..%d y %d..%d, 2x2 px blocks:\n", TDX0, TDX1, TDY0, TDY1);
	for(y = TDY0; y < TDY1; y += 2) {
		for(x = TDX0; x < TDX1; x += 2) {
			best = 0;
			for(j = 0; j < 2; j++)
				for(i = 0; i < 2; i++) {
					u32 p = px[(y+j)*screenW + x+i];
					l = ((p&0xFF) + ((p>>8)&0xFF) + ((p>>16)&0xFF))/3;
					if(l > best)
						best = l;
				}
			line[(x-TDX0)/2] = ramp[best*10/256];
		}
		line[(TDX1-TDX0)/2] = 0;
		printf("|%s|\n", line);
	}
}

/* real: 0x227E18, with the parts that read the System Configuration and
 * Version Information timers (0x27BE44, 0x27BF50) and the clock/wizard
 * anims dropped - none of those screens exist here, and all of them
 * contribute a factor of 1 while closed. */
static int
MainMenuAlpha(int fadeAlpha)
{
	int a;

	a = mtScale(&mainMenuAnim, 128);
	return a * fadeAlpha / 128;
}

/* real: 0x228050 - open the menu once the module's fade-up has come far
 * enough, close it again when the fade-out is complete.  The real guard
 * 0x227FC0 ("no other screen is open") is constant-true here. */
static void
MainMenuStep(int fadeMode, int fadeAlpha)
{
	if(mtIsState(&mainMenuAnim, 0)) {
		if(fadeMode == 2 && fadeAlpha == 128 - mainMenuDur)
			mtOpen(&mainMenuAnim);
	} else if(mtIsState(&mainMenuAnim, 2)) {
		if(fadeMode == 3 && fadeAlpha == 128)
			mtClose(&mainMenuAnim);
	}
}

/* real: 0x228110.  The x is a literal 430 in the ROM (the label column
 * is centred on it), the first row sits at screen_height/2 - 14 and the
 * rows are 16 px apart. */
static void
DrawMainMenu(int fadeAlpha)
{
	const int *col;
	int i, y, alpha;

	alpha = MainMenuAlpha(fadeAlpha);
	/* real: 0x228110 returns unless the Anim is state 2 (fully open) -
	 * the labels do NOT fade in with it.  What makes them appear
	 * gradually is 0x227E18's getFadeAlpha() factor: the Anim opens at
	 * curtain alpha 118 and the curtain still has ten frames to run. */
	if(!mtIsState(&mainMenuAnim, 2))
		return;

	/* real: 0x22A3B8(dbuff, evenOddFrame, 0, field) re-points the draw
	 * environment at the visible buffer; osdbits already draws there.
	 * real: 0x22A0C0(1, 2) - normal blend, ZTST GEQUAL - which
	 * 0x209640's own prologue then overrides to ALWAYS. */
	osdTextSetScale(1.0f);

	y = screenH/2 - 14;
	for(i = 0; i < mainMenu.count; i++) {
		col = i == mainMenu.cursor ? colSelected : colUnselected;
		drawTextC(430, y, col, alpha, osdGetString(mainMenu.items[i].strid));
		y += 16;
	}
}

/* ============================== entry ============================== */

/* real: the text-engine part of 0x21CE58 - do_load_font (0x21DBA0) and
 * the per-screen init 0x228460 */
void
InitMenuText(void)
{
	int rate;

	menuTextEnable = OsdArgInt(8, 0) ? 0 : 1;
	menuTextDumpFrame = OsdArgInt(9, 0);
	if(!menuTextEnable)
		return;

	InitTexture(&fontTexture);

	/* real: 0x209FD0's defaults, then do_load_font's tail */
	textGap = -3;			/* 0x207F38(-3) */
	textYBias = -7*8;		/* 0x207F48(-7) */
	textAdvMul = 1;			/* .data at 0x27156C */
	osdTextSetColor(128, 128, 128, 128);
	osdTextSetScale(1.0f);
	osdTextSetBaseScale(IsPAL() ? 1.15f : 1.0f);

	/* real: 0x228460 - refreshRate/6 into both the Anim's duration and
	 * gp-30396 */
	rate = IsPAL() ? 50 : 60;
	mainMenuDur = rate/6;
	memset(&mainMenuAnim, 0, sizeof(mainMenuAnim));
	mainMenuAnim.duration = mainMenuDur;

	mainMenu.cursor = clamp(OsdArgInt(7, 0), 0, mainMenu.count-1);

	printf("osdsys: menu text, cursor %d (\"%s\")\n",
		mainMenu.cursor, osdGetString(mainMenuItems[mainMenu.cursor].strid));
}

/* the counterpart of 0x228278, the ROM's main-menu pad handler, on top
 * of osdbits' own pad layer (pad.c): up/down walk the item list, the
 * confirm button opens the item.  Only while the menu is fully open -
 * the ROM's handler is equally unreachable until then.
 *
 * The retail cursor is not known to wrap and this one does not either;
 * argv[7] still picks where it starts.  The confirm button is X or
 * Circle: which of the two the ROM takes depends on the region flag
 * 0x204318 reads, and osdbits has no region, so both work. */
static void
MainMenuInput(void)
{
	int moved = 0;

	if(!mtIsState(&mainMenuAnim, 2))
		return;

	if((pad.dirPress & PAD_UP) && mainMenu.cursor > 0) {
		mainMenu.cursor--;
		moved = 1;
	}
	if((pad.dirPress & PAD_DOWN) && mainMenu.cursor < mainMenu.count-1) {
		mainMenu.cursor++;
		moved = 1;
	}
	if(moved)
		printf("menu: cursor %d (\"%s\")\n", mainMenu.cursor,
			osdGetString(mainMenu.items[mainMenu.cursor].strid));

	if(pad.press & (PAD_CROSS|PAD_CIRCLE)) {
		printf("menu: select item %d (\"%s\")\n", mainMenu.cursor,
			osdGetString(mainMenu.items[mainMenu.cursor].strid));
		MenuSelectItem(mainMenu.cursor);
	}
}

/* real: 0x2283A0, the main menu's slot in the per-screen hub 0x2283F0 -
 * step the Anim, run the open/close logic, draw.  (0x228278, the pad
 * input, is MainMenuInput above.) */
void
MenuTextFrame(int fadeMode, int fadeAlpha)
{
	if(!menuTextEnable)
		return;
	mtStep(&mainMenuAnim);
	MainMenuStep(fadeMode, fadeAlpha);
	MainMenuInput();
	DrawMainMenu(fadeAlpha);
	/* the readback itself moved to DoMenuScene's post-swap window */
}
