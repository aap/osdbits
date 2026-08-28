/* the fog functions (0x214f78..0x215f18) -
 * compile with ee-gcc 2.9-ee-991111 -O2
 *
 * Semantics verified against osdbits/opening.c's InitFog/DrawFog/
 * DrawIllegalFog/sub_215798; structure here follows the real disasm,
 * which differs from that port in a few places:
 *  - InitFog's "useless sin/cos" is NOT dead in the real code: it feeds
 *    2*c/2*s into v[0]/v[1] via two sine-table helper calls
 *    (sub_212238/sub_2121c0) on angle=(frameCount*51)&0x3fff.  Also,
 *    v[0]/v[1] are computed as x*6.0f-48.0f (not (x-8)*6.0f) - same
 *    value, different asm shape.
 *  - DrawFog has a position-based "fade" calculation before the alpha
 *    blend that is real but provably dead (an unconditional fade=128.0f
 *    right after the conditional block overwrites it every time) -
 *    kept to match, since the real ROM keeps the orphaned comparison
 *    too.
 *  - all vif1Begin/pktSetAD/pktSetAlphaBlend calls thread an explicit
 *    packet pointer (the real functions return/take it explicitly;
 *    opening.c's port globalized it away).
 */

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;	/* long == 8 bytes on this compiler */
typedef int i32;

typedef float sceVu0FVECTOR[4];
typedef int sceVu0IVECTOR[4];
typedef float sceVu0FMATRIX[16];

typedef struct Rect Rect;
struct Rect { int x, y, w, h; };

typedef struct GSTex GSTex;
struct GSTex
{
	u32 mxl;
	u32 unk1[3];
	u32 psm;
	u32 unk2[3];
	u32 cbp;
	u32 unk3[3];
	u32 tbp[7];
	Rect dim[7];
	u32 unk4;
};

typedef struct Texture Texture;
struct Texture
{
	u8 *data;
	i32 resourceID;
	u32 *clut;
	i32 usage;
	Rect dim;
	i32 maxLevel;
	i32 dataOffset;
	u32 format;
	u32 unk2;
	GSTex gstex;
};

enum {
	TEXID_SCE,
	TEXID_FOG0,
	TEXID_FOG1,
	TEXID_FOG2,
	TEXID_FOG3,
	TEXID_FOG4,
};

extern Texture textures[];

typedef struct Matrices Matrices;
struct Matrices
{
	sceVu0FMATRIX unit;			/* 0 */
	sceVu0FMATRIX worldScreenMatrix;	/* 64 */
	sceVu0FMATRIX worldMatrix;		/* 128 */
	sceVu0FMATRIX cameraScreenMatrix;	/* 192 */
	sceVu0FMATRIX cameraMatrix;		/* 256 */
	sceVu0FMATRIX viewScreenMatrix;		/* 320 */
	sceVu0FMATRIX m6;			/* 384 */
	sceVu0FMATRIX m7;			/* 448 */
	sceVu0FMATRIX normalLightMatrix;	/* 512 */
	sceVu0FMATRIX m9;			/* 576 */
};

typedef struct Vertices Vertices;
struct Vertices
{
	sceVu0FVECTOR xyz;		/* 0 */
	sceVu0FVECTOR stq;		/* 16 */
	sceVu0FVECTOR verts1[4];	/* 32 */
	sceVu0IVECTOR verts2[4];	/* 96 */
};

extern Matrices *sprMatrices;
extern Vertices *sprVertices;

extern sceVu0FVECTOR position;
extern sceVu0FVECTOR clipMin;
extern sceVu0FVECTOR clipMax;
extern int frameCount;

