#include <eekernel.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <libdev.h>
#include <eeregs.h>
#include <libgraph.h>
#include <libdma.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>
#include <sifrpc.h>
#include <libpad.h>

#define nil NULL
typedef u_long128 u128;
typedef u_long u64;
typedef unsigned int u32;
typedef int i32;
typedef unsigned short u16;
typedef short i16;
typedef unsigned char u8;
typedef signed char i8;

#define ALIGN16 __attribute__ ((aligned(16)))

#define PI 3.1415927f
#define TAU (2.0f*PI)

#define UNCACHED(a) ((void*)(((u32)a)|0x20000000U))
#define ACCEL(a) ((void*)(((u32)a)|0x30000000U))
#define NORMALMEM(a) ((void*)(((u32)a)&0x0FFFFFFFU))
#define DMASPR(a) ((void*)((((u32)a)&0x0FFFFFFFU)|0x80000000U))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define clamp(a, lo, hi) ((a) < (lo) ? (lo) : (a) > (hi) ? (hi) : (a))

typedef struct Rect Rect;
struct Rect
{
	int x, y;
	int w, h;
};

typedef struct Color Color;
struct Color
{
	u32 r, g, b, a;
};

/* texture descriptors - shared between the opening (opening.c) and the
 * menu background scene (menu.c), both of which upload through
 * InitTexture()/UploadImage() and bind through vif1SetTexture(). */
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

int IsPAL(void);
int GetLanguage(void);

/* OSD glue stubs (see main.c) */
extern int discReady;
extern int discType;
extern int bootLatch;
extern int osdBootParam;
extern int osdBootParamC;
extern int osdBootParam2;
int HasDisc(void);
int GetDiscType(void);
int BootLatchClear(void);
void OSDDispatch(int msg, int a, int b, int c);
void OSDDispatch2(int x, int msg, int a, int b, int c);

extern sceGsDBuff db;
extern int evenOddFrame;
extern int evenOddField;
extern int screenW, screenH;

extern int waitFrameSema;
extern int drawEndSema;
extern int swapSema;
extern int drawStartSema;

void StartFrame(void);
void WaitNextFrame(void);

int MakeOpeningThread(void);

/* ---- opening.c: the render/packet layer, shared with menu.c ---- */
void InitRender(void);
void InitTexture(Texture *tex);
void vif1Begin(void);
void vif1End(void);
void pktSetAD(u32 a, u64 d);
void pktSetAlphaBlend(u32 type, u32 mode, u32 fix);
void vif1SetAD(u32 a, u64 d);
void vif1SetTEST_1(u32 ate, u32 atst, u32 aref, u32 afail, u32 date, u32 datm, u32 zte, u32 ztst);
void vif1SetCLAMP_1(u32 wms, u32 wmt, u32 minu, u32 maxu, u32 minv, u32 maxv);
void vif1SetAlphaBlend(u32 type, u32 mode, u32 fix);
void vif1SetZTest(int enb);
void vif1SetZWrite(int enb);
void vif1SetXYOffset(int field, int halfpx);
void vif1SetTexture(Texture *tex);
extern int frameCount;
extern int hwFrameLimit;
int OsdArgInt(int n, int def);

/* ---- menu.c: the main-menu 3D background scene ---- */
void InitMenuScene(void);
void DoMenuScene(void);
