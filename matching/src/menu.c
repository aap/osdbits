/* Module U (0x21c910-0x230000) - the main-menu / system-configuration
 * high-level logic: the transition ("anim") object, the menu-page
 * engine's small accessors and its cursor/value navigation.
 *
 *	ee-gcc -O2 -c src/menu.c -o build/menu.o
 *	python3 check.py build/menu.o <expanded.bin> menu-functions.txt
 *
 * See docs/menu-logic.md for the reverse-engineering write-up that
 * these declarations come from.
 *
 * SCOREBOARD: 33/36 attempted functions MATCH byte-exact.  Residuals:
 *  - pageEnter (0x221c98) and pageSquare (0x221d40): 12/14, the two
 *    differing words are the same two values on adjacent registers
 *    (the ROM loads curPage into $a0 and dereferences it into $v0, we
 *    use $v0 for both).  That is the known allocator tie class - proven
 *    immune to source-order levers - so it is documented, not ground on.
 *  - pageClose (0x221b70): 22/25, purely a delay-slot/branch-likely
 *    scheduling difference (the ROM emits beqzl with the target's
 *    `addiu a0,a0,28' hoisted into both delay slots, we emit beqz with
 *    one slot left as nop).  Structure, registers and constants are
 *    otherwise identical.
 *
 * Codegen findings added here, on top of config.c's list:
 *  - store ORDER inside an `if' body is emitted in a fixed permutation
 *    of the source order, and the permutation depends on how many
 *    stores there are and whether one of them sources from a load.
 *    anmReset/anmFadeIn/anmFadeOut all needed a different source
 *    ordering than the ROM's emitted order; brute-forcing the 3! or 3
 *    rotations is faster than reasoning about it.
 *  - a two-armed `if (state==1) {...} else if (state==3) {...}` is
 *    emitted INLINE by gcc, but the ROM places both bodies after the
 *    function's `jr ra'.  Writing the tests as `goto' to labels placed
 *    after a bare `return' reproduces the ROM layout exactly
 *    (anmTick, 0x22acc0).
 *  - `x = -1' on an `int' global gives `li v0,-1'; the ROM's
 *    `lui 0xffff / ori 0xffff' pair only appears if the GLOBAL's type
 *    is unsigned and the constant is written 0xffffffff
 *    (modelInvalidate, 0x22b108) - the same storage-type effect
 *    config.c found for `~'-folded masks.
 *
 * Conventions follow matching/README.md and src/config.c:
 *  - every absolute-address global is an INCOMPLETE extern array so gcc
 *    cannot put it in small data ($gp);
 *  - globals the ROM really does reach through $gp stay plain externs;
 *  - everything is prototyped, declaration order = ROM address order.
 */

typedef unsigned int u32;

/* ------------------------------------------------------------------ */
/* the transition object: every screen, popup and item carousel owns
 * one.  state 0 = hidden, 1 = fading in, 2 = shown, 3 = fading out. */
typedef struct Anim {
	int	duration;	/* +0  frames the fade takes		*/
	int	timer;		/* +4  0..duration			*/
	int	dirty;		/* +8  set on the frame state changed	*/
	int	state;		/* +12					*/
} Anim;

/* one row of a menu page (56 bytes) */
typedef struct Item {
	int	label;		/* +0x00 osdGetString id		*/
	int	nvalues;	/* +0x04 number of options (0 = built at runtime) */
	int	value;		/* +0x08 selected option index		*/
	int	field;		/* +0x0c word index into the UI model	*/
	void	*values;	/* +0x10 option array, stride 48	*/
	void	(*activate)();	/* +0x14				*/
	void	*h18, *h1c, *h20, *h24;
	void	(*cursorHook)(struct Item *, int);	/* +0x28	*/
	void	(*init)(struct Item *);			/* +0x2c	*/
	void	(*changed)(struct Item *, int);		/* +0x30	*/
	void	*h34;
} Item;

/* a menu page (84 bytes) */
typedef struct Page {
	int	title;		/* +0x00 osdGetString id		*/
	Item	*items;		/* +0x04				*/
	int	nitems;		/* +0x08				*/
	int	rows;		/* +0x0c				*/
	int	cursor;		/* +0x10				*/
	int	top;		/* +0x14				*/
	int	mode;		/* +0x18 0 browse 1 edit 2 commit 3 step */
	Anim	anim;		/* +0x1c				*/
	int	f2c, f30;
	int	scroll;		/* +0x34				*/
	int	slide;		/* +0x38				*/
	int	buttons;	/* +0x3c accepted-button mask		*/
	int	lblSquare;	/* +0x40				*/
	int	lblCross;	/* +0x44				*/
	int	lblCircle;	/* +0x48				*/
	int	lblTriangle;	/* +0x4c				*/
	struct Vt *vt;		/* +0x50				*/
} Page;

