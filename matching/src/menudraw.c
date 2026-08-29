/* Module U (menu / system configuration) - the 2D/UI draw layer.
 * Compile with ee-gcc 2.9-ee-991111 -O2 (self-contained, no -I needed).
 *
 *	ee-gcc -O2 -c src/menudraw.c -o build/menudraw.o
 *	python3 check.py build/menudraw.o <expanded.bin> menudraw-functions.txt
 *
 * Reversing notes for all of this live in docs/menu-draw.md.  Only the
 * small, fully-understood leaf functions are attempted here.
 *
 * SCOREBOARD: 38/43 attempted functions MATCH byte-exact.  The five
 * misses are all late-schedule / block-order ties, documented at their
 * definitions; every one is structurally correct (same instructions,
 * different placement), so they are the known allocator/scheduler tie
 * class - not source bugs.
 *
 * TWO REUSABLE CODEGEN FINDINGS from this TU:
 *
 *  A. STORE ORDER.  gcc 2.9 -O2 does NOT emit a run of independent
 *     stores to the same struct in source order; it rotates them.  For
 *     a straight-line run of three ending in the epilogue (timerReset)
 *     the emitted order is (s3, s1, s2) - so source (count, edge,
 *     state) yields (state, count, edge).  For a run inside a branch
 *     (timerOpen, timerClose) it is (s1, s3, s2).  Both were found by
 *     brute-forcing the 6 permutations; guessing from the ROM order
 *     directly gets it wrong every time.
 *
 *  B. THE LOW BLOCK MUST BE ONE SYMBOL.  The ROM reaches evenOddFrame
 *     (0x1f0c40), screen_width (0x1f0c50) and screen_height (0x1f0c54)
 *     through ONE `lui r,0x1f` with three displacements.  Three
 *     separate `extern int x[]` declarations make gcc emit a separate
 *     %hi per symbol; a literal pointer cast `*(int*)0x1f0c50` folds
 *     when read once but materialises lui+ori when read twice (which
 *     the ROM never does).  Only a single `extern int lowBlock[]`
 *     indexed by constants reproduces the shared base.
 *
 * ADDRESS MATERIALISATION follows config.c's codegen law 10: gp-relative
 * .sdata scalars are plain `extern int x;` (small-data eligible), and
 * everything at a fixed non-gp address is an INCOMPLETE extern array
 * (`extern T x[];`) so gcc cannot small-data it and emits the ROM's
 * compensated lui/addiu.
 */

typedef unsigned int u32;

/* ---- the animation timer object (0x22ac10..0x22acc0) ------------- */

typedef struct Timer {
	int duration;
	int count;
	int edge;		/* set for the single frame a transition ends */
	int state;		/* 0 closed, 1 opening, 2 open, 3 closing */
} Timer;

/* ---- the 0x40-byte 2D sprite record (docs/menu-draw.md 3.1) ------ */

typedef struct Sprite {
	int r, g, b, a;
	int x0, y0, u0, v0;
	int x1, y1, u1, v1;
	int z;
	int abe;
	int tme;
	int pad;
} Sprite;

/* ---- the scratchpad packet context (docs/menu-draw.md 2.1) ------- */

typedef struct Pkt {
	u32 *cur;
	u32 *base;
	u32 *tag;
	u32 *vifcode;
	u32 *unused;
	u32 *giftag;
} Pkt;

/* ---- the TEXC descriptor (docs/menu-draw.md 5.2) ----------------- */

typedef struct TexC {
	void *data;
	int wexp;
	int hexp;
} TexC;

typedef struct Color {
	int r, g, b, a;
} Color;

/* text engine, black box (docs/menu-draw.md 6.1) */
extern void osdTextSetColor(int r, int g, int b, int a);
extern void osdTextSetPos(int x, int y);
extern void osdTextDraw(const char *s);
extern int osdTextWidth(const char *s);

/* module-local statics at fixed addresses */
extern float camCtx[];		/* 0x27b440 */
extern int buttonSetExtra[];	/* 0x27b5e0 */
extern int screenFlag[];	/* 0x27be40 */
extern TexC texc[];		/* 0x27f1c0 */
extern int texcOff[];		/* 0x27f280 */
extern Timer bgTimer[];		/* 0x27f190 */
extern Timer transTimer[];	/* 0x27f620 */
extern int uiModel[];		/* 0x352880 */
extern Sprite edgeStrip[];	/* 0x27b7f0 */
extern Sprite curtain[];	/* 0x27f630 */
extern Sprite fsBlit[];		/* 0x27f760 */

