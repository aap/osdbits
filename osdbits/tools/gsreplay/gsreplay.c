/*
 * gsreplay - replay a PCSX2 GS dump through Sony's 1998 libgpu2 GS model
 *            and snapshot the 4 MB local memory at chosen points.
 *
 *   gsprep.py dump.gs dir       # once, to unpack the dump
 *   gsreplay dir [options]
 *
 * Options
 *   -o PREFIX          snapshot filename prefix (default "<dir>/snap")
 *   -s SPEC            take a snapshot at SPEC; repeatable.  SPEC is one of
 *                        end            after the whole stream (the default)
 *                        vsync[:N]      at every vsync, or only the N-th
 *                        xfer:N         after GIF transfer N
 *                        reg:N          after GS register write N
 *                        prim:N         after PRIM write N
 *                        draw:N         after vertex-kick N
 *                        frame          whenever FRAME_1/2 changes fbp
 *   -e SPEC            stop the replay at SPEC (same syntax)
 *   -Z A:B:HEXZ        force the Z value of vertex kicks A..B (hypothesis test)
 *   -x A:B             make vertex kicks A..B write nothing (bisect a pass)
 *   -n                 do not seed VRAM from the dump (start black)
 *   -l                 log a line at every FRAME fbp change (find draw ranges)
 *   -q                 quiet
 *   -v                 log every register write to stderr (huge)
 *   -w                 open the model's X window and GS_REFRESH at each vsync
 *
 * Snapshots are 4 MB raw files in REAL-GS physical layout, so
 * osdbits/tools/gsmem.py reads them unchanged.  See notes.md for why the
 * model's own layout differs and how the page permutation undoes it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgpu2.h>

typedef long long ll;
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;

extern void *lg2_bigalloc[8];
extern int lg2_nbig;

#define VMSIZE (4 * 1024 * 1024)
#define PUT(a, d) put((a), (u64)(d))

static u8 *vram;                 /* model's local memory, model layout */
static u8 *buf;                  /* scratch, real-GS layout */
static int quiet, verbose, window, noseed, seglog;
static const char *prefix = "snap";

/* counters, all monotonically increasing over the replay */
static long c_xfer, c_reg, c_prim, c_draw, c_vsync, c_snap;
static int cur_fbp[2] = {-1, -1};

/* ---------------- snapshot / stop specs ---------------- */
enum { S_END, S_VSYNC, S_XFER, S_REG, S_PRIM, S_DRAW, S_FRAME };
struct spec { int kind; long n; int any; };
static struct spec snaps[64]; static int nsnaps;
static struct spec stopat; static int have_stop;

static int
parse_spec(const char *s, struct spec *sp)
{
	const char *colon = strchr(s, ':');
	static const char *names[] = {"end","vsync","xfer","reg","prim","draw","frame"};
	int i, len = colon ? (int)(colon - s) : (int)strlen(s);

	for (i = 0; i < 7; i++)
		if ((int)strlen(names[i]) == len && strncmp(s, names[i], len) == 0) {
			sp->kind = i;
			sp->any = colon == NULL;
			sp->n = colon ? atol(colon + 1) : -1;
			return 0;
		}
	return -1;
}

/* ---------------- page permutation (see notes.md) ----------------
 * The model's in-page dword index differs from the retail GS by
 *     pi(i) = i ^ (bit10(i) << 9) ^ (bit4(i) << 3)
 * which is an involution, so one routine converts both ways. */
static int pitab[2048];

static void
pi_init(void)
{
	int i;
	for (i = 0; i < 2048; i++)
		pitab[i] = i ^ (((i >> 10) & 1) << 9) ^ (((i >> 4) & 1) << 3);
}

/* dst and src are 4 MB; permutes every 8 KB page */
static void
pi_copy(void *dst, const void *src)
{
	const u32 *s = src;
	u32 *d = dst;
	int p, i;

	for (p = 0; p < VMSIZE / 8192; p++, s += 2048, d += 2048)
		for (i = 0; i < 2048; i++)
			d[pitab[i]] = s[i];
}

static void
snapshot(const char *why)
{
	char name[512];

	pi_copy(buf, vram);
	snprintf(name, sizeof name, "%s%03ld_%s.bin", prefix, c_snap, why);
	FILE *f = fopen(name, "wb");
	if (f == NULL) { perror(name); exit(1); }
	fwrite(buf, 1, VMSIZE, f);
	fclose(f);
	if (!quiet)
		printf("snap %-28s xfer=%ld reg=%ld prim=%ld draw=%ld vsync=%ld\n",
		    name, c_xfer, c_reg, c_prim, c_draw, c_vsync);
	c_snap++;
}

