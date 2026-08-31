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
/* the pad library's own header is libopad.h and it belongs to pad.c
 * alone - see the header comment there for why it is not libpad.h */

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
void vif1SetFramebuffer(u32 fbp, u16 psm, int width, int height, int clear);
void vif1SetTexRect(Rect *r, Rect *tr, Color *col, u32 abe, u32 z);
/* the two spare screen-sized GS buffers InitRender allocates; the menu
 * backdrop's composite reuses them as Module U's 3*w*h / 4*w*h work
 * buffers (menuback.c) */
extern u32 extraBuf1, extraBuf2;
extern int frameCount;
extern int hwFrameLimit;
int OsdArgInt(int n, int def);

/* ---- menu.c: the main-menu 3D background scene ---- */
void InitMenuScene(void);
void DoMenuScene(void);

/* the deferred, depth-sorted draw list (real: the dummy head at
 * 0x34E980 with 320-byte records after it).  Type 1 is an orb
 * (0x225ED0), type 0 one of the twelve System Configuration fly-in rods
 * (0x225DD8); the mesh half of the record is the subset of the ROM's
 * 224-byte scene-struct copy that menuconfig.c actually reads. */
typedef struct SceneRec SceneRec;
struct SceneRec
{
	SceneRec *next;		/* real +0x000 */
	float key;		/* real +0x004 view depth */
	sceVu0FMATRIX world;	/* real +0x030 */
	int type;		/* real +0x0F0 */
	float f12;		/* real +0x0F4 the front rod's split */
	int index;		/* real +0x130 orb index / ring slot */
	float progress;		/* real: the scene struct's +0x6C */
	float size;		/* real: the scene struct's +0x90 */
	int col0[4];		/* real: the scene struct's +0x80 */
	int col1[4];		/* real: the scene struct's +0xC0 */
	int colA[4];		/* real +0x100 */
	int colB[4];		/* real +0x120 */
	int aux;		/* real +0x110 */
};

void SceneAddMesh(sceVu0FMATRIX world, int slot, float progress, float size,
	float split, const int *col0, const int *col1,
	const int *colA, const int *colB, int aux);

/* menu.c's software clock (real: the 0x352980 block's accessors
 * 0x22B5E8 / 0x22B6B0 / 0x22B720) */
float MenuClockSeconds(void);
float MenuClockMinutes(void);
float MenuClockHours(void);

/* menu.c's matrix layer, in plain C because freesce's libvu0 is broken
 * (see the header comment there); the camera and view-screen matrices
 * 0x21CFD8 rebuilds every frame; and the 0x230000 MatrixDrive stack. */
extern sceVu0FMATRIX menuCamera, menuViewScreen, mdTop;
void matUnit(sceVu0FMATRIX m);
void matCopy(sceVu0FMATRIX d, sceVu0FMATRIX s);
void matMul(sceVu0FMATRIX d, sceVu0FMATRIX a, sceVu0FMATRIX b);
void matApply(sceVu0FVECTOR o, sceVu0FMATRIX m, sceVu0FVECTOR v);
void mdRotX(int a);
void mdRotY(int a);
void mdRotZ(int a);
void mdTranslatef(float x, float y, float z);

/* ---- menuconfig.c: the System Configuration screen ---- */
void InitMenuConfig(void);
void MenuEnterConfig(void);	/* real: 0x227268 */
void MenuLeaveConfig(void);	/* real: the closing arm of 0x227390 */
void MenuConfigStep(void);	/* real: 0x227DE8 */
void MenuConfigCarousel(void);	/* real: 0x225BF8 */
void MenuConfigEmit(void);	/* real: 0x226028 */
int MenuConfigCarouselActive(void);
void MenuConfigDrawMesh(SceneRec *rec);
void MenuConfigCubes(void);	/* real: 0x226FA8 */
int MenuConfigOpen(void);
int MenuConfigAlpha(int fadeAlpha);
int MenuConfigItemPos(int i, float *x, float *y);
void MenuConfigSetCursor(int n);

