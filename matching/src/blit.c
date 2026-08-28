/* the framebuffer blits and text overlay (0x214050..0x214f20) -
 * compile with ee-gcc 2.9-ee-991111 -O2, -I<freesce>/ee/include for the
 * real SCE_GS_SET_* macros (eestruct.h/libgraph.h) - see README. */

#include <eetypes.h>
#include <eestruct.h>
#include <libgraph.h>

typedef unsigned int u32;
typedef u_long u64;

typedef struct Rect { int x, y, w, h; } Rect;
typedef struct Color { u32 r, g, b, a; } Color;

/* screenW/screenH/evenOddFrame/evenOddField are lui-addressed absolute
 * memory locations in the real binary (0x1f0c40/44/50/54), not
 * relocatable extern symbols - a page-aligned struct pointer at
 * 0x1f0000 with the fields at their real byte offsets reproduces the
 * real "lui BASE,0x1f; lw x,0xc50(BASE); lw y,0xc54(BASE)" shared-page
 * addressing (a plain "extern int screenW,screenH;" pair would
 * materialise a pointer via an extra addiu instead, desyncing every
 * later instruction's address). */
struct DispRegs {
	char pad[0xc40];
	int evenOddFrame, evenOddField, pad0, pad1, screenW, screenH;
};
#define DISP ((struct DispRegs *)0x1f0000)
#define evenOddFrame DISP->evenOddFrame
#define evenOddField DISP->evenOddField
#define screenW      DISP->screenW
#define screenH      DISP->screenH

/* small gp-resident state (offsets are masked by check.py, so exact
 * addresses don't matter, only that these are plain scalar externs). */
extern int extraBufFrameFlag;	/* frame-parity flag read directly by
				 * DrawToExtraBuf2/DrawExtraBuf2, distinct
				 * from evenOddFrame */
extern int openingType;
extern int frameCount;
extern int openingEndFlag;
extern int illegalTextState;
extern int illegalTextAlpha;

extern float openingPosition[4];

/* the save/working buffers, allocated at runtime by gsAllocBuffer() but
 * already block-address units by the time these functions read them (no
 * /32 shift at the use sites in the real code) - hence the wide "ld"
 * load, matching a u64-sized declaration. */
extern u32 extraBuf1;	/* DrawToExtraBuf2 (write) / DrawExtraBuf2 (read) -
			 * real loads this via "ld" (64-bit), residual */
extern u32 extraBuf2;	/* sub_2144c0's scratch working buffer */

extern Color grayTemplate;	/* 0x2a42d8 */
extern Color barColTemplate;	/* 0x2a42e8 */
extern Rect  spriteRectTemplate;	/* 0x2a42f8: {0,0,?,?} */
extern Color whiteSpriteTemplate;	/* 0x2a4308: {255,255,255,0} */
extern Rect  sceTextRect[2][2];	/* 0x2a4558.. */
extern float screenAX, screenAY;

typedef struct Texture { int dim[2][2]; int pad[28]; } Texture;
extern Texture textures[];		/* opaque texture table */
#define TEXID_SCE 0
#define TEXID_PNG 1

extern void vif1SetZTest(int enb);
extern void vif1SetZWrite(int enb);
extern void vif1SetXYOffset(int field, int halfpx);
extern void vif1SetAlphaBlend(u32 type, u32 mode, u32 fix);
extern void vif1SetFlatRect(Rect *r, Color *col, u32 abe, u32 z);
extern void vif1SetTexRect(Rect *r, Rect *tr, Color *col, u32 abe, u32 z);
extern void vif1SetFramebuffer(u32 fbp, u32 psm, int width, int height, int clear);
extern void vif1SetAD(u32 a, u64 d);
extern void vif1SetTexture(void *tex);

extern void *vif1Begin(void);
extern void  vif1End(void);
extern void *pktSetAD(void *pkt, u32 a, u64 d);
extern void *pktSetTEST_1(void *pkt, u32 ate, u32 atst, u32 aref, u32 afail,
	u32 date, u32 datm, u32 zte, u32 ztst);
extern void *pktSetAlphaBlend(void *pkt, u32 type, u32 mode, u32 fix);
extern void *pktSetFlatRect(void *pkt, Rect *r, Color *col, u32 abe, u32 z);
extern void *pktSetTexRect(void *pkt, Rect *r, Rect *tr, Color *col, u32 abe, u32 z);

extern void *memset(void *s, int c, unsigned long n);
extern int IsPAL(void);
extern int GetLanguage(void);

extern void sub_2144c0(int n, int frame, int field);

