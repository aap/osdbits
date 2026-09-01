/*
 * fmt.c - for each pixel format, derive libgpu2's in-page byte layout and
 * test the hypothesis that it equals the real GS layout permuted by
 *
 *     pi(dword_index_in_page) = i ^ (bit10(i) << 9) ^ (bit4(i) << 3)
 *
 * (derived from the PSMCT32 measurement in swz.c: the model's block index
 * has by1 ^= bx2 and its column index has cx2 ^= cy1 relative to retail).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgpu2.h>

typedef long long ll;
typedef unsigned long long ull;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

extern void *lg2_bigalloc[8];

#define PUT(a, d) GS_PutPort((a), (ll)(ull)(d))
enum { R_BITBLTBUF = 0x50, R_TRXPOS = 0x51, R_TRXREG = 0x52,
       R_TRXDIR = 0x53, R_HWREG = 0x54 };

/* ---------------- real-GS tables (GSdx / gsmem.py) ---------------- */
static const int bt32[4][8] = {
	{ 0,  1,  4,  5, 16, 17, 20, 21},
	{ 2,  3,  6,  7, 18, 19, 22, 23},
	{ 8,  9, 12, 13, 24, 25, 28, 29},
	{10, 11, 14, 15, 26, 27, 30, 31}};
static const int ct32[8][8] = {
	{ 0,  1,  4,  5,  8,  9, 12, 13},
	{ 2,  3,  6,  7, 10, 11, 14, 15},
	{16, 17, 20, 21, 24, 25, 28, 29},
	{18, 19, 22, 23, 26, 27, 30, 31},
	{32, 33, 36, 37, 40, 41, 44, 45},
	{34, 35, 38, 39, 42, 43, 46, 47},
	{48, 49, 52, 53, 56, 57, 60, 61},
	{50, 51, 54, 55, 58, 59, 62, 63}};
static const int bt32z[4][8] = {
	{24, 25, 28, 29,  8,  9, 12, 13},
	{26, 27, 30, 31, 10, 11, 14, 15},
	{16, 17, 20, 21,  0,  1,  4,  5},
	{18, 19, 22, 23,  2,  3,  6,  7}};
static const int bt16[8][4] = {
	{ 0,  2,  8, 10}, { 1,  3,  9, 11}, { 4,  6, 12, 14}, { 5,  7, 13, 15},
	{16, 18, 24, 26}, {17, 19, 25, 27}, {20, 22, 28, 30}, {21, 23, 29, 31}};
static const int ct16[8][16] = {
	{  0,  2,  8, 10, 16, 18, 24, 26,  1,  3,  9, 11, 17, 19, 25, 27},
	{  4,  6, 12, 14, 20, 22, 28, 30,  5,  7, 13, 15, 21, 23, 29, 31},
	{ 32, 34, 40, 42, 48, 50, 56, 58, 33, 35, 41, 43, 49, 51, 57, 59},
	{ 36, 38, 44, 46, 52, 54, 60, 62, 37, 39, 45, 47, 53, 55, 61, 63},
	{ 64, 66, 72, 74, 80, 82, 88, 90, 65, 67, 73, 75, 81, 83, 89, 91},
	{ 68, 70, 76, 78, 84, 86, 92, 94, 69, 71, 77, 79, 85, 87, 93, 95},
	{ 96, 98,104,106,112,114,120,122, 97, 99,105,107,113,115,121,123},
	{100,102,108,110,116,118,124,126,101,103,109,111,117,119,125,127}};
static const int bt8[4][8] = {
	{ 0,  1,  4,  5, 16, 17, 20, 21},
	{ 2,  3,  6,  7, 18, 19, 22, 23},
	{ 8,  9, 12, 13, 24, 25, 28, 29},
	{10, 11, 14, 15, 26, 27, 30, 31}};
