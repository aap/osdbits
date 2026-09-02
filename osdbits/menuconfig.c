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
 * "The same renderer" is true of the geometry pipeline (0x22CFA8 ->
 * 0x22C888 -> 0x22C4E0) but NOT of the scene structs around it: the rod
 * scene is handed the frame's camera every frame (0x2268F0) while the
 * cube scene keeps a static, permanently IDENTITY one (0x352840, set by
 * 0x228460 and never touched again), and the two also disagree about the
 * refraction centre's 0.35 scale and the interlace half pixel.  See
 * cubeCamera and MeshDraw.
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

/* The per-screen durations, all derived from the refresh rate.  Two
 * separate initialisers write them: 0x228460 does dur40 (gp-30400) =
 * rate*40/60 and dur10 (gp-30396) = rate/6, and 0x22AD38 - which
 * 0x21CE58 calls BEFORE both 0x225998 and 0x228460 - does three more:
 *
 *   gp-30380 = rate*40/60   (0x22AD7C)  a SECOND forty-frame leg
 *   gp-30376 = rate*80/60   (0x22ADC4)  eighty frames, and 0x27F620's
 *                                       duration - no config timer uses it
 *   gp-30372 = 1            (0x22ADA4)  a literal one, the carousel's
 *
 * so the config Anim's first phase is forty frames, not eighty, and the
 * carousel's whole ramp is ONE frame long.  Confirmed against live
 * retail memory (savestate eeMemory.bin, NTSC): gp-30400/-30396/-30380/
 * -30376/-30372 = 40/10/40/80/1, Anim 0x27BE44 duration 90 = 40+40+10,
 * cube timer 0x27EC00 duration 40, backdrop fade 0x27F190 duration 40,
 * carousel 0x27EB00 duration 1.  An earlier pass here read gp-30380 as
 * "dur80" and gave the carousel that value, which is what made the rods
 * grow in over 80 frames instead of snapping to full size in one - see
 * MenuConfigCarousel/CarouselOpen. */
static int cfgDur40, cfgDur10;
static int cfgDur40b;		/* real: *(gp-30380), 0x22AD7C */
static int cfgCarouselDur;	/* real: *(gp-30372), 0x22ADA4 */

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
/* the two colour qwords 0x225DD8 copies into every deferred record at
 * +0x100 and +0x120.  0x225318's tail eases 0x34E910 toward 0x34E940
 * ({167,217,255,0}, the fixed cyan-white) and 0x34E920 toward 0x27EAE0
 * ({0x3C,0x3C,0x3C,0x80}); live retail memory (savestates .04/.05,
 * config screen up) shows both settled on exactly those targets.  An
 * earlier pass here had them as 0x27EAE0/0x27EAF0's values - harmless
 * then because nothing read the record's colA/colB, but they are the
 * front rod's SPLIT colours: colA paints the visible lower piece,
 * colB is what 0x22E4D0 hands 0x22CD78 - the reason all 86 of retail's
 * flush draws are rgba 3c3c3c80 while the front slot's own col1 is
 * {0x80,0x80,0x80,0x1E}. */
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
/* real 0x34E940: the fixed bright cyan-white 0x225878 rebuilds every
 * frame (22d8c8..22d8e8: {167, 217, 255, 0} written out in immediates).
 * It is both half of the front slot's body blend below and the target
 * 0x225318's tail eases 0x34E910 toward. */
static const int cfgColFixed[4] = { 167, 217, 255, 0 };
/* real 0x225878: the front slot's body colour is the average of
 * 0x27EAC0 and the fixed {167,217,255} at 0x34E940 */
static const int cfgColFront[4] = { (0x2D+167)/2, (0x55+217)/2, (0x66+255)/2, 0x80 };

/* =================== TEXCBUMP (TEXC slot 2) ===================
 *
 * The glass's second texture, and the one that makes the cubes look
 * bumpmapped.  The slot descriptor at 0x27F1C0 + 2*12 declares wexp =
 * hexp = 6 (a real 64x64 page) and the per-slot decoder table at
 * 0x2A4BA0 sends slot 2 to 0x22A720, the plain grey expander: one source
 * byte per texel, written as `b | b<<8 | b<<16 | 0x7F000000'.  The
 * resource is 4096 bytes - tools/extract-res.py already emits
 * res/TEXCBUMP_EXP.inc with the rest of TEXIMAGE, it was just never
 * wired into res.c. */

#define BUMPW 64
#define BUMPH 64

static u32 bumpTexels[BUMPW*BUMPH];

static Texture bumpTexture = {
	(u8*)bumpTexels, RESID_TEXCBUMP, nil, 0, { 0, 0, BUMPW, BUMPH },
	0, 0, SCE_GS_PSMCT32, 0, { 0 }
};

static u32 refaTexels[BUMPW*BUMPH];

static Texture refaTexture = {
	(u8*)refaTexels, RESID_TEXCREFA, nil, 0, { 0, 0, BUMPW, BUMPH },
	0, 0, SCE_GS_PSMCT32, 0, { 0 }
};

/* ============ TEXCFLOW (slot 0) and TEXCBINV (slot 3) ============
 *
 * The two textures the deferred rod bloom (0x2267E8, MenuConfigFlushMesh)
 * uses, and the last two TEXC pages this port was missing.  The slot
 * descriptor table at 0x27F1C0 declares both as 64x64 (wexp = hexp = 6)
 * and the per-slot decoder table 0x2A4BA0 sends BOTH to 0x22A720, the same
 * grey expander slots 2 and 5 use - so all four share one decode.
 *
 * TEXCBINV needs no resource of its own: the TEXIMAGE blobs `TEXCBUMP'
 * and `TEXCBINV' are the exact bitwise complement of each other (checked
 * byte for byte over the whole 4096-byte expansion, 0 mismatches), which
 * is what makes the two walks of the flush a classic emboss PAIR rather
 * than two unrelated passes. */

static u32 flowTexels[BUMPW*BUMPH];

static Texture flowTexture = {
	(u8*)flowTexels, RESID_TEXCFLOW, nil, 0, { 0, 0, BUMPW, BUMPH },
	0, 0, SCE_GS_PSMCT32, 0, { 0 }
};

static u32 binvTexels[BUMPW*BUMPH];

/* `data' is set, so InitTexture never looks the resource up - the id is
 * here to record which TEXC slot these texels really are. */
static Texture binvTexture = {
	(u8*)binvTexels, RESID_TEXCBINV, nil, 0, { 0, 0, BUMPW, BUMPH },
	0, 0, SCE_GS_PSMCT32, 0, { 0 }
};

/* real: 0x22A720 */
static void
DecodeBump(void)
{
	u8 *src;
	int i;

	src = GetResourceData(RESID_TEXCBUMP);
	for(i = 0; i < BUMPW*BUMPH; i++) {
		bumpTexels[i] = src[i] | src[i]<<8 | src[i]<<16 | 0x7F000000;
		/* TEXCBINV = ~TEXCBUMP, run through the same expander */
		binvTexels[i] = (u8)~src[i] | (u8)~src[i]<<8 | (u8)~src[i]<<16 |
			0x7F000000;
	}
	/* slot 5 shares the decoder (0x2A4BA0[5] == 0x2A4BA0[2] == 0x22A720) */
	src = GetResourceData(RESID_TEXCREFA);
	for(i = 0; i < BUMPW*BUMPH; i++)
		refaTexels[i] = src[i] | src[i]<<8 | src[i]<<16 | 0x7F000000;
	/* and so does slot 0 */
	src = GetResourceData(RESID_TEXCFLOW);
	for(i = 0; i < BUMPW*BUMPH; i++)
		flowTexels[i] = src[i] | src[i]<<8 | src[i]<<16 | 0x7F000000;
}

/* real: 0x22AB90(2, 1, 2) -> 0x22AA88, the same binder menuback.c's
 * BindKabe models for slot 1: TEX1 forced to 0x61 (LINEAR/LINEAR),
 * CLAMP_1 back to REPEAT/REPEAT over sceGsSetDefTexEnv's CLAMP/CLAMP,
 * TEST_1 = (ztst << 17) | 0x30000 so the 2 arrives as GREATER, and
 * ALPHA_1 = 0x48 with TEXA {0x7F, 1, 0x81}.  0x22C920 re-pushes its own
 * CLAMP_1 per primitive anyway; the UVs run 0..1.5. */
static void
MenuConfigBindBump(void)
{
	vif1Begin();
	pktSetAD(SCE_GS_TEXFLUSH, 0);
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(bumpTexture.gstex.tbp[0], BUMPW/64,
		SCE_GS_PSMCT32, 6, 6, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0));
	pktSetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(0x7F, 1, 0x81));
	vif1End();
}

/* ======================= TEXCREFA (TEXC slot 5) =======================
 *
 * TEXC slot 5, and the same shape as TEXCBUMP: the descriptor at
 * 0x27F1C0 + 5*12 is {0x01E3BCB0, 6, 6} (a 64x64 page) and the per-slot
 * decoder table 0x2A4BA0[5] is 0x22A720 - the SAME grey expander slot 2
 * uses.  So one decode serves both. */

/* real: 0x22AB90(5, 0, 1) -> 0x22AA88, the bump bind with slot 5's page.
 * (0x22AA88 also pushes ALPHA_1 = 0x48 for its a1 = 0 and TEST_1's ZTST =
 * ALWAYS for its a2 = 1; MeshDrawCubeMask sets both explicitly, as the
 * TEXCBUMP path does.) */
static void
MenuConfigBindRefa(void)
{
	vif1Begin();
	pktSetAD(SCE_GS_TEXFLUSH, 0);
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(refaTexture.gstex.tbp[0], BUMPW/64,
		SCE_GS_PSMCT32, 6, 6, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0));
	pktSetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(0x7F, 1, 0x81));
	vif1End();
}

/* real: 0x22AB90(0, 0, 2) - the flush's pass A texture.  Same binder, TEXC
 * slot 0's page; the ALPHA_1 0x44 and TEST_1 GREATER 0x22AA88 pushes for
 * (additive = 0, ztst = 2) are both overwritten by the 0x22A0C0(1, 1) that
 * follows, so as with MenuConfigBindBump only the texture state is here. */
static void
MenuConfigBindFlow(void)
{
	vif1Begin();
	pktSetAD(SCE_GS_TEXFLUSH, 0);
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(flowTexture.gstex.tbp[0], BUMPW/64,
		SCE_GS_PSMCT32, 6, 6, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0));
	pktSetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(0x7F, 1, 0x81));
	vif1End();
}

/* real: 0x22AB90(3, 1, 2) - the flush's walk-0 emboss texture, TEXCBUMP's
 * complement. */
static void
MenuConfigBindBinv(void)
{
	vif1Begin();
	pktSetAD(SCE_GS_TEXFLUSH, 0);
	pktSetAD(SCE_GS_TEX1_1, SCE_GS_SET_TEX1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR, 0, 0, 0));
	pktSetAD(SCE_GS_TEX0_1, SCE_GS_SET_TEX0(binvTexture.gstex.tbp[0], BUMPW/64,
		SCE_GS_PSMCT32, 6, 6, 1, SCE_GS_MODULATE, 0, SCE_GS_PSMCT32, 0, 0, 1));
	pktSetAD(SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0));
	pktSetAD(SCE_GS_TEXA, SCE_GS_SET_TEXA(0x7F, 1, 0x81));
	vif1End();
}

/* real: 0x27F180 - 0x226D00's clear record between its two cube walks,
 * {0,0,0,0}: work buffer 4 becomes the stage's ALPHA MASK.  0x2267E8's
 * record is a different one, {0,0,0,0x80} - see menu.c's flushClearColor. */
static const int cubeMaskClear[4] = { 0, 0, 0, 0 };

/* real: 0x27EFB0 + 0xC0, {120,120,120,128} in the live retail image
 * (0x27F070: 78 78 78 80).  0x22CD78 hands it straight to RGBAQ. */
static const int cubeReflColor[4] = { 120, 120, 120, 0x80 };

/* real: 0x27EFB0 + 0xA0, the colour 0x22D2E8 hands both TEXCBUMP passes
 * (the rod scene's 0x27E950 + 0xA0 is the same {8,8,8,128}).  With TFX
 * MODULATE that caps each pass at 8*255/128 = 15 levels, so the emboss
 * is a +-15 relief on top of the refraction, not a colour of its own. */
static const int cubeBumpColor[4] = { 8, 8, 8, 0x80 };
/* real: 0x27E950 + 0xA0 and +0xB0/+0xB4.  The rod scene's colour is the
 * same {8,8,8,128}, but its emboss offset is **-0.008**, not the cube
 * scene's +0.01 - read straight out of the live retail image (0x27EA00 =
 * bc03126f bc03126f, i.e. -0.0080 twice). */
