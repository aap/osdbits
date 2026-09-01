/* Reconstruction of the PS2 OSDSYS boot ("towers") opening animation,
 * reverse-engineered from a retail OSDSYS image.  Hex addresses in the
 * comments refer to that image; sub_XXXXXX names are real functions not
 * traced yet. */

#include <stdio.h>
#include "inc.h"
#include "res.h"

void
dumpMat(float *m)
{
	printf("%0.3f %0.3f %0.3f %0.3f\n", m[0], m[4], m[8], m[12]);
	printf("%0.3f %0.3f %0.3f %0.3f\n", m[1], m[5], m[9], m[13]);
	printf("%0.3f %0.3f %0.3f %0.3f\n", m[2], m[6], m[10], m[14]);
	printf("%0.3f %0.3f %0.3f %0.3f\n", m[3], m[7], m[11], m[15]);
}


/* GSTex/Texture now live in inc.h - menu.c uploads its own textures
 * through the same InitTexture()/vif1SetTexture() path. */

u32 clut[16] = {
	0x00000000,
	0x09111111,
	0x11222222,
	0x1A333333,
	0x22444444,
	0x2B555555,
	0x33666666,
	0x3C777777,
	0x44888888,
	0x4D999999,
	0x55AAAAAA,
	0x5EBBBBBB,
	0x66CCCCCC,
	0x6FDDDDDD,
	0x77EEEEEE,
	0x80FFFFFF
};

enum {
	TEXID_SCE,
	TEXID_FOG0,
	TEXID_FOG1,
	TEXID_FOG2,
	TEXID_FOG3,
	TEXID_FOG4,
	TEXID_WAL0,
	TEXID_CRLE,
	TEXID_CRBL,
	TEXID_FLAR,
	TEXID_REF,
	TEXID_BLP,
	TEXID_BLPR,
	TEXID_PNG,
};

Texture textures[] = {
	{ nil, RESID_TEXOSCE,	0,    1, { 0, 0, 256,  64 }, 0,  0, 5, 0, { 0 } },
	{ nil, RESID_TEXOFOG0,	0,    2, { 0, 0, 128, 128 }, 0, 20, SCE_GS_PSMCT16, 0, { 0 } },
	{ nil, RESID_TEXOFOG1,	0,    1, { 0, 0,  64,  64 }, 0, 20, SCE_GS_PSMCT16, 0, { 0 } },
	{ nil, RESID_TEXOFOG2,	0,    1, { 0, 0,  64,  64 }, 0, 20, SCE_GS_PSMCT16, 0, { 0 } },
	{ nil, RESID_TEXOFOG3,	0,    1, { 0, 0,  64,  64 }, 0, 20, SCE_GS_PSMCT16, 0, { 0 } },
	{ nil, RESID_TEXOFOG4,	0,    1, { 0, 0,  64,  64 }, 0, 20, SCE_GS_PSMCT16, 0, { 0 } },
	{ nil, RESID_TEXOWAL0,	0,    1, { 0, 0, 256, 256 }, 2, 20, SCE_GS_PSMCT16, 0, { 0 } },
	{ nil, RESID_TEXOCRLE,	0,    1, { 0, 0,  64,  64 }, 0,  0, SCE_GS_PSMCT32, 0, { 0 } },
	{ nil, RESID_TEXOCRBL,	0,    1, { 0, 0,  64,  64 }, 0,  0, SCE_GS_PSMCT32, 0, { 0 } },
	{ nil, RESID_TEXOFLAR,	0,    2, { 0, 0, 128, 128 }, 0, 20, SCE_GS_PSMCT16, 0, { 0 } },
	{ nil, RESID_TEXOREF,	0,    0, { 0, 0, 128, 128 }, 0, 20, SCE_GS_PSMCT16, 0, { 0 } },
	{ nil, RESID_TEXOBLP,	0,    0, { 0, 0,  64,  64 }, 0,  0, 4, 0, { 0 } },
	{ nil, RESID_TEXOBLPR,	0,    0, { 0, 0,  64,  64 }, 0,  0, 4, 0, { 0 } },
	{ nil, RESID_TEXOPNGJ,	clut, 3, { 0, 0, 512, 128 }, 0,  0,  SCE_GS_PSMT4, 0, { 0 } },
	{ nil, RESID_TEXOPNGE,	clut, 3, { 0, 0, 512, 128 }, 0,  0,  SCE_GS_PSMT4, 0, { 0 } },
	{ nil, RESID_TEXOPNGF,	clut, 3, { 0, 0, 512, 128 }, 0,  0,  SCE_GS_PSMT4, 0, { 0 } },
	{ nil, RESID_TEXOPNGS,	clut, 3, { 0, 0, 512, 128 }, 0,  0,  SCE_GS_PSMT4, 0, { 0 } },
	{ nil, RESID_TEXOPNGG,	clut, 3, { 0, 0, 512, 128 }, 0,  0,  SCE_GS_PSMT4, 0, { 0 } },
	{ nil, RESID_TEXOPNGI,	clut, 3, { 0, 0, 512, 128 }, 0,  0,  SCE_GS_PSMT4, 0, { 0 } },
	{ nil, RESID_TEXOPNGD,	clut, 3, { 0, 0, 512, 128 }, 0,  0,  SCE_GS_PSMT4, 0, { 0 } },
	{ nil, RESID_TEXOPNGP,	clut, 3, { 0, 0, 512, 128 }, 0,  0,  SCE_GS_PSMT4, 0, { 0 } },
};
int numTextures = 21;

sceDmaChan *chVIF1 = nil;
sceDmaChan *chGIF = nil;
sceDmaChan *chFromSPR = nil;

static void
InitDMA(void)
{
	sceDmaReset(1);
	chVIF1 = sceDmaGetChan(1);
	chVIF1->chcr.TTE = 1;
	chGIF = sceDmaGetChan(2);
	chGIF->chcr.TTE = 1;
	chFromSPR = sceDmaGetChan(8);
	chFromSPR->chcr.TTE = 1;
}

typedef struct Matrices Matrices;
struct Matrices
{
	sceVu0FMATRIX unit;
	sceVu0FMATRIX worldScreenMatrix;
	sceVu0FMATRIX worldMatrix;
	sceVu0FMATRIX cameraScreenMatrix;
	sceVu0FMATRIX cameraMatrix;
	sceVu0FMATRIX viewScreenMatrix;
	sceVu0FMATRIX m6;
	sceVu0FMATRIX m7;
	sceVu0FMATRIX normalLightMatrix;
	sceVu0FMATRIX m9;
};

typedef struct Vertices Vertices;
struct Vertices
{
	sceVu0FVECTOR xyz;
	sceVu0FVECTOR stq;	// or only q?
	sceVu0FVECTOR verts1[4];
	sceVu0IVECTOR verts2[4];
	sceVu0FVECTOR unk[26];
};

// TODO:
u32 *sprData1;
u32 *sprData2;
Matrices *sprMatrices;
Vertices *sprVertices;

u8 *sprBasePtr;
u8 *sprAllocPtr;
u32 sprNumAllocs;
u32 sprAllocSizes[32];

u32 *sprChains[2];
u32 *sprCurrentChain;
int sprChainBit;

void
sprInitAlloc(void)
{
	int n;
	sprAllocPtr = sprBasePtr;
	sprNumAllocs = 0;
	n = 32;
	while(n--)
		sprAllocSizes[n] = 0;
}

i32
sprGetFreeSize(void)
{
	return (u8*)0x70004000 - sprAllocPtr;
}

void
sprInitChains(void)
{
	sprChains[0] = nil;
	sprChains[1] = nil;
	sprCurrentChain = nil;
}

void
sprInit(void)
{
	sprBasePtr = (u8*)0x70000000;
	sprInitAlloc();
	sprInitChains();
}

void*
sprAlloc(u32 size)
{
	void *ret;
	if(sprAllocPtr > (u8*)0x70004000)
		for(;;);
	if(sprNumAllocs >= 32)
		for(;;);
	sprAllocSizes[sprNumAllocs++] = size;
	ret = sprAllocPtr;
	sprAllocPtr += size;
	return ret;
}

void
sprSetBasePtr(void)
{
	sprNumAllocs = 0;
	sprBasePtr = sprAllocPtr;
}

void
sprAllocChains(void)
{
	// this is dumb. no qword alignment
	i32 sz = sprGetFreeSize()/2;
	sprChains[0] = sprAlloc(sz);
	sprChains[1] = sprAlloc(sz);
	sprCurrentChain = sprChains[0];
	sprChainBit = 0;
	// TODO: unknown variable
}

u32*
sprGetChainBuffer(void)
{
	sprChainBit = !sprChainBit;
	sprCurrentChain = sprChains[sprChainBit];
	return sprCurrentChain;
}

static float
sprTransformVertex(sceVu0IVECTOR dst, sceVu0FVECTOR src, sceVu0FMATRIX mat)
{
	float q;
	sceVu0ApplyMatrix(sprVertices->xyz, mat, src);
	q = 1.0f/sprVertices->xyz[3];
	sprVertices->xyz[0] *= q;
	sprVertices->xyz[1] *= q;
	sprVertices->xyz[2] *= q;
	sprVertices->xyz[3] = 1.0f;
	sprVertices->stq[2] = q;
	sceVu0FTOI4Vector(dst, sprVertices->xyz);
	dst[2] >>= 4;
	return q;
}

static void
InitSPR(void)
{
	sprInit();
	sprData1 = sprAlloc(0x30);
	sprData2 = sprAlloc(0x30);
	sprMatrices = sprAlloc(sizeof(Matrices));
	sprVertices = sprAlloc(sizeof(Vertices));
	sprSetBasePtr();
	sceVu0UnitMatrix(sprMatrices->unit);
}

u32 gsAllocPtr = 0x4000;
i32 gsLastPSM = -1;

static u32
psmToBppGS(u32 psm)
{
	switch(psm) {
	case SCE_GS_PSMCT32:
	case SCE_GS_PSMCT24:
	case 3:
	case 4:
	case 5:
	case SCE_GS_PSMT8H:
	case SCE_GS_PSMT4HL:
	case SCE_GS_PSMT4HH:
		return 32;
	case SCE_GS_PSMCT16:
		return 16;
	case SCE_GS_PSMT8:
		return 8;
	case SCE_GS_PSMT4:
		return 4;
	default:
		return 0;
	}
}

static int
psmToBppEE(u32 psm)
{
	switch(psm) {
	case SCE_GS_PSMCT32:
		return 32;
	case SCE_GS_PSMCT24:
		return 24;
	case SCE_GS_PSMCT16:
	case 5:
		return 16;
	case 3:
	case 4:
	case SCE_GS_PSMT8H:
	case SCE_GS_PSMT8:
		return 8;
	case SCE_GS_PSMT4:
	case SCE_GS_PSMT4HL:
	case SCE_GS_PSMT4HH:
		return 4;
	default:
		return 0;
	}
}

static void
gsInitAlloc(void)
{
	/* start past draw0, draw1 AND the Z buffer - sceGsSetDefDBuff places
	 * Z (PSMZ32, same size as one colour buffer) right after the two
	 * colour buffers */
	gsAllocPtr = 3 * (screenW*screenH)/64;
	gsLastPSM = -1;
}

static u32
gsAllocBuffer(u32 psm, Rect *r)
{
	u32 d, ret;

	ret = gsAllocPtr;
	d = psmToBppGS(psm);
	gsLastPSM = psm;
	gsAllocPtr += (((r->w+0x3F)&~0x3F)*r->h*d)/(8*4)/64;
	return ret;
}

u32 extraBuf1, extraBuf2;

/* once-per-frame snapshots of evenOddFrame/evenOddField, captured at the
 * top of ProcessOpening().  SwapBuffers() flips the globals on the swap thread,
 * only loosely synchronized with the render thread, so a mid-frame read
 * can already see the NEXT frame's values; all buffer-identity decisions
 * use these snapshots instead.  (Harness-specific, not in the real ROM.) */
static int stableEvenOddFrame;
static int stableEvenOddField;

static void
gsAllocExtraBuffers(void)
{
	Rect r;

	r.x = r.y = 0;
	r.w = screenW;
	r.h = screenH;
	gsAllocPtr += 10*32;	/* page guard (Z buffer rounds into us) */
	extraBuf1 = gsAllocBuffer(SCE_GS_PSMCT32, &r);
	gsAllocPtr += 10*32;
	extraBuf2 = gsAllocBuffer(SCE_GS_PSMCT32, &r);
	/* guard gaps, NOT in the real ROM (which packs the buffers edge to
	 * edge - fine on real hardware): PCSX2's HW renderer tracks render
	 * targets page-ROUNDED (640x224 handled as 640x256 = 80 pages, not
	 * 70), so adjacent targets bleed into each other's pages and
	 * per-frame RT activity dirties texture pages in its cache.  The
	 * gaps keep all rounded target ranges apart. */
	gsAllocPtr += 10*32;
}

sceVif1Packet vifPackets[2];
sceVif1Packet *vifCurrentPacket;
int vifPacketBit;

float screenAX = 0.0f;
float screenAY = 0.0f;

static void
InitDoubleBuffer(void)
{
	screenAX = 1.0f;
	// How did they come up with this?
	if(IsPAL())
		screenAY = 0.52627105f;
	else
		screenAY = 0.457627f;
	// TODO: some more stuff
	sprInitAlloc();
	sprAllocChains();
	vifPacketBit = 0;
}

static struct BlendMode {
	u32 a, b, c, d;
} BlendModes[] = {
	{ 0, 2, 2, 1 },
	{ 2, 0, 2, 1 },
	{ 0, 1, 2, 1 },
	{ 1, 2, 2, 0 },
	{ 0, 1, 0, 1 },
	{ 0, 2, 0, 1 },
	{ 2, 0, 0, 1 },
	{ 0, 1, 0, 1 },
	{ 0, 2, 1, 1 },
	{ 2, 0, 1, 1 },
	{ 0, 1, 1, 1 }
};
void pktSetAD(u32 a, u64 d) { sceVif1PkAddGsAD(vifCurrentPacket, a, d); }
void pktSetTEST_1(u32 ate, u32 atst, u32 aref, u32 afail, u32 date, u32 datm, u32 zte, u32 ztst)
{
	pktSetAD(SCE_GS_TEST_1, SCE_GS_SET_TEST(ate, atst, aref, afail, date, datm, zte, ztst));
}
void pktSetCLAMP_1(u32 wms, u32 wmt, u32 minu, u32 maxu, u32 minv, u32 maxv)
{
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(wms, wmt, minu, maxu, minv, maxv));
}
void pktSetSCISSOR_1(u32 scax0, u32 scax1, u32 scay0, u32 scay1)
{
	pktSetAD(SCE_GS_SCISSOR_1, SCE_GS_SET_SCISSOR(scax0, scax1, scay0, scay1));
}
void pktSetAlphaBlend(u32 type, u32 mode, u32 fix)
{
	struct BlendMode *bm = &BlendModes[mode];
	pktSetAD(SCE_GS_PABE, !type);
	pktSetAD(SCE_GS_ALPHA_1, SCE_GS_SET_ALPHA(bm->a, bm->b, bm->c, bm->d, fix));
}
void pktSetFlatRect(Rect *r, Color *col, u32 abe, u32 z)
{
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 0, 0, abe, 0, 0, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col->r, col->g, col->b, col->a, 0x3f800000));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ((r->x+2048-screenW/2)<<4, (r->y+2048-screenH/2)<<4, z));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(((r->x+r->w+2048-screenW/2)<<4)-1, ((r->y+r->h+2048-screenH/2)<<4)-1, z));
}
void pktSetTexRect(Rect *r, Rect *tr, Color *col, u32 abe, u32 z)
{
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, abe, 0, 1, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col->r, col->g, col->b, col->a, 0x3f800000));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV((tr->x<<4)+8, (tr->y<<4)+8));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ((r->x+2048-screenW/2)<<4, (r->y+2048-screenH/2)<<4, z));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(((tr->x+tr->w)<<4)+8, ((tr->y+tr->h)<<4)+8));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(((r->x+r->w+2048-screenW/2)<<4)-1, ((r->y+r->h+2048-screenH/2)<<4)-1, z));
}


void
vif1Begin(void)
{
	const u64 giftag[2] = { SCE_GIF_SET_TAG(0, 1, 0, 0, 0, 1), 0xe };
	vifCurrentPacket = &vifPackets[vifPacketBit];
	vifPacketBit ^= 1;
	sceVif1PkInit(vifCurrentPacket, (void*)sprGetChainBuffer());
	sceVif1PkReset(vifCurrentPacket);
	sceVif1PkCnt(vifCurrentPacket, 0);
	sceVif1PkOpenDirectCode(vifCurrentPacket, 0);
	sceVif1PkOpenGifTag(vifCurrentPacket, *(u128*)&giftag);
}

void
vif1Pad(sceVif1Packet *pk)
{
	// this makes absolutely no sense
	int i, n;

	n = ((u128*)pk->pCurrent-1) - pk->pBase;
	n %= 4;
	for(i = 0; i < n; i++) {
		sceVif1PkAddCode(pk, 0);
		sceVif1PkAddCode(pk, 0);
		sceVif1PkAddCode(pk, 0);
		sceVif1PkAddCode(pk, 0);
	}
}

void
vif1End(void)
{
	sceVif1PkCloseGifTag(vifCurrentPacket);
	sceVif1PkCloseDirectCode(vifCurrentPacket);
	vif1Pad(vifCurrentPacket);
	sceVif1PkEnd(vifCurrentPacket, 0);
	sceVif1PkTerminate(vifCurrentPacket);
	sceDmaSync(chVIF1, 0, 0);
	sceDmaSend(chVIF1, DMASPR(vifCurrentPacket->pBase));
}

void vif1SetAD(u32 a, u64 d)
{ vif1Begin(); pktSetAD(a, d); vif1End(); }
void vif1SetTEST_1(u32 ate, u32 atst, u32 aref, u32 afail, u32 date, u32 datm, u32 zte, u32 ztst)
{ vif1Begin(); pktSetTEST_1(ate, atst, aref, afail, date, datm, zte, ztst); vif1End(); }
void vif1SetCLAMP_1(u32 wms, u32 wmt, u32 minu, u32 maxu, u32 minv, u32 maxv)
{ vif1Begin(); pktSetCLAMP_1(wms, wmt, minu, maxu, minv, maxv); vif1End(); }
void vif1SetSCISSOR_1(u32 scax0, u32 scax1, u32 scay0, u32 scay1)
{ vif1Begin(); pktSetSCISSOR_1(scax0, scax1, scay0, scay1); vif1End(); }
void vif1SetAlphaBlend(u32 type, u32 mode, u32 fix)
{ vif1Begin(); pktSetAlphaBlend(type, mode, fix); vif1End(); }
void vif1SetFlatRect(Rect *r, Color *col, u32 abe, u32 z)
{ vif1Begin(); pktSetFlatRect(r, col, abe, z); vif1End(); }
void vif1SetTexRect(Rect *r, Rect *tr, Color *col, u32 abe, u32 z)
{ vif1Begin(); pktSetTexRect(r, tr, col, abe, z); vif1End(); }

void
vif1SetFramebuffer(u32 fbp, u16 psm, int width, int height, int clear)
{
	vif1Begin();
	pktSetAD(SCE_GS_FRAME_1, SCE_GS_SET_FRAME(fbp, width/64, psm, 0));
	/* NB: SCE_GS_SET_SCISSOR args are (ax0, ax1, ay0, ay1), NOT
	 * (x0,y0,x1,y1) - getting this wrong encodes an empty region and
	 * scissors away every pixel until the next PutDrawEnv */
	pktSetAD(SCE_GS_SCISSOR_1, SCE_GS_SET_SCISSOR(0, width-1, 0, height-1));
	if(clear == 1) {
		pktSetAD(SCE_GS_TEST_1, SCE_GS_SET_TEST(1, SCE_GS_ALPHA_ALWAYS, 0, 0, 0, 0, 0, 0));
		pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 0, 0, 0, 0, 0, 0, 0));
		pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0, 0, 0, 0, 0x3f800000));
		// BUG? ignoring XYOFFSET here
		pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZ(0, 0, 0));
		pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZ(screenW<<4, screenH<<4, 0));
		pktSetAD(SCE_GS_TEST_1, SCE_GS_SET_TEST(1, SCE_GS_ALPHA_LESS, 0, 0, 0, 0, 0, 0));
	}
	vif1End();
}

void
vif1SetZTest(int enb)
{
	if(enb)
		vif1SetTEST_1(0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_GEQUAL);
	else
		vif1SetTEST_1(0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_ALWAYS);
}

/* The psm is **PSMZ32**, not PSMZ24: main.c's sceGsSetDefDBuff allocates the
 * Z buffer as PSMZ32 and every ZBUF write in a retail GS dump carries psm = 0
 * throughout the menu.  It used to matter nowhere (every z in this port fitted
 * in 24 bits, and PSMZ24/PSMZ32 share the addressing), but the cube stage now
 * emits retail's 0xFFFFF010 - PSMZ24 would silently mask that to 0xFFF010 on
 * both the write and the compare. */
void
vif1SetZWrite(int enb)
{
	if(enb)
		vif1SetAD(SCE_GS_ZBUF_1, SCE_GS_SET_ZBUF(((screenW*screenH)/64)*2/32, SCE_GS_PSMZ32, 0));
	else
		vif1SetAD(SCE_GS_ZBUF_1, SCE_GS_SET_ZBUF(((screenW*screenH)/64)*2/32, SCE_GS_PSMZ32, 1));
}

void
vif1SetXYOffset(int field, int halfpx)
{
	u32 ofx, ofy;
	ofx = (2048-screenW/2)<<4;
	ofy = (2048-screenH/2)<<4;
	if(halfpx && field)
		ofy += 8;
	vif1SetAD(SCE_GS_XYOFFSET_1, SCE_GS_SET_XYOFFSET(ofx, ofy));
}

static int
GetTexExponent(int sz)
{
	int exp;
	for(exp = 0; 1<<exp < sz; exp++);
	return exp;
}