/* a page's optional behaviour overrides; a non-zero return from a hook
 * means "handled, do not run the default" */
typedef struct Vt {
	int	(*open)(void);		/* +0x00 */
	int	(*close)(void);		/* +0x04 */
	void	(*enter)(void);		/* +0x08 */
	void	(*square)(void);	/* +0x0c */
	int	(*cancel)(void);	/* +0x10 */
	int	(*up)(Page *);		/* +0x14 */
	int	(*down)(Page *);	/* +0x18 */
	int	(*activate)(void);	/* +0x1c */
	int	(*left)(void);		/* +0x20 */
	int	(*right)(void);		/* +0x24 */
	int	(*input)(Page *, int *, int *);	/* +0x28 */
	int	(*frame)(Page *);	/* +0x2c */
} Vt;

/* ------------------------------------------------------------------ */
/* $gp-reached globals (check.py masks the %gprel field, but the base
 * register has to be $28, so these must stay small-data eligible) */
extern int legendFlagA;		/* gp-30768, set by 0x21d748	*/
extern int legendFlagB;		/* gp-30772, set by 0x21d750	*/
extern Page *curPage;		/* gp-30624			*/
extern Page *curPopup;		/* gp-30632			*/
extern int fadeMode;		/* gp-28828			*/
extern int fadeAlpha;		/* gp-28824			*/
extern unsigned int modelMode;	/* gp-30352			*/
extern int pageIndex;		/* gp-28844			*/

/* absolute-address globals - incomplete arrays, see codegen law 10 */
extern int legendArrows[];	/* 0x27b5e0			*/
extern Anim clockAnim[];	/* 0x27c258			*/
extern Anim wizardAnim[];	/* 0x27da70			*/
extern int sysCfgOpen[];	/* 0x27be40			*/
extern int uiModel[];		/* 0x352880			*/
extern int itemPrevValue;	/* gp-28868			*/

/* prototypes for everything that is called before it is defined */
void setLegendFlagA(int);
void setLegendFlagB(int);
void setLegendArrows(int);
int  getPageIndex(void);
Anim *getClockAnim(void);
Anim *getWizardAnim(void);
int  anmTimer(Anim *);
int  anmScaled(Anim *, int);
int  anmIsState(Anim *, int);
int  anmDirty(Anim *);
void anmReset(Anim *);
void anmFadeIn(Anim *);
void anmFadeOut(Anim *);
void anmTick(Anim *);
int  getFadeAlpha(void);
int  getFadeMode(void);
int *uiModelField(int);
void modelFreeze(void);
void modelInvalidate(void);
void modelThaw(void);
void loadModelFromConfig(void);
void modelReload(void);
Page *getCurPage(void);
void pageResetScroll(void);
void clearCurPage(void);
void pageDispatch(int);
void postMsg3(int, int, int);		/* 0x221900		*/
void pageSetCursor(int);		/* 0x2226b8		*/
void itemInitDefault(Item *);		/* 0x222920		*/
void pageDispatchExtra(int);		/* 0x22c3c0		*/
int  popupScaled(void);
void popupClose(void);
void pageClose(void);
void pageCursorDown(void);
void pageCursorUp(void);
void pageEnter(void);
void pageSquare(void);
void pageItemCursorHook(int);
void pageItemChanged(void);
void pageItemInit(Item *);
void screenPageTick(void);

/* ------------------------------------------------------------------ */
/* 0x21d748 - the two legend-bar flags */
void
setLegendFlagA(int v)
{
	legendFlagA = v;
}

/* 0x21d750 */
void
setLegendFlagB(int v)
{
	legendFlagB = v;
}

/* 0x21d758 - which d-pad arrows the legend shows (a pad-button mask) */
void
setLegendArrows(int mask)
{
	legendArrows[0] = mask;
}

/* 0x21f980 - how far the popup's fade has got, 0..128 */
int
popupScaled(void)
{
	return anmScaled(&curPopup->anim, 128);
}

/* 0x21fa18 */
void
popupClose(void)
{
	if (anmIsState(&curPopup->anim, 2)) {
		anmFadeOut(&curPopup->anim);
		postMsg3(20992, 1, 10);
	}
}

/* 0x221908 */
Page *
getCurPage(void)
{
	return curPage;
}

/* 0x2219d0 */
void
pageResetScroll(void)
{
	curPage->scroll = 0;
}

/* 0x221b68 */
void
clearCurPage(void)
{
	curPage = 0;
}