static const int ct8[16][16] = {
	{  0,  4, 16, 20, 32, 36, 48, 52,  2,  6, 18, 22, 34, 38, 50, 54},
	{  8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
	{ 33, 37, 49, 53,  1,  5, 17, 21, 35, 39, 51, 55,  3,  7, 19, 23},
	{ 41, 45, 57, 61,  9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31},
	{ 96,100,112,116, 64, 68, 80, 84, 98,102,114,118, 66, 70, 82, 86},
	{104,108,120,124, 72, 76, 88, 92,106,110,122,126, 74, 78, 90, 94},
	{ 65, 69, 81, 85, 97,101,113,117, 67, 71, 83, 87, 99,103,115,119},
	{ 73, 77, 89, 93,105,109,121,125, 75, 79, 91, 95,107,111,123,127},
	{128,132,144,148,160,164,176,180,130,134,146,150,162,166,178,182},
	{136,140,152,156,168,172,184,188,138,142,154,158,170,174,186,190},
	{161,165,177,181,129,133,145,149,163,167,179,183,131,135,147,151},
	{169,173,185,189,137,141,153,157,171,175,187,191,139,143,155,159},
	{224,228,240,244,192,196,208,212,226,230,242,246,194,198,210,214},
	{232,236,248,252,200,204,216,220,234,238,250,254,202,206,218,222},
	{193,197,209,213,225,229,241,245,195,199,211,215,227,231,243,247},
	{201,205,217,221,233,237,249,253,203,207,219,223,235,239,251,255}};

/* dword-index page permutation derived from the PSMCT32 measurement */
static int
pi(int i)
{
	return i ^ (((i >> 10) & 1) << 9) ^ (((i >> 4) & 1) << 3);
}

/* real-GS byte offset inside a page */
static int
gs_off32(int x, int y)
{
	return (bt32[(y / 8) % 4][(x / 8) % 8] * 64 + ct32[y % 8][x % 8]) * 4;
}
static int
gs_off32z(int x, int y)
{
	return (bt32z[(y / 8) % 4][(x / 8) % 8] * 64 + ct32[y % 8][x % 8]) * 4;
}
static int
gs_off16(int x, int y)
{
	return (bt16[(y / 8) % 8][(x / 16) % 4] * 128 + ct16[y % 8][x % 16]) * 2;
}
static int
gs_off8(int x, int y)
{
	return bt8[(y / 16) % 4][(x / 16) % 8] * 256 + ct8[y % 16][x % 16];
}

static int
pi_byte(int b)
{
	return pi(b / 4) * 4 + b % 4;
}

/* generic: upload W x H unique values in psm, return model byte offsets */
static void
run(const char *name, int psm, int bits, int W, int H, int bw,
    int (*gsoff)(int, int))
{
	static ull qw[65536];
	static int where[65536];
	u8 *vram = lg2_bigalloc[0];
	int i, x, y, n, nq, bad = 0, miss = 0, badpi = 0;

	memset(vram, 0, 0x400000);
	n = W * H;
	nq = (n * bits + 63) / 64;   /* HWREG takes 64 bits per write */
	memset(qw, 0, (size_t)nq * 8);
	for (i = 0; i < n; i++) {
		ull v = (ull)(i + 1);
		int bitpos = i * bits;
		if (bits == 32)
			((u32 *)qw)[i] = (u32)v;
		else if (bits == 16)
			((u16 *)qw)[i] = (u16)((i % 65535) + 1);
		else if (bits == 8)
			((u8 *)qw)[i] = (u8)((i % 255) + 1);
		(void)bitpos;
	}
	PUT(R_BITBLTBUF, (0ULL << 32) | ((ull)bw << 48) | ((ull)psm << 56));
	PUT(R_TRXPOS, 0ULL);
	PUT(R_TRXREG, (ull)W | ((ull)H << 32));
	PUT(R_TRXDIR, 0ULL);
	for (i = 0; i < nq; i++)
		PUT(R_HWREG, qw[i]);

	for (i = 0; i < n; i++)
		where[i] = -1;
	if (bits == 32) {
		for (i = 0; i < 0x400000 / 4; i++) {
			u32 v = ((u32 *)vram)[i];
			if (v >= 1 && v <= (u32)n && where[v - 1] < 0)
				where[v - 1] = i * 4;
		}
	} else if (bits == 16) {
		for (i = 0; i < 0x400000 / 2; i++) {
			u16 v = ((u16 *)vram)[i];
			if (v >= 1 && v <= n && where[v - 1] < 0)
				where[v - 1] = i * 2;
		}
	} else {
		for (i = 0; i < 0x400000; i++) {
			u8 v = vram[i];
			if (v >= 1 && where[v - 1] < 0)
				where[v - 1] = i;
		}
	}

	for (y = 0; y < H; y++)
		for (x = 0; x < W; x++) {
			int idx = y * W + x;
			if (bits == 16) idx = (y * W + x) % 65535;
			if (bits == 8)  idx = (y * W + x) % 255;
			int m = where[idx];
			if (m < 0) { miss++; continue; }
			if (m != gsoff(x, y))
				bad++;
			if (m != pi_byte(gsoff(x, y)))
				badpi++;
		}
	printf("%-10s psm=%-2d  %3dx%-3d  vs realGS: %5d bad   "
	    "vs pi(realGS): %5d bad   (miss %d of %d)\n",
	    name, psm, W, H, bad, badpi, miss, W * H);
}

int
main(void)
{
	GS_InitSim();
	GS_OpenSim("fmt", 640, 480, 0, 0);

	run("PSMCT32", 0,  32, 64, 32, 1, gs_off32);
	run("PSMCT16", 2,  16, 64, 64, 1, gs_off16);
	run("PSMT8",  19,   8, 128, 64, 1, gs_off8);
	run("PSMCT24", 1,  32, 64, 32, 1, gs_off32);
	run("PSMZ32", 48,  32, 64, 32, 1, gs_off32z);
	run("PSMZ32/32tbl", 48, 32, 64, 32, 1, gs_off32);

	GS_CloseSim();
	return 0;
}
