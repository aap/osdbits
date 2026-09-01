/* leaf.c - the smallest untouched Module U functions: empty bodies,
 * tail-call thunks, accessors and one-line setters.  They belong to
 * several different retail TUs; grouping them here is only a matching
 * convenience (check.py compares each function independently).
 *
 *	./try.sh src/leaf.c leaf-functions.txt
 *
 * Conventions from matching/README.md and matching/src/config.c:
 *  - absolute-address globals are INCOMPLETE extern arrays so gcc
 *    cannot put them in small data ($gp);
 *  - globals the ROM really does reach through $gp stay plain externs.
 */

/* ---- $gp globals -------------------------------------------------- */
extern int uiDirty;			/* gp-30328 = 0x2a79f8	*/
extern int scrollBase;			/* gp-30396 = 0x2a79b4	*/
extern int scrollSpan;			/* gp-30400 = 0x2a79b0	*/

/* ---- absolute globals --------------------------------------------- */
extern int cfgValA[];			/* 0x352990		*/
extern int cfgValB[];			/* 0x352994		*/
extern int cfgValC[];			/* 0x352998		*/
extern int cfgObj[];			/* 0x3529b0		*/
extern int wizObj[];			/* 0x268860		*/
extern int pageSlot[];			/* 0x27bf70		*/
extern int scrollTimer[];		/* 0x27ec40		*/
extern int scrollFrom[];		/* 0x27c21c		*/
extern int scrollAnim[];		/* 0x27c258		*/

/* ---- callees ------------------------------------------------------ */
extern void osdIdle(void);		/* 0x200b80 */
extern void cfgCubeState(void);		/* 0x226bb8 */
extern void modelSync(void);		/* 0x22b138 */
extern void backOpen(int *);		/* 0x2294b8 */
extern void wizOpen(int *);		/* 0x22ee00 */
extern void carouselClock(void);	/* 0x225628 */
extern void carouselTail(void);		/* 0x225878 */
extern void timerStep(int *);		/* 0x22acc0 */
extern void pageTail(void);		/* 0x227100 */
extern void timerOpen(int *);		/* 0x22ac70 */

/* ------------------------------------------------------------------ */
/* 0x21cfd0 - nullsub_3 */
void nullsub_21CFD0(void) {}

/* 0x21f978, 0x221900, 0x223650, 0x225310, 0x2287a8 - five identical
 * one-line thunks; gcc turns the trailing void call into a bare `j' */
void thunk_21F978(void) { osdIdle(); }
void thunk_221900(void) { osdIdle(); }
void thunk_223650(void) { osdIdle(); }
void thunk_225310(void) { osdIdle(); }
void thunk_2287A8(void) { osdIdle(); }

/* 0x226cf8 */
void thunk_226CF8(void) { cfgCubeState(); }

/* 0x22b588 */
void thunk_22B588(void) { modelSync(); }

/* 0x22b790 / 0x22b7a0 / 0x22b7b0 - three adjacent int getters */
int get_22B790(void) { return cfgValA[0]; }
int get_22B7A0(void) { return cfgValB[0]; }
int get_22B7B0(void) { return cfgValC[0]; }

/* 0x22b950 / 0x22b960 - set and clear the same $gp flag */
void set_22B950(void) { uiDirty = 1; }
void clr_22B960(void) { uiDirty = 0; }

/* 0x22ed10 / 0x22ee88 - thunks that pass one static object */
void thunk_22ED10(void) { backOpen(cfgObj); }
void thunk_22EE88(void) { wizOpen(wizObj); }

/* 0x221910 - store the argument into an absolute global */
void set_221910(int a) { pageSlot[0] = a; }

/* 0x225978 - call, then tail-call */
void seq_225978(void)
{
	carouselClock();
	carouselTail();
}

/* 0x227198 - step a static timer, then tail-call */
void seq_227198(void)
{
	timerStep(scrollTimer);
	pageTail();
}

/* 0x228898 - p[0] and p[1] scaled by 3.0f and by the third argument.
 * The ROM materialises 3.0f with `lui at,0x4040 / mtc1' - gcc 2.9's
 * no-literal-pool path for a float constant with a zero low half. */
void scale_228898(float *p, float x, float y, float s)
{
	float a, b;

	b = y * 3.0f;
	a = x * 3.0f;
	p[1] = b * s;
	p[0] = a * s;
}

/* 0x223658 - latch the scroll origin, then open the timer */
void open_223658(void)
{
	scrollFrom[0] = scrollBase;
	scrollAnim[0] = scrollBase + scrollSpan;
	timerOpen(scrollAnim);
}

/* 0x220640 - a packed pair of 3-bit fields, one byte per two indices,
 * over a table at the LITERAL address 0x1f1224 (built with lui+ori, not
 * lui+addiu(%lo), so it is an integer constant in the source, not a
 * symbol).  0x22066c is this function's odd-index arm, not a function
 * of its own - the jr-ra boundary scan over-splits duplicated tails. */
int get_220640(int a)
{
	unsigned char *t = (unsigned char *)0x1f1224;

	if (a & 1)
		return (t[a / 2] >> 4) & 7;
	return t[a / 2] & 7;
}