void
vif1SetTextureMIP(Texture *tex, int mipmap, u32 mmin, u32 k)
{
	u32 tw, th;
	GSTex *t = &tex->gstex;

	tw = GetTexExponent(t->dim[0].w);
	th = GetTexExponent(t->dim[0].h);
	if(mipmap) {
		vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, t->mxl, SCE_GS_LINEAR, mmin, 0, 0, k));
		if(t->mxl > 0)
			vif1SetAD(SCE_GS_MIPTBP1_1, SCE_GS_SET_MIPTBP1(t->tbp[1], t->dim[1].w/64,
				t->tbp[2], t->dim[2].w/64, t->tbp[3], t->dim[3].w/64));
		if(t->mxl > 3)
			vif1SetAD(SCE_GS_MIPTBP2_1, SCE_GS_SET_MIPTBP2(t->tbp[4], t->dim[4].w/64,
				t->tbp[5], t->dim[5].w/64, t->tbp[6], t->dim[6].w/64));
	} else {
		vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	}
	vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(t->tbp[0], t->dim[0].w/64, t->psm, tw, th,
		1, SCE_GS_MODULATE, t->cbp, SCE_GS_PSMCT32, 0, 0, 1));
}

void
vif1SetTexture(Texture *tex)
{
	vif1SetTextureMIP(tex, 0, 0, 0);
}

int frameCount;
int hwFrameLimit = -1;	/* argv[4]: run N frames then exit (for scripted
			 * screenshots) */
int openingType, nextOpeningType, fooOpeningType;
int sceneState;

/* NOT original: run mode from the command line.
 * boot    - the real boot sequence: fly-up, SCE text, scene end
 * idle    - stay over the tower field forever (no forward motion, so
 *           the state machine and text never trigger)
 * illegal - the illegal-disc scene (openingType 1)
 * menu    - the main-menu 3D background scene (Module U, menu.c) */
enum { MODE_BOOT, MODE_IDLE, MODE_ILLEGAL, MODE_MENU };
static int openingMode = MODE_BOOT;
static int argBase;	/* first numeric arg, set by ParseArgs */

/* camera state machine support (real addresses):
 * openingGo (0x2a7794) - "the main acceleration may start";
 * bootRequest (0x2a7790) - initialized 1 in the real data segment, the
 *   state-2 boot dispatch runs once and sets it -1;
 * lastBootParam (0x2a77a0);
 * openingEndFlag (0x2a77f4) / openingEndFrame (0x2a7800) - state 6's
 *   "request scene end 128 frames from now" latch (the end flag also
 *   flips the illegal text's fade direction);
 * drawBlackBars (0x2a7714) - letterbox bars, set at opening start when
 *   the system config's screen size is Letterbox (0x211f90/0x203690) */
static int openingGo;
static int bootRequest = 1;
static int lastBootParam;
static int openingEndFlag;
static int openingEndFrame;
static int drawBlackBars;

#define TMPDATA ((u32*)0x800000)	// TODO: might want to be careful with this

static u32
UploadImage(void *data, u32 gsAddr, u32 psm, Rect *r)
{
	u8 *texels;
	u32 tbp;
	int i, maxH, n;
	sceGsLoadImage limg;

	texels = data;
	tbp = gsAddr;
	// what's going on here?
	maxH = 0x8FFF / (r->w*psmToBppEE(psm) / 128);
	n = (r->h + maxH-1)/maxH;

	for(i = 0; i < n; i++) {
		int left = r->h - i*maxH;
		int h = min(maxH, left);
		sceGsSetDefLoadImage(&limg, tbp, r->w/64, psm, 0, 0, r->w, h);
		FlushCache(0);
		// BUG: original code uses h instead of maxH
		sceGsExecLoadImage(&limg, (u128*)(texels + r->w*i*maxH*psmToBppEE(psm)/8));
		sceGsSyncPath(0, 0);
		tbp += r->w*h*psmToBppGS(psm)/32/64;
	}

	/* NOT in the real ROM (which never TEXFLUSHes in the opening - safe
	 * on a cold boot where nothing stale exists): flush the GS
	 * texture/CLUT cache after every upload so emulators and warm
	 * resets can't keep sampling whatever these blocks held before. */
	vif1SetAD(SCE_GS_TEXFLUSH, 0);

	return gsAddr;
}

void
InitTexture(Texture *tex)
{
	int i, x, y;
	u32 bufsz, ui;
	u32 *dataBufs[2];
	int buf;
	u32 psm;
	u32 lum, alpha;
	Rect prevDim, mipDim;

	if(tex->data == nil)
		tex->data = GetResourceData(tex->resourceID);
	bufsz = tex->dim.w*tex->dim.h*psmToBppEE(tex->format)/32 + tex->dataOffset/4;
	switch(tex->format) {
	case SCE_GS_PSMCT32:
	case SCE_GS_PSMCT24:
	case SCE_GS_PSMCT16:
		break;
	// expand white/black with alpha into 32 bit
	case 3:
	case 4:
		bufsz *= 4;
		break;
	// expand lum+alpha into 32 bit
	case 5:
		bufsz *= 2;
		break;
	}
	dataBufs[0] = TMPDATA;
	dataBufs[1] = TMPDATA + bufsz;
	memcpy(TMPDATA, tex->data, tex->dim.w*tex->dim.h*psmToBppEE(tex->format)/8 + tex->dataOffset);

	switch(tex->format) {
	case 3:
	case 4:
		buf = 1;
		for(ui = 0; ui < bufsz; ui++) {
			lum = tex->format == 3 ? 255 : 0;
			alpha = ((u8*)dataBufs[0])[ui];
			dataBufs[buf][ui] = alpha<<24 | lum<<16 | lum<<8 | lum;
		}
		psm = SCE_GS_PSMCT32;
		break;
	case 5:
		buf = 1;
		for(ui = 0; ui < bufsz; ui++) {
			lum = ((u8*)dataBufs[0])[ui*2+0];
			alpha = ((u8*)dataBufs[0])[ui*2+1];
			dataBufs[buf][ui] = alpha<<24 | lum<<16 | lum<<8 | lum;
		}
		psm = SCE_GS_PSMCT32;
		break;
	default:
		buf = 0;
		psm = tex->format;
		break;
	}

	tex->gstex.mxl = tex->maxLevel % 8;
	tex->gstex.tbp[0] = UploadImage(dataBufs[buf] + tex->dataOffset/4, gsAllocBuffer(psm, &tex->dim), psm, &tex->dim);
	tex->gstex.psm = psm;

	if(tex->clut) {
		Rect r;
		r.x = r.y = 0;
		if(psm == SCE_GS_PSMT8 || psm == SCE_GS_PSMT8H) {
			r.w = 16;
			r.h = 16;
		} else {
			r.w = 8;
			r.h = 2;
		}
		tex->gstex.cbp = UploadImage(tex->clut, gsAllocBuffer(SCE_GS_PSMCT32, &r), SCE_GS_PSMCT32, &r);
	} else
		tex->gstex.cbp = 0;

	tex->gstex.dim[0] = tex->dim;
	prevDim = tex->gstex.dim[0];
	mipDim = prevDim;
	prevDim.x = prevDim.y = 0;
	mipDim.x = mipDim.y = 0;
	for(i = 1; i < tex->gstex.mxl+1; i++) {
		buf = !buf;

		mipDim.w = mipDim.w/2;
		mipDim.h = mipDim.h/2;
		tex->gstex.tbp[i] = gsAllocBuffer(psm, &mipDim);
		tex->gstex.dim[i] = mipDim;

		// only 16 bit mipmaps supported
		if(psm == SCE_GS_PSMCT16) {
			for(y = 0; y < mipDim.h; y++) {
				for(x = 0; x < mipDim.w; x++) {
					u32 off = i == 1 ? tex->dataOffset/4 : 0;
					u16 *src = (u16*)(dataBufs[!buf] + off);
					u16 *dst = (u16*)dataBufs[buf];
					/* the original's (y+1) term reads source row
					 * 2y+2, which on the LAST mip row is one row
					 * past the source buffer; clamp to the last
					 * valid row instead, matching the real mips
					 * (mid-tone there, not averaged with garbage).
					 * The real generator's exact filter still
					 * differs by +-1 LSB noise - invisible. */
					u32 row1 = y*prevDim.h;
					u32 row2 = y == mipDim.h-1 ? row1 : (y+1)*prevDim.h;
					// BUG: original code picks wrong colors here
					u16 col0 = src[(row1 + x)*2];
					u16 col1 = src[(row1 + (x+1))*2];
					u16 col2 = src[(row2 + x)*2];
					u16 col3 = src[(row2 + (x+1))*2];
					int r = col0&0x1F;
					int g = (col0>>5)&0x1F;
					int b = (col0>>10)&0x1F;
					int a = (col0>>15);
					r += col1&0x1F;
					g += (col1>>5)&0x1F;
					b += (col1>>10)&0x1F;
					r += col2&0x1F;
					g += (col2>>5)&0x1F;
					b += (col2>>10)&0x1F;
					r += col3&0x1F;
					g += (col3>>5)&0x1F;
					b += (col3>>10)&0x1F;
					a |= (col1>>15);
					a |= (col2>>15);
					a |= (col3>>15);
					r /= 4;
					g /= 4;
					b /= 4;
					dst[y*mipDim.w + x] = a<<15 | b<<10 | g<<5 | r;
				}
			}
		}

		UploadImage(dataBufs[buf], tex->gstex.tbp[i], psm, &mipDim);

		prevDim = mipDim;
	}
}

static void
InitTextures(void)
{
	int i;

	for(i = 0; i < numTextures; i++) {
		switch(textures[i].usage) {
		case 0:
			InitTexture(&textures[i]);
			break;
		case 1:
			if(openingType == 0)
				InitTexture(&textures[i]);
			break;
		case 2:
			if(openingType == 1)
				InitTexture(&textures[i]);
			break;
		case 3:
			if(openingType == 1 && i == TEXID_PNG+GetLanguage())
				InitTexture(&textures[i]);
			break;
		}
	}
}

void
InitRender(void)
{
	sceDevVif0Reset();
	sceDevVu0Reset();
	sceDevVif1Reset();
	sceDevVu1Reset();
	sceDevGifReset();
	sceGsResetPath();
	memset((void*)VU1_MEM, 0, 0x4000);

	InitDMA();
	InitSPR();
	InitDoubleBuffer();
	gsInitAlloc();
	gsAllocExtraBuffers();
	InitTextures();
	printf("gs alloc high water: %d/16384 blocks\n", gsAllocPtr);
}

static sceVu0FVECTOR clipMin = { 2048.0f-320.0f, 2048.0f-112.0f, 0.0f, 5.0f };
static sceVu0FVECTOR clipMax = { 2048.0f+320.0f, 2048.0f+112.0f, 0.0f, 16777215.0f };

static float rotation = 0.0f;
static sceVu0FVECTOR position = { 0.0f, 0.0f, 0.0f, 0.0f };
static sceVu0FVECTOR fwdDir = { 0.0f, 0.0f, 0.0f, 0.0f };
static sceVu0FVECTOR upDir = { 0.0f, 0.0f, 0.0f, 0.0f };
static sceVu0FVECTOR light1 = { 0.0f, 0.0f, 0.0f, 0.0f };
static sceVu0FVECTOR light2 = { 0.0f, 0.0f, 0.0f, 0.0f };
static sceVu0FVECTOR light3 = { 0.0f, 0.0f, 0.0f, 0.0f };
static sceVu0FVECTOR positionAccel1;
static sceVu0FVECTOR positionAccel2;
static sceVu0FVECTOR positionSpeed;
static sceVu0FVECTOR rotationAccel;
static sceVu0FVECTOR rotationSpeed;
static int openingState;


static float fogAnimation[6];
static sceVu0FVECTOR fogVertices[17][17];
static sceVu0IVECTOR fogColors[17][17];

/* real: OpeningInitAnimation (0x215f18) - initial drift across the
 * field: 0.04/frame forward, 0.001/frame roll. */
static void
InitAnimation(void)
{
	openingState = 0;

	positionSpeed[2] = 0.04f;	/* real: *(0x2a70bc) */
	rotationSpeed[2] = 0.001f;	/* real: *(0x2a70c0) */

	positionAccel1[2] = 0.0f;

	positionAccel2[0] = 0.0f;
	positionAccel2[1] = 0.0f;
	positionAccel2[2] = 0.0f;

	positionSpeed[0] = 0.0f;
	positionSpeed[1] = 0.0f;

	rotationAccel[0] = 0.0f;
	rotationAccel[1] = 0.0f;
	rotationAccel[2] = 0.0f;

	rotationSpeed[0] = 0.0f;
	rotationSpeed[1] = 0.0f;

	openingGo = 0;
}

/* real: InitIllegalDisc (0x215f68) - start the illegal-disc scene's
 * camera: state 4 at height 672, moving up fast and braking. */
static void
InitIllegalDisc(void)
{
	position[2] = 672.0f;
	openingState = 4;

	positionAccel2[2] = -0.0178f;	/* real: *(0x2a70c4) */
	positionSpeed[2] = 2.1599939f;	/* real: *(0x2a70c8) */
	rotationSpeed[2] = 0.00462f;	/* real: *(0x2a70cc) */

	positionAccel2[0] = 0.0f;
	positionAccel2[1] = 0.0f;

	positionSpeed[0] = 0.0f;
	positionSpeed[1] = 0.0f;

	rotationAccel[0] = 0.0f;
	rotationAccel[1] = 0.0f;
	rotationAccel[2] = 0.0f;

	rotationSpeed[0] = 0.0f;
	rotationSpeed[1] = 0.0f;

	openingGo = 0;
}

static void
InitFog(void)
{
	int y, x;
	float foo = sqrtf(5202.0f);	// what's this?

	for(y = 0; y < 17; y++)
		for(x = 0; x < 17; x++) {
			float dx, dy, d;
			// TODO: what's this about? some useless sin/cos here
			float c = 1.0f;
			float s = 0.0f;

			float *v = fogVertices[x][y];
			v[0] = (x-8)*6.0 + 2*c;
			v[1] = (y-8)*6.0 + 2*s;
			v[2] = 134.0f;
			v[3] = 1.0f;

			int *col = fogColors[x][y];
			dx = -5.1f - ((2*x-16)*6.0f*0.5f + 3.0f);
			dy = 0.0f - ((2*y-16)*6.0f*0.5f + 3.0f);
			d = sqrtf(dx*dx + dy*dy);
			d = (foo - d*4.0f)*96.0f/foo + 0.0f;
			d = clamp(d, 0.0f, 127.0f);
			// WTF is this? but only blue is used anyway...
			col[0] = col[1] = d*0.0f/128.0f;
			col[2] = d*128.0f/128.0f;
			col[3] = 128;
		}
}

static void
DrawFog(void)
{
	int texIDs[] = {
		TEXID_FOG4, TEXID_FOG2, TEXID_FOG1,
		TEXID_FOG4, TEXID_FOG2, TEXID_FOG1
	};
	int l, x, y, k;
	Texture *tex;

	// TODO: fading??

	vif1SetZWrite(0);
	vif1SetCLAMP_1(0, 0, 0, 0, 0, 0);
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));

	for(l = 0; l < 6; l++) {
		fogAnimation[l] += (14 - l)*0.0001f*(l+1)*0.5f;
		if(fogAnimation[l] > 1.0f)
			fogAnimation[l] -= 1.0f;

		float dz = l*5.0f;

		vif1Begin();
		tex = &textures[texIDs[l%6]];
		pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(tex->gstex.tbp[0], 64/64, SCE_GS_PSMCT16, 6, 6,
			1, SCE_GS_MODULATE, 0, 0, 0, 0, 0));
		pktSetAlphaBlend(1, 0, 20);	// what's with FIX here?
		vif1End();

		for(y = 0; y < 16; y++) {
			vif1Begin();
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

				pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 1, 0, 1, 0, 0, 0, 0));
				for(k = 0; k < 4; k++) {
					float q = sprTransformVertex(sprVertices->verts2[k], sprVertices->verts1[k], sprMatrices->cameraScreenMatrix);

					float s = ((k%2)*0.5f + (x%2)*0.5f - fogAnimation[l])*q;
					float t = ((k/2)*0.5f + (y%2)*0.5f)*q;
					pktSetAD(SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&s, *(u32*)&t));

					u32 c = fogColors[x+k%2][y+k/2][2];
					pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(c/4, c*2/5, c, 128, *(u32*)&q));
				//	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(255, 255, 255, 128, *(u32*)&q));

					u32 *v = sprVertices->verts2[k];
					pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
				}
			}
			vif1End();
		}
	}

	vif1SetZWrite(1);
}

static sceVu0FVECTOR lightPositions[4];
static sceVu0FMATRIX lightMatrixHistory[4][4];
static sceVu0IVECTOR lightTrailVerts[4][128];
static int lightTrailStart = 0;
static int lightTrailEnd = 0;
static int lightAlphas[] = { 24, 12, };

static sceVu0FVECTOR lightColors[] = {
	{ 32.0f, 128.0f, 0.0f, 0.0f },
	{ 128.0f, 32.0f, 64.0f, 0.0f },
	{ 128.0f, 0.0f, 0.0f, 0.0f },
	{ 64.0f, 32.0f, 128.0f, 0.0f },
};
static sceVu0FVECTOR lightVertices[] = {
	{ -0.8f, -0.8f, 0.0f, 1.0f },
	{  0.8f, -0.8f, 0.0f, 1.0f },
	{ -0.8f,  0.8f, 0.0f, 1.0f },
	{  0.8f,  0.8f, 0.0f, 1.0f },
	{ -0.25f, -0.25f, 0.0f, 1.0f },
	{  0.25f, -0.25f, 0.0f, 1.0f },
	{ -0.25f,  0.25f, 0.0f, 1.0f },
	{  0.25f,  0.25f, 0.0f, 1.0f },
};
static sceVu0FVECTOR lightTexCoords[] = {
	{ 0.0f, 0.0f, 0.0f, 0.0f },
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 1.0f, 1.0f, 0.0f, 0.0f },
};
static sceVu0FVECTOR origin = { 0.0f, 0.0f, 0.0f, 1.0f };

static int lightsSeed;

/* the ROM's own rand(): newlib-1999 32-bit LCG at 0x25b478, seed cell
 * starts at 1 and srand() provably never runs before the opening
 * (verified against two real savestates: the seed cell sits exactly N
 * LCG steps from 1).  The SDK libc's rand() is the modern 64-bit
 * newlib LCG - a DIFFERENT stream - so the port must roll its own to
 * reproduce the real boot's values (lightsSeed = draw #386 = 4002,
 * savestate-verified). */
static u32 osdRandSeed = 1;
static int
osdRand(void)
{
	osdRandSeed = osdRandSeed*1103515245 + 12345;
	return osdRandSeed & 0x7FFFFFFF;
}

static void InitCubes(void);

static void
InitLightsCubes(void)
{
	int i, l;

	/* the real OpeningInitLightsCubes (0x217ab0) computes the cube half
	 * (seed table -> cubeAnchor/cubeRate/cubeOutB) inline; split into
	 * InitCubes() for readability. */
	InitCubes();

	for(l = 0; l < 4; l++) {
		/* dead code in the ROM too (0x217c9c-0x217cf0): results
		 * discarded.  Kept to match. */
		cosf(frameCount*0.005f*(l+1));
		sinf(frameCount*0.003f*(l+1));

		for(i = 0; i < 128; i++) {
			lightTrailVerts[l][i][0] = 0.0f;
			lightTrailVerts[l][i][1] = 0.0f;
			lightTrailVerts[l][i][2] = 0.0f;
		}

		for(i = 0; i < 4; i++)
			sceVu0UnitMatrix(lightMatrixHistory[l][i]);
	}
	lightTrailStart = 0;
	lightTrailEnd = 0;
	lightsSeed = osdRand()%2345 + 3456;	/* real: 0x217d80 */
}

static void
DrawLights(void)
{
	int l, i, j, k;

	vif1SetZWrite(0);
	vif1SetZTest(0);
	vif1SetTexture(&textures[TEXID_CRBL]);

	for(l = 0; l < 4; l++) {
		float c = cosf((frameCount + lightsSeed + l*17)*0.01f*(l+10)*0.1f);
		float s = sinf((frameCount + lightsSeed + l*15)*0.005f*(l+10)*0.1f);

		for(i = 0; i < 4; i++) {
			if(i == 3) {
				lightPositions[l][0] = (10.0f-l)*c;
				lightPositions[l][1] = (3.0f+l)*s;
				lightPositions[l][2] = c*12.0f + 88.0f;
				lightPositions[l][3] = 0.0f;
				sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->unit, lightPositions[l]);
				sceVu0MulMatrix(sprMatrices->worldScreenMatrix, sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
				sceVu0CopyMatrix(lightMatrixHistory[l][3], sprMatrices->worldScreenMatrix);
			} else {
				sceVu0CopyMatrix(lightMatrixHistory[l][i], lightMatrixHistory[l][i+1]);
				sceVu0CopyMatrix(sprMatrices->worldScreenMatrix, lightMatrixHistory[l][i]);
			}

			sprTransformVertex(lightTrailVerts[l][lightTrailEnd], origin, sprMatrices->worldScreenMatrix);

			vif1Begin();
			for(j = 0; j < 2; j++) {
				pktSetAlphaBlend(1, 5, 128);
				pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 1, 0, 1, 0, 0, 0, 0));
				if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->worldScreenMatrix, &lightVertices[j*4], 4))
					continue;

				for(k = 0; k < 4; k++) {
					float q = sprTransformVertex(sprVertices->verts2[0], lightVertices[j*4+k], sprMatrices->worldScreenMatrix);
					float s = lightTexCoords[k][0]*q;
					float t = lightTexCoords[k][1]*q;
					osdRand();	/* real call at 0x216a54, result
						 * unused - kept for rand() phase */

					int r, g, b, a;
					if(j == 0) {
						r = lightColors[l%4][0]*0.5f;
						g = lightColors[l%4][1]*0.5f;
						b = lightColors[l%4][2]*0.5f;
						a = lightAlphas[j]*(i+1)/5;
					} else {
						r = g = b = 128;
						a = lightAlphas[j]*(i+1)/5;
					}
					pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(r, g, b, a, *(u32*)&q));

					pktSetAD(SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&s, *(u32*)&t));

					u32 *v = sprVertices->verts2[0];
					pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
				}
			}
			vif1End();
		}
	}

	lightTrailEnd = (lightTrailEnd+1) % 128;
	if(lightTrailEnd == lightTrailStart)
		lightTrailStart = (lightTrailStart+1) % 128;

	// trails

	vif1Begin();
	pktSetAlphaBlend(1, 5, 128);
	vif1End();

	for(l = 0; l < 4; l++) {
		vif1Begin();

		float x0 = 0.0f;
		float y0 = 0.0f;

		// weird setting...
		pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_LINESTRIP, 1, 0, 0, 0, 1, 1, 0, 0));

		i = (lightTrailStart+1) % 128;
		k = (lightTrailEnd+128-1) % 128;
		int intensity = (lightTrailEnd+128-1 - i) % 128;
		for(j = 0; k != i; k = (k+128-1) % 128, j++) {
			if((j & 7) != 0)
				continue;
			int r, g, b, a;
			a = (intensity - j)*64/intensity;
			r = lightColors[l][0]*a/128.0f;
			g = lightColors[l][1]*a/128.0f;
			b = lightColors[l][2]*a/128.0f;
			pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(r, g, b, a*2, 0));

			u32 *v = lightTrailVerts[l][k];
			float x1 = x0;	// BUG: this isn't updated in original
			float y1 = y0;
			x0 = (v[0]>>4)-2048+320;
			y0 = (v[1]>>4)-2048+112;
			if(v[2]>>4 < 5 ||
			   x0 < 0 || x0 > 640 ||
			   y0 < 0 || y0 > 224 ||
			   x1 < 0 || x1 > 640 ||
			   y1 < 0 || y1 > 224)
				pktSetAD(SCE_GS_XYZF3, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
			else
				pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
		}

		vif1End();
	}
}

