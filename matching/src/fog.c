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
 *  - DrawIllegalFog clamps `af` to [0,64] before the alpha multiply
 *    (`alpha = clamp(af, 0.0f, 64.0f)*globalAlpha*0.0078125f`).  Dead in
 *    practice - af is 64.0f or d*64/192 with 0 <= d < 192 - but the ROM
 *    really does emit the two-sided clamp, and opening.c:2385 does not.
 *  - DrawIllegalFog transforms into a 16-byte LOCAL vector on the stack
 *    (`move a0,sp` at 0x215dbc; the XYZF2 packet reads 0/4/8(sp)), not
 *    sprVertices->verts2[0].  That local is the whole reason its frame
 *    is 208 bytes where a verts2-based version needs only 192.
 *  - DrawFog transforms into sprVertices->verts2[0] (a0 = sprVertices+96,
 *    constant) while the SOURCE walks verts1[k] (sprVertices+32+16k);
 *    opening.c uses verts2[k] for both.  Same values, different scratch
 *    slot.
 *  - DrawFog's fogColors element is a signed `int c`, not u32: the ROM
 *    divides with the signed idioms (addiu 3 / movn / sra 2 for c/4, and
 *    a real signed `div` for c*2/5), and reads verts2 with `lw`, not the
 *    `lwu` an unsigned type produces.  Values are 0..128 so it does not
 *    matter numerically.
 *
 * gp = 0x2AF070 (NOT 0x2AF0B0): the .lit4 entries used here are
 * 5202.0 @0x2A7088, -5.1 @0x2A708C, 0.0001 @0x2A7090 and again
 * @0x2A709C (no dedup), 0.01 @0x2A7094, 550.0 @0x2A7098,
 * 3.14159265 @0x2A70A0, 6.28318531 @0x2A70A4 and again @0x2A70AC,
 * -3.14159265 @0x2A70A8, 0.02 @0x2A70B0, 1.2 @0x2A70B4, 805.0 @0x2A70B8.
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

/* 0x214f78 - 175/176: the only residual is one scheduling slot (the real
 * ROM issues `addiu s5,v1,1` - the outer counter's y+1 - as the 2nd insn
 * of the inner-loop preheader block, right after `sll v0,v1,4`; we issue
 * it 3 slots later, between `sll s4,v1,1` and `addu s0,v0,s7`).  Every
 * other word, register number and delay slot matches.
 *
 * Recovered source shapes:
 *  - the two arrays are reached through LOCAL POINTER VARIABLES holding
 *    their base, and indexed FLAT (`fv[x*17+y]`, not `fogVertices[x][y]`).
 *    This is the single biggest lever in the function.  With direct
 *    2-D array indexing gcc 2.9 builds THREE induction variables for the
 *    inner loop - a raw offset giv (y*16 + x*272) plus the two address
 *    givs - and then leaves half the address uses un-replaced, i.e. it
 *    recomputes `base + offset` inline (12 stray words, and the extra
 *    live pseudo is what pushed the loop multiplier 51 out of a register,
 *    see below).  Handing gcc a pointer base makes the giv's add_val a
 *    register, all uses get replaced, and exactly two walking pointers
 *    (s0 = colors, s1 = vertices, both += 272) survive - the real shape.
 *    A `float *v = fv[x*17+y];` temp does NOT work (a plain-register giv
 *    is not an address giv; on the R5900 the 3-operand `mult` is cheap
 *    enough that gcc leaves `mult v0,x,272` unreduced), nor does simply
 *    redeclaring the arrays 1-D - the pointer indirection is required.
 *  - `fv`/`fc` must be ASSIGNED (not initialized at their declaration)
 *    AFTER `foo = sqrtf(5202.0f)`: initializers materialize the two
 *    lui/addiu pairs in the function prologue, while the real ROM has
 *    them in the outer loop's preheader at 0x214fe8, after the sqrt's
 *    NaN-fallback branch merges.  Getting this right also drops the
 *    register pressure enough that gcc keeps the loop multiplier 51 in a
 *    callee-saved register (`li s6,51`, `mult a0,a0,s6` at both sine-
 *    table call sites) instead of rematerialising `li` per call site,
 *    which was the second open residual of the previous round.  The
 *    resulting preheader - lui, lui, addiu(colors), 6.0, -48.0,
 *    addiu(vertices), 0.5, y=0, 3.0, li 51, 0.0, 1/128 - is word-exact.
 *  - the colour chain is `fc[..][1] = fc[..][0] = d*0.0f/128.0f;` (the
 *    ROM stores 4(s0) then 0(s0); `a = b = X` stores b first, so the
 *    NAMED-FIRST element is [1], the reverse of what the sub_215798
 *    chain suggested - this one was measured, not assumed).
 *  - the four colour stores are emitted [3], [2], [1], [0], i.e. reverse
 *    source order (last-store-first), from source order [0/1], [2], [3].
 * Tried and rejected for the last slot: pre/post-increment, `y += 1`,
 * `y != 17`, `y <= 16`, `17 > y`, `17*x+y`, `x*2-16`, dx/dy statement
 * swap, while/do-while forms, declaring x before y, assigning fc before
 * fv.  All stay at 175/176 or regress; it is a pure list-scheduling
 * tie-break (equal-priority insns ordered by RTL position) of the same
 * class as sub_215798's s3/s4 residual. */