static const int rodBumpColor[4] = { 8, 8, 8, 0x80 };
static const float rodBumpOfs = -0.008f;

/* ==================== the mesh renderer ====================
 *
 * One transform pass (0x22CFA8's tail) into a per-face scratch record,
 * then one emit pass per draw pass (0x22C888 -> 0x22C4E0).  The ROM
 * keeps 32+ of the 352-byte records at 0x3529D0 and a second bank at
 * 0x3555D0 (= 0x3529D0 + 32*352) for the split copy of the front rod;
 * this port transforms one object at a time, so one bank is enough. */

#define MAXFACES 16
#define MAXFACEMASK 0xFFFFu

typedef struct MeshVertex MeshVertex;
struct MeshVertex
{
	sceVu0FVECTOR cam;	/* real +0x00 camera space */
	sceVu0FVECTOR proj;	/* real +0x20 after the w divide */
	float q;		/* real +0x40 = 1/w */
	float u, v;		/* real +0x10/+0x14, the model UV, v scaled */
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
	const float (*uvs)[4];
};

static MeshFace meshFaces[MAXFACES];
/* real: 0x3555D0 (= 0x3529D0 + 32*352), the second face bank.  Only the
 * front rod's split arms use it: the LOWER piece is transformed into the
 * first bank and the UPPER into this one, and each pass then walks both.
 *
 * The two arms also skip fixed face INDICES per piece.  The rod model's
 * sixteen faces are 0-1 = the flat top cap (y 26.39), 2-7 = the bevel
 * ring (26..26.39), 8-9 = the bottom cap (y 0), 10-15 = the six shaft
 * sides (0..26) - so the skips are exactly "no cap at the cut": the
 * lower piece draws 8..15 (bottom cap + shaft, `slti v0,s2,8' skips the
 * rest, 22e6b0/22dba0...) and the upper everything but 8 and 9 (top cap
 * + bevel + shaft, `addiu v0,s2,-8; sltiu v0,v0,2', 22e700/22dbf0...). */
static MeshFace meshFaces2[MAXFACES];
static MeshFace *meshBank = meshFaces;	/* the bank the passes below walk */
static u32 meshMask = MAXFACEMASK;	/* the split arms' face-index skips */

#define MESH_LOWER 0xFF00u		/* faces 8..15 */
#define MESH_UPPER (MAXFACEMASK & ~0x0300u)	/* all but 8 and 9 */

static void
MeshSelect(MeshFace *bank, u32 mask)
{
	meshBank = bank;
	meshMask = mask;
}

static float meshObjX, meshObjY;	/* 0x22CFA8's outX/outY */
/* the pair 0x22C4E0 actually shrinks toward: the rod path hands it
 * 0x22CFA8's outX/outY unchanged, the cube path (0x22D2E8, right after
 * the transform) multiplies both by *(gp-32064) = 0.35 first. */
static float meshRefX, meshRefY;

/* NOT original (argv[14]): 0 draws the meshes untextured, which is the
 * geometry on its own; 1 is the ROM's screen-space refraction sampling. */
static int cfgMeshTex = 1;

/* real: the scene struct's camera matrix pointer, +0x64.
 *
 * The two meshes do NOT share a camera.  0x2268F0's head writes the
 * frame's camera and view-screen matrices into the ROD scene
 * (`sw a1,96(v0); sw a0,100(v0)' on 0x27E950, whose static +0x60/+0x64
 * are 0), so the rods are seen through the same camera as the orbs.  The
 * CUBE scene 0x27EFB0 carries static pointers 0x352800/0x352840 instead,
 * and the only writer of either is the per-screen init 0x228460:
 * 0x267068 (sceVu0ViewScreenMatrix) builds 0x352800 from exactly the
 * arguments 0x21CFD8 gives the frame's own (scrz 512, ax/ay 0x27B44C/
 * 0x27B450, cx/cy 2048, nearz 1, farz 16777215, zmin 1, zmax 65536 - so
 * the two matrices are numerically identical and menuViewScreen serves
 * for both), and 0x267630 (sceVu0UnitMatrix) sets 0x352840 to the
 * IDENTITY.  Nothing in the image ever writes it again - 0x27EFB0 is
 * mentioned exactly once, at 0x226D78 - so the five cubes are drawn with
 * NO camera at all: their table positions (x -22.5..-10.75, z 47.5) are
 * already camera-space, 47.5 units from the eye rather than the 150.5
 * the orb camera (z = -103) would put them at.  Running them through
 * menuCamera made them 3.2x too small, 3.2x too close together and cut
 * the refraction offset (which scales with 1/w) by the same factor. */
static sceVu0FMATRIX cubeCamera;	/* real: 0x352840, the identity */

/* real: 0x22CFA8 - build camera x world, project the object's origin,
 * then transform every face's four vertices and its normal.  The scale
 * is the scene struct's +0x68/+0x6C/+0x70 triple; only +0x6C is ever
 * anything but 1.0 for the rod (it is the fly-in progress). */
static void
MeshTransform(MeshModel *mdl, sceVu0FMATRIX cam, sceVu0FMATRIX world,
	float sx, float sy, float sz)
{
	sceVu0FMATRIX m;
	sceVu0FVECTOR o, b, v;
	MeshFace *f;
	float q, e1x, e1y, e2x, e2y;
	int i, k;

	matMul(m, cam, world);

	o[0] = o[1] = o[2] = 0.0f; o[3] = 1.0f;
	matApply(o, m, o);
	matApply(b, menuViewScreen, o);
	q = 1.0f/b[3];
	meshObjX = b[0]*q - 2048.0f;
	meshObjY = b[1]*q - 2048.0f;

	for(i = 0; i < mdl->nfaces; i++) {
		f = &meshBank[i];
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
			/* real: the tail of the same loop - the U is the
			 * model's, the V is scaled by the same +0x6C the
			 * vertices' y was */
			f->v[k].u = mdl->uvs[i*4+k][0];
			f->v[k].v = mdl->uvs[i*4+k][1] * sy;
		}

		/* real: two 0x2676F8 (SubVector) calls on the PROJECTED
		 * positions of vertices 2 and 1 against vertex 0, then the z
		 * of their cross product - a screen-space winding test.
		 *
		 *   22d1ec  SubVector(sp+176, v[2].proj, v[0].proj)   ; e2
		 *   22d1fc  SubVector(sp+192, v[1].proj, v[0].proj)   ; e1
		 *   22d214  f1 = e2.x * e1.y
		 *   22d21c  f0 = e2.y * e1.x
		 *   22d220  f1 = f1 - f0
		 *   22d224  c.lt.s f4(0.0), f1                        ; cc = 0 < f1
		 *   22d22c  bc1f 0x22d238                             ; NOT taken ->
		 *   22d230  (delay) li v0,1                           ;   22d234
		 *   22d234  move v0,zero                              ; cc TRUE -> 0
		 *
		 * bc1f has no likely bit, so the delay slot always runs: cross
		 * > 0 falls through to `move v0,zero' and cross <= 0 branches
		 * over it with v0 = 1.  So the flag is
		 *
		 *     cull = !(e2.x*e1.y - e2.y*e1.x > 0)
		 *
		 * and the port had it INVERTED, which swapped the two glass
		 * layers everywhere: every pass that asks for the `cull == 0'
		 * (far) set drew the near faces and vice versa.  Confirmed
		 * against the live retail face bank at 0x3529D0 in savestate
		 * `20020207-164243 (00000000).04.p2s' - cube 4's six faces
		 * carry cull = 0,1,0,0,1,1 and their projected vertices give
		 * cross = +260, -95, +1417, +295, -104, -125, i.e. cull is 1
		 * exactly where the cross product is negative. */
		e2x = f->v[2].proj[0] - f->v[0].proj[0];
		e2y = f->v[2].proj[1] - f->v[0].proj[1];
		e1x = f->v[1].proj[0] - f->v[0].proj[0];
		e1y = f->v[1].proj[1] - f->v[0].proj[1];
		f->cull = !(e2x*e1y - e2y*e1x > 0.0f);
	}
}

/* real: 0x230068 (mdCos) over the quarter-wave table at 0x3581F0 */
static float
cfgCosf(int a)
{
	return cosf((short)a * (TAU/65536.0f));
}

/* real: the vertex kick shared by all five emits below, and the one place
 * this port had the wrong GS REGISTER.
 *
 * Every one of the ROM's REGLIST templates - 0x27F870 and 0x27F880 (the
 * black and flat passes, {PRIM, RGBAQ, XYZF2 x4}), 0x27F890 (the
 * refraction, {PRIM, CLAMP_1, CLAMP_1, RGBAQ, (UV, XYZF2) x4}), 0x27F8A0
 * (the reflection, {PRIM, RGBAQ, (UV, XYZF2) x4}) and 0x27F8B0 (the
 * emboss, {PRIM, CLAMP_1, (ST, RGBAQ, XYZF2) x4}) - names GIF register
 * **4 = XYZF2**, not 5 = XYZ2.  XYZF2's Z field is 24 bits wide where
 * XYZ2's is 32, so the GS silently truncates.
 *
 * That is where retail's flat cube depth comes from: sceVu0FTOI4 (a bare
 * `vftoi4') leaves -4080 in the face record's +0x38 and 0x22C4E0 pushes
 * the whole sign-extended word, but the register only keeps its low 24
 * bits - 0x00FFF010 = 16773136, exactly what the GS dumps show, against
 * this port's 0xFFFFF010.  Nothing in the ROM masks; the register does.
 *
 * F is 0 and dead: none of the five PRIMs (276, 404, 84, 132, 196) sets
 * FGE, so the fog value is never used. */
static void
MeshEmitXYZ(MeshFace *f, int k, int zbias)
{
	pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF((int)(f->v[k].proj[0]*16.0f),
		(int)(f->v[k].proj[1]*16.0f),
		((int)(f->v[k].proj[2]*16.0f) + zbias) & 0xFFFFFF, 0));
}

/* real: the emit half of 0x22C4E0.  Every vertex is drawn 5 % of the way
 * toward the object's own screen centre (*(gp-32068) = 0.95) and pushed
 * by the camera-space face normal scaled by 1/w - a cheap refraction
 * offset, 1000 units horizontally and 500 vertically.
 *
 * The UV is NOT the model's: it is the vertex's own SCREEN position, so
 * the surface samples the copy of the frame taken before the object list
 * ran (menuback.c's extraBuf1, the ROM's work buffer 3).  That is what
 * makes these look like glass.  (The "bumpmap-like" effect on the cubes
 * is the separate TEXCBUMP emboss, MeshEmitBumpFace below; the amount of
 * refraction is `normal * 1000 / 500 * q', so it is inversely
 * proportional to the object's w - which is exactly why running the
 * cubes through the wrong camera made them look flat as well as small.)
 * The two biases the ROM adds - 1024 to U (clamped, so a
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

	/* real: 276 / 404 - TRIANGLE_STRIP | TME | FST, and the AA1 bit
	 * (0x80, the only difference between the two) goes on the ORDINARY
	 * face, not the edge-on one:
	 *     c.lt.s  f0(0.99), f21(fres)  ; 0.99 < fres
	 *     bc1f    0x22c5cc             ; NOT taken -> v0 = 276
	 *     b       0x22c5d0 / li v0,276
	 *   0x22c5cc: li v0,404
	 * so fres > *(gp-32072) = 0.99 selects 276 (AA1 OFF) and everything
	 * else 404 (AA1 ON).  The port had the test the right way round for
	 * the constant but the wrong way round for the bit, which left AA1
	 * toggling on and off per face as a spinning rod's faces crossed
	 * 0.99 - a shimmer on the silhouettes rather than a stable edge. */
	prim = SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, cfgMeshTex, 0, 0,
		fres > 0.99f ? 0 : 1, 1, 0, 0);

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
		u = (f->v[k].proj[0] - 2048.0f - meshRefX)*0.95f + meshRefX - nx;
		v = (f->v[k].proj[1] - 2048.0f - meshRefY)*0.95f + meshRefY - ny;
		u += screenW/2 + 1024.0f;
		if(u < 1024.0f)
			u = 1024.0f;
		/* real: *(0x27B448), the module's own copy of the field, read
		 * once per frame.  NOT original: the port reads the live
		 * evenOddField, which SwapBuffers flips on the swap thread and
		 * which can therefore change between two objects of the same
		 * frame - menuback.c's per-frame snapshot instead (the same
		 * race commit 37efd18 fixed for the blur's buffer parity). */
		v += screenH/2 + 256.0f - MenuBackField()*0.5f;
		pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(((int)(u*16.0f)) & 0x3FFF,
			((int)(v*16.0f)) & 0x3FFF));
		MeshEmitXYZ(f, k, 0);
	}
	vif1End();
}