/* ==== cubes: the refractive-cube half of DrawLightsAndCubes ====
 *
 * Real structure (DrawCube 0x217520 and its helpers; full trace in
 * docs/towers-analysis.md):
 *  - once per cube: integrate+wrap a rotation angle, build a rotate+
 *    translate+view matrix chain, transform all 8 corners, clip-test
 *    the whole cube (real: cube_21BF88).
 *  - if visible: per-face normal, visibility side and per-vertex
 *    lighting for all 6 faces (real: cube_21BC90, called 6x).
 *  - capture the current screen into the cube's own working buffer
 *    (real: sub_21c7a8 - the cube-side twin of DrawToExtraBuf2, using
 *    a SEPARATE buffer slot, 0x279f10 vs DrawToExtraBuf2's 0x279f18).
 *  - draw each visible face in 10 layered passes (back faces into the
 *    working buffer first, then front faces onto the screen), each
 *    pass picking its texture source via a 4-way selector (real:
 *    CubeTextureFuckery 0x21c400): a texture resource, the in-progress
 *    frame, or the just-captured working buffer. */

#define CUBE_INSTANCES 5

/* real: 0x27a210, 5 x 16-byte (3 floats + pad), read by
 * OpeningInitLightsCubes with index i%5 */
static sceVu0FVECTOR cubeSeedTable[CUBE_INSTANCES] = {
	{  3.5679f,  0.5447f, 2.5932f, 0.0f },
	{ -0.9042f, -1.1173f, 3.7952f, 0.0f },
	{  3.2639f, -2.6491f, 4.1075f, 0.0f },
	{ -3.7296f, -2.3677f, 4.3654f, 0.0f },
	{ -3.1017f,  2.2409f, 4.5429f, 0.0f },
};
/* real 0x27af50: the ILLEGAL scene's own seed table - sub_219cb8 reads
 * this one, NOT the opening's 0x27a210 table above (two cubes park
 * right in front of the camera's rest position at z=800, two frame the
 * shot far off-axis) */
static sceVu0FVECTOR illegalCubeSeedTable[CUBE_INSTANCES] = {
	{ -10.4068f,  4.1636f, 5.0429f, 0.0f },
	{ -12.9184f, -4.2708f, 4.3654f, 0.0f },
	{   2.7639f,  0.1509f, 4.1075f, 0.0f },
	{   4.0958f, -1.3173f, 3.0952f, 0.0f },
	{  -2.4321f,  0.5447f, 2.7732f, 0.0f },
};
static sceVu0FVECTOR cubeAnchor[CUBE_INSTANCES];	/* real 0x27b0f0: world position */
static sceVu0FVECTOR cubeRate[CUBE_INSTANCES];		/* real 0x27b190: rotation rate/frame */
/* real 0x27b140: the live rotation angle, integrated by cubeRate and
 * wrapped to [-PI,PI] every frame (cube_21BF88) */
static sceVu0FVECTOR cubeOutB[CUBE_INSTANCES];

/* debug instrumentation: cull/face-count results, readable from a
 * savestate */
static int cubeCullResult[CUBE_INSTANCES];
static int cubeFacesDrawn[CUBE_INSTANCES];

/* cube geometry.  Real: face->vertex index table 0x27afe0, per-face
 * normal table 0x27b090, half-extent 0x2a714c = 1.8 (read via the real
 * corner-builder sub_21b690). */
#define CUBE_HALF_EXTENT 1.8f
static sceVu0FVECTOR cubeCorners[8] = {
	{ -CUBE_HALF_EXTENT,-CUBE_HALF_EXTENT,-CUBE_HALF_EXTENT, 1.0f }, {  CUBE_HALF_EXTENT,-CUBE_HALF_EXTENT,-CUBE_HALF_EXTENT, 1.0f },
	{ -CUBE_HALF_EXTENT, CUBE_HALF_EXTENT,-CUBE_HALF_EXTENT, 1.0f }, {  CUBE_HALF_EXTENT, CUBE_HALF_EXTENT,-CUBE_HALF_EXTENT, 1.0f },
	{ -CUBE_HALF_EXTENT,-CUBE_HALF_EXTENT, CUBE_HALF_EXTENT, 1.0f }, {  CUBE_HALF_EXTENT,-CUBE_HALF_EXTENT, CUBE_HALF_EXTENT, 1.0f },
	{ -CUBE_HALF_EXTENT, CUBE_HALF_EXTENT, CUBE_HALF_EXTENT, 1.0f }, {  CUBE_HALF_EXTENT, CUBE_HALF_EXTENT, CUBE_HALF_EXTENT, 1.0f },
};
static const u8 cubeFaceVerts[6][4] = {
	{ 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 6, 2 },
	{ 2, 3, 6, 7 }, { 1, 5, 3, 7 }, { 4, 5, 0, 1 },
};
static sceVu0FVECTOR cubeFaceNormal[6] = {
	{ 0.0f, 0.0f,-1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },
	{-1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f },
	{ 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f,-1.0f, 0.0f, 1.0f },
};
/* real 0x27afa0: the standard unit UV quad */
static const sceVu0FVECTOR cubeUV[4] = {
	{ 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f },
};
/* real: sceVu0LightColorMatrix's 4 inputs, 0x27b040/50/60/70 (fixed
 * grayscale lighting - a reflective/glass look, not a coloured one) */
static sceVu0FVECTOR cubeColorAmbient = { 0.0f, 0.0f, 0.0f, 0.0f };
static sceVu0FVECTOR cubeColor1 = { 0.5f, 0.5f, 0.5f, 0.0f };
static sceVu0FVECTOR cubeColor2 = { 0.5f, 0.5f, 0.5f, 0.0f };
static sceVu0FVECTOR cubeColorBase = { 0.2f, 0.2f, 0.2f, 1.0f };

static struct {
	sceVu0FMATRIX rotated;
	sceVu0FMATRIX translated;
	sceVu0FMATRIX transformed;
	sceVu0FMATRIX lightColor;	/* computed once at init, real: sceVu0LightColorMatrix */
	sceVu0IVECTOR screenVerts[8];
	sceVu0FVECTOR worldNormal[6];
	float faceSign[6];		/* >0 = facing away from camera (draw first) */
	float vertexLight[6][4];
	sceVu0FVECTOR worldCorners[8];	/* real struct+128: rot+trans x corner */
	sceVu0FVECTOR viewDir[8];	/* real struct+256: normalize(cameraMatrix x worldCorner) */
	float cornerQ[8];		/* real struct+512: per-corner 1/w */
	float center[2];		/* real 0x27b360: cube origin in window-float coords */
	sceVu0FVECTOR screenNormal[6];	/* real struct+736: normalize(cameraMatrix x worldNormal) */
} cubeStruct;

/* real 0x27b3f0: the base colour table is per-INSTANCE, not per-face -
 * cube_21BF88 slices it by its instance argument, so all six faces of
 * one cube share one colour.  Written by sub_21b690 (each caller's
 * {x,y,z}+128: the opening's (1.8,-16,-16,24) gives {112,112,152}, the
 * illegal init's (1.2,0,0,0) gives {128,128,128}); cube_21BC90 then
 * copies it through sceVu0ClampVector(.., 0.0, 127.0) - that clamp is
 * folded into the values sub_21b690 stores here. */
static float cubeBaseColor[3] = { 112.0f, 112.0f, 127.0f };

static void sub_21b690(float half, float x, float y, float z);

static void
InitCubes(void)
{
	int i;

	/* real: OpeningInitLightsCubes' own sub_21b690 call - rebuilds the
	 * corner table and base colour (the init-time sub_219cb8 leaves the
	 * ILLEGAL values in them) */
	sub_21b690(1.8f, -16.0f, -16.0f, 24.0f);

	sceVu0LightColorMatrix(cubeStruct.lightColor,
			cubeColorAmbient, cubeColor1, cubeColor2, cubeColorBase);

	for(i = 0; i < CUBE_INSTANCES; i++) {
		float d = i == 2 ? 0.9f : (float)(i-2)*0.8f;
		float v;

		cubeAnchor[i][0] = cubeSeedTable[i][0]*3.5f;
		cubeAnchor[i][1] = cubeSeedTable[i][1]*3.5f;
		cubeAnchor[i][2] = cubeSeedTable[i][2]*-15.0f + 150.0f;
		cubeAnchor[i][3] = 0.0f;

		cubeRate[i][0] = 0.0031f/d;
		cubeRate[i][1] = d*0.0022f;
		cubeRate[i][2] = d/1000.0f + 0.0013f;
		cubeRate[i][3] = 0.0f;

		v = d*(float)(i%3)*3.7f + 0.2856f;
		cubeOutB[i][0] = v;
		cubeOutB[i][1] = v;
		cubeOutB[i][2] = v;
		cubeOutB[i][3] = v;
	}
}

/* real: sub_21c7a8 - stamp the CURRENT screen (towers + composite + fog
 * + lights, and any cube instance already drawn this frame) into the
 * cube's working buffer (extraBuf2).  The screen is sampled as PSMCT24
 * (alpha-less) with TEXA{TA0=127, AEM=1}: every non-black pixel gets
 * alpha 127, black stays 0 - this manufactured "is there anything
 * behind" alpha is what the Ad-keyed env layers (blend mode 8) key on
 * later.
 *
 * Deliberately NO framebuffer restore at the end: every subsequent cube
 * pass sets FRAME itself (see CubeSetupPass), and the pass sequence
 * ends on screen-targeted passes.  (extraBuf2 is TBP0-block units;
 * FRAME.FBP is 32x coarser pages.) */
static void
CubeCaptureBuffer(void)
{
	Rect full;
	Color col = { 128, 128, 128, 0 };	/* real 0x2a46a8 - note alpha 0 */
	sceGsDrawEnv1 *cur = stableEvenOddFrame == 0 ? &db.draw0 : &db.draw1;

	full.x = full.y = 0;
	full.w = screenW;
	full.h = screenH;

	vif1SetFramebuffer(extraBuf2/32, SCE_GS_PSMCT32, screenW, screenH, 1);
	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetAlphaBlend(0, 2, 0);

	vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(cur->frame1.FBP*32, screenW/64,
			SCE_GS_PSMCT24, GetTexExponent(screenW), GetTexExponent(screenH),
			1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	vif1SetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(0x7f, 1, 0));
	vif1SetTexRect(&full, &full, &col, 0, 0);

	/* real 21c7a8's tail: restore the init TEXA (TA1=129) and CLEAR
	 * FBA_1.  Init sets FBA=1 (real 0x218d64, see InitOpeningScene) and
	 * the first capture clears it - without this every framebuffer
	 * write forces alpha>=128 and the Ad-gated env layers run away into
	 * a feedback loop. */
	vif1SetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(0x7f, 1, 0x81));
	vif1SetAD(SCE_GS_FBA_1, 0);
}

/* real: cube_21BC90, per face: the world normal (rotating the axis-
 * aligned cubeFaceNormal[] entry - numerically identical to the real
 * edge cross product), the SCREEN-projected normal (cameraMatrix x
 * worldNormal, normalized - real struct+736, the refraction-offset /
 * env-UV direction), the visibility sign, and the per-vertex "light":
 * |dot(faceNormal, viewDirToVertex)| - a fresnel-style view-angle term
 * (the (1-l)^2 term downstream peaks at silhouette edges: edge glow). */
static void
CubeFaceSetup(int face)
{
	sceVu0FVECTOR n, sn, dir;
	float l;
	int k;

	sceVu0ApplyMatrix(n, cubeStruct.rotated, cubeFaceNormal[face]);
	sceVu0Normalize(cubeStruct.worldNormal[face], n);
	cubeStruct.worldNormal[face][3] = 0.0f;

	/* the real 21BC90 normal is OuterProduct(v1-v0, v2-v0) over the
	 * face-vert table, which points INWARD (face 0 = {0,1,2,3} on the
	 * -z face gives +z) - the refraction/env UV offset direction
	 * depends on this sign.  Lighting and faceSign are sign-invariant,
	 * so only the projected normal uses the inward one. */
	n[0] = -cubeStruct.worldNormal[face][0];
	n[1] = -cubeStruct.worldNormal[face][1];
	n[2] = -cubeStruct.worldNormal[face][2];
	n[3] = 0.0f;
	sceVu0ApplyMatrix(sn, sprMatrices->cameraMatrix, n);
	sceVu0Normalize(cubeStruct.screenNormal[face], sn);

	/* real visibility sign (21bdc0..21be9c): 2D cross product of the
	 * NORMALIZED screen-projected edges (v0-v1) x (v0-v2) - a true
	 * winding test (under perspective the visibility boundary is
	 * vertex-dependent, so a dot with the view direction is not
	 * equivalent).  Convention: sign < 0 = back face (group 1). */
	{
		float x0 = cubeStruct.screenVerts[cubeFaceVerts[face][0]][0]/16.0f;
		float y0 = cubeStruct.screenVerts[cubeFaceVerts[face][0]][1]/16.0f;
		float x1 = cubeStruct.screenVerts[cubeFaceVerts[face][1]][0]/16.0f;
		float y1 = cubeStruct.screenVerts[cubeFaceVerts[face][1]][1]/16.0f;
		float x2 = cubeStruct.screenVerts[cubeFaceVerts[face][2]][0]/16.0f;
		float y2 = cubeStruct.screenVerts[cubeFaceVerts[face][2]][1]/16.0f;
		float e1x = x0-x1, e1y = y0-y1;
		float e2x = x0-x2, e2y = y0-y2;
		float l1 = sqrtf(e1x*e1x + e1y*e1y);
		float l2 = sqrtf(e2x*e2x + e2y*e2y);
		if(l1 > 0.0f && l2 > 0.0f)
			cubeStruct.faceSign[face] = (e1x/l1)*(e2y/l2) - (e1y/l1)*(e2x/l2);
		else
			cubeStruct.faceSign[face] = 0.0f;
	}

	for(k = 0; k < 4; k++) {
		int vi = cubeFaceVerts[face][k];
		dir[0] = cubeStruct.worldCorners[vi][0] - position[0];
		dir[1] = cubeStruct.worldCorners[vi][1] - position[1];
		dir[2] = cubeStruct.worldCorners[vi][2] - position[2];
		dir[3] = 0.0f;
		sceVu0Normalize(dir, dir);
		l = sceVu0InnerProduct(cubeStruct.worldNormal[face], dir);
		if(l < 0.0f) l = -l;
		cubeStruct.vertexLight[face][k] = l;
	}
}

/* real: cube_21BF88 - per-cube setup: integrate+wrap the rotation
 * angle, build the rotate+translate+view matrix chain, transform all 8
 * corners, clip-test, then set up all 6 faces.  Returns nonzero if the
 * whole cube is offscreen. */
static int
CubeTransformAndClip(int instance)
{
	int k;

	for(k = 0; k < 3; k++) {
		cubeOutB[instance][k] += cubeRate[instance][k];
		if(cubeOutB[instance][k] > PI)
			cubeOutB[instance][k] -= TAU;
		else if(cubeOutB[instance][k] < -PI)
			cubeOutB[instance][k] += TAU;
	}

	/* real cube_21BF88 rotates from sprMatrices->unit (always valid) -
	 * the illegal scene never runs InitCubes, so no per-scene base */
	sceVu0RotMatrix(cubeStruct.rotated, sprMatrices->unit, cubeOutB[instance]);
	sceVu0TransMatrix(cubeStruct.translated, cubeStruct.rotated, cubeAnchor[instance]);
	sceVu0MulMatrix(cubeStruct.transformed, sprMatrices->cameraScreenMatrix, cubeStruct.translated);

	if(sceVu0ClipAll(clipMin, clipMax, cubeStruct.transformed, cubeCorners, 8)) {
		cubeCullResult[instance] = 1;
		return 1;
	}
	cubeCullResult[instance] = 0;

	/* screen-space cube centre in window-float coords (real 0x27b360,
	 * used by the refraction pass's centre-relative zoom term) */
	{
		sceVu0FVECTOR origin = { 0.0f, 0.0f, 0.0f, 1.0f };
		sceVu0FVECTOR c4;
		float q;
		sceVu0ApplyMatrix(c4, cubeStruct.transformed, origin);
		q = 1.0f/c4[3];
		cubeStruct.center[0] = c4[0]*q;
		cubeStruct.center[1] = c4[1]*q;
	}

	for(k = 0; k < 8; k++) {
		sceVu0FVECTOR t;
		cubeStruct.cornerQ[k] = sprTransformVertex(cubeStruct.screenVerts[k],
				cubeCorners[k], cubeStruct.transformed);
		sceVu0ApplyMatrix(cubeStruct.worldCorners[k], cubeStruct.translated, cubeCorners[k]);
		sceVu0ApplyMatrix(t, sprMatrices->cameraMatrix, cubeStruct.worldCorners[k]);
		sceVu0Normalize(cubeStruct.viewDir[k], t);
	}

	for(k = 0; k < 6; k++)
		CubeFaceSetup(k);

	return 0;
}

/* ==== the real 10-pass cube draw ====
 * (DrawCube 0x217520, CubeTextureFuckery 0x21c400, DrawTexturedQuad
 * 0x21c560, UV builders cube_21B798/21BBE0/21BA08, colour/ST callback
 * 0x216f88, vif1SetTexture_24_32 0x21c338.)
 *
 * Per cube: after sub_21c7a8 stamps the current screen into the working
 * buffer, the BACK faces (5 passes) are composited INTO the working
 * buffer - the screen sampled at their positions, then scrolling
 * BLPR/REF/BLP/REF env-texture layers - and the FRONT faces (5 passes)
 * go onto the real screen: the working buffer sampled back (so the
 * back faces + background show through), then the same four env layers
 * again.  A two-depth glass effect - and since each pass targets its
 * own framebuffer explicitly and the sequence ends on screen passes,
 * the real ROM never needs a FRAME "restore". */
typedef struct CubePass CubePass;
struct CubePass {
	int targetWork;	/* +0x00: 1 = working buffer (extraBuf2), 0 = screen */
	int sel;	/* +0x10: >=0 textures[] index, -2 prev screen, -3 working buffer */
	int abe;	/* +0x20 */
	int blendMode;	/* +0x30: BlendModes[] index */
	int blendFix;	/* +0x40: blend FIX - doubles as colour brightness /128 */
	int texaFlag;	/* +0x50: write TEXA (the screen-sampling passes) */
	int stMode;	/* +0x60: 1 = pixel-UV/2^exp with Q=1, 0 = uv*q perspective */
	int colorMode;	/* +0x70: 0 flat 128, 2 tinted+edge, 3 pi-flavoured */
	int uvMode;	/* 0 = cube_21B798, 1 = cube_21BBE0, 2 = cube_21BA08 */
	float uvArg;	/* B798: centre-zoom / BBE0: scroll phase / BA08: normal offset */
	float uvArg2;	/* B798 only: normal-offset scale (f13) */
	int group;	/* faceFlag: 1 = back faces, 0 = front faces */
};
/* the ten pass-parameter structs DrawCube builds on its stack, in
 * order; scroll phases/zoom are consts at gp-32568..-32552 =
 * 0x2a7138..0x2a7148.  uvArg -0.084 on the front-face working-buffer
 * sample = the ~8% refraction zoom. */
static CubePass cubePasses[10] = {
	{ 1, -2,         0, 4, 122, 1, 1, 2, 0, 0.0f,      1.0f, 1 },
	{ 1, TEXID_BLPR, 1, 5, 128, 0, 0, 0, 1, -0.00375f, 0,    1 },
	{ 1, TEXID_REF,  1, 8,  42, 0, 0, 3, 2, -0.25f,    0,    1 },
	{ 1, TEXID_BLP,  1, 5, 128, 0, 0, 0, 1, 0.00375f,  0,    1 },
	{ 1, TEXID_REF,  1, 8,  42, 0, 0, 3, 2, -0.25f,    0,    1 },
	{ 0, -3,         0, 4, 240, 1, 1, 2, 0, -0.084f,   1.0f, 0 },
	{ 0, TEXID_BLPR, 1, 5, 128, 0, 0, 0, 1, -0.0075f,  0,    0 },
	{ 0, TEXID_REF,  1, 8,  64, 0, 0, 3, 2, 0.5f,      0,    0 },
	{ 0, TEXID_BLP,  1, 5, 128, 0, 0, 0, 1, 0.0075f,   0,    0 },
	{ 0, TEXID_REF,  1, 8,  64, 0, 0, 3, 2, 0.5f,      0,    0 },
};

/* DrawIllegalCube's pass stack (real: same builder code with the
 * constant pool at 0x2a71b8 instead of 0x2a7138 and the 0x2192c0
 * colour/ST callback): B798 zoom scale 0.8 instead of 1.0, the
 * env-map offsets (passes 2/4/7/9) zero, colour mode 4 (the red
 * variant), everything else identical. */