static void
InitFog(void)
{
	int y, x;
	sceVu0FVECTOR *fv;
	sceVu0IVECTOR *fc;
	float foo = sqrtf(5202.0f);

	fv = fogVertices[0];
	fc = fogColors[0];

	for(y = 0; y < 17; y++)
		for(x = 0; x < 17; x++) {
			float dx, dy, d, c, s;

			c = sub_212238((frameCount * 51) & 0x3fff);
			fv[x*17+y][0] = x*6.0f + -48.0f + (c + c);
			s = sub_2121c0((frameCount * 51) & 0x3fff);
			fv[x*17+y][1] = y*6.0f + -48.0f + (s + s);
			fv[x*17+y][2] = 134.0f;
			fv[x*17+y][3] = 1.0f;

			dx = -5.1f - ((2*x-16)*6.0f*0.5f + 3.0f);
			dy = 0.0f - ((2*y-16)*6.0f*0.5f + 3.0f);
			d = sqrtf(dx*dx + dy*dy);
			d = (foo - d*4.0f)*96.0f/foo + 0.0f;
			d = clamp(d, 0.0f, 127.0f);
			fc[x*17+y][1] = fc[x*17+y][0] = d*0.0f/128.0f;
			fc[x*17+y][2] = d*128.0f/128.0f;
			fc[x*17+y][3] = 128;
		}
}

