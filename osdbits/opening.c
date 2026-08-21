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
	u32 format;	// extended PSM
	u32 unk2;
	GSTex gstex;
};

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
	gsAllocPtr = 2 * (screenW*screenH)/64;
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

static void
gsAllocExtraBuffers(void)
{
	Rect r;

	r.x = r.y = 0;
	r.w = screenW;
	r.h = screenH;
	extraBuf1 = gsAllocBuffer(SCE_GS_PSMCT32, &r);
	extraBuf2 = gsAllocBuffer(SCE_GS_PSMCT32, &r);
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
void pktSetFlatRect(Rect *r, Color *col, u32 abe)
{
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 0, 0, abe, 0, 0, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col->r, col->g, col->b, col->a, 0x3f800000));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ((r->x+2048-screenW/2)<<4, (r->y+2048-screenH/2)<<4, 0));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(((r->x+r->w+2048-screenW/2)<<4)-1, ((r->y+r->h+2048-screenH/2)<<4)-1, 0));
}
void pktSetTexRect(Rect *r, Rect *tr, Color *col, u32 abe)
{
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, abe, 0, 1, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col->r, col->g, col->b, col->a, 0x3f800000));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV((tr->x<<4)+8, (tr->y<<4)+8));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ((r->x+2048-screenW/2)<<4, (r->y+2048-screenH/2)<<4, 0));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(((tr->x+tr->w)<<4)+8, ((tr->y+tr->h)<<4)+8));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(((r->x+r->w+2048-screenW/2)<<4)-1, ((r->y+r->h+2048-screenH/2)<<4)-1, 0));
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
void vif1SetFlatRect(Rect *r, Color *col, u32 abe)
{ vif1Begin(); pktSetFlatRect(r, col, abe); vif1End(); }
void vif1SetTexRect(Rect *r, Rect *tr, Color *col, u32 abe)
{ vif1Begin(); pktSetTexRect(r, tr, col, abe); vif1End(); }

