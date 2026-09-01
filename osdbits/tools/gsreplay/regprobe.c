/*
 * regprobe.c - which GS drawing-register addresses does the 1998 model accept?
 *
 * The model's error contract is fprintf(stderr,"Unknown register(0x%x)") +
 * exit(0), which would kill the replay harness.  Each address is therefore
 * probed in a forked child; a child that reaches _exit(42) is safe.
 */
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libgpu2.h>

typedef long long ll;
typedef unsigned long long ull;

static const char *nm[0x80];

static void
names(void)
{
	nm[0x00]="PRIM"; nm[0x01]="RGBAQ"; nm[0x02]="ST"; nm[0x03]="UV";
	nm[0x04]="XYZF2"; nm[0x05]="XYZ2"; nm[0x06]="TEX0_1"; nm[0x07]="TEX0_2";
	nm[0x08]="CLAMP_1"; nm[0x09]="CLAMP_2"; nm[0x0a]="FOG";
	nm[0x0c]="XYZF3"; nm[0x0d]="XYZ3";
	nm[0x14]="TEX1_1"; nm[0x15]="TEX1_2"; nm[0x16]="TEX2_1"; nm[0x17]="TEX2_2";
	nm[0x18]="XYOFFSET_1"; nm[0x19]="XYOFFSET_2"; nm[0x1a]="PRMODECONT";
	nm[0x1b]="PRMODE"; nm[0x1c]="TEXCLUT"; nm[0x22]="SCANMSK";
	nm[0x34]="MIPTBP1_1"; nm[0x35]="MIPTBP1_2"; nm[0x36]="MIPTBP2_1";
	nm[0x37]="MIPTBP2_2"; nm[0x3b]="TEXA"; nm[0x3d]="FOGCOL";
	nm[0x3f]="TEXFLUSH"; nm[0x40]="SCISSOR_1"; nm[0x41]="SCISSOR_2";
	nm[0x42]="ALPHA_1"; nm[0x43]="ALPHA_2"; nm[0x44]="DIMX"; nm[0x45]="DTHE";
	nm[0x46]="COLCLAMP"; nm[0x47]="TEST_1"; nm[0x48]="TEST_2"; nm[0x49]="PABE";
	nm[0x4a]="FBA_1"; nm[0x4b]="FBA_2"; nm[0x4c]="FRAME_1"; nm[0x4d]="FRAME_2";
	nm[0x4e]="ZBUF_1"; nm[0x4f]="ZBUF_2"; nm[0x50]="BITBLTBUF";
	nm[0x51]="TRXPOS"; nm[0x52]="TRXREG"; nm[0x53]="TRXDIR"; nm[0x54]="HWREG";
	nm[0x60]="SIGNAL"; nm[0x61]="FINISH"; nm[0x62]="LABEL"; nm[0x7f]="NOP/REFRESH";
}

int
main(void)
{
	int a;

	setvbuf(stdout, NULL, _IONBF, 0);
	names();
	for (a = 0; a <= 0x7f; a++) {
		pid_t pid = fork();
		int st;

		if (pid == 0) {
			int devnull = open("/dev/null", 1);
			dup2(devnull, 2);
			GS_InitSim();
			GS_OpenSim("rp", 640, 480, 0, 0);
			GS_PutPort(a, 0LL);
			_exit(42);
		}
		waitpid(pid, &st, 0);
		if (!WIFEXITED(st) || WEXITSTATUS(st) != 42)
			printf("  0x%02x %-12s REFUSED (exit %d sig %d)\n", a,
			    nm[a] ? nm[a] : "?",
			    WIFEXITED(st) ? WEXITSTATUS(st) : -1,
			    WIFSIGNALED(st) ? WTERMSIG(st) : 0);
	}
	printf("(everything not listed accepted a zero write)\n");
	return 0;
}