static CubePass illegalCubePasses[10] = {
	{ 1, -2,         0, 4, 122, 1, 1, 2, 0, 0.0f,      0.8f, 1 },
	{ 1, TEXID_BLPR, 1, 5, 128, 0, 0, 0, 1, -0.00375f, 0,    1 },
	{ 1, TEXID_REF,  1, 8,  42, 0, 0, 4, 2, 0.0f,      0,    1 },
	{ 1, TEXID_BLP,  1, 5, 128, 0, 0, 0, 1, 0.00375f,  0,    1 },
	{ 1, TEXID_REF,  1, 8,  42, 0, 0, 4, 2, 0.0f,      0,    1 },
	{ 0, -3,         0, 4, 240, 1, 1, 2, 0, -0.084f,   0.8f, 0 },
	{ 0, TEXID_BLPR, 1, 5, 128, 0, 0, 0, 1, -0.0075f,  0,    0 },
	{ 0, TEXID_REF,  1, 8,  64, 0, 0, 4, 2, 0.0f,      0,    0 },
	{ 0, TEXID_BLP,  1, 5, 128, 0, 0, 0, 1, 0.0075f,   0,    0 },
	{ 0, TEXID_REF,  1, 8,  64, 0, 0, 4, 2, 0.0f,      0,    0 },
};

/* set while DrawIllegalCube runs - the only behavioural switch not in
 * the pass structs (the ST clamp) */
static int illegalCubeDraw;

/* real: CubeTextureFuckery (0x21c400) - set the pass's render target,
 * texture source and blend. */
static void
CubeSetupPass(CubePass *p)
{
	sceGsDrawEnv1 *cur = stableEvenOddFrame == 0 ? &db.draw0 : &db.draw1;
	u32 fbp = p->targetWork ? extraBuf2/32 : cur->frame1.FBP;

	vif1SetFramebuffer(fbp, SCE_GS_PSMCT32, screenW, screenH, 0);

	if(p->sel >= 0)
		vif1SetTexture(&textures[p->sel]);
	else {
		/* selector -2 = the CURRENT in-progress frame (the same parity
		 * the capture reads and the screen passes target); the
		 * displayed/previous buffer is never sampled - the pipeline
		 * has NO cross-frame reference at all. */
		u32 tbp = p->sel == -3 ? extraBuf2 : cur->frame1.FBP*32;
		vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(tbp, screenW/64, SCE_GS_PSMCT32,
				GetTexExponent(screenW), GetTexExponent(screenH),
				1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
		vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	}
	vif1SetAlphaBlend(1, p->blendMode, p->blendFix);
	if(p->texaFlag)
		vif1SetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(0x7f, 1, 0x81));
}

/* real: cube_21B798 (screen position + projected-normal offset + centre
 * zoom, clamped to the screen rect), cube_21BBE0 (scrolled unit quad),
 * cube_21BA08 (fake env map: view direction + scaled projected normal,
 * xy swapped, 0.5-centred; the real |dot(crossNormal, tableNormal)|
 * factor is identically 1 for a rigid cube and is folded into uvArg). */
static void
CubeComputeUV(CubePass *p, int face, float uv[4][2])
{
	int k;

	for(k = 0; k < 4; k++) {
		int vi = cubeFaceVerts[face][k];
		switch(p->uvMode) {
		case 0: {
			float x = cubeStruct.screenVerts[vi][0]/16.0f;
			float y = cubeStruct.screenVerts[vi][1]/16.0f;
			float q = cubeStruct.cornerQ[vi];
			float u = x - (2048-screenW/2)
				+ cubeStruct.screenNormal[face][0]*(screenW/2)*p->uvArg2*q*4.0f
				+ (x - cubeStruct.center[0])*p->uvArg;
			float v = y - (2048-screenH/2)
				+ cubeStruct.screenNormal[face][1]*(screenH/2)*p->uvArg2*q*4.0f
				+ (y - cubeStruct.center[1])*p->uvArg;
			uv[k][0] = clamp(u, 1.0f, (float)(screenW-1));
			uv[k][1] = clamp(v, 1.0f, (float)(screenH-1));
			break;
		}
		case 1:
			uv[k][0] = cubeUV[k][0] + p->uvArg;
			uv[k][1] = cubeUV[k][1] - p->uvArg;
			break;
		case 2:
			uv[k][0] = cubeStruct.viewDir[vi][1]
				+ cubeStruct.screenNormal[face][1]*p->uvArg + 0.5f;
			uv[k][1] = cubeStruct.viewDir[vi][0]
				+ cubeStruct.screenNormal[face][0]*p->uvArg + 0.5f;
			break;
		}
	}
}

/* real: 0x216f88's colour modes. e = (1-light)^2/2 peaks at silhouette
 * edges (see CubeFaceSetup's view-angle light); blendFix doubles as a
 * /128 brightness (122 back, 240 front). */
static void
CubeVertexColor(CubePass *p, int face, int k, int rgb[3])
{
	float l = cubeStruct.vertexLight[face][k];
	float e = (1.0f-l)*(1.0f-l)*0.5f;
	float s = (float)p->blendFix/128.0f;
	float r, g, b;

	switch(p->colorMode) {
	default:
		rgb[0] = rgb[1] = rgb[2] = 128;
		return;
	case 2:
		r = (cubeBaseColor[0] + e*32.0f)*s;
		g = (cubeBaseColor[1] + e*32.0f)*s;
		b = (cubeBaseColor[2] + e*32.0f)*s;
		break;
	case 3: {
		/* real coefs at gp-32596..-32572 (0x2a711c..0x2a7134): scale
		 * 0.4, then (0.6, 0.2) per channel - the env layers are DIM
		 * (~11..18 for base 112/fix 64) */
		float t = e*0.4f*0.6f + 0.2f;
		r = cubeBaseColor[0]*t*s;
		g = cubeBaseColor[1]*t*s;
		b = cubeBaseColor[2]*t*s;
		break;
	}
	case 4: {
		/* the illegal cube's case-3 variant (real callback 0x2192c0,
		 * coefs 0x2a71a0..0x2a71b4): per-channel e*0.6 + 0.2 with no
		 * 0.4 scale, and green/blue divided by 3 - the red tint */
		float t = e*0.6f + 0.2f;
		r = cubeBaseColor[0]*t*s;
		g = cubeBaseColor[1]*t*s/3.0f;
		b = cubeBaseColor[2]*t*s/3.0f;
		break;
	}
	}
	rgb[0] = (int)clamp(r, 0.0f, 128.0f);
	rgb[1] = (int)clamp(g, 0.0f, 128.0f);
	rgb[2] = (int)clamp(b, 0.0f, 128.0f);
}

/* real: DrawTexturedQuad (0x21c560) - all of one group's faces in a
 * single packet: per face a fresh PRIM (flat-shaded tristrip, AA1 on
 * the screen-sampling passes), then RGBAQ/ST/XYZF2 per vertex. */
static void
DrawCubePass(CubePass *p)
{
	int face, k;
	float uv[4][2];

	CubeSetupPass(p);

	vif1Begin();
	for(face = 0; face < 6; face++) {
		int aa1;
		if((cubeStruct.faceSign[face] < 0.0f) != p->group)
			continue;
		CubeComputeUV(p, face, uv);
		aa1 = p->texaFlag && (p->group == 1 || cubeStruct.faceSign[face] > 0.005f);
		pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, 1, 0,
				p->abe, aa1, 0, 0, 0));
		for(k = 0; k < 4; k++) {
			int vi = cubeFaceVerts[face][k];
			u32 *v = (u32*)cubeStruct.screenVerts[vi];
			int rgb[3];
			float s, t, qf;

			CubeVertexColor(p, face, k, rgb);
			if(p->stMode == 1) {
				float half = (p->group == 0 && stableEvenOddField) ? 0.5f : 0.0f;
				s = uv[k][0]/(float)(1<<GetTexExponent(screenW));
				t = (uv[k][1] - half)/(float)(1<<GetTexExponent(screenH));
				/* the illegal cube's callback (0x2192c0) has no
				 * clamp here; the opening's (0x216f88) does */
				if(!illegalCubeDraw) {
					s = clamp(s, 0.0f, (float)screenW/(float)(1<<GetTexExponent(screenW)));
					t = clamp(t, 0.0f, (float)screenH/(float)(1<<GetTexExponent(screenH)));
				}
				qf = 1.0f;
			} else {
				qf = cubeStruct.cornerQ[vi];
				s = uv[k][0]*qf;
				t = uv[k][1]*qf;
			}
			pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(rgb[0], rgb[1], rgb[2], 128, *(u32*)&qf));
			pktSetAD(SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&s, *(u32*)&t));
			pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
		}
	}
	vif1End();
}

/* debug: one flat opaque red pass instead of the real stack */
#define CUBE_DEBUG_RED 0

/* real: DrawCube (0x217520), called once per cube instance (5x). */
static void
DrawCube(int instance)
{
	int k, face;

	if(CubeTransformAndClip(instance))
		return;

	CubeCaptureBuffer();

	vif1SetXYOffset(1, stableEvenOddField);
	vif1SetZTest(0);

	cubeFacesDrawn[instance] = 0;
	for(face = 0; face < 6; face++)
		if(cubeStruct.faceSign[face] >= 0.0f)
			cubeFacesDrawn[instance]++;

#if CUBE_DEBUG_RED
	{
		sceGsDrawEnv1 *cur = stableEvenOddFrame == 0 ? &db.draw0 : &db.draw1;
		vif1SetFramebuffer(cur->frame1.FBP, cur->frame1.PSM, screenW, screenH, 0);
		vif1Begin();
		for(face = 0; face < 6; face++) {
			pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, 0, 0, 0, 0, 0, 0, 0));
			for(k = 0; k < 4; k++) {
				u32 *v = (u32*)cubeStruct.screenVerts[cubeFaceVerts[face][k]];
				pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(255, 0, 0, 128, 0x3f800000));
				pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
			}
		}
		vif1End();
	}
#else
	for(k = 0; k < 10; k++)
		DrawCubePass(&cubePasses[k]);
#endif

	vif1SetZTest(1);
}


static void
DrawLightsAndCubes(void)
{
	int i;

	/* real order: sceVu0Normalize, DrawLights, vif1SetCLAMP_1 (0x217de4:
	 * WMS=WMT=CLAMP for all the cube passes' sampling), DrawCube x5 */

	sceVu0Normalize(sprVertices->verts1[3], fwdDir);

	DrawLights();

	vif1SetCLAMP_1(1, 1, 0, 0, 0, 0);

#if CUBE_DEBUG_RED
	/* isolation test: a fixed rectangle via the already-trusted
	 * vif1SetFlatRect(), independent of all cube state */
	{
		Rect r;
		Color red = { 255, 0, 0, 128 };
		r.x = screenW/2 - 100;
		r.y = screenH/2 - 50;
		r.w = 200;
		r.h = 100;
		vif1SetFlatRect(&r, &red, 0, 0);
	}
#endif

	for(i = 0; i < CUBE_INSTANCES; i++)
		DrawCube(i);
}

static void
InitOpeningScene(void)
{
	// W values are weird...

	upDir[0] = 0.0f;
	upDir[1] = 1.0f;
	upDir[2] = 0.0f;
	upDir[3] = 1.0f;

	fwdDir[0] = 0.0f;
	fwdDir[1] = 0.0f;
	fwdDir[2] = 1.0f;
	fwdDir[3] = 1.0f;

	position[0] = 0.0f;
	position[1] = 0.0f;
	position[2] = 16.0f;
	position[3] = 0.0f;

	rotation = -0.12f;

	light1[0] = 0.0f;
	light1[1] = 0.0f;
	light1[2] = -1.0f;
	light1[3] = 0.0f;

	light2[0] = 0.5f;
	light2[1] = 0.5f;
	light2[2] = 0.0f;
	light2[3] = 0.0f;

	light3[0] = -0.5f;
	light3[1] = -0.5f;
	light3[2] = 0.0f;
	light3[3] = 0.0f;

	vif1SetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(127, 1, 129));
	vif1SetAD(SCE_GS_FBA_1, 1);

	InitLightsCubes();
}

static void DrawTowers(void);

/* ==== NOT PORTED: DrawOpeningScene's remaining real siblings ====
 * real DrawOpeningScene (0x218d80) calls, in order: DrawTowers,
 * DrawExtraBuf2, DrawToExtraBuf2, DrawFog, DrawLightsAndCubes,
 * sub_218b20, sub_218bd0.  The unported ones are stubbed below so the
 * gap is visible in the call structure. */

/* real: DrawToExtraBuf2 (0x214050) - save the current frame:
 * vif1SetXYOffset(0,...) to disable the interlace half-pixel jitter for
 * this internal buffer, vif1SetZWrite(0), redirect SCE_GS_FRAME_1 to
 * the save buffer (extraBuf1, clear=1), read the screen as drawn so far
 * this frame (towers + DrawExtraBuf2's composite - it runs between them
 * and fog/lights/cubes) via a frame-parity-selected TEX0_1, and squeeze
 * the FULL source into a HALF-WIDTH dest rect; then restore Z-write and
 * the jittered offset.
 *
 * Feedback loop this creates: save_N = towers_N + fade(save_N-1) - the
 * mechanism behind the tower glow/brightness falloff.
 *
 * Unresolved details: the real TEX0 height exponent is a literal 9 vs
 * GetTexExponent's 8 (only affects wrap outside the sampled rect); the
 * real XYOFFSET halfpx arg (0x1f0c44) is assumed = evenOddField; the
 * real also does a SECOND vif1SetFramebuffer(clear=1) at the end whose
 * role is unclear - not ported.  The explicit ZTest toggles around the
 * blit and the FRAME_1 restore at the end are harness additions the
 * real ROM does without (it relies on every later draw setting FRAME
 * itself). */
#define EXTRABUF_FEEDBACK 1
static void
DrawToExtraBuf2(void)
{
#if EXTRABUF_FEEDBACK
	Rect full, half;
	Color gray = { 128, 128, 128, 128 };
	u32 src;
	int tw, th;

	/* the screen buffer being drawn this frame, in TBP0 block units */
	src = (stableEvenOddFrame == 0 ? &db.draw0 : &db.draw1)->frame1.FBP * 32;

	full.x = full.y = 0;
	full.w = screenW;
	full.h = screenH;
	half = full;
	half.w = screenW/2;

	vif1SetXYOffset(0, stableEvenOddField);
	vif1SetZWrite(0);
	/* extraBuf1 is TBP0-block units, FRAME.FBP is 32x coarser pages */
	vif1SetFramebuffer(extraBuf1/32, SCE_GS_PSMCT32, screenW, screenH, 1);
	vif1SetZTest(0);

	tw = GetTexExponent(screenW);
	th = GetTexExponent(screenH);
	vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(src, screenW/64, SCE_GS_PSMCT32,
			tw, th, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	vif1SetTexRect(&half, &full, &gray, 0, 0xFFFFFF);

	/* point FRAME_1/SCISSOR_1 back at the real screen */
	{
		sceGsDrawEnv1 *env = stableEvenOddFrame == 0 ? &db.draw0 : &db.draw1;
		vif1SetFramebuffer(env->frame1.FBP, env->frame1.PSM, screenW, screenH, 0);
	}
	vif1SetZTest(1);
	vif1SetZWrite(1);
	vif1SetXYOffset(1, stableEvenOddField);
#endif
}

/* real: DrawExtraBuf2 (0x214240) - draw the saved buffer back: reads
 * the save buffer LAST frame's DrawToExtraBuf2 wrote (it runs BEFORE
 * this frame's save), temporarily disables Z-test and Z-write,
 * stretches the HALF-WIDTH capture back out to a FULL-WIDTH dest rect
 * (the exact inverse of DrawToExtraBuf2's squeeze), and blends it onto
 * the screen via vif1SetAlphaBlend(1,2,fix) - a deliberate translucent
 * composite that reintroduces a fading copy of recent frames every
 * frame: the radial-brightness-falloff mechanism.  Z-test/write
 * restored before returning.  The real signature carries the blend
 * parameters; the opening passes fix 80, the illegal scene 112. */
static void
DrawExtraBuf2(int fix)
{
#if EXTRABUF_FEEDBACK
	Rect full, half;
	Color gray = { 128, 128, 128, 128 };
	u32 src;
	int tw, th;

	src = extraBuf1;

	full.x = full.y = 0;
	full.w = screenW;
	full.h = screenH;
	half = full;
	half.w = screenW/2;

	vif1SetZTest(0);
	vif1SetZWrite(0);

	tw = GetTexExponent(screenW);
	th = GetTexExponent(screenH);
	/* the real reads the save as PSMCT24 - alpha comes from the init
	 * TEXA{127,AEM,129}, not stored bits */
	vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(src, screenW/64, SCE_GS_PSMCT24,
			tw, th, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	vif1SetAlphaBlend(1, 2, fix);
	vif1SetTexRect(&full, &half, &gray, 1, 0xFFFFFF);

	vif1SetZWrite(1);
	vif1SetZTest(1);
#endif
}

/* real: sub_2144c0 (0x2144c0) - the fly-up motion blur: n feedback
 * passes bouncing the frame between the screen and the cube working
 * buffer (extraBuf2/0x279f10): screen -> buffer squeezed to 7/8 (minus
 * i*(n-1) more pixels, anchored at the XYOFFSET(0) origin), buffer ->
 * screen stretched back.  Blend is PABE-gated (vif1SetAlphaBlend(0,0,0):
 * pixels with the framebuffer-alpha MSB - the FBA-marked cube pixels -
 * keep Cd, all others take the copy).  The real screen TBP comes from
 * the frame parity (0 -> w*h/64 blocks, else 0); the port uses its
 * draw-env idiom.  n=0 (the illegal text's call) only sets up and
 * restores state. */
static void
sub_2144c0(int n, int frame, int field)
{
	Rect full, shrink;
	Color gray = { 128, 128, 128, 128 };	/* real: 0x2a42d8 */
	u32 screenTbp;
	int i, tw, th;

	full.x = full.y = 0;
	full.w = screenW;
	full.h = screenH;
	screenTbp = (frame == 0 ? &db.draw0 : &db.draw1)->frame1.FBP*32;
	tw = GetTexExponent(screenW);	/* real: hardcoded 10/8 in TEX0 */
	th = GetTexExponent(screenH);

	vif1SetXYOffset(0, field);
	vif1SetZWrite(0);
	vif1SetZTest(0);
	vif1SetAlphaBlend(0, 0, 0);
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	for(i = 0; i < n; i++) {
		shrink.x = shrink.y = 0;
		shrink.w = screenW*7/8 - 1 - i*(n-1);
		shrink.h = screenH*7/8 - 1 - i*(n-1);
		/* screen -> working buffer, squeezed */
		vif1SetFramebuffer(extraBuf2/32, SCE_GS_PSMCT32, screenW, screenH, 1);
		vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(screenTbp, screenW/64, SCE_GS_PSMCT32,
				tw, th, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
		vif1SetTexRect(&shrink, &full, &gray, 0, 0xFFFFFF);
		/* working buffer -> screen, stretched back out */
		vif1SetFramebuffer(screenTbp/32, SCE_GS_PSMCT32, screenW, screenH, 1);
		vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(extraBuf2, screenW/64, SCE_GS_PSMCT32,
				tw, th, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
		vif1SetTexRect(&full, &shrink, &gray, 0, 0xFFFFFF);
	}
	vif1SetZTest(1);
	vif1SetZWrite(1);
	vif1SetXYOffset(1, field);
}

/* real: sub_218b20 (0x218b20) - drive the fly-up motion blur: strength
 * (z-56)/12 clamped to 3. */
static void
sub_218b20(void)
{
	int n;

	n = 0;
	if(position[2] > 56.0f) {
		n = (position[2] - 56.0f)/12.0f;
		if(n > 3) n = 3;
		if(n < 0) n = 0;
	}
	if(n != 0)
		sub_2144c0(n, stableEvenOddFrame, stableEvenOddField);
}

/* real: DrawSomeSprite2 (0x214918) - IDA's name; really the full-screen
 * fade overlay: a screen-size flat rect ('B' = black, 'W' = white -
 * colour block 0x2a4308, rect template 0x2a42f8) source-alpha-blended
 * at the given alpha, clamped to 128. */
static void
DrawSomeSprite2(const char *mode, int alpha)
{
	Rect r;
	Color col;

	r.x = r.y = 0;
	r.w = screenW;
	r.h = screenH;
	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetAlphaBlend(1, 4, 0);
	if(alpha > 128)
		alpha = 128;
	if(mode[0] == 'B') {
		col.r = col.g = col.b = 0;
		col.a = alpha;
		vif1SetFlatRect(&r, &col, 1, 0xFFFFFF);
	} else if(mode[0] == 'W') {
		col.r = col.g = col.b = 255;
		col.a = alpha;
		vif1SetFlatRect(&r, &col, 1, 0xFFFFFF);
	}
	vif1SetZWrite(1);
	vif1SetZTest(1);
}

/* real: sub_218bd0 (0x218bd0) - the opening's fade to black (string
 * "B" at 0x2a77c8): full cover for the first two frames (boot-in),
 * fade out at (z-72)*4 during the fly-up, full black from z 320.
 * (0x25a368 in the real code is just float->int, not a callee of
 * interest.) */
static void
sub_218bd0(void)
{
	if(frameCount < 2)
		DrawSomeSprite2("B", 128);
	if(position[2] > 72.0f)
		DrawSomeSprite2("B", (position[2] - 72.0f)*128.0f*0.03125f);
	if(position[2] >= 320.0f)
		DrawSomeSprite2("B", 128);
}

static void
DrawOpeningScene(void)
{
	/* real OSDSYS order: DrawTowers -> DrawExtraBuf2 -> DrawToExtraBuf2 ->
	 * DrawFog -> DrawLightsAndCubes -> sub_218b20 -> sub_218bd0
	 * (docs/towers-analysis.md). Towers must come before fog/lights:
	 * fog's blend is additive (can only brighten, never erase), so
	 * drawing towers last would silently overpaint fog and light glow
	 * already drawn that frame. */

	DrawTowers();

	DrawExtraBuf2(80);
	DrawToExtraBuf2();

	DrawFog();

	DrawLightsAndCubes();

	/* NOT the SCE text (that's DoText, called from DrawEnd) - these two
	 * look like a dormant debug overlay: sub_218b20 wraps the
	 * sub_2144c0 blit, sub_218bd0 wraps DrawSomeSprite2 ('B'/'W'
	 * letter sprites) + a float formatter. */
	sub_218b20();
	sub_218bd0();
}

static void
DoOpening(void)
{
	switch(sceneState) {
	case 0:
		InitOpeningScene();
		sceneState++;
		// fall through
	case 1:
		DrawOpeningScene();
		break;
	case 2:
		nextOpeningType = 2;
		sceneState = 0;
		break;
	}
}

static float redFlareIntensity;	/* real: 0x2a7f98 */
static int illegalSceneWarm;	/* real: 0x2a77e4, cleared by sub_219f08 */
static int illegalFadeCounter;	/* real: 0x2a77e8 */

/* ==== the red flare: 7 spinning glow discs orbiting a centre at the
 * top of the illegal scene (height 1160 - what the camera climbs
 * toward), each drawn as two additive tri-fans (a small bright core
 * and a large dim halo, coloured centre fading to black rim), plus
 * billboard lens sprites (FLAR texture) at the innermost disc. ==== */
static sceVu0FVECTOR flareRingSmall[7][17];	/* real: 0x327e00 - centre + 16-gon r 8 */
static sceVu0FVECTOR flareRingLarge[7][17];	/* real: 0x328570 - r 28+i*8 */
static sceVu0FVECTOR flareRot[7];		/* real: 0x328d20 - spin angles */
static sceVu0FVECTOR flarePos[7];		/* real: 0x328d90 - disc centres */
/* real: 0x328ce0/0x328d00, filled by sub_21a438: {centre r,g,b, ?,
 * rim r,g,b, 128}, scaled by the flare intensity /64 at draw time */
static int flareColorSmall[8] = { 128, 16, 40, 2056, 0, 0, 0, 128 };
static int flareColorLarge[8] = { 48, 8, 12, 128, 0, 0, 0, 128 };

/* real: flare_21A6D8 (0x21a6d8) - per-frame update: spin each disc
 * (rates {0.2, 0.27, 0.35}*(i+1) on x/y/z, wrapped to +-pi) and orbit
 * its centre with radius/phase (7-i)^2*pi/32 offset by a 201-frame
 * global cycle. */
static void
flare_21A6D8(void)
{
	float phase, f, r;
	int i;

	phase = (float)(frameCount % 201)*0.03125f - PI;
	for(i = 0; i < 7; i++) {
		flareRot[i][0] += (i+1)*0.2f;	/* real: 0x2a7210 */
		flareRot[i][2] += (i+1)*0.35f;	/* real: 0x2a7214 */
		flareRot[i][1] += (i+1)*0.27f;	/* real: 0x2a7218 */
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

/* real: 0x219fb0 - one billboard lens sprite: project disc 0's centre
 * (with its own small 32-frame wobble written into flarePos[0]), pull
 * it toward the screen centre by 'scale', draw a size*2 x size rect of
 * the FLAR texture, additive with FIX alpha.  (The real leaves the
 * rect colour's alpha byte uninitialized - FIX blending ignores it.) */
static void
DrawFlareSprite(int size, int alpha, int r, int g, int b, float scale)
{
	Rect rect, uv;
	Color col;
	float ph, q;
	float *v;
	int x, y;

	vif1SetAlphaBlend(1, 0, alpha);
	ph = (float)(49 + (frameCount & 0x1f))*0.1f;
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
	x = (int)((v[0]*q - 2048.0f)*scale + screenW/2);
	y = (int)((v[1]*q - 2048.0f)*scale + screenH/2);
	rect.x = x - size;
	rect.y = y - size/2;
	rect.w = size*2;
	rect.h = size;
	uv.x = uv.y = 0;
	uv.w = uv.h = 128;
	col.r = r;
	col.g = g;
	col.b = b;
	col.a = 128;
	vif1SetTexRect(&rect, &uv, &col, 1, 0xFFFFFF);
}

/* real: FlareThing (0x21a300) - IDA's name: the lens-flare sprite
 * stack: four dim haloes (sizes 112/170/256/448, reddish) and the big
 * near-centred glow (size ~420+, alpha up to 255, scale 0.1). */
static void
FlareThing(int n)
{
	int m, a, size;

	if(n > 8)
		n = 8;
	vif1SetTexture(&textures[TEXID_FLAR]);
	m = n + 2;
	DrawFlareSprite(112, m, 128, 64, 64, 1.0f);
	DrawFlareSprite(170, m, 128, 64, 64, 1.0f);
	DrawFlareSprite(256, m, 128, 64, 64, 1.0f);
	DrawFlareSprite(448, m, 128, 64, 64, 1.0f);
	a = n*3 + (int)(redFlareIntensity*0.5f);
	if(a > 255) a = 255;
	if(a < 0) a = 0;
	size = redFlareIntensity*0.125f + 420.0f;
	/* real scale is 0.9f (.lit4 at 0x2a7200) - 0.1f was a misread that
	 * check.py's reloc masking couldn't catch */
	DrawFlareSprite(size, a, 128, 112, 96, 0.9f);
}

/* the shared body of flare_21AA50/flare_21AF18: 7 tri-fan discs
 * (centre vertex in the centre colour, 16 rim vertices + closing wrap
 * in the rim colour - black, so the discs fade out radially), additive
 * FIX 128, colours scaled by intensity/64 and clamped to 255. */
static void
DrawFlareFans(sceVu0FVECTOR rings[7][17], int *colblk)
{
	int center[3], rim[3];
	int i, k, c, vi;
	float q;
	u32 *v;

	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 0, 128);
	for(c = 0; c < 3; c++) {
		center[c] = colblk[c]*redFlareIntensity*0.015625f;
		if(center[c] > 255) center[c] = 255;
		if(center[c] < 0) center[c] = 0;
		rim[c] = colblk[4+c]*redFlareIntensity*0.015625f;
		if(rim[c] > 255) rim[c] = 255;
		if(rim[c] < 0) rim[c] = 0;
	}
	for(i = 0; i < 7; i++) {
		vif1Begin();
		sceVu0RotMatrix(sprMatrices->m9, sprMatrices->unit, flareRot[i]);
		sceVu0TransMatrix(sprMatrices->worldMatrix, sprMatrices->m9, flarePos[i]);
		sceVu0MulMatrix(sprMatrices->worldScreenMatrix,
			sprMatrices->cameraScreenMatrix, sprMatrices->worldMatrix);
		if(sceVu0ClipAll(clipMin, clipMax, sprMatrices->worldScreenMatrix, rings[i], 17)) {
			vif1End();
			continue;
		}
		pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRIFAN, 1, 0, 0, 1, 0, 1, 0, 0));
		for(k = 0; k <= 17; k++) {
			int *col = k == 0 ? center : rim;
			vi = k == 17 ? 1 : k;
			q = sprTransformVertex(sprVertices->verts2[2], rings[i][vi],
				sprMatrices->worldScreenMatrix);
			v = (u32*)sprVertices->verts2[2];
			pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2], 128, *(u32*)&q));
			pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
		}
		vif1End();
	}
	/* Z-test restore is the callers' (the sprites draw with it off) */
}

