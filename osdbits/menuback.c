/* The main menu's BACKDROP and the frame composite - Module U's
 * 0x21D0A0 (stage 2 of the frame body 0x21CF20), which runs between the
 * camera (0x21CFD8, menu.c's MenuCamera) and the 3D object list
 * (0x2268F0, menu.c's SceneReset/UpdateOrbs/SceneWalk).
 *
 * Two things live here:
 *
 *  1. THE TEXCKABE "WALL" (0x229358 -> 0x2292D0 -> 0x229130 ->
 *     0x2288C0 + 0x228E78).  Not a wall and not a smoke plane: it is a
 *     6000-unit-radius CYLINDER around the camera, 16 angular ribbons x
 *     33 rings running from z = -2500 (behind the camera) to z = 37500,
 *     capped at z = 38750 by a degenerate ring on the axis.  You are
 *     inside it, looking down it - a tunnel whose far end is black.
 *     The radius breathes with a travelling sine wave and the texture
 *     scrolls lengthwise, which is what reads as slow smoke.
 *
 *  2. THE COMPOSITE (0x21D0A0's tail).  The mesh is drawn BRIGHT into
 *     the screen buffer, copied out to two offscreen buffers, and the
 *     first copy is then drawn back over the screen as one opaque
 *     full-screen textured quad modulated by {0x37,0x28,0x3C} - i.e.
 *     x0.43 red, x0.31 green, x0.47 blue.  That multiply is where the
 *     menu's deep blue comes from; the wall itself is nearly white.
 *     0x22C3C0's zoom blur (down-scale to an offscreen buffer and back
 *     up, N times) runs in the same stage but only while a screen
 *     transition is in progress.
 *
 * Hex addresses are retail-image addresses; gp = 0x2AF070.  See
 * docs/menu-scene.md 10.9 and docs/menu-draw.md 3-4 and 7.3. */

#include <stdio.h>
#include "inc.h"
#include "res.h"

/* ===================== TEXCKABE (resource 46, TEXC slot 1) =====================
 *
 * The blob is 64x64 packed RGB triples.  The ROM's decoder for this slot
 * (0x22A7F4, reached through the per-slot jump table at 0x2A4BA0 - every
 * other TEXC slot uses either the grey expander 0x22A720 or the
 * white-with-alpha one 0x22A790 that opening.c calls "format 3") writes
 * each source pixel FOUR times, tiling the 64x64 image 2x2 into the
 * 128x128 page the descriptor at 0x27F1C0+12 declares (wexp = hexp = 7).
 * Alpha is a constant 0x7F.
 *
 * The tiling is not cosmetic: the mesh's S coordinate runs 0..3 over the
 * page with CLAMP_1 = REPEAT, so a 64x64 texture with the same UVs would
 * wrap half as often and halve the apparent detail. */

#define KABEW 128
#define KABEH 128

static u32 kabeTexels[KABEW*KABEH];

static Texture kabeTexture = {
	(u8*)kabeTexels, RESID_TEXCKABE, nil, 0, { 0, 0, KABEW, KABEH },
	0, 0, SCE_GS_PSMCT32, 0, { 0 }
};

/* real: 0x22A7F4 */
static void
DecodeKabe(void)
{
	u8 *src;
	u32 p;
	int x, y;

	src = GetResourceData(RESID_TEXCKABE);
	for(y = 0; y < KABEH/2; y++)
		for(x = 0; x < KABEW/2; x++) {
			p = src[0] | src[1]<<8 | src[2]<<16 | 0x7F000000;
			src += 3;
			kabeTexels[(y         )*KABEW + x         ] = p;
			kabeTexels[(y         )*KABEW + x + KABEW/2] = p;
			kabeTexels[(y+KABEH/2 )*KABEW + x         ] = p;
			kabeTexels[(y+KABEH/2 )*KABEW + x + KABEW/2] = p;
		}
}

/* ========================= the mesh's constants =========================
 * All five are plain floats in .data, read through gp: */
#define KABE_RADIUS	6000.0f		/* real *(gp-32096) = 0x2A7310 */
#define KABE_ANGDIV	65535.0f	/* real *(gp-32120) / *(gp-32104) */
#define KABE_SCROLL	0.0002f		/* real *(gp-32116) / *(gp-32108) */
#define KABE_WOBBLE	0.05f		/* real *(gp-32112) */
#define KABE_NEARZ	2050.0f		/* real *(gp-32124), a camera-space cull */
#define KABE_CAPZ	38750.0f	/* real *(gp-32100), the far cap ring */

#define NSEG	16	/* 0x2292D0's loop: 16 x 0x1000 = one 16-bit turn */
#define NRING	33	/* 0x2288C0's loop bound */
#define RINGSTEP	1250.0f	/* z step per ring */
#define RINGZ0	(-2500.0f)	/* z of ring 0 */

/* the module frame counter, real *(gp-30392): 0x2287D0 bumps it every
 * frame and 0x2287B0 (initBgTimer) zeroes it at module entry.  osdbits'
 * global frameCount is the same thing for menu mode, but keep a private
 * copy so the backdrop's phase does not depend on menu.c's bookkeeping. */
static int backFrame;

/* ================== the backdrop's fade timer (0x27F190) ==================
 * 0x229358 steps it every frame and, whenever it is not in state 0, runs
 * 0x229278 (bgFadeUpdate) to set three gp words that are added to every
 * vertex colour.  Nothing on the always-on path opens the timer, so in
 * the idle menu all three stay 0; the ROM's own initBgTimer (0x2287B0)
 * zeroes them explicitly.  Ported for completeness - if a caller is ever
 * found that opens it, the wall brightens by up to 40 over 40 frames. */
typedef struct BackTimer BackTimer;
struct BackTimer
{
	int duration, count, edge, state;
};

static BackTimer bgTimer;		/* real: 0x27F190 */
static int bgFade0, bgFade1, bgFade2;	/* real: gp-28840/-28836/-28832 */

/* real: 0x22ACC0 */
static void
BackTimerStep(BackTimer *t)
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

/* real: 0x22AC20 */
static int
BackTimerInterp(BackTimer *t, int n)
{
	return t->duration ? t->count*n/t->duration : 0;
}

/* real: 0x2291E8 and 0x229230, the timer's ONLY opener and closer.  A
 * whole-image scan of the call graph finds exactly one call site each:
 * 0x2272B8 inside "enter System Configuration" (0x227268) and 0x227478
 * inside that screen's state machine (0x227390).  Nothing else in the
 * image touches 0x27F190's state, so the timer being open is precisely
 * "the System Configuration screen is up". */
