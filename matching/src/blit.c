/* the framebuffer blits and text overlay (0x214050..0x214f20) -
 * compile with ee-gcc 2.9-ee-991111 -O2 (self-contained, no -I needed).
 *
 * THE ONE IDIOM THAT DOMINATES THIS TU (found 2026-08-29):
 * every "ldl/ldr + sdl/sdr" run in these functions is gcc 2.9 copying a
 * 16-byte, 4-byte-aligned struct.  They are NOT hand-written struct
 * assignments - they are LOCAL AGGREGATE INITIALIZERS with non-constant
 * elements:
 *
 *	Rect half = { 0, 0, screenW/2, screenH };
 *
 * gcc 2.9 builds such an initializer field by field into a stack TEMP
 * and then block-copies the temp onto the variable, and its temp-slot
 * allocator happily parks the temp on the (not yet live) slot of the
 * NEXT declared local - which is why the copies always run
 * "sp+N+16 -> sp+N".  A plain "half.x = 0; ... half.h = screenH;"
 * sequence stores straight into the variable and emits no copy at all,
 * which is what the previous round did and why nothing lined up.
 * Verified minimal repro: three initializers reproduce the real
 * 0x214050 prologue instruction for instruction.
 *
 * SEMANTIC DELTAS vs osdbits/opening.c's port (flagged, NOT fixed
 * there - opening.c is read-only from here; each is a candidate live
 * bug):
 *  1. DrawToExtraBuf2's source select is "frameCount & 1", read from
 *     the SAME gp slot DoIllegalText's frameCount uses (-31088), not
 *     from evenOddFrame (0x1f0c40) as the port assumes.
 *  2. DrawToExtraBuf2's trailing vif1SetFramebuffer - the one the port
 *     calls "role unclear - not ported" and replaces with a FRAME
 *     restore at clear=0 - is the same "frameCount&1 ? 0 :
 *     screenW*screenH/64" expression, PSMCT32, clear=1.  It re-points
 *     FRAME at the screen buffer AND clears, exactly like the first
 *     call.
 *  3. TEX0 TH: 9 in DrawToExtraBuf2 but 8 in DrawExtraBuf2 and
 *     sub_2144c0 (the port uses GetTexExponent(screenH)=8 in all
 *     three).  TW is a literal 10 in all three.
 *  4. TEX0 TBW is a compile-time 10 in DrawToExtraBuf2 and
 *     DrawExtraBuf2 (only sub_2144c0 computes screenW/64).  Same value
 *     at 640 wide, but hard-coded in the ROM.
 *  5. TEX0 CLD is 0 in DrawToExtraBuf2; the port passes 1.
 *  6. DrawToExtraBuf2/sub_2144c0 pass extraBuf1/extraBuf2 to
 *     vif1SetFramebuffer RAW.  The port divides them by 32 there while
 *     using them unshifted for TEX0, so the port's globals must hold a
 *     different unit than the ROM's.  Only DrawExtraBuf2's ZBUF
 *     address has a /32 in the ROM.
 *  7. DrawExtraBuf2 really takes 5 arguments (abe, blendmode,
 *     blendfix, z, gray) - there is no trailing "field" argument as
 *     the port's call-site comment guesses.
 *  8. DrawIllegalText's PAL rescale divides by the FLOAT 0.457627f
 *     (stored as a float widened to double), and casts the int to
 *     float first; the port divides by the double 0.457627.
 *  9. DrawSomeSprite2's mode string is read with lbu -> unsigned char.
 * 10. sub_2144c0 keeps its two buffer addresses in a 2-element int
 *     ARRAY (that is why the ROM spills them to the stack instead of
 *     giving them registers).
 */

typedef unsigned int u32;
typedef unsigned long u64;	/* SCE ee-gcc: long is 64-bit */

/* the handful of GS macros this TU needs, from the SDK's eestruct.h */
#define SCE_GS_TEX0_1	0x06
#define SCE_GS_TEX1_1	0x14
#define SCE_GS_ZBUF_1	0x4e

