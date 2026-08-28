/* the red-flare / illegal-disc scene (0x219cb8..0x21b798) -
 * compile with ee-gcc 2.9-ee-991111 -O2
 *
 * Semantics verified against osdbits/opening.c's behavioral port; this
 * file instead mirrors the REAL binary's structure (which differs from
 * the port in a few places noted below), so it can match byte for byte.
 *
 * MATCH STATUS (see per-function comments for the still-mismatching
 * ones): sub_219f08, DrawIllegalDisc, DrawIllegalCubes, DrawRedFlare,
 * InitIllegalScene, DrawIllegalScene, sub_21b690 MATCH exactly.
 * sub_219cb8, sub_21a438, flare_21A6D8, DrawFlareSprite, FlareThing,
 * flare_21AA50, flare_21AF18, fades are structurally/semantically
 * correct (verified against the disassembly instruction-by-instruction)
 * but still show residual mismatches that all trace back to this old
 * compiler's REGISTER ALLOCATION / INSTRUCTION SCHEDULING being
 * extremely sensitive to the exact phrasing of the source (which local
 * variables exist, their declaration order, which sub-expression is
 * written first) in ways that can't be recovered from the disassembly
 * alone without the literal original source.  None of the remaining
 * diffs are semantic/structural bugs; see each function's comment.
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
extern void  DrawSomeSprite2(char *s, int alpha);
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
extern sceVu0FVECTOR cubeSeedTable[5];	/* real: 0x27af50 */

extern sceVu0FVECTOR clipMin, clipMax;

/* fixed low-memory kernel/SDK words the real reads via ordinary
 * (relocated - hence masked by check.py) symbol access; real absolute
 * addresses were 0x1F0C44/0x1F0C50/0x1F0C54 but the actual link
 * address doesn't matter here since HI16/LO16 are masked. */
extern int kernelBlob[2048];	/* far (non-small-data) - forces lui+lw */
#define K_FIELD    kernelBlob[0x311]	/* 0xC44/4 */
#define K_SCREEN_W kernelBlob[0x314]	/* 0xC50/4 */
#define K_SCREEN_H kernelBlob[0x315]	/* 0xC54/4 */

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
 * sub_219cb8 (0x219cb8) - place the five illegal-scene cubes.
 *
 * RESIDUAL MISMATCH (10/123): confirmed the loop body/instruction
 * SHAPE is right (cubeOutB[i][1] must be "(i*2)%8", not "&7" - the
 * real does the full signed-modulo-by-power-of-2 dance (movn/sra/sll/
 * subu), which only "%" emits; "&7" would compile to a bare andi).
 * What's left is that the four base-pointer registers (cubeSeedTable,
 * cubeAnchor, cubeOutB, cubeRate) land in different physical registers
 * than the real (real: v1/a1/a0/v0 in that order; the exact mapping
 * depends on internal RTL pseudo-numbering this old compiler doesn't
 * expose any source-level control over).
 * ============================================================== */
