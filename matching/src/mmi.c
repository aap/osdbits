/* probe for 0x228278 v2 - shared sound tail via goto */
typedef struct Anim { int duration, timer, dirty, state; } Anim;
typedef struct MenuItem { int strid; void *aux; } MenuItem;
typedef struct Hdr {
	int title; MenuItem *items; int nitems; int rows;
	int cursor; int top; Anim anim;
} Hdr;
typedef struct Cam {
	float scale; int busy; int field; float w; float h;
} Cam;

extern Hdr mainHdr;		/* 0x27be90 */
extern Cam camCtx;		/* 0x27b440 */
extern int padWord;		/* gp-30316 */

extern int timerIsState(Anim *, int);	/* 0x22ac48 */
extern int noOtherScreen(void);		/* 0x227fc0 */
extern void soundThunk(int, int, int);	/* 0x2287a8 */
extern int getFadeState(void);		/* 0x22ad30 */
extern void enterItem0(int);		/* 0x227f50 */
extern void enterItem1(void);		/* 0x227268 */
extern void leaveMenu(void);		/* 0x2210c8 */



void
MainMenuInput(void)
{
	int pad;

	if(!timerIsState(&mainHdr.anim, 2))
		return;
	if(!noOtherScreen())
		return;
	if(camCtx.busy != 0)
		return;
	pad = padWord;
	if(pad & 0x1000) {
		int cur, nc;

		cur = mainHdr.cursor;
		nc = cur - 1;
		mainHdr.cursor = nc;
		if(nc >= 0)
			goto sound;
		mainHdr.cursor = cur;
	} else if(pad & 0x4000) {
		int cur, nc;

		cur = mainHdr.cursor;
		nc = cur + 1;
		mainHdr.cursor = nc;
		if(nc < mainHdr.nitems)
			goto sound;
		mainHdr.cursor = cur;
		goto out;
sound:
		soundThunk(20992, 1, 6);
	} else if(pad & 0x20) {
		if(mainHdr.cursor == 0 && getFadeState() == 0)
			enterItem0(0);
		else if(mainHdr.cursor == 1)
			enterItem1();
	} else if(pad & 0x10)
		leaveMenu();
out:;
}