/* the low block (0x1f0000), below the program image.  It has to be ONE
 * symbol: the ROM keeps 0x1f0000 in a single register and reaches
 * evenOddFrame / screen_width / screen_height through three different
 * displacements off it. */
extern int lowBlock[];		/* 0x1f0000 */
#define drawBufSel	lowBlock[0xc40/4]	/* evenOddFrame */
#define screenW		lowBlock[0xc50/4]
#define screenH		lowBlock[0xc54/4]

#define DBUFF	((void *)0x1f0a10)	/* the shared sceGsDBuff */
#define UNCACHED(p) ((Sprite *)((u32)(p) | 0x20000000))

/* gp-relative .sdata scalars */
extern int buttonSetOverride;	/* gp-30768 */
extern int buttonSetAlpha;	/* gp-30772 */
extern int transitionPhase;	/* gp-28844 */
extern int bgFade0;		/* gp-28840 */
extern int bgFade1;		/* gp-28836 */
extern int bgFade2;		/* gp-28832 */
extern int fadeState;		/* gp-28828 */
extern int fadeCount;		/* gp-28824 */
extern int padRepeatUp;		/* gp-28800 */
extern int padRepeatDown;	/* gp-28796 */
extern int padPressed;		/* gp-30312 */
extern int padReleased;		/* gp-30316 */
extern int modelDirty;		/* gp-30352 */
extern int dur40;		/* gp-30380 */
extern u32 vramCursor;		/* gp-30384 */
extern int frameCounter;	/* gp-30392 */

/* prototypes - EVERYTHING, in address order */
void setCamTarget(void);				/* 0x21ce40 */
int getButtonOverride(void);				/* 0x21d740 */
void setButtonOverride(int f);				/* 0x21d748 */
void setButtonOverrideAlpha(int a);			/* 0x21d750 */
void setButtonOverrideExtra(int v);			/* 0x21d758 */
void drawTextL(int x, int y, const Color *c, int alpha, const char *s);
void drawTextC(int x, int y, const Color *c, int alpha, const char *s);
void drawTextR(int x, int y, const Color *c, int alpha, const char *s);
int getTransitionPhase(void);				/* 0x226948 */
int screenIsOne(void);					/* 0x226a48 */
void initBgTimer(void);					/* 0x2287b0 */
void tickFrameCounter(void);				/* 0x2287d0 */
void bgTimerOpen(void);					/* 0x2291e8 */
void bgTimerClose(void);				/* 0x229230 */
void bgFadeUpdate(void);				/* 0x229278 */
void setTexCOffset(int i);				/* 0x229750 */
TexC *getTexC(int i);					/* 0x2297a0 */
int getTexCOffset(int i);				/* 0x2297b8 */
u32 gsVramAlloc(int n);					/* 0x2297d0 */
void pktOpen(Pkt *pk);					/* 0x2293e0 (extern) */
void pktAddPrim(Pkt *pk, const Sprite *s);		/* 0x2297e8 (extern) */
void pktAddQuad(Pkt *pk, const Sprite *s);		/* 0x2298a8 (extern) */
void pktKick(Pkt *pk);					/* 0x2294b8 (extern) */
void drawSprite(const Sprite *s);			/* 0x2299c0 */
int timerGetDuration(Timer *t);				/* 0x22ac10 */
int timerGetCount(Timer *t);				/* 0x22ac18 */
int timerScale(Timer *t, int n);			/* 0x22ac20 */
int timerIsState(Timer *t, int s);			/* 0x22ac48 */
int timerGetEdge(Timer *t);				/* 0x22ac58 */
void timerReset(Timer *t);				/* 0x22ac60 */
void timerOpen(Timer *t);				/* 0x22ac70 */
void timerClose(Timer *t);				/* 0x22ac90 */
void timerStep(Timer *t);				/* 0x22acc0 */
int getFadeCount(void);					/* 0x22ad28 */
int getFadeState(void);					/* 0x22ad30 */
void transTimerOpen(void);				/* 0x22ae80 */
void transTimerClose(void);				/* 0x22aec8 */
int *getUiModel(int i);					/* 0x22b0e8 */
void loadUiModel(void);					/* 0x22b138 (extern) */
void reloadUiModel(void);				/* 0x22b128 */
void clearPadState(void);				/* 0x22be18 */
void drawLetterboxBars(void);				/* 0x21d1f8 (extern) */
void drawLetterbox(void);				/* 0x21d368 */
void drawEdgeStrip(void);				/* 0x21db18 */
void *getResourcePtr(int i);				/* 0x205cb0 (extern) */
void initTexC(void);					/* 0x229698 */
void setDrawBuf(void *db, int sel, const Color *clr, int fld);
void setBlend(int mode, int ztst);			/* 0x22a0c0 (extern) */
void drawSweep(void);					/* 0x22af10 (extern) */
void drawFadeCurtain(void);				/* 0x22afb8 */
void transStep(void);					/* 0x22b020 */
void fadeStep(void);					/* 0x22b058 */
void drawFullScreenBlit(int abe);			/* 0x22c190 */
extern int dur80;			/* gp-30376 */