/* real: flare_21AA50 - the small bright cores */
static void
flare_21AA50(void)
{
	DrawFlareFans(flareRingSmall, flareColorSmall);
	vif1SetZTest(1);
}

/* real: flare_21AF18 - the large dim haloes, then the lens sprites at
 * intensity/16 */
static void
flare_21AF18(void)
{
	DrawFlareFans(flareRingLarge, flareColorLarge);
	FlareThing((int)(redFlareIntensity*0.0625f));
	vif1SetZTest(1);
}

/* the illegal-disc fog: 128 red cloud particles, each an instance of
 * one shared 3x3-vertex patch (30x30 units, only the centre vertex
 * coloured - a soft blob), streaming down past the rising camera and
 * respawning 805 above it.  Initialized by sub_215798. */
static sceVu0FVECTOR illegalFogVerts[9];	/* real: 0x3227d0 */
static sceVu0IVECTOR illegalFogColors[9];	/* real: 0x322860 */
static sceVu0FVECTOR illegalFogPos[128];	/* real: 0x3228f0 */
static sceVu0FVECTOR illegalFogRot[128];	/* real: 0x3230f0 */
static int illegalFogState[128];		/* real: 0x3238f0, zeroed in
						 * sub_215798, so far unseen
						 * elsewhere */

/* real: 0x27a050 - the patch's 4 tri-strips over the 3x3 vertices */
static int fogStrips[4][4] = {
	{ 0, 1, 3, 4 }, { 1, 2, 4, 5 }, { 3, 4, 6, 7 }, { 4, 5, 7, 8 }
};
/* real: 0x27a090 */
static float fogUV[9][2] = {
	{ 0.0f, 0.0f }, { 0.5f, 0.0f }, { 1.0f, 0.0f },
	{ 0.0f, 0.5f }, { 0.5f, 0.5f }, { 1.0f, 0.5f },
	{ 0.0f, 1.0f }, { 0.5f, 1.0f }, { 1.0f, 1.0f }
};

/* real: DrawIllegalFog (0x215a20).  Per particle and frame: jitter the
 * roll by 0.0001*(i+1) (sign flips with the frame parity), fall
 * trunc((i+1)*0.02 + 1.2) units, fade in over the first 192 units
 * below the spawn plane and out over the last 192 above the camera
 * (peak alpha 64), respawn 805 above the camera once fallen 32 below
 * it.  Additive blend, FOG0 texture. */
static void
DrawIllegalFog(void)
{
	float globalAlpha, af, d, rj, q, s, t;
	int i, j, k, vi, alpha;
	int *col;

	/* global fade-in below height 672 (the scene starts there, so
	 * full in practice; real constant 550 at 0x2a7098) */
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
		while(illegalFogRot[i][2] > PI)
			illegalFogRot[i][2] -= TAU;
		while(illegalFogRot[i][2] < -PI)
			illegalFogRot[i][2] += TAU;

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

		vif1Begin();
		pktSetAlphaBlend(1, 0, alpha);
		for(j = 0; j < 4; j++) {
			pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, 1, 0, 1, 0, 0, 0, 0));
			for(k = 0; k < 4; k++) {
				vi = fogStrips[j][k];
				q = sprTransformVertex(sprVertices->verts2[0], illegalFogVerts[vi], sprMatrices->worldScreenMatrix);
				col = illegalFogColors[vi];
				pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2], 128, *(u32*)&q));
				s = fogUV[vi][0]*q;
				t = fogUV[vi][1]*q;
				pktSetAD(SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&s, *(u32*)&t));
				pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(sprVertices->verts2[0][0],
					sprVertices->verts2[0][1], sprVertices->verts2[0][2], 0));
			}
		}
		vif1End();
	}
	vif1SetZWrite(1);
	vif1SetZTest(1);
}

/* real: DrawIllegalCube (0x219748) - DrawCube with the illegal pass
 * stack, Z-WRITE ON (the opening sets the interlace XYOffset here
 * instead), and no per-cube Z-test restore (DrawIllegalCubes does it
 * once after the loop). */
static void
DrawIllegalCube(int instance)
{
	int k;

	if(CubeTransformAndClip(instance))
		return;

	CubeCaptureBuffer();

	vif1SetZWrite(1);
	vif1SetZTest(0);

	illegalCubeDraw = 1;
	for(k = 0; k < 10; k++)
		DrawCubePass(&illegalCubePasses[k]);
	illegalCubeDraw = 0;
}

/* real: DrawIllegalCubes (0x219ea8) - 5 cube instances with Z-test off
 * (no DrawLights, no CLAMP change, unlike the opening's cube pass). */
static void
DrawIllegalCubes(void)
{
	int i;

	sceVu0Normalize(sprVertices->verts1[3], fwdDir);
	vif1SetZTest(0);
	for(i = 0; i < 5; i++)
		DrawIllegalCube(i);
	vif1SetZTest(1);
}

/* real: DrawRedFlare (0x21b4c8) - flare intensity from the camera
 * height ((z-420)*128/740 * 0.6, constant at 0x2a7258), the three
 * flare passes with Z-write off, then the same frame-feedback
 * composite as the opening but at blend fix 112. */
static void
DrawRedFlare(void)
{
	redFlareIntensity = (740.0f - (1160.0f - position[2]))*128.0f/740.0f*0.6f;
	vif1SetZWrite(0);
	flare_21A6D8();
	flare_21AA50();
	flare_21AF18();
	vif1SetZWrite(1);
	DrawExtraBuf2(112);	/* real: (1, 2, 112, 0xffffff, 128, field) */
	DrawToExtraBuf2();
}

/* real: 0x21b398 (tail of DrawIllegalScene) - the scene's screen fades
 * via the DrawSomeSprite2 black overlay: fade from black rising
 * 672->800, fade to black 1128->1160, and a +1/frame fade-out once
 * openingEndFlag is set. */
