/* the opening's camera state machine / thread TU
 * (0x211d30..0x2121c0 and 0x215f18..0x2166c8) -
 * compile with ee-gcc 2.9-ee-991111 -O2.
 *
 *	ee-gcc -O2 -c src/anim.c -o build/anim.o
 *	python3 check.py build/anim.o <expanded.bin> anim-functions.txt
 *
 * Status: 9/11 functions match instruction for instruction.
 *   OpeningThread            30/32 - openingType's temp lands in v1,
 *                                    the ROM has v0 (allocation tie)
 *   ProcessOpeningAnimation 442/446 - the position[1]/position[2]
 *                                    mul.s pairs are scheduled in the
 *                                    other order (equal-priority tie
 *                                    in sched2); everything else,
 *                                    including all three jump tables
 *                                    and the whole matrix tail, is
 *                                    identical.
 *
 * DATA MODEL (all of this was *derived* from the codegen, not assumed):
 *
 *  - 0x323af0 is ONE object: a single %hi is shared by every field and
 *    the fields are addressed as displacements off one %lo register.
 *      struct Anim { FVECTOR accel1, accel2, speed, rotAccel, rotSpeed;
 *                    int state; }   (0x00,0x10,0x20,0x30,0x40,0x50)
 *    It is a linker symbol, not a fixed address: the ROM materialises
 *    it with lui+addiu (%hi/%lo), while a constant address compiles to
 *    lui+ori.
 *  - position/fwdDir/upDir/light1..3 are SEPARATE 16-byte symbols -
 *    each reference re-materialises its own %hi.
 *  - the low-RAM block at 0x1f0000 is a struct at a CONSTANT ADDRESS
 *    (that is what lets one lui serve 0xc50+0xc54, and what makes the
 *    alias analysis keep the ra save ahead of the load), and its
 *    system fields are VOLATILE: bootParamC/bootParam/bootMode/
 *    systemState/systemFlag.  dispEnv1/dispEnv2 (the GS display words)
 *    and bootParam2/frameCounter/screenW/screenH are not - volatile
 *    would forbid the interleaved 64-bit read-modify-writes in
 *    InitOpening, and it changes whether a load may reuse its own
 *    address register (volatile: separate regs; plain: reuse).
 *  - float constants are plain literals.  ee-as puts each one in
 *    .lit4 and addresses it gp-relative, and it does NOT dedupe within
 *    a function, which is exactly why 0.0004 and 6.2831853 each get
 *    two pool slots in the ROM (0x2a70d4+0x2a70e8, 0x2a70fc+0x2a7104).
 *    Constants whose low half is zero (1.0, 0.5, 2048.0, 65536.0)
 *    come out as lui+mtc1 instead - also automatic.
 *  - the dispatch helpers are unprototyped and return int.  Both
 *    matter: no prototype is why OSDDispatch(20500,1) leaves a2/a3
 *    alone, and the discarded int return is what makes the register
 *    allocator split case 6's two "endFrame = frameCount" tails so
 *    that cross-jumping does not merge them (as in the ROM).
 *
 * CODEGEN QUIRKS THIS FILE HAD TO BE SHAPED AROUND:
 *  - a group of independent stores comes out with the LAST one first
 *    and the rest in source order, so e.g. OpeningInitAnimation has to
 *    write accel1[2] last to get the ROM's ascending 8,16,20,24,...
 *  - a switch that starts at case 1 gets an "addiu -1" before the
 *    bounds check; the ROM has none, so the switch really does have a
 *    "case 0:" (and case 6's parameter switch really does list 116,
 *    which shares the default's code, to make its table 17 entries).
 *  - case bodies are emitted in source order, which pins down the
 *    order the original listed them in (sub_212010's 114,112,111,110,
 *    108/109,106/107,115,116 and the 108/109/110-before-106/107 pair
 *    in ProcessOpeningAnimation).
 *  - "fps" must be a block-scoped local: (IsPAL()?50:60)*20/6 folds to
 *    a constant compare, and one function-wide fps gets different
 *    registers than the ROM's.
 */

typedef float FVECTOR[4];

struct Anim {
	FVECTOR accel1;		/* 0x00 */
	FVECTOR accel2;		/* 0x10 */
	FVECTOR speed;		/* 0x20 */
	FVECTOR rotAccel;	/* 0x30 */
	FVECTOR rotSpeed;	/* 0x40 */
	int state;		/* 0x50 */
};