/* ------------------------------------------------------------------ */

void
setCamTarget(void)
{
	camCtx[0] = 1.0f;
}

int
getButtonOverride(void)
{
	return buttonSetOverride;
}

void
setButtonOverride(int f)
{
	buttonSetOverride = f;
}

void
setButtonOverrideAlpha(int a)
{
	buttonSetAlpha = a;
}

void
setButtonOverrideExtra(int v)
{
	buttonSetExtra[0] = v;
}

/* the three text drawers: identical except for horizontal alignment */

void
drawTextL(int x, int y, const Color *c, int alpha, const char *s)
{
	osdTextSetColor(c->r, c->g, c->b, alpha);
	osdTextSetPos(x, y);
	osdTextDraw(s);
}

void
drawTextC(int x, int y, const Color *c, int alpha, const char *s)
{
	if (alpha < 16)
		return;
	drawTextL(x - osdTextWidth(s)/2, y, c, alpha, s);
}

void
drawTextR(int x, int y, const Color *c, int alpha, const char *s)
{
	if (alpha < 16)
		return;
	drawTextL(x - osdTextWidth(s), y, c, alpha, s);
}

int
getTransitionPhase(void)
{
	return transitionPhase;
}

int
screenIsOne(void)
{
	return screenFlag[0] == 1;
}

void
initBgTimer(void)
{
	bgTimer[0].duration = dur40;
	bgFade2 = 0;
	bgFade1 = 0;
	bgFade0 = 0;
	frameCounter = 0;
}

void
tickFrameCounter(void)
{
	frameCounter++;
}

void
bgTimerOpen(void)
{
	if (timerIsState(&bgTimer[0], 0))
		timerOpen(&bgTimer[0]);
}

void
bgTimerClose(void)
{
	if (timerIsState(&bgTimer[0], 2))
		timerClose(&bgTimer[0]);
}

void
bgFadeUpdate(void)
{
	bgFade0 = timerScale(&bgTimer[0], 40);
	bgFade1 = timerScale(&bgTimer[0], 40);
	bgFade2 = timerScale(&bgTimer[0], 40);
}

void
setTexCOffset(int i)
{
	texcOff[i] = vramCursor;
	vramCursor += (1 << texc[i].wexp) << texc[i].hexp;
}

TexC *
getTexC(int i)
{
	return &texc[i];
}

int
getTexCOffset(int i)
{
	return texcOff[i];
}

u32
gsVramAlloc(int n)
{
	u32 base = vramCursor;

	vramCursor = base + (n << 6);
	return base >> 6;
}

void
drawSprite(const Sprite *s)
{
	Pkt pk;

	pktOpen(&pk);
	pktAddPrim(&pk, s);
	pktKick(&pk);
	pktOpen(&pk);
	pktAddQuad(&pk, s);
	pktKick(&pk);
}

int
timerGetDuration(Timer *t)
{
	return t->duration;
}

int
timerGetCount(Timer *t)
{
	return t->count;
}

int
timerScale(Timer *t, int n)
{
	return t->count * n / t->duration;
}

int
timerIsState(Timer *t, int s)
{
	return t->state == s;
}

int
timerGetEdge(Timer *t)
{
	return t->edge;
}

void
timerReset(Timer *t)
{
	t->count = 0;
	t->edge = 0;
	t->state = 0;
}

void
timerOpen(Timer *t)
{
	if (t->state == 0) {
		t->count = 0;
		t->edge = 1;
		t->state = 1;
	}
}

void
timerClose(Timer *t)
{
	if (t->state == 2) {
		t->edge = 1;
		t->count = t->duration;
		t->state = 3;
	}
}

void
timerStep(Timer *t)
{
	t->edge = 0;
	switch (t->state) {
	case 1:
		if (++t->count == t->duration) {
			t->edge = 1;
			t->state = 2;
		}
		break;
	case 3:
		if (--t->count == 0) {
			t->edge = 1;
			t->state = 0;
		}
		break;
	}
}