#define SCE_GS_PSMCT32	0
#define SCE_GS_PSMCT24	1
#define SCE_GS_PSMZ24	49
#define SCE_GS_MODULATE	0
#define SCE_GS_LINEAR	1
#define SCE_GS_DEPTH_ALWAYS	1
#define SCE_GS_DEPTH_GEQUAL	2

#define SCE_GS_SET_TEX0(tbp, tbw, psm, tw, th, tcc, tfx,		\
			cbp, cpsm, csm, csa, cld)			\
	((u64)(tbp)         | ((u64)(tbw) << 14) |			\
	((u64)(psm) << 20)  | ((u64)(tw) << 26) |			\
	((u64)(th) << 30)   | ((u64)(tcc) << 34) |			\
	((u64)(tfx) << 35)  | ((u64)(cbp) << 37) |			\
	((u64)(cpsm) << 51) | ((u64)(csm) << 55) |			\
	((u64)(csa) << 56)  | ((u64)(cld) << 61))

#define SCE_GS_SET_TEX1(lcm, mxl, mmag, mmin, mtba, l, k)		\
	((u64)(lcm)         | ((u64)(mxl) << 2) |			\
	((u64)(mmag) << 5)  | ((u64)(mmin) << 6) |			\
	((u64)(mtba) << 9)  | ((u64)(l) << 19) |			\
	((u64)(k) << 32))

#define SCE_GS_SET_ZBUF(zbp, psm, zmsk)					\
	((u64)(zbp) | ((u64)(psm) << 24) | ((u64)(zmsk) << 32))

typedef struct Rect { int x, y, w, h; } Rect;
typedef struct Color { u32 r, g, b, a; } Color;

/* screenW/screenH/evenOddFrame/evenOddField are lui-addressed absolute
 * memory locations in the real binary (0x1f0c40/44/50/54), not
 * relocatable extern symbols - a page-aligned struct pointer at
 * 0x1f0000 with the fields at their real byte offsets reproduces the
 * real "lui BASE,0x1f; lw x,0xc50(BASE); lw y,0xc54(BASE)" shared-page
 * addressing, with one lui shared by every field access (an
 * "extern struct" at 0x1f0c40 instead emits a separate lui per field,
 * because gcc will not CSE %hi(sym+N) across different N - measured,
 * it costs ~250 aligned words across this TU).
 *
 * evenOddField, and ONLY evenOddField, must additionally be VOLATILE.
 * That is not cosmetic: for a non-volatile absolute address gcc emits
 * the load as a single gas macro ("lw $5,2034756" -> lui $5 + lw
 * $5,%lo($5)), which reuses the destination register and is also
 * eligible for the following jal's delay slot; the ROM instead has the
 * two-register form ("lui v0,0x1f; lw a1,3140(v0)") and, in
 * DrawIllegalText, an UNFILLED delay slot after it - both of which are
 * exactly what gcc emits for a volatile MEM.  Adding the qualifier
 * turned DrawToExtraBuf2, DrawBlackBars and DoIllegalText from
 * near-misses into exact matches in one step.  Semantically it is the
 * vblank-updated field parity, so a volatile is what the original
 * source would have had anyway. */
struct DispRegs {
	char pad[0xc40];
	int evenOddFrame, evenOddField, pad0, pad1, screenW, screenH;
};
#define DISP  ((struct DispRegs *)0x1f0000)
#define VDISP ((volatile struct DispRegs *)0x1f0000)
#define evenOddFrame DISP->evenOddFrame
#define evenOddField VDISP->evenOddField
#define screenW      DISP->screenW
#define screenH      DISP->screenH

/* small gp-resident state (the gp offsets are masked by check.py, so
 * only "plain scalar extern" matters; addresses below are for the
 * record, computed off gp = 0x2AF070).
 *
 * NOTE: DrawToExtraBuf2's frame-parity flag and DoIllegalText's
 * frameCount are read from the SAME gp slot (-31088 in both), so the
 * previous round's separate "extraBufFrameFlag" was a phantom - the
 * TEX0 source select really is "frameCount & 1". */