struct Matrices {
	float pad0[48];
	float cameraScreen[16];	/* 192 */
	float camera[16];	/* 256 */
	float viewScreen[16];	/* 320 */
	float pad1[32];
	float normalLight[16];	/* 512 */
};

struct Rect {
	int x, y, w, h;
};

/* the block at 0x1f0000, a struct at a constant address with volatile
 * system fields (see the header comment).  volatile pins the load in
 * InitOpeningType, which is the only way its bnel/annulled-store shape
 * appears; the constant address is what makes the scheduler keep the
 * ra save ahead of that load. */
struct Sys {
	int pad0[3];
	volatile int bootParamC;	/* 0x00c */
	volatile int bootParam;		/* 0x010 */
	volatile int bootMode;		/* 0x014 */
	int pad1[372];
	volatile int systemState;	/* 0x5e8 */
	volatile int systemFlag;	/* 0x5ec */
	int pad2[284];
	unsigned long long dispEnv1;	/* 0xa60 */
	int pad3[58];
	unsigned long long dispEnv2;	/* 0xb50 */
	int pad4[58];
	int frameCounter;	/* 0xc40 */
	int pad5[3];
	int screenW;		/* 0xc50 */
	int screenH;		/* 0xc54 */
	int pad6[40];
	int bootParam2;		/* 0xcf8 */
};

#define sys (*(struct Sys*)0x1f0000)
extern struct Anim anim;
extern struct Matrices *sprMatrices;

extern int openingStateLevels[8];
extern FVECTOR position, fwdDir, upDir;
extern FVECTOR light1, light2, light3;

extern unsigned int openingFrameCount;
extern int openingType;
extern int nextOpeningType;
extern int anotherOpeningType;
extern int sceneState;
extern int drawBlackBars;
extern float screenAX, screenAY;
extern int bootRequest;
extern int openingGo;
extern float rotation;
extern int lastBootParam;
extern int openingEndFlag;
extern unsigned int openingEndFrame;

extern int threadIds[8];	/* 0x26fe20; +20 is woken at scene end */

extern int IsPAL(void);			/* 0x204350 */
extern int HasDisc(void);		/* 0x2043a8 */
extern int GetDiscType(void);		/* 0x2043b8 */
extern int BootLatchClear(void);	/* 0x204378 */
extern int *DiscFlagPtr(void);		/* 0x2043c8 */
extern int GetVideoMode(void);		/* 0x203690 */
extern int OSDDispatch();		/* 0x200b80 - no prototype */
extern int OSDDispatch2();		/* 0x261738 - no prototype */
extern int PostStatus();		/* 0x20b780 */
extern int SleepThread(void);		/* 0x24d980 */
extern int WakeupThread(int id);	/* 0x24da80 */
extern void InitRender(void);		/* 0x212258 */
extern void InitTowersFog(void);	/* 0x218e00 */
extern void InitIllegalStuff(void);	/* 0x219f08 */
extern void initTextShit(void);		/* 0x214f20 */
extern void StartFrame(void);		/* 0x205e88 */
extern void DoText(void);		/* 0x214f58 */
extern void DrawBlackBars(void);	/* 0x214790 */
extern void WaitNextFrame(void);	/* 0x205f30 */
extern void DoOpening(void);		/* 0x219250 */
extern void DoIllegalDisc(void);	/* 0x219f40 */
extern void SetScissor(struct Rect *r);	/* 0x212d40 */
extern float sinf(float);		/* 0x253c08 */
extern float cosf(float);		/* 0x253a80 */
extern void sceVu0NormalLightMatrix(float *m, float *l0, float *l1, float *l2);
extern void sceVu0CameraMatrix(float *m, float *pos, float *fwd, float *up);
extern void sceVu0ViewScreenMatrix(float *m, float scrz, float ax, float ay,
	float cx, float cy, float nearz, float farz, float minz, float maxz);
extern void sceVu0MulMatrix(float *m, float *a, float *b);

/* NOTE: check.py reads *all* of ee-objdump -r's relocation tables, so the
 * .rodata jump-table relocations (R_MIPS_32 at small offsets) mask out the
 * instructions at the same offsets in .text.  These two .space blocks push
 * every real function clear of that range (and pad the tail so the last
 * function is long enough to compare). */
asm(".text\n\t.space 0x400");

