/* Reconstruction of the OSDSYS main-menu 3D background scene: the seven
 * glowing orbs that orbit the origin behind the system menu.
 *
 * This is Module U (0x21C910-0x230000 in the retail image), a completely
 * separate implementation from the opening's tower/light/cube pipeline -
 * it shares nothing with it but the sceVu0 matrix library and the C
 * runtime (docs/menu-scene.md 0).  Hex addresses in the comments refer
 * to the retail image; gp = 0x2AF070.
 *
 * The whole animation is driven by the real-time clock: the orbit rates
 * are the second and minute "hands" scaled to a 16-bit angle, and the
 * cloud's initial orientation comes from the hour.  osdbits has no RTC,
 * so menu.c runs its own software clock (InitClock/ClockTick) seeded
 * from argv or the cycle counter.
 *
 * What is here (milestone): the camera, the orbit transform, the entry
 * ease-in and camera fly-in, the deferred depth-sorted draw list, and
 * the per-orb renderer (50-sample trail + TEXCBLUR halo + TEXCNAVI
 * core).  What is NOT: the TEXCKABE backdrop/smoke mesh, the zoom-blur
 * feedback composite, the 12-slot fly-in carousel, the menu's 2D text
 * and widgets.  See the header comment of MenuFrame().
 *
 * Run it as
 *     main.elf menu [hh [mm [ss [framelimit [fromOpening [fadeAlpha
 *                    [debugFrame [cursor [notext [textDump [backPhase
 *                    [back [cfgEnter [cfgLeave [meshTex
 *                    [cfgCursor]]]]]]]]]]]]]]]]
 * cfgEnter/cfgLeave are frame numbers at which to call
 * MenuEnterConfig()/MenuLeaveConfig() - the headless stand-in for the
 * pad (menuconfig.c); 0 means never.
 * All optional; with no arguments the clock is seeded from the cycle
 * counter and the scene runs forever.  fromOpening (default 1) decides
 * whether all seven orbs fly in or only orb 0; fadeAlpha starts the
 * entry fade part-way (128 = skip it and go straight to the settled
 * scene); debugFrame prints the orb geometry for the first two frames
 * and an ASCII picture of frame `debugFrame` - the headless substitute
 * for a screenshot. */

#include <stdio.h>
#include "inc.h"
#include "res.h"


#define NUMORBS 7		/* real: the loop bound in 0x2261B8 */
#define TRAILLEN 50		/* real: the wrap constant in 0x22FEC0 */
#define TRAILDRAW 49		/* real: the vertex count in 0x22EFF0 */

/* ================= the clock (real: the 0x352980 block) =================
 *
 * The real block is filled from the low-block RTC copy and advanced
 * every frame by 0x22BB30 (which integrates *(gp-30324) = 1000/59.94 ms
 * per frame and re-syncs against the RTC whenever it drifts >3 s).  The
 * four accessors the scene uses:
 *   0x22B5E8 / 0x22B590  seconds + ms/1000
 *   0x22B6B0 / 0x22B640  minutes + seconds/60
 *   0x22B720             hours   + minutes/60
 * They are literal clock hands; the scene reads them as angles. */

static float clockMs;
static int clockSec, clockMin, clockHour;

float MenuClockSeconds(void) { return clockSec + clockMs*0.001f; }
float MenuClockMinutes(void) { return clockMin + MenuClockSeconds()/60.0f; }
float MenuClockHours(void)   { return clockHour + MenuClockMinutes()/60.0f; }
#define ClockSeconds MenuClockSeconds
#define ClockMinutes MenuClockMinutes
#define ClockHours MenuClockHours

/* real: 0x22BB30's ms integration, minus the RTC resync */
static void
ClockTick(void)
{
	clockMs += IsPAL() ? 1000.0f/50.0f : 1000.0f/59.94f;
	while(clockMs >= 1000.0f) {
		clockMs -= 1000.0f;
		if(++clockSec >= 60) {
			clockSec = 0;
			if(++clockMin >= 60) {
				clockMin = 0;
				clockHour = (clockHour+1) % 24;
			}
		}
	}
}

/* NOT original: osdbits has no RTC, so seed the clock from argv
 * (menu <hour> <minute> <second> <framelimit>) or, with no args, from
 * the cycle counter so repeated runs differ. */
static void
InitClock(void)
{
	u32 cycles;
	int t;

	asm volatile ("mfc0 %0, $9" : "=r"(cycles));
	t = OsdArgInt(0, -1);
	clockHour = t >= 0 ? t % 24 : (int)((cycles>>20) % 24);
	t = OsdArgInt(1, -1);
	clockMin = t >= 0 ? t % 60 : (int)((cycles>>10) % 60);
	t = OsdArgInt(2, -1);
	clockSec = t >= 0 ? t % 60 : (int)(cycles % 60);
	clockMs = 0.0f;
	hwFrameLimit = OsdArgInt(3, -1);
	printf("osdsys: menu clock %02d:%02d:%02d\n", clockHour, clockMin, clockSec);
}

/* ===================== matrix helpers, in plain C =====================
 *
 * These duplicate sceVu0MulMatrix / sceVu0RotMatrix{X,Y,Z} /
 * sceVu0CameraMatrix / sceVu0ViewScreenMatrix / sceVu0Normalize rather
 * than calling them, because **freesce's libvu0 versions of exactly
 * those functions are broken**: the hand-written assembly lost the
 * instruction out of several branch delay slots.  The worst is
 * sceVu0MulMatrix, whose loop lost its `addi a0,a0,16`, so all four
 * result columns land on column 0 (checked against the retail image's
 * copy at 0x267860 - the ROM has that addi in the `bne` delay slot).
 * sceVu0RotMatrix{X,Y,Z} have the same loop bug plus an unreachable
 * `li a3,1` (the negative-angle flag); sceVu0ViewScreenMatrix writes
 * its zmin/zmax term after the multiply instead of before.
 * sceVu0ApplyMatrix, UnitMatrix, CopyMatrix, TransMatrix, ScaleVector,
 * OuterProduct and InversMatrix ARE byte-identical to the ROM.
 *
 * Layout, as for sceVu0FMATRIX: m[col][row], so m[3] is the translation
 * and (M v)[r] = sum_c m[c][r]*v[c]. */

void
matUnit(sceVu0FMATRIX m)
{
	int c, r;

	for(c = 0; c < 4; c++)
		for(r = 0; r < 4; r++)
			m[c][r] = c == r ? 1.0f : 0.0f;
}

void
matCopy(sceVu0FMATRIX d, sceVu0FMATRIX s)
{
	memcpy(d, s, sizeof(sceVu0FMATRIX));
}

/* d = a x b, the sceVu0MulMatrix(d, a, b) argument order */
void
matMul(sceVu0FMATRIX d, sceVu0FMATRIX a, sceVu0FMATRIX b)
{
	sceVu0FMATRIX t;
	int c, r;

	for(c = 0; c < 4; c++)
		for(r = 0; r < 4; r++)
			t[c][r] = a[0][r]*b[c][0] + a[1][r]*b[c][1] +
			          a[2][r]*b[c][2] + a[3][r]*b[c][3];
	matCopy(d, t);
}

/* o = m x v, the sceVu0ApplyMatrix(o, m, v) argument order */
void
matApply(sceVu0FVECTOR o, sceVu0FMATRIX m, sceVu0FVECTOR v)
{
	sceVu0FVECTOR t;
	int r;

	for(r = 0; r < 4; r++)
		t[r] = m[0][r]*v[0] + m[1][r]*v[1] + m[2][r]*v[2] + m[3][r]*v[3];
	for(r = 0; r < 4; r++)
		o[r] = t[r];
}