/* real: 0x22C920 - the TEXCBUMP pass's emit.  A completely different
 * primitive from 0x22C4E0's: PRIM = 84 = TRIANGLE_STRIP | TME | ABE with
 * FST CLEAR, so it uses ST/Q and the MODEL's own UVs (0..1.5 over the
 * 64x64 page, which REPEAT wraps one and a half times) instead of the
 * screen position.  The GIFtag template is 0x27F8B0 (NREG 14,
 * {PRIM, CLAMP_1, ST, RGBAQ, XYZ2, ...}).
 *
 *     ST    = ((uv.x + ofsX) * q, (uv.y + ofsY) * q)
 *     RGBAQ = the scene's +0xA0 colour, Q = q
 *     XYZ2  = the same fixed-point position the refraction pass uses
 *
 * ABE is SET here, so the ALPHA_1 0x22A0C0 pushes around the two calls
 * is live and is the whole point: the cube path runs this twice per face
 * set, first ADDITIVE (0x22A0C0(0,2) -> ALPHA_1 0x48 = Cs*As + Cd) at
 * offset (scene+0xB0, +0xB4) = (0.01, 0.01), then SUBTRACTIVE
 * (0x22A0C0(2,2) -> 0x42 = (0 - Cs)*As + Cd) at offset (0, 0).  That
 * difference of the same texture at two slightly different UVs is a
 * classic emboss, and it is the "bumpmap" on aap's cubes. */
static void
MeshEmitBumpFace(MeshFace *f, const int *col, float ofsx, float ofsy)
{
	float s, t;
	int k;

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, 1, 0,
		1, 0, 0, 0, 0));	/* real: 84 */
	pktSetAD(SCE_GS_CLAMP_1, 0x01000000);
	for(k = 0; k < 4; k++) {
		s = (f->v[k].u + ofsx) * f->v[k].q;
		t = (f->v[k].v + ofsy) * f->v[k].q;
		pktSetAD(SCE_GS_ST, SCE_GS_SET_ST(*(u32*)&s, *(u32*)&t));
		pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2],
			col[3], *(u32*)&f->v[k].q));
		MeshEmitXYZ(f, k, 0);
	}
	vif1End();
}

/* real: 0x22CB58, the emit behind 0x22CCE8.  A THIRD primitive, and the
 * simplest of the three: PRIM = 196 = TRIANGLE_STRIP | ABE | AA1 with
 * **TME clear**, so the texture the caller's 0x22BFD0 just bound is not
 * sampled at all.  The colour is the scene's own +0x80 scaled by the
 * SQUARE of the Fresnel term (0x22CB58's `f20 = f12*f12' before the three
 * `mul.s' + `trunc.w.s' + clamp-at-255 blocks), alpha a flat 0x80, and
 * the four XYZF2 come straight out of the face record's +0x30 triple.
 *
 * GIFtag 0x27F880 (REGLIST, NREG 6, {PRIM, RGBAQ, XYZF2 x4}), a byte-for-
 * byte twin of 0x22CA68's 0x27F870. */
static void
MeshEmitFlatFace(MeshFace *f, float fres, const int *col)
{
	float s = fres*fres;
	int r, g, b, k;

	r = (int)(s * (float)col[0]); if(r >= 256) r = 255;
	g = (int)(s * (float)col[1]); if(g >= 256) g = 255;
	b = (int)(s * (float)col[2]); if(b >= 256) b = 255;

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, 0, 0,
		1, 1, 0, 0, 0));	/* real: 196 */
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(r, g, b, 0x80, 0));
	for(k = 0; k < 4; k++)
		MeshEmitXYZ(f, k, 0);
	vif1End();
}

/* real: 0x22CA68 - the fourth and last emit, and it takes only the face:
 * PRIM = 132 = TRIANGLE_STRIP | AA1, no TME, no ABE, no IIP, RGBAQ a flat
 * BLACK with A = 0x80, and Z bumped by one (`addiu a1,a1,1' on the +0x38
 * word) under 0x22A0C0(x, 3)'s ZTST GREATER.  Same GIFtag family
 * (0x27F870).
 *
 * It has two jobs, one per walk.  In 0x22D2E8 pass 5 it primes the cube's
 * area of work buffer 3 to black, so pass 7's refraction is all the glass
 * shows and none of the pre-object copy leaks through.  In 0x22D798 it is
 * the whole point of the second walk: drawn into a work buffer 4 that was
 * just cleared to alpha 0, its A = 0x80 IS the composite's mask.
 *
 * (RGBAQ's Q is `lui a2,0x3f8' = 0x03F80000, a ROM slip for 1.0f's
 * 0x3F800000.  Dead - PRIM has TME clear - and reproduced as 0.) */
static void
MeshEmitBlackFace(MeshFace *f)
{
	int k;

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, 0, 0,
		0, 1, 0, 0, 0));	/* real: 132 */
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0, 0, 0, 0x80, 0));
	for(k = 0; k < 4; k++)
		MeshEmitXYZ(f, k, 1);
	vif1End();
}

/* real: 0x22CD78 - the FIFTH emit, and the one this port was missing.  A
 * spherical environment map over TEXC slot 5 (TEXCREFA), per VERTEX:
 *
 *     n = sceVu0Normalize(v.cam)              ; 0x2677E0
 *     d = 2 * sceVu0InnerProduct(n, normal)   ; 0x267818, then f0 + f0
 *     if(d < 0) d = -d                        ; 22ce80..22ceb4, sign-folded
 *     r = sceVu0AddVector(n, ScaleVector(normal, d))  ; 0x267710/0x267050
 *     UV = ((r.x + 1) * 512, (r.y + 1) * 256) ; 0x25A368, 1/16 units
 *     PRIM = aa ? 404 : 276 ; 0x22D798 passes aa = 1, so 404, and
 *                             0x22E9A8 (the rod bloom) passes 0, so 276 -
 *                             no AA1 and, since 276 has ABE clear too,
 *                             a flat opaque write there
 *     RGBAQ = scene->+0xC0 = {120,120,120,128} for the cube scene, the
 *             ring slot's own col1 for a rod
 *     XYZ2 = v.fix, unchanged (no z + 1 here)
 *
 * `r = n + N*|2(n.N)|' is `reflect(n, N)' written for a normal that faces
 * the eye: n points from the eye at the vertex, so n.N is negative and
 * |2(n.N)| = -2(n.N), i.e. r = n - 2(n.N)N.  The UV scales put r.x over a
 * whole 64-texel wrap and r.y over half of one.
 *
 * WHY IT MATTERS, measured.  docs/menu-config.md's last write-up calls
 * this pass "invisible: 0x22CA68 redraws the SAME cull != 0 faces
 * immediately afterwards with PRIM = 132, whose ABE is clear, so the black
 * overwrites it".  It does not, and the retail GS memory says so: work
 * buffer 4 in savestate `20020207-164243 (00000000).04.p2s' holds smooth
 * per-face GREY over every cube - (44,44,44,128) at (85,140), (5,5,5,128)
 * at (60,90), (31,31,31,128) at (120,150) - where the "black overwrites
 * it" reading needs (0,0,0,128).
 *
 * The reason is the GS's AA1 rule: with AA1 set, alpha blending is
 * performed REGARDLESS of ABE, with As = the pixel's coverage (0x80 for a
 * fully covered pixel).  0x22CA68 runs under 0x22A0C0(0,3) = ALPHA_1
 * 0x48 = Cs*As + Cd, so its black adds NOTHING to the colour and only
 * stamps A = 0x80 - it is an alpha-only pass.  That is what makes the
 * mask, and it is why the reflection survives underneath it.
 *
 * And the reflection is exactly the port's remaining cube error.
 * 0x226D00's tail adds work buffer 4 over work buffer 3 (0x22C088,
 * additive), so the grey lands on the cube before the composite: retail's
 * screen reads (61,56,69) at (85,140) where its work buffer 3 alone holds
 * (17,12,25) - and this port, without the pass, measured (17,13,26) at
 * the same place.  17 + 44 = 61, 13 + 44 = 57, 26 + 44 = 70. */
static void
MeshEmitReflFace(MeshFace *f, const int *col, int aa)
{
	float n[3], r[3], d, l;
	int k, u, v;

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_TRISTRIP, 0, 1, 0,
		0, aa, 1, 0, 0));	/* real: aa ? 404 : 276 */
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2], col[3], 0));
	for(k = 0; k < 4; k++) {
		l = sqrtf(f->v[k].cam[0]*f->v[k].cam[0] +
		          f->v[k].cam[1]*f->v[k].cam[1] +
		          f->v[k].cam[2]*f->v[k].cam[2]);
		if(l == 0.0f)
			l = 1.0f;
		l = 1.0f/l;
		n[0] = f->v[k].cam[0]*l;
		n[1] = f->v[k].cam[1]*l;
		n[2] = f->v[k].cam[2]*l;
		d = 2.0f*(n[0]*f->normal[0] + n[1]*f->normal[1] + n[2]*f->normal[2]);
		if(d < 0.0f)
			d = -d;
		r[0] = n[0] + f->normal[0]*d;
		r[1] = n[1] + f->normal[1]*d;
		u = (int)((r[0] + 1.0f) * 512.0f);
		v = (int)((r[1] + 1.0f) * 256.0f);
		pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(u & 0x3FFF, v & 0x3FFF));
		MeshEmitXYZ(f, k, 0);
	}
	vif1End();
}

/* real: 0x22C888 and 0x22CCE8's shared head - normalize(vertex 0 in
 * camera space) dotted with the camera-space face normal, so 0 head-on
 * and 1 edge-on: the glass is bright only at its silhouette. */
static float
MeshFresnel(MeshFace *f)
{
	float d, l;

	l = sqrtf(f->v[0].cam[0]*f->v[0].cam[0] +
	          f->v[0].cam[1]*f->v[0].cam[1] +
	          f->v[0].cam[2]*f->v[0].cam[2]);
	if(l == 0.0f)
		return -1.0f;
	d = (f->v[0].cam[0]*f->normal[0] +
	     f->v[0].cam[1]*f->normal[1] +
	     f->v[0].cam[2]*f->normal[2]) / l;
	return 1.0f - (d < 0.0f ? -d : d);
}

/* `back' selects the winding 0x22CFA8 wrote into the face record's
 * +0x150: 0 is the cull == 0 set (the ROM's first, FAR layer) and 1 the
 * cull != 0 set (its last, NEAR layer). */
static void
MeshDrawPass(MeshModel *mdl, const int *col, float size, int back, int extra)
{
	MeshFace *f;
	float fres;
	int i;

	for(i = 0; i < mdl->nfaces; i++) {
		f = &meshBank[i];
		if(!(meshMask & 1u<<i))
			continue;
		if((f->cull != 0) != back)
			continue;
		fres = MeshFresnel(f);
		if(fres < 0.0f)
			continue;
		MeshEmitFace(f, fres, col, size, extra);
	}
}

/* real: the 0x22CCE8 loop (0x22D2E8 pass 4) */
static void
MeshFlatPass(MeshModel *mdl, const int *col, int back)
{
	MeshFace *f;
	float fres;
	int i;

	for(i = 0; i < mdl->nfaces; i++) {
		f = &meshBank[i];
		if(!(meshMask & 1u<<i))
			continue;
		if((f->cull != 0) != back)
			continue;
		fres = MeshFresnel(f);
		if(fres < 0.0f)
			continue;
		MeshEmitFlatFace(f, fres, col);
	}
}

/* real: the 0x22CA68 loops (0x22D2E8 pass 5, 0x22D798's second) */
static void
MeshBlackPass(MeshModel *mdl, int back)
{
	int i;

	for(i = 0; i < mdl->nfaces; i++)
		if(meshMask & 1u<<i && (meshBank[i].cull != 0) == back)
			MeshEmitBlackFace(&meshBank[i]);
}

/* real: the 0x22CD78 loop, 0x22D798's FIRST - the same cull != 0 set the
 * black pass then stamps alpha over (22d884: `beqz v0' skips cull == 0).
 * 0x22E9A8's pass A is the same loop with aa = 0 (22ea24: `move a2,zero'). */
static void
MeshReflPass(MeshModel *mdl, const int *col, int back, int aa)
{
	int i;

	for(i = 0; i < mdl->nfaces; i++)
		if(meshMask & 1u<<i && (meshBank[i].cull != 0) == back)
			MeshEmitReflFace(&meshBank[i], col, aa);
}

