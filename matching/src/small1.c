/* small1.c - Module U small-function sweep, batch 1 (m-leaf agent).
 * Convenience TU like leaf.c: functions from several retail TUs,
 * check.py scores each independently.
 *
 *	sh ref/try.sh src/small1.c small1-functions.txt
 */

typedef unsigned int u_long128 __attribute__((mode(TI)));
typedef float FMATRIX[4][4];

/* ---- $gp globals -------------------------------------------------- */
extern int gp_882c;			/* gp-30676 (0x21ee50)	*/
extern int *listPtr_8858;		/* gp-30632 (0x2202c8)	*/
extern float gpf_8260;			/* gp-32160 (0x225cf0)	*/
extern int gp_890c;			/* gp-30452 (0x225cf0)	*/
extern int gp_8954;			/* gp-30380 (0x2272f0)	*/
extern int gp_8940;			/* gp-30400 (0x2272f0)	*/
extern int gp_8944;			/* gp-30396 (0x2272f0)	*/

/* ---- absolute globals --------------------------------------------- */
extern int abs_27BE5C[];		/* 0x27be5c		*/
extern int tmr_27BF50[];		/* 0x27bf50		*/
extern int tmr_27C258[];		/* 0x27c258		*/
extern int tmr_27EC00[];		/* 0x27ec00		*/
extern int tmr_27BE44[];		/* 0x27be44		*/
extern int tmr_27BEA8[];		/* 0x27bea8		*/
extern u_long128 tag_27F2D0[];		/* 0x27f2d0		*/
extern u_long128 tag_27F2E0[];		/* 0x27f2e0		*/

typedef struct {
	int a, b, c;
	char pad[1604];
} Ent1616;				/* 1616 bytes */
extern Ent1616 ents_27F950[];		/* 0x27f950, 10 entries	*/

typedef struct {
	int a;
	float b;
	char pad[236];
	float c;			/* +244 */
} Scene;
extern Scene scene_34E980[];		/* 0x34e980		*/

typedef struct {
	int f28obj[1];
} PtrObj;

/* ---- callees ------------------------------------------------------ */
extern void fn_21DFF8(int, int, int, int, int);
extern void fn_22B3F8(void);
extern void fn_21EDB8(void);
extern void timerStep(int *);		/* 0x22acc0 */
extern void timerOpen(int *);		/* 0x22ac70 */
extern void timerClose(int *);		/* 0x22ac90 */
extern int timerIsState(int *, int);	/* 0x22ac48 */
extern void fn_21FA68(void);
extern void fn_21FAD0(void);
extern void fn_220270(void);
extern void fn_2211B0(void);
extern void fn_221230(void);
extern void fn_2215E0(void);
extern void fn_2272F0(void);
extern void fn_224118(int, int, int);
extern void fn_226CF8(void);
extern void fn_226D00(void);
extern void fn_227390(void);
extern void fn_227560(void);
extern void fn_227D08(void);
extern void fn_228050(void);
extern void fn_228110(void);
extern void fn_228278(void);
extern void fn_2287A8(void);
extern int *fn_22B0E8(int);
extern int fn_2036F8(void);
extern int fn_2040D0(void);
extern int fn_224D68(void);
extern void sceVu0MulMatrix(FMATRIX, FMATRIX, FMATRIX);

/* ------------------------------------------------------------------ */
/* 0x21e3b0 - pass-through thunk adding a constant 5th argument */
void thunk_21E3B0(int a, int b, int c, int d)
{
	fn_21DFF8(a, b, c, d, 1);
}

/* 0x21f150 - void tail-call thunk */
void thunk_21F150(void) { fn_22B3F8(); }

/* 0x21f158 / 0x21f160 / 0x220798 - empty */
void nullsub_21F158(void) {}
void nullsub_21F160(void) {}
void nullsub_220798(void) {}

/* 0x21ee50 - call, then clear one $gp flag and one absolute flag */
void clr_21EE50(void)
{
	fn_21EDB8();
	gp_882c = 0;
	abs_27BE5C[0] = 0;
}

/* 0x2202c8 - step a timer inside a pointed-to object, two calls, tail */
void step_2202C8(void)
{
	timerStep(listPtr_8858 + 7);
	fn_21FA68();
	fn_21FAD0();
	fn_220270();
}

/* 0x2217a8 - step a static timer, two calls, tail */
void step_2217A8(void)
{
	timerStep(tmr_27BF50);
	fn_2211B0();
	fn_221230();
	fn_2215E0();
}

/* 0x223680 - close a timer, one call, 3-arg tail */
void close_223680(void)
{
	timerClose(tmr_27C258);
	fn_2272F0();
	fn_224118(0x5200, 1, 4);
}

/* 0x225cf0 - reset the scene record.  MATCH with b before c (the
 * float-load statement before the -1.0f store; sweep probe_s1.c) */
void reset_225CF0(void)
{
	scene_34E980[0].a = 0;
	scene_34E980[0].b = gpf_8260;
	scene_34E980[0].c = -1.0f;
	gp_890c = 0;
}

/* 0x225d18 - depth of a node: multiply parent by local, take [3][2] */
typedef struct {
	char pad0[32];
	FMATRIX m;			/* +32 */
	int pad96;
	float (*parent)[4];		/* +100 */
} SceneNode;

