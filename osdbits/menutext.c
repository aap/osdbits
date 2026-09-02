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
 * its open/close state machine 0x228050 and its alpha 0x227E18, and the
 * System Configuration screen's 0x227560 with the two item widgets it
 * reaches on that screen (0x21EE78 and 0x21E350 -> 0x21DFF8).
 * Also here, the "OSD chrome" - stages 6, 7 and 8 of the frame body,
 * which sit on top of every screen: the letterbox bars (0x21D368), the
 * date/time header (0x21D3A0) and the button hint bar (0x21DA68 ->
 * 0x21D7F8), plus the second font page FNTEXOSD that the config
 * screen's page marker comes off.  Two surprises there: the hint bar's
 * coloured button glyphs are NOT font glyphs but TEXCMARU sprites
 * (0x21D590), and its fourth hint - triangle "Options" - is not in the
 * table at all: it comes from Clock Adjustment's focus callback
 * arming the caller-supplied set (0x21EB80 -> 0x21D768).
 *
 * What is NOT: the two-byte (Shift-JIS) path and its VRAM glyph cache
 * (0x208460's kinds 1 and 3, 0x208C60's 72-slot LRU), the 0x07 escapes
 * other than \7oNNN and \7rN.NN, the right-edge strip (0x21DB18), and
 * every other screen.  (The config items' value editors - the +0x14 /
 * +0x1C / +0x20 / +0x24 callbacks - ARE ported now; see "the item value
 * editors" below.  The RTC write and the NVRAM save are printf stubs.)
 *
 * Extra argv (menu mode), continuing menu.c's list:
 *     main.elf menu hh mm ss framelimit fromOpening fadeAlpha
 *                   debugFrame [cursor [notext [textDump [backPhase
 *                   [back [cfgEnter [cfgLeave [meshTex [cfgCursor]]]]]]]]]
 * cursor (default 0) picks the item the highlight STARTS on; the pad
 * (pad.c, MainMenuInput below) moves it from there.  notext = 1
 * suppresses the whole 2D layer (for the orbs-only regression run);
 * cfgCursor picks which System Configuration item starts highlighted;
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

/* real: 0x204318 - non-zero on the consoles where the CROSS button
 * confirms.  It reads a region/version word (0x204238) the port has no
 * counterpart for, so it is a switch: argv[16], default 1, which is the
 * arrangement the retail screenshots show (cross "Enter", circle
 * "Back").  It is used in exactly two places, both below - osdGetString
 * and 0x21D768 - and they undo each other; see HintSetCustom. */
static int textRegionSwap = 1;

/* real: 0x2041B8, including its two regional swaps: with the flag set,
 * ids 85 and 86 trade places, so asking for "Back" yields "Enter". */
static const char *
osdGetString(int id)
{
	if(textRegionSwap) {
		if(id == 86)
			id = 85;
		else if(id == 85)
			id = 86;
	}
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

/* Slot 3 of the same four-page table: FNTEXOSD, 512x80, PSMT4, TBW 8,
 * TW 9, TH 7, and the SAME 16-entry CLUT - the symbol page.  It is the
 * "kind 2" font: 0x2086A0's kind-2 arm is character for character the
 * ASCII arm with `div 16' instead of `div 8' and the 35-entry metric
 * table at 0x271460 instead of 0x26FE60, so the page is 16 columns of
 * the very same 32x40 cell, two rows.  Its glyphs are arrows, triangles,
 * (R), "(PS2)", a brightness sun and two up/down markers; nothing on it
 * is coloured - the pen colour supplies that, exactly as for ASCII.
 * Glyph 20 is the config screen's page marker, glyph 4 the (R) in
 * string 93 "PlayStation\7o004", glyph 19 the clock's optional prefix. */
#define OSDFONTW 512
#define OSDFONTH 80
#define OSDCOLS 16		/* real: the divisor 16 for kind 2 */
#define NOSDGLYPH 35		/* real: the 0x271460 table's extent */

static Texture fontTexture = {
	nil, RESID_FNTASCII, fontClut, 3, { 0, 0, FONTW, FONTH },
	0, 0, SCE_GS_PSMT4, 0, { 0 }
};

static Texture fontOsdTexture = {
	nil, RESID_FNTEXOSD, fontClut, 3, { 0, 0, OSDFONTW, OSDFONTH },
	0, 0, SCE_GS_PSMT4, 0, { 0 }
};

/* one row of the ROM's page table at 0x271578 plus the metrics that go
 * with the kind - everything 0x2086A0 needs once the kind is decided */
typedef struct GlyphFont GlyphFont;
struct GlyphFont
{
	Texture *tex;
	const int (*met)[2];
	int n;
	int cols;
	int kind;
};

static const GlyphFont fontAscii = { &fontTexture, fontAsciiMetrics, NASCII, CELLCOLS, 0 };
static const GlyphFont fontOsd = { &fontOsdTexture, fontOsdMetrics, NOSDGLYPH, OSDCOLS, 2 };

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
static float textScale = 1.0f;			/* real 0x2DDC44 */
static float textScaleBase = 1.0f;		/* real 0x2DDC48 */
static int textGlyphH16;			/* real 0x2DDC50 */
static int textW16;				/* real 0x2DDC4C */
static int textPct;				/* real 0x271564 */
static int textShiftX, textShiftY;		/* real 0x27186C/0x271870 */
static int textGap = -3;			/* real 0x271860 */
static int textYBias = -7*8;			/* real 0x271864 */
static int textAdvMul = 1;			/* real 0x27156C */

/* real: 0x207F68.  The ROM's chain of truncations matters - the height
 * is (int)(scale*16) rounded to a whole pixel BEFORE the 1.25 and the
 * base scale are applied, so 1.0 gives exactly 320 (20 px) on NTSC and
 * 368 (23 px) on PAL, not 400 and 460.
 *
 * The second half is the `\7rN.NN' percentage (0x271564), which the ROM
 * folds in AFTER the whole-pixel rounding and then splits into a scale
 * for the advance (0x2DDC44) and two shift terms (0x27186C/0x271870)
 * that keep the shrunken glyph sitting on the same baseline.  Only
 * 0x271870 reaches the sprite; 0x27186C is derived and never read on
 * this path, and is kept only because the arithmetic is one expression.
 * String 111 "\7r0.90DIGITAL OUT (OPTICAL)\7r0.00" is the reason this is
 * ported at all: without it that label measures ~11 % too wide, and the
 * page marker's column is clamped off the WIDEST label. */
static void
osdTextSetScale(float scale)
{
	int h, w, a2, a4, a5;

	textScale = textScaleBase = scale;
	h = (int)(scale*32.0f*0.5f) << 4;
	w = (int)(scale*32.0f*0.909091f*0.5f) << 4;
	h = (int)((float)h * 1.25f * textBaseScale);
	w = (int)((float)w * textBaseScale);
	textGlyphH16 = h;
	textW16 = w;
	if(textPct == 0) {
		textShiftX = textShiftY = 0;
		return;
	}
	a2 = h*textPct/100;
	a5 = w*textPct/100;
	a4 = ((h >= 0 ? h : h+15) >> 4) * textPct / 100;
	textScale = scale * (float)textPct / 100.0f;
	textShiftX = w - a5;
	textShiftY = (h - a2) - a4;
	textGlyphH16 = a2;
	textW16 = a5;
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
osdGlyphAdvance(const GlyphFont *f, int g)
{
	int w;

	if((u32)g >= (u32)f->n)
		return 0;
	w = f->met[g][1];
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
static int textBound;	/* real: 0x209640's sp+4, 0x2086A0's `bound' */

/* real: the tail of 0x208460 - the page bind, TCC 1 / MODULATE / CLD 1
 * and TEX1 = 97 (LINEAR both ways).  osdbits' vif1SetTexture is the same
 * pair; CLAMP comes with it because 0x262308 always writes CLAMP/CLAMP. */
static void
osdBindFont(const GlyphFont *f)
{
	vif1SetTexture(f->tex);
	vif1SetCLAMP_1(1, 1, 0, 0, 0, 0);
}

static void
osdDrawGlyph(const GlyphFont *f, int g)
{
	int col, row;
	int x0, y0, x1, y1;
	int u0, u1;
	int xoff16, yoff16;
	int w16;

	if((u32)g >= (u32)f->n)
		return;
	/* real: 0x2086A0's three arms.  Kind 0 binds only when the caller's
	 * flag is clear and then sets it (one bind per string); kinds 1 and
	 * 2 bind unconditionally, and 0x209640 clears the flag afterwards,
	 * so the Latin glyph after a \7o rebinds FNTASCII. */
	if(f->kind == 0) {
		if(!textBound) {
			osdBindFont(f);
			textBound = 1;
		}
	} else
		osdBindFont(f);

	col = g % f->cols;
	row = g / f->cols;

	xoff16 = ((4096 - screenW)/2) << 4;
	yoff16 = ((4096 - screenH)/2) << 4;

	x0 = textPenX + xoff16;
	y0 = textPenY + yoff16 + (int)(textScaleBase * (float)textYBias) + textShiftY;
	w16 = (int)(textScale * (float)(f->met[g][1]*textAdvMul)) << 4;
	x1 = x0 + w16;
	y1 = y0 + textGlyphH16;

	u0 = col*CELLW + f->met[g][0];
	u1 = u0 + f->met[g][1];

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
 * everything else is one ASCII glyph.  The two-byte arm is not ported;
 * of the escapes, the two the shipped Latin strings actually use are. */

#define ISSJISLEAD(c) ((u8)((c)+127) < 31 || (u8)((c)+32) < 16)
#define ISESCAPE(c) ((c) == 7 || (c) == 9 || (c) == 10)

/* real: 0x209300.  0x09 and 0x0A are newline and tab (one byte each,
 * handled at 0x2095A8); 0x07 introduces a letter that indexes the
 * 25-entry jump table at 0x2A3CB0, and each arm consumes a different
 * number of bytes.  The lengths below are read off each arm's final
 * `*(s) = ...'.  Two arms are obeyed, both because Module U's own data
 * needs them:
 *
 *   \7oNNN  (0x2094AC) three decimal digits; RETURNS the kind-2 glyph,
 *           which 0x209640/0x209998 then emit/measure through
 *           0x2086A0(2, ...) / 0x208610.  "\7o020" (gp-30416) is the
 *           System Configuration page marker; string 93 ends "\7o004".
 *   \7rN.NN (0x209400) the percentage size at 0x271564, then re-runs
 *           0x207F68 on the SAVED scale so the percentages do not
 *           compound.  String 111 is "\7r0.90...\7r0.00".
 *
 * The rest still only advance the pointer.  Unlisted letters fall
 * through to 0x2095A0 and consume nothing but the introducer. */
static int
osdEscape(const char **ps)
{
	const char *s = *ps;

	if(*s != 7) {			/* 0x09 newline / 0x0A tab */
		*ps = s+1;
		return -1;
	}
	s++;				/* real: 0x209334 eats the 0x07 */
	switch(*s) {
	case 'o':						/* 0x2094AC */
		*ps = s+4;
		return (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
	case 'r':						/* 0x209400 */
		textPct = (s[1]-'0')*100 + (s[3]-'0')*10 + (s[4]-'0');
		osdTextSetScale(textScaleBase);
		*ps = s+5;
		return -1;
	case 'g': case 's':	*ps = s+1; return -1;	/* 0x2094EC/0x209558 */
	case 'c': case 'w':	*ps = s+2; return -1;	/* 0x20936C/0x209584 */
	case 'p':		*ps = s+3; return -1;	/* 0x20939C */
	case 'a': case 'y':	*ps = s+4; return -1;	/* 0x209514/0x209454 */
	}
	*ps = s;			/* real: 0x2095A0 */
	return -1;
}

/* real: 0x209998, whose escape arm measures a returned kind-2 glyph with
 * 0x208610 - the same expression as 0x208540 against the 0x271460
 * table.  So "\7o020" is 26 - 3 = 23 px wide, not 0. */
static int
osdTextWidth(const char *s)
{
	int total = 0;
	int g;

	while(*s) {
		if(ISESCAPE(*s)) {
			g = osdEscape(&s);
			if(g >= 0)
				total += osdGlyphAdvance(&fontOsd, g) +
					(int)(textScale * (float)textGap);
			continue;
		}
		if(ISSJISLEAD(*s)) {
			s += 2;
			continue;
		}
		total += osdGlyphAdvance(&fontAscii, (u8)*s - 32) +
			(int)(textScale * (float)textGap);
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
	int g;

	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);
	/* real: 0x209640 clears its own `bound' before the loop, so the
	 * first glyph binds; the bind itself moved into osdDrawGlyph so a
	 * kind-2 glyph can take the page away and give it back */
	textBound = 0;

	while(*s) {
		if(ISESCAPE(*s)) {
			g = osdEscape(&s);
			if(g < 0)
				continue;
			osdDrawGlyph(&fontOsd, g);
			textPenX += (osdGlyphAdvance(&fontOsd, g) +
				(int)(textScale * (float)textGap)) << 4;
			/* real: 0x2096C8's delay slot - the flag is cleared so
			 * the next Latin glyph rebinds FNTASCII */
			textBound = 0;
			continue;
		}
		if(ISSJISLEAD(*s)) {
			s += 2;
			continue;
		}
		g = (u8)*s - 32;
		osdDrawGlyph(&fontAscii, g);
		textPenX += (osdGlyphAdvance(&fontAscii, g) +
			(int)(textScale * (float)textGap)) << 4;
		s++;
	}
	gsSyncPath();			/* real: 0x262418, the same tail */
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

/* real 0x27B830 / 0x27B840 - selected (blue) and unselected (grey), and
 * 0x27B850 / 0x27B860, which only the System Configuration screen uses:
 * the dim grey of its value rows and the olive of its page title */
static const int colSelected[4] = { 30, 110, 156, 128 };
static const int colUnselected[4] = { 44, 44, 44, 128 };
static const int colDim[4] = { 96, 96, 96, 128 };
static const int colTitle[4] = { 110, 110, 0, 128 };

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
/* the System Configuration screen's three rows are 88 / 112 / 130 and its
 * clock value is wider than any main-menu label, so the window opens up
 * while that screen is up.  The main menu's stays exactly as it was, so
 * its readback is still comparable byte for byte with older builds. */
#define TCX0 224
#define TCX1 640
#define TCY0 80
#define TCY1 152
/* the two chrome bands: the letterbox' top bar with the date/time header
 * inside it, and the bottom bar with the button hints.  Full width, so
 * the hint columns can be read straight off the ruler. */
#define THX0 0
#define THX1 640
#define TTY0 0
#define TTY1 40
#define TBY0 184
#define TBY1 224

/* called from DoMenuScene right after WaitNextFrame (swap-thread quiet
 * window - see menu.c); par = the parity of the buffer just drawn */
int
MenuTextDumpFrame(void)
{
	return menuTextDumpFrame;
}

static void
MenuTextDumpBand(u32 *px, const char *name, int x0, int x1, int y0, int y1)
{
	static const char ramp[] = " .:-=+*#%@";
	char line[(THX1-THX0)/2 + 2];
	int x, y, i, j, l, best;

	printf("%s band x %d..%d y %d..%d, 2x2 px blocks:\n", name, x0, x1, y0, y1);
	for(y = y0; y < y1; y += 2) {
		for(x = x0; x < x1; x += 2) {
			best = 0;
			for(j = 0; j < 2; j++)
				for(i = 0; i < 2; i++) {
					u32 p = px[(y+j)*screenW + x+i];
					l = ((p&0xFF) + ((p>>8)&0xFF) + ((p>>16)&0xFF))/3;
					if(l > best)
						best = l;
				}
			line[(x-x0)/2] = ramp[best*10/256];
		}
		line[(x1-x0)/2] = 0;
		printf("|%s|\n", line);
	}
}

void
MenuTextDump(int par)
{
	sceGsStoreImage si;
	u32 *px;
	int x0, x1, y0, y1;

	x0 = TDX0; x1 = TDX1; y0 = TDY0; y1 = TDY1;
	if(MenuConfigOpen()) {
		x0 = TCX0; x1 = TCX1; y0 = TCY0; y1 = TCY1;
	}

	gsSyncPath();
	sceGsSetDefStoreImage(&si, par == 0 ? 0 : (screenW*screenH)/64,
		screenW/64, SCE_GS_PSMCT32, 0, 0, screenW, screenH);
	FlushCache(0);
	sceGsExecStoreImage(&si, TEXTFBDUMP);
	gsSyncPath();
	sceDevVif1Reset();	/* clear the reversed FIFO - see DumpFrameAscii */

	px = UNCACHED(TEXTFBDUMP);
	/* the item band keeps its exact old window so its readback is still
	 * comparable line for line with pre-chrome builds */
	MenuTextDumpBand(px, "text", x0, x1, y0, y1);
	/* the chrome spans the whole width, and 320 characters is past what
	 * the emulator's log will keep on one line, so each bar comes out in
	 * two halves */
	MenuTextDumpBand(px, "chrome topL", THX0, THX1/2, TTY0, TTY1);
	MenuTextDumpBand(px, "chrome topR", THX1/2, THX1, TTY0, TTY1);
	MenuTextDumpBand(px, "chrome botL", THX0, THX1/2, TBY0, TBY1);
	MenuTextDumpBand(px, "chrome botR", THX1/2, THX1, TBY0, TBY1);
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

/* ================= the System Configuration item list =================
 *
 * The screen's drawer is 0x227560, the third of the four calls 0x227DE8
 * makes (timerStep, 0x227390's state machine, 0x227560's draw, and the
 * tail 0x227D08, which is only the focus notify plus the dispatch to a
 * pad handler - 0x2279B8 for mode 0, 0x227BE8 for mode 1).
 *
 * Its page header is static data at 0x27BE28 - {title 91 "System
 * Configuration", items 0x27BD10, count 5, ...} - and +0x0C is NOT a row
 * count: 0x228708, which 0x227560 calls right after the title, rewrites
 * it every frame with the WIDEST item label's measured width.  +0x18 is
 * the mode (0 = the item list, 1 = one item expanded into its value
 * list), +0x34 a free-running phase counter (below).
 *
 * The layout 0x227560 lays down is three fixed rows, all centred on
 * x = 430 like the main menu's:
 *
 *     y = 88            the page title, colour 0x27B860
 *     y = 88 + 24       the item label,  colour 0x27B830
 *     y = 88 + 42       the item value,  colour 0x27B850
 *
 * (101 / +27.6 / +48.3 on PAL - i.e. the same 88, 24 and 42 times the
 * 1.15 base scale, added as doubles and truncated.)  Three rows for five
 * items, because the ROM shows exactly ONE item at a time: each item's
 * label alpha is `(itemAlpha[i] * pageAlpha) >> 7' where itemAlpha[i] is
 * *(0x27F090 + i*48 + 0x24), which 0x226BB8 ramps +8 per frame toward
 * 128 for the item under the cursor and -8 toward 0 for the other four.
 * The five cubes menuconfig.c draws are these five items, and their
 * colours come off the same ramp; the label is the cube's caption, not a
 * row in a list.  (This is what the previous stopgap got wrong: it drew
 * all five labels at once, hung off the cubes' projected positions, so
 * they piled up wherever the cubes clustered.)
 *
 * Each item's own value row is its widget at +0x18 - 0x21EE78 for four
 * of the five (look the current setting up in the item's value list and
 * draw its string centred) and 0x21E350 for Clock Adjustment (the six
 * clock fields with their separators).  The +0x1C widget is the mode-1
 * drawer (the expanded value list) and +0x14 the confirm callback that
 * opens the item; neither is ported - each is a whole sub-screen. */

/* real: the value lists at 0x27BC20 / 0x27BCB0 / 0x27BBC0, 48-byte
 * records of which the draw path reads only +0x00 (the setting value)
 * and +0x04 (its string id).  Component Video Out really is stored in
 * that order, RGB second. */
typedef struct ConfigValue ConfigValue;
struct ConfigValue
{
	int value;		/* real +0x00 */
	int strid;		/* real +0x04 */
};

static const ConfigValue cfgScreenSize[3] =	/* real: 0x27BC20 */
	{ { 0, 108 }, { 1, 109 }, { 2, 110 } };
static const ConfigValue cfgDigitalOut[2] =	/* real: 0x27BCB0 */
	{ { 0, 112 }, { 1, 113 } };
static const ConfigValue cfgComponent[2] =	/* real: 0x27BBC0 */
	{ { 1, 116 }, { 0, 115 } };
/* real: 0x27B920 (and its twin 0x27BA70) - the language list 0x21ED18,
 * called from the per-screen init 0x228460 (0x2284D4), wires into
 * items[4].+0x04/+0x10 by region: region 0 gets 0x27B8C0 ({Japanese,
 * English}, count 2), regions 1 and 2 the seven European languages.
 * osdbits' region proxy is textRegionSwap (1 = the retail screenshots'
 * arrangement), so the seven-entry table is the one it wires. */
static const ConfigValue cfgLanguage[7] =
	{ { 1, 119 }, { 2, 120 }, { 3, 121 }, { 4, 122 },
	  { 5, 123 }, { 6, 124 }, { 7, 125 } };

/* real: the 56-byte item records at 0x27BD10 + n*56, with the fields the
 * draw path touches.  The six callbacks live at +0x14..+0x28; `draw' is
 * +0x18 and `edit' - the mode-1 widget, drawing AND its own pad input,
 * exactly as in the ROM - is +0x1C.  0x21EDB8, which every widget runs
 * first, re-syncs +0x08 from the live setting *(int*)0x22B0E8(+0x0C). */
typedef struct ConfigItem ConfigItem;
struct ConfigItem
{
	int strid;			/* real +0x00 */
	int nvalues;			/* real +0x04 */
	int value;			/* real +0x08 */
	int setting;			/* real +0x0C */
	const ConfigValue *values;	/* real +0x10 */
	void (*draw)(struct ConfigItem*, int, int, int);	/* real +0x18 */
	void (*edit)(struct ConfigItem*, int, int, int);	/* real +0x1C */
};

static void DrawItemValue(ConfigItem*, int, int, int);	/* real: 0x21EE78 */
static void DrawItemClock(ConfigItem*, int, int, int);	/* real: 0x21E350 */
static void EditItemClock(ConfigItem*, int, int, int);	/* real: 0x21EA20 */
static void EditItemValue(ConfigItem*, int, int, int);	/* real: 0x21F080 */
static void EditItemLang(ConfigItem*, int, int, int);	/* real: 0x21F168 */

static ConfigItem configItems[5] = {
	{ 106, 6, 0, 0, nil,           DrawItemClock, EditItemClock },
	{ 107, 3, 0, 0, cfgScreenSize, DrawItemValue, EditItemValue },
	{ 111, 2, 0, 1, cfgDigitalOut, DrawItemValue, EditItemValue },
	{ 114, 2, 0, 2, cfgComponent,  DrawItemValue, EditItemValue },
	/* real: nvalues 0 / values nil in .data; 0x21ED18 wires the region's
	 * language table in - InitMenuText models it below */
	{ 117, 0, 0, 3, nil,           DrawItemValue, EditItemLang },
};

/* ============== the live settings block (0x352880) ==============
 *
 * real: 0x22B0E8(n) returns &block[n].  The screen uses indices 0
 * (screen size), 1 (digital out), 2 (component out), 3 (language) and
 * 6..11 (the Clock Adjustment editor's Y/M/D/h/m/s fields).  The mode-1
 * widgets write it LIVE while scrolling - the ROM previews a setting the
 * moment it is highlighted - and the items' +0x20 confirm callbacks then
 * compare it against the persisted copy (0x203690/0x203658/0x2036F8/
 * 0x2040D0) and call 0x22B3F8 to push a changed value out to the
 * console's NVRAM.  osdbits has no NVRAM, so the persisted copy is a
 * plain array and 0x22B3F8 is a printf. */
static int cfgSettings[16];
static int cfgPersisted[4];

/* real: 0x27BE28 */
static struct
{
	int title;		/* +0x00 */
	ConfigItem *items;	/* +0x04 */
	int count;		/* +0x08 */
	int maxw;		/* +0x0C, rewritten by 0x228708 every frame */
	int cursor;		/* +0x10 */
	int top;		/* +0x14 */
	int mode;		/* +0x18 */
	int phase;		/* +0x34, the page marker's pulse */
} configMenu = { 91, configItems, 5, 0, 0, 0, 0, 0 };

#define configCursor configMenu.cursor

/* real: gp-30416 = 0x2A79A0.  Escape 'o' emits glyph 20 of the kind-2
 * table - the up/down chevron pair on FNTEXOSD's second row. */
static const char cfgMarker[] = "\7o020";

/* real: 0x227390's tail - the marker's sawtooth, +310 a frame, folded at
 * +-refreshRate*31400/60 (203 frames a lap on NTSC) */
static int cfgPhaseFold;

/* real: *(0x27F090 + i*48 + 0x24), ramped by 0x226BB8 (the cube stage's
 * first half, 0x226FA8 -> 0x226CF8).  It lives in the cube placement
 * table because it is the same number that drives each cube's colour
 * ease; the port keeps it here because menuconfig.c is not this task's
 * to touch, and the merge should fold the two together. */
static int configItemAlpha[5];

/* real: 0x227560's three row positions, derived in InitMenuText */
static int cfgTitleY, cfgLabelY, cfgValueY;

/* real: 0x27B870, six 12-byte {field kind, min, max} records.  Only the
 * kind reaches the draw path; the ranges belong to the editor 0x21E3B8. */
static const int cfgClockField[6][3] = {
	{  6, 2000, 2099 },	/* year   */
	{  7,    1,   12 },	/* month  */
	{  8,    1,   31 },	/* day    */
	{  9,    0,   23 },	/* hour   */
	{ 10,    0,   59 },	/* minute */
	{ 11,    0,   59 },	/* second */
};

/* real: the separator jump table at 0x2A4860.  The one after the day is
 * the odd one out - 0x21E27C measures " " and advances the pen without
 * ever drawing it. */
static const struct { const char *s; int draw; } cfgClockSep[6] = {
	{ "/", 1 }, { "/", 1 }, { " ", 0 }, { ":", 1 }, { ":", 1 }, { nil, 0 }
};

/* NOT original: the ROM's clock fields come from *(int*)0x22B0E8(6..8),
 * the RTC snapshot; osdbits has hh:mm:ss from argv and no date at all,
 * so the date starts at the PS2's own epoch.  The Clock Adjustment
 * editor writes an applied date back here (the ROM's sticks in the RTC). */
static int cfgClockDate[3] = { 2000, 1, 1 };

/* real: the ROM sprintfs each field with "%04d" (year), "%2d" (hour) or
 * "%02d" (the rest) - 0x2A7858 / 0x2A7868 / 0x2A7860. */
static void
cfgFmtNum(char *p, int v, int width, int zero)
{
	char tmp[12];
	int n;

	if(v < 0)
		v = 0;
	n = 0;
	do {
		tmp[n++] = '0' + v%10;
		v /= 10;
	} while(v && n < 11);
	while(n < width)
		tmp[n++] = zero ? '0' : ' ';
	while(n > 0)
		*p++ = tmp[--n];
	*p = 0;
}

/* real: 0x21DFF8, reached through item 0's +0x18 widget 0x21E350 with
 * its edit flag clear, so every field draws in 0x27B850 and the focused
 * one is not picked out (0x21E3B0 passes 1 and splits the colours into
 * 0x27B830 for the field being edited and 0x27B840 for the rest - that
 * is the Clock Adjustment editor, not this screen).
 *
 * The whole string is centred by measuring the template at 0x2A47F0,
 * "0000/00/00 00:00:00", and stepping a left-aligned pen from there.
 * 0x203968() returns 1 for the twelve-hour face, which swaps in the
 * longer template at 0x2A47C8 and appends " AM"/" PM" at 66% size after
 * the seconds (0x2A4808 / 0x2A4820); the port has no such setting and
 * always draws the 24-hour face.  The ROM also brackets every field with
 * \7p@0 / \7p00 (fixed-width on the width of '0'), which is a no-op for
 * the Latin face - '0'..'9' are all {5, 23} in the 0x26FE60 metrics. */
static void
DrawClockRow(ConfigItem *it, int x, int y, int alpha, int edit)
{
	char buf[16];
	const int *col, *sepcol;
	int i, v;

	x -= osdTextWidth("0000/00/00 00:00:00")/2;
	/* real: edit == 0 draws every field in 0x27B850; edit == 1 (the
	 * 0x21E3B0 entry) picks the field under item->0x08 out in 0x27B830
	 * and the rest in 0x27B840 */
	sepcol = edit ? colUnselected : colDim;
	for(i = 0; i < it->nvalues; i++) {
		/* real: 0x21DFF8 always reads the six settings 0x22B0E8(6..11);
		 * 0x21DDC0 keeps them synced to the live clock while the editor
		 * is closed.  The port reads the live clock directly then. */
		if(edit)
			v = cfgSettings[cfgClockField[i][0]];
		else switch(cfgClockField[i][0]) {
		case 6:  v = cfgClockDate[0]; break;
		case 7:  v = cfgClockDate[1]; break;
		case 8:  v = cfgClockDate[2]; break;
		case 9:  v = (int)MenuClockHours(); break;
		case 10: v = (int)MenuClockMinutes(); break;
		default: v = (int)MenuClockSeconds(); break;
		}
		switch(cfgClockField[i][0]) {
		case 6:  cfgFmtNum(buf, v, 4, 1); break;
		case 9:  cfgFmtNum(buf, v, 2, 0); break;
		default: cfgFmtNum(buf, v, 2, 1); break;
		}
		col = edit ? (i == it->value ? colSelected : colUnselected) :
			colDim;
		drawTextL(x, y, col, alpha, buf);
		x += osdTextWidth(buf);
		if(cfgClockSep[i].s == nil)
			continue;
		if(cfgClockSep[i].draw)
			drawTextL(x, y, sepcol, alpha, cfgClockSep[i].s);
		x += osdTextWidth(cfgClockSep[i].s);
	}
}

static void
DrawItemClock(ConfigItem *it, int x, int y, int alpha)
{
	DrawClockRow(it, x, y, alpha, 0);
}

/* real: 0x21EE78 - 0x21EDB8, then the item's current value's string,
 * centred, in 0x27B850.  (Language's list pointer is 0 in .data; the
 * per-screen init wires the region's table in - see cfgLanguage.) */
static void
DrawItemValue(ConfigItem *it, int x, int y, int alpha)
{
	if(it->values == nil || (u32)it->value >= (u32)it->nvalues)
		return;
	drawTextC(x, y, colDim, alpha, osdGetString(it->values[it->value].strid));
}

/* ================ the item value editors (mode 1) ================
 *
 * Entering an item (0x2279B8's confirm arm) calls its +0x14 and flips
 * the header's mode to 1; the item's +0x1C widget then owns the value
 * row - it both DRAWS the expanded value list and reads the pad for
 * left/right itself (the ROM's widgets poll gp-30316 directly), and it
 * writes the live setting on every change, so a setting is previewed
 * the moment it is highlighted.  0x227BE8 keeps only confirm (run the
 * item's +0x20, back to mode 0) and cancel (+0x24, back to mode 0).
 *
 * Each widget's input is gated on the 0x27EC40 timer being idle in the
 * ROM; nothing in the port ever opens it, so the gate is dropped. */

static void HintSetCustom(int, int, int, int);	/* real: 0x21D768 */
static int hintCustom[5];			/* real: 0x27B5D0 */
static void ConfigItemFocus(int, int);		/* real: the items' +0x28 */

/* real: 0x21EDB8 - re-sync the item's value index from the live setting */
static void
ConfigItemResync(ConfigItem *it)
{
	int i, live;

	live = cfgSettings[it->setting];
	if(it->values == nil || it->nvalues <= 0)
		return;
	if(it->values[it->value].value == live)
		return;
	it->value = 0;
	for(i = 0; i < it->nvalues; i++)
		if(it->values[i].value == live) {
			it->value = i;
			break;
		}
}

/* real: 0x21F978(20992, 1, n) - the UI clicks: 4 = confirm, 6 = a value
 * step, 10 = cancel */
static void
cfgClick(int n)
{
	OSDDispatch(20992, 1, n, 0);
}

/* real: the shared left/right half of 0x21F080 and 0x21F168 - move the
 * value index with wraparound and write the live setting.  The ROM
 * takes these edges from gp-30316, whose held-repeat matches pad.c's
 * dirPress. */
static void
ConfigValueStep(ConfigItem *it)
{
	if(it->values == nil || it->nvalues <= 0)
		return;
	if(pad.dirPress & PAD_LEFT) {
		if(--it->value < 0)
			it->value += it->nvalues;
		cfgClick(6);
	}
	if(pad.dirPress & PAD_RIGHT) {
		if(++it->value >= it->nvalues)
			it->value = 0;
		cfgClick(6);
	}
	/* real: 0x21F080's tail - *(0x22B0E8(+0x0C)) = values[+0x08].value,
	 * unconditionally, every frame the widget runs */
	cfgSettings[it->setting] = it->values[it->value].value;
}

/* real: 0x21EF00 (through 0x21F080) - every value on one row, centred as
 * a whole, separated by the " " at 0x2A7888; the current one in 0x27B830
 * and the rest in 0x27B840 */
static void
EditItemValue(ConfigItem *it, int x, int y, int alpha)
{
	int i, w;

	ConfigValueStep(it);		/* real: 0x21F080's input half */
	if(alpha <= 0 || it->values == nil)
		return;
	w = 0;
	for(i = 0; i < it->nvalues; i++)
		w += osdTextWidth(osdGetString(it->values[i].strid));
	w += (it->nvalues - 1) * osdTextWidth(" ");
	x -= w/2;
	for(i = 0; i < it->nvalues; i++) {
		drawTextL(x, y, i == it->value ? colSelected : colUnselected,
			alpha, osdGetString(it->values[i].strid));
		x += osdTextWidth(osdGetString(it->values[i].strid)) +
			osdTextWidth(" ");
	}
}

/* real: 0x21F168 - the language row.  Seven names do not fit on one
 * line, so the ROM shows only the current one and slides the old name
 * out sideways (gp-30676, +-150 px decaying by a sixth a frame, drawn by
 * 0x21F3A8 with the up/down marker glyph 61 alongside).  The slide is
 * not ported: the row shows the current name centred, in the selected
 * blue, and steps exactly like the other value items. */
static void
EditItemLang(ConfigItem *it, int x, int y, int alpha)
{
	ConfigValueStep(it);
	if(alpha <= 0 || it->values == nil ||
	   (u32)it->value >= (u32)it->nvalues)
		return;
	drawTextC(x, y, colSelected, alpha,
		osdGetString(it->values[it->value].strid));
}

/* ================ the Clock Adjustment editor ================ */

/* the h/m/s snapshot taken when the editor opens, for cancel.  real:
 * 0x21EB30 re-reads the console's RTC (0x22B838); the port has no RTC,
 * so the pre-edit clock stands in for it. */
static int cfgClockSaved[3];

/* real: the days-in-month half of 0x21E3B8.  The ROM clamps the whole
 * date through the calendar library (0x20ABB0/0x20B028, between
 * 2000-01-01 and 2099-12-31 in epoch seconds); the field ranges at
 * 0x27B870 already pin everything but the day-vs-month overflow, so the
 * port keeps just that: the day WRAPS against the real month length
 * (the up/down arm below uses this as its max) and shrinks to it when
 * a month or year step leaves it over. */
static int
ClockDaysInMonth(void)
{
	static const int mdays[12] =
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	int y, m, max;

	y = cfgSettings[6];
	m = cfgSettings[7];
	max = mdays[(m - 1) % 12];
	if(m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))
		max = 29;
	return max;
}

static void
ClockClampDay(void)
{
	int max = ClockDaysInMonth();

	if(cfgSettings[8] > max)
		cfgSettings[8] = max;
}

/* real: 0x21E870 - the editor's own pad half.  Left/right move the
 * field cursor (item->0x08, which the clock item reuses for it);
 * up/down step the field through its 0x27B870 range with wraparound.
 * The ROM reads up/down from gp-30308, the held-repeat word - pad.c's
 * dirPress carries the same repeat. */
static void
ClockEditInput(ConfigItem *it)
{
	int f, v, min, max;

	/* real: 0x21D768(1, 85, 86, 1) / 0x21D758(0xF000) - all four hint
	 * slots, the outer pair with the empty string 1 */
	HintSetCustom(1, 85, 86, 1);
	hintCustom[4] = 0xF000;

	f = it->value;
	if(pad.dirPress & PAD_LEFT) {
		if(--f < 0)
			f += it->nvalues;
		cfgClick(6);
	}
	if(pad.dirPress & PAD_RIGHT) {
		if(++f >= it->nvalues)
			f = 0;
		cfgClick(6);
	}
	it->value = f;

	v = cfgSettings[cfgClockField[f][0]];
	min = cfgClockField[f][1];
	max = f == 2 ? ClockDaysInMonth() : cfgClockField[f][2];
	if(pad.dirPress & PAD_UP) {
		if(++v > max)
			v = min;
		cfgClick(6);
	}
	if(pad.dirPress & PAD_DOWN) {
		if(--v < min)
			v = max;
		cfgClick(6);
	}
	cfgSettings[cfgClockField[f][0]] = v;
	ClockClampDay();		/* real: 0x21E3B8 after either step */
}

/* real: 0x21EA20 - draw the row in edit colours, run the pad, and
 * reseed the soft clock from the six fields EVERY frame (0x22B8E8's
 * tail call).  The per-frame reseed is the FREEZE: it re-pins h/m/s and
 * zeroes the ms fraction faster than 0x22BB30's tick can advance them
 * (the gp-30328 "hold" word 0x21DF28 writes is read by nothing in the
 * whole image), so the carousel's pose - which 0x225628 derives from
 * the soft clock every frame - settles and STOPS.  With the seconds
 * zeroed on entry that settled pose is spin 0 / tilt snapped to the
 * hour slot: the fixed presentation pose aap describes.  The pose only
 * moves again when a field VALUE is edited, and then the reaction IS
 * the highlight: an hour step re-fronts the ring onto the new hour's
 * rod (the front slot carries the bright colours and the split), a
 * minute step resizes the front rod's bright segment, a second step
 * swings the ring to the new second and parks again. */
static void
EditItemClock(ConfigItem *it, int x, int y, int alpha)
{
	DrawClockRow(it, x, y, alpha, 1);	/* real: 0x21E3B0 */
	ClockEditInput(it);			/* real: 0x21E870, 0x27EC40-gated */
	MenuClockSet(cfgSettings[9], cfgSettings[10], cfgSettings[11]);
}

/* real: 0x21DF28 - the clock item's +0x14, in the ROM's own order:
 * 0x21DDC0 (re-sync the six fields from the soft clock), `sw zero,
 * -30428(gp)' (the orb scale TARGET - 0x2285C0 eases the live 0x27B440
 * toward it at 0.1 a frame and 0x2261B8 multiplies the orbit radius by
 * it, so the orbs sink into the CENTRE), 0x22EF90 (trail fade down),
 * 0x22B960 (the gp-30328 word; write-only in the image - the real
 * freeze is the widget's per-frame reseed, see EditItemClock),
 * `0x22B0E8(11) = 0' (ZERO THE SECONDS - what parks the carousel at
 * the fixed spin-0 pose), 0x22B8E8 (seed the soft clock from the six
 * fields) and the 0x21E3B8 tail (the calendar clamp). */
static void
ClockEditOpen(ConfigItem *it)
{
	int h, m, s;

	(void)it;
	MenuClockGet(&h, &m, &s);	/* real: 0x21DDC0's re-sync */
	MenuOrbScaleTarget(0.0f);	/* real: `sw zero,-30428(gp)' */
	MenuOrbTrailFade(0);		/* real: 0x22EF90 */
	MenuClockHold(1);		/* real: 0x22B960 */
	cfgSettings[6] = cfgClockDate[0];
	cfgSettings[7] = cfgClockDate[1];
	cfgSettings[8] = cfgClockDate[2];
	cfgSettings[9] = h;
	cfgSettings[10] = m;
	cfgSettings[11] = 0;		/* real: 0x21DF68 - the seconds */
	/* NOT original: the cancel snapshot keeps the REAL seconds - the
	 * ROM's cancel re-reads the RTC (0x22B838), which never stopped */
	cfgClockSaved[0] = h;
	cfgClockSaved[1] = m;
	cfgClockSaved[2] = s;
	/* real: 0x21DFC4 - seed the soft clock, ms = 0 */
	MenuClockSet(cfgSettings[9], cfgSettings[10], cfgSettings[11]);
	ClockClampDay();		/* real: the j 0x21E3B8 tail */
}

/* real: 0x21EAE0 - the clock item's +0x20 confirm: 0x22B2A8 writes the
 * six fields to the CDVD RTC.  STUBBED - aap decides about hardware
 * side effects; the soft clock keeps the edited time either way. */
static void
ClockEditApply(ConfigItem *it)
{
	(void)it;
	printf("osdsys: clock adjustment: set %04d/%02d/%02d %02d:%02d:%02d"
		" (RTC write stubbed)\n",
		cfgSettings[6], cfgSettings[7], cfgSettings[8],
		cfgSettings[9], cfgSettings[10], cfgSettings[11]);
	cfgClockDate[0] = cfgSettings[6];	/* the port's stand-in RTC date */
	cfgClockDate[1] = cfgSettings[7];
	cfgClockDate[2] = cfgSettings[8];
	MenuOrbScaleTarget(1.0f);	/* real: 0x21EAFC - orbs fly back out */
	MenuClockHold(0);		/* real: 0x22B950 */
	MenuOrbTrailFade(1);		/* real: 0x22EF30 */
	ConfigItemFocus(0, 1);		/* real: the 0x21D768/0x21D758 tail */
}

/* real: 0x21EB30 - the +0x24 cancel: 0x22B838 reseeds the soft clock
 * from the real RTC; the port restores the pre-edit snapshot. */
static void
ClockEditCancel(ConfigItem *it)
{
	(void)it;
	MenuClockSet(cfgClockSaved[0], cfgClockSaved[1], cfgClockSaved[2]);
	MenuOrbScaleTarget(1.0f);	/* real: 0x21EB4C */
	MenuClockHold(0);		/* real: 0x22B950 */
	MenuOrbTrailFade(1);		/* real: 0x22EF30 */
	ConfigItemFocus(0, 1);
}

/* real: the items' +0x20 confirm callbacks - 0x21EBC8 (screen size),
 * 0x21EC08 (digital out), 0x21EC98 (component), 0x21ECD8 (language).
 * Each compares the live setting against the persisted copy (0x203690 /
 * 0x203658 / 0x2036F8 / 0x2040D0) and calls 0x22B3F8 - the NVRAM save -
 * only on a change; digital out additionally posts 20/21 into
 * *(0x1F00B4), the message that flips the IOP's S/PDIF driver.  osdbits
 * has neither NVRAM nor the IOP message port, so both are printfs. */
static void
ConfigItemApply(ConfigItem *it)
{
	int live = cfgSettings[it->setting];

	if(live == cfgPersisted[it->setting])
		return;
	if(it->setting == 1)		/* real: 0x21EC08's *(0x1F00B4) */
		printf("osdsys: digital out message %d (stubbed)\n",
			live == 0 ? 20 : 21);
	printf("osdsys: config save: setting %d = %d (\"%s\") (stubbed)\n",
		it->setting, live,
		it->values ? osdGetString(it->values[it->value].strid) : "");
	cfgPersisted[it->setting] = live;	/* real: 0x22B3F8 */
}

/* real: 0x228708 - the widest item label, into the header's +0x0C */
static int
ConfigMenuWidest(void)
{
	int i, w, best;

	best = 0;
	for(i = 0; i < configMenu.count; i++) {
		w = osdTextWidth(osdGetString(configMenu.items[i].strid));
		if(w > best)
			best = w;
	}
	return best;
}

/* real: 0x226BB8's share of the cube stage - the per-item ramp that
 * makes the screen a one-item-at-a-time display.  (The rest of 0x226BB8
 * eases each cube's colour toward 0x27EC10 for the item under the cursor
 * and 0x27EC20 for the others through 0x22EC60, and decays the entry's
 * +0x20 bias by *(gp-32144) = 0.95 a frame; that half belongs to
 * menuconfig.c.) */
static void
ConfigMenuStepItems(void)
{
	int i, a;

	/* real: 0x226BB8 reads the cursor straight out of the header at
	 * 0x27BE28+16; the port's cube half (menuconfig.c) keeps its own
	 * copy, so hand it over every frame - both sides of the selection
	 * (label alpha here, cube colour there) follow the same index */
	MenuConfigSetCursor(configCursor);

	for(i = 0; i < configMenu.count; i++) {
		a = configItemAlpha[i] + (i == configCursor ? 8 : -8);
		configItemAlpha[i] = clamp(a, 0, 128);
	}

	/* real: 0x227390's tail steps the header's +0x34 by 310 and folds it
	 * back by a whole lap once it passes +-cfgPhaseFold */
	configMenu.phase += 310;
	if(configMenu.phase > cfgPhaseFold)
		configMenu.phase -= 2*cfgPhaseFold;
	else if(configMenu.phase < -cfgPhaseFold)
		configMenu.phase += 2*cfgPhaseFold;
}

/* real: 0x227560.  Note that only the LABEL's column is pulled left when
 * it would overrun the right margin; the title and the value rows keep
 * the literal 430, and the marker's column is clamped off the header's
 * widest-label field instead of this item's. */
static void
DrawConfigMenu(int fadeAlpha)
{
	const char *label;
	int i, alpha, a, x, right, gap, markw;

	/* real: 0x227560 bails on timerIsState(0x27BE44, 0), and also on
	 * timerIsState(0x27EC40, 2) - a value sub-screen fully up hides the
	 * item list.  0x27EC40 has no counterpart here. */
	if(!MenuConfigOpen())
		return;
	alpha = MenuConfigAlpha(fadeAlpha);

	/* real: 0x22A3B8(0x1F0A10, evenOddFrame, 0, field) then
	 * 0x22A0C0(1, 2), exactly as the main menu's 0x228110 does */
	osdTextSetScale(1.0f);			/* real: 0x207F68(1.0) */

	drawTextC(430, cfgTitleY, colTitle, alpha,
		osdGetString(configMenu.title));
	configMenu.maxw = ConfigMenuWidest();	/* real: 0x228708 */

	/* real: s7, the width of the one-space string at 0x2A79A8, which is
	 * the gap between the label and the page marker; s6 = markW, the
	 * width of the marker string at gp-30416, "\7o020" - escape 'o'
	 * emits glyph 20 of the kind-2 (FNTEXOSD) table, the up/down
	 * chevron pair.  Both clamps below count it. */
	gap = osdTextWidth(" ");
	markw = osdTextWidth(cfgMarker);

	/* real: 0x227560's middle block.  Left-aligned at
	 * x + maxw/2 + gap on the LABEL row, in 0x27B850, at an alpha of
	 * |128 sin(phase/10000)| where phase is the header's +0x34 - a
	 * sawtooth 0x227390's tail steps by 310 a frame and folds at
	 * +-refreshRate*31400/60 (+-31400 NTSC, ~203 frames a lap).
	 * 0x21EE50 (the items' +0x14) zeroes it on entering an item.
	 * The ROM guards it with `mode != 1 && screen fully open'. */
	x = 430;
	right = x + markw + gap + configMenu.maxw/2;
	if(right >= screenW - 24)
		x -= right + 24 - screenW;
	if(configMenu.mode != 1 && alpha >= 128) {
		a = (int)(128.0f * sinf((float)configMenu.phase / 10000.0f));
		if(a < 0)
			a = -a;
		drawTextL(x + configMenu.maxw/2 + gap, cfgLabelY, colDim, a,
			cfgMarker);
	}

	for(i = 0; i < configMenu.count; i++) {
		/* real: (itemAlpha * pageAlpha) >> 7, with the ROM's round-to-
		 * zero fixup for a negative product that cannot happen here */
		a = configItemAlpha[i] * alpha / 128;
		label = osdGetString(configMenu.items[i].strid);

		x = 430;
		right = x + markw + gap + osdTextWidth(label)/2;
		if(right >= screenW - 24)
			x -= right + 24 - screenW;

		/* real: mode 1 draws the label in 0x27B850 and hands the row to
		 * the +0x1C widget for the cursor item.  The widget runs even at
		 * alpha 0 - it owns the left/right input (see EditItem*). */
		if(configMenu.mode == 1 && i == configCursor) {
			if(a)
				drawTextC(x, cfgLabelY, colDim, a, label);
			configMenu.items[i].edit(&configMenu.items[i], 430,
				cfgValueY, a);
			continue;
		}
		if(a == 0)
			continue;
		drawTextC(x, cfgLabelY, colSelected, a, label);
		configMenu.items[i].draw(&configMenu.items[i], 430, cfgValueY, a);
	}
}

/* real: the System Configuration screen's own share of 0x2279B8 and
 * 0x227BE8 - the two pad handlers 0x227D08 dispatches on the header's
 * mode, and ONLY while the screen's Anim is fully open (state 2, see
 * MenuConfigFullyOpen); during the whole opening and closing
 * choreography the ROM reads no pad at all, which is what makes entry
 * land in the item LIST rather than falling straight through into an
 * item's editor on the press that opened the screen.
 *
 * Both handlers read gp-30316, the press-EDGE word (pad.press) - only
 * the clock editor's up/down (0x21E870, gp-30308) repeats.  The arms
 * are exclusive: every taken arm of 0x2279B8 tail-jumps out through the
 * 0x2287A8 click, in this order: up (0x1000, wrap + the two +0x28 focus
 * calls + click 5), down (0x4000, same), confirm (0x20, the item's
 * +0x14 - 0x21DF28 for Clock Adjustment, 0x21EE50 for the rest - then
 * mode 1), 0x80 -> 0x227028 (the version sub-screen, not ported), the
 * cancel button (0x40) -> 0x227338 (close the Anim, click 10 - the way
 * BACK to the main menu), triangle (0x10) -> only from cursor 0: click
 * 4, close the Anim, 0x223658 (the Options screen - not ported, so it
 * leaves like cancel; the hint bar only offers triangle on item 0).
 * Mode 1 (0x227BE8): confirm (0x20) kicks the cursor's cube, runs the
 * item's +0x20 and drops to mode 0 (the 0x1F00B0 == 5 && 0x1F00A4 == 18
 * "launched to set the clock" arm that would pick mode 2 instead is not
 * ported); the cancel button (0x40) runs the +0x24 (0x21EB30 for the
 * clock, a no-op for the others).  The left/right value stepping is NOT
 * here in the ROM either - the mode-1 widgets poll gp-30316 themselves
 * (EditItem*).  The gp-30352 hint-refresh word (0x22B100/0x22B108/
 * 0x22B118 feeding 0x22B138) is not ported; the widgets set their own
 * hints. */
static void
ConfigMenuInput(void)
{
	ConfigItem *it;
	int old;

	if(!MenuConfigFullyOpen())	/* real: 0x227D08's state-2 gate */
		return;
	it = &configMenu.items[configCursor];

	if(configMenu.mode == 1) {		/* real: 0x227BE8 */
		if(pad.press & PAD_CIRCLE) {	/* the confirm button, 0x20 */
			MenuConfigCubeKick(configCursor);	/* *(gp-32136) */
			if(configCursor == 0)
				ClockEditApply(it);	/* real: +0x20, 0x21EAE0 */
			else
				ConfigItemApply(it);	/* real: +0x20 */
			configMenu.mode = 0;
			cfgClick(4);		/* real: 0x2287A8(20992,1,4) */
			configMenu.phase = 0;	/* real: 0x27BE28+0x34 = 0 */
		} else if(pad.press & PAD_CROSS) {	/* the cancel button, 0x40 */
			if(configCursor == 0)
				ClockEditCancel(it);	/* real: +0x24, 0x21EB30 */
			configMenu.mode = 0;	/* the others' +0x24 is a nop */
			cfgClick(10);		/* real: 0x2287A8(20992,1,10) */
			configMenu.phase = 0;
		}
		return;
	}

	/* real: 0x2279B8 - one arm a frame, in the ROM's own order */
	old = configCursor;
	if(pad.press & PAD_UP) {
		if(--configCursor < 0)
			configCursor = configMenu.count-1;
		ConfigItemFocus(old, 0);	/* real: the two +0x28 calls - */
		ConfigItemFocus(configCursor, 1); /* the hint-bar handover */
		cfgClick(5);		/* real: 0x2287A8(20992,1,5) */
	} else if(pad.press & PAD_DOWN) {
		if(++configCursor >= configMenu.count)
			configCursor = 0;
		ConfigItemFocus(old, 0);
		ConfigItemFocus(configCursor, 1);
		cfgClick(5);
	} else if(pad.press & PAD_CIRCLE) {	/* the confirm button, 0x20 */
		if(configCursor == 0)
			ClockEditOpen(it);	/* real: +0x14, 0x21DF28 */
		else
			ConfigItemResync(it);	/* real: 0x21EE50 -> 0x21EDB8 */
		configMenu.phase = 0;		/* real: 0x21EE50's 0x27BE5C */
		configMenu.mode = 1;
		cfgClick(4);
		printf("osdsys: config item %d (\"%s\") opened\n", configCursor,
			osdGetString(it->strid));
	} else if(pad.press & PAD_CROSS) {	/* the cancel button, 0x40 */
		MenuLeaveConfig();	/* real: 0x227338 - close the Anim */
		cfgClick(10);		/* real: 0x227338's 0x2287A8 tail */
	} else if(pad.press & PAD_TRIANGLE) {
		/* real: 0x227B90's `lw v1,16(s0); bnez v1' - triangle only
		 * acts on item 0, where the hint bar offers "Options"; the
		 * 0x223658 screen switch is not ported, so it leaves. */
		if(configCursor == 0) {
			cfgClick(4);	/* real: 0x227BB0 */
			MenuLeaveConfig();
		}
	}
}

/* real: 0x228050 - open the menu once the module's fade-up has come far
 * enough, close it again when the fade-out is complete.  The real guard
 * 0x227FC0 ("no other screen is open") is MenuConfigOpen() here. */
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
	/* real: 0x228110's own guard 0x227FC0 - the main menu's labels are
	 * suppressed the moment another screen's Anim leaves state 0 */
	if(MenuConfigOpen())
		return;
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

/* ======================== the OSD chrome ========================
 *
 * Stages 6, 7 and 8 of the frame body 0x21CF20 - drawn after every 2D
 * screen and in this order: 0x21D368 (the letterbox bars), 0x21D3A0
 * (the date/time header) and 0x21DA68 -> 0x21D7F8 (the button hint
 * bar).  All three read the screen-size setting through 0x22B0E8(0);
 * the port has no settings block, so they read the Screen Size item's
 * own +0x08, which is the same number (0x21EDB8 re-syncs it from
 * there every frame). */

/* real: uiModel[0] - 0 = 4:3, 1 = Full, 2 = 16:9 */
static int
osdScreenType(void)
{
	return configItems[1].value;
}

/* real: 0x27B450, the pixel aspect 0x21C9D0 stores (0x27B44C, its
 * divisor, is always 1.0).  Every "how tall is a field line" fudge in
 * the module is this pair of constants. */
#define PIXASPECT (IsPAL() ? 0.5405f : 0.47f)
#define PALSTRETCH (0.5405/0.47)

/* real: 0x2299C0 on an untextured record - 0x27B4F0's +0x30..+0x3C are
 * all zero, so 0x2298A8 emits a flat SPRITE with no TME. */
static void
osdFlatRect(int x0, int y0, int x1, int y1, const int *col, int alpha)
{
	int xoff16, yoff16;

	xoff16 = ((4096 - screenW)/2) << 4;
	yoff16 = ((4096 - screenH)/2) << 4;
	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 0, 0, 1, 0, 0, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2], alpha, 0x3f800000));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(x0 + xoff16, y0 + yoff16, 1));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(x1 + xoff16, y1 + yoff16, 1));
	vif1End();
}

/* real: 0x27B4F0's colour - opaque black */
static const int colBlack[4] = { 0, 0, 0, 128 };

/* real: 0x27B750 - the hint labels' and the clock's colour */
static const int hintTextCol[4] = { 96, 96, 96, 128 };

/* real: 0x21D368 -> 0x21D1F8.  The gate is uiModel[0] == 0 || == 2, so
 * the bars are there in 4:3 AND in 16:9 and only "Full" loses them; on
 * a 640x224 NTSC field the content band is 640*0.5625*0.47 = 169.2
 * lines and each bar is 27.4.  Note the two bars are NOT computed
 * symmetrically in the ROM - the top one truncates (bar*16) and the
 * bottom one truncates ((h - bar)*16) - so they can differ by one
 * sixteenth of a pixel.  Reproduced. */
static void
DrawLetterbox(void)
{
	float content, bar;
	int t;

	t = osdScreenType();
	if(t != 0 && t != 2)
		return;

	/* real: 0x22A0C0(1, 1) - normal blend, ZTST ALWAYS */
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);

	content = (float)screenW * 0.0625f * 9.0f * PIXASPECT;
	bar = ((float)screenH - content) * 0.5f;
	osdFlatRect(0, 0, screenW << 4, (int)(bar * 16.0f), colBlack, 128);
	osdFlatRect(0, (int)(((float)screenH - bar) * 16.0f),
		screenW << 4, screenH << 4, colBlack, 128);
}

/* real: 0x2A3EC8 / 0x2A3F38, the two format strings 0x20A998 and
 * 0x20AAA0 pick when the date format is 0 (Y/M/D) and the clock face is
 * 24-hour.  The ROM brackets each field with \7p@0 / \7p00 (fixed width
 * on '0'), a no-op for the Latin face, and sprintf()s the time into
 * "%s %s" (0x2A7830) behind a prefix that is empty unless 0x203928()
 * says so - hence the leading space.  The other two date orders
 * (0x2039A8 = 1 or 2) and the 12-hour face are not ported: the port has
 * no settings to select them. */
static void
cfgFmtClockLine(char *p, int date)
{
	char num[12];

	if(date) {
		cfgFmtNum(num, cfgClockDate[0], 4, 1); strcpy(p, num); p += strlen(num);
		*p++ = '/';
		cfgFmtNum(num, cfgClockDate[1], 2, 1); strcpy(p, num); p += strlen(num);
		*p++ = '/';
		cfgFmtNum(num, cfgClockDate[2], 2, 1); strcpy(p, num); p += strlen(num);
		*p = 0;
		return;
	}
	*p++ = ' ';			/* real: the "%s %s" with an empty %s */
	cfgFmtNum(num, (int)MenuClockHours(), 2, 0); strcpy(p, num); p += strlen(num);
	*p++ = ':';
	cfgFmtNum(num, (int)MenuClockMinutes(), 2, 1); strcpy(p, num); p += strlen(num);
	*p++ = ':';
	cfgFmtNum(num, (int)MenuClockSeconds(), 2, 1); strcpy(p, num); p += strlen(num);
	*p = 0;
}

/* real: 0x21D3A0 - the date at the left margin and the time at the
 * right, both at scale 0.83 (gp-32220) in 0x27B750's grey, on one row
 * whose y is 14 unless the screen is 16:9, when it is 32.
 *
 * Its alpha is 0x226A60, which is 128 while the timer 0x27C258 is open
 * and otherwise ramps with the SYSTEM CONFIGURATION screen's own timer
 * 0x27BE44 - so on the bare main menu the header is invisible, and it
 * fades up with System Configuration.  That ramp is byte for byte
 * MenuConfigAlpha(), which is why the port can just call it.
 *
 * NOT original: the ROM reads Y/M/D and h/m/s out of uiModel[6..11],
 * the RTC snapshot.  osdbits has hh:mm:ss from argv and no date, so the
 * date reads 2000/01/01 - the same fixed date the Clock Adjustment
 * value row already shows (cfgClockDate). */
static void
DrawTopBar(int fadeAlpha)
{
	char buf[32];
	int alpha, y;

	alpha = MenuConfigAlpha(fadeAlpha);	/* real: 0x226A60 */
	y = osdScreenType() == 2 ? 32 : 14;
	if(IsPAL())
		y = (int)((double)y * PALSTRETCH);

	osdTextSetScale(0.83f);			/* real: 0x207F68(gp-32220) */
	cfgFmtClockLine(buf, 1);
	drawTextL(22, y, hintTextCol, alpha, buf);
	cfgFmtClockLine(buf, 0);
	drawTextL(screenW - osdTextWidth(buf) - 22, y, hintTextCol, alpha, buf);
	osdTextSetScale(1.0f);
}

/* ------------------------- the hint bar -------------------------
 *
 * The glyphs are NOT font glyphs: 0x21D590 (DrawIcon) binds texture
 * slot 8 or 9 - TEXCSTSL (START/SELECT, 64x32) or TEXCMARU (the four
 * shape buttons, 64x64) - and emits one 25x25 sprite, drawn at half
 * height because the OSD renders one field.  Both are plain PSMCT32
 * with their colour IN the texture (pink square, green triangle, blue
 * cross, red circle), so there is no per-glyph CLUT and no vertex
 * colour: the record's RGB is a flat 128,128,128 and only its alpha is
 * the argument.  The slot table is 0x27F1C0 (12-byte {ptr, log2w,
 * log2h} records, slot i <- resource 45+i) and 0x27F280 (the VRAM
 * bases), filled by 0x229698/0x229750. */

static Texture maruTexture = {
	nil, RESID_TEXCMARU, nil, 0, { 0, 0, 64, 64 },
	0, 0, SCE_GS_PSMCT32, 0, { 0 }
};

/* real: 0x27B570 - six glyph rects.  0/1 are START and SELECT inside
 * TEXCSTSL; 2..5 are TEXCMARU's 2x2 grid: square, triangle, cross,
 * circle (in that memory order). */
static const int iconUV[6][4] = {
	{  0,  0, 32, 32 }, { 32,  0, 64, 32 },
	{  0,  0, 32, 32 }, { 32,  0, 64, 32 },
	{  0, 32, 32, 64 }, { 32, 32, 64, 64 }
};

/* real: 0x27B7E0 - the glyph each of the four hint slots uses, i.e.
 * square, cross, circle, triangle from left to right */
static const int hintGlyph[4] = { 2, 4, 5, 3 };

/* real: 0x21D590.  The record is 0x27B530; `w' and `h' are 28 for the
 * TEXCSTSL pair and 25 for the shape buttons, and the sprite is only
 * h/2 tall (one field), stretched by 0.5405/0.47 on PAL. */
static void
DrawIcon(int glyph, int x, int y, int alpha)
{
	const int *uv;
	int w, h, x0, y0, x1, y1, xoff16, yoff16;

	if((u32)glyph >= 6)
		return;
	/* real: 0x22AB90(8, 0, 1) for glyphs 0..1.  Neither of the two
	 * screens ported here ever asks for START or SELECT, so TEXCSTSL is
	 * not uploaded and those two glyphs are dropped. */
	if(glyph < 2)
		return;
	w = h = 25;

	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);
	vif1SetTexture(&maruTexture);
	vif1SetCLAMP_1(1, 1, 0, 0, 0, 0);

	uv = iconUV[glyph];
	xoff16 = ((4096 - screenW)/2) << 4;
	yoff16 = ((4096 - screenH)/2) << 4;
	x0 = x << 4;
	y0 = y << 4;
	x1 = (x + w) << 4;
	y1 = (y + h/2) << 4;
	if(IsPAL())
		y1 = y0 + (int)((double)(y1 - y0) * PALSTRETCH);

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(128, 128, 128, alpha, 0x3f800000));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(uv[0]*16 + 8, uv[1]*16 + 8));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(x0 + xoff16, y0 + yoff16, 1));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(uv[2]*16 + 8, uv[3]*16 + 8));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(x1 + xoff16, y1 + yoff16, 1));
	vif1End();
}