void
MenuBackFadeOpen(void)
{
	if(bgTimer.state == 0) {
		bgTimer.count = 0;
		bgTimer.edge = 1;
		bgTimer.state = 1;
	}
}

void
MenuBackFadeClose(void)
{
	if(bgTimer.state == 2) {
		bgTimer.edge = 1;
		bgTimer.count = bgTimer.duration;
		bgTimer.state = 3;
	}
}

/* NOT original as a gate.  0x229358 draws the tunnel whenever the
 * module-wide fade is idle, on every screen - and a retail GS dump of
 * the main menu really does contain all sixteen ribbons, bright.  Yet
 * the retail main menu measures (0,0,0) everywhere outside the orb glow
 * and aap confirms from the console that the tunnel is a System
 * Configuration-only sight (docs/menu-backdrop.md 11 and its
 * resolution).  Whatever makes the retail main menu black happens below
 * the GIF stream and has not been found; the ROM's own per-screen signal
 * for this screen is 0x27F190, so the port keys the mesh on it. */
int
MenuBackdropVisible(void)
{
	return bgTimer.state != 0;
}

/* ======================== small math helpers ========================
 *
 * The ROM does its trig with a 16384-entry quarter-wave table at
 * 0x3581F0 through mdSin (0x230018) and mdCos (0x230068 = mdSin(a +
 * 0x4000)); both take the angle as a SIGNED 16-bit value, a full circle
 * being 0x10000.  menu.c already uses libm for the same thing. */
static float
mdSinf(int a)
{
	return sinf((short)a * (TAU/65536.0f));
}

static float
mdCosf(int a)
{
	return cosf((short)a * (TAU/65536.0f));
}

/* real: sceVu0ApplyMatrix 0x2678A8 - o = m x v, m[col][row] */
static void
backApply(sceVu0FVECTOR o, sceVu0FMATRIX m, sceVu0FVECTOR v)
{
	sceVu0FVECTOR t;
	int r;

	for(r = 0; r < 4; r++)
		t[r] = m[0][r]*v[0] + m[1][r]*v[1] + m[2][r]*v[2] + m[3][r]*v[3];
	for(r = 0; r < 4; r++)
		o[r] = t[r];
}

/* real: 0x228838 - ApplyMatrix by the view-screen matrix, then divide
 * the whole vector by w and return q = 1/w.  q becomes the vertex's Q
 * (RGBAQ bits 32..63) and pre-multiplies its ST, which is what makes the
 * texturing perspective correct. */
static float
backProject(sceVu0FVECTOR v, sceVu0FMATRIX vs)
{
	float q;

	backApply(v, vs, v);
	q = 1.0f/v[3];
	v[0] *= q; v[1] *= q; v[2] *= q; v[3] *= q;
	return q;
}

/* ===================== the ribbon emitter (0x229130) =====================
 *
 * 0x2292D0 binds TEXCKABE once (0x22AB90(1,1,2)) and then, sixteen
 * times, emits a PRIM packet (0x2287E0) and one ribbon (0x229130).  Each
 * ribbon is one TRIANGLE STRIP of 33 vertex PAIRS - the two edges of a
 * 4096-unit-wide angular slice - plus the cap pair from 0x228E78, and
 * the PRIM in between is what breaks the strip between ribbons.
 *
 * Per ring i (0x2288C0's body):
 *   scale = 1 + 0.05*sin16(frame*100 + i*5120)      a travelling wave
 *   P     = (R*sin(a)*scale, R*cos(a)*scale, i*1250 - 2500, 1)
 *   cull if the camera-space z < 2050, or if the projected x or y is
 *        more than 1000 from 2000 (the ROM's own sloppy screen-centre
 *        constant - the real centre is 2048)
 *   ST    = (a/65535 * 3 * q, (i/32 + scroll) * 3 * q)
 *   p     = (32-i)^3 >> 10                          brightness ramp
 *   RGB   = (p*230>>5, p*260>>5, p*260>>5) + bgFade + (cos(a)+1)*10
 *   A     = 0x40
 * so the near end of the tunnel is bright and the far end is black, and
 * the ribbon facing +Y is 20 brighter than the one facing -Y.  Nothing
 * clamps the sum, which can reach 290 if the fade timer is ever opened
 * - the ROM would spill red into green there.
 *
 * A culled pair is simply not emitted, which shortens the strip; because
 * only the leading (nearest, off-screen) rings ever fail, that never
 * leaves a hole in the middle of the ribbon. */

/* one vertex: RGBAQ, ST, XYZF2 - exactly the register list of the
 * REGLIST GIFtag template at 0x27F1B0 (NREG 3, REGS {1, 2, 4}), here as
 * three A+D pairs because that is what osdbits' packet layer emits. */
static void
KabeVertex(sceVu0FVECTOR p, float q, float s, float t, int r, int g, int b)
{
	float sq = s*3.0f*q;		/* real: 0x228898, the 3.0 is literal */
	float tq = t*3.0f*q;

	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(r, g, b, 0x40, *(u32*)&q));
	pktSetAD(SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&sq, *(u32*)&tq));
	/* real: sceVu0FTOI4 (0x267668) - x16 fixed point, truncated */
	pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF((int)(p[0]*16.0f), (int)(p[1]*16.0f),
		(int)(p[2]*16.0f), 0));
}