void OpeningInitAnimation(void);
void ProcessOpening(void);
void DrawEnd(void);
void DoOpeningIllegal(void);
void InitOpeningType(void);
void sub_211fd8(void);
void sub_212010(void);
int ProcessOpeningAnimation(void);


/* 0x215f18 - MATCHES.  Note the store order: the LAST store of a group
 * is what ee-gcc's scheduler emits FIRST, so accel1[2] has to be the
 * last statement for the ROM's ascending 8,16,20,24,... order. */
void
OpeningInitAnimation(void)
{
	anim.state = 0;

	anim.speed[2] = 0.04f;
	anim.rotSpeed[2] = 0.001f;

	anim.accel2[0] = 0.0f;
	anim.accel2[1] = 0.0f;
	anim.accel2[2] = 0.0f;

	anim.speed[0] = 0.0f;
	anim.speed[1] = 0.0f;

	anim.rotAccel[0] = 0.0f;
	anim.rotAccel[1] = 0.0f;
	anim.rotAccel[2] = 0.0f;

	anim.rotSpeed[0] = 0.0f;
	anim.rotSpeed[1] = 0.0f;

	anim.accel1[2] = 0.0f;

	openingGo = 0;
}

/* 0x215f68 - MATCHES (same last-store-first rule: accel2[0] last) */
void
InitIllegalDisc(void)
{
	position[2] = 672.0f;
	anim.state = 4;

	anim.accel2[2] = 2.1599939f;
	anim.speed[2] = -0.0178f;
	anim.rotSpeed[2] = 0.00462f;

	anim.accel2[1] = 0.0f;

	anim.speed[0] = 0.0f;
	anim.speed[1] = 0.0f;

	anim.rotAccel[0] = 0.0f;
	anim.rotAccel[1] = 0.0f;
	anim.rotAccel[2] = 0.0f;

	anim.rotSpeed[0] = 0.0f;
	anim.rotSpeed[1] = 0.0f;

	anim.accel2[0] = 0.0f;

	openingGo = 0;
}

/* 0x215fd0 - 442/446.  Residual: the mul.s pairs for position[1] and
 * position[2] at 0x21655c/0x216560 and 0x216578/0x21657c come out
 * swapped - an equal-priority tie in the post-reload scheduler; the
 * arithmetic, the registers and every other instruction agree. */
