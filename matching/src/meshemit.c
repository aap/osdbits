/* Module U mesh/glass draw family - 0x22C920 (the TEXCBUMP emboss emit).
 *
 * Best candidate so far: 39/82 aligned, all MECHANISMS reproduced
 * (see notes.md - the remaining delta is the register-identity /
 * schedule tie class, not structure).
 *
 * THE KEY DISCOVERY OF THIS CLUSTER: the emit family is written with
 * GNU NESTED FUNCTIONS.
 *
 *  - 0x22C888 and 0x22CCE8 are little parent wrappers that home their
 *    args to sp+0.., compute the fresnel term, and call their nested
 *    child with `move v0,sp' - $2 is gcc 2.9's MIPS STATIC CHAIN
 *    register.  0x22C4E0 and 0x22CB58 are those children: each starts
 *    `move s1,v0; sw v0,0(sp)' and reads the parent's locals off the
 *    chain (0(s1)=scene, 4(s1)=face for 0x22CB58).
 *  - 0x22C920 / 0x22CA68 / 0x22CD78 contain an *`inline'* nested child
 *    that got integrated: no separate body remains, but the parent
 *    still homes every child-referenced arg to sp+0.. and the loop
 *    reads them back through a COPY of sp (the materialized chain
 *    pointer) every iteration.  Only inline nested children produce
 *    this signature; a plain loop keeps args in registers (probes
 *    nest1.c/nest2.c/nest3.c).
 *  - Home-slot order = order the children FIRST REFERENCE the parent
 *    vars, which for multiple children is child-definition order.
 *    0x22C920's ox@0 oy@4 col@8 f@12 comes out of three children
 *    defined in the order mkst (ox,oy), mkrgba (col), emit (f).
 *
 * Compile: ee-gcc 2.9-ee-991111 -O2 -Wall -fno-common
 * -fno-strict-aliasing (the -fno-strict-aliasing is LOAD-BEARING: it
 * is what makes the homed args reload every loop iteration).
 */

typedef unsigned int u32;
typedef long s64;			/* 64-bit on ee-gcc */

/* The per-face bank record - 0x160 bytes, bank at 0x3529D0 (rods) /
 * cube bank; verified against 0x22CFA8's writes:
 *   vert stride 0x50: cam@0, u@0x10, v@0x14, proj@0x20, fix4@0x30, q@0x40
 *   normal@0x140 (written by sceVu0ApplyMatrix at face+320)
 *   cull@0x150 (sw v0,336(v1))
 */
typedef struct MeshVert {
	float cam[4];			/* 0x00 camera-space position */
	float u, v;			/* 0x10 uv (v scaled by scene sy) */
	float pad18, pad1c;
	float proj[4];			/* 0x20 projected, already *q */
	int fix[4];			/* 0x30 sceVu0FTOI4 of proj */
	float q;			/* 0x40 1/w */
	float pad44, pad48, pad4c;
} MeshVert;				/* 0x50 */

typedef struct MeshFace {
	MeshVert v[4];			/* 0x000 */
	float normal[4];		/* 0x140 camera-space normal */
	int cull;			/* 0x150 screen-winding flag */
	int pad154, pad158, pad15c;
} MeshFace;				/* 0x160 */

/* The packet context at 0x3529B0 - same layout as menudraw.c's Pkt
 * {cur, base, tag, vifcode, unused, giftag}.  0x22C920 only touches
 * pk.cur (+0), so an incomplete extern array of u32* serves; the
 * siblings need the full struct + pktOpen/pktKick. */
extern u32 *mpk[];			/* 0x3529b0; [0] = cur, [5] = giftag */

/* real: 0x22C920 - the TEXCBUMP emboss emit.  REGLIST payload writes:
 * PRIM=84 u64, CLAMP=0x1000000 u64 (header, giftag template 0x27F8B0
 * already queued by the caller), then per vertex ST / RGBAQ / XYZ as
 * three 64-bit values split into sw pairs because pk.cur is a u32*.
 * 39/82 aligned - register/schedule tie remains, structure exact. */
void
MeshEmitBump(MeshFace *f, int *col, float ox, float oy)
{
	inline s64 mkst(MeshVert *vp)
	{
		float s, t;
		u32 *ps = (u32 *)&s;	/* forces the swc1 36/40(sp) + lwu */
		u32 *pt = (u32 *)&t;	/* round-trip the ROM has */

		s = (vp->u + ox) * vp->q;
		t = (vp->v + oy) * vp->q;
		return *ps | (s64)*pt << 32;
	}
	inline s64 mkrgba(MeshVert *vp)
	{
		return col[0] | (s64)col[1]<<8 | (s64)col[2]<<16 |
			(s64)col[3]<<24 | (s64)*(u32*)&vp->q << 32;
	}
	inline void emit(void)
	{
		u32 *p;
		MeshVert *vp;
		s64 v;
		int k;

		for(k = 0; k < 4; k++) {
			vp = &f->v[k];
			v = mkst(vp);
			p = mpk[0];
			*p++ = v;
			p[0] = v >> 32;
			v = mkrgba(vp);
			p[1] = v;
			p[2] = v >> 32;
			v = vp->fix[0] | (s64)vp->fix[1]<<16 |
				(s64)vp->fix[2]<<32;
			p[3] = v;
			p[4] = v >> 32;
			mpk[0] = p + 5;
		}
	}
	u32 *p;

	p = mpk[0];
	*p++ = 84;
	p[0] = 0;
	p[1] = 0x1000000;
	p[2] = 0;
	mpk[0] = p + 3;
	emit();
}
