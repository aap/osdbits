/* the red-flare / illegal-disc scene (0x219cb8..0x21b798) -
 * compile with ee-gcc 2.9-ee-991111 -O2
 *
 * Semantics verified against osdbits/opening.c's behavioral port; this
 * file instead mirrors the REAL binary's structure (which differs from
 * the port in a few places noted below), so it can match byte for byte.
 *
 * MATCH STATUS (2026-08-29, 10/15 byte-exact): sub_219cb8,
 * DrawIllegalCubes, sub_219f08, DrawIllegalDisc, FlareThing,
 * sub_21a438, DrawRedFlare, InitIllegalScene, DrawIllegalScene,
 * sub_21b690 MATCH exactly.  Still short:
 *   flare_21A6D8   217/221 (4 words: one load/store register tie)
 *   fades           63/76  (9 words: f1/f2 allocation ties)
 *   flare_21AA50   253/306
 *   flare_21AF18   247/287
 *   DrawFlareSprite 155/211
 * See each function's comment for the recovered source shapes and the
 * residual ties.
 *
 * NOTE gp == 0x2AF070 (crt0: lui a0,0x2b; addiu a0,a0,-3984; the IDB
 * names 0x2AF070 _gp_).  Every gp offset in the comments below resolves
 * against that: redFlareIntensity gp-28888 = 0x2A7F98, sprVertices
 * gp-30996 = 0x2A775C, sprMatrices gp-31000 = 0x2A7758,
 * openingFrameCount gp-31088 = 0x2A7700, .lit4 around 0x2A71D0.
 *
 * CODEGEN LAWS learned here (all verified with standalone compiles):
 *  - "x / <float literal>" is ALWAYS strength-reduced to a multiply by
 *    the reciprocal, even for non-power-of-2 divisors that are exactly
 *    invertible; the real's div.s against a plain 64.0f therefore means
 *    the divisor was a VARIABLE in the source, and its assignment sits
 *    inside the loop (see flare_21A6D8).
 *  - float -> UNSIGNED int calls the fp-bit helper __fixunssfsi
 *    (0x25a368, which calls unpack_d at 0x2599d8); float -> signed int
 *    is an inline cvt.w.s.  A call to 0x25a368 in the ROM is thus an
 *    (unsigned) conversion, nothing more exotic (see fades).
 *  - the 0..255 clamp idiom the ROM uses everywhere is
 *      c = <expr>; lvalue = c;
 *      if(c < 256) { if(c < 0) c = 0; } else c = 255;
 *      lvalue = c;
 *    i.e. the LOW bound is tested inside the "< 256" arm.  Written the
 *    natural way round ("if(c > 255) c = 255; else if(c < 0) c = 0;")
 *    the compiler inverts the branch and emits two cmoves instead of
 *    the ROM's branch + movz + merged store.
 *  - an extern array of UNKNOWN size is not small-data, so it is
 *    reached with lui/addiu even when it physically lands in .sdata
 *    (fadeSpriteName); an extern array of known size in the same place
 *    would be gp-relative.
 *  - 2026-08-29 round: ALL FOUR remaining functions' residuals reduce to
 *    ONE recurring tie class - two unrelated same-class pseudos (both
 *    live across the same span, neither with a real ordering constraint)
 *    land on ADJACENT hard registers in the SAME relative order on both
 *    sides, but the ROM and our build pick OPPOSITE members of the pair
 *    (fades: position[2]'s value vs local `f` on $f1/$f2; DrawFlareSprite:
 *    flarePos's %hi vs the early-hoisted `size*2` on $s5/$s6; see fog.c
 *    for the DrawFog/DrawIllegalFog instances of the same thing on
 *    $s0/$s1 and $s7/$s8).  Empirically this class is INSENSITIVE to
 *    every source-shape lever in this file: local-decl order (240
 *    permutations of fades'/DrawFog's int locals compiled byte-identical
 *    regardless of order - decls with no initializer clearly generate no
 *    RTL until first use, so declaration position is a no-op for this
 *    compiler), statement order within the affected block (exhaustive
 *    24-permutation search of DrawFog's k=0..3 vertex-store loop found
 *    zero net improvement - some orderings change which of {differ,
 *    missing, extra} a mismatch is filed under without changing the
 *    aligned-match count), and explicit pointer aliasing for the
 *    contested value (a named local alias compiles identically to the
 *    inline field access every time it was tried).  Whatever decides the
 *    tie lives deeper in gcc's RTL (probably total reference count over
 *    the WHOLE function, which our restructuring attempts didn't change
 *    even when they changed the instructions actually emitted) - this
 *    matches sub_215798's own note in fog.c ("~9k campaign variants
 *    never moved it") and flare_21A6D8's residual, so treat any future
 *    $sN/$sN+1 or $fN/$fN+1 swap-only residual as PROBABLY this same
 *    class rather than re-deriving the negative result from scratch.
 *  - flare_21AA50/AF18 additionally keep the sprVertices pointer in TWO
 *    hard registers across each 6-block colour-clamp chain (loaded once,
 *    duplicated via a plain `move`, pre-clamp store through one copy,
 *    post-clamp store through the other - see the function comment); an
 *    explicit second C-level alias for one of the two stores compiles
 *    down to the SAME single register both sides would otherwise use,
 *    so this duplication is also not source-shape-reachable and is the
 *    same tie class, just manifesting as a redundant reg-reg copy
 *    instead of a swap.
 */


typedef unsigned int u32;
typedef unsigned long u64;

typedef float  sceVu0FVECTOR[4] __attribute__((aligned(16)));
typedef int    sceVu0IVECTOR[4] __attribute__((aligned(16)));
typedef float  sceVu0FMATRIX[4][4] __attribute__((aligned(16)));

#define PI 3.1415927f
#define TAU (2.0f*PI)

typedef struct Rect Rect;
struct Rect { int x, y, w, h; };

typedef struct Color Color;
struct Color { u32 r, g, b, a; };