int
getFadeCount(void)
{
	return fadeCount;
}

int
getFadeState(void)
{
	return fadeState;
}

void
transTimerOpen(void)
{
	if (timerIsState(&transTimer[0], 0))
		timerOpen(&transTimer[0]);
}

void
transTimerClose(void)
{
	if (timerIsState(&transTimer[0], 2))
		timerClose(&transTimer[0]);
}

int *
getUiModel(int i)
{
	return &uiModel[i];
}

void
reloadUiModel(void)
{
	modelDirty = 1;
	loadUiModel();
}

void
clearPadState(void)
{
	padPressed = 0;
	padReleased = 0;
	padRepeatDown = 0;
	padRepeatUp = 0;
}

void
drawLetterbox(void)
{
	int t = *getUiModel(0);

	if (t == 0 || t == 2)
		drawLetterboxBars();
}

/* TIE: 23/33 aligned - the prologue schedules the screen_width load
 * before the `or s0,s0,0x20000000`; same instructions, different slots. */
void
drawEdgeStrip(void)
{
	Sprite *sp = UNCACHED(&edgeStrip[0]);

	sp->x0 = (screenW << 4) - 40;
	sp->x1 = (screenW << 4) - 8;
	sp->y1 = screenH << 4;
	setDrawBuf(DBUFF, drawBufSel, 0, 0);
	setBlend(1, 1);
	drawSprite(sp);
}

/* TIE: 44/46 - the ROM keeps texc[0].data on the bare `lui 0x28` with a
 * -3648 displacement and only then rebases s0; we form the base one
 * instruction earlier. */
void
initTexC(void)
{
	vramCursor = screenW * screenH * 5;
	texc[0].data = getResourcePtr(45);
	texc[1].data = getResourcePtr(46);
	texc[2].data = getResourcePtr(47);
	texc[3].data = getResourcePtr(48);
	texc[4].data = getResourcePtr(49);
	texc[5].data = getResourcePtr(50);
	texc[6].data = getResourcePtr(51);
	texc[7].data = getResourcePtr(52);
	texc[8].data = getResourcePtr(53);
	texc[9].data = getResourcePtr(54);
}

/* TIE: 24/26 - `sd ra,16(sp)` and the evenOddFrame load are swapped. */
void
drawFadeCurtain(void)
{
	Sprite *sp = UNCACHED(&curtain[0]);

	setDrawBuf(DBUFF, drawBufSel, 0, 0);
	setBlend(1, 1);
	sp->a = 128 - fadeCount;
	drawSprite(sp);
}

void
transStep(void)
{
	/* the curtain is drawn EVERY frame (it is invisible at alpha 0);
	 * only the sweep is conditional.  The ROM's `bnez` target IS the
	 * tail call to drawFadeCurtain, which the else path falls into. */
	timerStep(&transTimer[0]);
	if (getFadeState() == 0)
		drawSweep();
	drawFadeCurtain();
}

/* TIE: 27/36 - block ORDER only.  The ROM emits the whole test chain
 * first and both bodies out of line ([tests][fade-in][fade-out][tail]);
 * every if/else-if/switch spelling tried puts the fade-in body inline
 * between the two tests.  This is gcc's cross-jumping/block-reorder
 * pass, not a source shape - all four paths converge on the same tail
 * (note how the ROM's `else` path loads v0=2 itself and jumps PAST the
 * shared `li v0,2`). */
void
fadeStep(void)
{
	int st = fadeState;

	if (st <= 0)
		;
	else if (st < 3) {
		if (++fadeCount > 128) {
			fadeCount = 128;
			st = 0;
			fadeState = 0;
		}
	} else if (st == 3) {
		if (--fadeCount < 0)
			fadeCount = 0;
	}
	if (st == 2 && fadeCount == 128 - dur80)
		transTimerOpen();
}

/* TIE: 30/37 - same prologue-scheduling tie as drawEdgeStrip. */
void
drawFullScreenBlit(int abe)
{
	Sprite *sp = UNCACHED(&fsBlit[0]);

	sp->x1 = screenW << 4;
	sp->y1 = screenH << 4;
	sp->u1 = (screenW << 4) + 8;
	sp->v1 = (screenH << 4) + 8;
	setBlend(1, 1);
	sp->abe = abe;
	drawSprite(sp);
	setBlend(1, 3);
}