extern int frameCount;		/* 0x2a7700 */
extern int openingType;		/* 0x2a7704 */
extern int openingEndFlag;	/* 0x2a77f4 */
extern int illegalTextState;	/* 0x2a7f8c */
extern int illegalTextAlpha;	/* 0x2a7f90 */

extern float openingPosition[4];

/* the save/working buffers, allocated at runtime by gsAllocBuffer() but
 * already block-address units by the time these functions read them (no
 * /32 shift at the use sites in the real code) - hence the wide "ld"
 * load, matching a u64-sized declaration. */
/* extraBuf1 sits at 0x279f18 and is read with a 64-bit "ld", extraBuf2
 * at 0x279f10 with a 32-bit "lw".  Both are addressed as ordinary
 * symbols through a %hi/%lo pair in TWO registers ("lui v0,0x28;
 * ld a0,-24808(v0)"), i.e. NOT gp-relative even though a scalar u64/u32
 * would fall inside the default -G8 small-data window; declaring them
 * as incomplete-size arrays is what keeps gcc out of .sdata and
 * reproduces the two-register form (a cast-to-absolute-address instead
 * folds the lui into the destination register). */
extern u64 extraBuf1[];
extern u32 extraBuf2[];

extern Color grayTemplate;	/* 0x2a42d8: {128,128,128,128} */
extern Color barColTemplate;	/* 0x2a42e8: {255,255,255,128} */
extern Rect  spriteRectTemplate;	/* 0x2a42f8: {0,0,640,224} */
extern Color whiteSpriteTemplate;	/* 0x2a4308: {255,255,255,0} */
extern float screenAX, screenAY;	/* 0x2a7730, 0x2a7734 */

/* 0x2a4318: the SCE-logo destination rectangles, {NTSC,PAL} x {line0,
 * line1}.  The ROM indexes it as a flat 8-int row per TV mode (base+4,
 * base+8, ... each separately added to the pal*32 byte offset), NOT as
 * a Rect[2][2] - a struct-typed 2D array would emit one shared base
 * pointer and an ldl/ldr block copy instead of the four lw/sw pairs the
 * ROM has. */
extern int sceTextRect[2][8];
extern Rect sceTextXYTemplate;	/* 0x2a4558: {0,0,256,16} */
extern Rect sceTextUVTemplate;	/* 0x2a4568: {0,1,256,30} */
extern Rect illegalXYTemplate;	/* 0x2a4578: {64,88,512,64} */
extern Rect illegalUVTemplate;	/* 0x2a4588: {0,0,512,128} */

/* the texture descriptors are 240 bytes each (the ROM multiplies the
 * language index by 240); the illegal-disc text starts at index 13. */
typedef struct Texture { int pad[60]; } Texture;
extern Texture textures[];
#define TEXID_SCE 0
#define TEXID_PNG 13

extern void vif1SetZTest(int enb);
extern void vif1SetZWrite(int enb);
extern void vif1SetXYOffset(int field, int halfpx);
extern void vif1SetAlphaBlend(u32 type, u32 mode, u32 fix);
extern void vif1SetFlatRect(Rect *r, Color *col, u32 abe, u32 z);
extern void vif1SetTexRect(Rect *r, Rect *tr, Color *col, u32 abe, u32 z);
extern void vif1SetFramebuffer(u64 fbp, u32 psm, int width, int height, int clear);
extern void vif1SetAD(u32 a, u64 d);
extern void vif1SetTexture(void *tex);

extern void *vif1Begin(void);
extern void  vif1End(void);
extern void *pktSetAD(void *pkt, u32 a, u64 d);
extern void *pktSetTEST_1(void *pkt, u32 ate, u32 atst, u32 aref, u32 afail,
	u32 date, u32 datm, u32 zte, u64 ztst);
extern void *pktSetAlphaBlend(void *pkt, u32 type, u32 mode, u32 fix);
extern void *pktSetFlatRect(void *pkt, Rect *r, Color *col, u32 abe, u32 z);
extern void *pktSetTexRect(void *pkt, Rect *r, Rect *tr, Color *col, u32 abe, u32 z);