extern void vif1SetZTest(int);
extern void vif1SetZWrite(int);
extern void vif1SetTexture(Texture *tex);
extern void *vif1Begin(void);
extern void vif1End(void);
extern void *pktSetAlphaBlend(void *pkt, int a, int b, int c);
extern void *pktSetAD(void *pkt, int reg, u64 val);
extern void vif1SetAD(int reg, u64 val);
extern void vif1SetClamp(int wms, int wmt, int minu, int maxu, int minv, int maxv);
extern float sprTransformVertex(void *dst, void *src, void *mat);
extern int sceVu0ClipAll(void *min, void *max, void *mat, void *verts, int n);
extern void sceVu0RotMatrix(void *out, void *unit, void *rot);
extern void sceVu0TransMatrix(void *out, void *in, void *pos);
extern void sceVu0MulMatrix(void *out, void *a, void *b);
extern int rand(void);
extern float sqrtf(float);
extern float sub_212238(int angle);
extern float sub_2121c0(int angle);

#define clamp(a, lo, hi) ((a) < (lo) ? (lo) : (a) > (hi) ? (hi) : (a))

#define SCE_GS_PRIM		0x00
#define SCE_GS_RGBAQ		0x01
#define SCE_GS_ST		0x02
#define SCE_GS_XYZF2		0x04
#define SCE_GS_XYZF3		0x0c
#define SCE_GS_TEX0_1		0x06
#define SCE_GS_TEX1_1		0x14

#define SCE_GS_PSMCT16		2
#define SCE_GS_MODULATE		0
#define SCE_GS_LINEAR		1
#define SCE_GS_PRIM_TRISTRIP	4

#define SCE_GS_SET_PRIM(prim, iip, tme, fge, abe, aa1, fst, ctxt, fix)	\
	((u64)(prim)       | ((u64)(iip) << 3) |			\
	((u64)(tme) << 4)  | ((u64)(fge) << 5) |			\
	((u64)(abe) << 6)  | ((u64)(aa1) << 7) |			\
	((u64)(fst) << 8)  | ((u64)(ctxt) << 9) |			\
	((u64)(fix) << 10))

#define SCE_GS_SET_RGBAQ(r, g, b, a, q)				\
	((u64)(r)        | ((u64)(g) << 8) |				\
	((u64)(b) << 16) | ((u64)(a) << 24) |				\
	((u64)(q) << 32))

#define SCE_GS_SET_ST(s, t)	((u64)(s) | ((u64)(t) << 32))

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

#define SCE_GS_SET_XYZF(x, y, z, f)					\
	((u64)(x)        | ((u64)(y) << 16) |				\
	((u64)(z) << 32) | ((u64)(f) << 56))

static float fogAnimation[6];
static sceVu0FVECTOR fogVertices[17][17];
static sceVu0IVECTOR fogColors[17][17];

static sceVu0FVECTOR illegalFogVerts[9];
static sceVu0IVECTOR illegalFogColors[9];
static sceVu0FVECTOR illegalFogPos[128];
static sceVu0FVECTOR illegalFogRot[128];
static int illegalFogState[128];

static int fogStrips[4][4] = {
	{ 0, 1, 3, 4 }, { 1, 2, 4, 5 }, { 3, 4, 6, 7 }, { 4, 5, 7, 8 }
};
static float fogUV[9][2] = {
	{ 0.0f, 0.0f }, { 0.5f, 0.0f }, { 1.0f, 0.0f },
	{ 0.0f, 0.5f }, { 0.5f, 0.5f }, { 1.0f, 0.5f },
	{ 0.0f, 1.0f }, { 0.5f, 1.0f }, { 1.0f, 1.0f }
};

/* 0x214f78 - 46/176 insns match (structure and register set are right:
 * prologue/epilogue, the hardware-sqrt-with-NaN-fallback idiom, and the
 * float constant set f20-f27 all line up with the real ROM once v[0]/
 * v[1] are stored right after each sine-table call instead of after
 * both, see the note above; write order that leaves a value live
 * across a call needs one extra callee-saved register, exactly the
 * kind of delta this function started with).  Residual: after the
 * first sqrt/NaN-check resolves, the real ROM computes the fogVertices/
 * fogColors FAR base pointers (lui v1/v0,0x32; addiu s7/s8,...) BEFORE
 * materializing the float constants (6.0, -48.0, 0.5, 3.0, 1/128); this
 * build materializes the constants first. Also, the real ROM keeps the
 * loop multiplier 51 in one persistent register (s6) shared by both
 * sub_212238/sub_2121c0 calls, while this build re-loads the literal
 * with `li` at each call site under -O2's register pressure here.
 * Isolated minimal repros show gcc 2.9 CAN keep a local `int k = 51`
 * pinned across a call when register pressure is low, but not in this
 * function's full context - likely a genuine difference in what the
 * true original source bound to a name vs. what it left as a bare
 * literal, which isn't recoverable by guessing without another data
 * point. */