void
vif1SetFramebuffer(u32 fbp, u16 psm, int width, int height, int clear)
{
	vif1Begin();
	pktSetAD(SCE_GS_FRAME_1, SCE_GS_SET_FRAME(fbp, width/64, psm, 0));
	pktSetAD(SCE_GS_SCISSOR_1, SCE_GS_SET_SCISSOR(0, 0, width-1, height-1));
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

void
vif1SetZWrite(int enb)
{
	if(enb)
		vif1SetAD(SCE_GS_ZBUF_1, SCE_GS_SET_ZBUF(((screenW*screenH)/64)*2/32, SCE_GS_PSMZ24, 0));
	else
		vif1SetAD(SCE_GS_ZBUF_1, SCE_GS_SET_ZBUF(((screenW*screenH)/64)*2/32, SCE_GS_PSMZ24, 1));
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
int openingType, nextOpeningType, fooOpeningType;
int sceneState;

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

	return gsAddr;
}

static void
InitTexture(Texture *tex)
{
	int i, x, y;
	u32 bufsz, ui;
	u32 *dataBufs[2];
	int buf;
	u32 psm;
	u32 lum, alpha;
	Rect prevDim, mipDim;;

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
					// BUG: original code picks wrong colors here
					u16 col0 = src[(y*prevDim.h + x)*2];
					u16 col1 = src[(y*prevDim.h + (x+1))*2];
					u16 col2 = src[((y+1)*prevDim.h + x)*2];
					u16 col3 = src[((y+1)*prevDim.h + (x+1))*2];
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
					dst[y*prevDim.h + x] = a<<15 | b<<10 | g<<5 | r;
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

static void
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

static void
InitAnimation(void)
{
	openingState = 0;

	positionAccel1[0] = 0.0f;

	positionAccel2[0] = 0.0f;
	positionAccel2[1] = 0.0f;
	positionAccel2[2] = 0.0f;

	positionSpeed[0] = 0.0f;
	positionSpeed[1] = 0.0f;
//	positionSpeed[2] = 0.004f;

	rotationAccel[0] = 0.0f;
	rotationAccel[1] = 0.0f;
	rotationAccel[2] = 0.0f;

	rotationSpeed[0] = 0.0f;
	rotationSpeed[1] = 0.0f;
	rotationSpeed[2] = 0.001f;

	// TODO: one unknown
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

static void
InitLightsCubes(void)
{
	int i, l;

	// TODO:

	for(l = 0; l < 4; l++) {
		// unused, perhaps earlier formula?
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
	lightsSeed = rand()/2345 + 3456;
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
		// other formula from init
		//c = cosf(frameCount*0.005f*(l+1));
		//s = sinf(frameCount*0.003f*(l+1));

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
					rand();	// eh?

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

static void
DrawLightsAndCubes(void)
{
	// TODO

	sceVu0Normalize(sprVertices->verts1[3], fwdDir);

	DrawLights();
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

static void
DrawOpeningScene(void)
{
	// TODO

	DrawFog();

	DrawLightsAndCubes();
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

static void
InitIllegalScene(void)
{
}

static void
DrawIllegalScene(void)
{
}

static void
DoIllegalDisc(void)
{
	switch(sceneState) {
	case 0:
		InitIllegalScene();	// TODO: unused argument
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

static void
DoText(void)
{
}

static void
Process(void)
{
	float timestep;

	timestep = IsPAL() ? 1.2f : 1.0f;

	// TODO: big switch to handle state

	rotationSpeed[0] += (2.0f*rotationAccel[0] + 0.0f)*0.5f*timestep;
	rotationSpeed[1] += (2.0f*rotationAccel[1] + 0.0f)*0.5f*timestep;
	rotationSpeed[2] += (2.0f*rotationAccel[2] + 0.0f)*0.5f*timestep;

	positionSpeed[0] += (2.0f*positionAccel2[0] + positionAccel1[0])*0.5f*timestep;
	positionSpeed[1] += (2.0f*positionAccel2[1] + positionAccel1[1])*0.5f*timestep;
	positionSpeed[2] += (2.0f*positionAccel2[2] + positionAccel1[2])*0.5f*timestep;

	positionAccel2[2] += positionAccel1[2]*timestep;

	rotation += (2.0f*rotationSpeed[2] + positionAccel2[2])*0.5f*timestep;

	position[0] += (2.0f*positionSpeed[0] + positionAccel2[0])*0.5f*timestep;
	position[1] += (2.0f*positionSpeed[1] + positionAccel2[1])*0.5f*timestep;
	position[2] += (2.0f*positionSpeed[2] + positionAccel2[2])*0.5f*timestep;

	if(rotation > PI) rotation -= TAU;
	if(rotation < PI) rotation += TAU;

	upDir[0] = sinf(rotation);
	upDir[1] = cosf(rotation);

	sceVu0NormalLightMatrix(sprMatrices->normalLightMatrix, light1, light2, light3);
	sceVu0CameraMatrix(sprMatrices->cameraMatrix, position, fwdDir, upDir);
	sceVu0ViewScreenMatrix(sprMatrices->viewScreenMatrix, 1024.0f,
		screenAX, screenAY, 2048.0f, 2048.0f,
		1.0f, 16777215.0f, 1.0f, 65536.0f);
	sceVu0MulMatrix(sprMatrices->cameraScreenMatrix,
		sprMatrices->viewScreenMatrix, sprMatrices->cameraMatrix);
}

static void
DrawEnd(void)
{
	DoText();
	// TODO: unknown func
	WaitNextFrame();
	frameCount++;
}

static void
DoOpeningIllegal(void)
{
	sceneState = 0;
	while(openingType != 2) {
		Process();
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

static void
InitTowersFog(void)
{
	// ...
	InitFog();
	// ...
}

static void
Init(void)
{
	openingType = nextOpeningType = fooOpeningType;

	InitRender();
	// ...
	InitAnimation();
	InitTowersFog();
	// ??? flare and illegal stuff?
	// InitText();
	StartFrame();

	frameCount = evenOddFrame;
}

static void
OpeningThread(void *arg)
{
	Init();
	// ...
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