extern int IsPAL(void);
extern int GetLanguage(void);

extern void sub_2144c0(int n, int frame, int field);

/* 0x214050 - MATCHES.
 * Source shapes recovered from the ROM:
 *  - the three locals are aggregate INITIALIZERS (see the file header),
 *    declared half, full, gray in that order;
 *  - the tbp selector is written INLINE as the macro argument, not
 *    hoisted into a local: gcc duplicates the whole SCE_GS_SET_TEX0 /
 *    call-argument expression into both arms of the ternary (the
 *    constant 0x668028000 is materialised twice, and the "or" with the
 *    tbp only appears in the non-zero arm).  Hoisting it into a
 *    variable, as the previous round did, collapses that to a single
 *    tighter block;
 *  - the condition is "flag & 1" (andi+bnez), not "flag != 0";
 *  - TEX0's tbw is a compile-time 10 here, NOT screenW/64 (sub_2144c0
 *    does use the runtime screenW/64, so this is a real source
 *    difference, not folding), and TH is 9;
 *  - both vif1SetFramebuffer calls pass clear=1 (see SEMANTIC note). */
static void
DrawToExtraBuf2(void)
{
	Rect half = { 0, 0, screenW/2, screenH };
	Rect full = { 0, 0, screenW, screenH };
	Color gray = grayTemplate;

	vif1SetXYOffset(0, evenOddField);
	vif1SetZWrite(0);
	vif1SetFramebuffer(extraBuf1[0], SCE_GS_PSMCT32, screenW, screenH, 1);

	vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(frameCount & 1 ? 0 : screenW*screenH/64,
			10, SCE_GS_PSMCT32, 10, 9, 1, SCE_GS_MODULATE, 0, 0, 0, 0, 0));
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	vif1SetTexRect(&half, &full, &gray, 0, 0xFFFFFF);

	vif1SetFramebuffer(frameCount & 1 ? 0 : screenW*screenH/64,
		SCE_GS_PSMCT32, screenW, screenH, 1);
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
 * full/half are aggregate initializers (the ldl/ldr idiom, see file
 * header) but col is NOT - it is built by four plain field stores, so
 * no block copy is emitted for it.  TEX0 here has TH=8 (DrawToExtraBuf2
 * uses 9) and, like there, a compile-time TBW of 10.  pktSetTEST_1's
 * 9th argument goes on the stack with "sd", not "sw", so that parameter
 * is 64-bit.  The ZBUF address must be written "(X/64<<1)/32": spelled
 * "X/64*2/32" fold() collapses it to X/1024 (one addiu 1023 + sra 10)
 * and the ROM plainly has the three-step /64, *2, /32 sequence.
 *
 * RESIDUAL 141/160, 17 differ, all of them register NAMES: the seven
 * callee-saved pseudos are allocated
 *   ROM   s2=z s3=blendfix s4=&half s5=&col s6=abe s7=blendmode s8=&full
 *   ours  s2=z s3=&col     s4=abe   s5=mode s6=fix s7=&full     s8=&half
 * i.e. a pure allocation-order permutation with identical instruction
 * text.  Levers tried with no effect: declaring pkt first, u32 vs int
 * parameter types, casting z at the call, and moving vif1Begin() ahead
 * of the col stores (that one is strictly worse, 112/160).  Left for a
 * permuter pass. */
