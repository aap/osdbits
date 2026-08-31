/* The System Configuration screen - Module U's second screen, and the
 * only one that shows the whole background scene.
 *
 * Entering it (0x227268, reached from the main menu's input handler
 * 0x228278 when the cursor is on item 1) opens four timers, and every
 * visible difference between the main menu and this screen follows from
 * one of them:
 *
 *   0x27BE44  the screen's own Anim - gates the 2D item list, and its
 *             count drives everything else's phase
 *   0x27F190  the backdrop fade timer (0x2291E8) - the TEXCKABE tunnel
 *   0x27EB00  the carousel timer (0x225AD0) - the 12 glass rods
 *   0x27EC00  the cube timer (0x226B28, from the state machine 0x227390
 *             when the Anim's count reaches *(gp-30380)) - the 5 glass
 *             cubes, one per config item
 *
 * so the screen is: tunnel + orbs + a twelve-rod clock ring + five
 * spinning cubes.  The rods and the cubes are the same renderer over two
 * static meshes (res/MENUGEOM.inc, extracted from OSDSYS's own .data):
 *
 *   0x27E950  16 faces  a hexagonal prism 26 units tall, radius 2.6,
 *                       bevelled to 2.34 at y = 26..26.39 - one hour rod
 *   0x27EFB0   6 faces  a cube, half-extent 2.64 - one config item
 *
 * Hex addresses are retail-image addresses; gp = 0x2AF070.  See
 * docs/menu-text.md 6 for the carousel groundwork this builds on. */

#include <stdio.h>
#include "inc.h"
#include "res.h"

#include "res/MENUGEOM.inc"

/* ======================= the animation timer =======================
 * real 0x22AC10 (count), 0x22AC18 (raw count), 0x22AC20 (interp),
 * 0x22AC48 (isState), 0x22AC60 (reset), 0x22AC70 (open), 0x22AC90
 * (close), 0x22ACC0 (step).  Same four words menutext.c models. */

typedef struct CfgTimer CfgTimer;
struct CfgTimer
{
	int duration, count, edge, state;
};

static int cfgIsState(CfgTimer *t, int s)	{ return t->state == s; }
static int cfgCount(CfgTimer *t)		{ return t->count; }

static void
cfgOpen(CfgTimer *t)		/* real: 0x22AC70 */
{
	if(t->state == 0) {
		t->count = 0;
		t->edge = 1;
		t->state = 1;
	}
}

static void
cfgClose(CfgTimer *t)		/* real: 0x22AC90 */
{
	if(t->state == 2) {
		t->edge = 1;
		t->count = t->duration;
		t->state = 3;
	}
}

static void
cfgStep(CfgTimer *t)		/* real: 0x22ACC0 */
{
	t->edge = 0;
	if(t->state == 1) {
		if(++t->count == t->duration) {
			t->edge = 1;
			t->state = 2;
		}
	} else if(t->state == 3) {
		if(--t->count == 0) {
			t->edge = 1;
			t->state = 0;
		}
	}
}

/* real: 0x228460's three durations, all derived from the refresh rate.
 * dur40 (gp-30400) = rate*40/60, dur10 (gp-30396) = rate/6, and dur80
 * (gp-30380) is the long leg the config Anim's first phase uses. */
static int cfgDur40, cfgDur10, cfgDur80;

static CfgTimer cfgAnim;	/* real: 0x27BE44, the screen's own Anim */
static CfgTimer cfgCubeTimer;	/* real: 0x27EC00 */
static CfgTimer carouselTimer;	/* real: 0x27EB00 */

/* ================== the carousel ring (0x34E6C0) ==================
 *
 * Twelve entries, stride 48.  Entry 0's first two words double as the
 * ring's header: `offset' is which slot is at the front (seeded from
 * the hour), and the two shorts after it are the ring's own spin and
 * tilt, eased toward the clock every frame by 0x225628. */

#define NRING 12

typedef struct RingSlot RingSlot;
struct RingSlot
{
	float progress;		/* real +0x10, 0..1, driven by 0x27EB00 */
	float split;		/* real +0x14, the front slot's rod split */
	int col0[4];		/* real +0x20 */
	int col1[4];		/* real +0x30 */
};

