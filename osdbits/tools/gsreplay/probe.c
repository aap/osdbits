/*
 * probe.c - behavioural probes against Sony's libgpu2 GS model.
 *
 * Run with no args to run them all; each prints PASS/FAIL lines.
 *
 *   1. headless GS_OpenSim (disp_on=0, no DISPLAY)
 *   2. locate the 4 MB local memory (via the __builtin_new hook in shims.c)
 *   3. does the model's PSMCT32 address swizzle match the real GS
 *      (osdbits/tools/gsmem.py addr32)?  Also PSMCT16 / PSMT8 / PSMZ32.
 *   4. do flat (TME=0) primitives rasterize?  (port/README.md claims not)
 *   5. alpha blending, scissor, Z test, AA1 sanity
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgpu2.h>

typedef long long ll;
typedef unsigned long long ull;
typedef unsigned int u32;
typedef unsigned short u16;

extern void *lg2_bigalloc[8];
extern unsigned int lg2_bigsize[8];
extern int lg2_nbig;

#define PUT(a, d) GS_PutPort((a), (ll)(ull)(d))

enum {
	R_PRIM = 0x00, R_RGBAQ = 0x01, R_ST = 0x02, R_UV = 0x03,
	R_XYZF2 = 0x04, R_XYZ2 = 0x05, R_TEX0_1 = 0x06, R_CLAMP_1 = 0x08,
	R_TEX1_1 = 0x14, R_XYOFFSET_1 = 0x18, R_PRMODECONT = 0x1a,
	R_TEXA = 0x3b, R_TEXFLUSH = 0x3f, R_SCISSOR_1 = 0x40,
	R_ALPHA_1 = 0x42, R_DTHE = 0x45, R_COLCLAMP = 0x46, R_TEST_1 = 0x47,
	R_PABE = 0x49, R_FBA_1 = 0x4a, R_FRAME_1 = 0x4c, R_ZBUF_1 = 0x4e,
	R_BITBLTBUF = 0x50, R_TRXPOS = 0x51, R_TRXREG = 0x52, R_TRXDIR = 0x53,
	R_HWREG = 0x54, R_REFRESH = 0x7f
};

static unsigned char *vram;
static u32 *vw;
static int fails;

/* ---- reference swizzle (identical to osdbits/tools/gsmem.py) ---- */
static const int blockTable32[4][8] = {
	{ 0,  1,  4,  5, 16, 17, 20, 21},
	{ 2,  3,  6,  7, 18, 19, 22, 23},
	{ 8,  9, 12, 13, 24, 25, 28, 29},
	{10, 11, 14, 15, 26, 27, 30, 31}};
static const int columnTable32[8][8] = {
	{ 0,  1,  4,  5,  8,  9, 12, 13},
	{ 2,  3,  6,  7, 10, 11, 14, 15},
	{16, 17, 20, 21, 24, 25, 28, 29},
	{18, 19, 22, 23, 26, 27, 30, 31},
	{32, 33, 36, 37, 40, 41, 44, 45},
	{34, 35, 38, 39, 42, 43, 46, 47},
	{48, 49, 52, 53, 56, 57, 60, 61},
	{50, 51, 54, 55, 58, 59, 62, 63}};