/* real: 0x27B5E8 - nine 20-byte hint sets of {4 string ids, pad mask};
 * id 1 means "this slot has no button".  The second block is the same
 * table 180 bytes on, taken when 0x204318() is true - the region where
 * the cross confirms.  Sets 3..6 belong to screens this port does not
 * have; they are kept because the table is one object. */
static const int hintSet[2][9][5] = {
	{			/* real: 0x27B5E8 */
		{  1,  1,  1,  1, 0x0000 },
		{  1,  1, 86, 95, 0x5000 },	/* main menu */
		{ 94, 85, 86,  1, 0x5000 },	/* System Configuration */
		{  1, 85,  1, 87, 0x5000 },
		{  1, 85,  1,  1, 0x5000 },
		{  1, 85, 86,  1, 0x5000 },
		{ 94,  1,  1,  1, 0x0000 },
		{  1, 85, 86,  1, 0x5000 },
		{  0,  0,  0,  0, 0x0000 }
	}, {			/* real: 0x27B69C */
		{  1,  1,  1,  1, 0x0000 },
		{  1, 85,  1, 95, 0x5000 },	/* main menu */
		{ 94, 85, 86,  1, 0x5000 },	/* System Configuration */
		{  1,  1, 86, 87, 0x5000 },
		{  1,  1, 86,  1, 0x5000 },
		{  1, 85, 86,  1, 0x5000 },
		{ 94,  1,  1,  1, 0x0000 },
		{  1, 85, 86,  1, 0x5000 },
		{  0,  0,  0,  0, 0x0000 }
	}
};