/* real: 0x229130 (the packet) + 0x2288C0 (the body) + 0x228E78 (the cap) */
static void
DrawKabeRibbon(sceVu0FMATRIX cam, sceVu0FMATRIX vs, int ang0, int ang1)
{
	sceVu0FVECTOR e0, e1, p0, p1;
	float s0, s1, scroll, scale, q0, q1, tc;
	int tint0, tint1, i, rr, gg, pp, k;

	e0[0] = KABE_RADIUS*mdSinf(ang0);
	e0[1] = KABE_RADIUS*mdCosf(ang0);
	e0[2] = 0.0f; e0[3] = 1.0f;
	e1[0] = KABE_RADIUS*mdSinf(ang1);
	e1[1] = KABE_RADIUS*mdCosf(ang1);
	e1[2] = 0.0f; e1[3] = 1.0f;

	/* the S coordinate uses the UNSIGNED angle (0 .. 61440), so it runs
	 * 0 -> 0.9375 around the tunnel and, x3, wraps the page three times */
	s0 = ang0 / KABE_ANGDIV;
	s1 = ang1 / KABE_ANGDIV;
	tint0 = (int)((mdCosf(ang0) + 1.0f)*10.0f);
	tint1 = (int)((mdCosf(ang1) + 1.0f)*10.0f);
	scroll = (backFrame % 5000) * KABE_SCROLL;

	vif1Begin();
	/* real: 0x2287E0 - PRIM = 28 and CLAMP_1 = 0 in one REGLIST packet.
	 * PRIM 28 is TRISTRIP with IIP and TME set and ABE CLEAR: the wall is
	 * drawn OPAQUE even though 0x22AB90's `additive' argument pushed
	 * ALPHA_1 = 0x48, which the PRIM bit then makes dead state.  CLAMP_1
	 * = 0 is REPEAT/REPEAT, which the ST range needs. */
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 1, 0, 0, 0, 0, 0, 0));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0));

	for(i = 0; i < NRING; i++) {
		scale = 1.0f + mdSinf(backFrame*100 + i*5120)*KABE_WOBBLE;
		/* real: sceVu0ScaleVectorXYZ (0x267050) - w is left at 1, and z
		 * is overwritten right after, so the wave only breathes x/y */
		p0[0] = e0[0]*scale; p0[1] = e0[1]*scale;
		p1[0] = e1[0]*scale; p1[1] = e1[1]*scale;
		p0[2] = p1[2] = i*RINGSTEP + RINGZ0;
		p0[3] = p1[3] = 1.0f;

		backApply(p0, cam, p0);
		backApply(p1, cam, p1);
		if(p0[2] < KABE_NEARZ || p1[2] < KABE_NEARZ)
			continue;

		q0 = backProject(p0, vs);
		q1 = backProject(p1, vs);
		if(fabsf(p0[0] - 2000.0f) > 1000.0f) continue;
		if(fabsf(p1[0] - 2000.0f) > 1000.0f) continue;
		if(fabsf(p0[1] - 2000.0f) > 1000.0f) continue;
		if(fabsf(p1[1] - 2000.0f) > 1000.0f) continue;

		tc = i*(1.0f/32.0f) + scroll;
		k = NRING - 1 - i;
		pp = (k*k*k) >> 10;
		rr = (pp*230) >> 5;
		gg = (pp*260) >> 5;
		KabeVertex(p0, q0, s0, tc, rr+bgFade0+tint0, gg+bgFade1+tint0, gg+bgFade2+tint0);
		KabeVertex(p1, q1, s1, tc, rr+bgFade0+tint1, gg+bgFade1+tint1, gg+bgFade2+tint1);
	}

	/* real: 0x228E78 - one more pair, both vertices on the tunnel axis at
	 * z = 38750, coloured with the fade only (black in the idle menu), so
	 * the strip closes to a point and the far end goes dark.  No cull. */
	p0[0] = p0[1] = 0.0f; p0[2] = KABE_CAPZ; p0[3] = 1.0f;
	p1[0] = p1[1] = 0.0f; p1[2] = KABE_CAPZ; p1[3] = 1.0f;
	backApply(p0, cam, p0);
	backApply(p1, cam, p1);
	q0 = backProject(p0, vs);
	q1 = backProject(p1, vs);
	tc = 1.0f + scroll;
	KabeVertex(p0, q0, s0, tc, bgFade0, bgFade1, bgFade2);
	KabeVertex(p1, q1, s1, tc, bgFade0, bgFade1, bgFade2);

	vif1End();
}

/* real: 0x22AB90(1, 1, 2) -> 0x22AA88 - bind TEXC slot 1.
 *
 * Two details 0x22AA88 gets to by overriding what sceGsSetDefTexEnv
 * (0x262308) just wrote, and that docs/menu-draw.md 5.5 records the
 * other way round:
 *   CLAMP_1 is forced back to 0 (REPEAT/REPEAT); the library's default
 *     for that struct is 5 (CLAMP/CLAMP).
 *   TEST_1 = (ztst << 17) | 0x30000, i.e. the low ZTST bit is OR'd in,
 *     so the `2' (GEQUAL) every caller passes actually arrives as 3
 *     (GREATER).  0x22A0C0's version of the same line uses | 0x10000 and
 *     does not have the bug.
 * TEX1 is overridden to 0x61 = LCM with MMAG/MMIN LINEAR, and TCC is
 * hard-wired to 1 inside 0x262308. */
static void
BindKabe(void)
{
	vif1Begin();
	pktSetAD(SCE_GS_TEXFLUSH, 0);
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(kabeTexture.gstex.tbp[0], KABEW/64,
		SCE_GS_PSMCT32, 7, 7, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0));
	pktSetAD(SCE_GS_TEST_1, SCE_GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_GREATER));
	/* real: 0x2622A8 - ALPHA_1 = 0x48 (Cs*As + Cd), PABE 0, TEXA
	 * {TA0 0x7F, AEM 1, TA1 0x81}, FBA_1 0.  ALPHA_1 is dead here (PRIM
	 * clears ABE) but the composite's blits depend on TEXA. */
	pktSetAlphaBlend(1, 5, 128);
	pktSetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(0x7F, 1, 0x81));
	vif1End();
}

/* real: 0x2292D0 */
static void
DrawKabe(sceVu0FMATRIX cam, sceVu0FMATRIX vs)
{
	int a;

	BindKabe();
	vif1SetZWrite(1);
	for(a = 0; a < 65536; a += 65536/NSEG)
		DrawKabeRibbon(cam, vs, a, a + 65536/NSEG);
}

/* ======================= the composite (0x21D0A0) =======================
 *
 * The ROM keeps FIVE screen-sized buffers in GS memory: the two colour
 * buffers, the Z buffer, and two work buffers at word offsets 3*w*h and
 * 4*w*h (0x22A4C8 turns those into FRAME.FBP as w*h*3>>11 and w*h>>9;
 * 0x22A290 into the matching TBP as w*h*3>>6 and w*h>>4).  osdbits only
 * reserves three, so the port borrows opening.c's extraBuf1/extraBuf2,
 * which are allocated (and, in menu mode, otherwise unused) by
 * InitRender's gsAllocExtraBuffers.
 *
 * 0x22A198(sel) points TEX0 at "the buffer being drawn this frame":
 * sel != 0 -> TBP 0, sel == 0 -> TBP w*h/64, the complement of the
 * drawenv 0x22A3B8(dbuff, sel) selects, because sceGsSetDefDBuff gives
 * draw0 the SECOND colour buffer.  (opening.c's sub_2144c0 documents the
 * identical formula on the opening's side.)  The port reads the FBP out
 * of the live draw environment instead, which is the same thing without
 * the parity reasoning. */