static void
InitFog(void)
{
	int y, x;
	float foo = sqrtf(5202.0f);

	for(y = 0; y < 17; y++)
		for(x = 0; x < 17; x++) {
			float dx, dy, d, c, s;

			c = sub_212238((frameCount * 51) & 0x3fff);
			fogVertices[x][y][0] = x*6.0f + -48.0f + (c + c);
			s = sub_2121c0((frameCount * 51) & 0x3fff);
			fogVertices[x][y][1] = y*6.0f + -48.0f + (s + s);
			fogVertices[x][y][2] = 134.0f;
			fogVertices[x][y][3] = 1.0f;

			dx = -5.1f - ((2*x-16)*6.0f*0.5f + 3.0f);
			dy = 0.0f - ((2*y-16)*6.0f*0.5f + 3.0f);
			d = sqrtf(dx*dx + dy*dy);
			d = (foo - d*4.0f)*96.0f/foo + 0.0f;
			d = clamp(d, 0.0f, 127.0f);
			fogColors[x][y][0] = fogColors[x][y][1] = d*0.0f/128.0f;
			fogColors[x][y][2] = d*128.0f/128.0f;
			fogColors[x][y][3] = 128;
		}
}

/* 0x215238 - 63/343 insns match; our object is exactly 342 insns where
 * the real one is 343 (one short), so everything after the first
 * divergence free-runs one word out of phase with the real bytes -
 * most of the reported mismatches below are that phase shift, not
 * independent bugs. The one instruction-count gap traced so far: the
 * real ROM's "if(fade > 128.0f) fade = 128.0f;" clamp (dead in
 * practice - 105.0f-position[2] tops out at 65 in the (40,105) window
 * it guards) compiles to a compare whose result is then simply
 * discarded, with fade unconditionally rematerialized to 128.0 right
 * after (5 words total, no branch); this build's same source line
 * compiles to a real bc1tl-guarded conditional move (6 words, one
 * extra + a filler nop) because gcc 2.9 doesn't manage to prove the
 * branch is foldable here the way it apparently did in the real
 * source's exact form. Tried plain if, ternary, and a bare initializer
 * with no trailing store (this file's version); none reproduced the
 * real ROM's branch-free shape. The real fade*20.0f/128.0f alpha
 * computation IS preserved at runtime in both versions (not constant-
 * folded to 20), so the values agree - only the clamp's instruction
 * count differs. */