static void
DrawIllegalFades(void)
{
	float z, f;

	z = position[2];
	if(z < 800.0f) {
		DrawSomeSprite2("B", 128 - (int)(z - 672.0f));
		illegalFadeCounter = 0;
	} else if(z > 1128.0f) {
		f = (32.0f - (1160.0f - z))*4.0f;
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

/* real: InitIllegalScene (0x21b570, arg fooOpeningType unused) -
 * camera and lights for the red scene, then prime the frame-save
 * buffer.  InitIllegalDisc puts the camera at height 672 (the 320
 * written first is overridden, real does the same). */
static void
InitIllegalScene(void)
{
	position[0] = 0.0f;
	position[1] = 0.0f;
	position[2] = 320.0f;
	position[3] = 0.0f;
	fwdDir[0] = 0.0f;
	fwdDir[1] = -0.03f;	/* real: *(0x2a725c) */
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

/* real: DrawIllegalScene (0x21b648) - skip the very first frame (lets
 * the init's buffer stamp settle), then flare, red fog, cubes, fades. */
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
	DrawIllegalFades();
}

static void
DoIllegalDisc(void)
{
	switch(sceneState) {
	case 0:
		InitIllegalScene();	/* real arg (fooOpeningType) is unused */
		sceneState++;
		// fall through
	case 1:
		DrawIllegalScene();
		break;
	case 2:
		nextOpeningType = 2;
		sceneState = 0;
		break;
	}
}

/* "Sony Computer Entertainment" overlay state (real: 0x2a7f84 state,
 * 0x2a7f88 alpha, 0x2a7788 step) and the illegal-disc text counterpart
 * (0x2a7f8c state, 0x2a7f90 alpha).  state: -1 disarmed, 0 armed,
 * 1 fading.  initTextShit arms one of the two per opening type. */
static int sceTextState;
static int sceTextAlpha;
static int sceTextStep;
static int illegalTextState;
static int illegalTextAlpha;

/* real: 0x2a4318 - screen rectangles for the two text strips,
 * NTSC and PAL rows */
static Rect sceTextRect[2][2] = {
	{ { 120, 105, 256, 16 }, { 326, 105, 256, 16 } },
	{ { 120, 120, 256, 18 }, { 326, 120, 256, 18 } }
};

/* real: DrawSCEText (0x214a60, args (0, 0, alpha) - the first two are
 * unused) - the "Sony Computer Entertainment" text: the 256x64 SCE
 * texture holds the logo as two 256-texel strips (rows 1-31 = left
 * half, 33-63 = right half), drawn side by side around the screen
 * centre, blended by FIX alpha. */
static void
DrawSCEText(int alpha)
{
	Rect xy, uv;
	Color col;
	int pal;

	pal = IsPAL();
	uv.x = 0;	/* real: 0x2a4568 */
	uv.y = 1;
	uv.w = 256;
	uv.h = 30;
	col.r = col.g = col.b = 128;
	col.a = alpha;
	vif1SetXYOffset(1, stableEvenOddField);
	vif1SetTexture(&textures[TEXID_SCE]);
	xy = sceTextRect[pal][0];
	vif1Begin();
	pktSetAlphaBlend(1, 4, alpha);
	pktSetTexRect(&xy, &uv, &col, 1, 0xFFFFFE);
	xy = sceTextRect[pal][1];
	uv.y += 32;
	pktSetTexRect(&xy, &uv, &col, 1, 0xFFFFFE);
	vif1End();
}

/* real: DoSCEText (0x214c20) - arm once the camera has climbed past
 * height 18, then ramp alpha 0->240->0 in steps of 4, drawing at
 * min(alpha, 112): fade in, hold, fade out, disarm. */
static void
DoSCEText(void)
{
	int alpha;

	if(position[2] > 18.0f)
		if(sceTextState == 0)
			sceTextState = 1;
	if(sceTextState != 1)
		return;
	alpha = sceTextAlpha + sceTextStep;
	sceTextAlpha = alpha;
	if(alpha == 240)
		sceTextStep = -4;
	if(alpha == 0) {
		sceTextStep = 4;
		sceTextState = -1;
	}
	DrawSCEText(min(alpha, 112));
}

/* real: DrawIllegalText (0x214cb0, args (0,0,0,alpha)) - the localized
 * "Please insert a PlayStation or PlayStation 2 format disc" texture
 * (TEXOPNG* by GetLanguage()), additive blend (mode 5) with RGB =
 * fade level, at {64,88,512,64} (real: 0x2a4578), uv {0,0,512,128}
 * (0x2a4588); PAL scales y/h by the screenAY ratio (real doubles at
 * 0x2a4598/0x2a45a0). */
static void
DrawIllegalText(int alpha)
{
	Rect xy, uv;
	Color col;

	xy.x = 64;
	xy.y = 88;
	xy.w = 512;
	xy.h = 64;
	uv.x = 0;
	uv.y = 0;
	uv.w = 512;
	uv.h = 128;
	col.r = col.g = col.b = alpha;
	col.a = 128;
	if(openingType != 1)
		return;
	vif1SetXYOffset(1, stableEvenOddField);
	vif1SetTexture(&textures[TEXID_PNG + GetLanguage()]);
	if(IsPAL()) {
		xy.y = xy.y * 0.52627105 / 0.457627;
		xy.h = xy.h * 0.52627105 / 0.457627;
	}
	vif1SetAlphaBlend(1, 5, alpha);
	vif1Begin();
	pktSetTexRect(&xy, &uv, &col, 1, 0xFFFFFF);
	vif1End();
}

/* real: DoIllegalText (0x214e60) - arm past height 800; the fade level
 * ramps +1/frame, then -1/frame once openingEndFlag is set, drawn
 * clamped to 112.  Also ticks the n=0 sub_2144c0 state churn.  Inert
 * in the normal opening (initTextShit leaves it disarmed). */
static void
DoIllegalText(void)
{
	int alpha;

	if(position[2] > 800.0f)
		if(illegalTextState == 0)
			illegalTextState = 1;
	if(illegalTextState != 1)
		return;
	sub_2144c0(0, frameCount & 1, stableEvenOddField);
	alpha = illegalTextAlpha;
	if(openingEndFlag == 0)
		illegalTextAlpha = ++alpha;
	else if(alpha > 0)
		illegalTextAlpha = --alpha;
	alpha = min(alpha, 112);
	illegalTextAlpha = alpha;
	DrawIllegalText(alpha);
}

/* real: DoText (0x214f58) */
static void
DoText(void)
{
	DoSCEText();
	DoIllegalText();
}

/* real: 0x27a0d8 - camera heights at which openingState advances */
static int openingStateLevels[8] = { 16, 56, 104, 320, 672, 800, 1160, 1160 };

/* real: ProcessOpeningAnimation (0x215fd0) - the opening's camera state
 * machine + motion integration.  openingState advances when the camera
 * passes openingStateLevels[openingState] (the handlers also advance it
 * directly); the function returns the opening type the scene should
 * switch to (ProcessOpening turns a change into a sceneState bump,
 * which ends the current scene).  States:
 *   0    drift across the field (initial speed from InitAnimation)
 *   1    (z>16) disc present: adjust accel, go once the disc type is
 *        known; no disc: creep and go after 2 seconds
 *   2    (z>56, usually entered by state 1's go) dispatch the boot
 *        messages once (bootRequest) and apply the fly-up acceleration
 *   3    (z>104) request the next opening type (= end this scene)
 *   4,5  cruise (the illegal scene enters at state 4: InitIllegalDisc)
 *   6    (z>800) kill all motion; once the end condition holds,
 *        request type 2 128 frames later
 *   7    (z>1160) reset the animation, request type 2 */
static int
ProcessOpeningAnimation(void)
{
	float timestep;
	int type;
	int fps;

	timestep = IsPAL() ? 1.2f : 1.0f;
	fps = IsPAL() ? 50 : 60;

	if((float)openingStateLevels[openingState] < position[2])
		openingState++;
	type = openingType;

	switch(openingState) {
	case 1:
		if(HasDisc()) {
			rotationSpeed[2] = 0.0004f;
			if(frameCount < fps*20/6)
				positionAccel2[2] = -0.00014f;
			else
				positionAccel2[2] = 0.000025f;
			if(GetDiscType() != 0) {
				positionAccel2[2] = 0.003f;
				openingGo = 1;
				openingState++;
			} else if(frameCount > fps*20)
				discReady = 0;	/* give up on the disc */
		} else {
			positionAccel1[2] = 0.0000004f;
			switch(osdBootParam) {
			case 100: case 106: case 107: case 108: case 109:
			case 110: case 111: case 112: case 114: case 115:
			case 116:
				if(frameCount > fps*2) {
					openingGo = 1;
					openingState++;
				}
				break;
			}
		}
		break;
	case 2:
		if(frameCount > fps*10)
			openingGo = 1;
		if(openingGo) {
			if(bootRequest == 1) {
				if(BootLatchClear())
					OSDDispatch(20500, 1, 0, 0);
				else if(HasDisc() && GetDiscType() == 1)
					OSDDispatch(20501, 0, 0, 15);
				else {
					lastBootParam = osdBootParam;
					switch(osdBootParam) {
					case 106: case 107: case 114: case 115:
						OSDDispatch(20501, 0, 0, 15);
						break;
					case 108: case 109: case 110:
						OSDDispatch(20500, 7, 0, 0);
						OSDDispatch(20501, 0, 0, 17);
						break;
					default:  /* 111-113 and out of range */
						OSDDispatch(20500, 1, 0, 0);
						break;
					}
				}
				bootRequest = -1;
			}
			if(HasDisc() || GetDiscType() != 0) {
				positionAccel1[2] = 0.0004f;
				rotationAccel[2] = 0.00008f;
			} else {
				positionAccel2[2] = 0.0099f;
				rotationAccel[2] = 0.000195f;
			}
			positionAccel2[0] = 0.0f;
			positionAccel2[1] = 0.0f;
			rotationAccel[0] = 0.0f;
			rotationAccel[1] = 0.0f;
		}
		break;
	case 3:
		type++;
		break;
	case 6:
		positionAccel1[0] = positionAccel1[1] = positionAccel1[2] = 0.0f;
		positionAccel2[0] = positionAccel2[1] = positionAccel2[2] = 0.0f;
		positionSpeed[0] = positionSpeed[1] = positionSpeed[2] = 0.0f;
		rotationAccel[0] = rotationAccel[1] = rotationAccel[2] = 0.0f;
		if(osdBootParamC == 0) {
			lastBootParam = osdBootParam;
			switch(osdBootParam) {
			case 100: case 106: case 107: case 108: case 109:
			case 110: case 111: case 112: case 115:
				openingEndFlag = 1;
				if(openingEndFrame == 0) {
					OSDDispatch2(1, 20501, 6, 0, 15);
					openingEndFrame = frameCount;
				} else if(frameCount > openingEndFrame+128)
					type = 2;
				break;
			case 114:
				if(osdBootParam2 > 0) {
					openingEndFlag = 1;
					if(openingEndFrame == 0)
						openingEndFrame = frameCount;
					else if(frameCount > openingEndFrame+128)
						type = 2;
				}
				break;
			default:  /* 101-105, 113, 116 and out of range */
				if(openingEndFlag && frameCount > openingEndFrame+128)
					type = 2;
				break;
			}
		}
		break;
	case 7:
		InitAnimation();
		type = 2;
		break;
	}

	rotationSpeed[0] += (2.0f*rotationAccel[0] + 0.0f)*0.5f*timestep;
	rotationSpeed[1] += (2.0f*rotationAccel[1] + 0.0f)*0.5f*timestep;
	rotationSpeed[2] += (2.0f*rotationAccel[2] + 0.0f)*0.5f*timestep;

	positionSpeed[0] += (2.0f*positionAccel2[0] + positionAccel1[0])*0.5f*timestep;
	positionSpeed[1] += (2.0f*positionAccel2[1] + positionAccel1[1])*0.5f*timestep;
	positionSpeed[2] += (2.0f*positionAccel2[2] + positionAccel1[2])*0.5f*timestep;

	positionAccel2[2] += positionAccel1[2]*timestep;

	rotation += (2.0f*rotationSpeed[2] + rotationAccel[2])*0.5f*timestep;

	position[0] += (2.0f*positionSpeed[0] + positionAccel2[0])*0.5f*timestep;
	position[1] += (2.0f*positionSpeed[1] + positionAccel2[1])*0.5f*timestep;
	position[2] += (2.0f*positionSpeed[2] + positionAccel2[2])*0.5f*timestep;

	if(rotation > PI) rotation -= TAU;
	if(rotation < -PI) rotation += TAU;

	upDir[0] = sinf(rotation);
	upDir[1] = cosf(rotation);

	sceVu0NormalLightMatrix(sprMatrices->normalLightMatrix, light1, light2, light3);
	sceVu0CameraMatrix(sprMatrices->cameraMatrix, position, fwdDir, upDir);
	sceVu0ViewScreenMatrix(sprMatrices->viewScreenMatrix, 1024.0f,
		screenAX, screenAY, 2048.0f, 2048.0f,
		1.0f, 16777215.0f, 1.0f, 65536.0f);
	sceVu0MulMatrix(sprMatrices->cameraScreenMatrix,
		sprMatrices->viewScreenMatrix, sprMatrices->cameraMatrix);

	return type;
}

/* real: ProcessOpening (0x211e38) - run the state machine, turn a
 * requested type change into a sceneState bump (DoOpening/DoIllegalDisc
 * end their scene from case 2), and set the frame's one-pixel-inset
 * scissor. */
static void
ProcessOpening(void)
{
	int type;

	stableEvenOddFrame = evenOddFrame;
	stableEvenOddField = evenOddField;

	type = ProcessOpeningAnimation();
	if(type != openingType)
		sceneState++;
	vif1SetSCISSOR_1(1, screenW-2, 1, screenH-2);
}

/* real: DrawBlackBars (0x214790) - letterbox bars for the "Screen Size:
 * Letterbox" system config: compute the 16:9 image height from the
 * aspect factors and draw white rects with subtractive blend (Cd - Cs,
 * mode 1) over the top and bottom - i.e. black bars.  Colour is the
 * real qword at 0x2a42e8. */
static void
DrawBlackBars(void)
{
	Rect r1, r2;
	Color col = { 255, 255, 255, 128 };
	int imgh, bar;

	imgh = (float)screenW * 9.0f * screenAY / (screenAX * 16.0f);
	bar = (screenH - imgh + 1)/2;
	r1.x = 0; r1.y = 0;
	r1.w = screenW; r1.h = bar;
	r2.x = 0; r2.y = bar + imgh;
	r2.w = screenW; r2.h = bar;

	vif1SetXYOffset(1, stableEvenOddField);
	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetAlphaBlend(1, 1, 128);
	vif1SetFlatRect(&r1, &col, 1, 0xFFFFFF);
	vif1SetFlatRect(&r2, &col, 1, 0xFFFFFF);
	vif1SetZWrite(1);
	vif1SetZTest(1);
}

/* real: DrawEnd equivalent at 0x211eb8 */
static void
DrawEnd(void)
{
	DoText();
	if(drawBlackBars)
		DrawBlackBars();
	WaitNextFrame();
	frameCount++;
	if(hwFrameLimit > 0 && frameCount >= hwFrameLimit) {
		sceGsSyncPath(0, 0);
		printf("hw frame limit %d reached, exiting\n", hwFrameLimit);
		Exit(0);
	}
}

static void
DoOpeningIllegal(void)
{
	sceneState = 0;
	while(openingType != 2) {
		ProcessOpening();
		switch(openingType) {
		case 0:
			DoOpening();
			break;
		case 1:
			DoIllegalDisc();
			break;
		}
		DrawEnd();
		openingType = nextOpeningType;
	}
}

/* ==== towers: faithful port of the real OSDSYS VU1 tower pipeline ====
 *
 * The real OSDSYS draws the tower field through VU1 microcode: per
 * tower, the EE patches fields in a DMA chain (UNPACK source address,
 * parameters) and kicks a VIF1 chain-mode transfer.  The tower field
 * layout itself comes from the memory card boot history (21 x 22-byte
 * entries) - osdbits has no memcard, so the history is either a
 * captured real one or simulated with the real updater's rules (see
 * GenerateTowerField).
 *
 * The VU1 microcode + per-tower chain are reconstructed as real dvp-as
 * source (towerchain.dsm, with vucode_1.vsm for the microcode),
 * assembled and diffed byte-for-byte against the real ROM to confirm
 * the reconstruction is exact (docs/towers-analysis.md).  This gives
 * named, linker-relocated symbols for every real patch point; the
 * linker's relocations (R_MIPS_DVP_27_S4 on the chain tags) handle the
 * absolute addresses.  The patch points below are extern symbols
 * defined in towerchain.dsm. */
extern u32 TowerUpload  __attribute__((section(".vudata")));
extern u32 TowerChain   __attribute__((section(".vudata")));
extern u32 TowerView    __attribute__((section(".vudata")));
extern u32 TowerModel   __attribute__((section(".vudata")));
extern u32 TowerLightMatrix __attribute__((section(".vudata")));
extern u32 TowerGifTag  __attribute__((section(".vudata")));
extern u32 TowerBlock0  __attribute__((section(".vudata")));
extern u32 TowerBlock1  __attribute__((section(".vudata")));
extern u32 TowerBlock2  __attribute__((section(".vudata")));
extern u32 TowerBlock3  __attribute__((section(".vudata")));
extern u32 TowerBlock4  __attribute__((section(".vudata")));
extern u32 TowerBlock5  __attribute__((section(".vudata")));

/* one of the 6 quad packets of a tower - the 17-qword unpack blocks in
 * the chain at 0x2681a0: GIF tag, 4 vertices, 4 normals, 4 colours,
 * 4 st.  The real patchers (tower_2182bc/2183e0/21850c) address these
 * as byte offsets off the block pointer (+0x00/+0x10/+0x90/+0xd0);
 * this struct is that same layout with names. */
typedef struct {
	u32   tag[4];		/* qword 0:      GIF tag */
	float vert[4][4];	/* qwords 1-4:   x y z w */
	float norm[4][4];	/* qwords 5-8:   normals (static) */
	float colour[4][4];	/* qwords 9-12:  r g b a (floats) */
	float st[4][4];		/* qwords 13-16: s t q 0 */
} TowerPacket;

/* the 6 per-tower packet blocks, in real memory order (matches the real
 * static pointer table at 0x27a650) */
static u32 *const towerBlocks[6] = {
	&TowerBlock5, &TowerBlock1, &TowerBlock2, &TowerBlock3, &TowerBlock4, &TowerBlock0
};

/* sendDma - kick a VIF1 chain-mode transfer at addr.  The real ONE-TIME
 * vucode_1 upload sequence in OpeningInitTowersFog (0x218ff0-0x219044):
 * QWC=0; TADR=addr&0x0fffffff; *(0x1000e010)=2; FlushCache(0);
 * CHCR=325.  This exact sequence is real ONLY for the one-time upload -
 * the per-tower kick is a DIFFERENT real sequence, see towerKick(). */
static void
sendDma(void *addr)
{
	*D1_QWC = 0;
	*D1_TADR = (u32)addr & 0x0fffffff;
	*(volatile u32*)0x1000e010 = 2;
	FlushCache(0);
	*D1_CHCR = 325;		/* chain mode start */
}

static void
vu1Wait(void)
{
	/* real 0x266b08 = sceDmaSync, called right after the one-time
	 * upload's kick; this manual poll of the DMA-start bit stands in
	 * for that specific call.  Bounded so a wedged DMA can't hang the
	 * frame.  The real OSDSYS follows it with a further
	 * sceGsSyncPath(0,0) - done explicitly at the call site. */
	int i;
	for(i = 0; i < 1<<20; i++) {
		if(!(*D1_CHCR & 0x100))
			return;
	}
}

/* towerKick - kick the per-tower VIF1 chain.  The real per-tower kick
 * (DrawTowers 0x218a48-0x218a90):
 *   QWC=0; TADR=addr&0x0fffffff; *(0x1000e010)=2; sceGsSyncPath(0,0);
 *   CHCR=325 - NO FlushCache, and NO wait/sync immediately after the
 * kick either: DrawTowers kicks each tower and moves straight on to
 * computing the next one (pipelined, not serialized per-tower), with
 * exactly ONE sceGsSyncPath(0,0) after the ENTIRE r,c loop (0x218ad8) -
 * see towerSyncEnd(). */
static void
towerKick(void *addr)
{
	/* real instruction order (QWC; TADR; clear; sync; kick).  Writing
	 * QWC/TADR while the PREVIOUS kick's chain is still active corrupts
	 * it mid-flight (VIF ends up decoding vertex data as a vifcode ->
	 * ER1, permanent VIF1 stall).  SAFE here ONLY because DrawTowers
	 * drains the paths (sceGsSyncPath) before the patchers run, so the
	 * channel is provably idle by the time we get here - do not call
	 * this without that guarantee. */
	*D1_QWC = 0;
	*D1_TADR = (u32)addr & 0x0fffffff;
	*(volatile u32*)0x1000e010 = 2;
	sceGsSyncPath(0, 0);
	*D1_CHCR = 325;		/* chain mode start */
}

static void
towerSyncEnd(void)
{
	sceGsSyncPath(0, 0);	/* real: 0x218ad8, once after the whole field */
}

/* tower field state (the real OSDSYS BSS arrays) */
static int towerFlags[20][20];
static float towerGrid[20][20];
static float towerPos[14][9][4];
static float towerA[14][9];	/* alphas */
static float towerB[14][9];
static float towerC[14][9];
static float towerD[14][9];

/* height grid (real: sub_217e30): 20x20 floats, two radial bumps */
static void
HeightGrid(void)
{
	/* real sub_217e30 (0x217e30), bit-exact: a two-centre radial
	 * distance field (bright core ~220 over the tower cluster fading
	 * to 32 at the edges) with a per-cell integer-hash dither.  This
	 * grid is the tower-brightness radial falloff: DrawTowers' colour
	 * scale f12 = towerGrid[r+3][c+6]*(B/128 | A/30).  Real constants
	 * (gp-32508..-32488): 5202.0 (sq-arg), 5.1 (scale), 2.55 (offset),
	 * -5.1 / (10.2, 5.1) (the two centres), 0.85 (final scale); centre
	 * 1 weights its distance x2, centre 2 x4 with a further x0.5 on the
	 * value; final clamp [32, 220]. */
	float sq = sqrtf(5202.0f);
	int i, j;
	for(j = 0; j < 20; j++) {
		float gy = (j-10)*5.1f + 2.55f;
		for(i = 0; i < 20; i++) {
			float gx = (i-10)*5.1f + 2.55f;
			float d1, d2, v;
			d1 = 2.0f*sqrtf((-5.1f-gx)*(-5.1f-gx) + gy*gy);
			d2 = 4.0f*sqrtf((10.2f-gx)*(10.2f-gx) + (5.1f-gy)*(5.1f-gy));
			v = clamp(255.0f*(sq - d1)/sq, 32.0f, 255.0f)
			  + clamp(255.0f*(sq - d2)/sq*0.5f, 32.0f, 255.0f);
			v *= 0.85f;
			v -= (float)(10*((((i+j)*i)/(j+1)) % 11 - 5));
			towerGrid[i][j] = clamp(v, 32.0f, 220.0f);
		}
	}
}

/* ==== the memcard boot-history mechanism (real OpeningInitTowersFog
 * 0x218e00, steps 2+4) ====  The towers ARE the boot history: each of
 * the 21 history slots owns a fixed 6-cell neighbourhood; a title's
 * launch count picks its tower size (towerC/D via the count tables),
 * and at count 14, 24, ... the title "sprawls" - a random extra cell of
 * its neighbourhood becomes the new home cell and the old ones persist
 * as maxed C=D=1.0 towers.
 *
 * TOWER_FIELD_CAPTURED: 1 = parse a captured real history (the exact
 * real-BIOS field - use for A/B comparisons); 0 = parse a SIMULATED
 * history generated by the real updater's rules (real updater
 * ~0x2019a0: known title -> count++/date refresh + the sprawl rule;
 * unknown title -> evict lowest count, oldest date; see
 * docs/towers-analysis.md) - realistic varied scenery. */
#define TOWER_FIELD_CAPTURED 0

#if !TOWER_FIELD_CAPTURED
/* private LCG so field generation never touches the opening's osdRand()
 * stream (lightsSeed must stay draw #386, matching the real boot) */
static u32 towerRandState = 0x2b992ddf;
static int
towerRand(void)
{
	towerRandState = towerRandState*1103515245 + 12345;
	return (towerRandState >> 16) & 0x7fff;
}
#endif

/* count->towerC / towerD tables, real 0x27aee0 / 0x27af18 */
static const float histTableC[14] = {
	0.2f, 0.4f, 0.6f, 0.8f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};
static const float histTableD[14] = {
	0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f
};
/* per-history-slot 6-cell neighbourhoods, real 0x27a260 -
 * 21 slots x 6 (r,c) pairs, tiling all 126 grid cells exactly once */
static const u8 histCells[21][6][2] = {
	{ { 0,0},{ 0,1},{ 0,2},{ 0,3},{ 1,0},{ 1,1} },
	{ { 1,2},{ 2,0},{ 2,1},{ 2,2},{ 3,0},{ 4,0} },
	{ { 4,1},{ 5,0},{ 5,1},{ 6,0},{ 6,1},{ 6,2} },
	{ { 7,0},{ 7,1},{ 8,0},{ 8,1},{ 8,2},{ 8,3} },
	{ { 9,0},{ 9,1},{10,0},{10,1},{10,2},{11,0} },
	{ {12,0},{12,1},{13,0},{13,1},{13,2},{13,3} },
	{ { 1,3},{ 2,3},{ 3,1},{ 3,2},{ 3,3},{ 4,2} },
	{ { 5,2},{ 5,3},{ 5,4},{ 6,3},{ 7,2},{ 7,3} },
	{ { 9,2},{ 9,3},{ 9,5},{10,3},{10,4},{10,5} },
	{ {11,1},{11,2},{11,3},{11,4},{12,2},{12,3} },
	{ { 0,4},{ 1,4},{ 2,4},{ 2,5},{ 2,6},{ 3,4} },
	{ { 3,5},{ 3,6},{ 4,3},{ 4,4},{ 4,5},{ 4,6} },
	{ { 5,5},{ 5,6},{ 6,4},{ 6,5},{ 7,4},{ 7,5} },
	{ { 8,4},{ 8,5},{ 8,6},{ 9,4},{ 9,6},{ 9,7} },
	{ {12,4},{13,4},{13,5},{13,6},{13,7},{13,8} },
	{ { 0,5},{ 0,6},{ 1,5},{ 1,6},{ 1,7},{ 2,7} },
	{ { 0,7},{ 0,8},{ 1,8},{ 2,8},{ 3,7},{ 3,8} },
	{ { 4,7},{ 4,8},{ 5,7},{ 5,8},{ 6,6},{ 6,7} },
	{ { 6,8},{ 7,6},{ 7,7},{ 7,8},{ 8,7},{ 8,8} },
	{ { 9,8},{10,6},{10,7},{10,8},{11,7},{11,8} },
	{ {11,5},{11,6},{12,5},{12,6},{12,7},{12,8} },
};
/* static source position table, real 0x27a670 [14][9] (x,y,z) */
static const float histSrcPos[14][9][3] = {
	{ {-13.7428f,11.7512f,-2.5366f},{-13.7428f,10.4512f,-2.5366f},{-13.7428f,9.1512f,-2.5366f},{-13.1428f,7.8512f,-2.5366f},{-13.7428f,6.5512f,-6.5517f},{-13.7428f,5.2512f,-2.5366f},{-13.7428f,3.9512f,-2.5366f},{-13.7428f,2.6512f,-2.5366f},{-13.7428f,1.3512f,-2.5366f} },
	{ {-12.4428f,11.7512f,-2.5366f},{-12.4428f,10.4512f,-2.5366f},{-12.4428f,9.1512f,-2.5366f},{-12.1428f,7.8512f,-0.933f},{-12.4428f,6.5512f,-2.5366f},{-12.4428f,5.2512f,-2.5366f},{-12.4428f,3.9512f,-2.5366f},{-12.4428f,2.6512f,-2.5366f},{-12.4428f,1.3512f,-2.5366f} },
	{ {-11.1428f,11.7512f,-2.5366f},{-11.1428f,10.4512f,-2.5366f},{-11.1428f,9.1512f,-3.2422f},{-11.1428f,7.8512f,-2.5366f},{-11.1428f,6.5512f,-3.1781f},{-11.1428f,5.2512f,-2.5366f},{-11.1428f,3.9512f,-2.5366f},{-11.1428f,2.6512f,-2.5366f},{-11.1428f,1.3512f,-2.5366f} },
	{ {-9.8428f,11.7512f,-2.5366f},{-9.8428f,10.4512f,-2.5366f},{-9.8428f,9.1512f,-2.5366f},{-9.8428f,7.8512f,-3.3705f},{-9.8428f,6.5512f,-4.0119f},{-9.8428f,5.2512f,-2.5366f},{-9.8428f,3.9512f,-2.5366f},{-9.8428f,2.6512f,-2.5366f},{-9.8428f,1.3512f,-3.2422f} },
	{ {-8.5428f,11.7512f,-2.5366f},{-8.5428f,10.4512f,-2.5366f},{-8.5428f,9.1512f,-2.5366f},{-8.5428f,7.8512f,-5.4054f},{-8.5428f,6.5512f,-4.4491f},{-8.5428f,5.2512f,-2.5366f},{-8.5428f,3.9512f,-3.5629f},{-8.5428f,2.6512f,-2.5366f},{-8.5428f,1.3512f,-2.5366f} },
	{ {-7.2428f,11.7512f,-2.5366f},{-7.2428f,10.4512f,-2.5366f},{-7.2428f,9.1512f,-2.5366f},{-7.2428f,7.8512f,-5.4097f},{-7.2428f,6.5512f,-3.8195f},{-7.2428f,5.2512f,-2.5366f},{-7.2428f,3.9512f,-3.5629f},{-7.2428f,2.6512f,-2.5366f},{-7.2428f,1.3512f,-2.5366f} },
	{ {-5.9428f,11.7512f,-2.5366f},{-5.9428f,10.4512f,-2.5366f},{-5.9428f,9.1512f,-2.5366f},{-5.9428f,7.8512f,-3.3705f},{-5.9428f,6.5512f,-4.4491f},{-5.9428f,5.2512f,-3.8195f},{-5.9428f,3.9512f,-0.0309f},{-5.9428f,2.6512f,-2.5366f},{-5.9428f,1.3512f,-2.5366f} },
	{ {-4.6428f,11.7512f,-2.5366f},{-4.6428f,10.4512f,-2.5366f},{-4.6428f,9.1512f,-2.5366f},{-4.6428f,7.8512f,-2.5366f},{-4.6428f,6.5512f,-3.8195f},{-4.6428f,5.2512f,-2.5366f},{-4.6428f,3.9512f,-2.5366f},{-4.6428f,2.6512f,-4.5251f},{-4.6428f,1.3512f,-2.5366f} },
	{ {-3.3428f,11.7512f,-2.5366f},{-3.3428f,10.4512f,-2.5366f},{-3.3428f,9.1512f,-5.0383f},{-3.3428f,7.8512f,-4.5892f},{-3.3428f,6.5512f,-3.5629f},{-3.3428f,5.2512f,-4.5766f},{-3.3428f,3.9512f,-2.8536f},{-3.3428f,2.6512f,-0.619f},{-3.3428f,1.3512f,-2.5366f} },
	{ {-2.0428f,11.7512f,-2.5366f},{-2.0428f,10.4512f,-2.5366f},{-2.0428f,9.1512f,-2.5366f},{-2.0428f,7.8512f,-2.5366f},{-2.0428f,6.5512f,-3.3654f},{-2.0428f,5.2512f,-2.8799f},{-2.0428f,3.9512f,-2.5366f},{-2.0428f,2.6512f,-2.5366f},{-2.0428f,1.3512f,-2.5366f} },
	{ {-0.7428f,11.7512f,-2.5366f},{-0.7428f,10.4512f,-2.5366f},{-0.7428f,9.1512f,0.7348f},{-0.7428f,7.8512f,-2.5366f},{-0.7428f,6.5512f,-2.5366f},{-0.7428f,5.2512f,-2.8554f},{-0.7428f,3.9512f,-4.267f},{-0.7428f,2.6512f,-2.5366f},{-0.7428f,1.3512f,-2.5366f} },
	{ {0.5572f,11.7512f,-2.5366f},{0.5572f,10.4512f,-2.5366f},{0.5572f,9.1512f,-2.5366f},{0.5572f,7.8512f,-3.0466f},{0.5572f,6.5512f,-3.6204f},{0.5572f,5.2512f,-2.8554f},{0.5572f,3.9512f,-2.5366f},{0.5572f,2.6512f,-2.5366f},{0.5572f,1.3512f,-2.5366f} },
	{ {1.8572f,11.7512f,-2.5366f},{1.8572f,10.4512f,-2.5366f},{1.8572f,9.1512f,-2.5366f},{1.8572f,7.8512f,-2.5366f},{1.8572f,6.5512f,-2.5366f},{1.8572f,5.2512f,-3.1741f},{1.8572f,3.9512f,-2.5366f},{1.8572f,2.6512f,-2.5366f},{1.8572f,1.3512f,-2.5366f} },
	{ {3.1572f,11.7512f,-2.5366f},{3.1572f,10.4512f,-2.5366f},{3.1572f,9.1512f,-2.5366f},{3.1572f,7.8512f,-2.5366f},{3.1572f,6.5512f,-2.5366f},{3.1572f,5.2512f,-2.5366f},{3.1572f,3.9512f,-2.5366f},{3.1572f,2.6512f,-2.5366f},{3.1572f,1.3512f,-2.5366f} },
};

#if TOWER_FIELD_CAPTURED
/* a real memory card history, 21 x 22 bytes (real bootHistory[]
 * @0x1f0138): char name[16]; u8 count, mask, own, pad;
 * u16 date = ((y-2000)<<9)|(m<<5)|d */
static const u8 realBootHistory[21*22] = {
	0x53, 0x43, 0x45, 0x53, 0x5f, 0x35, 0x30, 0x30, 0x2e, 0x30, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x61, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x34, 0x30, 0x2e, 0x32, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x43, 0x45, 0x53, 0x5f, 0x35, 0x33, 0x33, 0x2e, 0x31, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x43, 0x55, 0x53, 0x5f, 0x39, 0x37, 0x31, 0x2e, 0x31, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x43, 0x55, 0x53, 0x5f, 0x39, 0x37, 0x31, 0x2e, 0x32, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x30, 0x33, 0x2e, 0x33, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x55, 0x53, 0x5f, 0x32, 0x30, 0x30, 0x2e, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x34, 0x36, 0x2e, 0x32, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x32, 0x35, 0x2e, 0x34, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x50, 0x4d, 0x5f, 0x36, 0x35, 0x34, 0x2e, 0x37, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x43, 0x45, 0x53, 0x5f, 0x35, 0x30, 0x38, 0x2e, 0x37, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x43, 0x55, 0x53, 0x5f, 0x39, 0x37, 0x33, 0x2e, 0x32, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x31, 0x37, 0x2e, 0x39, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x31, 0x31, 0x2e, 0x33, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x33, 0x35, 0x2e, 0x33, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x55, 0x53, 0x5f, 0x32, 0x31, 0x30, 0x2e, 0x35, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x50, 0x4d, 0x5f, 0x36, 0x35, 0x34, 0x2e, 0x38, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x34, 0x31, 0x2e, 0x38, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x30, 0x37, 0x2e, 0x35, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x4c, 0x45, 0x53, 0x5f, 0x35, 0x30, 0x38, 0x2e, 0x37, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
	0x53, 0x43, 0x45, 0x53, 0x5f, 0x35, 0x33, 0x33, 0x2e, 0x32, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x5c, 0x2e,
};
#else
typedef struct HistEntry HistEntry;
struct HistEntry {
	char name[16];
	u8 count, mask, own, pad;
	u16 date;
};
static HistEntry simHistory[21];

/* ELF launch args (see main.c): [mode] [seed [ngames [nboots
 * [framelimit]]]] - mode is 'boot' (default), 'idle' or 'illegal';
 * the cycle-counter seed alone repeats under an emulator's
 * deterministic boot timing, hence the explicit seed arg */
extern int gameArgc;
extern char **gameArgv;

static int
argInt(int n, int def)
{
	const char *s;
	int v = 0;

	if(n >= gameArgc || gameArgv == nil || gameArgv[n] == nil)
		return def;
	for(s = gameArgv[n]; *s >= '0' && *s <= '9'; s++)
		v = v*10 + (*s - '0');
	return s == gameArgv[n] ? def : v;
}

static void
ParseArgs(void)
{
	static const char *modeNames[] = { "boot", "idle", "illegal", "menu" };
	int i, m;

	/* arg base: PCSX2's -gameargs passes user args starting at argv[0]
	 * (no ELF path); other loaders put the path in argv[0].  If argv[0]
	 * is a pure number, the user args start there.  An optional mode
	 * word comes before the numeric args. */
	argBase = argInt(0, -1) >= 0 ? 0 : 1;
	for(i = 0; i < 2 && i < gameArgc; i++) {
		if(gameArgv[i] == nil)
			continue;
		for(m = 0; m < 4; m++)
			if(strcmp(gameArgv[i], modeNames[m]) == 0) {
				openingMode = m;
				argBase = i+1;
				break;
			}
	}
	printf("osdsys: mode %s\n", modeNames[openingMode]);
}

/* the numeric-argument accessors, for modes that parse their own args
 * (menu.c) instead of going through SimulateBootHistory */
int OsdArgInt(int n, int def) { return argInt(argBase + n, def); }

/* simulate a memory card's life with the REAL updater rules: a library
 * of titles booted with a favourites-skewed distribution, so some
 * titles appear once and the favourites grow big towers (and
 * occasionally sprawl). */
static int fullField;	/* NOT original: argv ngames == 0 fills every cell */

static void
SimulateBootHistory(void)
{
	int i, b, nboots, ngames;
	u32 cycles;

	/* numeric args start after the optional mode word (ParseArgs) */
	int base = argBase;

	/* argv seed if given; else per-boot-ish variety from the cycle
	 * counter (deterministic emulators may repeat, real HW won't) */
	asm volatile ("mfc0 %0, $9" : "=r"(cycles));
	towerRandState ^= argInt(base+0, cycles);

	/* tower HEIGHT comes from towerD, which only starts growing at
	 * count 5 (D table: 1..4 -> 0.1 = height 3; 5..13 -> 0.2..1.0 =
	 * heights 6..30, resetting each sprawl decade) - the defaults push
	 * the favourites well into that band so the field shows the full
	 * height ladder, not just brightness variation. */
	ngames = argInt(base+1, 16);
	if(ngames == 0)
		fullField = 1;	/* ngames 0 = populate the WHOLE field (test
				 * mode, see GenerateTowerField) */
	if(ngames < 1) ngames = 1;
	nboots = argInt(base+2, 80 + towerRand() % 80);
	hwFrameLimit = argInt(base+3, -1);
	printf("osdsys: field sim: seed %d, ngames %d, nboots %d\n",
			argInt(base+0, -1), ngames, nboots);

	memset(simHistory, 0, sizeof(simHistory));
	for(b = 0; b < nboots; b++) {
		int r1 = towerRand() % ngames, r2 = towerRand() % ngames;
		int title = r1 < r2 ? r1 : r2;	/* favourites-skew */
		HistEntry *e = nil;

		for(i = 0; i < 21; i++)
			if(simHistory[i].name[0] &&
			   simHistory[i].name[1] == (char)title &&
			   simHistory[i].name[2] == (char)(title>>8)) {
				e = &simHistory[i];
				break;
			}
		if(e == nil) {
			/* NOT original placement: the real updater fills empty
			 * slots front to back, so few-game simulations always
			 * occupy one fixed contiguous chunk of the field.  Pick
			 * a RANDOM empty slot instead so the field spreads; the
			 * authentic eviction rule still applies once the card
			 * is full. */
			int nempty = 0;
			for(i = 0; i < 21; i++)
				if(simHistory[i].name[0] == 0)
					nempty++;
			if(nempty > 0) {
				int pick = towerRand() % nempty;
				for(i = 0; i < 21; i++)
					if(simHistory[i].name[0] == 0 && pick-- == 0)
						break;
				e = &simHistory[i];
			} else {
				/* full card: evict lowest count, tie-break oldest
				 * date (the real rule) */
				e = &simHistory[0];
				for(i = 1; i < 21; i++)
					if(simHistory[i].count < e->count ||
					   (simHistory[i].count == e->count && simHistory[i].date < e->date))
						e = &simHistory[i];
			}
			memset(e, 0, sizeof(*e));
			e->name[0] = 'S';
			e->name[1] = (char)title;
			e->name[2] = (char)(title>>8);
			e->count = 1;
			/* random initial home cell too (real: always bit 0) -
			 * spreads towers within each 6-cell neighbourhood */
			e->own = towerRand() % 6;
			e->mask = 1 << e->own;
			e->date = b;
			continue;
		}
		e->date = b;
		if((e->mask & 0x3f) == 0x3f) {
			if(e->count < 63)
				e->count++;
			else
				e->own = 7;	/* frozen: all six cells maxed */
			continue;
		}
		if(e->count < 127)
			e->count++;
		if(e->count >= 14 && (e->count-14) % 10 == 0) {
			/* the sprawl: a random unset neighbourhood bit becomes home */
			int bit;
			do
				bit = towerRand() % 6;
			while(e->mask>>bit & 1);
			e->own = bit;
			e->mask |= 1 << bit;
		}
	}
}
#endif

/* real steps 2 + 4: parse a history into towerFlags/C/D + the static
 * per-cell positions. Positions come ONLY from the source table - the
 * history decides which cells light up and how big. Produces PRE-step-5
 * values (step 5 then applies its usual towerPos.z adjustment). */
static void
GenerateTowerField(void)
{
	const u8 *hist;
	int i, t, r, c;

#if TOWER_FIELD_CAPTURED
	hist = realBootHistory;
#else
	SimulateBootHistory();
	hist = (const u8*)simHistory;
#endif

	for(i = 0; i < 21; i++) {
		const u8 *e = hist + i*22;
		int count, mask, own, tidx;

		if(e[0] == 0)
			continue;
		count = e[16];
		mask = e[17];
		own = e[18];
		tidx = count < 14 ? count : (count-14) % 10 + 4;
		for(t = 0; t < 6; t++) {
			r = histCells[i][t][0];
			c = histCells[i][t][1];
			if(t == own) {
				towerFlags[r][c] = 1;
				towerC[r][c] = histTableC[tidx];
				towerD[r][c] = histTableD[tidx];
			} else if(mask>>t & 1) {
				towerFlags[r][c] = 1;
				towerC[r][c] = 1.0f;
				towerD[r][c] = 1.0f;
			}
		}
	}

	/* NOT original: test mode (argv ngames == 0) - light up every cell
	 * of the 14x9 field */
#if !TOWER_FIELD_CAPTURED
	if(fullField)
		for(r = 0; r < 14; r++)
			for(c = 0; c < 9; c++) {
				towerFlags[r][c] = 1;
				towerC[r][c] = 0.0f;
				towerD[r][c] = 1.0f;
//				towerC[r][c] = histTableC[towerRand() % 14];
//				towerD[r][c] = histTableD[towerRand() % 14];
			}
#endif

	for(r = 0; r < 14; r++)
		for(c = 0; c < 9; c++) {
			towerPos[r][c][0] = (histSrcPos[r][c][0] + 4.8f)*4.0f;
			towerPos[r][c][1] = (histSrcPos[r][c][1] - 6.5f)*4.0f;
			towerPos[r][c][2] = (histSrcPos[r][c][2] + 4.0f)*12.0f + 150.0f;
			towerPos[r][c][3] = 0.0f;
		}
}


/* runtime float tables (real 0x28aed0/0x28aec0, filled from memcard) */
static float towerAngleTab[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static float towerPosTab[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

/* real: 0x2a7190 = 0.01745329 (deg2rad), read as *(gp-32480) in
 * DrawTowers.  towerSway = the per-frame Z sway angle, computed once
 * per DrawTowers() call (real 0x218730-0x21876c) - NOT per tower. */
#define DEG2RAD 0.01745329f
/* real: 0x2a7194 = 1.5707964 (PI/2), read as *(gp-32476) in DrawTowers
 * - the per-cell rotation term's step size (see towerPatchVertices'
 * caller / the angles[2] build in DrawTowers). */
#define PI_2 1.5707964f
static float towerSway;
/* tower render struct (the real spr-allocated struct, ptr at
 * gp-31000): base matrix @0x000, translated matrix @0x080, packet ptr
 * @0x0c0, transformed vertices @0x180, source quad @0x200, rotated
 * matrix @0x240. */
static struct {
	sceVu0FMATRIX base;		/* 0x000 */
	u32 pad0[16];			/* 0x040-0x07f */
	sceVu0FMATRIX translated;	/* 0x080 */
	sceVu0FMATRIX view;		/* 0x0c0 (param matrix A = obj->screen view) */
	u32 pad1[32];			/* 0x100-0x17f */
	sceVu0FMATRIX transformed;	/* 0x180 (light matrix) */
	u32 pad2[16];			/* 0x1c0-0x1ff */
	sceVu0FMATRIX quad;		/* 0x200 (light vectors, rotated per tower) */
	sceVu0FMATRIX rotated;		/* 0x240 */
} towerStruct;

/* per-tower chain patch helpers (real: tower_218180/218210/218288)
 *
 * The chain layout (real OSDSYS):
 *   0x268020 head tag {NEXT 23, ADDR 0x2681a0, STCYCL, UNPACKR V4-32
 *           num=22} - a 4-word tag: with TTE the upper 8 bytes
 *           (STCYCL + UNPACK) are transferred to VIF1 as data, then
 *           QWC=23 qwords follow from tag+16.  tower_218288 rewrites
 *           only the ADDR field (0x2681a0, the static value).
 *   0x268030-0x26818f the 22 in-stream qwords: matrix A (view), matrix
 *           B (model), 4 light colours, light matrix, 2 clip vectors,
 *           the init GIF packet (qwords 18-20), tex flag (qword 21).
 *           tower_218180 patches qwords 0-3 / 4-7 / 12-15 per tower.
 *   0x268190 MSCAL (the 23rd qword of the head chain - starts vucode_1,
 *           which XGKICKs the init packet at qword 18, then loops on
 *           XTOP); 0x268194 BASE + 0x268198 OFFSET + 0x26819c NOP are
 *           dead static bytes between the head chain and the CNT tag.
 *   0x2681a0 CNT 106: 6 packet blocks, each = MSCNT + STCYCL +
 *           UNPACKR(17) + the 17-qword packet {GIF tag, 4 verts,
 *           4 normals, 4 colours, 4 st}.  Block 5 (first in the
 *           region) has no MSCNT.  The 6 packets = the 6 quads of one
 *           tower; tower_218288 rewrites ALL 6 tags every tower,
 *           tower_218318 patches the vertex z + colours, tower_2184d0
 *           the st coords.  The VU program renders each packet as it
 *           arrives (XTOP -> process -> XGKICK -> loop).
 *   0x268850 END tag (0x70000000)
 */

/* patch the per-tower matrix params into the 22-qword unpack window
 * (real: tower_218180 copies spr struct fields +0x80/+0xc0/+0x180) */
static void
towerPatchParams(void)
{
	/* real tower_218180: copies the FULL 4x4 (64 bytes) of three spr
	 * matrices into the 22-qword param window:
	 *   spr+0x0c0 (view)        -> 0x268030  (qwords 0-3  = matrix A)
	 *   spr+0x080 (model)       -> 0x268070  (qwords 4-7  = matrix B)
	 *   spr+0x180 (light matrix)-> 0x2680f0  (qwords 12-15)
	 * The VU program MulMatrix(0, 4, 22) computes obj->screen = A x B.
	 *
	 * ALL the real patchers write through the UNCACHED mirror (they OR
	 * 0x20000000 into the destination: 21818c/218214/2182bc/2183e0/
	 * 21850c) - that is how the patched chain stays coherent with the
	 * VIF1 DMA without any FlushCache in the per-tower kick path.
	 * Patching through cached pointers makes the DMA read stale RAM on
	 * real hardware (emulators without an EE dcache never show it). */
	memcpy(UNCACHED(&TowerView), &towerStruct.view, 64);
	memcpy(UNCACHED(&TowerModel), &towerStruct.translated, 64);
	memcpy(UNCACHED(&TowerLightMatrix), &towerStruct.transformed, 64);
}

/* rewrite the 6 packets' GIF tags (real: tower_218288, called with
 * a0=0 a1=1 every tower - see the caller 0x2189d0).  The tag words are
 * the packet's qword 0, NOT VIFcodes:
 *   {0x8004, 0x304e4000, 0x412, 0} =
 *     NLOOP=4 EOP=1 | PRE=1 PRIM=0xa2 (TME=1 TRISTRIP CTXT=1 -
 *     the towers are TEXTURED tristrips in context 1) FLG=PACKED
 *     NREG=3 | REGS = {ST, RGBAQ, XYZF2} (0x412) + 0
 * The static ROM state is the init variant {0x8004, 0x302e6000,
 * 0x412, 0} (PRIM=0x68: TRIANGLES FST, a0=1 a1=0).  All 6 packets
 * are rewritten, every tower.  The head REF (0x268024) is rewritten
 * to 0x2681a0 too (same value - the static chain already points
 * there). */
static void
towerPatchTags(void)
{
	int i;
	for(i = 0; i < 6; i++) {
		TowerPacket *pkt = UNCACHED(towerBlocks[i]);	/* real: 2182bc */
		pkt->tag[0] = 0x8004;
		pkt->tag[1] = 0x304e4000;	/* ((a0<<6)|(a1<<7)|28)<<15 | 0x30004000 */
		pkt->tag[2] = 0x412;		/* REGS {ST, RGBAQ, XYZF2} */
		pkt->tag[3] = 0;
	}
}

/* write the giftag block (real: tower_218210) - part of the param
 * window */
static void
towerPatchGifTag(void)
{
	u32 *g = UNCACHED(&TowerGifTag);	/* real: 218214 */
	g[0] = 0x8002;		/* NLOOP=2 EOP=1 */
	g[1] = 0x10000000;
	g[2] = 14;
	g[3] = 0;
	g[4] = 0x44;		/* real: sd of 0x80_0000_0044 */
	g[5] = 0x80;
	g[6] = 66;
	g[7] = 0;
	g[8] = 0;
	g[9] = 0;
	g[10] = 73;
	g[11] = 0;
	g[12] = 0;		/* 0x268180 */
	g[13] = 0;
	g[14] = 0;
	g[15] = 0;
}

/* static per-vertex z sign flags (real table at 0x27ae50,
 * [block][vertex]: 1 = +f0, 0 = -f0) */
static const u8 towerSignFlags[6][4] = {
	{ 0, 0, 0, 0 },
	{ 0, 1, 0, 1 },
	{ 1, 0, 1, 0 },
	{ 1, 1, 0, 0 },
	{ 0, 0, 1, 1 },
	{ 1, 1, 1, 1 },
};

/* patch the per-tower vertex heights + colours into the 6 packets
 * (real: tower_218318).  Per block b, per vertex v:
 *   vertex z = +/-f0  (f0 = the tower height from towerA)
 *   colours = {0,0,0,128} if f0 <= 0, else {s,s,s,128} with
 *   s = f12 (block 0) or f12*f3 (blocks 1-5); f12 = the grid value
 *   scaled by (towerB/128 or towerA/30), f3 = the side-face factor. */
static void
towerPatchVertices(int r, int c, float gridval)
{
	float f0 = towerA[r][c];
	int   bint = (int)towerB[r][c];
	float f12 = gridval * (bint != 0 ? (float)bint/128.0f : towerA[r][c]/30.0f);
	/* f3: real 2183d0 loads lwc1 f3, -32484(gp) = 0x2a718c = 0.8, the
	 * side-face darkening factor */
	float f3 = 0.8f;
	int b, v;

	for(b = 0; b < 6; b++) {
		TowerPacket *pkt = UNCACHED(towerBlocks[b]);	/* real: 2183e0 */
		float s = b == 0 ? f12 : f12*f3;
		for(v = 0; v < 4; v++) {
			int far = towerSignFlags[b][v];
			pkt->vert[v][2] = far ? f0 : -f0;
			/* the black-out condition is PER-VERTEX (real 218418-
			 * 218458): far-side (+z) vertices are black, near-side
			 * get the lit colour, so Gouraud shades every tower
			 * from lit at the camera end to black at its tail -
			 * that fade (times the banded wall texture) is the
			 * whole "column" look */
			pkt->colour[v][0] = far ? 0.0f : s;
			pkt->colour[v][1] = far ? 0.0f : s;
			pkt->colour[v][2] = far ? 0.0f : s;
			pkt->colour[v][3] = 128.0f;
		}
	}
}

/* patch the st texture coords into the 6 packets (real: tower_2184d0):
 * u = a0/256, quad {u,u},{u+1,u},{u,u+1},{u+1,u+1}, q=1, w=0 into all 6
 * packets.  PRIM 0x9c is TRISTRIP, IIP, TME, FST=0 - S,T are NORMALIZED
 * fractions of the texture, so u=a0/256 offsets by a0 texels of
 * TEXOWAL0's 256px.
 *
 * a0 comes from the caller (DrawTowers 0x218a08-0x218a3c):
 *   a0 = (r+c+9)*(r+8)/(c+7) + (r+c+9)*(r+7)/(c+9), integer division -
 * a per-tower pseudo-hash, 0..114 over the field.
 *
 * The u offset is real and the streaky band it produces is AUTHENTIC:
 * S,T span [u, u+1] but the towers draw with whatever CLAMP_1 is left
 * over - from frame 2 on that is WMS=WMT=CLAMP from the previous
 * frame's cube passes (the ROM's only two CLAMP_1 writers are DrawFog
 * (REPEAT) and DrawLightsAndCubes (CLAMP), both AFTER the towers in the
 * frame).  So a u-sized fraction of each face (the S>1 / T>1 side)
 * samples the clamped edge texel row/column of the wall texture,
 * smeared into streaks - up to a0=114/256 = ~45% of the face.  The
 * bands are visible on the tower faces in real footage; not a port
 * bug. */
static void
towerPatchST(int r, int c)
{
	int a0 = (r+c+9)*(r+8)/(c+7) + (r+c+9)*(r+7)/(c+9);
	float u = (float)a0/256.0f;
	int b, v;

	for(b = 0; b < 6; b++) {
		TowerPacket *pkt = UNCACHED(towerBlocks[b]);	/* real: 21850c */
		for(v = 0; v < 4; v++) {
			pkt->st[v][0] = u + (v & 1);
			pkt->st[v][1] = u + (v >> 1);
			pkt->st[v][2] = 1.0f;
			pkt->st[v][3] = 0.0f;
		}
	}
}

static void
InitTowersFog(void)
{
	/* real OpeningInitTowersFog (0x218e00) steps:
	 *   1. memset(towerFlags)                      - ported, below
	 *   2. parse memcard boot history into the     - ported as
	 *      field (bootHistory[] @0x1f0138)           GenerateTowerField
	 *                                                (captured or
	 *                                                simulated history)
	 *   3. VU1 upload (sendDma/vu1Wait)            - ported, below
	 *   4. transform the source positions          - folded into
	 *      (@0x28a670) into towerPos                 GenerateTowerField
	 *   5. alpha/halo tables (towerA/B/towerPos.z) - ported, below
	 *   6. HeightGrid()                            - ported, below
	 *   7. InitFog()                               - ported, below
	 *   8. tail: j DrawToExtraBuf2                 - ported (call below)
	 */
	int r, c;

	memset(towerFlags, 0, sizeof(towerFlags));

	/* the base matrix = identity (real: spr+0x000, initialized when the
	 * spr struct is allocated).  The RotMatrix helpers rotate FROM it;
	 * with a zero base the whole model matrix collapses to zero and the
	 * tower quads degenerate to a point. */
	memset(&towerStruct.base, 0, sizeof(towerStruct.base));
	towerStruct.base[0][0] = 1.0f;
	towerStruct.base[1][1] = 1.0f;
	towerStruct.base[2][2] = 1.0f;
	towerStruct.base[3][3] = 1.0f;

	/* the light source matrix (real: static spr+0x200, set up once):
	 *   light = rotation x source   (row-major product, see the
	 *   MulMatrix call in DrawTowers)
	 * with source rows
	 *   row0 = (0, s2, -s2, 0)   s2 = sqrt(0.5) - light dir (0,1,-1)/sqrt2
	 *   row1 = (0, s2, -s2, 0)   - the SAME vector again
	 *   row2 = (1, 0, 0, 0)
	 *   row3 = (0, 0, 0, 1)
	 * vucode_1's lighting is f = max(0, n^T x light) per column,
	 * colour = f.x*(1,1,1) + (f.y+f.z)*(0.8,..) + f.w*(0.4,..) (the
	 * colour vectors are the static window qwords 8-11) - with this
	 * source all four side faces get ~0.966 uniformly and the cap 1.4. */
#define TOWER_L 0.70710678f
	towerStruct.quad[0][0] = 0.0f;    towerStruct.quad[0][1] = TOWER_L;
	towerStruct.quad[0][2] = -TOWER_L; towerStruct.quad[0][3] = 0.0f;
	towerStruct.quad[1][0] = 0.0f;    towerStruct.quad[1][1] = TOWER_L;
	towerStruct.quad[1][2] = -TOWER_L; towerStruct.quad[1][3] = 0.0f;
	towerStruct.quad[2][0] = 1.0f;    towerStruct.quad[2][1] = 0.0f;
	towerStruct.quad[2][2] = 0.0f;    towerStruct.quad[2][3] = 0.0f;
	towerStruct.quad[3][0] = 0.0f;    towerStruct.quad[3][1] = 0.0f;
	towerStruct.quad[3][2] = 0.0f;    towerStruct.quad[3][3] = 1.0f;

	/* real steps 2+4: parse the boot history (captured or simulated)
	 * into the tower field */
	GenerateTowerField();

	/* real step 5: alpha/halo tables */
	for(r = 0; r < 14; r++)
		for(c = 0; c < 9; c++) {
			float f0 = towerD[r][c]*30.0f;
			float f3;
			if(f0 < 3.0f)
				f0 = 3.0f;
			towerA[r][c] = f0;
			f3 = towerC[r][c];
			towerPos[r][c][2] += f3*30.0f - f0;
			if(1.0f <= f3) {
				towerC[r][c] = 1.0f;
				towerB[r][c] = 0.0f;
			} else
				towerB[r][c] = (float)(int)((1.0f - f3)*128.0f);
		}

	/* real step 3: upload vucode_1 (the tower renderer): the CNT tag at
	 * the blob head (0x2678e0) pulls MPG(229 words) + 1832 bytes of
	 * microcode into VU1 code memory.  Real sequence after the kick is
	 * sceDmaSync THEN sceGsSyncPath(0,0) (0x21904c/0x219058) -
	 * vu1Wait() stands in for the former, the explicit call below for
	 * the latter. */
	sendDma(&TowerUpload);
	vu1Wait();
	sceGsSyncPath(0, 0);

	/* real steps 6 + 7 */
	HeightGrid();
	InitFog();

	DrawToExtraBuf2();	/* real: tail call, j not jal (see step 8 above) */
}

/* ==== TOWER_DEBUG_SQUARE: put the tower texture mapping on big screen
 * quads to inspect the clamp band + mip filtering up close.  Draws
 * AFTER the tower field, with the towers' own TEX0/TEX1/MIPTBP still
 * bound and CLAMP=(1,1) set explicitly (the towers' steady-state).
 * Each quad is an FST=0 STQ tristrip exactly like the tower packets,
 * so q steers the GS mip LOD directly: LOD = log2(1/q) + K.  Five
 * quads left to right:
 *   1. q=1        LOD ~ -4   -> mip0,  ST [u, u+1] (the clamp band)
 *   2. q=1/32     LOD ~ 0.9  -> mip0/1 blend, same band
 *   3. q=1/180    LOD ~ 3.4  -> the FAR-TOWER operating point (mip2)
 *   4. q=1/256    LOD ~ 3.9  -> deep mip2, same band
 *   5. q=1        ST [0,1] control - no clamping anywhere
 * u = 104/256 (the field's biggest offset - a ~40% band). */
#define TOWER_DEBUG_SQUARE 0

#if TOWER_DEBUG_SQUARE
static void
towerDebugQuad(int x, int y, int size, float s0, float s1, float q)
{
	static const float st[4][2] = { {0,0},{1,0},{0,1},{1,1} };
	union { float f; u32 i; } fq, fs, ft;
	int v;

	fq.f = q;
	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP,
		1, 1, 0, 0, 0, 0, 0, 0));	/* IIP, TME, FST=0, ctxt 1 */
	for(v = 0; v < 4; v++) {
		int px = x + (v & 1)*size;
		int py = y + (v >> 1)*size;
		fs.f = (s0 + (s1-s0)*st[v][0]) * q;
		ft.f = (s0 + (s1-s0)*st[v][1]) * q;
		pktSetAD(SCE_GS_RGBAQ,
			SCE_GS_SET_RGBAQ(128, 128, 128, 128, fq.i));
		pktSetAD(SCE_GS_ST, SCE_GS_SET_ST(fs.i, ft.i));
		pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(
			(px+2048-screenW/2)<<4, (py+2048-screenH/2)<<4, 0xffffff));
	}
	vif1End();
}

static void
DrawTowerDebugSquares(void)
{
	float u = 104.0f/256.0f;

	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetCLAMP_1(1, 1, 0, 0, 0, 0);
	towerDebugQuad(  8, 48, 120, u, u+1.0f, 1.0f);
	towerDebugQuad(134, 48, 120, u, u+1.0f, 1.0f/32.0f);
	towerDebugQuad(260, 48, 120, u, u+1.0f, 1.0f/180.0f);
	towerDebugQuad(386, 48, 120, u, u+1.0f, 1.0f/256.0f);
	towerDebugQuad(512, 48, 120, 0.0f, 1.0f, 1.0f);
	vif1SetZTest(1);
	vif1SetZWrite(1);
}
#endif

static void
DrawTowers(void)
{
	int r, c;

	/* deliberately NO CLAMP_1 write here - the real DrawTowers has none
	 * (vif1SetClamp 0x212bd0 has exactly two callers: DrawFog and
	 * DrawLightsAndCubes), so the towers inherit the previous frame's
	 * final CLAMP_1 = the cube passes' (1,1) CLAMP.  That leftover
	 * clamp state x the towerPatchST u offset IS the authentic streaky
	 * band on the tower faces - see towerPatchST's comment. */
	vif1SetZWrite(1);
	vif1SetZTest(1);

	/* real 0x218728: vif1SetTextureMIP(&OpeningTexList[
	 * *(0x27aeb0)], 1, 5, -65).  towerWallTexID = 6 -> OpeningTexList[6]
	 * = resid 0x1c = TEXOWAL0, the 256x256 mipmapped wall texture
	 * (mxl=2, so mipmap=1 / mmin=5 = LINEAR_MIP_LINEAR make sense).
	 * The towers draw with PRIM 0x9c: TRISTRIP, IIP, TME, FST=0
	 * (S,T are normalized STQ fractions, scaled by the texture's
	 * width/height - NOT direct texel coords), CTXT=0 - so they
	 * sample this TEX0_1/TEX1_1 in context 1. */
	vif1SetTextureMIP(&textures[TEXID_WAL0], 1, 5, -65);

	/* real: the view (param matrix A) = the opening scene camera
	 * matrix, updated per frame by Process() */
	memcpy(&towerStruct.view, &sprMatrices->cameraScreenMatrix, 64);

	/* real: a "shadow walk" over the field rows exists here in the ROM
	 * (0x218650-0x2186f4) but is a mathematical no-op: the gate it
	 * feeds always reads back cell (0,0)'s |dx|+|dy| (a fixed pointer,
	 * never reassigned), which is >= 0, so its "skip if <= 0" never
	 * skips anything.  Intentionally not ported - full trace in
	 * docs/towers-analysis.md. */

	/* real: a SINGLE per-frame sway angle, computed once (loop-
	 * invariant across all towers this frame, NOT per-tower):
	 * f23 = sinf(((frameCount%360)-180) * DEG2RAD) * 10.0 * DEG2RAD */
	towerSway = sinf((float)((frameCount % 360) - 180) * DEG2RAD)
			* 10.0f * DEG2RAD;

	for(r = 0; r < 14; r++) {
		for(c = 0; c < 9; c++) {
			float angles[4];
			float pos[4];
			float term;
			int q;
			int k;

			if(!towerFlags[r][c])
				continue;

			/* the real ROM stores -1.0 into a per-frame array on
			 * DrawTowers' own stack for C==1.0 towers - part of a
			 * dormant multi-pass build-in animation (the shipped
			 * pass count is 1), with no colour effect.  Colour
			 * always comes from the grid; the only C==1.0
			 * difference that matters is the sway suppression
			 * below. */

			/* per-cell Z rotation term (DrawTowers 0x218830-
			 * 0x218920): q = (row+col+9)*(row+3) / 7 (integer
			 * div); term = (q mod 4) * PI/2, a quantized 90-degree
			 * step per cell.  Mathematically invisible for this
			 * geometry (a rigid 90-degree co-rotation of a square,
			 * uniformly-textured box) - see towers-analysis.md. */
			q = (r+c+9)*(r+3)/7;
			term = (float)(q % 4) * PI_2;
			angles[0] = towerAngleTab[0];
			angles[1] = towerAngleTab[1];
			angles[2] = towerAngleTab[2] + term
					+ (towerC[r][c] == 1.0f ? 0.0f : towerSway);
			angles[3] = towerAngleTab[3];

			/* real helpers 0x267370 / 0x2676b0 / 0x267860 are ONE
			 * call each (sceVu0RotMatrix/TransMatrix/MulMatrix);
			 * the calls below are their libvu0.a bodies manually
			 * inlined - a verified-exact expansion, not a
			 * mismatch. */
			sceVu0RotMatrixZ(towerStruct.rotated, towerStruct.base, angles[2]);
			sceVu0RotMatrixY(towerStruct.rotated, towerStruct.rotated, angles[1]);
			sceVu0RotMatrixX(towerStruct.rotated, towerStruct.rotated, angles[0]);

			pos[0] = towerPos[r][c][0] + towerPosTab[0];
			pos[1] = towerPos[r][c][1] + towerPosTab[1];
			pos[2] = towerPos[r][c][2] + towerPosTab[2];
			pos[3] = towerPos[r][c][3] + towerPosTab[3];
			sceVu0CopyMatrix(towerStruct.translated, towerStruct.rotated);
			/* real 0x2676b0: out = rotation with the position added
			 * to ROW 3.  The VU program applies the model matrix
			 * TRANSPOSED, so a column-3 translation would land in
			 * the w channel and blow up the perspective divide. */
			towerStruct.translated[3][0] += pos[0];
			towerStruct.translated[3][1] += pos[1];
			towerStruct.translated[3][2] += pos[2];
			/* the light matrix: real 0x2189a4 is sceVu0MulMatrix(
			 * spr+0x180, spr+0x200, spr+0x240) = MulMatrix(light,
			 * SOURCE, ROTATION), and MulMatrix computes m0.row[i] =
			 * m1 x m2.row[i] - each ROTATION row goes through the
			 * SOURCE matrix, NOT each source vector through the
			 * rotation (the transposed variant darkens two of the
			 * four side faces). */
			for(k = 0; k < 4; k++)
				sceVu0ApplyMatrix(towerStruct.transformed[k],
						towerStruct.quad, towerStruct.rotated[k]);

			/* drain the previous tower's chain BEFORE touching the
			 * blob: the patchers write (uncached, straight to RAM)
			 * the very memory the in-flight chain DMA is reading,
			 * so patching mid-transfer mixes two towers' data.  The
			 * real ROM has the same structural race and lives on
			 * timing margin (its per-tower matrix math outlasts the
			 * small chain); we make the margin explicit.  This
			 * drain is also what makes towerKick's raw register
			 * sequence safe. */
			sceGsSyncPath(0, 0);

			/* real per-tower sequence: giftag (params window),
			 * block NOPs, 218318 (vertex z + colours), 2184d0
			 * (st coords), params, ONE chain kick at the head */
			towerPatchGifTag();
			towerPatchTags();
			towerPatchVertices(r, c, towerGrid[r+3][c+6]);
			towerPatchST(r, c);
			towerPatchParams();
			towerKick(&TowerChain);
		}
	}
	/* real: ONE sceGsSyncPath(0,0) after the whole field, not per-tower
	 * (0x218ad8) */
	towerSyncEnd();

#if TOWER_DEBUG_SQUARE
	DrawTowerDebugSquares();
#endif
}

/* ==== NOT PORTED: the "flare and illegal stuff" init chain ====
 * real InitOpening calls sub_219f08 here, between OpeningInitTowersFog
 * and StartFrame.  sub_21b690 is ALSO called once from
 * OpeningInitLightsCubes (shared helper) - not wired in there yet. */

/* real: sub_21a438 (0x21a438) - build the flare rings: for each of the
 * 7 discs a 16-gon of radius 8 (flareRingSmall) and one of radius
 * 28+i*8 (flareRingLarge), spin phase i*0.925*2pi/7, centre at
 * {0,0,1160}.  (The colour blocks it also fills are the port's
 * flareColorSmall/Large initializers.) */
static void
sub_21a438(void)
{
	int i, k;
	float a, r;

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
		flareRot[i][2] = i*0.925f*PI*2.0f/7.0f;	/* real: *(0x2a7208) */
	}
}