int
ProcessOpeningAnimation(void)
{
	float timestep;
	int type;

	timestep = IsPAL() == 0 ? 1.0f : 1.2f;

	type = openingType;
	if((float)openingStateLevels[anim.state] < position[2])
		anim.state++;

	switch(anim.state) {
	case 0:
		break;
	case 1:
		if(HasDisc()) {
			int fps;
			anim.rotSpeed[2] = 0.0004f;
			fps = IsPAL() ? 50 : 60;
			if(openingFrameCount < fps*20/6)
				anim.accel2[2] = -0.00014f;
			else
				anim.accel2[2] = 0.000025f;
			if(GetDiscType() != 0) {
				anim.accel2[2] = 0.003f;
				openingGo = 1;
				anim.state++;
			} else {
				int fps = IsPAL() ? 50 : 60;
				if(openingFrameCount > fps*20)
					*DiscFlagPtr() = 0;
			}
		} else {
			anim.accel1[2] = 0.0000004f;
			switch(sys.bootParam) {
			case 100:
			case 106: case 107: case 108: case 109: case 110:
			case 111: case 112: case 114: case 115: case 116:
				{
				int fps = IsPAL() ? 50 : 60;
				if(openingFrameCount > fps*2) {
					openingGo = 1;
					anim.state++;
				}
				}
				break;
			}
		}
		break;
	case 2:
	    {
		int fps = IsPAL() ? 50 : 60;
		if(openingFrameCount > fps*10)
			openingGo = 1;
		if(openingGo) {
			if(bootRequest == 1) {
				if(BootLatchClear())
					OSDDispatch(20500, 1);
				else if(HasDisc() && GetDiscType() == 1)
					OSDDispatch(20501, 0, 0, 15);
				else {
					int param = sys.bootParam;
					lastBootParam = param;
					switch(param) {
					case 108: case 109: case 110:
						OSDDispatch(20500, 7);
						OSDDispatch(20501, 0, 0, 17);
						break;
					case 106: case 107: case 114: case 115:
						OSDDispatch(20501, 0, 0, 15);
						break;
					default:
						OSDDispatch(20500, 1);
						break;
					}
				}
				bootRequest = -1;
			}
			if(HasDisc() || GetDiscType() != 0) {
				anim.accel1[2] = 0.0004f;
				anim.rotAccel[2] = 0.00008f;
				anim.accel2[1] = 0.0f;
				anim.rotAccel[0] = 0.0f;
				anim.rotAccel[1] = 0.0f;
				anim.accel2[0] = 0.0f;
			} else {
				anim.accel2[2] = 0.0099f;
				anim.rotAccel[2] = 0.000195f;
				anim.accel2[1] = 0.0f;
				anim.rotAccel[0] = 0.0f;
				anim.rotAccel[1] = 0.0f;
				anim.accel2[0] = 0.0f;
			}
		}
	    }
		break;
	case 3:
		type++;
		break;
	case 6:
		anim.speed[0] = 0.0f;
		anim.speed[1] = 0.0f;
		anim.speed[2] = 0.0f;
		anim.accel2[0] = 0.0f;
		anim.accel2[1] = 0.0f;
		anim.accel2[2] = 0.0f;
		anim.accel1[0] = 0.0f;
		anim.accel1[1] = 0.0f;
		anim.accel1[2] = 0.0f;
		anim.rotAccel[0] = 0.0f;
		anim.rotAccel[1] = 0.0f;
		anim.rotAccel[2] = 0.0f;
		if(sys.bootParamC == 0) {
			int param = sys.bootParam;
			lastBootParam = param;
			switch(param) {
			case 100:
			case 106: case 107: case 108: case 109: case 110:
			case 111: case 112: case 115:
				openingEndFlag = 1;
				if(openingEndFrame == 0) {
					OSDDispatch2(1, 20501, 6, 0, 15);
					openingEndFrame = openingFrameCount;
				} else if(openingFrameCount > openingEndFrame+128)
					type = 2;
				break;
			case 114:
				if(sys.bootParam2 > 0) {
					openingEndFlag = 1;
					if(openingEndFrame == 0)
						openingEndFrame = openingFrameCount;
					else if(openingFrameCount > openingEndFrame+128)
						type = 2;
				}
				break;
			case 116:
			default:
				if(openingEndFlag &&
				   openingFrameCount > openingEndFrame+128)
					type = 2;
				break;
			}
		}
		break;
	case 7:
		OpeningInitAnimation();
		type = 2;
		break;
	}

	anim.rotSpeed[0] += (2.0f*anim.rotAccel[0] + 0.0f)*0.5f*timestep;
	anim.rotSpeed[1] += (2.0f*anim.rotAccel[1] + 0.0f)*0.5f*timestep;
	anim.rotSpeed[2] += (2.0f*anim.rotAccel[2] + 0.0f)*0.5f*timestep;

	anim.speed[0] += (2.0f*anim.accel2[0] + anim.accel1[0])*0.5f*timestep;
	anim.speed[1] += (2.0f*anim.accel2[1] + anim.accel1[1])*0.5f*timestep;
	anim.speed[2] += (2.0f*anim.accel2[2] + anim.accel1[2])*0.5f*timestep;

	anim.accel2[2] += anim.accel1[2]*timestep;

	rotation += (2.0f*anim.rotSpeed[2] + anim.rotAccel[2])*0.5f*timestep;

	position[0] += (2.0f*anim.speed[0] + anim.accel2[0])*0.5f*timestep;
	position[1] += (2.0f*anim.speed[1] + anim.accel2[1])*0.5f*timestep;
	position[2] += (2.0f*anim.speed[2] + anim.accel2[2])*0.5f*timestep;

	if(rotation > 3.14159265f)
		rotation -= 6.28318531f;
	if(rotation < -3.14159265f)
		rotation += 6.28318531f;

	upDir[0] = sinf(rotation);
	upDir[1] = cosf(rotation);

	sceVu0NormalLightMatrix(sprMatrices->normalLight, light1, light2, light3);
	sceVu0CameraMatrix(sprMatrices->camera, position, fwdDir, upDir);
	sceVu0ViewScreenMatrix(sprMatrices->viewScreen, 1024.0f,
		screenAX, screenAY, 2048.0f, 2048.0f,
		1.0f, 16777215.0f, 1.0f, 65536.0f);
	sceVu0MulMatrix(sprMatrices->cameraScreen, sprMatrices->viewScreen,
		sprMatrices->camera);

	return type;
}