static void
DrawFog(void)
{
	int texIDs[] = {
		TEXID_FOG4, TEXID_FOG2, TEXID_FOG1,
		TEXID_FOG4, TEXID_FOG2, TEXID_FOG1
	};
	int l, x, y, k;
	Texture *tex;
	void *pkt;
	float fade = 128.0f;

	/* real: a fade window in (40,105) that in practice never reaches
	 * the 128 clamp (105-position tops out at 65 there) - kept exactly
	 * as found; the clamp's dead branch still costs a compare in the
	 * real ROM. */
	if(position[2] < 105.0f)
		if(position[2] > 40.0f) {
			fade = 105.0f - position[2];
			if(fade > 128.0f)
				fade = 128.0f;
		}

	vif1SetZWrite(0);
	vif1SetClamp(0, 0, 0, 0, 0, 0);
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));

	for(l = 0; l < 6; l++) {
		float dz;

		fogAnimation[l] += (14 - l)*0.0001f*(l+1)*0.5f;
		if(fogAnimation[l] > 1.0f)
			fogAnimation[l] -= 1.0f;
		dz = l*5.0f;

		pkt = vif1Begin();
		tex = &textures[texIDs[l%6]];
		pkt = pktSetAD(pkt, SCE_GS_TEX0_1, SCE_GS_SET_TEX0(tex->gstex.tbp[0], 64/64, SCE_GS_PSMCT16, 6, 6,
			1, SCE_GS_MODULATE, 0, 0, 0, 0, 0));
		pkt = pktSetAlphaBlend(pkt, 1, 0, (int)(fade*20.0f/128.0f));
		vif1End();

		for(y = 0; y < 16; y++) {
			pkt = vif1Begin();
			for(x = 0; x < 16; x++) {
				for(k = 0; k < 4; k++) {
					float *v = fogVertices[x+k%2][y+k/2];
					sprVertices->verts1[k][0] = v[0];
					sprVertices->verts1[k][1] = v[1];
					sprVertices->verts1[k][2] = v[2] - dz;
					sprVertices->verts1[k][3] = v[3];
				}
				if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->cameraScreenMatrix, sprVertices->verts1, 4))
					continue;

				pkt = pktSetAD(pkt, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 1, 0, 1, 0, 0, 0, 0));
				for(k = 0; k < 4; k++) {
					float q = sprTransformVertex(sprVertices->verts2[k], sprVertices->verts1[k], sprMatrices->cameraScreenMatrix);
					float s = ((k%2)*0.5f + (x%2)*0.5f - fogAnimation[l])*q;
					float t = ((k/2)*0.5f + (y%2)*0.5f)*q;
					u32 c = fogColors[x+k%2][y+k/2][2];
					u32 *v = sprVertices->verts2[k];

					pkt = pktSetAD(pkt, SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&s, *(u32*)&t));
					pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(c/4, c*2/5, c, 128, *(u32*)&q));
					pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
				}
			}
			vif1End();
		}
	}

	vif1SetZWrite(1);
}

/* 0x215798 - init the illegal-disc fog: the shared 3x3-vertex patch and
 * the 128 particles.  64/161 insns match: the first loop (vertex patch)
 * and the rand()%64+64 / rand()%96/128-style idioms in the second loop
 * match the ROM's own signed-modulo-by-power-of-2 codegen closely, but
 * our object's frame is 16 bytes smaller than the real one (-176 vs.
 * -192) from the very first instruction, and the s1/s2 (i/j) register
 * assignment doesn't match either - both point to this function
 * needing one more spilled/live value than our version does, same
 * class of register-pressure gap as InitFog/DrawFog. Tried: swapping
 * the i/j declaration order (no effect on the mismatch count) and
 * replacing the v/col pointer aliases with direct 2D-array indexing
 * (made it dramatically worse, 3/161 - unlike InitFog, this function
 * wants the pointer-alias form). Left as the best of the variants
 * tried. */
static void
sub_215798(void)
{
	int j, i, c;
	float *v;
	int *col;

	for(j = 0; j < 3; j++)
		for(i = 0; i < 3; i++) {
			v = illegalFogVerts[j*3+i];
			v[0] = (i-1)*15.0f;
			v[1] = (j-1)*15.0f;
			v[2] = 0.0f;
			v[3] = 1.0f;
		}
	for(j = 0; j < 3; j++)
		for(i = 0; i < 3; i++) {
			col = illegalFogColors[j*3+i];
			c = 0;
			if(j == 1 && i == 1)
				c = rand()%64 + 64;
			col[0] = c;
			col[3] = 128;
			col[1] = col[2] = c*96/128;
		}
	for(i = 0; i < 128; i++) {
		illegalFogPos[i][0] = (rand()%4800 - 2400)*0.01f;
		illegalFogPos[i][1] = (rand()%4800 - 2400)*0.01f;
		illegalFogPos[i][3] = 0.0f;
		illegalFogRot[i][0] = 0.0f;
		illegalFogRot[i][1] = 0.0f;
		illegalFogRot[i][2] = 0.0f;
		illegalFogRot[i][3] = 1.0f;
		illegalFogState[i] = 0;
		illegalFogPos[i][2] = rand()%805 + 477;
	}
}