/* member order/padding chosen only to reproduce the real offsets seen
 * in the disassembly (576=m9, 128=worldMatrix, 64=worldScreenMatrix,
 * 192=cameraScreenMatrix); relocation immediates are masked by check.py
 * so the exact field names/gaps don't matter for matching. */
typedef struct Matrices Matrices;
struct Matrices {
	sceVu0FMATRIX unit;
	sceVu0FMATRIX worldScreenMatrix;
	sceVu0FMATRIX worldMatrix;
	sceVu0FMATRIX cameraScreenMatrix;
	sceVu0FMATRIX pad4, pad5, pad6, pad7, pad8;
	sceVu0FMATRIX m9;
};

typedef struct Vertices Vertices;
struct Vertices {
	sceVu0FVECTOR xyz;
	sceVu0FVECTOR stq;
	sceVu0FVECTOR verts1[4];
	sceVu0IVECTOR verts2[4];
	sceVu0FVECTOR unk[26];
};

extern Matrices *sprMatrices;
extern Vertices *sprVertices;

/* ---- externs (real signatures - packet pointer threaded explicitly) */
extern void  vif1SetZTest(int);
extern void  vif1SetZWrite(int);
extern void  vif1SetTexture(void *tex);
extern void  vif1SetXYOffset(int, int);
extern void *vif1Begin(void);
extern void  vif1End(void *pkt);
extern void *pktSetAlphaBlend(void *pkt, u32 type, u32 mode, u32 fix);
extern void *pktSetAD(void *pkt, u32 addr, u64 data);
extern void  vif1SetAlphaBlend(int type, int mode, int fix);
extern void  vif1SetTexRect(Rect *r, Rect *uv, Color *col, int abe, u32 z);
extern float sprTransformVertex(sceVu0IVECTOR dst, sceVu0FVECTOR src, sceVu0FMATRIX mat);
extern float cosf(float);
extern float sinf(float);
extern void  sceVu0RotMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FVECTOR rot);
extern void  sceVu0TransMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FVECTOR pos);
extern void  sceVu0MulMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FMATRIX m2);
extern void  sceVu0ApplyMatrix(sceVu0FVECTOR v0, sceVu0FMATRIX m, sceVu0FVECTOR v1);
extern int   sceVu0ClipAll(sceVu0FVECTOR min, sceVu0FVECTOR max, sceVu0FMATRIX m, sceVu0FVECTOR verts, int n);
extern void  sceVu0FTOI0Vector(sceVu0IVECTOR dst, sceVu0FVECTOR src);
extern void  sceVu0Normalize(sceVu0FVECTOR dst, sceVu0FVECTOR src);
extern void  DrawToExtraBuf2(void);
extern void  DrawExtraBuf2(int a0, int a1, int fix, u32 rgb, int alpha, int field);
extern void  DrawSomeSprite2(char *s, u32 alpha);
extern char  fadeSpriteName[];	/* real: 0x2A77F0, "B" - reached with
				 * lui/addiu, i.e. NOT small-data, i.e.
				 * an extern array of unknown size */
extern void  InitIllegalDisc(void);
extern void  DrawIllegalFog(void);
extern void  DrawIllegalCube(void *work, int instance);
extern int   IsPAL(void);

/* GS register numbers / macros (freesce libgraph.h/eestruct.h shapes) */
#define SCE_GS_PRIM	0x00
#define SCE_GS_RGBAQ	0x01
#define SCE_GS_XYZF2	0x04

#define SCE_GS_PRIM_TRIFAN 5
#define SCE_GS_PRIM_SET(prim, iip, tme, fge, abe, aa1, fst, ctxt, fix) \
	((u64)(prim) | ((u64)(iip)<<3) | ((u64)(tme)<<4) | ((u64)(fge)<<5) | \
	((u64)(abe)<<6) | ((u64)(aa1)<<7) | ((u64)(fst)<<8) | ((u64)(ctxt)<<9) | ((u64)(fix)<<10))
#define SCE_GS_RGBAQ_SET(r,g,b,a,q) \
	((u64)(r) | ((u64)(g)<<8) | ((u64)(b)<<16) | ((u64)(a)<<24) | ((u64)(q)<<32))
#define SCE_GS_XYZF_SET(x,y,z,f) \
	((u64)(x) | ((u64)(y)<<16) | ((u64)(z)<<32) | ((u64)(f)<<56))

/* ---- KEY globals (see task notes for real addresses) */
extern unsigned int openingFrameCount;	/* real: unsigned - divu/unsigned-to-float dance */
extern int openingType;
extern int nextOpeningType;
extern int anotherOpeningType;
extern int sceneState;			/* real: 0x2a7710 */
extern int illegalSceneWarm;		/* real: 0x2a77e4 */
extern int illegalFadeCounter;		/* real: 0x2a77e8 */
extern int openingEndFlag;		/* real: 0x2a77f4 */
extern int openingEndFrame;		/* real: 0x2a7800 */
extern float redFlareIntensity;	/* real: 0x2a7f98 */

extern sceVu0FVECTOR openingPosition;	/* real: 0x279f30 */
extern sceVu0FVECTOR fwdDir;
extern sceVu0FVECTOR upDir;
extern float rotation;
extern sceVu0FVECTOR light1, light2, light3;

extern sceVu0FVECTOR flareRingSmall[7][17];	/* real: 0x327e00 */
extern sceVu0FVECTOR flareRingLarge[7][17];	/* real: 0x328570 */
extern sceVu0FVECTOR flareRot[7];		/* real: 0x328d20 */
extern sceVu0FVECTOR flarePos[7];		/* real: 0x328d90 */
extern int flareColorSmall[8];			/* real: 0x328ce0 - filled at runtime */
extern int flareColorLarge[8];			/* real: 0x328d00 - filled at runtime */