static u32 backScreenTbp;	/* TBP0 blocks of this frame's draw buffer */
static int backFrameParity;
static int backFrameField;

/* real: the extents patch at the head of 0x21D0A0 and 0x22C190's, both
 * of which write x1/y1 = w<<4, h<<4 and u1/v1 = (w<<4)+8, (h<<4)+8 into
 * their static 0x40-byte sprite record.  The +8 pairs with the u0/v0 = 8
 * in the record's initialiser: half a texel, so a bilinear 1:1 blit
 * lands on texel centres. */
static void
FullScreenBlit(u32 tbp, u32 psm, Color *col)
{
	Rect full;

	full.x = full.y = 0;
	full.w = screenW;
	full.h = screenH;

	vif1Begin();
	/* real: 0x22A198/0x22A290 pass flush = 0 to sceGsSetDefTexEnv, which
	 * writes the NOP register 0x7F where the TEXFLUSH would go - the ROM
	 * deliberately samples a just-rendered page without flushing.  Only
	 * the TEXC bind (0x22AB90) asks for a flush. */
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	/* real: TW/TH are the literals 10 and 8 (1024x256), not the screen
	 * size - only the wrap outside the sampled rect depends on them */
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(tbp, screenW/64, psm,
		10, 8, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(1, 1, 0, 0, 0, 0));
	vif1End();
	/* the record's ABE is 0 in every one of these: the blits and the
	 * composite are opaque overwrites, and the ALPHA_1 0x22A0C0 pushes
	 * around them never takes effect */
	vif1SetTexRect(&full, &full, col, 0, 0);
}

/* real: 0x22A4C8 and 0x22A3B8 push XYOFFSET_1 next to FRAME_1 off a
 * `field' argument, and every work-buffer site passes 0 for it -
 * 0x22C3C0's 0x22A4C8(1,0,0) and 0x22A3B8(dbuff,evenOddFrame,0,0),
 * 0x21D0A0's 0x22A4C8(0,0,0)/(1,0,0) - where only the scene's own
 * 0x22A3B8(dbuff, evenOddFrame, 0x27B4A0, field) passes the real field.
 * A retail dump of a half-offset frame shows exactly that: orbs and text
 * at OFY 1936.5, all ten blur blits and the composite's at 1936.0.
 *
 * The half pixel is a per-field correction for the DISPLAY, so it must
 * not ride along on a buffer-to-buffer resample.  Left in, the shrink
 * lands 0.5 px off and the stretch magnifies that by 223/149.25 and adds
 * another 0.5: 1.247 px per pass, 6.24 px over the five, on one field
 * only - a second copy of the whole 3D layer alternating at 60 Hz.
 * opening.c's DrawToExtraBuf2 brackets its own capture the same way. */
static void
BackHalfOffset(int on)
{
	vif1SetXYOffset(backFrameField, on);
}

/* real: *(0x27B448), the module's own copy of the field, which every
 * stage reads instead of the live register.  menuconfig.c's glass emit
 * (0x22C4E0's `- field*0.5' on the V) needs the same value for every
 * object of one frame, so hand it the snapshot. */
int
MenuBackField(void)
{
	return backFrameField;
}

/* real: the FRAME_1 half of 0x22A4C8 / 0x22A3B8.  0x22A4C8's third
 * argument is a clear-colour RECORD, not a flag: 0x21D0A0 passes NULL
 * for both work buffers, which drops the drawenv GIFtag's NLOOP from 14
 * to 8 and skips the clear entirely.  (docs/menu-draw.md 4.1/4.3 read
 * that patched field as FRAME.FBP; it is the GIFtag's NLOOP.) */
static void
SetTarget(u32 tbp)
{
	vif1SetFramebuffer(tbp/32, SCE_GS_PSMCT32, screenW, screenH, 0);
}

static void
SetScreenTarget(void)
{
	sceGsDrawEnv1 *env = backFrameParity == 0 ? &db.draw0 : &db.draw1;

	vif1SetFramebuffer(env->frame1.FBP, env->frame1.PSM, screenW, screenH, 0);
}

/* real: the clear half of 0x22A4C8, i.e. the six extra A+D pairs the
 * drawenv GIFtag's NLOOP 14 turns on.  Its third argument is a RECORD -
 * four ints that 0x22A64C..0x22A668 pack into the drawenv's RGBAQ (with Q
 * forced to 1.0) - and the menu has two of them, which is why the colour
 * is a parameter here:
 *
 *   0x27F180 = {0,0,0,**0**}     0x226D00's clear between the two cube
 *                                walks - ALPHA ZERO, because work buffer 4
 *                                becomes that stage's ALPHA MASK and the
 *                                tail's 0x22C088 carries the alpha into
 *                                work buffer 3 for the composite;
 *   0x27EBF0 = {0,0,0,**0x80**}  0x2267E8's clear before each of the rod
 *                                bloom's two walks - alpha 0x80, because
 *                                there the alpha is what the additive
 *                                composite MODULATES BY, and 0 would make
 *                                the whole stage invisible.
 *
 * osdbits' own vif1SetFramebuffer(clear = 1) cannot be used: it emits the
 * clear sprite in raw GS coordinates, ignoring XYOFFSET.
 *
 * THE DEPTH TEST IS PART OF THE CLEAR.  sceGsSetDefDrawEnv2 brackets the
 * clear sprite with its own TEST_1 pushes, and a retail dump shows all
 * three (retail614.log D1759 and its neighbours):
 *
 *     TEST ZTE ztst=2      the drawenv's own
 *     TEST ZTE ztst=1      the clear's - ALWAYS
 *     PRIM SPRITE ... z=(0,0) rgba=00000080
 *     TEST ZTE ztst=2      restored
 *
 * The port left the live test in place, and the clear sprite is at z = 0,
 * so any caller that reached here with ZTST GEQUAL and a non-zero Z buffer
 * silently got NO CLEAR at all.  0x226D00's call site happens to set ZTST
 * ALWAYS just before, so the cube mask clear was fine; 0x2267E8's is not
 * so lucky - it follows MeshDrawRod's pass 5, which leaves GEQUAL - and
 * with the rod bloom's clear skipped, work buffer 4 still held
 * MenuBackdrop's copy of the whole screen and the stage's additive
 * composite laid ~23 % of a second copy of the frame over the frame.  That
 * was measured, not guessed: a build with this stage in but the ZMSK
 * alignment out (so the pre-scene blits no longer reset Z to 0) lifted
 * every background block of the config screen by two ramp steps. */
