/* probe for 0x228110 DrawMainMenu */
typedef unsigned int u32;
typedef struct Anim { int duration, timer, dirty, state; } Anim;
typedef struct Color { int r, g, b, a; } Color;
typedef struct MenuItem { int strid; void *aux; } MenuItem;
typedef struct Hdr {
	int title; MenuItem *items; int nitems; int rows;
	int cursor; int top; Anim anim;
} Hdr;
typedef struct Cam {
	float scale; float f4; int field; float w; float h;
} Cam;

extern Hdr mainHdr;		/* 0x27be90 */
extern Cam camCtx;		/* 0x27b440 */
extern Color colSelected;	/* 0x27b830 */
extern Color colUnselected;	/* 0x27b840 */
extern int drawBufSelV[];	/* 0x1f0c40 */
extern int screenWV[];		/* 0x1f0c50 */
extern int screenHV[];		/* 0x1f0c54 */
#define drawBufSel	drawBufSelV[0]
#define screenW		screenWV[0]
#define screenH		screenHV[0]
#define DBUFF	((void *)0x1f0a10)

extern int mainMenuAlpha(int);			/* 0x227e18 */
extern int timerIsState(Anim *, int);		/* 0x22ac48 */
extern int noOtherScreen(void);			/* 0x227fc0 */
extern void setDrawBuf(void *, int, void *, int);/* 0x22a3b8 */
extern void setBlend(int, int);			/* 0x22a0c0 */
extern void osdTextSetScale(float);		/* 0x207f68 */
extern const char *osdGetString(int);		/* 0x2041b8 */
extern void drawTextC(int, int, const Color *, int, const char *);




void
DrawMainMenu(int fadeAlpha)
{
	int i, y, alpha;
	

	y = screenH/2 - 14;
	alpha = mainMenuAlpha(fadeAlpha);
	if(!timerIsState(&mainHdr.anim, 2))
		return;
	if(!noOtherScreen())
		return;
	{ int fld = camCtx.field; setDrawBuf(DBUFF, drawBufSel, 0, fld); }
	setBlend(1, 2);
	osdTextSetScale(1.0f);
	
	for(i = 0; i < mainHdr.nitems; i++) {
		if(i == mainHdr.cursor)
			drawTextC(430, y + i*16, &colSelected, alpha,
				osdGetString(mainHdr.items[i].strid));
		else
			drawTextC(430, y + i*16, &colUnselected, alpha,
				osdGetString(mainHdr.items[i].strid));
	}
}