/* 0x211d30 - 30/32.  Residual: the openingType temp gets v1 where the
 * ROM has v0 (and the ROM then uses v1 for the thread-id address).
 * Everything else, delay slots included, is identical; sweeping the
 * signature, the if/else sense and the callees' return types did not
 * flip the pair. */
void
OpeningThread(void *arg)
{
	for(;;) {
		SleepThread();
		InitOpeningType();
		sub_211fd8();
		InitOpening();
		if(openingType == 0)
			PostStatus(0);
		else
			PostStatus(1);
		DoOpeningIllegal();
		sub_212010();
		PostStatus(1);
		WakeupThread(threadIds[5]);
	}
}

/* 0x211db0 - MATCHES */
void
InitOpening(void)
{
	openingType = nextOpeningType = anotherOpeningType;
	InitRender();
	sys.dispEnv1 = (sys.dispEnv1 & ~0x7fffULL) | 14;
	sys.dispEnv2 = (sys.dispEnv2 & ~0x7fffULL) | 14;
	OpeningInitAnimation();
	InitTowersFog();
	InitIllegalStuff();
	initTextShit();
	StartFrame();
	openingFrameCount = sys.frameCounter;
}

/* 0x211e38 - MATCHES.  The Rect is a non-constant aggregate
 * initialiser: gcc builds it in a temp at 16(sp) and block-copies it
 * to the variable with ldl/ldr/sdl/sdr (alignment 4). */
void
ProcessOpening(void)
{
	struct Rect rect = { 1, 1, sys.screenW-2, sys.screenH-2 };

	if(ProcessOpeningAnimation() != openingType)
		sceneState++;
	SetScissor(&rect);
}

/* 0x211eb8 - MATCHES */
void
DrawEnd(void)
{
	DoText();
	if(drawBlackBars)
		DrawBlackBars();
	WaitNextFrame();
	openingFrameCount++;
}

/* 0x211f00 - MATCHES (IDB calls it DoOpeningIllegal) */
void
DoOpeningIllegal(void)
{
	sceneState = 0;
	while(openingType != 2) {
		ProcessOpening();
		switch(openingType) {
		case 0:
			DoOpening();
			break;
		case 1:
			DoIllegalDisc();
			break;
		}
		DrawEnd();
		openingType = nextOpeningType;
	}
}

/* 0x211f90 - MATCHES */
void
InitOpeningType(void)
{
	if(sys.systemState == 4)
		anotherOpeningType = 1;
	else
		anotherOpeningType = 0;
	if(GetVideoMode() == 1)
		drawBlackBars = 0;
	else
		drawBlackBars = 1;
}

/* 0x211fd8 - MATCHES.  case 4 has to be written before case 1:
 * the decision tree tests 1 first either way, but the bodies are
 * emitted in source order. */
void
sub_211fd8(void)
{
	switch(sys.systemState) {
	case 4:
		OSDDispatch(20500, 6);
		break;
	case 1:
		OSDDispatch(20500, 0);
		break;
	}
}

/* 0x212010 - MATCHES.  The case order below is the ROM's source
 * order (case bodies are emitted in source order). */
void
sub_212010(void)
{
	sys.bootMode = -1;
	if(BootLatchClear() && !HasDisc() && GetDiscType() != 1) {
		sys.systemState = 2;
		sys.systemFlag = 1;
		return;
	}
	switch(lastBootParam) {
	case 114:
		if(sys.bootParam2 > 0)
			sys.systemState = 5;
		else
			sys.systemState = 2;
		break;
	case 112:
		sys.bootMode = 4;
		break;
	case 111:
		sys.bootMode = 5;
		break;
	case 110:
		sys.bootMode = 0;
		break;
	case 108: case 109:
		sys.bootMode = 1;
		break;
	case 106: case 107:
		sys.bootMode = 2;
		break;
	case 115:
		sys.bootMode = 3;
		break;
	case 116:
		sys.systemState = 4;
		break;
	default:
		sys.systemState = 2;
		break;
	}
	if(HasDisc() && GetDiscType() == 1) {
		sys.systemState = 0;
		sys.systemFlag = 1;
		sys.bootMode = 6;
	}
	if(sys.bootMode == -1)
		sys.systemFlag = 1;
}

asm(".text\n\t.space 0x400");