/* real: the 0x22C920 halves of the same per-face loops - same cull test,
 * no Fresnel (the bump pass does not use one).
 *
 * `step' is the ROD path's per-face phase: 0x22E0EC advances the ST
 * offset by *(gp-32032) = 0.1 for every face INDEX (22e1ec-22e200:
 * `mtc1 s1,f12; cvt.s.w; mul.s f12,f12,f20; add.s f12,f21,f12', and s1
 * counts skipped faces too), on top of 0x22D920's f21 = slot * 0.1, so
 * each rod and each of its faces samples the 64x64 page at a different
 * place.  0x22D2E8's cube loops have no such term - the offset is the
 * scene's +0xB0/+0xB4 pair for every face - so the cubes pass step 0. */
static void
MeshBumpPass(MeshModel *mdl, const int *col, int back, float ofsx, float ofsy,
	float step)
{
	int i;

	for(i = 0; i < mdl->nfaces; i++)
		if(meshMask & 1u<<i && (meshBank[i].cull != 0) == back)
			MeshEmitBumpFace(&meshBank[i], col,
				ofsx + i*step, ofsy + i*step);
}

/* THE PASS STRUCTURE, as the ROM has it.
 *
 * 0x22D2E8 (one cube) runs EIGHT loops over the face bank, and NOT ONE of
 * them draws on the screen.  `far' below is the cull == 0 set 0x22CFA8's
 * winding test picks out, `near' the cull != 0 set; the ROM always draws
 * far first and near last, which is what makes the pair a two-layer piece
 * of glass rather than two coats of the same one:
 *
 *   1  0x22BF58(1,0,1)  FRAME wb4, TEX **the live screen** (0x22A198,
 *                       PSMCT24) - far faces, 0x22C888 -> 0x22C4E0, extra 0
 *   2  0x22AB90(2,1,2)  TEXCBUMP; 0x22A0C0(0,2) = ALPHA 0x48 additive
 *                       far, 0x22C920, ST offset (+0xB0,+0xB4) = 0.01
 *   3  0x22A0C0(2,2)    = ALPHA 0x42 subtractive
 *                       far, 0x22C920, offset (0, 0)
 *   4  0x22BFD0(0,1,1)  FRAME wb3, TEX wb4 - far, 0x22CCE8 -> 0x22CB58
 *                       (untextured, colour x fres^2)
 *   5  0x22A0C0(0,3)    ZTST GREATER - near, 0x22CA68 (untextured BLACK)
 *   6  0x22BFD0(1,0,0) + 0x22C100()  wb3 -> wb4, additive, LEFT HALF only
 *   7  0x22BFD0(0,1,1)  FRAME wb3, TEX wb4 - near, 0x22C888, extra 0
 *   8  0x22AB90 / 0x22A0C0(0,2) then (2,2)  - near, 0x22C920 x2
 *
 * so work buffer 4 collects the FAR glass refracting the finished screen,
 * and work buffer 3 collects a black cube silhouette with the NEAR glass
 * refracting work buffer 4 painted over it.  0x226D00 then runs all five
 * cubes through that, and only afterwards:
 *
 *   0x22A4C8(1, 0x27F180, field)   clear wb4 to {0,0,0,**0**}
 *   five x 0x22D798                near faces only, into the cleared wb4:
 *                                  0x22CD78 (TEXCREFA reflection) then
 *                                  0x22CA68 again - black, A = 0x80
 *   0x22BFD0(0,1,0) + 0x22C088()   wb4 -> wb3, ADDITIVE
 *   0x22C2A0(phase < 5 ? 5-phase : 0)   the work-buffer zoom blur
 *   0x22C020(0,0,0) + 0x22C190(1)  wb3 -> the screen, **ABE = 1**
 *
 * The second walk is a MASK GENERATOR.  wb4 is cleared to alpha 0, the
 * only thing painted into it is 0x22CA68's black at A = 0x80, and
 * 0x22C088 - whose colour add therefore contributes nothing - copies that
 * alpha into wb3 (the GS writes As to the destination alpha whatever the
 * blend equation does to RGB).  0x22C190(1) then blends wb3 over the
 * screen by that alpha, so the stage replaces the frame exactly inside
 * the cubes and leaves the tunnel, the orbs and the rods untouched
 * everywhere else.
 *
 * WHY THIS IS THE FIX.  eba5595 collapsed the eight loops to two, drawn
 * straight on the screen, and had to bind extraBuf1 (wb3) as the
 * refraction source because binding the screen "measurably killed" the
 * cubes.  Both halves of that were the same bug: wb3 is 0x21D0A0's copy
 * of the frame taken BEFORE the composite multiplies it by
 * {0x37,0x28,0x3C}/128, so the collapsed chain painted a ~3x brighter,
 * un-tinted image of the backdrop onto the tinted backdrop - glass that
 * glows is glass that reads as a solid milky block.  Measured on HEAD at
 * frame 145, cube 0's face against the background 30 px to its left:
 *
 *     face  (5,117,185) (152,166,202) (0,80,131)
 *     bkgd  (33,26,49)  (31,24,47)    (27,21,40)
 *
 * i.e. 3-6x, exactly the reciprocal of the tint.  The ROM has no such
 * problem because pass 1's texture IS the tinted screen and the result
 * only ever reaches the frame through 0x22C190(1)'s masked composite.
 *
 * NOT PORTED, and why: 0x22CD78, the second walk's first loop.  It is a
 * spherical environment map - reflect(normalize(v.cam), normal) turned
 * into UV = ((r.x+1)*512, (r.y+1)*256) over TEXC slot 5 = TEXCREFA, at
 * the cube scene's +0xC0 = {120,120,120,128} - and the port ships no
 * TEXCREFA (res.c has no entry and res/ no .inc).  It is also, by the
 * ROM's own ordering, invisible: 0x22CA68 redraws the SAME cull != 0
 * faces immediately after it with PRIM 132, whose ABE is clear, so the
 * black overwrites it everywhere the depth test lets the black through -
 * and the cubes' z is ~22000 against the tunnel's 43..526, so GREATER
 * passes everywhere.  Only its AA1 edge pixels can survive.
 *
 * THE RODS (0x22D920's 0x22E0EC arm) are a different shape and need no
 * fix.  Their five loops are
 *
 *   1  0x22BFD0(1,0,1)  FRAME wb4, TEX wb3   far,  0x22C888
 *   2  0x22A0C0(2,2)    subtractive          far,  0x22C920, phase
 *                                            f21 + i * *(gp-32032)
 *   3  0x22A0C0(0,2)    additive             far,  0x22C920, phase
 *                                            +0xB0 + f21 + i*(gp-32028)
 *   4  0x22C020(1,0,1)  FRAME **the screen**, TEX wb4  near, 0x22C888
 *   5  0x22BFD0(0,1,1)  FRAME wb3, TEX wb4   near, 0x22C888, extra 255
 *
 * - pass 4 puts their visible content on the screen DIRECTLY, with no
 * work-buffer composite anywhere in the path, so the brightness mismatch
 * above simply does not arise for them: in the ROM as in the port the
 * rods sample an un-tinted copy and draw it onto the tinted frame, which
 * is why retail's rods glow and its cubes do not.  What the port still
 * skips for them is the first glass layer (passes 1-3 into wb4, which
 * pass 4 then samples) and pass 5's white-ish silhouette into wb3; both
 * are left alone here because 0x22D920 ends with FRAME = wb3 and the ROM
 * never restores it - the orb that sorts after a rod in 0x226700's walk
 * draws into a work buffer - and untangling that is a separate job. */
static int cfgDebug;		/* NOT original: menu.c's debugFrame */

static void
MeshDebug(MeshModel *mdl, float sy)
{
	int i, front = 0;

	if(!cfgDebug || frameCount != cfgDebug)
		return;
	for(i = 0; i < mdl->nfaces; i++)
		front += meshBank[i].cull == 0;
	printf("mesh %d faces at %.1f %.1f, scale %.2f, %d front, v0 %.0f %.0f\n",
		mdl->nfaces, meshObjX, meshObjY, sy, front,
		meshBank[0].v[0].proj[0] - 2048.0f,
		meshBank[0].v[0].proj[1] - 2048.0f);
}

/* real: 0x22D920's 0x22E0EC arm - the plain one-piece rod, and all five
 * of its loops.  The shape is the cubes' (MeshDrawCube) with one loop
 * fewer and one crucial difference: pass 4 draws on the SCREEN, so a rod
 * needs no work-buffer composite to become visible.
 *
 *   1  0x22BFD0(1,0,1)  FRAME wb4, TEX wb3; 0x22A0C0(1,1) ALPHA 0x44,
 *                       ZTST ALWAYS - far faces, 0x22C888, extra 0
 *   2  0x22AB90(2,1,2)  TEXCBUMP; 0x22A0C0(**2**,2) ALPHA 0x42
 *                       SUBTRACTIVE, ZTST GEQUAL - far, 0x22C920 at
 *                       ST offset f21 + i*(gp-32032)
 *   3  0x22A0C0(**0**,2) ALPHA 0x48 ADDITIVE - far, 0x22C920 at
 *                       scene->+0xB0 + f21 + i*(gp-32028)
 *   4  0x22C020(1,0,1)  FRAME **the screen**, TEX wb4; 0x22A0C0(1,2)
 *                       ALPHA 0x44, ZTST GEQUAL - NEAR faces, 0x22C888,
 *                       extra 0.  This is the only thing the player sees.
 *   5  0x22BFD0(0,1,1)  FRAME wb3, TEX wb4; same blend - near faces,
 *                       0x22C888 with extra **255** (22e3c4: li a2,255)
 *
 * Note the order of 2 and 3 is the MIRROR of the cubes' (0x22D2E8 does
 * additive first, subtractive second), and the rod scene's own offset
 * 0x27E950+0xB0 is **-0.008** where the cube scene's is +0.01.
 *
 * Two more numbers the collapsed version had wrong:
 *
 *  * the refraction centre.  22d9c0/22d9c4 multiply 0x22CFA8's outX and
 *    outY by *(gp-32052) = **0.9** before any pass, exactly as 0x22D2E8
 *    multiplies them by *(gp-32064) = 0.35.  The port passed them
 *    through unscaled.
 *  * pass 4 and 5 run under ZTST **GEQUAL** (0x22A0C0(1,2)), not ALWAYS.
 *
 * WHAT IT LOOKS LIKE.  wb4 collects the far glass refracting wb3 (the
 * un-tinted pre-object copy of the frame), pass 4 then refracts THAT
 * onto the screen - so a retail rod shows two layers of displaced,
 * Fresnel-brightened backdrop, which is where its crystalline sparkle
 * comes from.  The old two-pass collapse drew both face sets straight
 * off wb3, i.e. one layer of the same picture twice, which is why aap's
 * rods came out as soft featureless sticks.
 *
 * FRAME IS LEFT ON wb3, and the ROM never puts it back - see MeshDrawRod's
 * caller notes in menu.c.  0x226700 alternates 0x226360 (orb) and
 * 0x2266E0 (mesh) with no drawenv of its own, but the orb draw 0x22EFF0
 * pushes ITS OWN FRAME at 0x22F0CC (0x22A3B8(0x1F0A10, *(0x1F0C40), 0,
 * *(0x27B448))) before it emits anything, and so does every later stage:
 * 0x2267E8's bloom (0x22A4C8 at 0x22681C, 0x22C020 at 0x226864), the
 * cubes' 0x22BF58, and the 2D layers' 0x22A3B8.  Nothing in the ROM ever
 * draws into a work buffer by accident. */
