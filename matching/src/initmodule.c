/* 0x21CE58 - the menu module's one-shot init, reconstructed as ONE
 * function (the osdbits port split it as menu:InitMenuScene +
 * menuback:InitMenuBackdrop + menuconfig:InitMenuConfig). */

extern float menuCamZOffset;		/* gp-28880 */

void ClearOrbs(void);			/* 0x22EE88 */
void ClearPadState(void);		/* 0x22BE18 */
void InitClock(void);			/* 0x22B838 */
void ReloadUiModel(void);		/* 0x22B128 */
void InitDurations(void);		/* 0x22AD38 */
void InitKabe(void);			/* 0x229698 */
void InitTextureSlot(int slot);		/* 0x22A9B8 */
void LoadFont(void);			/* 0x21DBA0 */
void InitOrbPhases(void);		/* 0x225998 */
void InitScreens(void);			/* 0x228460 */
void InitBgTimer(void);			/* 0x2287B0 */
void FadeArm(int mode);			/* 0x22ADD8 */

void
InitMenuModule(void)
{
	ClearOrbs();
	ClearPadState();
	InitClock();
	ReloadUiModel();
	InitDurations();
	InitKabe();
	InitTextureSlot(0);
	InitTextureSlot(1);
	InitTextureSlot(2);
	InitTextureSlot(3);
	InitTextureSlot(4);
	InitTextureSlot(5);
	InitTextureSlot(6);
	InitTextureSlot(7);
	InitTextureSlot(8);
	InitTextureSlot(9);
	LoadFont();
	InitOrbPhases();
	InitScreens();
	InitBgTimer();
	FadeArm(2);
	menuCamZOffset = -100.0f;
}