/* the three rotation matrices, with the signs the ROM's own mdRotX
 * (0x230198) builds by hand and the library's vf6..vf9 columns agree on */
static void
matRotX(sceVu0FMATRIX m, float a)
{
	float c = cosf(a), s = sinf(a);

	matUnit(m);
	m[1][1] = c;  m[1][2] = s;
	m[2][1] = -s; m[2][2] = c;
}

static void
matRotY(sceVu0FMATRIX m, float a)
{
	float c = cosf(a), s = sinf(a);

	matUnit(m);
	m[0][0] = c;  m[0][2] = -s;
	m[2][0] = s;  m[2][2] = c;
}

static void
matRotZ(sceVu0FMATRIX m, float a)
{
	float c = cosf(a), s = sinf(a);

	matUnit(m);
	m[0][0] = c;  m[0][1] = s;
	m[1][0] = -s; m[1][1] = c;
}

static void
vecCross(sceVu0FVECTOR o, sceVu0FVECTOR a, sceVu0FVECTOR b)
{
	float x, y, z;

	x = a[1]*b[2] - a[2]*b[1];
	y = a[2]*b[0] - a[0]*b[2];
	z = a[0]*b[1] - a[1]*b[0];
	o[0] = x; o[1] = y; o[2] = z; o[3] = 0.0f;
}

static void
vecNormalize(sceVu0FVECTOR o, sceVu0FVECTOR v)
{
	float d = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

	if(d != 0.0f)
		d = 1.0f/d;
	o[0] = v[0]*d; o[1] = v[1]*d; o[2] = v[2]*d; o[3] = v[3];
}

/* real: sceVu0CameraMatrix 0x267298 - basis x = normalize(yd x zd),
 * z = normalize(zd), y = z x x, then invert (world -> camera).  zd is a
 * DIRECTION, not a look-at point. */
static void
matCamera(sceVu0FMATRIX m, sceVu0FVECTOR p, sceVu0FVECTOR zd, sceVu0FVECTOR yd)
{
	sceVu0FVECTOR bx, by, bz, t;

	vecCross(t, yd, zd);
	vecNormalize(bx, t);
	vecNormalize(bz, zd);
	vecCross(by, bz, bx);

	m[0][0] = bx[0]; m[1][0] = bx[1]; m[2][0] = bx[2]; m[3][0] = 0.0f;
	m[0][1] = by[0]; m[1][1] = by[1]; m[2][1] = by[2]; m[3][1] = 0.0f;
	m[0][2] = bz[0]; m[1][2] = bz[1]; m[2][2] = bz[2]; m[3][2] = 0.0f;
	m[0][3] = m[1][3] = m[2][3] = 0.0f; m[3][3] = 1.0f;

	/* the inverse's translation: -basis(transposed) x p */
	m[3][0] = -(bx[0]*p[0] + bx[1]*p[1] + bx[2]*p[2]);
	m[3][1] = -(by[0]*p[0] + by[1]*p[1] + by[2]*p[2]);
	m[3][2] = -(bz[0]*p[0] + bz[1]*p[1] + bz[2]*p[2]);
}

/* real: sceVu0ViewScreenMatrix 0x267068 - the product of a "scale x/y by
 * scrz, put z into w" matrix and a "scale by the aspect, bias by the
 * screen centre, remap z" one.  After the w divide:
 *   X = ax*scrz*x/z + cx,  Y = ay*scrz*y/z + cy,  Z = A + B/z */
static void
matViewScreen(sceVu0FMATRIX m, float scrz, float ax, float ay,
	float cx, float cy, float zmin, float zmax, float nearz, float farz)
{
	float a, b;

	a = (zmin*farz - zmax*nearz) / (farz - nearz);
	b = farz*nearz*(zmax - zmin) / (farz - nearz);

	memset(m, 0, sizeof(sceVu0FMATRIX));
	m[0][0] = ax*scrz;
	m[1][1] = ay*scrz;
	m[2][0] = cx;
	m[2][1] = cy;
	m[2][2] = a;
	m[2][3] = 1.0f;
	m[3][2] = b;
}

/* ============ the MatrixDrive transform stack (0x230000) ============
 *
 * Module U's private matrix helper: a 16-deep stack at 0x368200 with a
 * quarter-wave sine table at 0x3581F0 (mdSin 0x230018, a full circle =
 * 0x10000).  The scene never pushes, so one matrix is enough here; the
 * angles stay in the ROM's 16-bit units so the constants below read the
 * same as the disassembly.
 *
 * mdRotX/Y/Z (0x230198/0x230260/0x230328) POST-multiply: the ROM does
 * sceVu0MulMatrix(top, top, R), i.e. top = top x R.  (The camera builder
 * 0x22ED20 pre-multiplies instead - see MenuCameraMatrix.) */

sceVu0FMATRIX mdTop;

#define MDANGLE(a) ((a) * (TAU/65536.0f))

void
mdRotX(int a)
{
	sceVu0FMATRIX r;
	matRotX(r, MDANGLE((short)a));
	matMul(mdTop, mdTop, r);
}

void
mdRotY(int a)
{
	sceVu0FMATRIX r;
	matRotY(r, MDANGLE((short)a));
	matMul(mdTop, mdTop, r);
}

void
mdRotZ(int a)
{
	sceVu0FMATRIX r;
	matRotZ(r, MDANGLE((short)a));
	matMul(mdTop, mdTop, r);
}

/* real: mdTranslate 0x2303E8 - the top matrix's translation ROW becomes
 * ApplyMatrix(top, v), i.e. translate in the matrix's own frame,
 * accumulating onto the existing translation.
 *
 * All FOUR components are written back (`sq' of the whole qword at
 * 0x230428), and the caller's vector supplies the fourth: 0x230440
 * (mdTranslatef) builds {x, y, z, **1.0**} on its stack, but 0x226D00
 * calls 0x2303E8 DIRECTLY with a pointer into the cube table at 0x27F090,
 * whose five position vectors all carry **w = 0** (res/MENUGEOM.inc has
 * them, and the port used to throw the w away by going through the
 * three-float form).
 *
 * That zero is the whole cube-stage depth story.  With m[3][3] = 0 the
 * camera-space w of every cube vertex comes out 0, so 0x22CFA8's second
 * transform gives proj.z = viewscreen[2][2]*z_cam + viewscreen[3][2]*0 and
 * proj.w = z_cam, and the perspective divide leaves proj.z = the CONSTANT
 * viewscreen[2][2] = (zmin*farz - zmax*nearz)/(farz - nearz) = -255.0039
 * for every vertex of every cube.  sceVu0FTOI4 turns that into -4080 =
 * 0xFFFFF010, which is what the retail GS packets carry (the GS's 24-bit
 * view of it, 0xFFF010 = 16773136, is what a dump decoder prints).
 *
 * Verified against the live retail face bank at 0x3529D0 in savestate
 * `20020207-164243 (00000000).04.p2s': every vertex has cam.w = 0,
 * proj = (x, y, -255.0039, 1.0) and fixed z = -4080, while the camera-space
 * z legitimately varies 44.3..50.7.  x and y are untouched by the w
 * (viewscreen[3][0] = viewscreen[3][1] = 0), which is exactly why the
 * cubes land in the right place in this port but at the wrong depth. */
