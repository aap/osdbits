/*
 * swz.c - derive libgpu2's actual PSMCT32 in-page address mapping and compare
 * with the real-GS tables in osdbits/tools/gsmem.py.
 *
 * Two independent derivations:
 *   mode "blt"  - upload unique values through the model's BitBLT
 *   mode "draw" - draw 2048 one-pixel flat sprites with unique colours
 * then reverse-look-up where each (x,y) landed and print the in-page table.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgpu2.h>

typedef long long ll;
typedef unsigned long long ull;
typedef unsigned int u32;

extern void *lg2_bigalloc[8];

#define PUT(a, d) GS_PutPort((a), (ll)(ull)(d))
enum { R_PRIM = 0x00, R_RGBAQ = 0x01, R_XYZ2 = 0x05, R_XYOFFSET_1 = 0x18,
       R_PRMODECONT = 0x1a, R_SCISSOR_1 = 0x40, R_COLCLAMP = 0x46,
       R_TEST_1 = 0x47, R_FRAME_1 = 0x4c, R_ZBUF_1 = 0x4e,
       R_BITBLTBUF = 0x50, R_TRXPOS = 0x51, R_TRXREG = 0x52,
       R_TRXDIR = 0x53, R_HWREG = 0x54 };

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

static int
addr32(int x, int y, int tbp, int bw)
{
	int page = tbp / 32 + (y / 32) * bw + (x / 64);
	int px = x % 64, py = y % 32;
	return page * 2048 + blockTable32[py / 8][px / 8] * 64 +
	    columnTable32[py % 8][px % 8];
}

#define W 64
#define H 32

int
main(int argc, char **argv)
{
	static ull qw[W * H / 2];
	static int where[W * H];
	u32 *vw;
	int i, x, y;
	const char *mode = argc > 1 ? argv[1] : "draw";

	GS_InitSim();
	GS_OpenSim("swz", 640, 480, 0, 0);
	vw = (u32 *)lg2_bigalloc[0];
	memset(vw, 0, 0x400000);

	if (strcmp(mode, "blt") == 0) {
		for (i = 0; i < W * H / 2; i++)
			qw[i] = ((ull)(2 * i + 2) << 32) | (ull)(2 * i + 1);
		PUT(R_BITBLTBUF, (0ULL << 32) | (1ULL << 48) | (0ULL << 56));
		PUT(R_TRXPOS, 0ULL);
		PUT(R_TRXREG, (ull)W | ((ull)H << 32));
		PUT(R_TRXDIR, 0ULL);
		for (i = 0; i < W * H / 2; i++)
			PUT(R_HWREG, qw[i]);
	} else {
		PUT(R_FRAME_1, 0ULL | (1ULL << 16) | (0ULL << 24));
		PUT(R_ZBUF_1, 0x1c0ULL | (1ULL << 32));
		PUT(R_XYOFFSET_1, 0ULL);
		PUT(R_SCISSOR_1, 0ULL | (63ULL << 16) | (0ULL << 32) |
		    (31ULL << 48));
		PUT(R_PRMODECONT, 1ULL);
		PUT(R_TEST_1, (1ULL << 16) | (1ULL << 17));
		PUT(R_COLCLAMP, 1ULL);
		for (y = 0; y < H; y++)
			for (x = 0; x < W; x++) {
				PUT(R_PRIM, 6ULL);
				PUT(R_RGBAQ, (ull)(u32)(y * W + x + 1));
				PUT(R_XYZ2, (ull)(unsigned)(x << 4) |
				    ((ull)(unsigned)(y << 4) << 16) |
				    (0x1000ULL << 32));
				PUT(R_XYZ2, (ull)(unsigned)((x + 1) << 4) |
				    ((ull)(unsigned)((y + 1) << 4) << 16) |
				    (0x1000ULL << 32));
			}
	}

	for (i = 0; i < W * H; i++)
		where[i] = -1;
	for (i = 0; i < 0x400000 / 4; i++) {
		u32 v = vw[i];
		if (v >= 1 && v <= W * H && where[v - 1] < 0)
			where[v - 1] = i;
	}

	printf("mode=%s   in-page dword index per (x,y), 64x32 page\n", mode);
	{
		int bad = 0, miss = 0;
		for (y = 0; y < H; y++)
			for (x = 0; x < W; x++) {
				if (where[y * W + x] < 0) { miss++; continue; }
				if (where[y * W + x] != addr32(x, y, 0, 1))
					bad++;
			}
		printf("vs gsmem.addr32: %d mismatches, %d missing (of %d)\n",
		    bad, miss, W * H);
	}
	printf("\nmodel block index (x/8, y/8):\n     ");
	for (x = 0; x < 8; x++)
		printf("%4d", x);
	printf("\n");
	for (y = 0; y < 4; y++) {
		printf("%4d ", y);
		for (x = 0; x < 8; x++) {
			int m = where[(y * 8) * W + (x * 8)];
			printf("%4d", m < 0 ? -1 : m / 64);
		}
		printf("   gsmem:");
		for (x = 0; x < 8; x++)
			printf("%4d", blockTable32[y][x]);
		printf("\n");
	}
	printf("\nmodel column index (x%%8, y%%8) inside block 0:\n");
	for (y = 0; y < 8; y++) {
		printf("%4d ", y);
		for (x = 0; x < 8; x++) {
			int m = where[y * W + x];
			printf("%4d", m < 0 ? -1 : m % 64);
		}
		printf("   gsmem:");
		for (x = 0; x < 8; x++)
			printf("%4d", columnTable32[y][x]);
		printf("\n");
	}
	GS_CloseSim();
	return 0;
}