/* 0x215238 - 232/343 (was 196).  Recovered source shapes:
 *  - `fade` is NOT initialised to 128.0f; the plain `fade = 128.0f;`
 *    comes AFTER the whole position window, which is what makes the
 *    window dead and reproduces the ROM's branch-free residue exactly
 *    (c.lt.s with the result discarded at 0x2152f4, then an
 *    unconditional lui/mtc1 128.0 into $f24 at 0x2152f8).  Written as an
 *    initialiser gcc emits a live bc1tl-guarded move instead - this was
 *    the previous round's unexplained one-instruction gap.
 *  - the alpha byte is computed BEFORE vif1Begin() and bound to an int
 *    (`a = fade*20.0f/128.0f;` at 0x2153a8..0x2153d4, jal vif1Begin at
 *    0x2153d8), not passed as an expression to pktSetAlphaBlend.
 *  - there is no `Texture *tex` temp: the ROM writes
 *    textures[texIDs[l%6]].gstex.tbp[0] straight into the TEX0 packet,
 *    which is what makes gcc pick the mtlo/madd address idiom
 *    (0x215424) over mult+addu.
 *  - dz is not a variable: `v[2] - l*5.0f` inline, which gcc hoists to
 *    the y-loop preheader (mul.s $f22,$f23,$f25 at 0x215468) with
 *    (float)l in $f23 and 5.0f in $f25 pinned at the l-loop level.
 *  - the fogColors element is a signed int (see the file header), and
 *    the verts2 words are read with `int *v` so the loads are `lw`.
 *  - unlike InitFog, DrawFog does NOT use pointer bases for
 *    fogVertices/fogColors: the ROM rematerialises lui/addiu for both
 *    inside the loops (0x2154dc, 0x215658).  Adding pointer bases here
 *    costs 10-20 words (measured), so the plain 2-D subscript is right.
 * Residual (~68 differing + 43 inserted/deleted): (a) every callee-saved
 * integer register is off by one slot (the ROM has l in s0 and the
 * fogAnimation base in s1, we have them swapped, and the shift
 * propagates through x/y/k/pkt) - no declaration-order or loop-form
 * lever moved it; (b) we hoist the fogVertices/fogColors symbol
 * addresses out of the k/x loops and fold the [2] element offset into
 * the %lo (base+4656 with a 0 displacement instead of base+4648 with
 * an 8), the same reassociation documented in DrawIllegalFog;
 * (c) the s/t and RGBAQ blocks are scheduled differently, which is
 * probably downstream of (a). */