/* 0x214050
 * RESIDUAL (3/123 insns matched): structure, call order and the
 * template/field-order details below are all confirmed correct (no
 * vif1SetZTest calls here - those are opening.c's own harness addition,
 * not in the real ROM). What's NOT reproduced: the real expands each
 * "flag ? 0 : screenW*screenH/64" ternary (there are two, one for the
 * TEX0 src and one for the FRAME restore) into ~25/~14 instructions
 * with a full duplicate computation in both arms; every C shape tried
 * here (ternary, if/else) collapses it to a tighter ~17/~12-instruction
 * branch-likely form instead, which desyncs every later fixed-offset
 * comparison even though the logic is identical. */
static void
DrawToExtraBuf2(void)
{
	Rect half, full;
	Color gray;
	u32 src, dstFbp;

	half.x = 0;
	half.y = 0;
	half.w = screenW/2;
	half.h = screenH;
	full.x = 0;
	full.y = 0;
	full.w = screenW;
	full.h = screenH;
	gray = grayTemplate;

	vif1SetXYOffset(0, evenOddField);
	vif1SetZWrite(0);
	vif1SetFramebuffer(extraBuf1, SCE_GS_PSMCT32, screenW, screenH, 1);

	src = extraBufFrameFlag ? 0 : screenW*screenH/64;
	vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(src, screenW/64, SCE_GS_PSMCT32,
			10, 9, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 0));
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	vif1SetTexRect(&half, &full, &gray, 0, 0xFFFFFF);

	dstFbp = extraBufFrameFlag ? 0 : screenW*screenH/64;
	vif1SetFramebuffer(dstFbp, SCE_GS_PSMCT32, screenW, screenH, 0);
	vif1SetZWrite(1);
	vif1SetXYOffset(1, evenOddField);
}

/* 0x214240 - real args (abe, blendmode, blendfix, z, gray) - the 5th
 * arg (clamped to >=0 via max(gray,0)) is NOT a field/parity flag as
 * first guessed from the prompt's "gray, field" hint: it's decoded
 * straight into col.r/g/b (col.a is a fixed 128), i.e. a runtime tint
 * level. Unlike DrawToExtraBuf2, the real builds ALL of this function's
 * GS state changes into a SINGLE vif1Begin()/vif1End() packet via
 * pktSetTEST_1/pktSetAD/pktSetAlphaBlend/pktSetTexRect (each of which
 * takes and returns the packet pointer for chaining) instead of calling
 * the vif1Set* one-shot wrappers - confirmed by decoding the packed
 * SCE_GS_SET_ZBUF/TEST/TEX0 immediates directly out of the real bytes.
 * RESIDUAL (2/160 insns matched): extraBuf1 is really loaded via a
 * 64-bit "ld" in the real (see its declaration above); declaring it
 * u64 here produced far worse codegen (a spurious 32<->64 truncation
 * dance) than accepting this one load-width delta as-is. */
static void
DrawExtraBuf2(int abe, int blendmode, int blendfix, int z, int gray)
{
	Rect full, half;
	Color col;
	void *pkt;

	if(gray < 0)
		gray = 0;

	full.x = 0;
	full.y = 0;
	full.w = screenW;
	full.h = screenH;
	half.x = 0;
	half.y = 0;
	half.w = screenW/2;
	half.h = screenH;
	col.r = gray;
	col.g = gray;
	col.b = gray;
	col.a = 128;

	pkt = vif1Begin();
	pkt = pktSetTEST_1(pkt, 0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_ALWAYS);
	pkt = pktSetAD(pkt, SCE_GS_ZBUF_1, SCE_GS_SET_ZBUF(screenW*screenH/64*2/32, SCE_GS_PSMZ24, 1));
	pkt = pktSetAD(pkt, SCE_GS_TEX0_1, SCE_GS_SET_TEX0(extraBuf1, screenW/64, SCE_GS_PSMCT24,
			10, 9, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 0));
	pkt = pktSetAD(pkt, SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pkt = pktSetAlphaBlend(pkt, abe, blendmode, blendfix);
	pkt = pktSetTexRect(pkt, &full, &half, &col, 1, z);
	pkt = pktSetAD(pkt, SCE_GS_ZBUF_1, SCE_GS_SET_ZBUF(screenW*screenH/64*2/32, SCE_GS_PSMZ24, 0));
	pkt = pktSetTEST_1(pkt, 0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_GEQUAL);
	vif1End();
}

/* 0x2144c0 - the fly-up feedback blur.
 * RESIDUAL (5/179 insns matched): same class of issue as
 * DrawToExtraBuf2 - the "frame==0 ? screenW*screenH/64 : 0" screenTbp
 * ternary here (used identically in DrawToExtraBuf2) compiles tighter
 * than whatever branch expansion the real compiler chose, desyncing
 * the fixed-offset comparison for the rest of the function even though
 * the loop body and vif1Set* call sequence are structurally right. */
void
sub_2144c0(int n, int frame, int field)
{
	Rect full, shrink;
	Color gray = grayTemplate;
	u32 screenTbp;
	int i;

	full.x = 0;
	full.y = 0;
	full.w = screenW;
	full.h = screenH;
	screenTbp = frame == 0 ? screenW*screenH/64 : 0;

	vif1SetXYOffset(0, field);
	vif1SetZWrite(0);
	vif1SetZTest(0);
	vif1SetAlphaBlend(0, 0, 0);
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	for(i = 0; i < n; i++) {
		shrink.x = 0;
		shrink.y = 0;
		shrink.w = screenW*7/8 - 1 - i*(n-1);
		shrink.h = screenH*7/8 - 1 - i*(n-1);

		vif1SetFramebuffer(extraBuf2, SCE_GS_PSMCT32, screenW, screenH, 1);
		vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(screenTbp, screenW/64, SCE_GS_PSMCT32,
				10, 9, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 0));
		vif1SetTexRect(&shrink, &full, &gray, 0, 0xFFFFFF);

		vif1SetFramebuffer(screenTbp, SCE_GS_PSMCT32, screenW, screenH, 1);
		vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(extraBuf2, screenW/64, SCE_GS_PSMCT32,
				10, 9, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 0));
		vif1SetTexRect(&full, &shrink, &gray, 0, 0xFFFFFF);
	}
	vif1SetZTest(1);
	vif1SetZWrite(1);
	vif1SetXYOffset(1, field);
}

