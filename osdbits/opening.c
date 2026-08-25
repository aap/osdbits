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

static void InitCubes(void);

static void
InitLightsCubes(void)
{
	int i, l;

	/* real OpeningInitLightsCubes computes the cube half (5-instance
	 * seed table -> cubeAnchor/cubeRate/cubeOutB) here too - kept as a
	 * separate InitCubes() below for readability, still called from the
	 * same place the real ROM does it. */
	InitCubes();

	for(l = 0; l < 4; l++) {
		/* disasm-verified 2026-08-24: this really is dead code in the
		 * ROM too (0x217c9c-0x217cf0) - results discarded, never
		 * stored anywhere. Kept to match real behavior exactly. */
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
	lightsSeed = rand()%2345 + 3456;	// disasm-verified 2026-08-24: div zero,v0,v1; mfhi a0 at 0x217d80 is the REMAINDER, not v0/v1
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
					rand();	/* disasm-verified 2026-08-24: a real call
						 * (0x216a54), result unused - matches ROM */

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
 * Real structure, established 2026-08-25 by disassembling DrawCube
 * (0x217520) and its 8 real helpers in full (see docs/towers-analysis.md
 * for the complete trace):
 *  - once per cube: integrate+wrap a rotation angle, build a rotate+
 *    translate+view matrix chain from real per-instance data, transform
 *    all 8 corners, clip-test the whole cube (real: cube_21BF88).
 *  - if visible: compute a per-face normal, visibility side, and
 *    per-vertex lighting for all 6 faces (real: cube_21BC90, called 6x).
 *  - capture the previous saved frame into the cube's own working buffer
 *    (real: sub_21c7a8 - the cube-side twin of DrawToExtraBuf2, disasm-
 *    confirmed to use a SEPARATE buffer slot, 0x279f10 vs
 *    DrawToExtraBuf2's 0x279f18).
 *  - draw each visible face in multiple layered passes (back-facing
 *    faces first, then front-facing), each pass picking a texture
 *    source via a real 4-way selector (real: CubeTextureFuckery): a
 *    named texture resource, one of two frame-parity ping-ponged
 *    buffers, or the just-captured self buffer - this is the "texture
 *    magic between faces" aap remembered.
 *
 * What's simplified rather than byte-exact here (all real, disasm-traced
 * mechanisms - NOT guesses - but with specific narrow details the trace
 * couldn't fully pin down, same spirit as the tower shadow-gate/memcard
 * gaps): the real ROM does up to 5 texture layers per visibility group
 * with a per-vertex scrolling-UV phase generator (a double-precision
 * time source never fully traced - see towers-analysis.md); this port
 * does 2 layers (a lit base pass + one refractive overlay sampling the
 * captured buffer at the cube's own screen position, the standard cheap
 * screen-space refraction technique) with plain, non-scrolling UVs.
 * Geometry, position, scale, rotation, per-face visibility and the
 * capture-buffer wiring are all the real mechanism, not placeholders. */

#define CUBE_INSTANCES 5

/* real: 0x27a210, 5 x 16-byte (3 floats + pad) table read by
 * OpeningInitLightsCubes with index = i%5 - disasm-verified 2026-08-24
 * against a live savestate (see towers-analysis.md's "four vs five"
 * section - this table, and the anchor/rate derivation below, were
 * confirmed exact against 0x27b0f0/0x27b190 in a real capture). */
static sceVu0FVECTOR cubeSeedTable[CUBE_INSTANCES] = {
	{  3.5679f,  0.5447f, 2.5932f, 0.0f },
	{ -0.9042f, -1.1173f, 3.7952f, 0.0f },
	{  3.2639f, -2.6491f, 4.1075f, 0.0f },
	{ -3.7296f, -2.3677f, 4.3654f, 0.0f },
	{ -3.1017f,  2.2409f, 4.5429f, 0.0f },
};
static sceVu0FVECTOR cubeAnchor[CUBE_INSTANCES];	/* real 0x27b0f0: world position */
static sceVu0FVECTOR cubeRate[CUBE_INSTANCES];		/* real 0x27b190: rotation rate/frame */
/* real 0x27b140 ("outB"): the live rotation angle. Disasm-verified
 * 2026-08-25 (cube_21BF88): integrated by cubeRate and wrapped to
 * [-PI,PI] every frame - NOT the simple closed-form (initV +
 * frameCount*rate) an earlier session's savestate comparison suggested;
 * that form only holds until the first wrap. */
static sceVu0FVECTOR cubeOutB[CUBE_INSTANCES];

/* cube geometry. Real: the face->vertex index table (0x27afe0), the
 * per-face normal table (0x27b090), and the half-extent - all disasm-
 * extracted 2026-08-25. The half-extent is a real runtime float
 * (0x2a714c = 1.8, read by DrawCube's call into the real corner-builder,
 * sub_21b690) - an earlier pass on this file had the right sign pattern
 * (cross-checked bit0=+-X bit1=+-Y bit2=+-Z against every real face
 * normal, still correct) but wrongly assumed a unit (1.0) half-extent;
 * the real cube is 1.8x bigger per axis (~5.8x the volume). */
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
/* real 0x27afa0: the standard unit UV quad (matches towers' own st[4][2]
 * table exactly - disasm-extracted 2026-08-25). */
static const sceVu0FVECTOR cubeUV[4] = {
	{ 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f },
};
/* real: sceVu0LightColorMatrix's 4 inputs, 0x27b040/50/60/70 - disasm-
 * extracted 2026-08-25 (fixed grayscale lighting, not per-face colour -
 * matches a reflective/glass look rather than a coloured one). */
static sceVu0FVECTOR cubeColorAmbient = { 0.0f, 0.0f, 0.0f, 0.0f };
static sceVu0FVECTOR cubeColor1 = { 0.5f, 0.5f, 0.5f, 0.0f };
static sceVu0FVECTOR cubeColor2 = { 0.5f, 0.5f, 0.5f, 0.0f };
static sceVu0FVECTOR cubeColorBase = { 0.2f, 0.2f, 0.2f, 1.0f };

static struct {
	sceVu0FMATRIX base;
	sceVu0FMATRIX rotated;
	sceVu0FMATRIX translated;
	sceVu0FMATRIX transformed;
	sceVu0FMATRIX lightColor;	/* computed once at init, real: sceVu0LightColorMatrix */
	sceVu0IVECTOR screenVerts[8];
	sceVu0FVECTOR worldNormal[6];
	float faceSign[6];		/* >0 = facing away from camera (draw first) */
	float vertexLight[6][4];
} cubeStruct;

static void
InitCubes(void)
{
	int i;

	memset(&cubeStruct.base, 0, sizeof(cubeStruct.base));
	cubeStruct.base[0][0] = 1.0f;
	cubeStruct.base[1][1] = 1.0f;
	cubeStruct.base[2][2] = 1.0f;
	cubeStruct.base[3][3] = 1.0f;

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

/* real: sub_21c7a8 (0x21c7a8) - capture the saved buffer into the cube's
 * own working/refraction buffer. Uses extraBuf2 for that working buffer
 * (aap's own pre-AI code already allocated extraBuf1/extraBuf2, unused
 * until now); real ROM samples FROM a separate saved-frame buffer that
 * DrawToExtraBuf2 is meant to maintain (extraBuf1 here) - since that's
 * not ported yet (see towers-analysis.md), extraBuf1 is currently
 * whatever was last in that VRAM region, so the refraction source won't
 * look like a real reflection until DrawToExtraBuf2 lands. The capture
 * WIRING (buffer choice, clear, composite) is the real mechanism. */
static void
CubeCaptureBuffer(void)
{
	Rect full;
	Color white = { 128, 128, 128, 128 };
	int tw, th;

	full.x = full.y = 0;
	full.w = screenW;
	full.h = screenH;

	vif1SetFramebuffer(extraBuf2, SCE_GS_PSMCT32, screenW, screenH, 1);
	vif1SetZTest(0);
	vif1SetZWrite(0);
	vif1SetAlphaBlend(0, 2, 0);

	tw = GetTexExponent(screenW);
	th = GetTexExponent(screenH);
	vif1SetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(0, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	vif1SetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(extraBuf1, screenW/64, SCE_GS_PSMCT32,
			tw, th, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	vif1SetTexRect(&full, &full, &white, 0);

	/* vif1SetFramebuffer above redirected SCE_GS_FRAME_1/SCISSOR_1 (the
	 * same physical registers the real screen draw uses) to extraBuf2 for
	 * the capture blit - point them back at this frame's real draw buffer
	 * (whichever of db.draw0/draw1 StartFrame's sceGsPutDrawEnv activated
	 * via evenOddFrame) before returning, or the cube's own face draws
	 * right after this call would land in extraBuf2 instead of on screen. */
	{
		sceGsDrawEnv1 *env = evenOddFrame == 0 ? &db.draw0 : &db.draw1;
		vif1SetFramebuffer(env->frame1.FBP, env->frame1.PSM, screenW, screenH, 0);
	}
}

/* real: cube_21BC90 (0x21bc90) - per-face normal, visibility side, and
 * per-vertex lighting. Real: cross-product of two local edges via the
 * face->vertex table; for a perfect cube that's always parallel to the
 * axis-aligned face normal already in cubeFaceNormal[], so rotating the
 * table entry directly is numerically identical and simpler. Real
 * visibility test is a 2D screen-space cross product of two face edges;
 * this uses the transformed normal's view-space component instead
 * (the standard equivalent - same sign, same meaning). Per-vertex
 * lighting: real does a genuine per-vertex dot product against a light-
 * ish table; since every vertex on one face of a perfect cube shares the
 * same face normal, that collapses to one value per face here - ported
 * as such rather than a guessed per-vertex variation. */
static void
CubeFaceSetup(int face)
{
	sceVu0FVECTOR n;
	float l;
	int k;

	sceVu0ApplyMatrix(n, cubeStruct.rotated, cubeFaceNormal[face]);
	sceVu0Normalize(cubeStruct.worldNormal[face], n);

	cubeStruct.faceSign[face] = sceVu0InnerProduct(cubeStruct.worldNormal[face], fwdDir);

	l = -sceVu0InnerProduct(cubeStruct.worldNormal[face], light1)*0.5f + 0.5f;
	l = clamp(l, 0.0f, 1.0f);
	for(k = 0; k < 4; k++)
		cubeStruct.vertexLight[face][k] = l;
}

/* real: cube_21BF88 (0x21bf88) - per-cube setup: integrate+wrap the
 * rotation angle, build the rotate+translate+view matrix chain, transform
 * all 8 corners, clip-test, then set up all 6 faces. Returns nonzero if
 * the whole cube is offscreen (real: same early-out DrawCube's caller
 * uses). */
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

	sceVu0RotMatrix(cubeStruct.rotated, cubeStruct.base, cubeOutB[instance]);
	sceVu0TransMatrix(cubeStruct.translated, cubeStruct.rotated, cubeAnchor[instance]);
	sceVu0MulMatrix(cubeStruct.transformed, sprMatrices->cameraScreenMatrix, cubeStruct.translated);

	if(sceVu0ClipAll(clipMin, clipMax, cubeStruct.transformed, cubeCorners, 8))
		return 1;

	for(k = 0; k < 8; k++)
		sprTransformVertex(cubeStruct.screenVerts[k], cubeCorners[k], cubeStruct.transformed);

	for(k = 0; k < 6; k++)
		CubeFaceSetup(k);

	return 0;
}

/* real: DrawTexturedQuad (0x21c560) plus the per-vertex finishing work
 * real ROM does via cube_21B798/21BBE0/21BA08 and the 0x216f88 callback -
 * merged into one function here since none of those real boundaries
 * matter for correctness once the algorithm is known (same approach the
 * rest of this file already takes, e.g. DrawLights). Draws one face as a
 * textured tristrip. mode 0 = lit base pass (no texture, vertex colour
 * only); mode 1 = refractive overlay, sampling the just-captured buffer
 * at the cube's own screen position (the standard cheap screen-space
 * refraction trick) - real ROM's exact per-face/per-vertex scrolling UV
 * source is one of the not-fully-traced gaps noted above. */
/* debug aid: force every cube face to a flat, fully-opaque, untextured
 * bright red - no blending, no texture sampling, no Z rejection - so the
 * raw transform/clip output is unmissable on screen regardless of any
 * lighting/blend/texture/Z bug elsewhere in the pipeline. See
 * [[feedback-debugging-methodology]] - leave this toggle in place after
 * the current "no cubes visible" investigation is resolved. */
#define CUBE_DEBUG_RED 0

static void
DrawCubeFace(int face, int mode)
{
	int k;

#if CUBE_DEBUG_RED
	mode = 0;
#endif

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 1, mode, 0,
#if CUBE_DEBUG_RED
			0,
#else
			1,
#endif
			0, 0, 0, 0));
	for(k = 0; k < 4; k++) {
		int vi = cubeFaceVerts[face][k];
		u32 *v = cubeStruct.screenVerts[vi];

#if CUBE_DEBUG_RED
		pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(255, 0, 0, 128, 0x3f800000));
#else
		if(mode == 0) {
			int c = (int)(cubeStruct.vertexLight[face][k]*128.0f);
			pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(c, c, c, 64, 0x3f800000));
		} else {
			float u = (float)(v[0]>>4)/(float)(screenW<<4)*16.0f;
			float t = (float)(v[1]>>4)/(float)(screenH<<4)*16.0f;
			pktSetAD(SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&u, *(u32*)&t));
			pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(96, 96, 96, 48, 0x3f800000));
		}