static const int blockTable16[8][4] = {
	{ 0,  2,  8, 10}, { 1,  3,  9, 11}, { 4,  6, 12, 14}, { 5,  7, 13, 15},
	{16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};
static const int columnTable16[8][16] = {
	{  0,  2,  8, 10, 16, 18, 24, 26,  1,  3,  9, 11, 17, 19, 25, 27},
	{  4,  6, 12, 14, 20, 22, 28, 30,  5,  7, 13, 15, 21, 23, 29, 31},
	{ 32, 34, 40, 42, 48, 50, 56, 58, 33, 35, 41, 43, 49, 51, 57, 59},
	{ 36, 38, 44, 46, 52, 54, 60, 62, 37, 39, 45, 47, 53, 55, 61, 63},
	{ 64, 66, 72, 74, 80, 82, 88, 90, 65, 67, 73, 75, 81, 83, 89, 91},
	{ 68, 70, 76, 78, 84, 86, 92, 94, 69, 71, 77, 79, 85, 87, 93, 95},
	{ 96, 98,104,106,112,114,120,122, 97, 99,105,107,113,115,121,123},
	{100,102,108,110,116,118,124,126,101,103,109,111,117,119,125,127}};

/* The model's in-page dword index differs from the retail GS by this
 * involution -- see swz.c / notes.md. */
static int
pi(int i)
{
	return i ^ (((i >> 10) & 1) << 9) ^ (((i >> 4) & 1) << 3);
}

/* retail-GS address (gsmem.py addr32) */
static int
gs32(int x, int y, int tbp, int bw)
{
	int page = tbp / 32 + (y / 32) * bw + (x / 64);
	int px = x % 64, py = y % 32;
	return page * 2048 + blockTable32[py / 8][px / 8] * 64 +
	    columnTable32[py % 8][px % 8];
}

/* the same pixel in the MODEL's local memory */
static int
addr32(int x, int y, int tbp, int bw)
{
	int a = gs32(x, y, tbp, bw);
	return (a & ~2047) | pi(a & 2047);
}

static int
addr16(int x, int y, int tbp, int bw)
{
	int page = tbp / 32 + (y / 64) * bw + (x / 64);
	int px = x % 64, py = y % 64;
	int a = page * 4096 + blockTable16[py / 8][px / 16] * 128 +
	    columnTable16[py % 8][px % 16];
	/* pi acts on dwords; a halfword keeps its position inside one */
	return ((a / 2) & ~2047) * 2 + pi((a / 2) & 2047) * 2 + (a & 1);
}

static void
chk(const char *what, int ok)
{
	printf("%-46s %s\n", what, ok ? "PASS" : "*** FAIL ***");
	if (!ok)
		fails++;
}

static void
basic_state(int fbp, int fbw)
{
	PUT(R_FRAME_1, (ull)fbp | ((ull)fbw << 16) | (0ULL << 24));
	PUT(R_ZBUF_1, (ull)0x1c0 | (0ULL << 24) | (1ULL << 32)); /* ZMSK */
	PUT(R_XYOFFSET_1, 0ULL);
	PUT(R_SCISSOR_1, 0ULL | (639ULL << 16) | (0ULL << 32) | (447ULL << 48));
	PUT(R_PRMODECONT, 1ULL);
	PUT(R_TEST_1, (1ULL << 16) | (1ULL << 17));   /* ZTE=1 ZTST=ALWAYS */
	PUT(R_COLCLAMP, 1ULL);
	PUT(R_DTHE, 0ULL);
	PUT(R_PABE, 0ULL);
	PUT(R_FBA_1, 0ULL);
	PUT(R_TEXA, 0ULL);
}

/* host->local upload helper: psm, dbp (blocks), dbw (in 64-px units) */
static void
upload(int dbp, int dbw, int dpsm, int dsax, int dsay, int w, int h,
    const ull *qw, int nq)
{
	int i;

	PUT(R_BITBLTBUF, (0ULL) | (0ULL << 16) | (0ULL << 24) |
	    ((ull)dbp << 32) | ((ull)dbw << 48) | ((ull)dpsm << 56));
	PUT(R_TRXPOS, ((ull)dsax << 32) | ((ull)dsay << 48));
	PUT(R_TRXREG, (ull)w | ((ull)h << 32));
	PUT(R_TRXDIR, 0ULL);
	for (i = 0; i < nq; i++)
		PUT(R_HWREG, qw[i]);
}

static void
xyz(int x, int y, int z)
{
	PUT(R_XYZ2, (ull)(unsigned)(x << 4) | ((ull)(unsigned)(y << 4) << 16) |
	    ((ull)(unsigned)z << 32));
}

int
main(void)
{
	int i, x, y, bad;
	static ull qw[1 << 17];

	GS_InitSim();
	GS_OpenSim("probe", 640, 480, 0, 0);
	printf("--- 1/2 init ---\n");
	printf("big allocations: %d\n", lg2_nbig);
	for (i = 0; i < lg2_nbig; i++)
		printf("  [%d] %p size 0x%x\n", i, lg2_bigalloc[i],
		    lg2_bigsize[i]);
	chk("headless GS_OpenSim(disp_on=0)", lg2_nbig >= 1);
	vram = lg2_bigalloc[0];
	vw = (u32 *)vram;

	/* ---- 3: PSMCT32 swizzle, dbp=0 bw=10 ---- */
	printf("--- 3 address swizzle ---\n");
	memset(vram, 0, 0x400000);
	for (i = 0; i < 128 * 64 / 2; i++)
		qw[i] = ((ull)(2 * i + 2) << 32) | (ull)(2 * i + 1);
	upload(0, 10, 0, 0, 0, 128, 64, qw, 128 * 64 / 2);
	bad = 0;
	for (y = 0; y < 64; y++)
		for (x = 0; x < 128; x++)
			if (vw[addr32(x, y, 0, 10)] != (u32)(y * 128 + x + 1))
				bad++;
	chk("PSMCT32 layout = pi(gsmem.addr32), tbp=0 bw=10", bad == 0);
	if (bad)
		printf("   %d/%d mismatches; (0,0)->%u (1,0)->%u (0,1)->%u\n",
		    bad, 128 * 64, vw[addr32(0, 0, 0, 10)],
		    vw[addr32(1, 0, 0, 10)], vw[addr32(0, 1, 0, 10)]);

	/* ---- PSMCT32 at a nonzero block pointer + nonzero dsax/dsay ---- */
	memset(vram, 0, 0x400000);
	for (i = 0; i < 64 * 32 / 2; i++)
		qw[i] = ((ull)(2 * i + 2) << 32) | (ull)(2 * i + 1);
	upload(6720, 10, 0, 33, 17, 64, 32, qw, 64 * 32 / 2);
	bad = 0;
	for (y = 0; y < 32; y++)
		for (x = 0; x < 64; x++)
			if (vw[addr32(x + 33, y + 17, 6720, 10)] !=
			    (u32)(y * 64 + x + 1))
				bad++;
	chk("PSMCT32 layout = pi(...), tbp=6720 dsax/y=33/17", bad == 0);

	/* ---- PSMCT16 ---- */
	memset(vram, 0, 0x400000);
	for (i = 0; i < 64 * 64 / 4; i++)
		qw[i] = (ull)(4 * i + 1) | ((ull)(4 * i + 2) << 16) |
		    ((ull)(4 * i + 3) << 32) | ((ull)(4 * i + 4) << 48);
	upload(0, 1, 2, 0, 0, 64, 64, qw, 64 * 64 / 4);   /* PSMCT16 */
	bad = 0;
	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++)
			if (((u16 *)vram)[addr16(x, y, 0, 1)] !=
			    (u16)(y * 64 + x + 1))
				bad++;
	chk("PSMCT16 layout = pi(gsmem-style addr16)", bad == 0);

	/* ---- 4: flat primitives ---- */
	printf("--- 4 flat (TME=0) primitives ---\n");
	memset(vram, 0, 0x400000);
	basic_state(0, 10);
	PUT(R_PRIM, 6ULL);                       /* SPRITE */
	PUT(R_RGBAQ, 0x80402010ULL);
	xyz(10, 10, 0x1000);
	xyz(50, 30, 0x1000);
	bad = 0;
	for (i = 0; i < 0x400000 / 4; i++)
		if (vw[i])
			bad++;
	chk("flat SPRITE rasterizes 40x20 = 800 px", bad == 800);
	if (bad != 800)
		printf("   got %d nonzero dwords\n", bad);
	{
		int ok = 1;
		for (y = 10; y < 30; y++)
			for (x = 10; x < 50; x++)
				if (vw[addr32(x, y, 0, 10)] != 0x80402010u)
					ok = 0;
		chk("  flat SPRITE lands at pi(gsmem.addr32) positions", ok);
		chk("  PSMCT32 word layout = A<<24|B<<16|G<<8|R",
		    vw[addr32(10, 10, 0, 10)] == 0x80402010u);
	}

	memset(vram, 0, 0x400000);
	basic_state(0, 10);
	PUT(R_PRIM, 3ULL);                       /* TRIANGLE STRIP */
	PUT(R_RGBAQ, 0x80ff8040ULL);
	xyz(100, 100, 0x1000);
	xyz(200, 100, 0x1000);
	xyz(100, 200, 0x1000);
	bad = 0;
	for (i = 0; i < 0x400000 / 4; i++)
		if (vw[i])
			bad++;
	chk("flat TRISTRIP rasterizes (~5000 px)", bad > 4000 && bad < 5300);
	printf("   (%d px)\n", bad);

	/* gouraud */
	memset(vram, 0, 0x400000);
	basic_state(0, 10);
	PUT(R_PRIM, 3ULL | (1ULL << 3));         /* TRISTRIP + IIP */
	PUT(R_RGBAQ, 0x000000ffULL);
	xyz(0, 0, 0x1000);
	PUT(R_RGBAQ, 0x00ff0000ULL);
	xyz(64, 0, 0x1000);
	PUT(R_RGBAQ, 0x0000ff00ULL);
	xyz(0, 64, 0x1000);
	chk("gouraud interpolates (corner red, mid mixed)",
	    (vw[addr32(1, 1, 0, 10)] & 0xff) > 0xf0 &&
	    (vw[addr32(20, 20, 0, 10)] & 0xff) < 0xd0 &&
	    (vw[addr32(20, 20, 0, 10)] & 0xff) > 0x30);
	printf("   (0,0)=%08x (20,20)=%08x\n", vw[addr32(1, 1, 0, 10)],
	    vw[addr32(20, 20, 0, 10)]);

	/* ---- 5: alpha blend / scissor / ztest ---- */
	printf("--- 5 back end ---\n");
	memset(vram, 0, 0x400000);
	basic_state(0, 10);
	PUT(R_PRIM, 6ULL);
	PUT(R_RGBAQ, 0xff804020ULL);
	xyz(0, 0, 0x1000);
	xyz(64, 64, 0x1000);
	/* Cs*As + Cd*(1-As): A=0x80, src 0x00,0x00,0x00 over dst */
	PUT(R_ALPHA_1, 0ULL | (1ULL << 2) | (0ULL << 4) | (1ULL << 6));
	                                         /* (Cs-Cd)*As>>7 + Cd */
	PUT(R_PRIM, 6ULL | (1ULL << 6));         /* SPRITE + ABE */
	PUT(R_RGBAQ, 0x40000000ULL);             /* black, As=0x40 = 1/2 */
	xyz(0, 0, 0x1000);
	xyz(32, 32, 0x1000);
	printf("   blended=%08x unblended=%08x\n", vw[addr32(4, 4, 0, 10)],
	    vw[addr32(40, 40, 0, 10)]);
	chk("ALPHA blend halves the destination (As=0x40)",
	    (vw[addr32(4, 4, 0, 10)] & 0xff) == 0x10 &&
	    (vw[addr32(40, 40, 0, 10)] & 0xff) == 0x20);

	memset(vram, 0, 0x400000);
	basic_state(0, 10);
	PUT(R_SCISSOR_1, 10ULL | (19ULL << 16) | (10ULL << 32) | (19ULL << 48));
	PUT(R_PRIM, 6ULL);
	PUT(R_RGBAQ, 0x11111111ULL);
	xyz(0, 0, 0x1000);
	xyz(64, 64, 0x1000);
	bad = 0;
	for (i = 0; i < 0x400000 / 4; i++)
		if (vw[i])
			bad++;
	chk("SCISSOR clips to 10x10", bad == 100);
	if (bad != 100)
		printf("   got %d px\n", bad);

	/* Z test */
	memset(vram, 0, 0x400000);
	basic_state(0, 10);
	PUT(R_ZBUF_1, (ull)0x1c0 | (0ULL << 24) | (0ULL << 32));  /* ZMSK=0 */
	PUT(R_TEST_1, (1ULL << 16) | (2ULL << 17));               /* ZTST=GEQUAL */
	PUT(R_PRIM, 6ULL);
	PUT(R_RGBAQ, 0x00112233ULL);
	xyz(0, 0, 0x8000);
	xyz(32, 32, 0x8000);
	PUT(R_RGBAQ, 0x00445566ULL);
	xyz(0, 0, 0x4000);                        /* behind: should fail */
	xyz(32, 32, 0x4000);
	chk("ZTST=GEQUAL rejects the farther sprite",
	    vw[addr32(4, 4, 0, 10)] == 0x00112233u);
	printf("   px=%08x\n", vw[addr32(4, 4, 0, 10)]);

	/* AA1 */
	memset(vram, 0, 0x400000);
	basic_state(0, 10);
	PUT(R_PRIM, 3ULL | (1ULL << 7));          /* TRISTRIP + AA1 */
	PUT(R_RGBAQ, 0x80ffffffULL);
	xyz(10, 10, 0x1000);
	xyz(100, 12, 0x1000);
	xyz(10, 100, 0x1000);
	{
		int cov = 0;
		for (i = 0; i < 0x400000 / 4; i++)
			if (vw[i] && (vw[i] >> 24) != 0x80)
				cov++;
		chk("AA1 writes partial-coverage alpha on edges", cov > 20);
		printf("   %d partial-coverage px\n", cov);
	}

	printf("\n%d failures\n", fails);
	GS_CloseSim();
	return fails != 0;
}