static void
WorkClear(const int *col)
{
	int ox = (2048-screenW/2)<<4;
	int oy = (2048-screenH/2)<<4;

	vif1SetZTest(0);		/* real: the clear's own ZTST ALWAYS */
	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 0, 0, 0, 0, 0, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2], col[3],
		0x3f800000));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(ox, oy, 0));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(ox + (screenW<<4), oy + (screenH<<4), 0));
	vif1End();
	vif1SetZTest(1);		/* real: back to the drawenv's GEQUAL */
}

/* real: 0x2299C0 over the record at 0x27F820, {0x80,0x80,0x80,0x80},
 * x0/y0 = 0, u0/v0 = 8, ABE = 0, TME = 1.  The extents go straight in as
 * 1/16 units, which is what the zoom blur needs: its destination is
 * 319.25 x 149.25, not an integer rectangle, so the Rect/pktSetTexRect
 * path (whole pixels, and a 1/16 shrink on the far corner) cannot express
 * it.  0x2298A8 adds (2048 - w/2, 2048 - h/2) << 4 to both corners, the
 * same convention pktSetFlatRect uses.
 *
 * `alpha' is the record's fourth word.  Every sprite record in this chain
 * carries 0x80 except 0x2267E8's (0x27EBB0), whose alpha 0x226768 PATCHES
 * from its own argument (`sw a0,12(s0)') - see MenuBackFlushOver. */
static void
BlurBlit(u32 tbp, u32 psm, int abe, int alpha, int x1, int y1, int u1, int v1)
{
	int ox = (2048-screenW/2)<<4;
	int oy = (2048-screenH/2)<<4;

	vif1Begin();
	/* real: sceGsSetDefTexEnv's `filter' argument is 1 in both
	 * 0x22A198 and 0x22A290, so MMAG = MMIN = LINEAR (TEX1 = 0x61).
	 * The bilinear taps are the whole effect - with NEAREST this loop
	 * would be a lossy resample and nothing else. */
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(tbp, screenW/64, psm,
		10, 8, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(1, 1, 0, 0, 0, 0));
	/* real: the record's +0x34, which 0x2297E8 shifts into PRIM's ABE bit
	 * (`(rec+0x34)<<6 | (rec+0x38)<<4 | 6 | 256').  The zoom blur's record
	 * 0x27F820 and 0x22C2A0's 0x27F7E0 carry 0, the two work-buffer merges
	 * 0x27F6E0/0x27F720 carry 1, and 0x22C190 PATCHES its record 0x27F760's
	 * from the argument - 0 for 0x21D0A0's two screen copies, 1 for the
	 * cube stage's composite back over the screen. */
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, abe, 0, 1, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0x80, 0x80, 0x80, alpha, 0x3f800000));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(8, 8));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(ox, oy, 0));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(u1, v1));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(ox + x1, oy + y1, 0));
	vif1End();
}

/* real: 0x22A290(n) - point TEX0 at work buffer 3 (n = 0) or work buffer
 * 4 (n = 1), the two copies of the frame MenuBackdrop() takes before the
 * object list runs.  The glass passes sample one of them by screen
 * position, so the rods and cubes refract whatever is in it.  TW/TH are
 * the ROM's literal 10 and 8, and CLAMP/CLAMP is what sceGsSetDefTexEnv
 * writes; the glass primitives override CLAMP_1 to REPEAT per primitive,
 * which is what makes 0x22C4E0's +1024/+256 UV biases cancel. */
void
MenuBackBindWork(int buf)
{
	vif1Begin();
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(buf ? extraBuf2 : extraBuf1,
		screenW/64, SCE_GS_PSMCT32,
		10, 8, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0));
	vif1End();
}

void
MenuBackBindScreenCopy(void)
{
	MenuBackBindWork(0);
}

/* real: 0x22A198(evenOddFrame) - TEX0 = the buffer BEING DRAWN (the
 * complement of the FBP `sceGsSetDefDBuff' gave draw0, see 2.1 of
 * docs/menu-backdrop.md), as PSMCT24 - the `psm' 0x22A198 hands
 * sceGsSetDefTexEnv is 1 where 0x22A290's is 0.  0x22D2E8's first pass
 * binds THIS, not a work buffer: the cubes' outer glass layer refracts
 * the live, composited, zoom-blurred screen. */
void
MenuBackBindScreen(void)
{
	vif1Begin();
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(backScreenTbp, screenW/64,
		SCE_GS_PSMCT24, 10, 8, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0));
	vif1End();
}

/* real: 0x22A4C8(sel, rec, field) and 0x22A3B8(dbuff, evenOddFrame, rec,
 * field).  Both push FRAME_1 and XYOFFSET_1 together, and the `field'
 * argument is PER CALL SITE - which is the whole half-pixel story for
 * this stage.  In 0x22D2E8/0x22D798 the mesh passes pass 1 (the real
 * field) and only the buffer-to-buffer sprites pass 0:
 *
 *   0x22BF58(1,0,1)  pass 1   FRAME wb4   field      | 22d36c: li a2,1
 *   0x22BFD0(0,1,1)  pass 4/7 FRAME wb3   field      | 22d51c/22d5dc: li a2,1
 *   0x22BFD0(1,0,0)  pass 6   FRAME wb4   NO field   | 22d5b8: move a2,zero
 *   0x22A4C8(1,rec,*(0x27B448))  the wb4 clear, field
 *   0x22BFD0(1,0,1)  0x22D798 FRAME wb4   field      | 22d810: li a2,1
 *   0x22BFD0(0,1,0)  the tail FRAME wb3   NO field   | 226efc..226f04
 *   0x22C020(0,0,0)  the tail FRAME screen NO field  | 226f34..226f40
 *   0x22A4C8(1,0x27EBF0,*(0x27B448))  0x2267E8's two wb4 clears, field
 *
 * i.e. exactly the rule 37efd18 established - a buffer-to-buffer resample
 * must not carry the display's per-field half pixel, and a mesh drawn for
 * the display must.  (This corrects the note eba5595 left here: 0x22D2E8's
 * FRAME push is 0x22BF58(1,0,**1**), not (1,0,0), so the cube MESHES do
 * carry the half pixel; only the three blits around them do not.)
 *
 * `clear' is 0x22A4C8's own third argument: nil for no clear (which in the
 * ROM drops the drawenv GIFtag's NLOOP from 14 to 8), otherwise the RGBA
 * record to clear to - see WorkClear. */