/* real: 0x27B760 - eight languages x four slot x positions.  Slot 3's
 * column is dead data: 0x21D7F8 right-anchors that slot on the screen
 * edge instead. */
static const int hintX[8][4] = {
	{ 20, 226, 332, 508 },	/* Japanese */
	{ 24, 213, 335, 441 },	/* English */
	{ 24, 188, 302, 483 },	/* French */
	{ 24, 197, 343, 471 },	/* Spanish */
	{ 24, 193, 333, 477 },	/* German */
	{ 24, 233, 350, 483 },	/* Italian */
	{ 24, 193, 362, 477 },	/* Dutch */
	{ 24, 164, 313, 500 }	/* Portuguese */
};

/* real: 0x27B5D0 plus its three setters 0x21D748 (arm, gp-30768),
 * 0x21D750 (alpha, gp-30772) and 0x21D758 (the pad mask) */
static int hintCustom[5] = { 1, 1, 1, 1, 0 };
static int hintCustomOn;
static int hintCustomAlpha = 128;

/* real: 0x21D768(a, b, c, d).  Its region arm is the confirm/cancel
 * swap, and it is subtle: it puts argument c on the CROSS slot and
 * argument b on the CIRCLE slot, rewriting 86 -> 85 and 85 -> 86 on the
 * way so that osdGetString's own 85/86 exchange (which fires under the
 * same flag) restores the intended words.  Net effect with the usual
 * (b, c) = (85, 86): flag clear -> cross "Back", circle "Enter"; flag
 * set -> cross "Enter", circle "Back", which is what the retail
 * screenshots show. */