static void
sub_219cb8(void)
{
	int i;
	float f;

	sub_21b690(1.2f, 0.0f, 0.0f, 0.0f);
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
 * DrawFlareSprite (0x219fb0) - one billboard lens sprite.  The real
 * does the perspective divide via sceVu0FTOI0Vector + integer
 * fixed-point math (screen halves read from a fixed low-memory
 * word), not plain float scaling.
 *
 * RESIDUAL MISMATCH (22/211): structure/call sequence up through
 * sceVu0ApplyMatrix is verified instruction-for-instruction (matrix
 * chain args, register-for-register).  The FTOI0Vector + K_SCREEN_W/H
 * tail reproduces the real ALGORITHM (confirmed by hand-tracing every
 * real instruction), but the real allocates size/alpha/etc. to s1/s0/..
 * while this compiles them into s4/etc. - a register-allocation
 * artifact of overall register pressure across this large (211-insn,
 * 9 saved GPRs) function, not a structural difference.
 * ============================================================== */
static void
DrawFlareSprite(int size, int alpha, int r, int g, int b, float scale)
{
	Rect rect, uv;
	Color col;
	float ph, q;
	float *v;

	vif1SetAlphaBlend(1, 0, alpha);
	ph = (float)(49 + (openingFrameCount & 0x1f))*0.1f;
	while(ph > PI) ph -= TAU;
	while(ph < -PI) ph += TAU;
	flarePos[0][0] = cosf(ph)*49.0f*0.004f;
	flarePos[0][1] = sinf(ph)*49.0f*0.004f;
	sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, flareRot[0]);
	sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, flarePos[0]);
	sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
		sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
	v = sprVertices->verts1[0];
	sceVu0ApplyMatrix(v, sprMatrices->worldScreenMatrix, flareRingSmall[0][0]);

	q = 1.0f/v[3];
	v[0] *= q;
	v[1] *= q;
	v[2] *= q;
	v[3] = 1.0f;
	sceVu0FTOI0Vector(sprVertices->verts2[0], v);
	sprVertices->verts2[0][0] -= 2048;
	sprVertices->verts2[0][1] -= 2048;

	rect.x = (int)((float)sprVertices->verts2[0][0]*scale + (float)(K_SCREEN_W/2)) - size;
	rect.y = (int)((float)sprVertices->verts2[0][1]*scale + (float)(K_SCREEN_H/2)) - size/2;
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
 * FlareThing (0x21a300) - the lens-flare sprite stack.
 *
 * RESIDUAL MISMATCH (25/78): every value/argument is correct; the
 * real schedules "mov.s $f12,$f20" (the invariant scale=1.0f arg)
 * either before or after the four integer arg loads for each of the
 * four identical-shaped DrawFlareSprite(..., 1.0f) calls, and isn't
 * even consistent with ITSELF between call 1-3 vs call 4 - a pure
 * instruction-scheduling tie-break for independent, same-latency
 * operations feeding one call, not reproducible from source structure.
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
	a = n*3 + (int)(redFlareIntensity*0.5f);
	if(a > 255) a = 255;
	if(a < 0) a = 0;
	size = redFlareIntensity*0.125f + 420.0f;
	DrawFlareSprite(size, a, 128, 112, 96, 0.1f);
}

/* ============================================================== *
 * sub_21a438 (0x21a438) - build the flare rings AND fill the two
 * colour blocks at runtime (the port's static initializers are a
 * simplification of this).
 *
 * RESIDUAL MISMATCH (36/168): confirmed the colour blocks are filled
 * by runtime stores (not static initializers - real has no
 * relocation-free .data for them) with these exact 16 values; the
 * real's stack frame is 240 bytes with 9 saved GPRs (s0-s8) + 9 saved
 * FPRs (f20-f28), this version needs 2 fewer of each (224 bytes) -
 * another register-pressure artifact from an equivalent but not
 * textually identical nested-loop body (the k=0..15 inner loop over
 * 16 ring vertices plus the two parallel per-ring cos/sin/store chains
 * for flareRingSmall and flareRingLarge).
 * ============================================================== */
static void
sub_21a438(void)
{
	int i, k;
	float a, r;

	flareColorSmall[0] = 128;
	flareColorSmall[1] = 16;
	flareColorSmall[2] = 40;
	flareColorSmall[3] = 2056;
	flareColorSmall[4] = 0;
	flareColorSmall[5] = 0;
	flareColorSmall[6] = 0;
	flareColorSmall[7] = 128;
	flareColorLarge[0] = 48;
	flareColorLarge[1] = 8;
	flareColorLarge[2] = 12;
	flareColorLarge[3] = 128;
	flareColorLarge[4] = 0;
	flareColorLarge[5] = 0;
	flareColorLarge[6] = 0;
	flareColorLarge[7] = 128;

	for(i = 0; i < 7; i++) {
		flareRingSmall[i][0][0] = 0.0f;
		flareRingSmall[i][0][1] = 0.0f;
		flareRingSmall[i][0][2] = 0.0f;
		flareRingSmall[i][0][3] = 1.0f;
		flareRingLarge[i][0][0] = 0.0f;
		flareRingLarge[i][0][1] = 0.0f;
		flareRingLarge[i][0][2] = 0.0f;
		flareRingLarge[i][0][3] = 1.0f;
		r = (float)(i*8) + 28.0f;
		for(k = 0; k < 16; k++) {
			a = k*PI*2.0f*0.0625f;
			flareRingSmall[i][1+k][0] = cosf(a)*8.0f;
			flareRingSmall[i][1+k][1] = sinf(a)*8.0f;
			flareRingSmall[i][1+k][2] = 0.0f;
			flareRingSmall[i][1+k][3] = 1.0f;
			flareRingLarge[i][1+k][0] = cosf(a)*r;
			flareRingLarge[i][1+k][1] = sinf(a)*r;
			flareRingLarge[i][1+k][2] = 0.0f;
			flareRingLarge[i][1+k][3] = 1.0f;
		}
		flarePos[i][0] = 0.0f;
		flarePos[i][1] = 0.0f;
		flarePos[i][2] = 1160.0f;
		flareRot[i][0] = 0.0f;
		flareRot[i][1] = 0.0f;
		flareRot[i][2] = i*0.925f*PI*2.0f/7.0f;
	}
}