void
mdTranslate(const float *v4)
{
	sceVu0FVECTOR v, o;

	v[0] = v4[0]; v[1] = v4[1]; v[2] = v4[2]; v[3] = v4[3];
	matApply(o, mdTop, v);
	mdTop[3][0] = o[0];
	mdTop[3][1] = o[1];
	mdTop[3][2] = o[2];
	mdTop[3][3] = o[3];
}

/* real: mdTranslatef 0x230440 - 0x2303E8 with w = 1 */
void
mdTranslatef(float x, float y, float z)
{
	sceVu0FVECTOR v;

	v[0] = x; v[1] = y; v[2] = z; v[3] = 1.0f;
	mdTranslate(v);
}

/* =================== the animation timer (0x22AC10) ===================
 * Module U's most-used abstraction; the orb renderer owns one instance
 * (0x27F900) that ramps the trail's overall brightness at scene entry. */

typedef struct Timer Timer;
struct Timer
{
	int duration, count, edge, state;
};

static Timer orbTrailTimer;	/* real: 0x27F900 */

static int
TimerInterp(Timer *t, int n)	/* real: 0x22AC20 */
{
	return t->duration ? t->count*n/t->duration : 0;
}

static void
TimerOpen(Timer *t)		/* real: 0x22AC70 */
{
	if(t->state == 0) {
		t->count = 0;
		t->state = 1;
	}
}

static void
TimerStep(Timer *t)		/* real: 0x22ACC0 */
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

/* ==================== the screen fade (0x22ADD8) ====================
 *
 * `mode` (gp-28828, read back by 0x22AD30) and `alpha` (gp-28824, read
 * by 0x22AD28) are the module's whole-screen fade.  0x22ADD8 arms it,
 * 0x22B058 runs the counter, 0x22AFB8 paints the curtain at
 * A = 128 - alpha.  Modes: 1 = white flash, 2 = fade UP from black
 * (alpha 0 -> 128, then mode clears to 0), 3 = fade DOWN to black
 * (alpha 128 -> 0, then the module leaves), 4 = instant black.
 *
 * This matters to the orbs because 0x226360 keys its entry animation on
 * the same two words: while mode is 2 the orbs are scattered across the
 * screen by their random phase and their colour is lerped from the
 * per-orb table to the shared base, both driven by `alpha`.  So the
 * fade-up IS the menu's entry animation, not a separate transition.
 * The module's one-shot init (0x21CE58) ends with 0x22ADD8(2), so it
 * runs on every entry - which is why menuFade{Mode,Alpha} start armed. */
static int menuFadeMode;	/* real: *(gp-28828), 0x22AD30() */
static int menuFadeAlpha;	/* real: *(gp-28824), 0x22AD28() */
/* real 0x27F630, the curtain's colour record: 0x22ADD8 sets it black
 * for modes 2/3/4 and white for mode 1 */
static int menuFadeColor[3];

/* real: *(int*)0x1F05EC == 1, "the module we came from was the
 * opening".  It widens the fade-up scatter from orb 0 alone to all
 * seven - i.e. the full seven-orb fly-in only happens on the first
 * entry after the boot animation, which is the case osdbits models. */
static int menuFromOpening = 1;
static int menuDebug;		/* NOT original: orb geometry + frame dump */

/* real: 0x22ADD8 */
static void
FadeArm(int mode)
{
	menuFadeMode = mode;
	menuFadeColor[0] = menuFadeColor[1] = menuFadeColor[2] = mode == 1 ? 255 : 0;
	menuFadeAlpha = mode == 3 ? 128 : 0;
}

/* real: the counter half of 0x22B058 */
static void
FadeStep(void)
{
	if(menuFadeMode <= 0)
		return;
	if(menuFadeMode < 3) {
		if(++menuFadeAlpha > 128) {
			menuFadeAlpha = 128;
			menuFadeMode = 0;
		}
	} else if(menuFadeMode == 3) {
		if(--menuFadeAlpha < 0)
			menuFadeAlpha = 0;
	}
}

/* =================== scene data (Module U scratch) =================== */

/* real 0x27B460 / 0x27B470 / 0x27B480 / 0x27B490 - the camera's rest
 * position, forward and up directions, and its fixed Euler angles */
static sceVu0FVECTOR menuCamPos = { 10.436f, 0.0f, -103.0f, 0.0f };
static sceVu0FVECTOR menuCamFwd = { 0.0f, 0.0f, 1.0f, 1.0f };
static sceVu0FVECTOR menuCamUp  = { 0.0f, 1.0f, 0.0f, 1.0f };
static sceVu0FVECTOR menuCamRot = { 0.031f, 0.145f, 0.0f, 0.0f };
static sceVu0FVECTOR menuOrigin = { 0.0f, 0.0f, 0.0f, 1.0f };

/* real *(gp-28880) = 0x2A7FA0: the scene's fly-in - the camera starts
 * 100 units further back (set by 0x21CE58's tail) and eases to its rest
 * position by *(gp-32224) = 0.97 per frame (0x21CFD8's tail) */
static float menuCamZOffset;

/* real *(0x27B440), set to 1.0 by 0x21CE40 - a global size multiplier
 * for the orbit radius */
static float menuScale;

/* real *(0x27B44C) / *(0x27B450), set by 0x21C9D0 from IsPAL: the
 * projection's aspect factors.  0.47 is the 640x224 NTSC field's pixel
 * aspect - note the opening uses 0.457627 for the same thing. */
static float menuScreenAX, menuScreenAY;

/* real *(gp-28848) = 0x2A7FC0 and *(gp-28852) = 0x2A7FBC: the orbit
 * radius eases from 10 to 17.25 at *(gp-32152) = 0.005 per frame.  The
 * ease TARGET is 1 - orbEaseOut, so the browser transition can pull the
 * cloud back in by driving orbEaseOut up. */
static float orbEase;
static float orbEaseOut;

/* real *(gp-28854) = 0x2A7FBA and *(gp-28856) = 0x2A7FB8, seeded from
 * the clock by 0x225420: the cloud's fixed tilt (hour hand, a full turn
 * per 12 hours) and spin (second hand at scene entry) */
static short orbTiltZ;
static short orbSpinY;

/* real 0x34E960: a random 16-bit phase per orb, from the ROM rand() at
 * 0x25B478 (0x225998) - the direction each orb flies off in during the
 * browser transition */
static int orbPhase[NUMORBS];

/* real 0x27EB30: the colour every orb has in the idle menu (a dim blue,
 * alpha 60), and real 0x27EB40: the per-orb colours.  0x226360's idle
 * path multiplies the per-orb table by 0 and the base by 128, so all
 * seven orbs match; only during the browser transition does the lerp
 * run, per-orb -> base as its counter goes 0 -> 128. */
static int orbBaseColor[4] = { 0x30, 0x62, 0x80, 0x3C };
static int orbColor[NUMORBS][4] = {
	{ 0x00, 0x00, 0x80, 0x3C },
	{ 0x00, 0x80, 0x00, 0x3C },
	{ 0x00, 0x80, 0x80, 0x3C },
	{ 0x80, 0x00, 0x00, 0x3C },
	{ 0x80, 0x00, 0x44, 0x3C },
	{ 0x80, 0x44, 0x00, 0x3C },
	{ 0x80, 0x80, 0x80, 0x3C },
};
/* real 0x27F930: the core sprite's constant colour */
static int orbCoreColor[4] = { 0x80, 0x80, 0x80, 0x80 };