static void
MeshDrawRod(MeshModel *mdl, sceVu0FMATRIX cam, sceVu0FMATRIX world,
	float sx, float sy, float sz, const int *col, float size,
	int slot, int field)
{
	float phase;

	MeshTransform(mdl, cam, world, sx, sy, sz);
	/* real: 22d9c0/22d9c4, *(gp-32052) = 0.9 */
	meshRefX = meshObjX * 0.9f;
	meshRefY = meshObjY * 0.9f;
	MeshDebug(mdl, sy);

	/* real: 0x22D920's head, f21 = (float)scene->+0x00 * *(gp-32056) */
	phase = (float)slot * 0.1f;

	/* ZMSK **0**, as everywhere else in the menu: all 416 ZBUF writes in
	 * a retail GS dump of this screen are `zbp=4480 psm=0' with ZMSK
	 * clear, because 0x22BF58 / 0x22BFD0 / 0x22C020 / 0x22A4C8 / 0x22A3B8
	 * push FRAME and ZBUF together and the ZBUF they push never masks.
	 * The port's `vif1SetZWrite(0)' here was an addition; passes 2..5 of
	 * this function run ZTST GEQUAL, so with Z writes off they were
	 * comparing every rod against whatever the previous frame's cube
	 * stage happened to leave behind. */
	vif1SetZWrite(1);

	if(!cfgMeshTex) {			/* NOT original: argv[14] = 0 */
		MenuBackScreenTarget(field);
		vif1SetZTest(0);
		vif1SetAlphaBlend(1, 4, 128);
		MeshDrawPass(mdl, col, size, 0, 0);
		MeshDrawPass(mdl, col, size, 1, 0);
		return;
	}

	/* 1 - the far glass refracting wb3, into wb4 */
	MenuBackBindWork(0);			/* real: 0x22BFD0(1,0,1) */
	MenuBackWorkTarget(1, nil, field);
	vif1SetZTest(0);			/* real: 0x22A0C0(1,1) */
	vif1SetAlphaBlend(1, 4, 128);		/* real: ALPHA_1 0x44 */
	MeshDrawPass(mdl, col, size, 0, 0);

	/* 2, 3 - the TEXCBUMP emboss on the same faces, subtractive first */
	MenuConfigBindBump();			/* real: 0x22AB90(2,1,2) */
	vif1SetZTest(1);			/* real: 0x22A0C0(x,2) GEQUAL */
	vif1SetAlphaBlend(1, 6, 128);		/* real: 0x22A0C0(2,2) 0x42 */
	MeshBumpPass(mdl, rodBumpColor, 0, phase, phase, 0.1f);
	vif1SetAlphaBlend(1, 5, 128);		/* real: 0x22A0C0(0,2) 0x48 */
	MeshBumpPass(mdl, rodBumpColor, 0, rodBumpOfs + phase,
		rodBumpOfs + phase, 0.1f);

	/* 4 - the near glass refracting wb4, ON THE SCREEN.  The visible one. */
	MenuBackBindWork(1);			/* real: 0x22C020(1,0,1) */
	MenuBackScreenTarget(field);
	vif1SetZTest(1);			/* real: 0x22A0C0(1,2) GEQUAL */
	vif1SetAlphaBlend(1, 4, 128);
	MeshDrawPass(mdl, col, size, 1, 0);

	/* 5 - the same faces again into wb3, washed out by extra 255 */
	MenuBackBindWork(1);			/* real: 0x22BFD0(0,1,1) */
	MenuBackWorkTarget(0, nil, field);
	vif1SetZTest(1);
	vif1SetAlphaBlend(1, 4, 128);
	MeshDrawPass(mdl, col, size, 1, 255);
}

/* real: 0x22D920's f12 > 0 arm (0x22D9D4) - the FRONT rod, split in two
 * along Y.  Only the front ring slot ever reaches it: 0x226028 passes
 * every other record split = -1, and the front slot's split climbs from
 * 0 by *(gp-32164) = 0.004 per frame toward 1 - minutes/60 (0x225BF8) -
 * the rod is the clock's minute PROGRESS bar.
 *
 * The arm makes two stack copies of the scene (22d9fc/22da38), then:
 *
 *   copy1 (LOWER)  +0x6C = progress * split           (22da74)
 *                  +0x80 = *(rec+0x100)  = ringColA, the fixed bright
 *                          {167,217,255,0}            (22da7c)
 *                  +0x90 = f13 = (float)rec->0x110 = 100 (22da70)
 *                  -> transformed into the 0x3529D0 bank (22da98)
 *   copy2 (UPPER)  +0x6C = progress * (1 - split)     (22dab0)
 *                  world matrix translated by (0, 26 * progress * split,
 *                  0) through the 0x230180/0x230440 stack-top dance
 *                  (22dabc..22db20) - 26 is the rod's model height, so
 *                  the upper piece sits exactly on the cut
 *                  -> transformed into the 0x3555D0 bank (22db38)
 *
 * and runs the SAME five passes as the one-piece arm (same binds, same
 * targets, same blends - 22db44..22e0e0), each pass walking both banks:
 * lower faces 8..15, upper all but 8-9 (see meshFaces2).  Three deltas
 * against the one-piece pass bodies:
 *
 *   - both pieces shrink toward the WHOLE rod's 0.9 centre (every
 *     0x22C888 gets f12/f13 = sp+448/452, the head's outX/outY);
 *   - the upper piece's emboss T offset gains 2 * progress * split
 *     (22dd58 `add.s f13,f13,f13' on copy1's +0x6C) - the model V spans
 *     0..2 and is scaled by +0x6C, so the term makes the upper piece's
 *     TEXCBUMP phase continue exactly where the lower piece's ends;
 *   - the per-face ST steps are gp-32048/-32044/-32040/-32036, all 0.1
 *     like the one-piece arm's.
 *
 * So the visible result is one rod with a bright cyan-white lower
 * segment growing at 0.004/frame to the minute mark, glass-refracted
 * exactly like the rest of the carousel.  (The lower size 100 halves the
 * Fresnel brightness term against the upper's 200.) */
static void
MeshDrawRodSplit(MeshModel *mdl, sceVu0FMATRIX cam, sceVu0FMATRIX world,
	float progress, float split, const int *colLow, float sizeLow,
	const int *colUp, float sizeUp, int slot, int field)
{
	sceVu0FMATRIX worldUp;
	float phase, refx, refy, tofs;

	/* the lower piece; its origin is the whole rod's, so 0x22CFA8's
	 * outX/outY here are the head's (22d9a8) and both pieces' passes
	 * use them */
	MeshSelect(meshFaces, MESH_LOWER);
	MeshTransform(mdl, cam, world, 1.0f, progress*split, 1.0f);
	refx = meshObjX * 0.9f;		/* real: 22d9c0/22d9c4, *(gp-32052) */
	refy = meshObjY * 0.9f;
	MeshDebug(mdl, progress*split);

	/* real: 22dabc..22db20 - load the world into the stack top,
	 * 0x230440(0, 26 * progress * split, 0), read it back */
	matCopy(mdTop, world);
	mdTranslatef(0.0f, 26.0f*progress*split, 0.0f);
	matCopy(worldUp, mdTop);

	MeshSelect(meshFaces2, MESH_UPPER);
	MeshTransform(mdl, cam, worldUp, 1.0f, progress*(1.0f - split), 1.0f);

	meshRefX = refx;
	meshRefY = refy;
	phase = (float)slot * 0.1f;	/* real: 22d928, *(gp-32056) */
	tofs = 2.0f*progress*split;	/* the upper piece's V continuation */

	vif1SetZWrite(1);		/* ZMSK 0 - see MeshDrawRod */

	if(!cfgMeshTex) {		/* NOT original: argv[14] = 0 */
		MenuBackScreenTarget(field);
		vif1SetZTest(0);
		vif1SetAlphaBlend(1, 4, 128);
		MeshSelect(meshFaces, MESH_LOWER);
		MeshDrawPass(mdl, colLow, sizeLow, 0, 0);
		MeshDrawPass(mdl, colLow, sizeLow, 1, 0);
		MeshSelect(meshFaces2, MESH_UPPER);
		MeshDrawPass(mdl, colUp, sizeUp, 0, 0);
		MeshDrawPass(mdl, colUp, sizeUp, 1, 0);
		MeshSelect(meshFaces, MAXFACEMASK);
		return;
	}

	/* 1 - the far glass refracting wb3, into wb4 (22db44..22dc38) */
	MenuBackBindWork(0);			/* real: 0x22BFD0(1,0,1) */
	MenuBackWorkTarget(1, nil, field);
	vif1SetZTest(0);			/* real: 0x22A0C0(1,1) */
	vif1SetAlphaBlend(1, 4, 128);		/* real: ALPHA_1 0x44 */
	MeshSelect(meshFaces, MESH_LOWER);
	MeshDrawPass(mdl, colLow, sizeLow, 0, 0);
	MeshSelect(meshFaces2, MESH_UPPER);
	MeshDrawPass(mdl, colUp, sizeUp, 0, 0);

	/* 2, 3 - the TEXCBUMP emboss, subtractive at (phase) then additive
	 * at (B0 + phase), exactly the one-piece order; the upper piece's T
	 * carries the +2ps continuation in both (22dd58 / 22deb8) */
	MenuConfigBindBump();			/* real: 0x22AB90(2,1,2) */
	vif1SetZTest(1);			/* real: 0x22A0C0(x,2) GEQUAL */
	vif1SetAlphaBlend(1, 6, 128);		/* real: 0x22A0C0(2,2) 0x42 */
	MeshSelect(meshFaces, MESH_LOWER);
	MeshBumpPass(mdl, rodBumpColor, 0, phase, phase, 0.1f);
	MeshSelect(meshFaces2, MESH_UPPER);
	MeshBumpPass(mdl, rodBumpColor, 0, phase, phase + tofs, 0.1f);
	vif1SetAlphaBlend(1, 5, 128);		/* real: 0x22A0C0(0,2) 0x48 */
	MeshSelect(meshFaces, MESH_LOWER);
	MeshBumpPass(mdl, rodBumpColor, 0, rodBumpOfs + phase,
		rodBumpOfs + phase, 0.1f);
	MeshSelect(meshFaces2, MESH_UPPER);
	MeshBumpPass(mdl, rodBumpColor, 0, rodBumpOfs + phase,
		rodBumpOfs + phase + tofs, 0.1f);

	/* 4 - the near glass refracting wb4, ON THE SCREEN (22defc) */
	MenuBackBindWork(1);			/* real: 0x22C020(1,0,1) */
	MenuBackScreenTarget(field);
	vif1SetZTest(1);			/* real: 0x22A0C0(1,2) GEQUAL */
	vif1SetAlphaBlend(1, 4, 128);
	MeshSelect(meshFaces, MESH_LOWER);
	MeshDrawPass(mdl, colLow, sizeLow, 1, 0);
	MeshSelect(meshFaces2, MESH_UPPER);
	MeshDrawPass(mdl, colUp, sizeUp, 1, 0);

	/* 5 - the same faces into wb3, extra 255 (22dff4) */
	MenuBackBindWork(1);			/* real: 0x22BFD0(0,1,1) */
	MenuBackWorkTarget(0, nil, field);
	vif1SetZTest(1);
	vif1SetAlphaBlend(1, 4, 128);
	MeshSelect(meshFaces, MESH_LOWER);
	MeshDrawPass(mdl, colLow, sizeLow, 1, 255);
	MeshSelect(meshFaces2, MESH_UPPER);
	MeshDrawPass(mdl, colUp, sizeUp, 1, 255);

	MeshSelect(meshFaces, MAXFACEMASK);
}

/* real: 0x22D2E8, all eight loops, in the ROM's own order and against the
 * ROM's own targets.  The `field' arguments are the ones the disassembly
 * has (see MenuBackWorkTarget's comment): the mesh passes carry the
 * interlace half pixel, the one blit between them does not. */