void
MenuBackWorkTarget(int buf, const int *clear, int field)
{
	SetTarget(buf ? extraBuf2 : extraBuf1);
	vif1SetXYOffset(backFrameField, field);
	if(clear)
		WorkClear(clear);
}

void
MenuBackScreenTarget(int field)
{
	SetScreenTarget();
	vif1SetXYOffset(backFrameField, field);
}

/* real: 0x22C088 - record 0x27F6E0 {0x80,0x80,0x80,0x80}, x0/y0 = 0,
 * u0/v0 = 8, ABE = 1, TME = 1, extents patched to (w<<4, h<<4) and
 * ((w<<4)+8, (h<<4)+8), under 0x22A0C0(0, 1) = ALPHA_1 0x48 = Cs*As + Cd.
 *
 * Its one call site is 0x226D00's tail, right after 0x22BFD0(0,1,0) aims
 * FRAME at work buffer 3 and TEX0 at work buffer 4, so it is "add work
 * buffer 4 over work buffer 3".  Two things happen at once, and the
 * second is the one that matters:
 *
 *  - the COLOUR add.  Work buffer 4 was cleared to black and the second
 *    cube walk only ever painted black into it (0x22CA68), so this adds
 *    nothing;
 *  - the ALPHA copy.  Alpha blending only touches RGB; the GS still
 *    writes As - here work buffer 4's own stored alpha - into the
 *    destination.  So this one blit stamps the cube silhouette mask into
 *    work buffer 3's alpha channel, which is exactly what the composite
 *    below then blends by. */
void
MenuBackWorkAdd(void)
{
	vif1SetZTest(0);			/* real: 0x22A0C0(0,1)'s ZTST 1 */
	vif1SetAlphaBlend(1, 5, 128);		/* real: ALPHA_1 0x48 */
	BlurBlit(extraBuf2, SCE_GS_PSMCT32, 1, 0x80,
		screenW<<4, screenH<<4, (screenW<<4)+8, (screenH<<4)+8);
}

/* real: 0x22C100 - record 0x27F720, the same {0x80,...} additive sprite,
 * but its extents are patched to x1 = (w/2)<<4, u1 = ((w/2)<<4)+8: a 1:1
 * copy of the LEFT HALF only.  0x22D2E8 pass 6 runs it with FRAME = work
 * buffer 4, TEX0 = work buffer 3, so pass 7 (which samples work buffer 4)
 * sees the silhouette passes 4 and 5 just wrote.  Half width because the
 * five cubes live at x -242..-79 of centre, i.e. entirely in the left
 * half of the frame - the ROM simply does not pay for the right half. */
void
MenuBackWorkHalfAdd(void)
{
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 5, 128);		/* real: 0x22A0C0(0,1), 0x48 */
	BlurBlit(extraBuf1, SCE_GS_PSMCT32, 1, 0x80,
		(screenW/2)<<4, screenH<<4, ((screenW/2)<<4)+8, (screenH<<4)+8);
}

/* real: 0x22C190(abe) - record 0x27F760, {0x80,0x80,0x80,0x80}, u0/v0 =
 * 8, TME = 1 and ABE PATCHED FROM THE ARGUMENT (`sw s1,52(s0)' writes
 * rec+0x34, which 0x2297E8 shifts into PRIM bit 6), under 0x22A0C0(1, 1)
 * = ALPHA_1 0x44 = (Cs - Cd)*As + Cd.
 *
 * 0x21D0A0 calls it TWICE with abe = 0 - the two opaque screen -> work
 * buffer copies - and 0x226D00's tail calls it with abe = **1** (226f48:
 * `li a0,1' in the delay slot chain before the tail jump).  With ABE set
 * and As coming from the texture's own alpha (TFX MODULATE, Af = 0x80),
 * that last one is a MASKED composite: work buffer 3 replaces the screen
 * where its alpha is 0x80 and leaves it alone where the alpha is 0.  That
 * is the only thing standing between the cube stage and wiping the orbs,
 * the rods and the tint off the frame, and it is why the whole chain
 * exists rather than the cubes just being drawn on the screen. */
void
MenuBackWorkOver(int abe)
{
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);		/* real: 0x22A0C0(1,1), 0x44 */
	BlurBlit(extraBuf1, SCE_GS_PSMCT32, abe, 0x80,
		screenW<<4, screenH<<4, (screenW<<4)+8, (screenH<<4)+8);
}

/* real: 0x22C020(1, 0, 0) then 0x226768(a) - the tail of each of the rod
 * bloom's two walks (menu.c's SceneFlush).
 *
 * 0x226768 patches the static sprite record at 0x27EBB0 - whose fixed part
 * is {0x80,0x80,0x80, A}, x0/y0 = 0, u0/v0 = 8, +0x34 = 1 (ABE) and +0x38 =
 * 1 (TME) - with the screen extents ((w<<4, h<<4) and ((w<<4)+8,
 * (h<<4)+8), the same half-texel pairing the rest of the chain uses) and,
 * from its argument, the record's fourth word: the ALPHA.  Then it pushes
 * 0x22A0C0(0, 1) = ALPHA_1 0x48 (Cs*As + Cd) with ZTST ALWAYS and draws it
 * through 0x2299C0.
 *
 * So this is "add work buffer 4 over the screen at alpha `alpha'".  As is
 * the texture's own alpha modulated by the sprite's, which is why the wb4
 * clear that starts each walk must be {0,0,0,0x80} and not the cube
 * stage's {0,0,0,0}: at 0x80 the whole buffer contributes, and the passes
 * that write their own alpha into it (0x22CD78's PRIM 276 has ABE clear)
 * scale their own contribution.
 *
 * In a retail dump: `SPRITE+TME+ABE+FST, FB = 0, uv (0.5,0.5)-(640.5,224.5),
 * rgba = 8080801e' - 0x1E = the 30 that 0x2267E8 passes, twice a frame. */