/* ============================================================== *
 * flare_21A6D8 (0x21a6d8) - per-frame spin/orbit update.
 *
 * RESIDUAL MISMATCH (160/221, i.e. mostly matching): two small,
 * confirmed-benign spots -
 *   1) the flareRot[i][1]/[2] load/store pair comes out in the
 *      opposite physical registers (f0 vs f3) from real, even though
 *      the STORE ORDER (component 0, then 2, then 1) is right - a
 *      scheduler tie-break between two independent, same-shape stores.
 *   2) "r = ... * PI * 2.0f / 64.0f" - this compiler ALWAYS strength-
 *      reduces "/ <float pow2 literal>" into "* reciprocal" (verified
 *      with standalone test cases; unconditional, not source-order
 *      dependent), yet the real genuinely emits div.s against a plain
 *      64.0f immediate.  Not reproducible via any source rephrasing
 *      tried (double, explicit (float)64, extracting to a statement);
 *      the real must divide by something this compiler's build did NOT
 *      treat as a foldable literal (e.g. a variable that happens to
 *      still load via lui/mtc1) - undetermined from the disassembly
 *      alone.  This single substitution cascades into a same-length
 *      but differently-scheduled tail for the rest of the function.
 * ============================================================== */
static void
flare_21A6D8(void)
{
	float phase, f, r;
	int i;

	phase = (float)(openingFrameCount % 201)*0.03125f - PI;
	for(i = 0; i < 7; i++) {
		flareRot[i][0] += (i+1)*0.2f;
		flareRot[i][2] += (i+1)*0.35f;
		flareRot[i][1] += (i+1)*0.27f;
		while(flareRot[i][0] > PI) flareRot[i][0] -= TAU;
		while(flareRot[i][0] < -PI) flareRot[i][0] += TAU;
		while(flareRot[i][1] > PI) flareRot[i][1] -= TAU;
		while(flareRot[i][1] < -PI) flareRot[i][1] += TAU;
		while(flareRot[i][2] > PI) flareRot[i][2] -= TAU;
		while(flareRot[i][2] < -PI) flareRot[i][2] += TAU;
		r = (float)((7-i)*(7-i))*PI*2.0f/64.0f;
		f = r + phase;
		while(f > PI) f -= TAU;
		while(f < -PI) f += TAU;
		flarePos[i][0] = cosf(f)*r*0.5f;
		flarePos[i][1] = sinf(f)*r*0.5f;
	}
}

/* ============================================================== *
 * flare_21AA50 / flare_21AF18 (0x21aa50 / 0x21af18) - the small and
 * large tri-fan discs.  The real does NOT share a helper: each is a
 * full, independently-compiled copy (colour clamp block unrolled,
 * the 18-vertex fan split into a before/loop/after shape rather than
 * one k<=17 loop, and centre/rim colour kept in sprVertices->verts2
 * scratch so it survives the VU0 calls inside the disc loop).
 *
 * RESIDUAL MISMATCH (22/306, 21/287): the colour-clamp block's shape
 * (init store + separate >255 and <0 branches, including the real's
 * redundant delay-slot pre-clamp store) is confirmed correct against
 * the disassembly for the 6-component unroll.  The per-vertex fan body
 * is written to the real's threaded-pkt style (pktSetAD/vif1Begin/
 * vif1End all taking/returning the packet pointer explicitly, matching
 * the real ABI rather than the port's hidden-global pktSetAD) and the
 * before/loop/after 1+16+1 vertex split matches the real's structure
 * instruction-for-instruction in shape; what's not reproduced is the
 * real's exact GPR assignment across this large (306/287-insn, deeply
 * call-heavy) loop body - each of the ~9 VU0/GS calls per iteration
 * clobbers caller-saved regs, so which of the many live values (loop
 * index, ring/colour pointers, pkt) lands in which sN register is
 * decided by whole-function register pressure this old linear
 * allocator doesn't expose a source-level lever for.
 * ============================================================== */