/* 0x221b70 - the page's "Back" action */
void
pageClose(void)
{
	if (curPage->vt && curPage->vt->close && curPage->vt->close())
		return;
	if (!anmIsState(&curPage->anim, 2))
		return;
	anmFadeOut(&curPage->anim);
}

/* 0x221bd8 */
void
pageCursorDown(void)
{
	int c;

	if (curPage->vt && curPage->vt->down && curPage->vt->down(curPage))
		return;
	c = curPage->cursor + 1;
	pageSetCursor(c < curPage->nitems ? c : 0);
}

/* 0x221c38 */
void
pageCursorUp(void)
{
	int c;

	if (curPage->vt && curPage->vt->up && curPage->vt->up(curPage))
		return;
	c = curPage->cursor - 1;
	if (c < 0)
		c = curPage->nitems;
	pageSetCursor(c);
}

/* 0x221c98 */
void
pageEnter(void)
{
	if (curPage->vt && curPage->vt->enter)
		curPage->vt->enter();
}

/* 0x221d40 */
void
pageSquare(void)
{
	if (curPage->vt && curPage->vt->square)
		curPage->vt->square();
}

/* 0x222798 - tell the focused row the cursor moved onto/off it */
void
pageItemCursorHook(int arg)
{
	Item *it;

	it = &curPage->items[curPage->cursor];
	if (it->cursorHook)
		it->cursorHook(it, arg);
}

/* 0x2228b0 */
void
pageItemChanged(void)
{
	Item *it;

	it = &curPage->items[curPage->cursor];
	if (it->changed)
		it->changed(it, itemPrevValue);
}

/* 0x2228f0 */
void
pageItemInit(Item *it)
{
	if (!it->init)
		itemInitDefault(it);
	else
		it->init(it);
}

/* 0x223790 */
Anim *
getClockAnim(void)
{
	return clockAnim;
}

/* 0x224d68 */
Anim *
getWizardAnim(void)
{
	return wizardAnim;
}

/* 0x226948 */
int
getPageIndex(void)
{
	return pageIndex;
}

/* 0x226a48 */
int
isSysCfgOpen(void)
{
	return sysCfgOpen[0] == 1;
}

/* 0x2283d0 - pages 5.. are drawn by a separate handler */
void
screenPageTick(void)
{
	int p;

	p = pageIndex;
	if ((u32)p >= 5)
		pageDispatchExtra(p - 5);
}

/* 0x22ac18 */
int
anmTimer(Anim *a)
{
	return a->timer;
}

/* 0x22ac20 - interpolate n over the fade */
int
anmScaled(Anim *a, int n)
{
	return a->timer * n / a->duration;
}

/* 0x22ac48 */
int
anmIsState(Anim *a, int s)
{
	return a->state == s;
}

/* 0x22ac58 */
int
anmDirty(Anim *a)
{
	return a->dirty;
}

/* 0x22ac60 */
void
anmReset(Anim *a)
{
	a->timer = 0;
	a->dirty = 0;
	a->state = 0;
}

/* 0x22ac70 */
void
anmFadeIn(Anim *a)
{
	if (a->state == 0) {
		a->timer = 0;
		a->dirty = 1;
		a->state = 1;
	}
}

/* 0x22ac90 */
void
anmFadeOut(Anim *a)
{
	if (a->state == 2) {
		a->timer = a->duration;
		a->dirty = 1;
		a->state = 3;
	}
}

/* 0x22acc0 - one frame of the fade */
void
anmTick(Anim *a)
{
	a->dirty = 0;
	if (a->state == 1)
		goto in;
	if (a->state == 3)
		goto out;
	return;
in:
	if (++a->timer == a->duration) {
		a->dirty = 1;
		a->state = 2;
	}
	return;
out:
	if (--a->timer == 0) {
		a->dirty = 1;
		a->state = 0;
	}
}

/* 0x22ad28 */
int
getFadeAlpha(void)
{
	return fadeAlpha;
}

/* 0x22ad30 */
int
getFadeMode(void)
{
	return fadeMode;
}

/* 0x22b0e8 - the UI model is a plain word array at 0x352880; an item's
 * `field' is its word index */
int *
uiModelField(int i)
{
	return &uiModel[i];
}

/* 0x22b100 - stop refreshing the model from NVM while the user edits */
void
modelFreeze(void)
{
	modelMode = 0;
}

/* 0x22b108 */
void
modelInvalidate(void)
{
	modelMode = 0xffffffff;
}

/* 0x22b118 */
void
modelThaw(void)
{
	modelMode = 1;
}

/* 0x22b128 */
void
modelReload(void)
{
	modelMode = 1;
	loadModelFromConfig();
}