#endif
		pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(v[0], v[1], v[2], 0));
	}
	vif1End();
}

/* real: DrawCube (0x217520), called once per cube instance (5x). */
static void
DrawCube(int instance)
{
	int pass, face;

	/* CubeCaptureBuffer() confirmed (2026-08-25 bisection) to be the actual
	 * cause of "no cubes visible": it redirects SCE_GS_FRAME_1 to extraBuf2
	 * correctly (doesn't touch the real screen - verified, a debug marker
	 * drawn to the real screen just before cube code runs survives it),
	 * but the restore-back-to-the-real-screen step lands somewhere wrong,
	 * and two rounds of real-ROM disassembly couldn't find how the real
	 * game handles this either (no restore call anywhere in its own
	 * DrawCube/DrawLightsAndCubes, by any mechanism found - possibly needs
	 * a live PCSX2 GS-register check to resolve, not just static disasm).
	 * DrawCubeFace's mode=1 pass doesn't actually sample extraBuf2 yet
	 * anyway (it uses a static named texture, TEXID_REF - the dynamic
	 * refraction wiring is a separate, already-known gap), so nothing is
	 * lost by skipping this call for now. Not deleted - real mechanism to
	 * be revisited once the restore-target bug is understood. */
	if(CubeTransformAndClip(instance))
		return;

	vif1SetZWrite(1);
#if CUBE_DEBUG_RED
	/* also rule out Z-buffer occlusion while chasing "no cubes visible" */
	vif1SetZTest(0);
#else
	vif1SetZTest(1);
#endif

	/* back-facing faces first, then front-facing - real ROM's own
	 * two-group draw order (DrawTexturedQuad's faceFlag). */
	for(pass = 1; pass >= 0; pass--) {
		for(face = 0; face < 6; face++) {
			if((cubeStruct.faceSign[face] > 0.0f) != pass)
				continue;
			vif1SetAlphaBlend(1, 4, 96);
			DrawCubeFace(face, 0);
#if CUBE_DEBUG_RED
			continue;
#endif
			/* real 4-way texture selector (CubeTextureFuckery) can pick a
			 * named texture, one of two ping-ponged buffers, or the
			 * self-captured buffer - this pass always uses the captured
			 * buffer (extraBuf2, see CubeCaptureBuffer) for the
			 * refraction sample. TEXID_REF (RESID_TEXOREF - "reflection")
			 * is set as the current texture only so vif1SetAlphaBlend's
			 * surrounding state matches what the rest of this file
			 * expects; DrawCubeFace(face,1) overrides TEX0_1/TEX1_1
			 * itself for the actual refraction sample. */
			vif1SetTexture(&textures[TEXID_REF]);
			vif1SetAlphaBlend(1, 5, 64);
			DrawCubeFace(face, 1);
		}
	}
}