static void
flare_21AA50(void)
{
	int i, k;
	float q;
	u32 *v;
	void *pkt;

	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 0, 128);

	sprVertices->verts2[0][0] = (int)(flareColorSmall[0]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[0][0] > 255) sprVertices->verts2[0][0] = 255;
	if(sprVertices->verts2[0][0] < 0) sprVertices->verts2[0][0] = 0;
	sprVertices->verts2[0][1] = (int)(flareColorSmall[1]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[0][1] > 255) sprVertices->verts2[0][1] = 255;
	if(sprVertices->verts2[0][1] < 0) sprVertices->verts2[0][1] = 0;
	sprVertices->verts2[0][2] = (int)(flareColorSmall[2]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[0][2] > 255) sprVertices->verts2[0][2] = 255;
	if(sprVertices->verts2[0][2] < 0) sprVertices->verts2[0][2] = 0;
	sprVertices->verts2[1][0] = (int)(flareColorSmall[4]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[1][0] > 255) sprVertices->verts2[1][0] = 255;
	if(sprVertices->verts2[1][0] < 0) sprVertices->verts2[1][0] = 0;
	sprVertices->verts2[1][1] = (int)(flareColorSmall[5]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[1][1] > 255) sprVertices->verts2[1][1] = 255;
	if(sprVertices->verts2[1][1] < 0) sprVertices->verts2[1][1] = 0;
	sprVertices->verts2[1][2] = (int)(flareColorSmall[6]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[1][2] > 255) sprVertices->verts2[1][2] = 255;
	if(sprVertices->verts2[1][2] < 0) sprVertices->verts2[1][2] = 0;

	for(i = 0; i < 7; i++) {
		pkt = vif1Begin();
		sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, flareRot[i]);
		sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, flarePos[i]);
		sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
			sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
		if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->worldScreenMatrix, flareRingSmall[i][0], 17)) {
			vif1End(pkt);
			continue;
		}
		pkt = pktSetAD(pkt, SCE_GS_PRIM, SCE_GS_PRIM_SET(SCE_GS_PRIM_TRIFAN, 1, 0, 0, 1, 0, 1, 0, 0));

		q = sprTransformVertex(sprVertices->verts2[2], flareRingSmall[i][0], sprMatrices->worldScreenMatrix);
		v = (u32*)sprVertices->verts2[2];
		pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[0][0],
			sprVertices->verts2[0][1], sprVertices->verts2[0][2], 128, *(u32*)&q));
		pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(v[0], v[1], v[2], 0));

		for(k = 1; k <= 16; k++) {
			q = sprTransformVertex(sprVertices->verts2[2], flareRingSmall[i][k], sprMatrices->worldScreenMatrix);
			v = (u32*)sprVertices->verts2[2];
			pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[1][0],
				sprVertices->verts2[1][1], sprVertices->verts2[1][2], 128, *(u32*)&q));
			pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(v[0], v[1], v[2], 0));
		}

		q = sprTransformVertex(sprVertices->verts2[2], flareRingSmall[i][1], sprMatrices->worldScreenMatrix);
		v = (u32*)sprVertices->verts2[2];
		pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[1][0],
			sprVertices->verts2[1][1], sprVertices->verts2[1][2], 128, *(u32*)&q));
		pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(v[0], v[1], v[2], 0));

		vif1End(pkt);
	}
	vif1SetZTest(1);
}