static void
MeshDrawCube(MeshModel *mdl, sceVu0FMATRIX cam, sceVu0FMATRIX world,
	float sx, float sy, float sz, const int *col, float size,
	const int *bumpCol, float bumpOfs, int field)
{
	MeshTransform(mdl, cam, world, sx, sy, sz);
	/* real: *(gp-32064) = 0.35, applied to 0x22CFA8's outX/outY right
	 * after the transform and before any pass */
	meshRefX = meshObjX * 0.35f;
	meshRefY = meshObjY * 0.35f;
	MeshDebug(mdl, sy);

	/* ZMSK **0**.  The ROM never masks Z anywhere in the menu: all 416
	 * ZBUF writes in a retail GS dump of this screen are
	 * `zbp=4480 psm=0' with ZMSK clear, because 0x22BF58/0x22BFD0/
	 * 0x22A4C8 push FRAME and ZBUF together and the ZBUF they push has
	 * ZMSK = 0.  The port's `vif1SetZWrite(0)' here was an addition
	 * (124 of our 136 ZBUF writes carried ZMSK), and it is the second
	 * half of the AA1 crack repair: with Z writes on, a fully covered
	 * pixel stores the stage's flat Z while an AA1 partial-coverage
	 * pixel does not, so the next primitive over that pixel wins its
	 * depth test against the stale value and re-blends the coverage up
	 * to solid.  With ZMSK set nothing is ever stored and the repair
	 * cannot happen - which is why raising the Z alone did not heal our
	 * mask in the replay harness (see notes.md). */
	vif1SetZWrite(1);

	/* 1 - the far glass, refracting the finished screen, into wb4 */
	MenuBackBindScreen();			/* real: 0x22A198 */
	MenuBackWorkTarget(1, nil, field);	/* real: 0x22BF58(1,0,1) */
	vif1SetZTest(0);			/* real: 0x22A0C0(1,1) ZTST 1 */
	vif1SetAlphaBlend(1, 4, 128);		/* real: ALPHA_1 0x44 */
	MeshDrawPass(mdl, col, size, 0, 0);

	if(bumpCol && cfgMeshTex) {
		/* 2, 3 - the TEXCBUMP emboss on the same faces */
		MenuConfigBindBump();		/* real: 0x22AB90(2,1,2) */
		vif1SetZTest(1);		/* real: 0x22A0C0(x,2) GEQUAL */
		vif1SetAlphaBlend(1, 5, 128);	/* real: 0x22A0C0(0,2) 0x48 */
		MeshBumpPass(mdl, bumpCol, 0, bumpOfs, bumpOfs, 0.0f);
		vif1SetAlphaBlend(1, 6, 128);	/* real: 0x22A0C0(2,2) 0x42 */
		MeshBumpPass(mdl, bumpCol, 0, 0.0f, 0.0f, 0.0f);
	}

	/* 4 - the far faces again, flat, into wb3 */
	MenuBackBindWork(1);			/* real: 0x22BFD0(0,1,1) */
	MenuBackWorkTarget(0, nil, field);
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);		/* real: 0x22A0C0(1,1) */
	MeshFlatPass(mdl, col, 0);

	/* 5 - and the near faces black over them, which is what leaves the
	 * cube's area of wb3 holding no backdrop at all */
	vif1SetTEST_1(0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_GREATER);
	vif1SetAlphaBlend(1, 5, 128);		/* real: 0x22A0C0(0,3) 0x48 */
	MeshBlackPass(mdl, 1);

	/* 6 - wb3's left half additively into wb4, so pass 7 can sample it.
	 * NO half pixel: this one is a buffer-to-buffer resample. */
	MenuBackWorkTarget(1, nil, 0);		/* real: 0x22BFD0(1,0,0) */
	MenuBackWorkHalfAdd();			/* real: 0x22C100() */

	/* 7 - the near glass, refracting wb4, into wb3 */
	MenuBackBindWork(1);			/* real: 0x22BFD0(0,1,1) */
	MenuBackWorkTarget(0, nil, field);
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);
	MeshDrawPass(mdl, col, size, 1, 0);

	if(bumpCol && cfgMeshTex) {
		/* 8 - the emboss on the near faces */
		MenuConfigBindBump();
		vif1SetZTest(1);
		vif1SetAlphaBlend(1, 5, 128);
		MeshBumpPass(mdl, bumpCol, 1, bumpOfs, bumpOfs, 0.0f);
		vif1SetAlphaBlend(1, 6, 128);
		MeshBumpPass(mdl, bumpCol, 1, 0.0f, 0.0f, 0.0f);
	}
}

/* real: 0x22D798 - 0x226D00's SECOND walk, into the work buffer 4 that
 * was just cleared to alpha 0.  Only the black pass is ported (0x22CD78's
 * TEXCREFA reflection is the documented gap above); its A = 0x80 is the
 * mask the tail composites by.  0x22D798 scales the refraction centre by
 * *(gp-32060) = 0.2 rather than 0.35, which only 0x22CD78 would use. */
static void
MeshDrawCubeMask(MeshModel *mdl, sceVu0FMATRIX cam, sceVu0FMATRIX world,
	float sx, float sy, float sz, int field)
{
	MeshTransform(mdl, cam, world, sx, sy, sz);
	meshRefX = meshObjX * 0.2f;
	meshRefY = meshObjY * 0.2f;

	vif1SetZWrite(1);			/* ZMSK 0 - see MeshDrawCube */
	MenuBackBindWork(0);			/* real: 0x22BFD0(1,0,1) */
	MenuBackWorkTarget(1, nil, field);
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);		/* real: 0x22A0C0(1,1) */

	/* real: 0x22AB90(5,0,1) - TEXCREFA, ALPHA_1 **0x44**, ZTST ALWAYS -
	 * then the 0x22CD78 loop.  It paints the cubes' only COLOUR into work
	 * buffer 4; the black pass below adds none (see MeshEmitReflFace).
	 *
	 * The 0x44 matters: an earlier read of 0x22AA88 concluded 0x48
	 * (additive), but the retail GS dumps show ALPHA 0101 = 0x44 (source
	 * blend) on every REFA strip, in both this walk and the visible one.
	 * The difference is exactly the bright seams: AA1's antialiased edge
	 * pixels overlap the neighbouring triangle's interior, so an ADDITIVE
	 * pass applies twice along every shared edge and tristrip diagonal
	 * (double the env-map sample = a bright line), while a BLEND applied
	 * twice just converges on the same colour. */
	if(cfgMeshTex) {
		MenuConfigBindRefa();
		vif1SetZTest(0);
		vif1SetAlphaBlend(1, 4, 128);	/* real: ALPHA_1 0x44 */
		MeshReflPass(mdl, cubeReflColor, 1, 1);	/* real: aa = 1 */
	}

	vif1SetTEST_1(0, 0, 0, 0, 0, 0, 1, SCE_GS_DEPTH_GREATER);
	vif1SetAlphaBlend(1, 5, 128);		/* real: 0x22A0C0(0,3) */
	MeshBlackPass(mdl, 1);
}

/* ==================== the models ==================== */

static MeshModel rodModel  = { 0, nil, nil, nil };
static MeshModel cubeModel = { 0, nil, nil, nil };

/* ============== the deferred record's mesh half ==============
 *
 * real: 0x2266E0 unpacks the record into 0x22D920(rec+0x10, rec+0x100,
 * f12 = rec->0xF4, f13 = (float)rec->0x110).  0x22D920's f12 <= 0 arm
 * is the plain one-piece rod; its f12 > 0 arm (only the front slot, once
 * its `split' has grown past zero) is MeshDrawRodSplit above. */
void
MenuConfigDrawMesh(SceneRec *rec)
{
	if(rec->progress < 0.0f)
		return;
	/* real: the rod scene's camera pointer is the frame's own, written
	 * into 0x27E950+0x64 by 0x2268F0.  rec->index is the ring slot, i.e.
	 * the scene struct's +0x00, which 0x22D920 turns into the TEXCBUMP
	 * phase f21; the field is the module's own snapshot, which is what
	 * all three of 0x22D920's FRAME pushes pass. */
	if(rec->f12 > 0.0f) {		/* real: 22d9b4/22d9cc, 0 < split */
		MeshDrawRodSplit(&rodModel, menuCamera, rec->world,
			rec->progress, rec->f12,
			rec->colA, (float)rec->aux,	/* real: rec+0x100, +0x110 */
			rec->col0, rec->size,
			rec->index, MenuBackField());
		return;
	}
	MeshDrawRod(&rodModel, menuCamera, rec->world, 1.0f, rec->progress, 1.0f,
		rec->col0, rec->size, rec->index, MenuBackField());
}

/* ============ the deferred bloom's mesh half (0x22E428) ============
 *
 * real: 0x22E428, called twice per record by 0x2267E8 (menu.c's
 * SceneFlush) after the sorted walk has finished.  0x22E428 branches on
 * the record's split (+0xF4): `> 0' takes the split-rod arm at 0x22E4D0
 * (MeshFlushSplit below), everything else the simple arm at **0x22E9A8**,
 * which is what this is.
 *
 * 0x22E9A8, guarded by `0 <= walk < 2' (22e9a8 slti / 22e9b4 bltz - an
 * out-of-range walk falls to 0x22EB98, a lone pass A that nothing calls):
 *
 *   0x22AB90(0, 0, 2)      TEXCFLOW
 *   0x22A0C0(1, 1)         ALPHA_1 0x44, ZTST ALWAYS
 *   for each cull != 0 face:  0x22CD78(face, scene, aa = **0**)
 *   0x22ED10               flush the packet
 *   walk 1: 0x22AB90(2, 1, 2)  TEXCBUMP, ST offset f21 + i*0.1 + scene->0xB0
 *   walk 0: 0x22AB90(3, 1, 2)  TEXCBINV, ST offset f21 + i*0.1 + 0
 *   0x22A0C0(2, 1)         ALPHA_1 0x42 SUBTRACTIVE, ZTST ALWAYS
 *   for each cull != 0 face:  0x22C920(face, scene + 0xD0)
 *   0x22ED10
 *
 * with `f21 = (float)scene->+0x00 * *(gp-32024)' - the same slot * 0.1
 * phase MeshDrawRod's emboss uses - and the per-face step *(gp-32000) /
 * *(gp-31996), both 0.1, counted over EVERY face index, culled ones
 * included, exactly as 0x22E0EC does for the visible rod.
 *
 * So each walk is an environment-mapped rod (TEXCFLOW through the SAME
 * spherical reflect() 0x22CD78 the cube mask uses, but with AA1 and ABE
 * both clear) with a subtractive emboss over it, and the two walks differ
 * ONLY in which half of the emboss pair they use: TEXCBUMP on walk 1,
 * TEXCBINV - its exact bitwise complement - on walk 0.  Each is composited
 * additively over the screen at alpha 30 by SceneFlush.  That pair of
 * near-cancelling layers is the clock's sparkle, and it is the last render
 * stage of the System Configuration screen the port was missing.
 *
 * PRIM 276 (no ABE) means pass A writes work buffer 4 flat, alpha included
 * - the colour it stamps IS the record's own col1, so the composite's As
 * comes out of that alpha and not out of the 0x27EBF0 clear's 0x80 wherever
 * a rod covers a pixel. */

/* real: 0x27E950 + 0xD0, {0x28,0x28,0x28,0x80} - static .data, where the
 * rod's other emboss colour (+0xA0, rodBumpColor) is {8,8,8,128}.  This
 * pass is five times as strong because it is subtracted once and never
 * added back. */
static const int rodFlowColor[4] = { 0x28, 0x28, 0x28, 0x80 };

/* real: 0x22E4D0 - 0x22E428's split arm, the front rod's half of the
 * deferred bloom.  The same two stack copies as MeshDrawRodSplit
 * (lower = progress*split, upper = progress*(1-split) translated up by
 * 26*progress*split through the 0x230180/0x230440 dance, same face-index
 * skips, same 2*progress*split T continuation for the upper piece) with
 * three deltas of its own:
 *
 *   - BOTH copies' +0xC0 - the colour 0x22CD78 stamps - become
 *     *(rec+0x120) = ringColB = {0x3C,0x3C,0x3C,0x80} (22e580/22e5c0),
 *     not the slot's own col1.  This is why every one of retail's 86
 *     flush draws is rgba=3c3c3c80 even though the front slot's col1 is
 *     {0x80,0x80,0x80,0x1E}: at RGB 0x3C the front rod blooms like the
 *     other eleven, and - because PRIM 276's pass A writes its alpha
 *     flat - composites at As 0x80 instead of 0x1E over its own faces.
 *   - pass A runs under 0x22A0C0(0,1) = ALPHA_1 0x48 (22e674/22e688)
 *     where the simple arm pushes (1,1) = 0x44 - both dead, PRIM 276
 *     has ABE clear; mirrored anyway.
 *   - the emboss TEXTURES are SWAPPED against the simple arm: walk 1
 *     binds slot 3 = TEXCBINV (22e74c `li a0,3') and walk 0 slot 2 =
 *     TEXCBUMP (22e884 `li a0,2'), while the ST offset stays with the
 *     walk (walk 1 gets scene->+0xB0, walk 0 gets 0) exactly as in the
 *     simple arm.  Net effect: the front rod's sparkle uses the emboss
 *     pair in the opposite order - the sum is the same, the sign of the
 *     surviving difference is not.
 *
 * The per-face ST steps are gp-32016/-32012/-32008/-32004, all 0.1 like
 * the simple arm's.  0x22E4D0 has no f13/size argument - pass A's
 * 0x22CD78 never reads +0x90 - and neither piece keeps the record's
 * col1, so only progress, split, world and colB come out of the record. */