static RingSlot ring[NRING];
static int ringOffset;		/* real: *(int*)0x34E6C0 */
static short ringSpinY;		/* real: *(short*)0x34E6C4 */
static short ringTiltZ;		/* real: *(short*)0x34E6C6 */
static float ringSplitMax;	/* real: *(float*)0x34E930 = 1 - minutes/60 */
static int ringColA[4];		/* real: 0x34E910 */
static int ringColB[4];		/* real: 0x34E920 */

/* real 0x27EAC0 / 0x27EAD0 / 0x27EAE0 / 0x27EAF0 - the four colour
 * vectors 0x225318 spreads over the ring: the plain slots get 0x27EAC0
 * and 0x27EAD0, the front slot a blend of 0x27EAC0 with {167,217,255}
 * (0x225878) and 0x27EAF0.  The keyframe cyclers 0x225528/0x2255A8 that
 * animate them over 8 entries are not ported - the tables' idle values
 * are used directly. */
static const int cfgColBody[4]  = { 0x2D, 0x55, 0x66, 0x80 };	/* 0x27EAC0 */
static const int cfgColEdge[4]  = { 0x3C, 0x3C, 0x3C, 0x80 };	/* 0x27EAD0 */
static const int cfgColRingA[4] = { 0x3C, 0x3C, 0x3C, 0x80 };	/* 0x27EAE0 */
static const int cfgColRingB[4] = { 0x80, 0x80, 0x80, 0x1E };	/* 0x27EAF0 */
/* real 0x225878: the front slot's body colour is the average of
 * 0x27EAC0 and the fixed {167,217,255} at 0x34E940 */
static const int cfgColFront[4] = { (0x2D+167)/2, (0x55+217)/2, (0x66+255)/2, 0x80 };

/* ==================== the mesh renderer ====================
 *
 * One transform pass (0x22CFA8's tail) into a per-face scratch record,
 * then one emit pass per draw pass (0x22C888 -> 0x22C4E0).  The ROM
 * keeps 32+ of the 352-byte records at 0x3529D0 and a second bank at
 * 0x3555D0 (= 0x3529D0 + 32*352) for the split copy of the front rod;
 * this port transforms one object at a time, so one bank is enough. */

#define MAXFACES 16

typedef struct MeshVertex MeshVertex;
struct MeshVertex
{
	sceVu0FVECTOR cam;	/* real +0x00 camera space */
	sceVu0FVECTOR proj;	/* real +0x20 after the w divide */
	float q;		/* real +0x40 = 1/w */
};

typedef struct MeshFace MeshFace;
struct MeshFace
{
	MeshVertex v[4];
	sceVu0FVECTOR normal;	/* real +0x140 */
	int cull;		/* real +0x150, the screen-space winding */
};

typedef struct MeshModel MeshModel;
struct MeshModel
{
	int nfaces;
	const float (*verts)[4];
	const float (*norms)[4];
};

static MeshFace meshFaces[MAXFACES];
static float meshObjX, meshObjY;	/* 0x22CFA8's outX/outY */

/* NOT original (argv[14]): 0 draws the meshes untextured, which is the
 * geometry on its own; 1 is the ROM's screen-space refraction sampling. */
static int cfgMeshTex = 1;

/* real: 0x22CFA8 - build camera x world, project the object's origin,
 * then transform every face's four vertices and its normal.  The scale
 * is the scene struct's +0x68/+0x6C/+0x70 triple; only +0x6C is ever
 * anything but 1.0 for the rod (it is the fly-in progress). */