static void
DrawExtraBuf2(int abe, int blendmode, int blendfix, int z, int gray)
{
	Rect full = { 0, 0, screenW, screenH };
	Rect half = { 0, 0, screenW/2, screenH };
	Color col;
	void *pkt;

	if(gray < 0)
		gray = 0;

	col.r = gray;
	col.g = gray;
	col.b = gray;
	col.a = 128;

	pkt = vif1Begin();
	pkt = pktSetTEST_1(pkt, 0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_ALWAYS);
	pkt = pktSetAD(pkt, SCE_GS_ZBUF_1, SCE_GS_SET_ZBUF((screenW*screenH/64<<1)/32, SCE_GS_PSMZ24, 1));
	pkt = pktSetAD(pkt, SCE_GS_TEX0_1, SCE_GS_SET_TEX0(extraBuf1[0], 10, SCE_GS_PSMCT24,
			10, 8, 1, SCE_GS_MODULATE, 0, 0, 0, 0, 0));
	pkt = pktSetAD(pkt, SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pkt = pktSetAlphaBlend(pkt, abe, blendmode, blendfix);
	pkt = pktSetTexRect(pkt, &full, &half, &col, 1, z);
	pkt = pktSetAD(pkt, SCE_GS_ZBUF_1, SCE_GS_SET_ZBUF((screenW*screenH/64<<1)/32, SCE_GS_PSMZ24, 0));
	pkt = pktSetTEST_1(pkt, 0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_GEQUAL);
	vif1End();
}

/* 0x2144c0 - the fly-up feedback blur.  MATCHES.
 * Source shapes recovered from the ROM's stack layout (gray at sp+0,
 * full at sp+16, two ints at sp+32/36, shrink at sp+48, an aggregate
 * temp at sp+64):
 *  - gray is the FIRST declaration (a plain copy of the template, no
 *    temp - copies from an lvalue need none), full the second (an
 *    aggregate initializer, temp at sp+32);
 *  - screenTbp and buf2 are two int locals that land in the 16 bytes
 *    the (already freed) sp+32 temp gave back - which pins them as
 *    declarations, not spills;
 *  - buf2 MUST be a local: gcc cannot hoist the extraBuf2 load out of
 *    the loop itself (the vif1Set* calls could clobber it), yet the ROM
 *    loads it exactly once before the loop;
 *  - shrink is declared INSIDE the loop body with an aggregate
 *    initializer (that is what puts its temp at sp+64, above shrink,
 *    instead of reusing the sp+32/36 hole);
 *  - TEX0's TH is 8 here (DrawToExtraBuf2 uses 9) and TBW really is the
 *    runtime screenW/64. */
void
sub_2144c0(int n, int frame, int field)
{
	Color gray = grayTemplate;
	Rect full = { 0, 0, screenW, screenH };
	int bufs[2];
	int i;

	bufs[0] = frame == 0 ? screenW*screenH/64 : 0;
	bufs[1] = extraBuf2[0];

	vif1SetXYOffset(0, field);
	vif1SetZWrite(0);
	vif1SetZTest(0);
	vif1SetAlphaBlend(0, 0, 0);
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	for(i = 0; i < n; i++) {
		Rect shrink = { 0, 0, screenW*7/8 - (i*(n-1) + 1),
				screenH*7/8 - (i*(n-1) + 1) };

		vif1SetFramebuffer(bufs[1], SCE_GS_PSMCT32, screenW, screenH, 1);
		vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(bufs[0], screenW/64, SCE_GS_PSMCT32,
				10, 8, 1, SCE_GS_MODULATE, 0, 0, 0, 0, 0));
		vif1SetTexRect(&shrink, &full, &gray, 0, 0xFFFFFF);

		vif1SetFramebuffer(bufs[0], SCE_GS_PSMCT32, screenW, screenH, 1);
		vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(bufs[1], screenW/64, SCE_GS_PSMCT32,
				10, 8, 1, SCE_GS_MODULATE, 0, 0, 0, 0, 0));
		vif1SetTexRect(&full, &shrink, &gray, 0, 0xFFFFFF);
	}
	vif1SetZTest(1);
	vif1SetZWrite(1);
	vif1SetXYOffset(1, field);
}

/* 0x214790 - letterbox bars.  MATCHES.  Confirmed structural detail: col is
 * copied from its template FIRST (slot sp+0), and the two bars are ONE
 * two-element ARRAY, not two separate Rects: the single 32-byte
 * aggregate initializer is what produces the single memset(temp, 0, 32)
 * at sp+48 followed by one 32-byte block copy down to sp+16.  Two
 * separate Rect initializers would emit two 16-byte clears and two
 * copies.  Only .w is set in the initializer; the three .h/.y patches
 * come afterwards and, being independent stores, are emitted
 * last-store-first (bars[1].h, bars[1].y, bars[0].h). */