extern sceVu0FVECTOR cubeAnchor[5];	/* real: 0x27b0f0 */
extern sceVu0FVECTOR cubeOutB[5];	/* real: 0x27b140 */
extern sceVu0FVECTOR cubeRate[5];	/* real: 0x27b190 */
extern sceVu0FVECTOR cubeCorners[8];	/* real: 0x27b370 */
extern sceVu0FVECTOR cubeBaseColor[5];	/* real: 0x27b3f0 - instance colours */
/* 0x27AF50 is the ILLEGAL scene's own seed table.  The opening scene
 * (OpeningInitLightsCubes, 0x217ab8) reads a DIFFERENT one at 0x27A210;
 * osdbits/opening.c has only the 0x27A210 values and feeds them to both
 * inits, which is why its illegal cubes end up too far away.  Real
 * 0x27AF50 contents:
 *   {-10.4068,  4.1636, 5.0429}  {-12.9184, -4.2708, 4.3654}
 *   {  2.7639,  0.1509, 4.1075}  {  4.0958, -1.3173, 3.0952}
 *   { -2.4321,  0.5447, 2.7732} */
extern sceVu0FVECTOR cubeSeedTable[5];	/* real: 0x27af50 */

extern sceVu0FVECTOR clipMin, clipMax;

/* fixed low-memory kernel/SDK words (0x1F0C44 field, 0x1F0C50 screen
 * width, 0x1F0C54 screen height).
 * NOT a relocated symbol: the real reaches these through ONE "lui
 * reg,0x1f" whose base register is shared by both the 0xC50 and 0xC54
 * loads (DrawFlareSprite).  A relocated extern array needs an extra
 * addiu for the %lo once it is indexed twice; a struct at a literal
 * address reproduces the real exactly (verified with standalone test
 * compiles), and the resulting lui/offset immediates are the real
 * ones, so check.py compares them unmasked. */
struct KernelBlob { int pad0[785]; int field; int pad1[2]; int screenW, screenH; };
#define KB	   (*(struct KernelBlob*)0x1F0000)
#define K_SCREEN_W KB.screenW		/* 0x1F0C50 */
#define K_SCREEN_H KB.screenH		/* 0x1F0C54 */
/* DrawRedFlare's single 0xC44 read, by contrast, is the RELOCATED-symbol
 * shape: the real puts the lui in its own register ("lui v0,0x1f; lw
 * t1,3140(v0)"), which only the extern-array form produces - the literal
 * struct above reuses one register and loses the scheduling gap. */
extern int kernelBlob[2048];
#define K_FIELD    kernelBlob[0x311]	/* 0x1F0C44 */

/* forward decls (static, so .o keeps only our named functions) */
static void sub_21b690(float half, float x, float y, float z);
static void sub_219cb8(void);
static void DrawIllegalCubes(void);
static void sub_219f08(void);
static void DrawIllegalDisc(void);
static void DrawFlareSprite(int size, int alpha, int r, int g, int b, float scale);
static void FlareThing(int n);
static void sub_21a438(void);
static void flare_21A6D8(void);
static void flare_21AA50(void);
static void flare_21AF18(void);
static void fades(void);
static void DrawRedFlare(void);
static void InitIllegalScene(int fooOpeningType);
static void DrawIllegalScene(void);

extern int fooOpeningType;

extern void sub_215798(void);	/* out-of-TU: illegal fog init */

/* The per-quad shading callback DrawTexturedQuad (0x21c560) calls
 * through gp-30824.  gp == 0x2AF070, so that is 0x2A7808 (.sdata) -
 * the slot docs/object-order.md already calls "the cube callback
 * pointer".  Exactly three accesses exist in the whole ROM:
 *   0x217b5c  OpeningInitLightsCubes  sw v1 -> sub_216f88 (opening cubes)
 *   0x219d64  sub_219cb8              sw v1 -> sub_2192c0 (illegal cubes)
 *   0x21c68c  DrawTexturedQuad        lw v0, then "jalr v0" at 0x21c6bc
 * NOTE the sign extension: "lui v1,0x22; addiu v1,v1,-27968" is
 * 0x220000-0x6D40 = 0x2192C0, NOT 0x2292C0 (which is mid-function
 * anyway); sub_2192c0 sits directly in front of DrawIllegalCube. */
extern void sub_2192c0(void *quad, int corner, int face, void *work,
	int instance, int a6, int a7, float t);
extern void (*cubeQuadFunc)(void *quad, int corner, int face, void *work,
	int instance, int a6, int a7, float t);

/* ============================================================== *
 * sub_21b690 (0x21b690) - build the cube corner table and set all
 * five instances' base colour to {x,y,z}+128 (NO clamp here - a
 * later, out-of-TU function clamps when it actually uses this).
 * ============================================================== */
static void
sub_21b690(float half, float x, float y, float z)
{
	cubeCorners[0][0] = -half; cubeCorners[0][1] = -half; cubeCorners[0][2] = -half; cubeCorners[0][3] = 1.0f;
	cubeCorners[1][0] =  half; cubeCorners[1][1] = -half; cubeCorners[1][2] = -half; cubeCorners[1][3] = 1.0f;
	cubeCorners[2][0] = -half; cubeCorners[2][1] =  half; cubeCorners[2][2] = -half; cubeCorners[2][3] = 1.0f;
	cubeCorners[3][0] =  half; cubeCorners[3][1] =  half; cubeCorners[3][2] = -half; cubeCorners[3][3] = 1.0f;
	cubeCorners[4][0] = -half; cubeCorners[4][1] = -half; cubeCorners[4][2] =  half; cubeCorners[4][3] = 1.0f;
	cubeCorners[5][0] =  half; cubeCorners[5][1] = -half; cubeCorners[5][2] =  half; cubeCorners[5][3] = 1.0f;
	cubeCorners[6][0] = -half; cubeCorners[6][1] =  half; cubeCorners[6][2] =  half; cubeCorners[6][3] = 1.0f;
	cubeCorners[7][0] =  half; cubeCorners[7][1] =  half; cubeCorners[7][2] =  half; cubeCorners[7][3] = 1.0f;

	cubeBaseColor[0][0] = x + 128.0f;
	cubeBaseColor[0][1] = y + 128.0f;
	cubeBaseColor[0][2] = z + 128.0f;
	cubeBaseColor[0][3] = 128.0f;
	cubeBaseColor[1][0] = x + 128.0f;
	cubeBaseColor[1][1] = y + 128.0f;
	cubeBaseColor[1][2] = z + 128.0f;
	cubeBaseColor[1][3] = 128.0f;
	cubeBaseColor[2][0] = x + 128.0f;
	cubeBaseColor[2][1] = y + 128.0f;
	cubeBaseColor[2][2] = z + 128.0f;
	cubeBaseColor[2][3] = 128.0f;
	cubeBaseColor[3][0] = x + 128.0f;
	cubeBaseColor[3][1] = y + 128.0f;
	cubeBaseColor[3][2] = z + 128.0f;
	cubeBaseColor[3][3] = 128.0f;
	cubeBaseColor[4][0] = x + 128.0f;
	cubeBaseColor[4][1] = y + 128.0f;
	cubeBaseColor[4][2] = z + 128.0f;
	cubeBaseColor[4][3] = 128.0f;
}