static void
HintSetCustom(int a, int b, int c, int d)
{
	hintCustom[0] = a;
	if(textRegionSwap) {
		hintCustom[1] = c == 86 ? 85 : c;
		hintCustom[2] = b == 85 ? 86 : b;
	} else {
		hintCustom[1] = b;
		hintCustom[2] = c;
	}
	hintCustom[3] = d;
}

/* real: 0x21EB80 - item 0 (Clock Adjustment)'s +0x28 focus callback,
 * and the ONLY reason the config screen's bar has four hints instead of
 * set 2's three: the other four items' +0x28 is 0x21F160, a bare
 * `jr ra'.  Move the cursor off Clock Adjustment on retail and the
 * triangle/"Options" hint goes away with it. */
static void
ConfigItemFocus(int i, int on)
{
	if(i != 0)
		return;
	if(!on) {
		hintCustomOn = 0;	/* real: 0x21D748(0) */
		return;
	}
	HintSetCustom(94, 85, 86, 87);	/* Display / Back / Enter / Options */
	hintCustom[4] = 0x5000;		/* real: 0x21D758(0x5000), TRIANGLE|CROSS */
	hintCustomOn = 1;		/* real: 0x21D748(1) */
}

/* real: gp-30404, 0x227D08's "already notified" latch */
static int cfgFocusNotified;