static void
DrawBlackBars(void)
{
	Color col = barColTemplate;
	Rect bars[2] = { { 0, 0, screenW, 0 }, { 0, 0, screenW, 0 } };
	int imgh, bar;

	imgh = (float)screenW * 9.0f * screenAY / (screenAX * 16.0f);
	bar = (screenH - imgh + 1)/2;
	bars[0].h = bar;
	bars[1].y = bar + imgh;
	bars[1].h = bar;

	vif1SetXYOffset(1, evenOddField);
	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetAlphaBlend(1, 1, 128);
	vif1SetFlatRect(&bars[0], &col, 1, 0xFFFFFF);
	vif1SetFlatRect(&bars[1], &col, 1, 0xFFFFFF);
	vif1SetZWrite(1);
	vif1SetZTest(1);
}

/* 0x214918 - full-screen 'B'/'W' fade rect.  MATCHES.  The real builds BOTH
 * colours unconditionally: white from a pre-baked {255,255,255,0}
 * template, black via a zero-fill (memset), then patches only .a
 * before drawing whichever one the mode byte selects.  81/82 insns
 * matched.  The one byte load is "lbu", so mode is an UNSIGNED char
 * pointer (ee-gcc's plain char is signed); it is loaded once and both
 * comparisons reuse the register. */
static void
DrawSomeSprite2(const unsigned char *mode, int alpha)
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

/* 0x214a60 - args (0, 0, alpha), only alpha is used.  109/111.
 * Source shapes recovered from the ROM:
 *  - "int pal = IsPAL() != 0;" is the FIRST declaration (that is why the
 *    call precedes the two template copies) and the "!= 0" is real: the
 *    ROM normalises the result with sltu before scaling it by 32
 *    (sll 5), which a bare IsPAL() index would not do;
 *  - xy and uv are initialised from two SEPARATE 16-byte templates
 *    (0x2a4558/0x2a4568 - each gets its own lui, so they are two
 *    symbols, not one two-element array), and xy's initializer is DEAD:
 *    both draws overwrite all four fields.  gcc keeps the store because
 *    xy's address escapes to pktSetTexRect;
 *  - the per-draw xy update is four scalar assignments out of the flat
 *    sceTextRect row, not a struct copy (see that extern's comment);
 *  - col is filled .a FIRST, then .r/.g/.b: with the usual r,g,b,a
 *    order every scratch register in the two template copies and the
 *    128 constant shifts by one (11 extra register diffs), which is how
 *    the field order was pinned down.
 *
 * RESIDUAL 2 words, 0 differ: the ROM emits the two "addu" that form
 * the [pal][1]/[pal][3] addresses BEFORE the two "lw" that use the
 * [pal][0]/[pal][2] ones, this build emits them after - a pure sched
 * tie inside one basic block.  Reordering the four field assignments
 * and moving "uv.y += 32" both only make it worse (103, 101, 90 and
 * 107/111 respectively). */
static void
DrawSCEText(int x, int y, int alpha)
{
	int pal = IsPAL() != 0;
	Rect xy = sceTextXYTemplate;
	Rect uv = sceTextUVTemplate;
	Color col;
	void *pkt;

	col.a = alpha;
	col.r = 128;
	col.g = 128;
	col.b = 128;

	vif1SetXYOffset(1, evenOddField);
	vif1SetTexture(&textures[TEXID_SCE]);

	xy.x = sceTextRect[pal][0];
	xy.y = sceTextRect[pal][1];
	xy.w = sceTextRect[pal][2];
	xy.h = sceTextRect[pal][3];
	pkt = vif1Begin();
	pkt = pktSetAlphaBlend(pkt, 1, 4, alpha);
	pkt = pktSetTexRect(pkt, &xy, &uv, &col, 1, 0xFFFFFE);
	xy.x = sceTextRect[pal][4];
	xy.y = sceTextRect[pal][5];
	xy.w = sceTextRect[pal][6];
	xy.h = sceTextRect[pal][7];
	uv.y += 32;
	pkt = pktSetTexRect(pkt, &xy, &uv, &col, 1, 0xFFFFFE);
	vif1End();
}