/* ============================================================== *
 * sub_219cb8 (0x219cb8) - place the five illegal-scene cubes.  MATCH.
 *
 * Recovered shapes: (a) "cubeOutB[i][1] = f*((i*2)%8)/5.0f" really is
 * "%", not "&7" - the ROM does the full signed-modulo-by-power-of-2
 * dance (slt/movn/sra/sll/subu), which only "%" emits; (b) the cube
 * quad callback store (see the cubeQuadFunc note above) is the FIRST
 * statement after sub_21b690(), which is what pins the four table base
 * pointers into v1/a1/a0/v0 the way the ROM has them - without it every
 * base register shifts by one and 13 words differ.
 *
 * NOTE the ILLEGAL seed table is 0x27AF50, a DIFFERENT table from the
 * opening's 0x27A210 (which OpeningInitLightsCubes reads).  See the
 * cube-distance report: osdbits/opening.c uses the opening table in
 * both places.
 * ============================================================== */
static void
sub_219cb8(void)
{
	int i;
	float f;

	sub_21b690(1.2f, 0.0f, 0.0f, 0.0f);
	cubeQuadFunc = sub_2192c0;
	for(i = 0; i < 5; i++) {
		f = (i-2)*0.8f;
		if(f == 0.0f)
			f = 0.9f;
		cubeAnchor[i][0] = cubeSeedTable[i][0];
		cubeAnchor[i][1] = cubeSeedTable[i][1];
		cubeAnchor[i][2] = (cubeSeedTable[i][2] - 2.5f)*128.0f + 800.0f - 12.0f;
		cubeAnchor[i][3] = 0.0f;
		cubeOutB[i][0] = f*((i*2)%9)*0.25f;
		cubeOutB[i][1] = f*((i*2)%8)/5.0f;
		cubeOutB[i][2] = f*((i*2)%7)/6.0f;
		cubeOutB[i][3] = f*((i*2)%9)*0.25f;
		cubeRate[i][0] = 0.004f/f;
		cubeRate[i][1] = f*0.003f;
		cubeRate[i][2] = f/800.0f + 0.002f;
		cubeRate[i][3] = 0.0f;
	}
}

/* ============================================================== *
 * DrawIllegalCubes (0x219ea8) - real DrawIllegalCube takes an
 * explicit work buffer allocated on THIS frame (1200-byte stack).
 * ============================================================== */
static void
DrawIllegalCubes(void)
{
	struct { char pad[1168]; } work;
	int i;

	sceVu0Normalize(sprVertices->verts1[3], fwdDir);
	vif1SetZTest(0);
	for(i = 0; i < 5; i++)
		DrawIllegalCube(&work, i);
	vif1SetZTest(1);
}

/* ============================================================== *
 * sub_219f08 (0x219f08) - InitOpening's real init-chain wrapper.
 * ============================================================== */
static void
sub_219f08(void)
{
	sub_21a438();
	sub_215798();
	sub_219cb8();
	illegalSceneWarm = 0;
	openingEndFlag = 0;
	openingEndFrame = 0;
}

/* ============================================================== *
 * DrawIllegalDisc (0x219f40) - sceneState switch: init/draw/end.
 * ============================================================== */
static void
DrawIllegalDisc(void)
{
	switch(sceneState) {
	case 0:
		InitIllegalScene(fooOpeningType);
		sceneState++;
		/* fall through */
	case 1:
		DrawIllegalScene();
		break;
	case 2:
		nextOpeningType = 2;
		sceneState = 0;
		break;
	}
}

/* ============================================================== *
 * DrawFlareSprite (0x219fb0) - one billboard lens sprite.  155/211.
 *
 * Recovered shapes:
 *  - the 49.0f is a VARIABLE, not a literal: the ROM computes
 *    "rad*0.004f" ONCE (mul.s f20,f22,f20) and reuses it for both the
 *    cos and the sin store, which only happens if "(rad*0.004f)" is a
 *    common subexpression, which in turn requires rad to be a variable
 *    (a literal 49.0f*0.004f would be folded at compile time).
 *  - the phase is "(rad + (openingFrameCount & 0x1f))*0.1f" - a FLOAT
 *    add of 49.0f to the (unsigned!) masked frame counter, not
 *    "(float)(49 + ...)"; that is where the unsigned-to-float
 *    srl/or/add.s dance comes from.
 *  - there is NO "float *v" alias for sprVertices->verts1[0]: the ROM
 *    reloads sprVertices from gp after every call, which a local
 *    pointer (kept in a callee-saved register) would prevent.
 *  - the projected coordinates are WRITTEN BACK into
 *    sprVertices->verts2[0][0..1] and rect.x/rect.y are then computed
 *    FROM the stored values (hence the ROM's swc1-then-lw round trip on
 *    the y component).
 *  - K_SCREEN_W/H are reached through ONE shared "lui reg,0x1f" (see
 *    the KernelBlob note above).
 *
 * Residual (41 differ / 15+15): 2026-08-29 re-check - the "CSE into
 * fully-formed pointers" theory above is WRONG for the current source;
 * both sides already keep %hi(flarePos/flareRot/flareRingSmall) in s5,
 * s7, s8 and only addiu the %lo when a pointer VALUE is actually needed
 * (the shapes are identical instruction-for-instruction).  The real
 * divergence is narrower: flarePos's %hi lands in $s6 in the ROM but
 * $s5 in ours, and the OTHER member of that pair is the completely
 * unrelated `size*2` (rect.w) that gcc hoists and schedules into a
 * callee-saved register right in the middle of this block for its own
 * scheduling reasons (real: $s5; ours: $s6) - i.e. this is the
 * $sN/$sN+1 tie class from the file header, not a hoisting-strategy
 * difference; every use of flarePos later in the function differs by
 * exactly this one swapped register letter, which is where all 41
 * words come from.  Everything from the prologue through
 * sceVu0FTOI0Vector matches word for word.
 * ============================================================== */