/* real: the head of 0x227D08 - fire the cursor item's +0x28 with 1 on
 * the frame the screen becomes fully open (state 2 with no sub-screen
 * up) and with 0 when it stops being.  The ROM's condition is the same
 * 0x22AC48 pair the pad gate uses, not an alpha threshold. */
static void
ConfigFocusNotify(void)
{
	if(MenuConfigFullyOpen()) {
		if(!cfgFocusNotified) {
			ConfigItemFocus(configCursor, 1);
			cfgFocusNotified = 1;
		}
	} else if(cfgFocusNotified) {
		ConfigItemFocus(configCursor, 0);
		cfgFocusNotified = 0;
	}
}

/* real: 0x228660 - the six-entry jump table at 0x2A4B50, one per-screen
 * alpha function, and then `movn v0, zero, (v0 < 128)'.  That is NOT a
 * clamp above 127 as docs/menu-draw.md 9.1 has it: it ZEROES anything
 * below 128, so the hint sets never cross-fade - a set appears only
 * while its screen is fully up.  Set 0 is unreachable (the table is
 * indexed by set-1 and bounded at 6). */
static int
HintSetAlpha(int set, int fadeAlpha)
{
	int a;

	switch(set) {
	case 1:				/* real: 0x227E18 */
		/* the ROM's 0x227E18 also multiplies in
		 * clamp(dur10 - timerCount(0x27BE44), 0, dur10), which is 0
		 * from ten frames into System Configuration's entry - the same
		 * thing DrawMainMenu's MenuConfigOpen() guard does here */
		if(MenuConfigOpen())
			return 0;
		a = MainMenuAlpha(fadeAlpha);
		break;
	case 2:				/* real: 0x2271B8 */
		a = MenuConfigAlpha(fadeAlpha);
		break;
	default:			/* screens this port does not have */
		return 0;
	}
	return a < 128 ? 0 : a;
}