/* 0x214790 - letterbox bars.  Confirmed structural detail: col is
 * copied from its template FIRST, then r1/r2 are zero-filled via one
 * memset(&r1, 0, 32) covering both adjacent 16-byte Rects (matches an
 * aggregate-zero-initializer for two adjacently-declared locals under
 * this compiler), with only w/h (and r2.y) patched afterward.
 * RESIDUAL (2/97 insns matched): a similar early-instruction-count
 * delta to DrawToExtraBuf2 desyncs the tail of the function. */
static void
DrawBlackBars(void)
{
	Rect r1 = {0}, r2 = {0};
	Color col = barColTemplate;
	int imgh, bar;

	imgh = (float)screenW * 9.0f * screenAY / (screenAX * 16.0f);
	bar = (screenH - imgh + 1)/2;
	r1.w = screenW;
	r1.h = bar;
	r2.y = bar + imgh;
	r2.w = screenW;
	r2.h = bar;

	vif1SetXYOffset(1, evenOddField);
	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetAlphaBlend(1, 1, 128);
	vif1SetFlatRect(&r1, &col, 1, 0xFFFFFF);
	vif1SetFlatRect(&r2, &col, 1, 0xFFFFFF);
	vif1SetZWrite(1);
	vif1SetZTest(1);
}

/* 0x214918 - full-screen 'B'/'W' fade rect.  The real builds BOTH
 * colours unconditionally: white from a pre-baked {255,255,255,0}
 * template, black via a zero-fill (memset), then patches only .a
 * before drawing whichever one the mode byte selects.  81/82 insns
 * matched; RESIDUAL is a single instruction: the real loads mode[0] via
 * signed "lb" here too, matched everywhere else this byte is read, but
 * in this one spot (right after the alpha>128 clamp) the register
 * allocator's context gives "lbu" instead - tried plain char, signed
 * char, and a separate local for the loaded byte, all reproduce the
 * same one-instruction opcode difference with zero effect on
 * semantics. */
static void
DrawSomeSprite2(const signed char *mode, int alpha)
{
	Rect r = spriteRectTemplate;
	Color colW = whiteSpriteTemplate;
	Color colB = {0, 0, 0, 0};

	r.w = screenW;
	r.h = screenH;

	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetAlphaBlend(1, 4, 0);
	if((u32)alpha > 128)
		alpha = 128;
	if(mode[0] != 'B') {
		if(mode[0] == 'W') {
			colW.a = alpha;
			vif1SetFlatRect(&r, &colW, 1, 0xFFFFFF);
		}
	} else {
		colB.a = alpha;
		vif1SetFlatRect(&r, &colB, 1, 0xFFFFFF);
	}
	vif1SetZWrite(1);
	vif1SetZTest(1);
}