static void
MeshTransform(MeshModel *mdl, sceVu0FMATRIX world, float sx, float sy, float sz)
{
	sceVu0FMATRIX m;
	sceVu0FVECTOR o, b, v;
	MeshFace *f;
	float q, e1x, e1y, e2x, e2y;
	int i, k;

	matMul(m, menuCamera, world);

	o[0] = o[1] = o[2] = 0.0f; o[3] = 1.0f;
	matApply(o, m, o);
	matApply(b, menuViewScreen, o);
	q = 1.0f/b[3];
	meshObjX = b[0]*q - 2048.0f;
	meshObjY = b[1]*q - 2048.0f;

	for(i = 0; i < mdl->nfaces; i++) {
		f = &meshFaces[i];
		/* the normals carry w = 0, so this is the rotation only */
		v[0] = mdl->norms[i][0];
		v[1] = mdl->norms[i][1];
		v[2] = mdl->norms[i][2];
		v[3] = 0.0f;
		matApply(f->normal, m, v);

		for(k = 0; k < 4; k++) {
			v[0] = mdl->verts[i*4+k][0] * sx;
			v[1] = mdl->verts[i*4+k][1] * sy;
			v[2] = mdl->verts[i*4+k][2] * sz;
			v[3] = 1.0f;
			matApply(f->v[k].cam, m, v);
			matApply(f->v[k].proj, menuViewScreen, f->v[k].cam);
			q = 1.0f/f->v[k].proj[3];
			f->v[k].proj[0] *= q;
			f->v[k].proj[1] *= q;
			f->v[k].proj[2] *= q;
			f->v[k].proj[3] = 1.0f;
			f->v[k].q = q;
		}

		/* real: two 0x2676F8 (SubVector) calls on the PROJECTED
		 * positions of vertices 2 and 1 against vertex 0, then the z
		 * of their cross product - a screen-space winding test.  The
		 * flag is 1 for a back face; every draw pass keys on it. */
		e2x = f->v[2].proj[0] - f->v[0].proj[0];
		e2y = f->v[2].proj[1] - f->v[0].proj[1];
		e1x = f->v[1].proj[0] - f->v[0].proj[0];
		e1y = f->v[1].proj[1] - f->v[0].proj[1];
		f->cull = (e2x*e1y - e2y*e1x) > 0.0f;
	}
}

/* real: 0x230068 (mdCos) over the quarter-wave table at 0x3581F0 */
static float
cfgCosf(int a)
{
	return cosf((short)a * (TAU/65536.0f));
}

/* real: the emit half of 0x22C4E0.  Every vertex is drawn 5 % of the way
 * toward the object's own screen centre (*(gp-32068) = 0.95) and pushed
 * by the camera-space face normal scaled by 1/w - a cheap refraction
 * offset, 1000 units horizontally and 500 vertically.
 *
 * The UV is NOT the model's: it is the vertex's own SCREEN position, so
 * the surface samples the copy of the frame taken before the object list
 * ran (menuback.c's extraBuf1, the ROM's work buffer 3).  That is what
 * makes these look like glass, and it is the "bumpmap-like" effect the
 * cubes show.  The two biases the ROM adds - 1024 to U (clamped, so a
 * negative U comes out 0 and the 14-bit UV field takes the modulo) and
 * 256 to V - are its own; they are reproduced verbatim. */
static void
MeshEmitFace(MeshFace *f, float fres, const int *col, float size, int extra)
{
	int r, g, b, bright, prim, k;
	float bx, u, v, nx, ny;

	/* real: size * 10.0 * fres^4, then a cosine rolloff over the last
	 * tenth so a face seen exactly edge-on goes dark again */
	bx = size * 10.0f * fres*fres*fres*fres;
	bright = (int)bx;
	if(fres > 0.9f) {		/* real *(gp-32080) */
		float t = (1.0f - fres) * 32768.0f / 0.1f;	/* *(gp-32076) */
		bright = (int)((float)bright * (1.0f - cfgCosf((int)t)) * 0.5f);
	}

	r = bright + col[0] + extra; if(r >= 256) r = 255;
	g = bright + col[1] + extra; if(g >= 256) g = 255;
	b = bright + col[2] + extra; if(b >= 256) b = 255;

	/* real: 276 / 404 - TRIANGLE_STRIP | TME | FST, with AA1 added
	 * for a nearly edge-on face (*(gp-32072) = 0.99) */
	prim = SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, cfgMeshTex, 0, 0,
		fres > 0.99f, 1, 0, 0);

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, prim);
	/* real: the REGLIST template at 0x27F890 is {PRIM, CLAMP_1, CLAMP_1,
	 * RGBAQ, (UV, XYZ2) x4} and 0x22C4E0 writes 0x01000000 into both
	 * CLAMP_1 slots - WMS = WMT = REPEAT (the MINV bit it also sets is
	 * dead in REPEAT mode).  That override is load-bearing: it is what
	 * undoes the two biases below.  sceGsSetDefTexEnv left CLAMP/CLAMP
	 * behind, under which the whole surface samples one clamped row. */
	pktSetAD(SCE_GS_CLAMP_1, 0x01000000);
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(r, g, b, 0x80, 0));
	for(k = 0; k < 4; k++) {
		nx = f->normal[0] * 1000.0f * f->v[k].q;
		ny = f->normal[1] *  500.0f * f->v[k].q;
		u = (f->v[k].proj[0] - 2048.0f - meshObjX)*0.95f + meshObjX - nx;
		v = (f->v[k].proj[1] - 2048.0f - meshObjY)*0.95f + meshObjY - ny;
		u += screenW/2 + 1024.0f;
		if(u < 1024.0f)
			u = 1024.0f;
		v += screenH/2 + 256.0f - evenOddField*0.5f;
		pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(((int)(u*16.0f)) & 0x3FFF,
			((int)(v*16.0f)) & 0x3FFF));
		pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ((int)(f->v[k].proj[0]*16.0f),
			(int)(f->v[k].proj[1]*16.0f), (int)(f->v[k].proj[2]*16.0f)));
	}
	vif1End();
}