void
MenuBackFlushOver(int alpha)
{
	MenuBackScreenTarget(0);		/* real: 0x22C020(1, 0, 0) */
	vif1SetZTest(0);			/* real: 0x22A0C0(0, 1) ZTST 1 */
	vif1SetAlphaBlend(1, 5, 128);		/* real: ALPHA_1 0x48 */
	BlurBlit(extraBuf2, SCE_GS_PSMCT32, 1, alpha,
		screenW<<4, screenH<<4, (screenW<<4)+8, (screenH<<4)+8);
}

/* real: 0x22C2A0 - the work-buffer twin of 0x22C3C0's zoom blur, and the
 * caller docs/menu-backdrop.md 6 could not find for 0x22C228's family.
 * Instruction for instruction the same loop with the same 5108/2388
 * extents and the same -32/-16 per pass; only the two FRAME/TEX pushes
 * differ - 0x22BFD0(1,0,0) and 0x22BFD0(0,1,0) instead of 0x22BF58 and
 * 0x22C020, i.e. it ping-pongs work buffer 3 against work buffer 4
 * instead of the screen against work buffer 4, and BOTH pass field = 0.
 * Its record 0x27F7E0 has ABE = 0, so both blits are opaque resamples.
 *
 * 0x226D00 runs it with `phase < 5 ? 5 - phase : 0' off the transition
 * ramp *(gp-28844), which idles at 10 - so in a settled screen n is 0 and
 * the loop does not run at all.  (0x2283D0's always-on blur keys on the
 * same ramp the other way round, `phase - 5' = 5.) */
void
MenuBackWorkBlur(int n)
{
	int x, y;

	if(n <= 0)
		return;

	/* ZMSK 0, as everywhere else in the menu (416/416 retail ZBUF writes
	 * are `zbp=4480 psm=0' with ZMSK clear).  Harmless either way here -
	 * ZTST is ALWAYS on both blits - but a retail dump has these passes
	 * stamping the resampled sprite's z = 0 over the buffer, so match it. */
	vif1SetZWrite(1);
	vif1SetZTest(0);
	x = 5108;
	y = 2388;
	for(; n > 0; n--) {
		/* real: 0x22BFD0(1,0,0) - FRAME wb4, TEX wb3, no half pixel */
		MenuBackWorkTarget(1, nil, 0);
		vif1SetAlphaBlend(1, 4, 128);	/* real: 0x22A0C0(1,1) */
		BlurBlit(extraBuf1, SCE_GS_PSMCT32, 0, 0x80,
			x, y, (screenW<<4)+8, ((screenH-1)<<4)+8);

		/* real: 0x22BFD0(0,1,0) - FRAME wb3, TEX wb4 */
		MenuBackWorkTarget(0, nil, 0);
		vif1SetAlphaBlend(1, 4, 128);
		BlurBlit(extraBuf2, SCE_GS_PSMCT32, 0, 0x80,
			screenW<<4, (screenH-1)<<4, x+8, y+8);

		x -= 32;
		y -= 16;
	}
}

/* real: 0x22C3C0 - the zoom blur, N passes of "shrink the screen into a
 * work buffer, stretch it back over the screen".  Pass k shrinks to
 * (5108 - 32k)/16 x (2388 - 16k)/16 pixels = 319.25 x 149.25 down to
 * 311.25 x 145.25, from a 640 x 223 source, and reads back a half-texel
 * wider rect.  The vertical ratio is not the horizontal one, so it is a
 * squash as much as a blur, and the per-pass shrink decorrelates the
 * sampling grids so the softening accumulates smoothly.
 *
 * Both blits are opaque (the record's ABE is 0); the ALPHA_1 = 0x44 that
 * 0x22A0C0(1,1) pushes between them is dead state.  What does the work is
 * TEX1's LINEAR/LINEAR - see BlurBlit. */
static void
ZoomBlur(int n)
{
	int x, y;

	if(n <= 0)
		return;

	vif1SetZWrite(1);			/* ZMSK 0 - see MenuBackWorkBlur */
	vif1SetZTest(0);			/* real: 0x22A0C0(1,1) - ZTST ALWAYS */
	BackHalfOffset(0);			/* real: 0x22BF58/0x22C020 pass field = 0 */
	vif1SetAlphaBlend(1, 4, 128);		/* real: ALPHA_1 = 0x44, unused (ABE 0) */
	x = 5108;
	y = 2388;
	for(; n > 0; n--) {
		/* real: 0x22BF58(1,0,0) - TEX = the screen (PSMCT24, the psm
		 * 0x22A198 passes), FRAME = work buffer 4 */
		SetTarget(extraBuf2);
		BlurBlit(backScreenTbp, SCE_GS_PSMCT24, 0, 0x80,
			x, y, (screenW<<4)+8, ((screenH-1)<<4)+8);

		/* real: 0x22C020(1,0,0) - TEX = work buffer 4 (PSMCT32, the psm
		 * 0x22A290 passes), FRAME = the screen */
		SetScreenTarget();
		BlurBlit(extraBuf2, SCE_GS_PSMCT32, 0, 0x80,
			screenW<<4, (screenH-1)<<4, x+8, y+8);

		x -= 32;
		y -= 16;
	}
	/* real: the next 0x22A3B8 (the 2D layer's) puts the field back */
	BackHalfOffset(1);
	/* real: 0x22A0C0(1, 3) at the tail leaves ALPHA_1 = 0x44 and ZTST
	 * GREATER; the port already holds 0x44 and the next stage
	 * (menutext.c) sets both itself, so there is nothing to re-push. */
}

/* real: the colour record at 0x27B4B0, {0x37, 0x28, 0x3C, 0x80}.  With
 * TEX0's TFX = MODULATE that is x0.43 / x0.31 / x0.47 - the tint that
 * turns the near-white wall into the menu's deep blue. */
static Color compositeColor = { 0x37, 0x28, 0x3C, 0x80 };
static Color whiteColor = { 0x80, 0x80, 0x80, 0x80 };

/* The screen-transition ramp *(gp-28844), which 0x2285C0 rewrites every
 * frame as `count >= duration ? 0 : 10 - count*10/duration' off the timer
 * at 0x27EC40 (duration *(gp-30396) = 10 on NTSC).  Nothing in the idle
 * menu ever opens that timer, so count stays 0 and the ramp sits at its
 * maximum, **10** - which is what the two blur call sites key on.
 * argv[11] overrides it so a transition can be faked. */