/* 0x214a60 - args (0, 0, alpha), only alpha is used.
 * RESIDUAL (1/111 insns matched, needs more work): confirmed the real
 * uses the vif1Begin()/pktSetAlphaBlend()/pktSetTexRect()/vif1End()
 * pkt-chaining idiom (each pkt* call takes/returns the packet pointer,
 * as established for DrawExtraBuf2) and calls a vif1SetTexture-style
 * helper at 0x213ec0 before drawing, both reflected below. NOT yet
 * matched: the real appears to copy TWO static 16-byte UV templates
 * (0x2a4558, 0x2a4568 - 16 bytes apart) unconditionally at the top of
 * the function, rather than building one uv Rect and doing "uv.y+=32"
 * between the two pktSetTexRect calls as done here; it also scales the
 * sceTextRect index via "pal_bool<<5" (pal!=0 turned into a 0/32 byte
 * offset) rather than a 2D array index. Both need re-deriving from the
 * real bytes before this will line up. */
static void
DrawSCEText(int x, int y, int alpha)
{
	Rect xy, uv;
	Color col;
	int pal;
	void *pkt;

	pal = IsPAL();
	uv.x = 0;
	uv.y = 1;
	uv.w = 256;
	uv.h = 30;
	col.r = 128;
	col.g = 128;
	col.b = 128;
	col.a = alpha;

	vif1SetXYOffset(1, evenOddField);
	vif1SetTexture(&textures[TEXID_SCE]);

	xy = sceTextRect[pal][0];
	pkt = vif1Begin();
	pkt = pktSetAlphaBlend(pkt, 1, 4, alpha);
	pkt = pktSetTexRect(pkt, &xy, &uv, &col, 1, 0xFFFFFE);
	xy = sceTextRect[pal][1];
	uv.y += 32;
	pkt = pktSetTexRect(pkt, &xy, &uv, &col, 1, 0xFFFFFE);
	vif1End();
}

/* 0x214cb0 - args (0,0,0,alpha), only alpha is used.
 * RESIDUAL (2/108 insns matched, needs more work): same open items as
 * DrawSCEText (pkt-chaining confirmed right in shape, but the exact
 * struct/template layout at the top hasn't been re-derived byte for
 * byte) plus the PAL y/h rescale, which per the callee notes goes
 * through the softfloat helpers (fptodp/dpmul/dpdiv/dptoli) - not yet
 * verified that plain "double" arithmetic here reproduces that exact
 * call sequence. */
static void
DrawIllegalText(int x, int y, int z, int alpha)
{
	Rect xy, uv;
	Color col;
	void *pkt;

	xy.x = 64;
	xy.y = 88;
	xy.w = 512;
	xy.h = 64;
	uv.x = 0;
	uv.y = 0;
	uv.w = 512;
	uv.h = 128;
	col.r = alpha;
	col.g = alpha;
	col.b = alpha;
	col.a = 128;

	if(openingType != 1)
		return;

	vif1SetXYOffset(1, evenOddField);
	vif1SetTexture(&textures[TEXID_PNG + GetLanguage()]);
	if(IsPAL()) {
		xy.y = xy.y * 0.52627105 / 0.457627;
		xy.h = xy.h * 0.52627105 / 0.457627;
	}
	vif1SetAlphaBlend(1, 5, alpha);
	pkt = vif1Begin();
	pkt = pktSetTexRect(pkt, &xy, &uv, &col, 1, 0xFFFFFF);
	vif1End();
}

/* 0x214e60 - structure matches the real exactly through the
 * illegalTextState arm/disarm and the sub_2144c0 call (verified
 * instruction-for-instruction). RESIDUAL (33/47 insns matched): after
 * the call, the real reloads illegalTextAlpha into $a0 (register
 * allocator's free choice once sub_2144c0's own a0-a2 params die); this
 * build gets the identical control flow but the allocator picks $a1
 * instead, which flips the final clamp's movn/movz polarity too since
 * it's a register-allocation cascade, not a source-shape difference -
 * every reordering of the reload/branch tried here reproduced the same
 * $a1 choice. */
void
DoIllegalText(void)
{
	int alpha;

	if(openingPosition[2] > 800.0f)
		if(illegalTextState == 0)
			illegalTextState = 1;
	if(illegalTextState != 1)
		return;
	sub_2144c0(0, frameCount & 1, evenOddField);
	alpha = illegalTextAlpha;
	if(openingEndFlag != 0) {
		if(alpha > 0)
			illegalTextAlpha = --alpha;
	} else {
		illegalTextAlpha = ++alpha;
	}
	alpha = alpha < 113 ? alpha : 112;
	illegalTextAlpha = alpha;
	DrawIllegalText(0, 0, 0, alpha);
}