/* ---- menutext.c: the readback diagnostic, run from DoMenuScene's
 * post-swap window ---- */
int MenuTextDumpFrame(void);
void MenuTextDump(int par);

/* ---- menuback.c: the TEXCKABE backdrop tunnel and the composite ---- */
void InitMenuBackdrop(void);
void MenuBackFrameStart(void);
void MenuBackdrop(sceVu0FMATRIX cam, sceVu0FMATRIX vs, int fadeMode);
void MenuZoomBlur(void);
void MenuBackFadeOpen(void);	/* real: 0x2291E8 */
void MenuBackFadeClose(void);	/* real: 0x229230 */
int MenuBackdropVisible(void);
void MenuBackBindScreenCopy(void);
int MenuBackField(void);
/* the work-buffer stage the glass renders through (real: 0x22A198,
 * 0x22A290, 0x22A4C8, 0x22A3B8 and the four sprite records 0x27F6E0 /
 * 0x27F720 / 0x27F760 / 0x27F7E0 that 0x22C088, 0x22C100, 0x22C190 and
 * 0x22C2A0 draw).  buf 0 = the ROM's work buffer 3 (extraBuf1), 1 = work
 * buffer 4 (extraBuf2). */
void MenuBackBindWork(int buf);		/* real: 0x22A290(n) */
void MenuBackBindScreen(void);		/* real: 0x22A198(evenOddFrame) */
void MenuBackWorkTarget(int buf, int clear, int field);	/* real: 0x22A4C8 */
void MenuBackScreenTarget(int field);	/* real: 0x22A3B8 */
void MenuBackWorkAdd(void);		/* real: 0x22C088 */
void MenuBackWorkHalfAdd(void);		/* real: 0x22C100 */
void MenuBackWorkOver(int abe);		/* real: 0x22C190(abe) */
void MenuBackWorkBlur(int n);		/* real: 0x22C2A0 */
int MenuBackPhase(void);		/* real: *(gp-28844) */

/* ---- menutext.c: the menu's 2D text/item layer ---- */
void InitMenuText(void);
void MenuTextFrame(int fadeMode, int fadeAlpha);

/* ---- pad.c: the port-0 controller (NOT original - the retail OSDSYS
 * gets its buttons from the OSD system module) ---- */
enum {
	PAD_L2       = 0x0001,
	PAD_R2       = 0x0002,
	PAD_L1       = 0x0004,
	PAD_R1       = 0x0008,
	PAD_TRIANGLE = 0x0010,
	PAD_CIRCLE   = 0x0020,
	PAD_CROSS    = 0x0040,
	PAD_SQUARE   = 0x0080,
	PAD_SELECT   = 0x0100,
	PAD_L3       = 0x0200,
	PAD_R3       = 0x0400,
	PAD_START    = 0x0800,
	PAD_UP       = 0x1000,
	PAD_RIGHT    = 0x2000,
	PAD_DOWN     = 0x4000,
	PAD_LEFT     = 0x8000
};

typedef struct Pad Pad;
struct Pad
{
	int connected;		/* padman answered a read this frame */
	u16 btns;		/* held right now */
	u16 press;		/* went down this frame */
	u16 release;		/* went up this frame */
	u16 dirs;		/* the four directions, dpad OR left stick */
	u16 dirPress;		/* their edges, plus the held-key repeat */
	/* sticks, -1..1 with the deadzone applied; down/right positive.
	 * Zero unless the pad is in analog mode. */
	float lx, ly;
	float rx, ry;
};

extern Pad pad;			/* the pad in port 0 */

int InitPad(void);		/* brings up the SIF RPC layer too */
void UpdatePad(void);		/* once per frame */

/* ---- menu.c: what the menu's confirm button ends up calling ---- */
void MenuSelectItem(int n);