/* real: 0x21D7F8(set, alpha, y) */
static void
DrawHintSet(int set, int alpha, int y)
{
	const int *ids;
	const int *xs;
	const char *s;
	int i, id, w;

	ids = set == 8 ? hintCustom : hintSet[textRegionSwap ? 1 : 0][set];
	xs = hintX[clamp(GetLanguage(), 0, 7)];

	osdTextSetScale(0.8f);		/* real: 0x207F68(gp-32216) */
	for(i = 0; i < 4; i++) {
		id = ids[i];
		if(id == 1)		/* real: 1 = no button in this slot */
			continue;
		s = osdGetString(id);
		if(i < 3) {
			DrawIcon(hintGlyph[i], xs[i], y, alpha);
			drawTextL(xs[i] + 28, y, hintTextCol, alpha, s);
		} else {
			/* real: slot 3 is right-anchored on the screen edge */
			w = osdTextWidth(s) + 24;
			DrawIcon(hintGlyph[3], screenW - w - 28, y, alpha);
			drawTextL(screenW - w, y, hintTextCol, alpha, s);
		}
	}
	osdTextSetScale(1.0f);		/* real: the tail 0x207F68(1.0) */
}

/* real: 0x21D9E0 - the bar's row, 182 in 16:9 and 200 otherwise */
static int
HintBarY(void)
{
	int y;

	y = osdScreenType() == 2 ? 182 : 200;
	if(IsPAL())
		y = (int)((double)y * PALSTRETCH);
	return y;
}