static void
DrawFlareSprite(int size, int alpha, int r, int g, int b, float scale)
{
	Rect rect, uv;
	Color col;
	float ph, q, rad;

	vif1SetAlphaBlend(1, 0, alpha);
	rad = 49.0f;
	ph = (rad + (openingFrameCount & 0x1f))*0.1f;
	while(ph > PI) ph -= TAU;
	while(ph < -PI) ph += TAU;
	flarePos[0][0] = cosf(ph)*(rad*0.004f);
	flarePos[0][1] = sinf(ph)*(rad*0.004f);
	sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, flareRot[0]);
	sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, flarePos[0]);
	sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
		sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
	sceVu0ApplyMatrix(sprVertices->verts1[0], sprMatrices->worldScreenMatrix, flareRingSmall[0][0]);

	q = 1.0f/sprVertices->verts1[0][3];
	sprVertices->verts1[0][0] *= q;
	sprVertices->verts1[0][1] *= q;
	sprVertices->verts1[0][2] *= q;
	sprVertices->verts1[0][3] = 1.0f;
	sceVu0FTOI0Vector(sprVertices->verts2[0], sprVertices->verts1[0]);
	sprVertices->verts2[0][0] -= 2048;
	sprVertices->verts2[0][1] -= 2048;

	sprVertices->verts2[0][0] = (int)((float)sprVertices->verts2[0][0]*scale + (float)(K_SCREEN_W/2));
	rect.x = sprVertices->verts2[0][0] - size;
	sprVertices->verts2[0][1] = (int)((float)sprVertices->verts2[0][1]*scale + (float)(K_SCREEN_H/2));
	rect.y = sprVertices->verts2[0][1] - size/2;
	rect.w = size*2;
	rect.h = size;
	uv.x = 0;
	uv.y = 0;
	uv.h = 128;
	uv.w = 128;
	col.r = r;
	col.g = g;
	col.b = b;
	/* col.a left uninitialized - the real never writes it (FIX
	 * blending ignores it) */
	vif1SetTexRect(&rect, &uv, &col, 1, 0xFFFFFF);
}

/* ============================================================== *
 * FlareThing (0x21a300) - the lens-flare sprite stack.  MATCH.
 *
 * Recovered shapes: the four fixed-size sprites are written out one
 * call per size (the ROM has no loop), and "a" is computed as a FLOAT
 * sum "n*3 + redFlareIntensity*0.5f" that is then converted, not an
 * int plus a converted float.  The clamp uses the "< 256" idiom
 * described in the file header.
 * ============================================================== */
extern char textures[16][64];	/* placeholder texture table (far array) */
#define TEXID_FLAR 4

static void
FlareThing(int n)
{
	int m, a, size;

	if(n > 8)
		n = 8;
	vif1SetTexture(textures[TEXID_FLAR]);
	m = n + 2;
	DrawFlareSprite(112, m, 128, 64, 64, 1.0f);
	DrawFlareSprite(170, m, 128, 64, 64, 1.0f);
	DrawFlareSprite(256, m, 128, 64, 64, 1.0f);
	DrawFlareSprite(448, m, 128, 64, 64, 1.0f);
	a = n*3 + redFlareIntensity*0.5f;
	if(a < 256) { if(a < 0) a = 0; } else a = 255;
	size = redFlareIntensity*0.125f + 420.0f;
	/* NOTE: gp-32368 == 0x2A7200 holds 0.9f, not 0.1f - the port's
	 * 0.1f is wrong (the .lit4 immediate is masked by check.py, so
	 * this does not show up as a mismatch). */
	DrawFlareSprite(size, a, 128, 112, 96, 0.9f);
}

/* ============================================================== *
 * sub_21a438 (0x21a438) - build the flare rings AND fill the two
 * colour blocks at runtime.  MATCH.
 *
 * Recovered shapes (all found by watching which pseudo gets which
 * register / which store lands where):
 *  - the sixteen colour words are written in the interleaved order
 *    Small[0..2], Large[0..2], Small[3], Small[4..7], Large[4..7],
 *    Large[3] - the constant-to-register order (v0=48, a1=128, t0=16,
 *    t1=40, t2=8, t3=12, t4=2056) only comes out with 8 and 12 created
 *    before 2056, and Large[3]=128 has to be the LAST store of all.
 *  - the ring-centre zeros are CHAINED
 *    (ring[i][0][0] = ring[i][0][1] = ring[i][0][2] = 0.0f), which is
 *    why they are stored from one FP register instead of "sw zero";
 *    the per-vertex "[2] = 0.0f" inside the k loop is standalone and
 *    does get "sw zero".
 *  - the large ring's radius is written inline at both use sites
 *    ("cosf(a)*((float)(i*8) + 28.0f)"), not lifted to a variable
 *    before the loop - as a variable it would be hoisted out of the k
 *    loop, but the ROM recomputes mtc1/cvt/add every iteration.
 *  - flareRot's zeros come BEFORE flarePos's, and flarePos's chain runs
 *    the other way ("flarePos[i][1] = flarePos[i][0] = 0.0f").
 * ============================================================== */