/* the two matrices 0x21CF20 builds on its stack and hands to every
 * stage; the scene struct at 0x27E950 keeps pointers to them at +0x60
 * (viewscreen) and +0x64 (camera) */
sceVu0FMATRIX menuCamera;
sceVu0FMATRIX menuViewScreen;
/* real 0x27E970 = the scene struct's world matrix (scene+0x20) */
static sceVu0FMATRIX menuWorld;

/* ================= the per-orb trail buffer (0x27F950) =================
 *
 * Ten 1616-byte structs, seven used.  0x22FEC0 appends the orb's
 * projected screen position and colour to a 50-entry ring, but only
 * advances the write head every third call - so the trail spans 150
 * frames (2.5 s).  0x22FE88 zeroes the whole array at init. */

typedef struct TrailPoint TrailPoint;
struct TrailPoint
{
	sceVu0FVECTOR pos;	/* screen x, y (centre-relative) and the
				 * projected depth used for the size */
	int col[4];
};

typedef struct Orb Orb;
struct Orb
{
	int head;		/* +0x00 write index */
	int sub;		/* +0x04 0..2 subframe counter */
	int wrapped;		/* +0x08 set once the ring has filled */
	int pad;
	TrailPoint trail[TRAILLEN];	/* +0x10, stride 32 */
};

static Orb orbs[NUMORBS];

/* real: 0x22FEC0 */
static void
PushTrail(int i, float *pos, int *col)
{
	Orb *o = &orbs[i];
	int k;

	for(k = 0; k < 4; k++) {
		o->trail[o->head].pos[k] = pos[k];
		o->trail[o->head].col[k] = col[k];
	}
	if(++o->sub != 3)
		return;
	o->sub = 0;
	if(++o->head == TRAILLEN) {
		o->head = 0;
		o->wrapped = 1;
	}
	/* the new head starts as a copy of the old one, so a fresh slot is
	 * never read as garbage */
	for(k = 0; k < 4; k++) {
		o->trail[o->head].pos[k] = pos[k];
		o->trail[o->head].col[k] = col[k];
	}
}

/* ================ the deferred, depth-sorted draw list ================
 *
 * real: the dummy head at 0x34E980 with 320-byte records after it,
 * singly linked through +0, insertion-sorted ascending by the view
 * depth at +4 (0x225D18).  Type 1 is an orb (0x225ED0); type 0 is one
 * of the twelve System Configuration fly-in rods (0x225DD8), drawn by
 * menuconfig.c.  The record layout is in inc.h. */

#define MAXRECS 32

static SceneRec sceneRecs[MAXRECS];
static SceneRec sceneHead;
static int sceneNumRecs;

/* real: 0x225CF0 */
static void
SceneReset(void)
{
	sceneHead.next = nil;
	sceneHead.key = 1.0e24f;	/* real *(gp-32160) */
	sceneHead.type = 0;
	sceneHead.f12 = -1.0f;
	sceneNumRecs = 0;
}

/* real: 0x225D18 - the sort key is the object's Z in camera space */
static float
SceneDepth(sceVu0FMATRIX world)
{
	sceVu0FMATRIX m;

	matMul(m, menuCamera, world);
	return m[3][2];
}

/* real: 0x225D40 - insertion sort into the singly linked list.  The
 * comparison walks on while the new key is SMALLER than the node's, so
 * the list comes out descending by camera-space Z: farthest first. */
static void
SceneInsert(SceneRec *rec)
{
	SceneRec *p;

	for(p = &sceneHead; p->next; p = p->next)
		if(rec->key >= p->next->key)
			break;
	rec->next = p->next;
	p->next = rec;
}

/* real: 0x225ED0 - append an orb record */
static void
SceneAddOrb(int i)
{
	SceneRec *rec;

	if(sceneNumRecs >= MAXRECS)
		return;
	rec = &sceneRecs[sceneNumRecs++];
	rec->key = SceneDepth(menuWorld);
	matCopy(rec->world, menuWorld);
	rec->type = 1;
	rec->f12 = 0.0f;
	rec->index = i;
	SceneInsert(rec);
}

/* real: 0x225DD8 - append a fly-in mesh record.  The ROM copies the
 * whole 224-byte scene struct into the record (so the world matrix lands
 * at +0x30) and writes the two colour qwords at +0x100 and +0x120, the
 * split at +0xF4 and the front-object flag at +0x110; this carries the
 * same fields as named members. */
void
SceneAddMesh(sceVu0FMATRIX world, int slot, float progress, float size,
	float split, const int *col0, const int *col1,
	const int *colA, const int *colB, int aux)
{
	SceneRec *rec;
	int k;

	if(sceneNumRecs >= MAXRECS)
		return;
	rec = &sceneRecs[sceneNumRecs++];
	rec->key = SceneDepth(world);
	matCopy(rec->world, world);
	rec->type = 0;
	rec->f12 = split;
	rec->index = slot;
	rec->progress = progress;
	rec->size = size;
	rec->aux = aux;
	for(k = 0; k < 4; k++) {
		rec->col0[k] = col0[k];
		rec->col1[k] = col1[k];
		rec->colA[k] = colA[k];
		rec->colB[k] = colB[k];
	}
	SceneInsert(rec);
}

/* ======================= camera and projection ======================= */

/* real: 0x22ED20 - build a camera matrix from a position, a forward and
 * an up direction, all three rotated by the same fixed Euler triple.
 * sceVu0CameraMatrix's zd argument is a DIRECTION (0x267298 normalises
 * it and crosses it with yd), not a look-at point. */
static void
MenuCameraMatrix(sceVu0FMATRIX out, sceVu0FVECTOR pos, sceVu0FVECTOR fwd,
	sceVu0FVECTOR up, sceVu0FVECTOR rot)
{
	sceVu0FMATRIX m, r;
	sceVu0FVECTOR p, zd, yd;

	matUnit(m);
	matRotX(r, rot[0]); matMul(m, r, m);
	matRotY(r, rot[1]); matMul(m, r, m);
	matRotZ(r, rot[2]); matMul(m, r, m);
	matApply(zd, m, fwd);
	matApply(yd, m, up);
	matApply(p, m, pos);
	matCamera(out, p, zd, yd);
}

/* real: 0x21CFD8 - stage 1 of the frame body.  The projection is
 * rebuilt every frame even though only the aspect can change; the
 * camera's Z offset decays towards 0, which is the scene's fly-in. */
static void
MenuCamera(void)
{
	sceVu0FVECTOR pos;

	matViewScreen(menuViewScreen, 512.0f, menuScreenAX, menuScreenAY,
		2048.0f, 2048.0f, 1.0f, 16777215.0f, 1.0f, 65536.0f);

	pos[0] = menuCamPos[0];
	pos[1] = menuCamPos[1];
	pos[2] = menuCamPos[2] + menuCamZOffset;
	pos[3] = menuCamPos[3];
	MenuCameraMatrix(menuCamera, pos, menuCamFwd, menuCamUp, menuCamRot);

	menuCamZOffset *= 0.97f;	/* real *(gp-32224) */
}

/* real: 0x22CFA8 (through the 0x22D298 wrapper) - project the object's
 * origin.  out[0]/out[1] are screen pixels relative to the screen
 * centre; out[2] is the perspective depth AFTER the w divide, which is
 * what the sprite size and the trail's GS Z are scaled from. */