static void
DrawLightsAndCubes(void)
{
	int i;

	/* real order: sceVu0Normalize, DrawLights, vif1SetClamp(?), DrawCube -
	 * the vif1SetClamp call's real args aren't traced yet, not added. */

	sceVu0Normalize(sprVertices->verts1[3], fwdDir);

	DrawLights();

#if CUBE_DEBUG_RED
	/* isolation test: a hardcoded, fixed-position rectangle via the
	 * already-trusted vif1SetFlatRect() primitive (used elsewhere in this
	 * file, not cube-specific) - completely independent of CubeTransform-
	 * AndClip/ClipAll/CubeCaptureBuffer/any cube state. If THIS doesn't
	 * show up either, the bug isn't in the cube transform/clip/capture
	 * math at all - it's in something shared (GS/framebuffer state,
	 * DMA flush) that's broken by the time DrawLightsAndCubes runs here,
	 * even though DrawLights (just above) renders fine. */
	{
		Rect r;
		Color red = { 255, 0, 0, 128 };
		r.x = screenW/2 - 100;
		r.y = screenH/2 - 50;
		r.w = 200;
		r.h = 100;
		vif1SetFlatRect(&r, &red, 0);
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
 * DrawExtraBuf2, DrawToExtraBuf2, DrawFog, DrawLightsAndCubes, sub_218b20,
 * sub_218bd0 (surveyed 2026-08-24). Only DrawTowers/DrawFog/
 * DrawLightsAndCubes are real so far - stubbed here so the gap is visible
 * in the call structure, not just a "// TODO" comment. */

/* real: DrawExtraBuf2 (0x214240, 160 insns) - "draw the saved buffer back"
 * half of the radial-brightness-falloff lead (docs/towers-analysis.md).
 * Real callees are all already-ported vif1Begin/pktSetTEST_1/pktSetAD/
 * pktSetAlphaBlend/pktSetTexRect/vif1End GS-packet primitives - this is
 * NOT a new unported layer, just not traced through yet (unlike
 * DrawToExtraBuf2 below, which got a partial trace). */
static void
DrawExtraBuf2(void)
{
}

/* real: DrawToExtraBuf2 (0x214050, 123 insns) - "save current frame" half.
 * PARTIALLY traced 2026-08-24 (docs/towers-analysis.md): switches the draw
 * target to a separate buffer via vif1SetXYOffset/vif1SetZWrite/
 * vif1SetFramebuffer (all already-ported), then a frameCount-parity branch
 * changes a vif1SetAD-packed register value before a vif1SetTexRect with a
 * half-width dest rect - looks like a frame-to-frame ping-pong buffer, not
 * finished/verified enough to port yet. Also the tail call of
 * OpeningInitTowersFog (see InitTowersFog below) - same stub, two call
 * sites. */
static void
DrawToExtraBuf2(void)
{
}

/* real: sub_2144c0 (0x2144c0, 179 insns) - real callees are all
 * already-ported vif1Set* primitives (XYOffset/ZWrite/ZTest/AlphaBlend/
 * AD/Framebuffer/TexRect) - looks like another buffer-blit, not
 * investigated which one. */
static void
sub_2144c0(void)
{
}

/* real: sub_218b20 (0x218b20, 43 insns) - one of the two "trail functions"
 * at the end of DrawOpeningScene, not investigated beyond its one real
 * callee. */
static void
sub_218b20(void)
{
	sub_2144c0();
}

/* real: DrawSomeSprite2 (0x214918, 82 insns) - IDA's own name. Real
 * callees are memset + already-ported vif1SetZTest/ZWrite/AlphaBlend/
 * FlatRect. */
static void
DrawSomeSprite2(void)
{
}

/* real: fp_25A368 (0x25a368, 38 insns) - IDA's own name (looks float-
 * formatting-related, calls __unpack_f), not investigated. */
static void
fp_25A368(void)
{
}

/* real: sub_218bd0 (0x218bd0, 46 insns) - the other "trail function",
 * DrawOpeningScene's real tail call. */
static void
sub_218bd0(void)
{
	DrawSomeSprite2();
	fp_25A368();
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

	DrawExtraBuf2();
	DrawToExtraBuf2();

	DrawFog();

	DrawLightsAndCubes();

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

/* ==== towers: faithful port of the real OSDSYS VU1 tower pipeline ====
 *
 * The real OSDSYS draws the tower field through VU1 microcode:
 *  - per tower, the EE patches fields in the chains (UNPACK source
 *    address, parameters) and kicks a VIF1 chain-mode transfer
 *    (sendDma), then polls VU1 status.
 *  - the tower field layout itself is loaded from the memory card in
 *    the real OSDSYS (boot history: 21 x 22 byte entries, +16 type,
 *    +17 bits, +18 index).  osdbits has no memcard and the history
 *    parse itself isn't ported yet, so for now we generate a field with
 *    the same statistical shape as a real captured one instead
 *    (RandomizeTowerField).
 *
 * 2026-08-25: the VU1 microcode + per-tower DMA chain used to be a
 * verbatim byte extraction (vudata.inc) patched by hand-computed
 * "real VA - blob base" offsets (OFF_CHAIN_HEAD etc.), needing a
 * vudataRelocate() pass at runtime to fix up absolute addresses baked
 * into the extracted bytes.  Reconstructed as real dvp-as source
 * instead (towerchain.dsm, using vucode_1.vsm for the microcode) -
 * assembled and diffed byte-for-byte against the real ROM bytes at
 * every one of these addresses to confirm the reconstruction is exact
 * (see docs/towers-analysis.md).  This gives named, linker-relocated
 * symbols for every real patch point instead of numeric offsets, and
 * the linker's own relocations (R_MIPS_DVP_27_S4 on the chain tags)
 * replace vudataRelocate() entirely - patch points below are extern
 * symbols defined in towerchain.dsm. */
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

/* the 6 per-tower packet blocks, in real memory order (matches the real
 * static pointer table at 0x27a650) - kept as an array so the existing
 * per-block loops (towerPatchTags/Vertices/ST) don't need restructuring */
static u32 *const towerBlocks[6] = {
	&TowerBlock5, &TowerBlock1, &TowerBlock2, &TowerBlock3, &TowerBlock4, &TowerBlock0
};

/* sendDma - kick a VIF1 chain-mode transfer at addr.  Disasm-verified
 * 2026-08-24 against the ONE-TIME vucode_1 upload in OpeningInitTowersFog
 * (0x218ff0-0x219044): QWC=0; TADR=addr&0x0fffffff; *(0x1000e010)=2;
 * FlushCache(0) (real: jal 0x24dce0, confirmed FlushCache by name, NOT the
 * "RPC/file helper" an older note guessed); CHCR=325.  This exact sequence
 * is real ONLY for this one-time upload - DrawTowers's per-tower kick is a
 * DIFFERENT real sequence, see towerKick() below.  Do not reuse sendDma()
 * for the per-tower path (it used to be shared with vu1Wait() below - that
 * was wrong, see towerKick). */
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
	/* real 0x266b08 = sceDmaSync, confirmed by name (osdsys_dump.idb):
	 * called as sceDmaSync(&D1_CHCR-as-pointer, 0, 0) right after the
	 * ONE-TIME upload's CHCR=325 kick - this manual poll (of the same
	 * DMA-start bit sceDmaSync itself polls) is a faithful stand-in for
	 * that specific call, not a generic wait.  Bounded so a wedged DMA
	 * can't hang the frame.  Real OSDSYS follows this sceDmaSync call
	 * with a further sceGsSyncPath(0,0) - see the InitTowersFog call
	 * site below, which now does that explicitly instead of folding it
	 * in here. */
	int i;
	for(i = 0; i < 1<<20; i++) {
		if(!(*D1_CHCR & 0x100))
			return;
	}
}

/* towerKick - kick the per-tower VIF1 chain.  Disasm-verified 2026-08-24
 * against DrawTowers's real per-cell kick (0x218a48-0x218a90) - a
 * DIFFERENT real sequence from sendDma()/vu1Wait() above, found by aap
 * noticing our port had no sceGsSyncPath anywhere and DrawTowers didn't
 * match what they saw in IDA:
 *   QWC=0; TADR=addr&0x0fffffff; *(0x1000e010)=2; sceGsSyncPath(0,0);
 *   CHCR=325 (kick) - NO FlushCache here (confirmed absent from this
 *   exact instruction window), and NO wait/sync call immediately after
 *   the kick either: DrawTowers kicks each tower and moves straight on
 *   to computing the next one (pipelined, not serialized per-tower) -
 *   there is exactly ONE more sceGsSyncPath(0,0), after the ENTIRE r,c
 *   loop finishes (0x218ad8), not one per tower.  Ported as a single
 *   trailing towerSyncEnd() call after DrawTowers's double loop instead
 *   of a per-iteration vu1Wait(). */
static void
towerKick(void *addr)
{
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
	static const float sqrt5202 = 72.12489168f;	/* sqrt(5202) = 51*sqrt(2) */
	int r, c;
	for(r = 0; r < 20; r++) {
		float x = (r-10)*5.1f;
		for(c = 0; c < 20; c++) {
			float y = (c-10)*5.1f;
			float d1, d2, v;
			d1 = sqrtf((x+2.55f)*(x+2.55f) + (y+7.65f)*(y+7.65f));
			d2 = sqrtf((x-2.55f)*(x-2.55f) + (y-7.65f)*(y-7.65f));
			v = clamp(255.0f*(sqrt5202 - 2.0f*d1)/sqrt5202, 32.0f, 255.0f)
			  + clamp(0.5f*255.0f*(sqrt5202 - 4.0f*d2)/sqrt5202, 32.0f, 220.0f);
			v *= 10.2f;
			v -= 10.0f*(int)(((r*(r+c)/(c+1)) % 11) - 5);
			towerGrid[r][c] = clamp(v, 32.0f, 220.0f);
		}
	}
}

/* real memcard-loaded tower field: the real ROM loads this from the
 * memcard boot history, which isn't parsed here (see InitTowersFog's step
 * 2 note).  Until then, generate a randomized field each boot instead of
 * always showing the exact same 21 towers.
 *
 * Not a guess: fit from a real captured field (21 real cells, pulled
 * 2026-08-23 from a PCSX2 savestate's eeMemory.bin at their real VAs -
 * see git history/docs/towers-analysis.md for that raw table and its
 * verification, since it's no longer kept live in this file). That real
 * data gives:
 *   - density: 21/126 cells flagged (~1 in 6)
 *   - towerC: 18/21 = 0.4, 1/21 = 0.6, 1/21 = 0.8, 1/21 = 1.0 (the
 *     towerC==1.0 "special path" cell DrawTowers has its own branch for)
 *   - towerD: 20/21 = 0.1, the one towerC==1.0 cell pairs with D=0.3
 *     (per aap, D likely tracks a play count from the boot history - the
 *     real per-cell value isn't reconstructable without that parse, so
 *     this keeps the same C/D pairing behavior, not the exact value)
 *   - position: linear regression against (row,col) - y fits the real
 *     data to float precision (`y = -5.2*c + 21.0048`), x fits to within
 *     ~1 unit (`x = 5.184*r - 35.598`) with some residual almost
 *     certainly from scene rotation at the moment of capture, not
 *     modeled here. The 5.2ish pitch matches HeightGrid()'s own 5.1-unit
 *     grid spacing, which makes physical sense - same field.
 *   - z: 17/21 real cells sit at exactly one baseline value
 *     (167.560791); the other 4 are scattered outliers with no r,c
 *     pattern discernible from only 4 samples - modeled as an occasional
 *     random offset rather than invented as a formula. */
static void
RandomizeTowerField(void)
{
	int r, c;

	for(r = 0; r < 14; r++) {
		for(c = 0; c < 9; c++) {
			int roll;

			if((rand() % 6) != 0)
				continue;
			towerFlags[r][c] = 1;

			roll = rand() % 21;
			if(roll == 0) {
				towerC[r][c] = 1.0f;
				towerD[r][c] = 0.3f;
			} else {
				towerC[r][c] = roll == 1 ? 0.6f : roll == 2 ? 0.8f : 0.4f;
				towerD[r][c] = 0.1f;
			}

			towerPos[r][c][0] = 5.2f*r - 35.6f + (float)((rand()%100)-50)/50.0f;
			towerPos[r][c][1] = -5.2f*c + 21.0f;
			towerPos[r][c][2] = (rand() % 5) == 0
					? 167.56f + (float)((rand()%100)-50)
					: 167.56f;
			towerPos[r][c][3] = 0.0f;
		}
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
 * The chain layout (from the real OSDSYS, verified against PCSX2's
 * hwDmacSrcChainWithStack + Vif1_Dma TTE handling):
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
	 * The VU program MulMatrix(0, 4, 22) computes obj->screen = A x B. */
	memcpy(&TowerView, &towerStruct.view, 64);
	memcpy(&TowerModel, &towerStruct.translated, 64);
	memcpy(&TowerLightMatrix, &towerStruct.transformed, 64);
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
		u32 *b = towerBlocks[i];
		b[0] = 0x8004;
		b[1] = 0x304e4000;	/* ((a0<<6)|(a1<<7)|28)<<15 | 0x30004000 */
		b[2] = 0x412;		/* REGS {ST, RGBAQ, XYZF2} */
		b[3] = 0;
	}
}

/* write the giftag block (real: tower_218210) - part of the param
 * window */
static void
towerPatchGifTag(void)
{
	u32 *g = &TowerGifTag;
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
 *   scaled by (towerB/128 or towerA/30), f3 = the side-face factor
 *   (real table at 0x27af38, first entry 0.5). */
static void
towerPatchVertices(int r, int c)
{
	float f0 = towerA[r][c];
	int   bint = (int)towerB[r][c];
	float f12 = towerGrid[r+3][c+6] * (bint != 0 ? (float)bint/128.0f : towerA[r][c]/30.0f);
	float f3 = 0.5f;
	int b, v;

	for(b = 0; b < 6; b++) {
		float *pkt = (float*)towerBlocks[b];
		float s = b == 0 ? f12 : f12*f3;
		for(v = 0; v < 4; v++) {
			/* vertex z = word 2 of the vertex qword at
			 * +0x10 + v*16 */
			pkt[(0x18 + v*16)/4] = towerSignFlags[b][v] ? f0 : -f0;
			/* colours at +0x90 + v*16 */
			pkt[(0x90 + v*16)/4 + 0] = f0 <= 0.0f ? 0.0f : s;
			pkt[(0x90 + v*16)/4 + 1] = f0 <= 0.0f ? 0.0f : s;
			pkt[(0x90 + v*16)/4 + 2] = f0 <= 0.0f ? 0.0f : s;
			pkt[(0x90 + v*16)/4 + 3] = 128.0f;
		}
	}
}

/* patch the st texture coords into the 6 packets (real: tower_2184d0):
 * a UNIT quad {u,u},{u+1,u},{u,u+1},{u+1,u+1} with u = a0/256, z=1, w=0,
 * into all 6 packets at +0xd0.  PRIM is 0x304e4000 = 0x9c (TRISTRIP,
 * IIP, TME, FST=0 - S,T are NORMALIZED fractions of the texture, which
 * is why u=a0/256 lines up with TEXOWAL0's 256px width) sampling
 * TEX0_1/TEX1_1 = TEXOWAL0 (the wall texture, set by vif1SetTextureMIP
 * in DrawTowers).
 *
 * a0 traced to the caller (DrawTowers 0x218a08-0x218a3c): a0 = with
 * plain r,c: (r+c+9)*(r+8)/(c+7) + (r+c+9)*(r+7)/(c+9), all integer
 * division. */
static void
towerPatchST(int r, int c)
{
	int a0 = (r+c+9)*(r+8)/(c+7) + (r+c+9)*(r+7)/(c+9);
	float u = (float)a0/256.0f;
	static const float st[4][2] = {
		{ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }
	};
	int b, v;

	for(b = 0; b < 6; b++) {
		float *pkt = (float*)towerBlocks[b];
		for(v = 0; v < 4; v++) {
			pkt[(0xd0 + v*16)/4 + 0] = u + st[v][0];
			pkt[(0xd0 + v*16)/4 + 1] = u + st[v][1];
			pkt[(0xd0 + v*16)/4 + 2] = 1.0f;
			pkt[(0xd0 + v*16)/4 + 3] = 0.0f;
		}
	}
}

static void
InitTowersFog(void)
{
	/* real OpeningInitTowersFog (0x218e00, 276 insns) has 8 steps
	 * (docs/towers-analysis.md); status of each here:
	 *   1. memset(towerFlags)                    - ported, below
	 *   2. parse memcard boot history into the    - NOT ported. We
	 *      field (bootHistory[] @0x1f0138,          generate a field with
	 *      21x22B entries) into towerC/D/          the same statistical
	 *      towerFlags + the SOURCE position         shape instead
	 *      table @0x28a670                          (RandomizeTowerField).
	 *   3. VU1 upload (sendDma/vu1Wait equivalent) - ported, below
	 *   4. transform 0x28a670 source positions    - NOT ported at all.
	 *      into towerPos: {(x+0.85)*4, (y-6.5)*4,   Only matters once
	 *      (z+4)*12+150} per cell                   step 2 is real -
	 *                                                RandomizeTowerField
	 *                                                writes already-final
	 *                                                towerPos, bypassing
	 *                                                this transform.
	 *   5. alpha/halo tables (towerA/B/towerPos.z) - ported, below
	 *   6. HeightGrid()                            - ported, below
	 *   7. InitFog()                               - ported, below
	 *   8. tail: j DrawToExtraBuf2                 - stubbed (empty), below
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

	/* the 4 light vectors (real spr+0x200 source matrix, transformed by
	 * the per-tower rotation into the light matrix).  The tower quads'
	 * normals are {0,0,1,1}. */
	towerStruct.quad[0][0] = 0.0f;  towerStruct.quad[0][1] = 0.0f;
	towerStruct.quad[0][2] = 1.0f;  towerStruct.quad[0][3] = 0.0f;
	towerStruct.quad[1][0] = 0.5f;  towerStruct.quad[1][1] = 0.5f;
	towerStruct.quad[1][2] = 0.0f;  towerStruct.quad[1][3] = 0.0f;
	towerStruct.quad[2][0] = -0.5f; towerStruct.quad[2][1] = -0.5f;
	towerStruct.quad[2][2] = 0.0f;  towerStruct.quad[2][3] = 0.0f;
	towerStruct.quad[3][0] = 0.0f;  towerStruct.quad[3][1] = 0.0f;
	towerStruct.quad[3][2] = 0.0f;  towerStruct.quad[3][3] = 1.0f;

	/* real OSDSYS: parse the memcard boot history (bootHistory[])
	 * into the tower field.  The memcard cell table + float tables
	 * are not implemented yet, so for now generate a field with the
	 * same statistical shape as a real one (RandomizeTowerField). */
	RandomizeTowerField();

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

	/* real: upload vucode_1 (the tower renderer): the CNT tag at the
	 * blob head (0x2678e0) pulls MPG(229 words) + 1832 bytes of
	 * microcode into VU1 code memory.  Real sequence after the kick is
	 * sceDmaSync(...) THEN sceGsSyncPath(0,0) (0x21904c/0x219058,
	 * disasm-verified 2026-08-24) - vu1Wait() stands in for the former,
	 * the explicit call below for the latter (previously missing). */
	sendDma(&TowerUpload);
	vu1Wait();
	sceGsSyncPath(0, 0);

	/* real: VU1 prepass on the height grid (D1 kick + wait), then RPC.
	 * Unconditional regardless of the toggles above: fog depends on
	 * this. */
	HeightGrid();
	InitFog();

	DrawToExtraBuf2();	/* real: tail call, j not jal (see step 8 above) */
}

static void
DrawTowers(void)
{
	int r, c;

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
	 * (0x218650-0x2186f4) but fully traced 2026-08-25 and confirmed to be
	 * a mathematical no-op: it computes a per-cell camera displacement,
	 * but the gate it feeds always reads back cell (0,0)'s value (a fixed
	 * pointer, never reassigned) as |dx|+|dy| - a sum of absolute values,
	 * so always >= 0, so its "skip if <= 0" gate never actually skips
	 * anything for any real camera position. Intentionally not ported -
	 * see docs/towers-analysis.md for the full trace. */

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
			/* real: towerGrid[r+3][c+6], see towerPatchVertices */
			if(towerGrid[r+3][c+6] <= 0.0f)
				continue;

			/* special path when towerC == 1.0 (real: grid cell
			 * set to -1.0) */
			if(towerC[r][c] == 1.0f)
				towerGrid[r+3][c+6] = -1.0f;

			/* per-cell Z rotation term (disasm-verified, DrawTowers
			 * 0x218830-0x218920, both towerC branches): q = (row+
			 * col+9)*(row+3) / 7 (integer div); term = (q mod 4) *
			 * PI/2, a quantized 90-degree-step rotation per cell.
			 * Real ROM behaviour, correctly ported - but per aap's
			 * testing and a "null-result proof" in towers-analysis.md,
			 * it's mathematically invisible for this geometry (a
			 * rigid co-rotation of a square, uniformly-textured box
			 * at exactly 90-degree steps), so don't expect a visual
			 * change from this term alone. */
			q = (r+c+9)*(r+3)/7;
			term = (float)(q % 4) * PI_2;
			angles[0] = towerAngleTab[0];
			angles[1] = towerAngleTab[1];
			angles[2] = towerAngleTab[2] + term
					+ (towerC[r][c] == 1.0f ? 0.0f : towerSway);
			angles[3] = towerAngleTab[3];

			/* real helpers 0x267370 / 0x2676b0 / 0x267860 - IN IDA
			 * these are ONE call each (sceVu0RotMatrix/TransMatrix/
			 * MulMatrix), not the several calls below - disassembled
			 * libvu0.a directly (2026-08-24) to confirm this expansion
			 * is exact, not just "close enough":
			 *   sceVu0RotMatrix(m0,m1,rotVec) body IS literally
			 *     RotMatrixZ(m0,m1,rotVec.z); RotMatrixY(m0,m0,rotVec.y);
			 *     RotMatrixX(m0,m0,rotVec.x);  <- exactly the 3 calls below
			 *   sceVu0TransMatrix(m0,m1,tv) body IS literally
			 *     copy m1's rows 0-2 to m0; m0.row3.xyz = m1.row3.xyz+tv;
			 *     row3.w untouched  <- exactly CopyMatrix + the row-3 add
			 *   sceVu0MulMatrix(m0,m1,m2) body IS, per row i:
			 *     m0.row[i] = m2.row[i] applied through m1 (same vmulax/
			 *     vmadday/vmaddaz/vmaddw as sceVu0ApplyMatrix)  <- exactly
			 *     the 4x sceVu0ApplyMatrix loop below
			 * So this block is a verified-exact, just manually-inlined,
			 * expansion of those 3 SDK calls - not a mismatch. */
			sceVu0RotMatrixZ(towerStruct.rotated, towerStruct.base, angles[2]);
			sceVu0RotMatrixY(towerStruct.rotated, towerStruct.rotated, angles[1]);
			sceVu0RotMatrixX(towerStruct.rotated, towerStruct.rotated, angles[0]);

			pos[0] = towerPos[r][c][0] + towerPosTab[0];
			pos[1] = towerPos[r][c][1] + towerPosTab[1];
			pos[2] = towerPos[r][c][2] + towerPosTab[2];
			pos[3] = towerPos[r][c][3] + towerPosTab[3];
			sceVu0CopyMatrix(towerStruct.translated, towerStruct.rotated);
			/* real 0x2676b0: out = rotation with the position added to
			 * ROW 3 (vadd.xyz rot-row3 += pos).  The VU program applies
			 * the model matrix TRANSPOSED (MulMatrix computes A x B^T,
			 * so the vertex gets p.R + t.w).  A column-3 translation
			 * lands in the w channel as pos.p + w and blows up the
			 * perspective divide (big scaling/flickering artefacts). */
			towerStruct.translated[3][0] += pos[0];
			towerStruct.translated[3][1] += pos[1];
			towerStruct.translated[3][2] += pos[2];
			for(k = 0; k < 4; k++)
				sceVu0ApplyMatrix(towerStruct.transformed[k],
						towerStruct.rotated, towerStruct.quad[k]);

			/* real per-tower sequence: giftag (params window),
			 * block NOPs, 218318 (vertex z + colours), 2184d0
			 * (st coords), params, ONE chain kick at the head */
			towerPatchGifTag();
			towerPatchTags();
			towerPatchVertices(r, c);
			towerPatchST(r, c);
			towerPatchParams();
			/* real: sync BEFORE the kick, no wait after - pipelined,
			 * not serialized per-tower (see towerKick's comment) */
			towerKick(&TowerChain);
		}
	}
	/* real: ONE sceGsSyncPath(0,0) after the whole field, not per-tower
	 * (0x218ad8, disasm-verified 2026-08-24) */
	towerSyncEnd();
}