static void
check(int kind, long n, const char *why)
{
	int i;

	for (i = 0; i < nsnaps; i++)
		if (snaps[i].kind == kind && (snaps[i].any || snaps[i].n == n))
			snapshot(why);
	if (have_stop && stopat.kind == kind && (stopat.any || stopat.n == n)) {
		if (!quiet)
			printf("stop at %s %ld\n", why, n);
		snapshot("stop");
		GS_CloseSim();
		exit(0);
	}
}

/* ---------------- register gate ----------------
 * The 1998 model answers an unknown drawing register with
 * fprintf(stderr,...) + exit(0), which would silently truncate the replay.
 * regprobe.c measured exactly which addresses it accepts. */
static u8 ok_reg[0x80];

static void
gate_init(void)
{
	int a;
	static const int bad[] = {0x0b,0x0e,0x0f,0x10,0x1d,0x1e,0x1f,0x20,0x21,
	    0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
	    0x30,0x31,0x32,0x33,0x38,0x39,0x3a,0x3c,0x3e,0x55,0x56,0x57,0x58,
	    0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,-1};
	int i;

	for (a = 0; a < 0x80; a++)
		ok_reg[a] = 1;
	for (i = 0; bad[i] >= 0; i++)
		ok_reg[bad[i]] = 0;
	for (a = 0x60; a <= 0x7e; a++)
		ok_reg[a] = 0;         /* SIGNAL/FINISH/LABEL + reserved */
}

static long nskipped;

/* ---------------- experiment knobs ----------------
 * -Z A:B:HEXZ   force the Z of every vertex kick numbered A..B
 * -x A:B        make vertex kicks A..B write nothing (FBMSK/ZMSK forced)
 * Ranges are in the same "draw" units the snapshot specs and -v log use, so
 * you find them once with -v and then bisect. */
struct range { long a, b; u64 z; };
static struct range zrng[16]; static int nzrng;
static struct range xrng[16]; static int nxrng;
static u64 last_frame[2], last_zbuf[2];
static int masking;

static int
in_range(struct range *r, int n, long v, u64 *z)
{
	int i;
	for (i = 0; i < n; i++)
		if (v >= r[i].a && v <= r[i].b) {
			if (z) *z = r[i].z;
			return 1;
		}
	return 0;
}

/* re-issue FRAME/ZBUF with (or without) the kill mask */
static void
apply_mask(int on)
{
	int c;
	for (c = 0; c < 2; c++) {
		u64 f = last_frame[c], z = last_zbuf[c];
		if (on) { f |= 0xffffffffULL << 32; z |= 1ULL << 32; }
		GS_PutPort(0x4c + c, (ll)f);
		GS_PutPort(0x4e + c, (ll)z);
	}
	masking = on;
}

static void
put(int addr, u64 data)
{
	if (addr < 0 || addr > 0x7f || !ok_reg[addr]) {
		nskipped++;
		return;
	}
	if (addr == 0x4c || addr == 0x4d) {
		last_frame[addr - 0x4c] = data;
		if (masking) data |= 0xffffffffULL << 32;
	}
	if (addr == 0x4e || addr == 0x4f) {
		last_zbuf[addr - 0x4e] = data;
		if (masking) data |= 1ULL << 32;
	}
	if ((addr == 0x04 || addr == 0x05 || addr == 0x0c || addr == 0x0d)) {
		u64 z;
		long next = c_draw + 1;
		if (nxrng) {
			int want = in_range(xrng, nxrng, next, NULL);
			if (want != masking)
				apply_mask(want);
		}
		if (nzrng && in_range(zrng, nzrng, next, &z)) {
			if (addr == 0x04 || addr == 0x0c)
				data = (data & ~(0xffffffULL << 32)) |
				    ((z & 0xffffff) << 32);
			else
				data = (data & 0xffffffffULL) | (z << 32);
		}
	}
	if (verbose)
		fprintf(stderr, "%08lx r%02x %016llx\n", c_reg, addr, data);
	GS_PutPort(addr, (ll)data);
	c_reg++;
	if (addr == 0x00 || addr == 0x1b) {
		c_prim++;
		check(S_PRIM, c_prim, "prim");
	} else if (addr == 0x04 || addr == 0x05 || addr == 0x0c || addr == 0x0d) {
		c_draw++;
		check(S_DRAW, c_draw, "draw");
	} else if (addr == 0x4c || addr == 0x4d) {
		int ctx = addr - 0x4c, fbp = (int)(data & 0x1ff);
		if (fbp != cur_fbp[ctx]) {
			if (seglog)
				printf("FRAME_%d fbp=%-4d(blk %-5d) psm=%-2d  at "
				    "draw=%ld prim=%ld xfer=%ld vsync=%ld\n",
				    ctx + 1, fbp, fbp * 32,
				    (int)((data >> 24) & 0x3f),
				    c_draw, c_prim, c_xfer, c_vsync);
			cur_fbp[ctx] = fbp;
			check(S_FRAME, fbp, "frame");
		}
	}
	check(S_REG, c_reg, "reg");
}