static void
ProjectOrigin(float *out, sceVu0FMATRIX world)
{
	sceVu0FMATRIX m;
	sceVu0FVECTOR a, b;
	float q;

	matMul(m, menuCamera, world);
	matApply(a, m, menuOrigin);
	matApply(b, menuViewScreen, a);
	q = 1.0f/b[3];
	b[0] *= q; b[1] *= q; b[2] *= q; b[3] = 1.0f;
	out[0] = b[0] - 2048.0f;
	out[1] = b[1] - 2048.0f;
	out[2] = b[2];
	out[3] = 1.0f;
}

/* ========================= the orbit (0x2261B8) =========================
 *
 * Seven orbs on one ring, spun by the clock:
 *   rateX = seconds * 65536/60   - a full turn per minute
 *   rateY = minutes * 65536/60   - a full turn per hour
 * Each orb takes a different multiple of rateX (i+21, i.e. 21..27), so
 * they shear apart along the ring instead of moving as a rigid body;
 * rateY x 1100 tumbles the whole ring about once every 3.3 s.
 *
 * The chain per orb, in the ROM's exact order:
 *   identity, RotZ(hour tilt), RotY(entry spin), RotZ(180 degrees),
 *   RotY(rateY*1100), RotX((i+21)*rateX), translate +Y by the radius,
 *   RotY(90 degrees). */
static void
UpdateOrbs(void)
{
	float rateX, rateY, t, radius;
	int i;

	rateX = ClockSeconds() * (65536.0f/60.0f);
	rateY = ClockMinutes() * (65536.0f/60.0f);

	/* real: 0x225628 (stage 10, via 0x225BF8) rewrites this EVERY frame,
	 * so the radius ease target 1-orbEaseOut is the minute hand: the ring
	 * grows over the hour and snaps back in on the hour.  (0x225628 also
	 * re-eases orbTiltZ/orbSpinY toward the live clock at *(gp-32168),
	 * snapping tiltZ when |spinY| < 201 - not ported yet, the seed-once
	 * approximation only lags on runs crossing a minute boundary.) */
	orbEaseOut = 1.0f - ClockMinutes()*(1.0f/60.0f);

	t = orbEase;
	radius = (t*7.25f + 10.0f) * menuScale;
	orbEase = t + ((1.0f - orbEaseOut) - t)*0.005f;

	for(i = 0; i < NUMORBS; i++) {
		matUnit(mdTop);
		mdRotZ(orbTiltZ);
		mdRotY(orbSpinY);
		mdRotZ(-32768);
		mdRotY((int)(rateY*1100.0f));	/* real *(gp-32156) = 1100.0 */
		mdRotX((int)((i+21)*rateX));
		mdTranslatef(0.0f, radius, 0.0f);
		mdRotY(8192);
		matCopy(menuWorld, mdTop);
		SceneAddOrb(i);
	}
}

/* ======================= the orb renderer (0x22EFF0) =======================
 *
 * Three primitives per orb, all additive:
 *   1. the trail - a 49-vertex line strip back through the ring, whose
 *      colour decays as (age^4, age^2, age) so it reddens out first and
 *      the tail fades to blue;
 *   2. the halo  - a TEXCBLUR sprite 30 world units across;
 *   3. the core  - a TEXCNAVI sprite 4.5 units across.
 * Sizes are the projected depth times *(gp-31992) = 6.5e-06, which for
 * the menu's camera distance works out at roughly 63x32 px for the halo
 * and 9x5 px for the core. */

enum { MTEX_BLUR, MTEX_NAVI, NUMMENUTEXTURES };

static Texture menuTextures[NUMMENUTEXTURES] = {
	/* both TEXC slots decode 8-bit alpha to white-with-alpha (the real
	 * decoder 0x22A790, which is what opening.c calls format 3) */
	{ nil, RESID_TEXCBLUR, 0, 0, { 0, 0, 64, 64 }, 0, 0, 3, 0, { 0 } },
	{ nil, RESID_TEXCNAVI, 0, 0, { 0, 0, 64, 64 }, 0, 0, 3, 0, { 0 } },
};

/* the whole 64x64 page, in 1/16 texel - real: the static record at
 * 0x27F8C0, whose UVs 0x22EFF0 never touches */
#define ORBUV0 0
#define ORBUV1 (63*16)

/* real: the sprite half of 0x22EFF0.  The +2048 puts the vertex in the
 * GS's primitive space, from which XYOFFSET_1 = (2048 - w/2) is
 * subtracted again - the same convention opening.c's pktSetFlatRect
 * uses.  The vertical half-size is halved because one NTSC field line
 * covers two source rows. */
static void
DrawOrbSprite(Orb *o, float half, int *col)
{
	float *p = o->trail[o->head].pos;
	int x0, y0, x1, y1;

	x0 = (int)((p[0] - half + 2048.0f)*16.0f);
	y0 = (int)((p[1] - half*0.5f + 2048.0f)*16.0f);
	x1 = (int)((p[0] + half + 2048.0f)*16.0f);
	y1 = (int)((p[1] + half*0.5f + 2048.0f)*16.0f);

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col[0], col[1], col[2], col[3], 0x3f800000));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(ORBUV0, ORBUV0));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(x0, y0, 0));
	pktSetAD(SCE_GS_UV, SCE_GS_SET_UV(ORBUV1, ORBUV1));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ(x1, y1, 0));
	vif1End();
}

/* one trail vertex: fade factor f (0..128) applied to the stored colour
 * with the ROM's asymmetric powers, then the position in 4.4 fixed */
static void
TrailVertex(TrailPoint *tp, int f)
{
	int r, g, b, a;
	int x, y, z;

	b = tp->col[2]*f >> 7;
	g = tp->col[1]*f*f >> 14;
	r = ((((tp->col[0]*f*f >> 7)*f >> 7)*f) >> 14);
	a = f >> 1;

	x = (int)((tp->pos[0] + 2048.0f)*16.0f);
	y = (int)((tp->pos[1] + 2048.0f)*16.0f);
	z = (int)(tp->pos[2]*16.0f);

	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(r, g, b, a, 0x3f800000));
	pktSetAD(SCE_GS_XYZF2, SCE_GS_SET_XYZF(x, y, z, 0));
}

/* real: the two identical trail loops of 0x22EFF0 (the ROM runs the
 * whole orb twice, once into the visible buffer and once into the
 * offscreen buffer the zoom-blur composite samples; only the visible
 * pass is ported).
 *
 * The ROM's PRIM is 0x82 = LINE_STRIP with AA1 and ABE off, exactly the
 * "weird setting" opening.c's DrawLights uses for its light trails: the
 * antialiasing path is what actually blends the line. */
static void
DrawOrbTrail(Orb *o, int baseAlpha)
{
	int k, f, idx, n;

	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_LINESTRIP, 0, 0, 0, 0, 1, 0, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(0, 0, 0, baseAlpha, 0x3f800000));

	if(o->wrapped) {
		/* the full ring: three frames of age per sample, so the trail
		 * is spent (f == 0) by sample 43 of 49 */
		for(k = 0; k < TRAILDRAW; k++) {
			f = 128 - k*3;
			if(f < 0)
				f = 0;
			idx = (o->head + 100 - k) % TRAILLEN;
			TrailVertex(&o->trail[idx], f);
		}
	} else {
		/* still filling: stretch the same fade over however many
		 * samples exist, so a young trail is not just a stub */
		n = o->head - 1;
		for(k = 0; k < n; k++) {
			f = 128 - (k*TRAILLEN/n)*3;
			if(f < 0)
				f = 0;
			TrailVertex(&o->trail[o->head - k], f);
		}
	}

	vif1End();
}

