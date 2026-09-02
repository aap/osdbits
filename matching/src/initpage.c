/* probe for 0x228460 v2 */
typedef unsigned int u32;
typedef struct Anim { int duration, timer, dirty, state; } Anim;
typedef struct Page {
	int title; void *items; int nitems; int rows;
	int cursor; int top; int mode; Anim anim;
} Page;
typedef struct Hdr {
	int title; void *items; int nitems; int rows;
	int cursor; int top; Anim anim;
} Hdr;
extern float camCtx[];		/* 0x27b440 */
extern int carouselDur[];	/* 0x27ec00 */
extern Anim carouselTimer[];	/* 0x27ec40 */
extern Page cfgPage[];		/* 0x27be28 */
extern Hdr mainHdr[];		/* 0x27be90 */
extern float camWork1[];	/* 0x352800 */
extern float camWork2[];	/* 0x352840 */
extern float pixAspect;		/* gp-32132 */
extern short someShort;		/* gp-30432 */
extern int pageIndex;		/* gp-28844 */
extern int dur40b;		/* gp-30400 */
extern int dur40;		/* gp-30380 */
extern int menuDur;		/* gp-30396 */
extern void setCamera(void *, float, float, float, float, float, float, float, float, float);
extern void setCamera2(void *);
extern void f21ED18(void);
extern int IsPAL(void);
extern void f2217D8(void);
extern void timerReset(Anim *);
extern int f204378(void);
extern void timerClose(Anim *);
extern void f2241C0(void);
extern void f200B80(int, int);


void
initScreenPage(void)
{
	int rate, rate2, d;

	someShort = 0;
	setCamera(camWork1, 512.0f, camCtx[3], camCtx[4], 2048.0f, 2048.0f,
		1.0f, pixAspect, 1.0f, 65536.0f);
	setCamera2(camWork2);
	pageIndex = 0;
	f21ED18();
	rate = IsPAL() ? 50 : 60;
	dur40b = rate * 40 / 60;
	rate2 = IsPAL() ? 50 : 60;
	d = rate2 / 6;
	cfgPage[0].anim.duration = dur40 + dur40b + d;
	cfgPage[0].mode = 0;
	carouselDur[0] = dur40b;
	carouselTimer[0].duration = dur40b + d;
	mainHdr[0].anim.duration = d;
	menuDur = d;
	f2217D8();
	timerReset(&mainHdr[0].anim);
	if(f204378()) {
		timerClose(&mainHdr[0].anim);
		f2241C0();
	} else
		f200B80(20500, 2);
}