/* ==== NOT PORTED: the "flare and illegal stuff" init chain ====
 * real InitOpening calls sub_219f08 here, between OpeningInitTowersFog and
 * StartFrame (surveyed 2026-08-24). sub_21b690 is ALSO called once from
 * OpeningInitLightsCubes (shared helper) - not wired in there since that
 * function is otherwise fully verified/ported and this session didn't
 * trace sub_21b690 itself, so no confident call site to add for the
 * light-cube side yet. */

/* real: sub_21a438 (0x21a438, 168 insns) - real callees are just cosf/sinf
 * (no stub needed), not investigated further. */
static void
sub_21a438(void)
{
}

/* real: sub_215798 (0x215798, 161 insns) - makes 4 unconditional rand()
 * calls (disasm-verified 2026-08-24, see docs/towers-analysis.md): this is
 * the actual real function the earlier placeholder comment described.
 * lightsSeed's phase depends on rand() call COUNT, so keep these 4 calls
 * even though the rest of the function isn't traced. */
static void
sub_215798(void)
{
	rand(); rand(); rand(); rand();
}

/* real: sub_21b690 (0x21b690, 65 insns) - shared with OpeningInitLightsCubes
 * (see comment above), not investigated. */
static void
sub_21b690(void)
{
}

/* real: sub_219cb8 (0x219cb8, 123 insns) - real callee is sub_21b690 above. */
static void
sub_219cb8(void)
{
	sub_21b690();
}

/* real: sub_219f08 (0x219f08, 14 insns) - InitOpening's real call, wraps
 * the three functions above. */
static void
sub_219f08(void)
{
	sub_21a438();
	sub_215798();
	sub_219cb8();
}

/* real: initTextShit (0x214f20, 13 insns) - IDA's own name (leaf, no
 * calls out), InitOpening's real call right before StartFrame. */
static void
initTextShit(void)
{
}

static void
Init(void)
{
	openingType = nextOpeningType = fooOpeningType;

	/* real InitOpening order: OpeningInitRender, OpeningInitAnimation,
	 * OpeningInitTowersFog, sub_219f08, initTextShit, StartFrame -
	 * confirmed complete 2026-08-24, nothing else in between. */
	InitRender();
	InitAnimation();
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