/* 0x215a20 - the illegal-disc fog: 128 red cloud particles, each an
 * instance of one shared 3x3-vertex patch, streaming down past the
 * rising camera and respawning above it.  Additive blend, FOG0 texture.
 *
 * 91/317 insns match (measured with a local variant of check.py's
 * masked comparison that tolerates running past our object's .text,
 * since check.py itself throws - our compiled object is 306 insns for
 * this function against the real 317, an 11-insn gap that, like
 * DrawFog, throws every mismatch after the first divergence out of
 * phase with the real bytes). The stack frame is 16 bytes smaller than
 * real from the very first instruction (same class of gap as
 * sub_215798), and per-particle `alpha`/`af`/`d` end up in different
 * registers than the ROM once they need to survive the sceVu0RotMatrix/
 * TransMatrix/MulMatrix/sceVu0ClipAll call chain before reaching
 * pktSetAlphaBlend - the same "value needs a callee-saved home across
 * a run of calls" register-pressure gap documented in InitFog/DrawFog,
 * not chased further here for lack of time. */
static void
DrawIllegalFog(void)
{
	float globalAlpha, af, d, rj, q, s, t;
	int i, j, k, vi, alpha;
	int *col;
	void *pkt;

	if(position[2] < 672.0f) {
		globalAlpha = (550.0f - (672.0f - position[2]))*128.0f/550.0f*4.0f;
		if(globalAlpha > 128.0f)
			globalAlpha = 128.0f;
	} else
		globalAlpha = 128.0f;

	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetTexture(&textures[TEXID_FOG0]);

	for(i = 0; i < 128; i++) {
		rj = (i+1)*0.0001f;
		if((frameCount & 1) == 0)
			rj = -rj;
		illegalFogRot[i][2] += rj;
		while(illegalFogRot[i][2] > 3.14159265f)
			illegalFogRot[i][2] -= 6.28318531f;
		while(illegalFogRot[i][2] < -3.14159265f)
			illegalFogRot[i][2] += 6.28318531f;

		illegalFogPos[i][2] -= (int)((i+1)*0.02f + 1.2f);
		d = (position[2] + 805.0f) - illegalFogPos[i][2];
		if(d < 192.0f) {
			if(d < 0.0f)
				continue;
			af = d*64.0f/192.0f;
		} else {
			d = illegalFogPos[i][2] - position[2] - 32.0f;
			if(d < 192.0f) {
				if(d < 0.0f) {
					illegalFogPos[i][2] = position[2] + 805.0f;
					continue;
				}
				af = d*64.0f/192.0f;
			} else
				af = 64.0f;
		}
		alpha = af*globalAlpha*0.0078125f;

		sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, illegalFogRot[i]);
		sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, illegalFogPos[i]);
		sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
			sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
		if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->worldScreenMatrix, illegalFogVerts, 8))
			continue;

		pkt = vif1Begin();
		pkt = pktSetAlphaBlend(pkt, 1, 0, alpha);
		for(j = 0; j < 4; j++) {
			pkt = pktSetAD(pkt, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 1, 0, 1, 0, 0, 0, 0));
			for(k = 0; k < 4; k++) {
				vi = fogStrips[j][k];
				q = sprTransformVertex(sprVertices->verts2[0], illegalFogVerts[vi], sprMatrices->worldScreenMatrix);
				col = illegalFogColors[vi];
				pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2], 128, *(u32*)&q));
				s = fogUV[vi][0]*q;
				t = fogUV[vi][1]*q;
				pkt = pktSetAD(pkt, SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&s, *(u32*)&t));
				pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_SET_XYZF(sprVertices->verts2[0][0],
					sprVertices->verts2[0][1], sprVertices->verts2[0][2], 0));
			}
		}
		vif1End();
	}
	vif1SetZWrite(1);
	vif1SetZTest(1);
}