/* real: 0x22C888 - the Fresnel term.  normalize(vertex 0 in camera
 * space) dotted with the camera-space face normal, so 0 head-on and 1
 * edge-on: the glass is bright only at its silhouette. */
static void
MeshDrawPass(MeshModel *mdl, const int *col, float size, int back, int extra)
{
	MeshFace *f;
	float d, l, fres;
	int i;

	for(i = 0; i < mdl->nfaces; i++) {
		f = &meshFaces[i];
		if((f->cull != 0) != back)
			continue;
		l = sqrtf(f->v[0].cam[0]*f->v[0].cam[0] +
		          f->v[0].cam[1]*f->v[0].cam[1] +
		          f->v[0].cam[2]*f->v[0].cam[2]);
		if(l == 0.0f)
			continue;
		d = (f->v[0].cam[0]*f->normal[0] +
		     f->v[0].cam[1]*f->normal[1] +
		     f->v[0].cam[2]*f->normal[2]) / l;
		fres = 1.0f - (d < 0.0f ? -d : d);
		MeshEmitFace(f, fres, col, size, extra);
	}
}

/* real: the five passes of 0x22D920's f12 <= 0 arm (0x22E0EC).  The ROM
 * spreads them over three render targets - front faces into work buffer
 * 4 (0x22BFD0(1,0,1)), two more TEXCBUMP passes through 0x22C920 into
 * the same buffer, back faces onto the screen (0x22C020(1,0,1)) and a
 * fifth back-face pass into work buffer 3 - and 0x2267E8 then adds work
 * buffer 4 back over the screen twice at alpha 30.
 *
 * The port collapses that to the two untextured-geometry passes that
 * carry the shape, both straight onto the screen and both sampling the
 * pre-object copy of the frame (extraBuf1 = the ROM's work buffer 3):
 * back faces first, then front faces.  The TEXCBUMP passes and the
 * offscreen bloom are NOT ported. */
static int cfgDebug;		/* NOT original: menu.c's debugFrame */

static void
MeshDraw(MeshModel *mdl, sceVu0FMATRIX world, float sx, float sy, float sz,
	const int *col, float size)
{
	MeshTransform(mdl, world, sx, sy, sz);

	if(cfgDebug && frameCount == cfgDebug) {
		int i, front = 0;
		for(i = 0; i < mdl->nfaces; i++)
			front += meshFaces[i].cull == 0;
		printf("mesh %d faces at %.1f %.1f, scale %.2f, %d front, v0 %.0f %.0f\n",
			mdl->nfaces, meshObjX, meshObjY, sy, front,
			meshFaces[0].v[0].proj[0] - 2048.0f,
			meshFaces[0].v[0].proj[1] - 2048.0f);
	}

	vif1SetZWrite(0);
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 5, 128);	/* real: 0x22A0C0(1,1)/(1,2) */
	if(cfgMeshTex)
		MenuBackBindScreenCopy();

	MeshDrawPass(mdl, col, size, 1, 0);
	MeshDrawPass(mdl, col, size, 0, 0);
}

/* ==================== the models ==================== */