/* real: sub_215798 (0x215798) - init the illegal-disc fog: the shared
 * 3x3 patch (30x30 units; only the centre vertex is lit, with a random
 * warm red - r = 64+rand%64, g = b = r*96/128) and the 128 particles
 * (x,y = (rand%4800-2400)*0.01, z = rand%805+477, rotation zeroed).
 * Runs for BOTH opening types.  (An earlier note said "4 rand calls" -
 * that was the call SITES; 3 of them are in the particle loop, so the
 * real stream advance is 1+3*128.) */
static void
sub_215798(void)
{
	int i, j, c;
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
				c = osdRand()%64 + 64;
			col[0] = c;
			col[3] = 128;
			col[1] = col[2] = c*96/128;
		}
	for(i = 0; i < 128; i++) {
		illegalFogPos[i][0] = (osdRand()%4800 - 2400)*0.01f;
		illegalFogPos[i][1] = (osdRand()%4800 - 2400)*0.01f;
		illegalFogPos[i][3] = 0.0f;
		illegalFogRot[i][0] = 0.0f;
		illegalFogRot[i][1] = 0.0f;
		illegalFogRot[i][2] = 0.0f;
		illegalFogRot[i][3] = 1.0f;
		illegalFogState[i] = 0;
		illegalFogPos[i][2] = osdRand()%805 + 477;
	}
}