/* ---------------- GIF walker (one state machine per path) ---------------- */
struct gif {
	int state;          /* 0 tag, 1 packed, 2 reglist, 3 image */
	int nloop, eop, nreg, regi;
	u8 reg[16];
	u32 q;              /* internal Q, loaded by PACKED ST */
};
static struct gif path[4];

static void
packed(struct gif *g, u64 lo, u64 hi)
{
	int r = g->reg[g->regi];

	switch (r) {
	case 0x00:                                   /* PRIM */
		PUT(0x00, lo & 0x7ff);
		break;
	case 0x01:                                   /* RGBAQ */
		PUT(0x01, (lo & 0xff) | ((lo >> 32 & 0xff) << 8) |
		    ((hi & 0xff) << 16) | ((hi >> 32 & 0xffULL) << 24) |
		    ((u64)g->q << 32));
		break;
	case 0x02:                                   /* ST (Q goes internal) */
		g->q = (u32)(hi & 0xffffffff);
		PUT(0x02, lo);
		break;
	case 0x03:                                   /* UV */
		PUT(0x03, (lo & 0x3fff) | ((lo >> 32 & 0x3fff) << 16));
		break;
	case 0x04:                                   /* XYZF2 / XYZF3 */
		PUT((hi >> 47) & 1 ? 0x0c : 0x04,
		    (lo & 0xffff) | ((lo >> 32 & 0xffff) << 16) |
		    (((hi >> 4) & 0xffffff) << 32) |
		    (((hi >> 36) & 0xff) << 56));
		break;
	case 0x05:                                   /* XYZ2 / XYZ3 */
		PUT((hi >> 47) & 1 ? 0x0d : 0x05,
		    (lo & 0xffff) | ((lo >> 32 & 0xffff) << 16) |
		    ((hi & 0xffffffffULL) << 32));
		break;
	case 0x0a:                                   /* FOG */
		PUT(0x0a, ((hi >> 36) & 0xff) << 56);
		break;
	case 0x0e:                                   /* A+D */
		PUT((int)(hi & 0xff), lo);
		break;
	case 0x0f:                                   /* NOP */
		break;
	default:                                     /* TEX0/CLAMP/XYZ*3 etc */
		PUT(r, lo);
		break;
	}
	if (++g->regi == g->nreg) {
		g->regi = 0;
		if (--g->nloop == 0)
			g->state = 0;
	}
}

static void
qword(struct gif *g, u64 lo, u64 hi)
{
	switch (g->state) {
	case 0: {
		int i;
		g->nloop = lo & 0x7fff;
		g->eop = (lo >> 15) & 1;
		g->nreg = (lo >> 60) & 0xf;
		if (g->nreg == 0)
			g->nreg = 16;
		for (i = 0; i < g->nreg; i++)
			g->reg[i] = (hi >> (4 * i)) & 0xf;
		g->regi = 0;
		if ((lo >> 46) & 1)                       /* PRE */
			PUT(0x00, (lo >> 47) & 0x7ff);
		if (g->nloop == 0)
			break;
		g->state = ((lo >> 58) & 3) == 0 ? 1 :
		    ((lo >> 58) & 3) == 1 ? 2 : 3;
		break;
	}
	case 1:
		packed(g, lo, hi);
		break;
	case 2:
		PUT(g->reg[g->regi], lo);
		if (++g->regi == g->nreg) { g->regi = 0; if (--g->nloop == 0) { g->state = 0; break; } }
		PUT(g->reg[g->regi], hi);
		if (++g->regi == g->nreg) { g->regi = 0; if (--g->nloop == 0) g->state = 0; }
		break;
	case 3:
		PUT(0x54, lo);
		PUT(0x54, hi);
		if (--g->nloop == 0)
			g->state = 0;
		break;
	}
}

/* ---------------- main ---------------- */
static void *
slurp(const char *dir, const char *file, long *len)
{
	char p[512];
	FILE *f;
	void *d;
	long n;

	snprintf(p, sizeof p, "%s/%s", dir, file);
	if ((f = fopen(p, "rb")) == NULL) { perror(p); exit(1); }
	fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
	d = malloc(n);
	if (fread(d, 1, n, f) != (size_t)n) { perror(p); exit(1); }
	fclose(f);
	if (len) *len = n;
	return d;
}