static void
sub_21a438(void)
{
	int i, k;
	float a;

	flareColorSmall[0] = 128;
	flareColorSmall[1] = 16;
	flareColorSmall[2] = 40;
	flareColorLarge[0] = 48;
	flareColorLarge[1] = 8;
	flareColorLarge[2] = 12;
	flareColorSmall[3] = 2056;
	flareColorSmall[4] = 0;
	flareColorSmall[5] = 0;
	flareColorSmall[6] = 0;
	flareColorSmall[7] = 128;
	flareColorLarge[4] = 0;
	flareColorLarge[5] = 0;
	flareColorLarge[6] = 0;
	flareColorLarge[3] = 128;
	flareColorLarge[7] = 128;

	for(i = 0; i < 7; i++) {
		flareRingSmall[i][0][0] = flareRingSmall[i][0][1] = flareRingSmall[i][0][2] = 0.0f;
		flareRingSmall[i][0][3] = 1.0f;
		flareRingLarge[i][0][0] = flareRingLarge[i][0][1] = flareRingLarge[i][0][2] = 0.0f;
		flareRingLarge[i][0][3] = 1.0f;
		for(k = 0; k < 16; k++) {
			a = k*PI*2.0f*0.0625f;
			flareRingSmall[i][1+k][0] = cosf(a)*8.0f;
			flareRingSmall[i][1+k][1] = sinf(a)*8.0f;
			flareRingSmall[i][1+k][2] = 0.0f;
			flareRingSmall[i][1+k][3] = 1.0f;
			flareRingLarge[i][1+k][0] = cosf(a)*((float)(i*8) + 28.0f);
			flareRingLarge[i][1+k][1] = sinf(a)*((float)(i*8) + 28.0f);
			flareRingLarge[i][1+k][2] = 0.0f;
			flareRingLarge[i][1+k][3] = 1.0f;
		}
		flareRot[i][0] = flareRot[i][1] = 0.0f;
		flareRot[i][2] = i*0.925f*PI*2.0f/7.0f;
		flarePos[i][1] = flarePos[i][0] = 0.0f;
		flarePos[i][2] = 1160.0f;
	}
}

/* ============================================================== *
 * flare_21A6D8 (0x21a6d8) - per-frame spin/orbit update.  217/221.
 *
 * Recovered shape: "/64.0f" cannot be a literal (this compiler always
 * turns float-division-by-literal into a reciprocal multiply, verified
 * standalone).  The ROM loads 0x42800000 into f21 INSIDE the loop and
 * then overwrites f21 with the quotient, i.e. the divisor variable
 * shares a register with r and its assignment is inside the loop and
 * is not the only assignment to that variable - "r = 64.0f; r = .../r;"
 * reproduces exactly that (a separate variable "d = 64.0f" gets hoisted
 * into its own callee-saved register and costs 8 words).
 *
 * MATCHES (221/221, and the TU's 36-word .lit4 pool is byte-exact).
 * The increments are written in ADDRESS order [0], [1], [2] - that is
 * what makes the ROM's store order come out.  Counter-intuitively the
 * pool then holds 0.2, 0.35, 0.27: .lit4 emission order is NOT naive
 * first-use statement order here (the scheduler's load clustering
 * decides), so never infer statement order from pool order alone -
 * check.py's #lit4 comparator now verifies the pool bytes directly.
 * ============================================================== */
static void
flare_21A6D8(void)
{
	float phase, f, r;
	int i;

	phase = (float)(openingFrameCount % 201)*0.03125f - PI;
	for(i = 0; i < 7; i++) {
		flareRot[i][0] += (i+1)*0.2f;
		flareRot[i][1] += (i+1)*0.27f;
		flareRot[i][2] += (i+1)*0.35f;
		while(flareRot[i][0] > PI) flareRot[i][0] -= TAU;
		while(flareRot[i][0] < -PI) flareRot[i][0] += TAU;
		while(flareRot[i][1] > PI) flareRot[i][1] -= TAU;
		while(flareRot[i][1] < -PI) flareRot[i][1] += TAU;
		while(flareRot[i][2] > PI) flareRot[i][2] -= TAU;
		while(flareRot[i][2] < -PI) flareRot[i][2] += TAU;
		r = 64.0f;
		r = (float)((7-i)*(7-i))*PI*2.0f/r;
		f = r + phase;
		while(f > PI) f -= TAU;
		while(f < -PI) f += TAU;
		flarePos[i][0] = cosf(f)*r*0.5f;
		flarePos[i][1] = sinf(f)*r*0.5f;
	}
}

/* ============================================================== *
 * flare_21AA50 / flare_21AF18 (0x21aa50 / 0x21af18) - the small and
 * large tri-fan discs.  253/306 and 247/287.
 *
 * The two are NOT the same code with a different table: 21AF18 is 19
 * instructions shorter because it draws the whole 17-vertex fan in ONE
 * loop with an "if(k == 0)" picking the centre colour, while 21AA50
 * peels the centre vertex out and runs a 16-iteration loop after it.
 * Recovered shapes common to both:
 *  - the fan vertices are walked with an explicit "float *w" pointer
 *    (w += 4), not by indexing ring[i][k]; 21AA50 counts DOWN
 *    (k = 15; k >= 0; k--) from &ring[i][1], 21AF18 counts up
 *    (k = 0; k < 17; k++) from &ring[i][0].
 *  - the RGBAQ/XYZF2 operands are plain "sprVertices->verts2[..][..]"
 *    ints (signed lw), not a "u32 *v" alias (which produces lwu and
 *    keeps sprVertices in a callee-saved register instead of the ROM's
 *    reload-from-gp before every packet write).
 *  - the clip test wraps the body ("if(ClipAll(...) == 0) { ... }"
 *    followed by an unconditional vif1End) rather than doing
 *    "if(clipped) { vif1End; continue; }" - the ROM branches straight
 *    to the shared vif1End at the bottom of the loop.
 *  - six clamp blocks in the "< 256" idiom through a local int c.
 *
 * Residual: the ROM keeps TWO copies of the sprVertices pointer across
 * the clamp blocks (lw a0; move a1,a0; the FIRST (pre-clamp) store of
 * each block uses one copy, the SECOND (post-clamp, after the merge of
 * the if/else) uses the other) where we keep one, the loop counter and
 * i+1 are swapped between two callee-saved registers, and the ROM
 * re-materialises the constant 272 and the &ring[0][1] address instead
 * of reusing the hoisted copies.  All register/hoisting ties.
 *
 * 2026-08-29: confirmed unmovable via source shape.  Tried and
 * rejected for the sprVertices duplicate: an explicit second pointer
 * (`int *v0 = sprVertices->verts2[0];` etc. used for one of the two
 * stores per block) - compiles to the exact same single register both
 * stores already use, because the two accesses are provably the same
 * value with no intervening call and this compiler's CSE folds them
 * regardless of how the source spells the second reference.  Whatever
 * makes the ROM re-derive (not alias) the value across the if/else
 * merge is internal to gcc, not reachable from here; see the file
 * header's $sN/$sN+1 tie-class note - flareColorSmall/Large's %hi also
 * belongs to that same family ($a1 here vs $a2 in the ROM, which is
 * simply the just-freed third argument register of the preceding
 * vif1SetAlphaBlend(1,0,128) call reused for a new purpose - free in
 * both builds, picked differently by each).
 * ============================================================== */