static void
MeshFlushSplit(SceneRec *rec, int walk)
{
	sceVu0FMATRIX worldUp;
	float phase, ofs, tofs;

	/* the lower piece; 0x22E4D0 computes the 0.9 centre from the head's
	 * transform (22e4ac/22e4c4), but the origin is scale-invariant so
	 * copy1's is the same - and neither of this stage's emits reads it
	 * (see MenuConfigFlushMesh) */
	MeshSelect(meshFaces, MESH_LOWER);
	MeshTransform(&rodModel, menuCamera, rec->world, 1.0f,
		rec->progress*rec->f12, 1.0f);
	meshRefX = meshObjX * 0.9f;
	meshRefY = meshObjY * 0.9f;

	/* real: 22e5c8..22e634 - the same stack-top translate */
	matCopy(mdTop, rec->world);
	mdTranslatef(0.0f, 26.0f*rec->progress*rec->f12, 0.0f);
	matCopy(worldUp, mdTop);

	MeshSelect(meshFaces2, MESH_UPPER);
	MeshTransform(&rodModel, menuCamera, worldUp, 1.0f,
		rec->progress*(1.0f - rec->f12), 1.0f);

	phase = (float)rec->index * 0.1f;	/* real: 22e490, *(gp-32024) */
	tofs = 2.0f*rec->progress*rec->f12;	/* real: 22e850/22e974 */

	/* pass A - both pieces, colour = *(rec+0x120) */
	MenuConfigBindFlow();			/* real: 0x22AB90(0, 0, 2) */
	vif1SetZTest(0);			/* real: 0x22A0C0(0, 1) ALWAYS */
	vif1SetAlphaBlend(1, 5, 128);		/* real: ALPHA_1 0x48, dead */
	MeshSelect(meshFaces, MESH_LOWER);
	MeshReflPass(&rodModel, rec->colB, 1, 0);
	MeshSelect(meshFaces2, MESH_UPPER);
	MeshReflPass(&rodModel, rec->colB, 1, 0);

	/* pass B - the emboss, textures swapped against the simple arm */
	if(walk) {
		MenuConfigBindBinv();		/* real: 0x22AB90(3, 1, 2) */
		ofs = rodBumpOfs + phase;
	} else {
		MenuConfigBindBump();		/* real: 0x22AB90(2, 1, 2) */
		ofs = phase;
	}
	vif1SetZTest(0);			/* real: 0x22A0C0(2, 1) ALWAYS */
	vif1SetAlphaBlend(1, 6, 128);		/* real: ALPHA_1 0x42 */
	MeshSelect(meshFaces, MESH_LOWER);
	MeshBumpPass(&rodModel, rodFlowColor, 1, ofs, ofs, 0.1f);
	MeshSelect(meshFaces2, MESH_UPPER);
	MeshBumpPass(&rodModel, rodFlowColor, 1, ofs, ofs + tofs, 0.1f);

	MeshSelect(meshFaces, MAXFACEMASK);
}

