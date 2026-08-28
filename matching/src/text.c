/* the text-overlay state machines (0x214c20..0x214f74) -
 * compile with ee-gcc 2.9-ee-991111 -O2 */

extern float openingPosition[4];

int anotherOpeningType;
int sceTextState;
int sceTextAlpha;
int sceTextStep;
int illegalTextState;
int illegalTextAlpha;

extern void DrawSCEText(int x, int y, int alpha);
extern void DoIllegalText(void);

/* 0x214c20 - MATCHES except two source-shape deltas (the alpha
 * register a1 vs a3, the min() slt/movz idiom) */
void
DoSCEText(void)
{
	int alpha;

	if(openingPosition[2] > 18.0f) {
		if(sceTextState == 0)
			sceTextState = 1;
	}
	if(sceTextState != 1)
		return;
	alpha = sceTextAlpha + sceTextStep;
	sceTextAlpha = alpha;
	if(alpha == 240)
		sceTextStep = -4;
	if(alpha == 0) {
		sceTextStep = 4;
		sceTextState = -1;
	}
	DrawSCEText(0, 0, 112 < alpha ? 112 : alpha);
}

/* 0x214f20 - MATCHES */
void
initTextShit(void)
{
	if(anotherOpeningType == 0) {
		sceTextState = 0;
		illegalTextState = -1;
	} else {
		illegalTextState = 0;
		sceTextState = -1;
	}
	sceTextAlpha = 0;
	sceTextStep = 4;
	illegalTextAlpha = 0;
}

/* 0x214f58 */
void
DoText(void)
{
	DoSCEText();
	DoIllegalText();
}