static void
flare_21AA50(void)
{
	int i, k, c;
	float q;
	float *w;
	void *pkt;

	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 0, 128);

	c = (int)(flareColorSmall[0]*redFlareIntensity*0.015625f);
	sprVertices->verts2[0][0] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[0][0] = c;
	c = (int)(flareColorSmall[1]*redFlareIntensity*0.015625f);
	sprVertices->verts2[0][1] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[0][1] = c;
	c = (int)(flareColorSmall[2]*redFlareIntensity*0.015625f);
	sprVertices->verts2[0][2] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[0][2] = c;
	c = (int)(flareColorSmall[4]*redFlareIntensity*0.015625f);
	sprVertices->verts2[1][0] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[1][0] = c;
	c = (int)(flareColorSmall[5]*redFlareIntensity*0.015625f);
	sprVertices->verts2[1][1] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[1][1] = c;
	c = (int)(flareColorSmall[6]*redFlareIntensity*0.015625f);
	sprVertices->verts2[1][2] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[1][2] = c;
	for(i = 0; i < 7; i++) {
		pkt = vif1Begin();
		sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, flareRot[i]);
		sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, flarePos[i]);
		sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
			sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
		if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->worldScreenMatrix, flareRingSmall[i][0], 17) == 0) {
			pkt = pktSetAD(pkt, SCE_GS_PRIM, SCE_GS_PRIM_SET(SCE_GS_PRIM_TRIFAN, 1, 0, 0, 1, 0, 1, 0, 0));

			q = sprTransformVertex(sprVertices->verts2[2], flareRingSmall[i][0], sprMatrices->worldScreenMatrix);
			pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[0][0],
				sprVertices->verts2[0][1], sprVertices->verts2[0][2], 128, *(u32*)&q));
			pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(sprVertices->verts2[2][0],
				sprVertices->verts2[2][1], sprVertices->verts2[2][2], 0));

			w = flareRingSmall[i][1];
			for(k = 15; k >= 0; k--) {
				q = sprTransformVertex(sprVertices->verts2[2], w, sprMatrices->worldScreenMatrix);
				pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[1][0],
					sprVertices->verts2[1][1], sprVertices->verts2[1][2], 128, *(u32*)&q));
				pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(sprVertices->verts2[2][0],
					sprVertices->verts2[2][1], sprVertices->verts2[2][2], 0));
				w += 4;
			}

			q = sprTransformVertex(sprVertices->verts2[2], flareRingSmall[i][1], sprMatrices->worldScreenMatrix);
			pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[1][0],
				sprVertices->verts2[1][1], sprVertices->verts2[1][2], 128, *(u32*)&q));
			pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(sprVertices->verts2[2][0],
				sprVertices->verts2[2][1], sprVertices->verts2[2][2], 0));

		}
		vif1End(pkt);
	}
	vif1SetZTest(1);
}

static void
flare_21AF18(void)
{
	int i, k, c;
	float q;
	float *w;
	void *pkt;

	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 0, 128);

	c = (int)(flareColorLarge[0]*redFlareIntensity*0.015625f);
	sprVertices->verts2[0][0] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[0][0] = c;
	c = (int)(flareColorLarge[1]*redFlareIntensity*0.015625f);
	sprVertices->verts2[0][1] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[0][1] = c;
	c = (int)(flareColorLarge[2]*redFlareIntensity*0.015625f);
	sprVertices->verts2[0][2] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[0][2] = c;
	c = (int)(flareColorLarge[4]*redFlareIntensity*0.015625f);
	sprVertices->verts2[1][0] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[1][0] = c;
	c = (int)(flareColorLarge[5]*redFlareIntensity*0.015625f);
	sprVertices->verts2[1][1] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[1][1] = c;
	c = (int)(flareColorLarge[6]*redFlareIntensity*0.015625f);
	sprVertices->verts2[1][2] = c;
	if(c < 256) { if(c < 0) c = 0; } else c = 255;
	sprVertices->verts2[1][2] = c;

	for(i = 0; i < 7; i++) {
		pkt = vif1Begin();
		sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, flareRot[i]);
		sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, flarePos[i]);
		sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
			sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
		if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->worldScreenMatrix, flareRingLarge[i][0], 17) == 0) {
			pkt = pktSetAD(pkt, SCE_GS_PRIM, SCE_GS_PRIM_SET(SCE_GS_PRIM_TRIFAN, 1, 0, 0, 1, 0, 1, 0, 0));

			w = flareRingLarge[i][0];
			for(k = 0; k < 17; k++) {
				q = sprTransformVertex(sprVertices->verts2[2], w, sprMatrices->worldScreenMatrix);
				if(k == 0)
					pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[0][0],
						sprVertices->verts2[0][1], sprVertices->verts2[0][2], 128, *(u32*)&q));
				else
					pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[1][0],
						sprVertices->verts2[1][1], sprVertices->verts2[1][2], 128, *(u32*)&q));
				pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(sprVertices->verts2[2][0],
					sprVertices->verts2[2][1], sprVertices->verts2[2][2], 0));
				w += 4;
			}

			q = sprTransformVertex(sprVertices->verts2[2], flareRingLarge[i][1], sprMatrices->worldScreenMatrix);
			pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[1][0],
				sprVertices->verts2[1][1], sprVertices->verts2[1][2], 128, *(u32*)&q));
			pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(sprVertices->verts2[2][0],
				sprVertices->verts2[2][1], sprVertices->verts2[2][2], 0));

		}
		vif1End(pkt);
	}
	FlareThing((int)(redFlareIntensity*0.0625f));
	vif1SetZTest(1);
}