void
MenuConfigFlushMesh(SceneRec *rec, int walk)
{
	float phase, ofs;

	/* real: 22e484/22e48c - `scene->+0x6C < 0' returns immediately */
	if(rec->progress < 0.0f)
		return;

	if(rec->f12 > 0.0f) {		/* real: 22e4b8/22e4d0, 0 < split */
		MeshFlushSplit(rec, walk);
		return;
	}

	MeshTransform(&rodModel, menuCamera, rec->world, 1.0f, rec->progress, 1.0f);
	/* real: 22e4c4/22e4c8, *(gp-32020) = 0.9 - the same 0.9 MeshDrawRod
	 * applies.  Neither of this stage's emits reads meshRefX/Y (0x22CD78
	 * takes its UV from the reflection vector, 0x22C920 from the model's),
	 * but the ROM computes it here, so do it and leave nothing stale. */
	meshRefX = meshObjX * 0.9f;
	meshRefY = meshObjY * 0.9f;

	/* real: 22e490, f21 = (float)scene->+0x00 * *(gp-32024) = slot * 0.1 */
	phase = (float)rec->index * 0.1f;

	/* pass A - the environment map, identical in both walks.  ALPHA_1
	 * 0x44 is pushed but dead: PRIM 276 has ABE clear. */
	MenuConfigBindFlow();			/* real: 0x22AB90(0, 0, 2) */
	vif1SetZTest(0);			/* real: 0x22A0C0(1, 1) ALWAYS */
	vif1SetAlphaBlend(1, 4, 128);		/* real: ALPHA_1 0x44 */
	MeshReflPass(&rodModel, rec->col1, 1, 0);	/* real: 0x22CD78(f, scene, 0) */

	/* pass B - the emboss, subtractive.  Walk 1 carries the rod scene's
	 * own -0.008 ST offset (0x27E950 + 0xB0, rodBumpOfs); walk 0 gets 0. */
	if(walk) {
		MenuConfigBindBump();		/* real: 0x22AB90(2, 1, 2) */
		ofs = rodBumpOfs + phase;
	} else {
		MenuConfigBindBinv();		/* real: 0x22AB90(3, 1, 2) */
		ofs = phase;
	}
	vif1SetZTest(0);			/* real: 0x22A0C0(2, 1) ALWAYS */
	vif1SetAlphaBlend(1, 6, 128);		/* real: ALPHA_1 0x42 */
	MeshBumpPass(&rodModel, rodFlowColor, 1, ofs, ofs, 0.1f);
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

/* real: 0x225318 - the per-frame colour spreader.  Every frame it
 * repaints the whole ring from the current ringOffset: the eleven plain
 * slots get the body/edge pair (0x27EAC0 / 0x27EAD0) and the front slot
 * (i == ringOffset) gets the bright front blend and col1 (0x27EAF0).
 * InitMenuConfig only seeded the INITIAL front slot, so once an hour step
 * moved ringOffset the old rod kept its bright colour while the new rod
 * got only the split - two highlighted rods.  (The keyframe cyclers
 * 0x225528 / 0x2255A8 that animate the four vectors over 8 entries are
 * not ported; the idle values are spread directly.)  The tail eases the
 * record colours ringColA -> cfgColFixed and ringColB -> cfgColRingA,
 * which InitMenuConfig already parks on their targets, so idle it is a
 * no-op. */
static void
CarouselColors(void)
{
	int i, k;

	for(i = 0; i < NRING; i++)
		for(k = 0; k < 4; k++)
			if(i == ringOffset) {
				ring[i].col0[k] = cfgColFront[k];
				ring[i].col1[k] = cfgColRingB[k];
			} else {
				ring[i].col0[k] = cfgColBody[k];
				ring[i].col1[k] = cfgColEdge[k];
			}
	for(k = 0; k < 4; k++) {
		ringColA[k] = cfgColFixed[k];
		ringColB[k] = cfgColRingA[k];
	}
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
	CarouselColors();	/* real: 0x225318 - re-front the bright colours */

	for(i = 0; i < NRING; i++) {
		if(i != ringOffset)
			ring[i].split = 0.0f;
		else if(ring[i].split < ringSplitMax) {
			ring[i].split += 0.004f;	/* real *(gp-32164) */
			if(ring[i].split > ringSplitMax)
				ring[i].split = ringSplitMax;
		} else
			/* real: 225c6c's bc1fl arm - `split >= max' STORES the
			 * max, so the front rod's bright segment snaps DOWN the
			 * moment the max shrinks.  The Clock Adjustment editor
			 * is where it shows: stepping the minute field UP
			 * shrinks 1 - minutes/60 and retail's rod follows
			 * instantly; the port only ever grew toward it. */
			ring[i].split = ringSplitMax;
		ring[i].progress = carouselTimer.duration ?
			(float)(carouselTimer.count*128/carouselTimer.duration) * 0.0078125f : 0.0f;
	}
}

/* real: 0x225AD0 - the ONE opener of 0x27EB00, called only from
 * 0x2272C0 inside "enter System Configuration".  It resets the timer,
 * opens it and clears every slot's progress.
 *
 * Its duration is 1 (see cfgCarouselDur), so the very next step takes
 * the count straight to the duration: progress goes 0 -> 1 in a single
 * frame and the rods are at full height the first time the emitter's
 * 0.05 gate lets them through.  The ramp is real - 0x226028 still feeds
 * it to the scene's +0x6C and 0x22D920 still uses that as the rods' Y
 * scale - it just never spans more than one frame, which is why retail
 * shows no growth. */
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

/* real: 0x225B68 - the ONE closer of 0x27EB00, called only from
 * 0x22749C, the last edge of the state machine's closing arm.  Like the
 * opener it zeroes every slot's progress by hand, so the rods are gone
 * on the same frame rather than shrinking back down. */
static void
CarouselClose(void)
{
	int i;

	if(!cfgIsState(&carouselTimer, 2))
		return;
	cfgClose(&carouselTimer);
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
 * 0x226FA8 also runs 0x226CF8 = 0x226BB8, which rewrites two of the
 * table's own fields every frame (see MenuConfigCubeState below), so the
 * 0x27F090 colour and size bias are live state, not constants.
 *
 * The ROM draws the whole set twice - the eight-loop 0x22D2E8 and then
 * 0x22D798 into work buffer 3 - then runs the work-buffer-3 twin of the
 * zoom blur (0x22BFD0/0x22C088/0x22C2A0) and blits the result back over
 * the screen.  The port runs the first walk's six picture-carrying loops
 * (see MeshDraw) and neither the second walk nor the blur composite.
 *
 * The camera is NOT the orb scene's - see cubeCamera above; that is what
 * puts the cubes at their real size. */

static int cubeSpin;		/* real: *(u16*)(gp-30432) */

/* ---- 0x226BB8, the per-frame cube state (0x226FA8's 0x226CF8) ----
 *
 * The 0x27F090 table is not read-only: its +0x10 colour and +0x20 size
 * bias are LIVE, and 0x226BB8 rewrites both every frame, between the
 * cube timer's step and 0x226D00's walk.  For each of the five entries:
 *
 *   if(i == *(0x27BE28+16))            the item list's cursor
 *       0x22EC60(0x27EC30, 0x27EC10, 1)      tracker -> {0,150,200,128}
 *       target = *(0x27EC30)                 the tracker
 *       alpha(+0x24) += 8, clamped to 128
 *   else
 *       target = *(0x27EC20) = {100,100,100,128}
 *       alpha(+0x24) -= 8, clamped to 0
 *   0x22EC60(entry+0x10, target, 7)          the cube's own colour
 *   entry+0x20 *= *(gp-32144) = 0.95         the size bias decays
 *
 * so an unselected cube settles on a dim grey 100 and the cursor's one
 * drifts to the menu's blue-cyan, and the {128,128,128,128} in the .data
 * table is only where they start.  The bias is a KICK, not a constant:
 * 0x227BE8's confirm arm writes *(gp-32136) = -0.1 into the cursor's
 * entry, so the pressed cube shrinks 10 % and grows back as the 0.95
 * decays it.  menutext.c's mode-1 confirm seeds it through
 * MenuConfigCubeKick below.
 *
 * The +0x24 label alpha is deliberately NOT touched here: it is the 2D
 * item layer's, and menutext.c owns that side. */
static int cubeColor[5][4];	/* real: 0x27F090 + n*48 + 0x10, live */
static float cubeBias[5];	/* real: 0x27F090 + n*48 + 0x20, live */
static int cubeTracker[4];	/* real: 0x27EC30 */
static const int cubeColSel[4]   = {   0, 150, 200, 128 };	/* 0x27EC10 */
static const int cubeColPlain[4] = { 100, 100, 100, 128 };	/* 0x27EC20 */

/* real: *(0x27BE28 + 16), the config item list header's cursor.  That
 * header is 2D-layer state and menutext.c owns it in the port, so it
 * hands the value over here; until it does, item 0 is selected, which is
 * the header's own .data value. */
static int cubeCursor;

void
MenuConfigSetCursor(int n)
{
	cubeCursor = (u32)n < 5u ? n : 0;
}

/* real: 0x227BE8's confirm arm writes *(gp-32136) = -0.1 straight into
 * the cursor's 0x27F090 entry at +0x20 (22c28: `swc1 $f0,32(v0)') - the
 * pressed cube snaps 10 % smaller and 0x226BB8's 0.95 decay grows it
 * back.  menutext.c's mode-1 confirm calls this. */
void
MenuConfigCubeKick(int n)
{
	if((u32)n < 5u)
		cubeBias[n] = -0.1f;
}

/* real: 0x22EC60(dst, src, rate) for rate != 1 - four independent
 * integer components, each stepped by `rate' toward the target and
 * clamped on overshoot.  (Its rate == 1 arm at 0x22EC70 is the same
 * thing with +-1 written out; the tracker uses that one.) */
static void
cfgEase(int *dst, const int *src, int rate)
{
	int k;

	for(k = 0; k < 4; k++) {
		if(dst[k] == src[k])
			continue;
		if(dst[k] < src[k]) {
			dst[k] += rate;
			if(dst[k] > src[k])
				dst[k] = src[k];
		} else {
			dst[k] -= rate;
			if(dst[k] < src[k])
				dst[k] = src[k];
		}
	}
}

/* real: 0x226BB8 */
static void
MenuConfigCubeState(void)
{
	const int *target;
	int i;

	for(i = 0; i < 5; i++) {
		if(i == cubeCursor) {
			cfgEase(cubeTracker, cubeColSel, 1);
			target = cubeTracker;
		} else
			target = cubeColPlain;
		cfgEase(cubeColor[i], target, 7);
		cubeBias[i] *= 0.95f;		/* real: *(gp-32144) */
	}
}

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
	int i, ang, field;

	cfgStep(&cfgCubeTimer);
	cubeSpin = (cubeSpin + 30) & 0xFFFF;	/* real: 0x2285C0 */
	/* real: 0x226FA8 runs 0x226CF8 (= 0x226BB8) between the timer step
	 * and 0x226D00, unconditionally - the colours keep easing even while
	 * the cube timer is closed */
	MenuConfigCubeState();
	if(cfgIsState(&cfgCubeTimer, 0)) {
		cubeScreenValid = 0;
		return;
	}

	s = cfgCubeTimer.duration ?
		(float)cfgCubeTimer.count / (float)cfgCubeTimer.duration : 0.0f;

	/* real: 0x22D2E8's FRAME push is 0x22BF58(1, 0, **1**) - 22d36c is
	 * `li a2,1', not `move a2,zero' - so the cube meshes carry the
	 * interlace half pixel exactly as the rods' three pushes do.  Only
	 * the buffer-to-buffer sprites in the chain (0x22C100, 0x22C088,
	 * 0x22C190) are pushed with field = 0, which is 37efd18's rule.
	 * (eba5595's commit message and docs/menu-config.md's glass write-up
	 * both say the whole cube stage runs with no half pixel; that reading
	 * of 0x22BF58's third argument was wrong.) */
	field = 1;

	/* ---- 0x226D00's first walk: the eight-pass glass, per cube ---- */
	for(i = 0; i < 5; i++) {
		ang = (short)(cubeSpin + i*7000);
		matUnit(mdTop);
		/* real: 0x226DD4 is `jal 0x2303E8' with a0 = the table entry
		 * ITSELF, not the three-float 0x230440 - so the table's w = 0
		 * reaches the matrix and flattens the stage's Z.  See
		 * mdTranslate() in menu.c for the full derivation; the port used
		 * mdTranslatef here, which forced w = 1 and emitted real
		 * projected Z (~5.2M-6.2M against retail's flat 0xFFFFF010).
		 * That broke the AA1 crack repair - the refract pass is AA1 and
		 * leaves Z unwritten on partial-coverage pixels (face outlines
		 * AND every tristrip diagonal), the non-AA1 bump taps that
		 * follow run ZTST GEQUAL and so lost the repair wherever the
		 * stale Z happened to be higher, which is where the black seams
		 * and the "1px dashed dark line along the diagonals" came from.
		 * Retail's near-max flat Z makes the repair win unconditionally. */
		mdTranslate(menuCubePos[i]);
		mdRotX(ang);
		mdRotY(ang);
		mdRotZ(ang);
		/* real: the identity camera at 0x352840, *(gp-32064) = 0.35 on
		 * the refraction centre, the bump colour at scene+0xA0 =
		 * {8,8,8,128} and the emboss offset at scene+0xB0 = 0.01 */
		MeshDrawCube(&cubeModel, cubeCamera, mdTop, s + cubeBias[i],
			s + cubeBias[i], s + cubeBias[i],
			cubeColor[i], 200.0f,	/* real: scene 0x27EFB0 +0x90 */
			cubeBumpColor, 0.01f, field);
		cubeScreenX[i] = meshObjX;
		cubeScreenY[i] = meshObjY;
	}

	/* real: 0x22A4C8(1, 0x27F180, *(0x27B448)) - work buffer 4 becomes
	 * the mask, so it starts at alpha 0 everywhere */
	vif1SetZTest(0);
	MenuBackWorkTarget(1, cubeMaskClear, field);

	/* ---- the second walk: near faces only, black at A = 0x80 ---- */
	for(i = 0; i < 5; i++) {
		ang = (short)(cubeSpin + i*7000);
		matUnit(mdTop);
		mdTranslate(menuCubePos[i]);	/* real: 0x226E98, same w = 0 */
		mdRotX(ang);
		mdRotY(ang);
		mdRotZ(ang);
		MeshDrawCubeMask(&cubeModel, cubeCamera, mdTop, s + cubeBias[i],
			s + cubeBias[i], s + cubeBias[i], field);
	}

	/* ---- 0x226D00's tail ---- */
	/* real: 0x22BFD0(0,1,0) then 0x22C088() - wb4 over wb3, additive,
	 * and it is this blit that stamps the mask into wb3's alpha */
	MenuBackWorkTarget(0, nil, 0);
	MenuBackWorkAdd();
	/* real: 0x22C2A0(phase < 5 ? 5 - phase : 0); the ramp idles at 10, so
	 * this is 0 passes on a settled screen */
	MenuBackWorkBlur(MenuBackPhase() < 5 ? 5 - MenuBackPhase() : 0);
	/* real: 0x22C020(0,0,0) then 0x22C190(**1**) - wb3 back over the
	 * screen, ABE set, blended by the alpha the mask put there */
	MenuBackScreenTarget(0);
	MenuBackWorkOver(1);

	/* real: the next stage's own 0x22A3B8 puts the field back; the port's
	 * 2D layer does not push one, so restore it here */
	MenuBackScreenTarget(1);
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);
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

/* real: the head of 0x227D08, the 0x227DE8 tail that dispatches the pad.
 * It runs the focus dance and reaches a pad handler (0x2279B8/0x227BE8)
 * ONLY while the screen's Anim is fully open - 0x22AC48(0x27BE44, 2) -
 * AND no value sub-screen timer is running - 0x22AC48(0x27EC40, 0); on
 * any other frame it fires the focus-off callback and returns.  So the
 * ROM ignores the pad for the whole 90-frame opening (and closing)
 * choreography, which is what keeps the confirm press that ENTERED the
 * screen from falling through into the item list on the same frame.
 * The port never opens its 0x27EC40 counterpart, so that half of the
 * gate is always true and only the state-2 check is modelled. */
int
MenuConfigFullyOpen(void)
{
	return cfgIsState(&cfgAnim, 2);
}

/* real: 0x226A60 - the config screen's own alpha: the last dur10 frames
 * of the Anim's count, scaled by the module fade. */
int
MenuConfigAlpha(int fadeAlpha)
{
	int v;

	v = cfgCount(&cfgAnim) - (cfgDur40b + cfgDur40);
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
 * is recomputed from the three per-screen durations (0x227290: gp-30380
 * + gp-30400 + gp-30396 = 40+40+10 = 90 on NTSC), 0x22AEC8 closes the
 * vignette timer at 0x27F620 (menu.c), 0x2291E8 opens the backdrop's
 * fade timer and 0x225AD0 opens the carousel.  The cube timer is NOT
 * opened here - the state machine 0x227390 does that when the Anim's
 * count reaches gp-30380, and MenuConfigStateMachine now models that
 * edge. */
void
MenuEnterConfig(void)
{
	if(!cfgIsState(&cfgAnim, 0))
		return;
	cfgAnim.duration = cfgDur40b + cfgDur40 + cfgDur10;
	cfgOpen(&cfgAnim);
	MenuVignetteClose();		/* real: 0x22AEC8 at 0x2272B0 */
	MenuBackFadeOpen();		/* real: 0x2291E8 */
	CarouselOpen();			/* real: 0x225AD0 */
	OSDDispatch(20992, 1, 4, 0);	/* real: the 0x2287A8 tail */
	printf("osdsys: enter System Configuration\n");
}

/* real: 0x227C20's TRIANGLE arm - "leave System Configuration" only
 * closes the Anim.  Every other timer is closed by the state machine
 * below, on its own edge of the Anim's countdown. */
void
MenuLeaveConfig(void)
{
	if(!cfgIsState(&cfgAnim, 2))
		return;
	cfgClose(&cfgAnim);
	printf("osdsys: leave System Configuration\n");
}

/* real: 0x227390 - the state machine proper, run right after the Anim's
 * own step in 0x227DE8.  Every other timer's edge is keyed on the
 * Anim's count here, and that phasing is the whole entry/exit
 * choreography:
 *
 *   opening  count == gp-30380 (40)          open the cube timer (0x226B28)
 *   closing  duration-count == dur10 (10)    close the cube timer (0x226B70)
 *   closing  count == gp-30380+dur40 (80)    reopen the vignette (0x22AE80)
 *   closing  count == gp-30380 (40)          close the backdrop fade (0x229230)
 *   closing  count == gp-30372 (1)           close the carousel (0x225B68)
 *
 * so on the way out the clock stays lit at full height for 89 of the 90
 * frames and then goes in one - it never shrinks.  The three closing
 * arms are mutually exclusive `else if's in the ROM and are gated on
 * 0x223790's timer (0x27C258, the module-level screen state) being
 * idle; the port has no such timer, so the gate is dropped. */
static void
MenuConfigStateMachine(void)
{
	int count;

	count = cfgCount(&cfgAnim);
	if(cfgIsState(&cfgAnim, 1)) {		/* real: 0x2273AC */
		if(count == cfgDur40b)
			cfgOpen(&cfgCubeTimer);	/* real: 0x2273D0 */
		return;
	}
	if(!cfgIsState(&cfgAnim, 3))		/* real: 0x2273E0 */
		return;
	if(cfgAnim.duration - count == cfgDur10)
		cfgClose(&cfgCubeTimer);	/* real: 0x227414 */
	if(count == cfgDur40b + cfgDur40)
		MenuVignetteOpen();		/* real: 0x227454, 0x22AE80 */
	else if(count == cfgDur40b)
		MenuBackFadeClose();		/* real: 0x227478, 0x229230 */
	else if(count == cfgCarouselDur)
		CarouselClose();		/* real: 0x22749C, 0x225B68 */
}

/* real: 0x227DE8 - the screen's per-frame slot in the hub 0x2283F0:
 * step the Anim, run the state machine, read the pad, draw the items.
 * The item drawing lives in menutext.c. */
void
MenuConfigStep(void)
{
	cfgStep(&cfgAnim);		/* real: 0x227DF4 */
	MenuConfigStateMachine();	/* real: 0x227DFC */
}

/* real: 0x21CE58's share - the ring is rebuilt by 0x225998 and the
 * durations come from 0x22AD38 and 0x228460, in that order. */
void
InitMenuConfig(void)
{
	int rate, i, k;

	rate = IsPAL() ? 50 : 60;
	cfgDur40 = rate*40/60;		/* real: *(gp-30400), 0x22850C */
	cfgDur10 = rate/6;		/* real: *(gp-30396), 0x228568 */
	cfgDur40b = rate*40/60;		/* real: *(gp-30380), 0x22AD7C */
	cfgCarouselDur = 1;		/* real: *(gp-30372), 0x22ADA4 */

	memset(&cfgAnim, 0, sizeof(cfgAnim));
	memset(&carouselTimer, 0, sizeof(carouselTimer));
	memset(&cfgCubeTimer, 0, sizeof(cfgCubeTimer));
	cfgAnim.duration = cfgDur40b + cfgDur40 + cfgDur10;
	carouselTimer.duration = cfgCarouselDur;	/* real: 0x2259B4 */
	cfgCubeTimer.duration = cfgDur40;	/* real: 0x228534 */

	rodModel.nfaces = menuRodFaces;
	rodModel.verts = menuRodVerts;
	rodModel.norms = menuRodNorms;
	rodModel.uvs = menuRodUVs;
	cubeModel.nfaces = menuCubeFaces;
	cubeModel.verts = menuCubeVerts;
	cubeModel.norms = menuCubeNorms;
	cubeModel.uvs = menuCubeUVs;

	/* real: 0x228460's 0x267630(0x352840) - the cube scene's camera, and
	 * the only thing in the image that ever writes it */
	matUnit(cubeCamera);

	/* real: 0x22A9B8(n) for TEXC slots 2, 5, 0 and 3 - one decoder
	 * (0x22A720) serves all four, so one DecodeBump() does */
	DecodeBump();
	InitTexture(&bumpTexture);
	InitTexture(&refaTexture);
	InitTexture(&flowTexture);
	InitTexture(&binvTexture);

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
		/* the record colours - see ringColA/ringColB's comment */
		ringColA[k] = cfgColFixed[k];	/* real: 0x34E910 -> 0x34E940 */
		ringColB[k] = cfgColRingA[k];	/* real: 0x34E920 -> 0x27EAE0 */
	}

	cubeSpin = 0;			/* real: 0x2284C4 */
	/* real: the .data the 0x27F090 table and 0x27EC30 start at, which
	 * 0x226BB8 then eases away from */
	for(i = 0; i < 5; i++) {
		for(k = 0; k < 4; k++)
			cubeColor[i][k] = menuCubeColor[i][k];
		cubeBias[i] = menuCubeBias[i];
	}
	for(k = 0; k < 4; k++)
		cubeTracker[k] = 0x80;
	cubeCursor = 0;
	cfgMeshTex = OsdArgInt(14, 1);
	cfgDebug = OsdArgInt(6, 0);
}