static void
DrawFog(void)
{
	int texIDs[] = {
		TEXID_FOG4, TEXID_FOG2, TEXID_FOG1,
		TEXID_FOG4, TEXID_FOG2, TEXID_FOG1
	};
	int l, x, y, k, a;
	void *pkt;
	float fade;

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
	fade = 128.0f;

	vif1SetZWrite(0);
	vif1SetClamp(0, 0, 0, 0, 0, 0);
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));

	for(l = 0; l < 6; l++) {
		fogAnimation[l] += (14 - l)*0.0001f*(l+1)*0.5f;
		if(fogAnimation[l] > 1.0f)
			fogAnimation[l] -= 1.0f;

		a = fade*20.0f/128.0f;
		pkt = vif1Begin();
		pkt = pktSetAD(pkt, SCE_GS_TEX0_1, SCE_GS_SET_TEX0(textures[texIDs[l%6]].gstex.tbp[0], 64/64, SCE_GS_PSMCT16, 6, 6,
			1, SCE_GS_MODULATE, 0, 0, 0, 0, 0));
		pkt = pktSetAlphaBlend(pkt, 1, 0, a);
		vif1End();

		for(y = 0; y < 16; y++) {
			pkt = vif1Begin();
			for(x = 0; x < 16; x++) {
				for(k = 0; k < 4; k++) {
					float *v = fogVertices[x+k%2][y+k/2];
					sprVertices->verts1[k][0] = v[0];
					sprVertices->verts1[k][1] = v[1];
					sprVertices->verts1[k][2] = v[2] - l*5.0f;
					sprVertices->verts1[k][3] = v[3];
				}
				if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->cameraScreenMatrix, sprVertices->verts1, 4))
					continue;

				pkt = pktSetAD(pkt, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 1, 0, 1, 0, 0, 0, 0));
				for(k = 0; k < 4; k++) {
					float q = sprTransformVertex(sprVertices->verts2[0], sprVertices->verts1[k], sprMatrices->cameraScreenMatrix);
					float s = ((k%2)*0.5f + (x%2)*0.5f - fogAnimation[l])*q;
					float t = ((k/2)*0.5f + (y%2)*0.5f)*q;
					int c = fogColors[x+k%2][y+k/2][2];
					int *v = sprVertices->verts2[0];

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
 * the 128 particles.
 *
 * 2026-08-29, with the resync differ: 149/161.  Real source shapes
 * recovered from the ROM's codegen:
 *  - the center-cell guard is `if(j == 1 && i == j)` (the ROM compares
 *    i against the REGISTER holding j, not against the constant 1);
 *  - `col[2] = col[1] = c*96/128;` comes BEFORE `col[3] = 128;` (the
 *    hoisted 96 gets its callee-saved reg before the 128, and the
 *    chain stores 8(s0) then 4(s0));
 *  - `pos[2] = rand()%805 + 477` is the THIRD particle statement, so
 *    the pos[3]/rot/state stores schedule into the div-805's shadow;
 *  - rot is cleared with a CHAIN (w[0]=w[1]=w[2]=0.0f -> one hoisted
 *    0.0f in $f20) while pos[3]=0.0f standalone gets `sw zero`;
 *  - the particle loop is an UPWARD `for(k = 0; k < 128; k++)` over
 *    two walking pointers (w += 4; v += 4; in that order) plus
 *    illegalFogState indexed [k] - gcc's loop reversal turns exactly
 *    this into the ROM's li 127/addiu -1/bgez countdown AND puts k in
 *    s3, 805 in s4 (found by the campaign search after a hand-written
 *    countdown had gotten the shape right but the registers swapped;
 *    reusing one alias for pos AND rot still blocks strength
 *    reduction entirely);
 *  - the pointer inits must go w-then-v to put v in s1/w in s2.
 * Residual 5 words (156/161): one bnez/bnezl annulment at 0x215850
 * and a sched1 tie on the preheader lui temps (ROM issues three
 * back-to-back into a0/v1/v0, we reuse one) - ~9k campaign variants
 * never moved it. */
static void
sub_215798(void)
{
	int i, j, c, k;
	float *v, *w;
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
			if(j == 1 && i == j)
				c = rand()%64 + 64;
			col[0] = c;
			col[2] = col[1] = c*96/128;
			col[3] = 128;
		}
	w = (float*)illegalFogRot;
	v = (float*)illegalFogPos;
	for(k = 0; k < 128; k++) {
		v[0] = (rand()%4800 - 2400)*0.01f;
		v[1] = (rand()%4800 - 2400)*0.01f;
		v[2] = rand()%805 + 477;
		v[3] = 0.0f;
		w[0] = w[1] = w[2] = 0.0f;
		w[3] = 1.0f;
		illegalFogState[k] = 0;
		w += 4;
		v += 4;
	}
}