/* real: 0x27F940 - the SECOND walk's core colour, where the first walk's
 * is 0x27F930 = orbCoreColor above.  The two records differ only in the
 * RGB: the copy that goes into work buffer 3 is pure white. */
static int orbCoreColorWork[4] = { 0xFF, 0xFF, 0xFF, 0x80 };

/* one walk of 0x22EFF0's three primitives.  `blurCol'/`coreCol' are the
 * colour records the walk uses (see DrawOrb). */
static void
DrawOrbPass(Orb *o, float zscale, int baseAlpha, int *blurCol, int *coreCol)
{
	/* real: 0x22A0C0(0, 3) - additive, depth GREATER.  osdbits draws
	 * the whole scene with the depth test off (as opening.c's DrawLights
	 * does); with nothing but additive primitives in the scene the
	 * result is identical. */
	vif1SetZWrite(0);
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 5, 128);
	DrawOrbTrail(o, baseAlpha);

	/* real: 0x22AB90(7, 1, 1) then 0x22AB90(6, 1, 1) */
	vif1SetTexture(&menuTextures[MTEX_BLUR]);
	vif1SetAlphaBlend(1, 5, 128);
	DrawOrbSprite(o, zscale*30.0f, blurCol);

	vif1SetTexture(&menuTextures[MTEX_NAVI]);
	vif1SetAlphaBlend(1, 5, 128);
	DrawOrbSprite(o, zscale*4.5f, coreCol);
}

/* real: 0x22EFF0.
 *
 * 0x22EFF0 draws EVERY orb TWICE - once on the visible buffer and once,
 * identically placed, into work buffer 3 - and the port only had the first
 * walk.  Confirmed in the GS dumps: retail's per-orb pattern is
 * `LINESTRIP+AA1 x1 + SPRITE+TME+ABE+FST x2' to FB = 0/2240 immediately
 * followed by the same three to FB = 6720, interleaved per orb with the
 * rods (retail614.log L7850/L7882 and their 6 repeats).  Work buffer 3 is
 * what the frame-start tint, the rods' pass 1/5 and the whole cube chain
 * sample, so without the twin everything that refracts the scene refracted
 * a version of it with no orbs in it - which is why an orb passing behind
 * the clock or a cube showed nothing.
 *
 * The two walks are not quite identical, and the difference is in the two
 * sprite colour records (measured in the dump, then read out of the ROM):
 *
 *   walk 1  halo = the orb's own trail colour {0x30,0x62,0x80,0x3C}
 *           core = *0x27F930 = {0x80,0x80,0x80,0x80}
 *   walk 2  halo = the same RGB with **alpha 0x80**
 *           core = *0x27F940 = {0xFF,0xFF,0xFF,0x80}
 *
 * The halo's alpha is not a separate record: 0x22EFF0 keeps the colour on
 * its own stack (sp+48) and the sprite code overwrites the qword's fourth
 * word with `TimerInterp(orbTrailTimer, <that word>)' every time it draws
 * (22f54c/22f558) - so walk 1 feeds it the orb's 0x3C and walk 2's head
 * (22f788/22f794: `li v1,128; sw v1,60(sp)') resets it to 128 first.  With
 * the trail timer open both come out unchanged, i.e. exactly `baseAlpha'. */
static void
DrawOrb(Orb *o)
{
	float zscale;
	int baseAlpha, k;
	int blurCol[4];

	TimerStep(&orbTrailTimer);
	zscale = o->trail[o->head].pos[2] * 6.5e-06f;	/* real *(gp-31992) */
	baseAlpha = TimerInterp(&orbTrailTimer, 128);

	/* real: 0x22F0CC, the FIRST thing 0x22EFF0 does -
	 * 0x22A3B8(0x1F0A10, *(0x1F0C40), 0, *(0x27B448)), i.e. aim FRAME at
	 * the visible buffer with the module's own field.  It looks
	 * redundant on the main menu, where nothing else moves FRAME, and
	 * that is exactly why it is here: 0x226700's sorted walk interleaves
	 * orbs and carousel rods, and 0x22D920 leaves FRAME on work buffer 3
	 * (its pass 5) without restoring it - and so, now, does this
	 * function's own second walk.  Every orb that sorts after either
	 * would be drawn into a work buffer if it did not re-aim first. */
	MenuBackScreenTarget(MenuBackField());
	DrawOrbPass(o, zscale, baseAlpha, o->trail[o->head].col, orbCoreColor);

	/* real: 0x22F798 - 0x22A4C8(0, NULL, *(0x27B448)): FRAME = work
	 * buffer 3, no clear, the module's own field (the same half pixel the
	 * screen walk carries - this is a mesh-style draw, not a blit). */
	for(k = 0; k < 3; k++)
		blurCol[k] = o->trail[o->head].col[k];
	blurCol[3] = baseAlpha;
	MenuBackWorkTarget(0, 0, MenuBackField());
	DrawOrbPass(o, zscale, baseAlpha, blurCol, orbCoreColorWork);
}

/* real: 0x226360 - project the record, apply the entry animation's
 * screen scatter and colour lerp, push the result onto the trail, draw. */
static void
DrawOrbRecord(SceneRec *rec)
{
	float sp[4];
	int col[4];
	int i, k, c;

	i = rec->index;
	ProjectOrigin(sp, rec->world);

	/* the entry fly-in: while the screen is fading up (mode 2) each orb
	 * is thrown out along its random phase angle by half a screen,
	 * pulled back in as `1 - sin(alpha*128)` closes.  Only orb 0 does it
	 * when the menu is re-entered from another menu screen; all seven do
	 * when the previous module was the opening.  Mode 3 is the mirror
	 * image on the way out, orb 0 only. */
	if((menuFadeMode == 2 && (menuFromOpening || i == 0)) ||
	   (menuFadeMode == 3 && i == 0)) {
		float amp, dx, dy;

		amp = 1.0f - sinf(MDANGLE((short)(menuFadeAlpha*128)));
		dx = (screenW/2) * cosf(MDANGLE((short)orbPhase[i])) * amp;
		dy = (screenH/2) * sinf(MDANGLE((short)orbPhase[i])) * amp;
		if(menuFadeMode == 2) {
			sp[0] += dx;
			sp[1] += dy;
		} else {
			float t = menuFadeAlpha * (1.0f/128.0f);
			sp[0] = sp[0]*t + dx*1.5f;
			sp[1] = sp[1]*t + dy*1.5f;
		}
	}

	/* colour: during a fade the orbs lerp from their individual colours
	 * to the shared base blue as alpha runs 0..128; with no fade running
	 * every orb just gets the base (real 0x226638's t1 = 0, t2 = 128). */
	if(menuFadeMode == 2 || menuFadeMode == 3) {
		c = menuFadeAlpha;
		for(k = 0; k < 4; k++)
			col[k] = (orbColor[i][k]*(128-c) + orbBaseColor[k]*c) >> 7;
	} else
		for(k = 0; k < 4; k++)
			col[k] = orbBaseColor[k];

	PushTrail(i, sp, col);
	if(menuDebug && frameCount <= 2)
		printf("orb %d: x %.1f y %.1f z %.1f  half %.2f  col %d %d %d %d\n",
			i, sp[0], sp[1], sp[2], sp[2]*6.5e-06f*30.0f,
			col[0], col[1], col[2], col[3]);
	DrawOrb(&orbs[i]);
}

