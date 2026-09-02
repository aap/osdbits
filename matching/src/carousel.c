/* Module U - the System Configuration carousel (12 glass rods).
 *   0x225628  CarouselClock - per-frame clock easing (sel/spin/tilt/split)
 *   0x225998  CarouselInit  - rebuild the ring + orb phases
 * Object layout proven from disassembly: ONE struct at 0x34E6C0
 * (header sel/spin/tilt + 12 x 48-byte rods at +0x10 + two current
 * qwords at +0x250 + split at +0x270).  Rod: progress at +0x00,
 * colour qword at +0x10.
 * gp-32172 (tiltRate) and gp-32168 are BOTH 0.1f: the first is a
 * variable, the second is the .lit4 pool entry for the literal 0.1f
 * written in the other three easing statements.
 */
typedef unsigned int u_long128 __attribute__((mode(TI)));

typedef struct Timer { int duration, count, edge, state; } Timer;

typedef struct Rod {
	float progress;		/* +0x00 */
	int f04;
	int f08, f0c;
	u_long128 colour;	/* +0x10 */
	int f20, f24, f28, f2c;
} Rod;

typedef struct Carousel {
	unsigned int sel;	/* +0x00 hour index 0..11 */
	unsigned short spin;	/* +0x04 */
	unsigned short tilt;	/* +0x06 */
	int f08, f0c;
	Rod rod[12];		/* +0x10 (0x34E6D0) */
	u_long128 curA;		/* +0x250 (0x34E910) */
	u_long128 curB;		/* +0x260 */
	float split;		/* +0x270 (0x34E930) */
} Carousel;

extern Carousel carousel[];		/* 0x34E6C0 */
extern Timer carouselTimer[];		/* 0x27EB00 */
extern u_long128 rodColourOff[];	/* 0x27EB10 */
extern u_long128 rodColourOn[];		/* 0x27EB20 */
extern int orbPhase[];			/* 0x34E960 */

extern int carouselDur;			/* gp-30372, = 1 */
extern float tiltRate;			/* gp-32172, 0.1f (variable) */
extern short hourAngle;			/* gp-28854 */
extern unsigned short secAngle;		/* gp-28856 */
extern float gSplit;			/* gp-28852 */

extern float ClockHours(void);		/* 0x22B720 */
extern float ClockSeconds(void);	/* 0x22B590 */
extern float ClockMinutes(void);	/* 0x22B640 */

extern int rand(void);			/* 0x25B478 */
void sub_225420(void);
void sub_225978(void);
void sub_22FE88(void);
void sub_22EE98(void);

/* 0x225628 - 145/148 aligned, 0 differing words; residual is two
 * scheduler placements (the tiltRate lwc1 slot and one unfilled jal
 * delay slot the compiler always fills) - the known schedule-tie class. */
void
CarouselClock(void)
{
	unsigned int n;
	int s;
	short d;

	n = (int)ClockHours() % 12;
	carousel->sel = n;
	s = (short)carousel->spin;
	s = s < 0 ? -s : s;
	if (s >= 201) {
		d = n * 65536 / 12 - carousel->tilt;
		carousel->tilt = d * tiltRate + (short)carousel->tilt;
	} else
		carousel->tilt = n * 65536 / 12;

	d = (int)(ClockSeconds() * 65536.0f / 60.0f) - carousel->spin;
	carousel->spin = d * 0.1f + (short)carousel->spin;

	d = (int)(ClockHours() * 65536.0f / 12.0f - hourAngle);
	hourAngle = d * 0.1f + hourAngle;

	d = (int)(ClockSeconds() * 65536.0f / 60.0f) - secAngle;
	secAngle = d * 0.1f + (short)secAngle;

	carousel->split = 1.0f - ClockMinutes() / 60.0f;
	gSplit = 1.0f - ClockMinutes() / 60.0f;
}

/* 0x225998 - structurally right (all 4 pieces present: timer duration,
 * the two current-colour qword stores, the 12-slot rebuild with the
 * (i+sel)%12 divu, the 7 orbPhase rand()%65536 stores, the two tail
 * calls) but a register-allocation/addressing plateau: the ROM keeps
 * &curA + &rodColourOn across the calls and derives the rod base from
 * curA-0x250; we do the mirror-image derivation.  38/78 aligned. */
void
CarouselInit(void)
{
	int i;
	unsigned int n;
	u_long128 *q;
	Rod *r;

	carouselTimer->duration = carouselDur;
	q = &carousel->curA;
	q[0] = rodColourOn[0];
	q[1] = rodColourOn[0];
	sub_225420();
	sub_225978();
	n = carousel->sel;
	r = carousel->rod;
	for (i = 0; i < 12; i++) {
		r[(i + n) % 12].progress = 0;
		r[i].f04 = 0;
		if (i == n)
			r[i].colour = rodColourOn[0];
		else
			r[i].colour = rodColourOff[0];
	}
	for (i = 0; i < 7; i++)
		orbPhase[i] = rand() % 65536;
	sub_22FE88();
	sub_22EE98();
}