static MeshModel rodModel  = { 0, nil, nil };
static MeshModel cubeModel = { 0, nil, nil };

/* ============== the deferred record's mesh half ==============
 *
 * real: 0x2266E0 unpacks the record into 0x22D920(rec+0x10, rec+0x100,
 * f12 = rec->0xF4, f13 = (float)rec->0x110).  0x22D920's f12 <= 0 arm
 * is the plain one-piece rod; its f12 > 0 arm (only the front slot, once
 * its `split' has grown) cuts the rod in two along Y and draws the upper
 * half translated by 26 * progress * split.  Only the one-piece arm is
 * ported. */
void
MenuConfigDrawMesh(SceneRec *rec)
{
	if(rec->progress < 0.0f)
		return;
	MeshDraw(&rodModel, rec->world, 1.0f, rec->progress, 1.0f,
		rec->col0, rec->size);
}

/* ================= the carousel (0x225BF8 / 0x226028) ================= */

/* real: 0x225F80 - the matrix chain that places rod i on the ring */
static void
CarouselMatrix(int i, int angZ, int angY)
{
	matUnit(mdTop);
	mdRotZ(angZ);
	mdRotY(angY);
	mdRotZ((i*65536)/12 - 32768);	/* the slot's place on the ring */
	mdTranslatef(0.0f, 20.0f, 0.0f);
	mdRotY(angY*4);			/* each rod spins 4x the ring */
}

/* real: 0x226028 - append the twelve fly-in records, front slot first.
 * The ROM's `size' (scene+0x90) is 200 for the front object and 160 for
 * the rest, and only the front one gets a non-negative split. */
void
MenuConfigEmit(void)
{
	int i, slot;

	/* real: the emitter also copies the slot's two colour qwords into
	 * the scene struct at +0x80 and +0xC0; SceneAddMesh carries them */
	for(i = 0; i < NRING; i++) {
		slot = (i + ringOffset) % NRING;
		CarouselMatrix(i, ringTiltZ, ringSpinY);
		SceneAddMesh(mdTop, slot, ring[slot].progress,
			i == 0 ? 200.0f : 160.0f,
			i == 0 ? ring[slot].split : -1.0f,
			ring[slot].col0, ring[slot].col1,
			ringColA, ringColB, i == 0 ? 100 : 0);
	}
}

/* real: 0x2268F0's gate - the emitter only runs once the ring's
 * progress has passed *(gp-32148) = 0.05, which on the main menu never
 * happens because 0x27EB00 is closed there. */
int
MenuConfigCarouselActive(void)
{
	return ring[0].progress > 0.05f;
}

/* real: 0x225628's share of the per-frame update - re-derive the ring's
 * offset, tilt and spin from the clock and ease toward them at
 * *(gp-32168).  The port keeps menu.c's clock accessors. */
static void
CarouselClock(void)
{
	int target;

	ringOffset = (int)MenuClockHours() % NRING;

	/* real: tiltZ snaps to the slot's angle whenever |spinY| < 201,
	 * otherwise it eases; spinY always eases toward the second hand */
	target = (ringOffset << 16) / NRING;
	if(ringSpinY < 201 && ringSpinY > -201)
		ringTiltZ = (short)target;
	else
		ringTiltZ = (short)(ringTiltZ + (short)(target - ringTiltZ)*0.1f);
	target = (int)(MenuClockSeconds() * (65536.0f/60.0f));
	ringSpinY = (short)(ringSpinY + (short)(target - ringSpinY)*0.1f);

	ringSplitMax = 1.0f - MenuClockMinutes()*(1.0f/60.0f);
}

/* real: 0x225BF8 - stage 10 of the frame body.  Steps the carousel
 * timer, re-derives the clock angles (0x225978 -> 0x225628, 0x225878)
 * and refreshes every slot's progress from the timer. */
void
MenuConfigCarousel(void)
{
	int i;

	cfgStep(&carouselTimer);
	CarouselClock();

	for(i = 0; i < NRING; i++) {
		if(i != ringOffset)
			ring[i].split = 0.0f;
		else if(ring[i].split < ringSplitMax) {
			ring[i].split += 0.02f;	/* real *(gp-32164) */
			if(ring[i].split > ringSplitMax)
				ring[i].split = ringSplitMax;
		}
		ring[i].progress = carouselTimer.duration ?
			(float)(carouselTimer.count*128/carouselTimer.duration) * 0.0078125f : 0.0f;
	}
}