static int backPhase = 10;

/* NOT original: SwapBuffers flips evenOddFrame on the swap thread, so
 * reading it twice in one frame can straddle a flip and point the two
 * blur stages at different buffers.  Snapshot it once, at the top of the
 * frame, exactly as opening.c's stableEvenOddFrame does. */
void
MenuBackFrameStart(void)
{
	sceGsDrawEnv1 *env;

	backFrameParity = evenOddFrame;
	backFrameField = evenOddField;
	env = backFrameParity == 0 ? &db.draw0 : &db.draw1;
	backScreenTbp = env->frame1.FBP * 32;
}

/* real: 0x2283D0, the head of 0x2283F0 - stage 5 of 0x21CF20, which runs
 * AFTER the 3D scene (0x2268F0) and the fade curtain (0x22B020) and
 * BEFORE the 2D layer (0x2283A0 and friends):
 *
 *     phase = *(gp-28844);
 *     if(phase >= 5) 0x22C3C0(phase - 5);
 *
 * In the idle menu phase is 10, so this is **0x22C3C0(5)** on every
 * single frame: the whole 3D scene - orbs, trails, halos - is run through
 * five bilinear shrink/stretch round trips before the crisp 2D text goes
 * on top.  That is what makes the retail orbs so much softer than a
 * straight port of 0x22EFF0, and it is a screen-wide effect, not
 * something in the orb draws themselves.
 *
 * 0x21D0A0's own `phase < 6 ? phase : 10 - phase' call is the complement:
 * 0 while idle, rising as this one falls, so the blur migrates from
 * after the scene to before it as a transition runs.  docs/menu-backdrop
 * 6 saw only 0x21D0A0's site and concluded the blur was transition-only;
 * that is wrong - see the notes. */
void
MenuZoomBlur(void)
{
	if(backPhase >= 5)
		ZoomBlur(backPhase - 5);
}

/* real: *(gp-28844) itself.  0x226D00's tail reads it raw for
 * 0x22C2A0's pass count (`phase < 5 ? 5 - phase : 0'), so hand it over
 * rather than duplicating argv[10]'s override in menuconfig.c. */
int
MenuBackPhase(void)
{
	return backPhase;
}

/* real: 0x21CE58's share of the backdrop's setup - 0x229698 + 0x22A9B8(1)
 * (TEXC slot 1's descriptor and upload), 0x2287B0 (initBgTimer) and the
 * frame-counter reset. */
void
InitMenuBackdrop(void)
{
	DecodeKabe();
	InitTexture(&kabeTexture);

	memset(&bgTimer, 0, sizeof(bgTimer));
	bgTimer.duration = IsPAL() ? 40*50/60 : 40;	/* real: gp-30380 */
	bgFade0 = bgFade1 = bgFade2 = 0;
	backFrame = 0;

	backPhase = clamp(OsdArgInt(10, 10), 0, 10);
	printf("osdsys: backdrop tbp %u, zoom-blur phase %d\n",
		kabeTexture.gstex.tbp[0], backPhase);
}

/* real: 0x21D0A0 - stage 2 of the frame body.
 *
 *   0x22A3B8(dbuff, evenOddFrame, 0x27B4A0, field)  drawenv + CLEAR
 *   0x229358(m0, m1)                                the wall
 *   0x22C3C0(phase)                                 the zoom blur
 *   0x22A4C8(0,0,0); 0x22A198(evenOddFrame); 0x22C190(0)   screen -> buf3
 *   0x22A4C8(1,0,0);                         0x22C190(0)   screen -> buf4
 *   0x22A3B8(dbuff, evenOddFrame, 0, 0)             back to the screen
 *   0x22A290(0); 0x22A0C0(0,1); 0x2299C0(0x27B4B0)  buf3 -> screen, tinted
 *
 * osdbits' StartFrame/SwapBuffers already do the clear (sceGsSetDefDBuff
 * with SCE_GS_CLEAR), so the first drawenv push has no counterpart here.
 *
 * `fadeMode' is menu.c's copy of *(gp-28828): 0x229358 skips the wall
 * entirely whenever the module-wide fade is running, so the backdrop
 * only appears once the 128-frame fade-up from black has finished.  The
 * composite is NOT gated - it runs every frame regardless, which during
 * the fade just re-tints the (empty) screen. */
void
MenuBackdrop(sceVu0FMATRIX cam, sceVu0FMATRIX vs, int fadeMode)
{
	/* the buffer parity was snapshotted by MenuBackFrameStart() at the
	 * top of the frame - both this stage and MenuZoomBlur() need the same
	 * one, and evenOddFrame can flip between them */

	/* real: 0x229358's head - step the timer, and refresh the three fade
	 * words only while the timer is not closed */
	BackTimerStep(&bgTimer);
	if(bgTimer.state != 0) {
		bgFade0 = BackTimerInterp(&bgTimer, 40);
		bgFade1 = BackTimerInterp(&bgTimer, 40);
		bgFade2 = BackTimerInterp(&bgTimer, 40);
	}

	if(fadeMode == 0)
		DrawKabe(cam, vs);

	ZoomBlur(backPhase < 6 ? backPhase : 10 - backPhase);

	/* the two copies.  Both read the screen; the second buffer only
	 * matters to the zoom blur and to 0x2267E8's carousel bloom. */
	vif1SetZWrite(1);	/* ZMSK 0 - see MenuBackWorkBlur */
	vif1SetZTest(0);
	BackHalfOffset(0);	/* real: 0x22A4C8(...,field=0), as the blur */
	SetTarget(extraBuf1);
	FullScreenBlit(backScreenTbp, SCE_GS_PSMCT24, &whiteColor);
	SetTarget(extraBuf2);
	FullScreenBlit(backScreenTbp, SCE_GS_PSMCT24, &whiteColor);

	/* and back over the screen, tinted.  real: 0x22A290(0) reads the work
	 * buffer as PSMCT32 where 0x22A198 read the screen as PSMCT24 - with
	 * ABE clear in every one of these records the difference only shows in
	 * the alpha each blit stores, which nothing downstream reads. */
	SetScreenTarget();
	FullScreenBlit(extraBuf1, SCE_GS_PSMCT32, &compositeColor);
	BackHalfOffset(1);

	backFrame++;	/* real: 0x2287D0, a separate stage of 0x21CF20 */
}