int
main(int argc, char **argv)
{
	const char *dir = NULL;
	char pbuf[512];
	u8 *stream, *p, *end;
	long slen, i;
	u32 nrec;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
			prefix = argv[++i];
		else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
			if (parse_spec(argv[++i], &snaps[nsnaps++]) < 0) {
				fprintf(stderr, "bad -s spec %s\n", argv[i]);
				return 1;
			}
		} else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
			if (parse_spec(argv[++i], &stopat) < 0) {
				fprintf(stderr, "bad -e spec %s\n", argv[i]);
				return 1;
			}
			have_stop = 1;
		} else if (strcmp(argv[i], "-Z") == 0 && i + 1 < argc) {
			char *s = argv[++i], *e;
			zrng[nzrng].a = strtol(s, &e, 0);
			zrng[nzrng].b = *e == ':' ? strtol(e + 1, &e, 0) : zrng[nzrng].a;
			zrng[nzrng].z = *e == ':' ? strtoull(e + 1, NULL, 16) : 0;
			nzrng++;
		} else if (strcmp(argv[i], "-x") == 0 && i + 1 < argc) {
			char *s = argv[++i], *e;
			xrng[nxrng].a = strtol(s, &e, 0);
			xrng[nxrng].b = *e == ':' ? strtol(e + 1, &e, 0) : xrng[nxrng].a;
			nxrng++;
		} else if (strcmp(argv[i], "-n") == 0) noseed = 1;
		else if (strcmp(argv[i], "-l") == 0) seglog = 1;
		else if (strcmp(argv[i], "-q") == 0) quiet = 1;
		else if (strcmp(argv[i], "-v") == 0) verbose = 1;
		else if (strcmp(argv[i], "-w") == 0) window = 1;
		else if (argv[i][0] == '-') {
			fprintf(stderr, "unknown option %s\n", argv[i]);
			return 1;
		} else
			dir = argv[i];
	}
	if (dir == NULL) {
		fprintf(stderr, "usage: gsreplay <dir-from-gsprep> [-s spec]... "
		    "[-e spec] [-o prefix] [-n] [-q] [-v] [-w]\n");
		return 1;
	}
	if (nsnaps == 0)
		snaps[nsnaps++].kind = S_END, snaps[0].any = 1;
	if (strcmp(prefix, "snap") == 0) {
		snprintf(pbuf, sizeof pbuf, "%s/snap", dir);
		prefix = pbuf;
	}

	pi_init();
	gate_init();
	GS_InitSim();
	GS_OpenSim("gsreplay", 640, 480, window, 0);
	if (lg2_nbig < 1) { fprintf(stderr, "no VRAM allocation seen\n"); return 1; }
	vram = lg2_bigalloc[0];
	buf = malloc(VMSIZE);

	if (!noseed) {
		u8 *v = slurp(dir, "vram.bin", &slen);
		if (slen != VMSIZE) {
			fprintf(stderr, "vram.bin is %ld bytes, want %d\n",
			    slen, VMSIZE);
			return 1;
		}
		pi_copy(vram, v);
		free(v);
	} else
		memset(vram, 0, VMSIZE);

	stream = slurp(dir, "stream.bin", &slen);
	if (memcmp(stream, "GSR1", 4) != 0) {
		fprintf(stderr, "stream.bin: bad magic\n");
		return 1;
	}
	memcpy(&nrec, stream + 4, 4);
	p = stream + 8;
	end = stream + slen;
	if (!quiet)
		printf("replaying %u records (%ld bytes)\n", nrec, slen);

	while (p + 8 <= end) {
		int type = p[0], pth = p[1];
		u32 nqw;
		memcpy(&nqw, p + 4, 4);
		p += 8;
		if (type == 0) {
			u8 *q = p;
			u32 k;
			if (pth > 3) pth = 3;
			for (k = 0; k < nqw; k++, q += 16) {
				u64 lo, hi;
				memcpy(&lo, q, 8);
				memcpy(&hi, q + 8, 8);
				qword(&path[pth], lo, hi);
			}
			c_xfer++;
			check(S_XFER, c_xfer, "xfer");
			p += (size_t)nqw * 16;
		} else if (type == 1) {
			c_vsync++;
			if (window)
				GS_PutPort(0x7f, 0);
			check(S_VSYNC, c_vsync, "vsync");
		} else if (type == 2) {
			p += (size_t)nqw * 16;
		} else {
			fprintf(stderr, "bad record type %d\n", type);
			return 1;
		}
	}

	check(S_END, 0, "end");
	if (!quiet)
		printf("done: xfer=%ld reg=%ld prim=%ld draw=%ld vsync=%ld "
		    "skipped-regs=%ld\n", c_xfer, c_reg, c_prim, c_draw,
		    c_vsync, nskipped);
	GS_CloseSim();
	return 0;
}