/* 0x215a20 - the illegal-disc fog: 128 red cloud particles, each an
 * instance of one shared 3x3-vertex patch, streaming down past the
 * rising camera and respawning above it.  Additive blend, FOG0 texture.
 *
 * 227/317 (was 172).  Recovered source shapes:
 *  - the transform destination is a LOCAL sceVu0IVECTOR, not
 *    sprVertices->verts2[0]: the ROM passes `sp` to sprTransformVertex
 *    (0x215dbc) and builds the XYZF2 packet from 0/4/8(sp)
 *    (0x215e70..0x215e80).  Adding that local is what makes our frame
 *    208 bytes and lines the whole prologue/epilogue up word for word -
 *    the previously suspected "one more spilled float slot" was in fact
 *    this 16-byte local at the bottom of the frame.
 *  - `af` is CLAMPED to [0,64] before the alpha multiply (0x215c90 ff:
 *    af<0 -> 0.0, 64<af -> 64.0, else af).  Dead by construction, but
 *    the ROM emits it and it is missing from opening.c.
 *  - `af = 64.0f;` is an INITIALISER before the whole distance
 *    if/else, not the final else branch: the ROM materialises 64.0 into
 *    $f20 at 0x215bfc and copies it to $f6 (0x215c0c) for the two
 *    d*64.0f/192.0f multiplies, so $f20 already holds the fall-through
 *    value.  Writing it as the else branch costs ~20 words.
 *  - globalAlpha is two statements (`= ...*128.0f/550.0f;` then
 *    `= globalAlpha*4.0f;`), which is why the ROM divides straight into
 *    the callee-saved $f21 and then multiplies it in place
 *    (div.s $f21,$f0,$f1 / mul.s $f21,$f21,$f2 at 0x215aa0).
 *  - the k loop is a countdown (`for(k = 3; k >= 0; k--)`, bgez at
 *    0x215e98) over an explicit `int *strip = fogStrips[j];` walking
 *    pointer (`strip++` after the ST computation, 0x215e34), and the
 *    strip element is re-dereferenced at each of its three use sites -
 *    the ROM reloads `lw v1,0(s1)` three times (0x215db0, 0x215dd8,
 *    0x215e28), so there is no `vi` temp in the original.
 * Residual (~70 differing + 20/17 inserted/deleted):
 *  - THE +8/+0 LANE IS NOT A SEMANTIC DIFFERENCE.  Both sides address
 *    element [2].  gcc folds `illegalFogRot[i][2]` into
 *    (symbol+8) + i*16 and emits `lwc1 $f0,0(v0)`; the ROM keeps
 *    symbol + i*16 and emits `lwc1 $f0,8(v0)`.  Same address, same
 *    value - a plus_constant reassociation, worth 6 words.  Pointer
 *    bases, (*(a+i))[2], *(a[i]+2), *(&a[i][0]+2) and a named-member
 *    struct all fail to reproduce the ROM's form (pointer bases fix the
 *    displacement but then keep the base in a callee-saved register
 *    instead of rematerialising lui/addiu per block, which costs more
 *    than it saves: 213 vs 227).
 *  - $f22 holds 0.0078125f here where the ROM holds 0.0f (the ROM
 *    rematerialises 1/128 at 0x215cbc and keeps the comparison zero
 *    pinned); a loop-invariant-motion tie, ~5 words.
 *  - illegalFogColors is reached as %hi in s8 + an in-loop addiu where
 *    the ROM keeps the full address in s7, and s7/s8 are swapped
 *    against fogUV+4. */
static void
DrawIllegalFog(void)
{
	float globalAlpha, af, d, rj, q, s, t;
	int i, j, k, alpha;
	int *col;
	void *pkt;
	sceVu0IVECTOR vert;

	if(position[2] < 672.0f) {
		globalAlpha = (550.0f - (672.0f - position[2]))*128.0f/550.0f;
		globalAlpha = globalAlpha*4.0f;
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

		af = 64.0f;
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
			}
		}
		alpha = clamp(af, 0.0f, 64.0f)*globalAlpha*0.0078125f;

		sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, illegalFogRot[i]);
		sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, illegalFogPos[i]);
		sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
			sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
		if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->worldScreenMatrix, illegalFogVerts, 8))
			continue;

		pkt = vif1Begin();
		pkt = pktSetAlphaBlend(pkt, 1, 0, alpha);
		for(j = 0; j < 4; j++) {
			int *strip = fogStrips[j];

			pkt = pktSetAD(pkt, SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 1, 0, 1, 0, 0, 0, 0));
			for(k = 3; k >= 0; k--) {
				q = sprTransformVertex(vert, illegalFogVerts[*strip], sprMatrices->worldScreenMatrix);
				col = illegalFogColors[*strip];
				pkt = pktSetAD(pkt, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2], 128, *(u32*)&q));
				s = fogUV[*strip][0]*q;
				t = fogUV[*strip][1]*q;
				strip++;
				pkt = pktSetAD(pkt, SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&s, *(u32*)&t));
				pkt = pktSetAD(pkt, SCE_GS_XYZF2, SCE_GS_SET_XYZF(vert[0], vert[1], vert[2], 0));
			}
		}
		vif1End();
	}
	vif1SetZWrite(1);
	vif1SetZTest(1);
}