/* real: 0x226700 - walk the sorted list.  0x2267E8's two-pass flush
 * (the mesh emitter plus the two full-screen composite quads) is
 * deferred with the backdrop mesh. */
static void
SceneWalk(void)
{
	SceneRec *rec;

	for(rec = sceneHead.next; rec; rec = rec->next)
		if(rec->type == 1)
			DrawOrbRecord(rec);
		else
			MenuConfigDrawMesh(rec);	/* real: 0x2266E0 -> 0x22D920 */
}

/* ============================== init ============================== */

/* real: the rand() at 0x25B478, the same LCG opening.c's osdRand()
 * wraps (Module U calls it directly from 0x225998).  The real stream is
 * whatever the OSD has advanced it to by the time the menu starts, so
 * the phases here are self-consistent but not the console's - the same
 * caveat opening.c documents for lightsSeed. */
static u32 menuRandSeed = 1;
static int
menuRand(void)
{
	menuRandSeed = menuRandSeed*1103515245 + 12345;
	return menuRandSeed & 0x7FFFFFFF;
}

/* real: 0x225420 - seed the cloud's orientation from the clock */
static void
InitOrbAngles(void)
{
	orbTiltZ = (short)(int)(ClockHours() * (65536.0f/12.0f));
	orbSpinY = (short)(int)(ClockSeconds() * (65536.0f/60.0f));
}

/* real: 0x225998's tail - a random 16-bit phase per orb */
static void
InitOrbPhases(void)
{
	int i;

	for(i = 0; i < NUMORBS; i++)
		orbPhase[i] = menuRand() % 65536;
}

/* real: 0x21CE58 - the module's one-shot init, minus the parts that
 * belong to the 2D/UI layer (config model, fonts, screen wizards) */
void
InitMenuScene(void)
{
	int i, fps;

	InitClock();

	menuFromOpening = OsdArgInt(4, 1);
	menuDebug = OsdArgInt(6, 0);

	matUnit(mdTop);

	/* real: 0x21C9D0's per-frame aspect setup (constant in practice) */
	menuScreenAX = 1.0f;
	menuScreenAY = IsPAL() ? 0.5405f : 0.47f;

	menuScale = 1.0f;		/* real: 0x21CE40 */
	menuCamZOffset = -100.0f;	/* real: 0x21CE58's tail */
	orbEase = 0.0f;
	orbEaseOut = 0.0f;

	for(i = 0; i < NUMMENUTEXTURES; i++)
		InitTexture(&menuTextures[i]);
	InitMenuBackdrop();	/* real: 0x229698/0x22A9B8(1) + 0x2287B0 */

	InitOrbAngles();
	InitOrbPhases();

	/* real: 0x22FE88 */
	memset(orbs, 0, sizeof(orbs));

	/* real: 0x22EE98 - the trail's brightness ramp, (fps<<8)/60 frames
	 * (256 on NTSC), stepped once per orb so it fills in ~37 frames */
	fps = IsPAL() ? 50 : 60;
	memset(&orbTrailTimer, 0, sizeof(orbTrailTimer));
	orbTrailTimer.duration = (fps<<8)/60;
	TimerOpen(&orbTrailTimer);

	/* real: 0x21CE58's last call, 0x22ADD8(2) - arm the fade up from
	 * black, which is also what drives the orbs' entry fly-in.  argv[5]
	 * can start it part-way (128 = skip the entry entirely). */
	FadeArm(2);
	menuFadeAlpha = clamp(OsdArgInt(5, 0), 0, 128);

	/* real: 0x21CE58 also runs do_load_font (0x21DBA0) and the
	 * per-screen init 0x228460 - menutext.c */
	InitMenuText();
	InitMenuConfig();

	SceneReset();
}

/* real: 0x22AFB8 - the whole-screen fade curtain, normal alpha blend,
 * A = 128 - alpha, from the record at 0x27F630 */
static void
DrawFadeCurtain(void)
{
	int a = 128 - menuFadeAlpha;

	if(a <= 0)
		return;
	vif1SetZWrite(0);
	vif1SetZTest(0);
	vif1SetAlphaBlend(1, 4, 128);	/* (Cs-Cd)*As + Cd */
	vif1Begin();
	pktSetAD(SCE_GS_PRIM, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 0, 0, 1, 0, 0, 0, 0));
	pktSetAD(SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(menuFadeColor[0], menuFadeColor[1],
		menuFadeColor[2], a, 0x3f800000));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ((2048-screenW/2)<<4, (2048-screenH/2)<<4, 0));
	pktSetAD(SCE_GS_XYZ2, SCE_GS_SET_XYZ((2048+screenW/2)<<4, (2048+screenH/2)<<4, 0));
	vif1End();
}

/* real: the scene-relevant subset of the frame body 0x21CF20 -
 *   0x21CFD8  camera + projection                        [ported]
 *   0x21D0A0  zoom-blur composite + full-screen blit     [deferred]
 *   0x2268F0  reset / carousel fly-in / orbit / draw / flush
 *   0x22B020  the fade curtain (0x22AFB8)                [ported]
 *   0x2283F0  per-screen UI init hub                     [not scene]
 *   0x21D368  letterbox, 0x21D3A0/0x21DA68/0x21DB18 UI   [not scene]
 *   0x225BF8  the fly-in carousel timer                  [ported]
 *   0x2285C0/0x2287D0/0x22B588/0x22BE30 UI               [not scene]
 *   0x22B058  the fade counter                           [ported]
 *   0x22BB30  clock tick                                 [ported]
 * The 0x2268F0 stage's carousel fly-in (0x226028) only fires while the
 * carousel's progress exceeds 0.05, and its timer (0x27EB00) is opened
 * only by "enter System Configuration", so on the main menu the whole
 * mesh half of the list is correctly inert. */