/* real: sub_21b690 (0x21b690) - build the cube corner table (0x27b370)
 * with the given half-extent and set all five instances' base colour
 * (0x27b3f0) to {x,y,z}+128 (stored here with cube_21BC90's 0..127
 * clamp folded in, see cubeBaseColor). */
static void
sub_21b690(float half, float x, float y, float z)
{
	int i;

	for(i = 0; i < 8; i++) {
		cubeCorners[i][0] = i & 1 ? half : -half;
		cubeCorners[i][1] = i & 2 ? half : -half;
		cubeCorners[i][2] = i & 4 ? half : -half;
		cubeCorners[i][3] = 1.0f;
	}
	cubeBaseColor[0] = clamp(x + 128.0f, 0.0f, 127.0f);
	cubeBaseColor[1] = clamp(y + 128.0f, 0.0f, 127.0f);
	cubeBaseColor[2] = clamp(z + 128.0f, 0.0f, 127.0f);
}

/* real: sub_219cb8 (0x219cb8) - place the five cubes for the ILLEGAL
 * scene (runs at init for both opening types; the normal scene then
 * overwrites everything in InitLightsCubes): corners/colour from
 * sub_21b690(1.2, 0,0,0), anchors from its OWN seed table (0x27af50,
 * NOT the opening's 0x27a210) spanning the 823..1113 height band the
 * camera climbs through, rates and start angles from small
 * index-derived factors (constants at 0x2a71d4..0x2a71e4).  The real
 * also selects the illegal colour/ST callback (sub_2192c0 into the
 * 0x2a7808 quad-callback pointer read by DrawTexturedQuad) - the port
 * keys that off the illegal pass table instead. */
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
		cubeAnchor[i][0] = illegalCubeSeedTable[i][0];
		cubeAnchor[i][1] = illegalCubeSeedTable[i][1];
		cubeAnchor[i][2] = (illegalCubeSeedTable[i][2] - 2.5f)*128.0f + 800.0f - 12.0f;
		cubeAnchor[i][3] = 0.0f;
		/* start ANGLES (0x27b140) from index-derived factors, tiny
		 * tumble RATES (0x27b190) */
		cubeOutB[i][0] = f*((i*2)%9)*0.25f;
		cubeOutB[i][1] = f*((i*2)&7)/5.0f;
		cubeOutB[i][2] = f*((i*2)%7)/6.0f;
		cubeOutB[i][3] = f*((i*2)%9)*0.25f;
		cubeRate[i][0] = 0.004f/f;
		cubeRate[i][1] = f*0.003f;
		cubeRate[i][2] = f/800.0f + 0.002f;
		cubeRate[i][3] = 0.0f;
	}
}

/* real: sub_219f08 (0x219f08, 14 insns) - InitOpening's real call, wraps
 * the three functions above. */
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

/* real: initTextShit (0x214f20) - IDA's own name.  Arm the text
 * overlay matching the opening type (SCE text for the normal opening,
 * the "insert a PlayStation disc" text for the illegal one) and reset
 * the fade. */
static void
initTextShit(void)
{
	if(fooOpeningType == 0) {
		sceTextState = 0;
		illegalTextState = -1;
	} else {
		illegalTextState = 0;
		sceTextState = -1;
	}
	sceTextAlpha = 0;
	sceTextStep = 4;
	illegalTextAlpha = 0;
}

static void
Init(void)
{
	ParseArgs();
	/* the real fooOpeningType comes from systemState (0x1f05e8) == 4 =
	 * illegal disc, set at 0x211f90 */
	fooOpeningType = openingMode == MODE_ILLEGAL ? 1 : 0;
	if(openingMode == MODE_ILLEGAL)
		/* a boot param in state 6's "do nothing" set: the real illegal
		 * screen idles forever with no end fade (confirmed against a
		 * real GS dump - no endFlag activity); which exact param an
		 * illegal boot carries is unverified, any of 101-105/113/116
		 * behaves this way */
		osdBootParam = 101;
	openingType = nextOpeningType = fooOpeningType;

	/* real InitOpening order: OpeningInitRender, OpeningInitAnimation,
	 * OpeningInitTowersFog, sub_219f08, initTextShit, StartFrame -
	 * complete, nothing else in between. */
	InitRender();

	/* NOT original: the menu background scene is a different module
	 * (Module U, 0x21C910-0x230000) with its own one-shot init
	 * (0x21CE58) and its own frame body (0x21CF20) - none of the
	 * opening's animation/tower/text state applies. */
	if(openingMode == MODE_MENU) {
		InitMenuScene();
		StartFrame();
		frameCount = evenOddFrame;
		return;
	}

	InitAnimation();
	if(openingMode == MODE_IDLE)
		positionSpeed[2] = 0.0f;	/* stay put: the state machine and
						 * text never trigger */
	InitTowersFog();
	sub_219f08();
	initTextShit();
	StartFrame();

	frameCount = evenOddFrame;
}

static void
OpeningThread(void *arg)
{
	Init();
	// ...
	if(openingMode == MODE_MENU)
		DoMenuScene();
	else
		DoOpeningIllegal();
	// ...
}

#define STACKSZ 12*1024
u_char OpeningStack[STACKSZ] ALIGN16;

int
MakeOpeningThread(void)
{
	struct ThreadParam tparam;

	tparam.entry = OpeningThread;
	tparam.stack = OpeningStack;
	tparam.stackSize = STACKSZ;
	tparam.initPriority = 1;
	tparam.gpReg = &_gp;
	int id = CreateThread(&tparam);
	StartThread(id, nil);
	return id;
}