/* 0x214cb0 - args (0,0,0,alpha), only alpha is used.  89/108, and the
 * instruction COUNT and text now agree exactly (0 missing, 0 extra).
 * Source shapes recovered from the ROM:
 *  - "int pal = IsPAL();" is the first declaration and its result is
 *    DEAD (v0 is immediately overwritten by GetLanguage's); IsPAL() is
 *    called a second time for the actual test further down.  The dead
 *    call is really in the ROM - gcc cannot delete it - so the source
 *    genuinely had a leftover unused local here;
 *  - "int lang = GetLanguage();" is likewise hoisted into a declaration
 *    (the call sits at the top, not at the vif1SetTexture use site);
 *  - xy/uv come from two separate 16-byte templates, as in DrawSCEText;
 *  - the openingType guard comes AFTER col is filled in (the col stores
 *    are before the branch, one of them in its delay slot);
 *  - the PAL rescale is DOUBLE arithmetic (softfloat fptodp/dpmul/
 *    dpdiv/dptoli): the multiplier is a plain double literal, but the
 *    divisor's stored bits are a FLOAT value widened to double, so it
 *    is written 0.457627f in the source;
 *  - the int operand carries an explicit (float) cast: the ROM converts
 *    it with cvt.s.w and then calls the float->double helper
 *    (0x25a468), whereas an uncast int would call the int->double
 *    helper with the value in a0.  This one cast is what closed the
 *    last instruction-count gap here.
 *
 * RESIDUAL 19 words, all register names, confined to the two template
 * copies and the 128/1/openingType constants:
 *   ROM   copy1 scratch a0,a1  copy2 scratch v0,a1  128=a0 type=v0 1=a1
 *   ours  copy1 scratch v1,a1  copy2 scratch t0,t1  128=v1 type=a1 1=v0
 * (ours burns two extra scratch registers on the second copy).  Levers
 * tried with no effect: "int pal = IsPAL(), lang = GetLanguage();" as
 * one declaration, pkt declared before col, and the col field order
 * (unlike DrawSCEText, it makes no difference here).  Same tie class as
 * DrawExtraBuf2. */
static void
DrawIllegalText(int x, int y, int z, int alpha)
{
	int pal = IsPAL();
	int lang = GetLanguage();
	Rect xy = illegalXYTemplate;
	Rect uv = illegalUVTemplate;
	Color col;
	void *pkt;

	col.a = 128;
	col.r = alpha;
	col.g = alpha;
	col.b = alpha;

	if(openingType != 1)
		return;

	vif1SetXYOffset(1, evenOddField);
	vif1SetTexture(&textures[TEXID_PNG + lang]);
	if(IsPAL()) {
		xy.y = (float)xy.y * 0.52627105 / 0.457627f;
		xy.h = (float)xy.h * 0.52627105 / 0.457627f;
	}
	vif1SetAlphaBlend(1, 5, alpha);
	pkt = vif1Begin();
	pkt = pktSetTexRect(pkt, &xy, &uv, &col, 1, 0xFFFFFF);
	vif1End();
}

/* 0x214e60 - MATCHES.  The clamp is a separate named limit variable (the ROM
 * keeps alpha in a0 across it and computes the clamped value into a
 * fresh register: slti 113 / li 112 / movn).  The winning phrasing is
 * "lim = alpha; if(alpha >= 113) lim = 112;" - gcc inverts it into
 * "load 112, conditionally move alpha in".  Writing it the DoSCEText
 * way round ("lim = 112; if(alpha < 113) lim = alpha;") makes gcc reuse
 * the 112 it already has in a register and compare with slt against
 * that register instead of slti against 113. */
void
DoIllegalText(void)
{
	int alpha, lim;

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
	lim = alpha;
	if(alpha >= 113)
		lim = 112;
	illegalTextAlpha = lim;
	DrawIllegalText(0, 0, 0, lim);
}