static void
MenuFrame(void)
{
	/* NOT original: the pad, read once per frame before anything looks
	 * at it (menutext.c's MainMenuInput is the only consumer so far).
	 * The real menu's buttons come in from the OSD system module. */
	UpdatePad();
	/* real: the CLAMP_1 = CLAMP/CLAMP the TEXC bind (0x22AB90 ->
	 * 0x262308) pushes with every texture; osdbits' vif1SetTexture
	 * leaves the GS default (REPEAT), which would wrap the halo's
	 * edge texels */
	vif1SetCLAMP_1(1, 1, 0, 0, 0, 0);
	/* NOT original: snapshot evenOddFrame once for the whole frame - both
	 * 0x21D0A0's stage and 0x2283D0's zoom blur address "the buffer being
	 * drawn", and the swap thread can flip the flag between them */
	MenuBackFrameStart();
	MenuCamera();
	/* real: 0x21D0A0 - the TEXCKABE backdrop tunnel plus the composite
	 * that tints the whole screen deep blue.  It has to run between the
	 * camera and the object list because its last act overwrites the
	 * frame buffer (menuback.c). */
	/* aap ground truth (real console): the TEXCKABE tunnel is visible
	 * ONLY in System Configuration.  The ROM's own per-screen signal for
	 * that is the backdrop fade timer 0x27F190, whose sole opener
	 * (0x2291E8) is called from one place in the whole image - 0x2272B8,
	 * inside "enter System Configuration" - and whose sole closer
	 * (0x229230) from that screen's leave path.  0x229358 does not itself
	 * gate the mesh on it (menuback.c's MenuBackdropVisible documents the
	 * divergence); argv[11] still forces it on. */
	if(MenuBackdropVisible() || OsdArgInt(11, 0))
		MenuBackdrop(menuCamera, menuViewScreen, menuFadeMode);
	SceneReset();
	/* real: 0x2268F0's head - the fly-in emitter runs before the orbit
	 * so the rods sort into the same list as the orbs */
	if(MenuConfigCarouselActive())
		MenuConfigEmit();	/* real: 0x226028 */
	UpdateOrbs();
	SceneWalk();
	/* real: 0x2267E8, the stage's own tail, pushes FRAME before its two
	 * composite quads - which is what puts the target back on the screen
	 * after 0x22D920's pass 5 and 0x22EFF0's second walk both leave it on
	 * work buffer 3.  That flush is still deferred here (see SceneWalk),
	 * so re-aim explicitly: ZoomBlur() would normally do it as a side
	 * effect, but not when argv[10] drops the phase to 5 and it runs zero
	 * passes, and menutext.c never pushes a FRAME of its own. */
	MenuBackScreenTarget(MenuBackField());
	/* real: 0x2283D0, the FIRST thing stage 5 (0x2283F0) does, before any
	 * of the 2D layer: 0x22C3C0(phase - 5), which is 0x22C3C0(5) in the
	 * idle menu.  Five bilinear shrink/stretch round trips over the whole
	 * frame buffer - this is why the retail orbs are so much softer than
	 * the port's, and why the text drawn after it stays crisp.
	 *
	 * The ROM has the fade curtain (0x22B020) before this and the text
	 * after; the port draws the curtain last.  That reordering is safe:
	 * the curtain is a uniform full-screen alpha blend and the blur is
	 * linear, so blur(lerp(scene, black, a)) == lerp(blur(scene), black,
	 * a) either way. */
	MenuZoomBlur();
	/* real: 0x226FA8, 0x2283F0's second slot - the five config cubes,
	 * drawn over the blurred scene and before any 2D layer */
	MenuConfigCubes();
	/* real: 0x227DE8, 0x2283F0's sixth slot - the System Configuration
	 * screen's own Anim and state machine */
	MenuConfigStep();
	/* real: 0x2283F0's main-menu slot (0x2283A0), which runs between
	 * the 3D list and the letterbox.  The fade words are 0x22AD30/
	 * 0x22AD28's job in the ROM; pass them instead of exporting them. */
	MenuTextFrame(menuFadeMode, menuFadeAlpha);
	DrawFadeCurtain();
	/* real: 0x225BF8, stage 10 - after the 2D layer, so the progress the
	 * emitter reads at stage 3 is always the previous frame's */
	MenuConfigCarousel();
	FadeStep();
	ClockTick();
}

/* NOT original: read the frame we just drew back out of GS memory and
 * print it as an 8x8-block luminance map.  PCSX2's screenshot hotkey
 * needs a window, so this is how a headless run can show what it drew. */
#define FBDUMP ((u128*)0x1000000)

static void
DumpFrameAscii(int par)
{
	static const char ramp[] = " .:-=+*#%@";
	sceGsStoreImage si;
	u32 *px;
	int x, y, i, j, l, best;
	char line[81];

	sceGsSyncPath(0, 0);
	sceGsSetDefStoreImage(&si, par == 0 ? 0 : (screenW*screenH)/64,
		screenW/64, SCE_GS_PSMCT32, 0, 0, screenW, screenH);
	FlushCache(0);
	sceGsExecStoreImage(&si, FBDUMP);
	sceGsSyncPath(0, 0);
	/* the store image reverses VIF1 for the download; whatever is left
	 * in the FIFO afterwards gets parsed as vifcodes by the next packet
	 * (PCSX2 logs "Unknown VifCmd" and libdma eventually hangs; real
	 * VIF1 would ER1-stall).  A full VIF1 reset clears it. */
	sceDevVif1Reset();

	px = UNCACHED(FBDUMP);
	printf("frame %d, %dx%d, 8x8 blocks (max luminance):\n", frameCount, screenW, screenH);
	for(y = 0; y+8 <= screenH; y += 8) {
		for(x = 0; x+8 <= screenW; x += 8) {
			best = 0;
			for(j = 0; j < 8; j++)
				for(i = 0; i < 8; i++) {
					u32 p = px[(y+j)*screenW + x+i];
					l = ((p&0xFF) + ((p>>8)&0xFF) + ((p>>16)&0xFF))/3;
					if(l > best)
						best = l;
				}
			line[x/8] = ramp[best*10/256];
		}
		line[x/8] = 0;
		printf("|%s|\n", line);
	}
}

/* real: 0x21CA38's inner frame loop (the screen-id dispatch that ends
 * it has no counterpart here - osdbits has no screens to leave to).
 *
 * NOT original: argv[12]/argv[13] are a headless stand-in for the pad,
 * for runs where no controller is attached.  The real path in is
 * MenuSelectItem(1) below. */
void
DoMenuScene(void)
{
	int par, enterFrame, leaveFrame;

	enterFrame = OsdArgInt(12, 0);
	leaveFrame = OsdArgInt(13, 0);

	for(;;) {
		if(enterFrame > 0 && frameCount == enterFrame)
			MenuEnterConfig();
		if(leaveFrame > 0 && frameCount == leaveFrame)
			MenuLeaveConfig();
		if(menuDebug && frameCount % 10 == 0)
			printf("menu frame %d (fade %d/%d)\n",
				frameCount, menuFadeMode, menuFadeAlpha);
		MenuFrame();
		/* the GS readbacks run AFTER WaitNextFrame: the swap thread
		 * kicks its own GIF writes at vsync, asynchronously, and one
		 * landing while the bus is reversed for the download splits a
		 * packet mid-stream (Unknown VifCmd spam, then a libdma hang).
		 * Right after the handshake the swap thread is quiet for a
		 * whole frame.  `par` is the parity of the buffer just drawn,
		 * captured before the swap flips it. */
		WaitNextFrame();
		/* the parity of the buffer just drawn.  Read AFTER the
		 * handshake: evenOddFrame is flipped by the swap thread, so a
		 * mid-frame read races it (the stableEvenOddFrame lesson from
		 * opening.c); the post-handshake value is the settled one and
		 * empirically selects the just-drawn buffer. */
		par = evenOddFrame;
		if((menuDebug && frameCount == menuDebug) ||
		   (MenuTextDumpFrame() > 0 && MenuTextDumpFrame() == frameCount)) {
			if(menuDebug && frameCount == menuDebug)
				DumpFrameAscii(par);
			if(MenuTextDumpFrame() > 0 && MenuTextDumpFrame() == frameCount)
				MenuTextDump(par);
			/* the reversed-bus download leaves VIF1/DMA desynced
			 * under PCSX2 (the downloaded data also runs through the
			 * vifcode parser; not even sceDevVif1Reset recovers it),
			 * so a readback ends the run.  Real hardware has dsedb's
			 * storeimage for this instead. */
			printf("readback done, exiting\n");
			Exit(0);
		}
		frameCount++;
		if(hwFrameLimit > 0 && frameCount >= hwFrameLimit) {
			sceGsSyncPath(0, 0);
			printf("hw frame limit %d reached, exiting\n", hwFrameLimit);
			Exit(0);
		}
	}
}

/* real: the two arms of 0x228278's CIRCLE branch.  n is the item index
 * - 0 Browser (0x227F50(0), no counterpart here), 1 System
 * Configuration (0x227268).  The pad layer calls this; menuconfig.c
 * owns everything the entry switches on. */
void
MenuSelectItem(int n)
{
	if(n == 1)
		MenuEnterConfig();	/* real: 0x227268 */
}