/* real: 0x225AD0 - the ONE opener of 0x27EB00, called only from
 * 0x2272C0 inside "enter System Configuration".  It resets the timer,
 * opens it and clears every slot's progress. */
static void
CarouselOpen(void)
{
	int i;

	if(!cfgIsState(&carouselTimer, 0))
		return;
	carouselTimer.count = 0;
	carouselTimer.edge = 0;
	cfgOpen(&carouselTimer);
	for(i = 0; i < NRING; i++)
		ring[i].progress = 0.0f;
}

/* ================== the five cubes (0x226D00) ==================
 *
 * The config items, as glass.  0x226FA8 (stage 5's second slot, right
 * after the zoom blur) steps 0x27EC00 and, while it is open, walks the
 * five-entry table at 0x27F090: each cube is placed at its fixed point,
 * spun on all three axes by one angle (*(gp-30432), which 0x2285C0 bumps
 * by 30 every frame, plus 7000 per cube), and scaled by the timer's
 * progress plus the entry's own bias.
 *
 * The ROM draws them twice - 0x22D2E8 to the screen and 0x22D798 into
 * work buffer 3 - then runs the work-buffer-3 twin of the zoom blur
 * (0x22BFD0/0x22C088/0x22C2A0) and blits the result back.  The port
 * draws the first pass only. */

static int cubeSpin;		/* real: *(u16*)(gp-30432) */

/* NOT original: where each cube ended up on screen, so menutext.c can
 * hang its label off it.  The ROM's five item widgets (0x21DF28 and
 * friends, reached through the 56-byte item records at 0x27BD10) place
 * their own rows and are not ported. */
static float cubeScreenX[5], cubeScreenY[5];
static int cubeScreenValid;

int
MenuConfigItemPos(int i, float *x, float *y)
{
	if(!cubeScreenValid || (u32)i >= 5u)
		return 0;
	*x = cubeScreenX[i];
	*y = cubeScreenY[i];
	return 1;
}

void
MenuConfigCubes(void)
{
	float s;
	int i, ang;

	cfgStep(&cfgCubeTimer);
	cubeSpin = (cubeSpin + 30) & 0xFFFF;	/* real: 0x2285C0 */
	if(cfgIsState(&cfgCubeTimer, 0)) {
		cubeScreenValid = 0;
		return;
	}

	s = cfgCubeTimer.duration ?
		(float)cfgCubeTimer.count / (float)cfgCubeTimer.duration : 0.0f;

	for(i = 0; i < 5; i++) {
		ang = (short)(cubeSpin + i*7000);
		matUnit(mdTop);
		mdTranslatef(menuCubePos[i][0], menuCubePos[i][1], menuCubePos[i][2]);
		mdRotX(ang);
		mdRotY(ang);
		mdRotZ(ang);
		MeshDraw(&cubeModel, mdTop, s + menuCubeBias[i],
			s + menuCubeBias[i], s + menuCubeBias[i],
			menuCubeColor[i], 200.0f);	/* real: scene 0x27EFB0 +0x90 */
		cubeScreenX[i] = meshObjX;
		cubeScreenY[i] = meshObjY;
	}
	cubeScreenValid = 1;
}

/* ============= the screen's own state machine (0x227390) ============= */

/* real: 0x227FC0 - "no other screen is open".  menu.c only has these
 * two, so the clock/wizard/browser factors are dropped. */
int
MenuConfigOpen(void)
{
	return !cfgIsState(&cfgAnim, 0);
}

/* real: 0x226A60 - the config screen's own alpha: the last dur10 frames
 * of the Anim's count, scaled by the module fade. */
int
MenuConfigAlpha(int fadeAlpha)
{
	int v;

	v = cfgCount(&cfgAnim) - (cfgDur80 + cfgDur40);
	if(v < 0)
		v = 0;
	else if(v > cfgDur10)
		v = cfgDur10;
	return cfgDur10 ? (v << 7)/cfgDur10 * fadeAlpha / 128 : 0;
}