/* ============================================================== *
 * fades (0x21b398, unnamed) - the illegal scene's screen fades.  63/76.
 *
 * Recovered shapes:
 *  - DrawSomeSprite2's alpha parameter is UNSIGNED: both branches route
 *    their float through __fixunssfsi (0x25a368).  A signed (int) cast
 *    would have been an inline cvt.w.s.
 *  - the first branch's argument really is
 *    "(128.0f - (672.0f - z) - 128.0f)*128.0f/128.0f" - five FP ops
 *    that net out to z-672 but that the compiler does not fold (only
 *    the /128.0f becomes a reciprocal multiply).
 *  - the alpha must be computed into a LOCAL before the call; written
 *    inline as the second argument, the string address is materialised
 *    before the __fixunssfsi call and so needs a callee-saved register
 *    (32-byte frame + "move a0,s0"), where the ROM has a 16-byte frame
 *    and builds the address straight into a0 afterwards.
 *  - the 0..128 clamp writes a SECOND variable (g), leaving f intact -
 *    hence the ROM's "mtc1 zero,f12" + two "mov.s f12,.." arms.
 *  - the "B" sprite name is an extern array of unknown size at
 *    0x2A77F0 (see fadeSpriteName).
 *
 * Residual (9 words): f1/f2 allocation ties only.  2026-08-29: confirmed
 * unmovable - openingPosition[2]'s value sits in $f1 for the WHOLE
 * function in the ROM (loaded once, reused by both branches) and in $f2
 * for us, with local `f` taking the other of the pair in each branch;
 * tried and rejected: hoisting `float z = openingPosition[2];` to a
 * named top-of-function local (byte-identical output - this compiler's
 * CSE already treats the two array reads as one value regardless of
 * whether a source-level temp names it), and swapping `f`/`g`'s
 * declaration order within the else-if block (also byte-identical - see
 * the file-header law about declarations with no initializer generating
 * no RTL until first use).  This is the $fN/$fN+1 tie class described
 * in the file header; see that note before spending more time here.
 * ============================================================== */
static void
fades(void)
{
	u32 a;

	if(openingPosition[2] < 800.0f) {
		a = 128 - (u32)((128.0f - (672.0f - openingPosition[2]) - 128.0f)*128.0f/128.0f);
		DrawSomeSprite2(fadeSpriteName, a);
		illegalFadeCounter = 0;
	} else if(openingPosition[2] > 1128.0f) {
		float f, g;

		f = (32.0f - (1160.0f - openingPosition[2]))*4.0f;
		if(f < 0.0f)
			g = 0.0f;
		else if(f > 128.0f)
			g = 128.0f;
		else
			g = f;
		a = g;
		DrawSomeSprite2(fadeSpriteName, a);
		illegalFadeCounter = 0;
	}
	if(openingEndFlag) {
		illegalFadeCounter++;
		if(illegalFadeCounter > 128)
			illegalFadeCounter = 128;
		DrawSomeSprite2(fadeSpriteName, illegalFadeCounter);
	}
}

/* ============================================================== *
 * DrawRedFlare (0x21b4c8)
 * ============================================================== */
static void
DrawRedFlare(void)
{
	redFlareIntensity = (740.0f - (1160.0f - openingPosition[2]))*128.0f/740.0f*0.6f;
	vif1SetZWrite(0);
	flare_21A6D8();
	flare_21AA50();
	flare_21AF18();
	vif1SetZWrite(1);
	DrawExtraBuf2(1, 2, 112, 0xffffff, 128, K_FIELD);
	DrawToExtraBuf2();
}

/* ============================================================== *
 * InitIllegalScene (0x21b570)
 * ============================================================== */
static void
InitIllegalScene(int fooOpeningType)
{
	openingPosition[0] = 0.0f;
	openingPosition[1] = 0.0f;
	openingPosition[2] = 320.0f;
	openingPosition[3] = 0.0f;
	fwdDir[0] = 0.0f;
	fwdDir[1] = -0.03f;
	fwdDir[2] = 1.0f;
	fwdDir[3] = 1.0f;
	upDir[0] = 0.0f;
	upDir[1] = 1.0f;
	upDir[2] = 0.0f;
	upDir[3] = 1.0f;
	rotation = 0.0f;
	InitIllegalDisc();
	light1[0] = 0.0f;  light1[1] = 0.0f;  light1[2] = -1.0f; light1[3] = 0.0f;
	light2[0] = 0.5f;  light2[1] = 0.5f;  light2[2] = 0.0f;  light2[3] = 0.0f;
	light3[0] = -0.5f; light3[1] = -0.5f; light3[2] = 0.0f;  light3[3] = 0.0f;
	DrawToExtraBuf2();
}

/* ============================================================== *
 * DrawIllegalScene (0x21b648)
 * ============================================================== */
static void
DrawIllegalScene(void)
{
	if(illegalSceneWarm == 0) {
		illegalSceneWarm = 1;
		return;
	}
	DrawRedFlare();
	DrawIllegalFog();
	DrawIllegalCubes();
	fades();
}