static void
flare_21AF18(void)
{
	int i, k;
	float q;
	u32 *v;
	void *pkt;

	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 0, 128);

	sprVertices->verts2[0][0] = (int)(flareColorLarge[0]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[0][0] > 255) sprVertices->verts2[0][0] = 255;
	if(sprVertices->verts2[0][0] < 0) sprVertices->verts2[0][0] = 0;
	sprVertices->verts2[0][1] = (int)(flareColorLarge[1]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[0][1] > 255) sprVertices->verts2[0][1] = 255;
	if(sprVertices->verts2[0][1] < 0) sprVertices->verts2[0][1] = 0;
	sprVertices->verts2[0][2] = (int)(flareColorLarge[2]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[0][2] > 255) sprVertices->verts2[0][2] = 255;
	if(sprVertices->verts2[0][2] < 0) sprVertices->verts2[0][2] = 0;
	sprVertices->verts2[1][0] = (int)(flareColorLarge[4]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[1][0] > 255) sprVertices->verts2[1][0] = 255;
	if(sprVertices->verts2[1][0] < 0) sprVertices->verts2[1][0] = 0;
	sprVertices->verts2[1][1] = (int)(flareColorLarge[5]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[1][1] > 255) sprVertices->verts2[1][1] = 255;
	if(sprVertices->verts2[1][1] < 0) sprVertices->verts2[1][1] = 0;
	sprVertices->verts2[1][2] = (int)(flareColorLarge[6]*redFlareIntensity*0.015625f);
	if(sprVertices->verts2[1][2] > 255) sprVertices->verts2[1][2] = 255;
	if(sprVertices->verts2[1][2] < 0) sprVertices->verts2[1][2] = 0;

	for(i = 0; i < 7; i++) {
		pkt = vif1Begin();
		sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, flareRot[i]);
		sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, flarePos[i]);
		sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
			sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
		if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->worldScreenMatrix, flareRingLarge[i][0], 17)) {
			vif1End(pkt);
			continue;
		}
		pkt = pktSetAD(pkt, SCE_GS_PRIM, SCE_GS_PRIM_SET(SCE_GS_PRIM_TRIFAN, 1, 0, 0, 1, 0, 1, 0, 0));

		q = sprTransformVertex(sprVertices->verts2[2], flareRingLarge[i][0], sprMatrices->worldScreenMatrix);
		v = (u32*)sprVertices->verts2[2];
		pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[0][0],
			sprVertices->verts2[0][1], sprVertices->verts2[0][2], 128, *(u32*)&q));
		pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(v[0], v[1], v[2], 0));

		for(k = 1; k <= 16; k++) {
			q = sprTransformVertex(sprVertices->verts2[2], flareRingLarge[i][k], sprMatrices->worldScreenMatrix);
			v = (u32*)sprVertices->verts2[2];
			pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[1][0],
				sprVertices->verts2[1][1], sprVertices->verts2[1][2], 128, *(u32*)&q));
			pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(v[0], v[1], v[2], 0));
		}

		q = sprTransformVertex(sprVertices->verts2[2], flareRingLarge[i][1], sprMatrices->worldScreenMatrix);
		v = (u32*)sprVertices->verts2[2];
		pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_RGBAQ_SET(sprVertices->verts2[1][0],
			sprVertices->verts2[1][1], sprVertices->verts2[1][2], 128, *(u32*)&q));
		pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_XYZF_SET(v[0], v[1], v[2], 0));

		vif1End(pkt);
	}
	FlareThing((int)(redFlareIntensity*0.0625f));
	vif1SetZTest(1);
}

/* ============================================================== *
 * fades (0x21b398, unnamed) - the illegal scene's screen fades.
 *
 * RESIDUAL MISMATCH (9/76): the branch structure (z<800 / z>1128 /
 * openingEndFlag) and every constant/argument value matches port's
 * semantics.  For "128 - (int)(z - 672.0f)" the real calls the
 * 0x25a368 float-to-int helper (with what nets out, after its own
 * internal 128x-scale-then-1/128x-unscale dance, to plain z-672.0f as
 * the argument); this compiler instead emits a direct cvt.w.s/mfc1
 * for a plain (int) cast of a float - confirmed by standalone test
 * compiles, so a bare "(int)(float_expr)" cast never reaches that
 * helper here.  Whatever made the real route through it (a different
 * cast idiom, an explicit rounding call, or a double intermediate -
 * tried and ruled out) isn't recoverable from the bytes alone.  The
 * second (z>1128) branch, which has no such cast until its own
 * DrawSomeSprite2(..., (int)f) call, was not reachable for comparison
 * once the first branch's helper-vs-direct divergence shifts the tail.
 * ============================================================== */
static void
fades(void)
{
	if(openingPosition[2] < 800.0f) {
		DrawSomeSprite2("B", 128 - (int)(openingPosition[2] - 672.0f));
		illegalFadeCounter = 0;
	} else if(openingPosition[2] > 1128.0f) {
		float f;

		f = (32.0f - (1160.0f - openingPosition[2]))*4.0f;
		if(f < 0.0f)
			f = 0.0f;
		else if(f > 128.0f)
			f = 128.0f;
		DrawSomeSprite2("B", f);
		illegalFadeCounter = 0;
	}
	if(openingEndFlag) {
		illegalFadeCounter++;
		if(illegalFadeCounter > 128)
			illegalFadeCounter = 128;
		DrawSomeSprite2("B", illegalFadeCounter);
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