float depth_225D18(SceneNode *n)
{
	FMATRIX r;

	sceVu0MulMatrix(r, n->parent, n->m);
	return r[3][2];
}

/* 0x226950 - carousel idle test */
int idle_226950(void)
{
	return timerIsState((int *)fn_224D68(), 0) == 0;
}

/* 0x226fa8 - step a static timer, one call, tail */
void step_226FA8(void)
{
	timerStep(tmr_27EC00);
	fn_226CF8();
	fn_226D00();
}

/* 0x227de8 - step a static timer, two calls, tail */
void step_227DE8(void)
{
	timerStep(tmr_27BE44);
	fn_227390();
	fn_227560();
	fn_227D08();
}

/* 0x2283a0 - step a static timer, two calls, tail */
void step_2283A0(void)
{
	timerStep(tmr_27BEA8);
	fn_228050();
	fn_228110();
	fn_228278();
}

/* 0x229a18 - push a 24-byte node on a packet list.  NOTE: the node must
 * NOT contain a u_long128 member (16-byte alignment would pad it to 32;
 * the ROM advances by 24) - the qword is stored through a cast. */
typedef struct {
	int t[4];
	int a, b;
} Node24;

typedef struct {
	int t[4];
} Node16;

typedef struct {
	char *cur;
	char pad[16];
	void *last;			/* +20 */
} PList;

void push_229A18(PList *q)
{
	Node24 *p = (Node24 *)q->cur;
	u_long128 t = tag_27F2D0[0];

	/* close (7 diffs, right insn count): register-naming tie, no
	 * statement order or temp shape found that moves it */
	p->a = 76;
	q->cur = (char *)(p + 1);
	*(u_long128 *)p = t;
	q->last = p;
	p->b = 0;
}

/* 0x229a48 - push a 16-byte node on a packet list.  close (2 diffs):
 * only the initial lui/lw pair is swapped - in-block sched tie */
void push_229A48(PList *q)
{
	Node16 *p = (Node16 *)q->cur;
	u_long128 t = tag_27F2E0[0];

	q->last = p;
	q->cur = (char *)(p + 1);
	*(u_long128 *)p = t;
}

/* 0x229a70 - clamp.  FAR (best 4/13): the ROM has a 16-byte frame with
 * an undefined `sw $2,0($sp)' right after the prologue - an
 * uninitialized-pseudo spill no shape tried reproduces (same artifact
 * as 0x22b270; probably the same retail TU).  Open idiom question. */
int clamp_229A70(int a, int lo, int hi)
{
	if (a < lo)
		a = lo;
	else if (hi < a)
		a = hi;
	return a;
}

/* 0x22b270 - binary to BCD.  FAR (best 7/14): same undefined
 * `sw $2,0($sp)' frame artifact as 0x229a70, plus a dead li a0,10. */
unsigned char bcd_22B270(int a)
{
	return a / 10 * 16 + a % 10;
}

/* 0x22fe88 - clear three ints in each of ten 1616-byte entries.
 * MATCH as do-while with an (int) pointer compare: that is what makes
 * the ROM's loop test a SIGNED slt with no entry guard. */
void clear_22FE88(void)
{
	Ent1616 *p;

	p = ents_27F950;
	do {
		p->a = 0;
		p->c = 0;
		p->b = 0;
		p++;
	} while ((int)p < (int)&ents_27F950[10]);
}

/* 0x21ec98 - compare a stored id against the current one, resync */
typedef struct {
	int pad0, pad4, pad8;
	int key;			/* +12 */
} KeyObj;

void chk_21EC98(KeyObj *k)
{
	if (*fn_22B0E8(k->key) != fn_2036F8())
		fn_22B3F8();
}

/* 0x21ecd8 - same against the other id source */
void chk_21ECD8(KeyObj *k)
{
	if (*fn_22B0E8(k->key) != fn_2040D0())
		fn_22B3F8();
}

/* 0x224ff0 - fetch a row value into the keyed slot */
typedef struct {
	int pad0, pad4;
	int idx;			/* +8 */
	int key;			/* +12 */
	int *table;			/* +16 */
} RowObj;

void fetch_224FF0(RowObj *t)
{
	*fn_22B0E8(t->key) = *(int *)((char *)t->table + t->idx * 48);
}

/* 0x226b28 / 0x226b70 / 0x227f08 - conditional open/close of timers */
void open_226B28(void)
{
	if (timerIsState(tmr_27EC00, 0))
		timerOpen(tmr_27EC00);
}

void close_226B70(void)
{
	if (timerIsState(tmr_27EC00, 2))
		timerClose(tmr_27EC00);
}

void open_227F08(void)
{
	if (timerIsState(tmr_27BEA8, 0))
		timerOpen(tmr_27BEA8);
}

/* 0x2272f0 - arm the scroll record, then tail-call */
typedef struct {
	char pad0[28];
	int end;			/* +28 */
	int from;			/* +32 */
	char pad36[4];
	int flag;			/* +40 */
	char pad44[8];
	int state;			/* +52 */
} Scroll;
extern Scroll scr_27BE28[];		/* 0x27be28 */

void arm_2272F0(void)
{
	scr_27BE28[0].from = gp_8954 - 1;
	scr_27BE28[0].end = gp_8954 + gp_8940 + gp_8944;
	scr_27BE28[0].flag = 1;
	scr_27BE28[0].state = 0;
	fn_224118(0x5200, 1, 4);
}