/* real: 0x21DA68 - the caller-supplied set wins outright; otherwise
 * every set whose screen is fully open gets drawn (in practice exactly
 * one, since 0x228660 zeroes anything under 128).  The middle arm,
 * `else if (0x226A48()) 0x21D7F8(7, 128, y)', tests *(0x27BE40), a mode
 * flag 0x228460 clears and nothing on these two screens sets. */
static void
DrawHintBar(int fadeAlpha)
{
	int i, y, a;

	y = HintBarY();
	if(hintCustomOn) {
		DrawHintSet(8, hintCustomAlpha, y);
		return;
	}
	for(i = 0; i < 7; i++) {
		a = HintSetAlpha(i, fadeAlpha);
		if(a > 0)
			DrawHintSet(i, a, y);
	}
}

/* ============================== entry ============================== */

/* real: the text-engine part of 0x21CE58 - do_load_font (0x21DBA0) and
 * the per-screen init 0x228460 */
void
InitMenuText(void)
{
	int rate, i;

	menuTextEnable = OsdArgInt(8, 0) ? 0 : 1;
	menuTextDumpFrame = OsdArgInt(9, 0);
	if(!menuTextEnable)
		return;

	/* NOT original: argv[16] stands in for 0x204318's region word */
	textRegionSwap = OsdArgInt(16, 1);

	/* real: 0x20A3C8's four 0x20A280 uploads, of which the port needs
	 * the first (FNTASCII, slot 0) and the last (FNTEXOSD, slot 3);
	 * FNTEX000/001 are the two-byte pages.  TEXCMARU is not a font page
	 * at all - it is texture slot 9, uploaded by 0x22A9B8's own loop. */
	InitTexture(&fontTexture);
	InitTexture(&fontOsdTexture);
	InitTexture(&maruTexture);

	/* real: 0x209FD0's defaults, then do_load_font's tail */
	textGap = -3;			/* 0x207F38(-3) */
	textYBias = -7*8;		/* 0x207F48(-7) */
	textAdvMul = 1;			/* .data at 0x27156C */
	textPct = 0;			/* real: 0x271564 */
	osdTextSetColor(128, 128, 128, 128);
	osdTextSetScale(1.0f);
	osdTextSetBaseScale(IsPAL() ? 1.15f : 1.0f);

	/* real: 0x228460 - refreshRate/6 into both the Anim's duration and
	 * gp-30396 */
	rate = IsPAL() ? 50 : 60;
	mainMenuDur = rate/6;
	memset(&mainMenuAnim, 0, sizeof(mainMenuAnim));
	mainMenuAnim.duration = mainMenuDur;

	/* real: 0x227390's tail folds the marker's phase at
	 * refreshRate*31400/60 - 31400 on NTSC, ~203 frames a lap */
	cfgPhaseFold = rate*31400/60;
	configMenu.phase = 0;
	cfgFocusNotified = 0;
	hintCustomOn = 0;

	mainMenu.cursor = clamp(OsdArgInt(7, 0), 0, mainMenu.count-1);
	/* real: 0x27BE28's cursor, moved by 0x2279B8's UP/DOWN arms */
	configCursor = clamp(OsdArgInt(15, 0), 0, configMenu.count-1);
	memset(configItemAlpha, 0, sizeof(configItemAlpha));

	/* real: 0x21ED18, called from the per-screen init 0x228460
	 * (0x2284D4) - wire the region's language table into items[4]
	 * (region 0 gets the two-entry 0x27B8C0; the port's textRegionSwap
	 * stands in for regions 1/2, whose tables are identical) */
	configItems[4].nvalues = 7;		/* real: `li v0,7' */
	configItems[4].values = cfgLanguage;	/* real: 0x27B920 / 0x27BA70 */

	/* real: the 0x352880 block starts as the NVRAM copy the OSD loader
	 * read; osdbits has no NVRAM, so every item starts on its first
	 * value and the items' +0x08 already agree (all 0 in .data) */
	memset(cfgSettings, 0, sizeof(cfgSettings));
	for(i = 1; i < configMenu.count; i++)
		if(configItems[i].values)
			cfgSettings[configItems[i].setting] =
				configItems[i].values[0].value;
	for(i = 0; i < 4; i++)
		cfgPersisted[i] = cfgSettings[i];
	cfgClockDate[0] = 2000;			/* real: the RTC's date */
	cfgClockDate[1] = 1;
	cfgClockDate[2] = 1;

	/* real: 0x227560's own three rows.  The title's y is a literal 88,
	 * or 101 when 0x204350() (IsPAL) says so, and the two rows under it
	 * are that plus the doubles 24.0 / 42.0, or 27.6 / 48.3 on PAL - the
	 * same two numbers times the 1.15 base scale do_load_font hands
	 * 0x2080D0.  The ROM adds them as doubles and truncates. */
	cfgTitleY = IsPAL() ? 101 : 88;
	cfgLabelY = (int)((double)cfgTitleY + (IsPAL() ? 27.6 : 24.0));
	cfgValueY = (int)((double)cfgTitleY + (IsPAL() ? 48.3 : 42.0));

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
	/* real: 0x228278's own 0x227FC0 guard - once System Configuration is
	 * up, its own handler (0x2279B8) owns the pad */
	if(MenuConfigOpen())
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
	/* real: 0x226BB8, from the cube stage 0x226FA8 - one hub slot ahead
	 * of both 2D layers, and running whether or not the screen is open */
	ConfigMenuStepItems();
	mtStep(&mainMenuAnim);
	MainMenuStep(fadeMode, fadeAlpha);
	MainMenuInput();
	/* real: 0x227D08's head, which runs after 0x227560's draw and before
	 * the pad handler it dispatches to */
	ConfigFocusNotify();
	ConfigMenuInput();
	DrawMainMenu(fadeAlpha);
	/* real: 0x227DE8's tail 0x227D08, one slot earlier in 0x2283F0 */
	DrawConfigMenu(fadeAlpha);

	/* real: stages 6, 7 and 8 of 0x21CF20, in this order and after every
	 * 2D screen.  (Stage 9, 0x21DB18's right-edge strip 0x27B7F0, is not
	 * ported - it draws nothing on either of these two screens.) */
	DrawLetterbox();		/* real: 0x21D368 */
	DrawTopBar(fadeAlpha);		/* real: 0x21D3A0 */
	DrawHintBar(fadeAlpha);		/* real: 0x21DA68 */
	/* the readback itself moved to DoMenuScene's post-swap window */
}