/* real: 0x227268 - "enter System Configuration".  Called from the main
 * menu's input handler (0x22836C) when the cursor sits on item 1; the
 * tail message 0x2287A8(20992, 1, 4) is the UI click.
 *
 * Everything the screen shows is switched on here: the Anim's duration
 * is recomputed from the three per-screen durations, 0x22AEC8 closes the
 * screen at 0x27F620, 0x2291E8 opens the backdrop's fade timer and
 * 0x225AD0 opens the carousel.  The cube timer is NOT opened here - the
 * state machine 0x227390 does that when the Anim's count reaches dur80
 * - but the port opens it from the same place for lack of the count
 * hook (documented divergence; the visible effect is the cubes growing
 * from frame 0 of the entry instead of frame dur80). */
void
MenuEnterConfig(void)
{
	if(!cfgIsState(&cfgAnim, 0))
		return;
	cfgAnim.duration = cfgDur80 + cfgDur40 + cfgDur10;
	cfgOpen(&cfgAnim);
	MenuBackFadeOpen();		/* real: 0x2291E8 */
	CarouselOpen();			/* real: 0x225AD0 */
	cfgOpen(&cfgCubeTimer);		/* real: 0x226B28, from 0x2273D0 */
	OSDDispatch(20992, 1, 4, 0);	/* real: the 0x2287A8 tail */
	printf("osdsys: enter System Configuration\n");
}

/* real: the closing arm of 0x227390 - as the Anim counts back down it
 * closes the cube timer at dur10 from the end (0x226B70), the backdrop
 * fade at dur80 (0x229230) and the carousel at *(gp-30372) (0x225B68).
 * The port fires all three at once, which is the same end state. */
void
MenuLeaveConfig(void)
{
	if(!cfgIsState(&cfgAnim, 2))
		return;
	cfgClose(&cfgAnim);
	cfgClose(&cfgCubeTimer);
	cfgClose(&carouselTimer);
	MenuBackFadeClose();
	printf("osdsys: leave System Configuration\n");
}

/* real: 0x227DE8 - the screen's per-frame slot in the hub 0x2283F0:
 * step the Anim, run the state machine, read the pad, draw the items.
 * The item drawing lives in menutext.c. */
void
MenuConfigStep(void)
{
	cfgStep(&cfgAnim);
}

/* real: 0x21CE58's share - the ring is rebuilt by 0x225998 and the
 * durations come from 0x228460. */
void
InitMenuConfig(void)
{
	int rate, i, k;

	rate = IsPAL() ? 50 : 60;
	cfgDur40 = rate*40/60;
	cfgDur10 = rate/6;
	cfgDur80 = rate*80/60;		/* real: *(gp-30380) */

	memset(&cfgAnim, 0, sizeof(cfgAnim));
	memset(&carouselTimer, 0, sizeof(carouselTimer));
	memset(&cfgCubeTimer, 0, sizeof(cfgCubeTimer));
	cfgAnim.duration = cfgDur80 + cfgDur40 + cfgDur10;
	carouselTimer.duration = cfgDur80;	/* real: *(gp-30372), 0x225998 */
	cfgCubeTimer.duration = cfgDur40;	/* real: 0x228460 */

	rodModel.nfaces = menuRodFaces;
	rodModel.verts = menuRodVerts;
	rodModel.norms = menuRodNorms;
	cubeModel.nfaces = menuCubeFaces;
	cubeModel.verts = menuCubeVerts;
	cubeModel.norms = menuCubeNorms;

	memset(ring, 0, sizeof(ring));
	for(i = 0; i < NRING; i++)
		for(k = 0; k < 4; k++) {
			ring[i].col0[k] = cfgColBody[k];
			ring[i].col1[k] = cfgColEdge[k];
		}
	CarouselClock();
	for(k = 0; k < 4; k++) {
		ring[ringOffset].col0[k] = cfgColFront[k];
		ring[ringOffset].col1[k] = cfgColRingB[k];
		ringColA[k] = cfgColRingA[k];
		ringColB[k] = cfgColRingB[k];
	}

	cubeSpin = 0;			/* real: 0x2284C4 */
	cfgMeshTex = OsdArgInt(14, 1);
	cfgDebug = OsdArgInt(6, 0);
}
