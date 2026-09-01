/* matrixDrive.c - the shared matrix-stack / sine-table service the menu
 * module (and the opening) calls into.  The real source file name is
 * known: the two panic strings at 0x2a4bd8/0x2a4c20 are
 *
 *	"%s::MatrixDrive_PushMatrix: Matrix stack over flow!!\n"
 *	"%s::MatrixDrive_PopMatrix: Matrix stack under flow!!\n"
 *
 * with __FILE__ = "matrixDrive.c" at 0x2a4c10, so the functions really
 * are MatrixDrive_*.
 *
 *	../try.sh src/matrixdrive.c matrixdrive-functions.txt
 *
 * Conventions from matching/README.md and matching/src/config.c:
 *  - absolute-address globals are INCOMPLETE extern arrays so gcc
 *    cannot put them in small data ($gp);
 *  - globals the ROM really does reach through $gp stay plain externs;
 *  - everything prototyped, declaration order = ROM address order.
 */

typedef float FMATRIX[4][4];
typedef float FVECTOR[4];

/* The ONLY way to make gcc 2.9-ee emit lq/sq is a 128-bit-mode type -
 * `__attribute__((aligned(16)))' on a 16-byte struct or array does NOT
 * do it (probed: still ld/sd pairs).  This is eetypes.h's u_long128. */
typedef unsigned int u_long128 __attribute__((mode(TI)));

/* absolute globals */
extern float sinTable[];		/* 0x3581f0, 16385 entries	*/
extern FMATRIX mtxStack[];		/* 0x368200, 16 deep		*/

/* $gp globals */
extern int sinTableReady;		/* gp-30304 = 0x2a7a10		*/
extern int mtxSp;			/* gp-30300 = 0x2a7a14		*/

extern double sin(double);
/* The ROM loads these two ONCE into s3/s2 before the loop, from
 * 0x2a4bc8/0x2a4bd0 in .rdata via `lui at' - i.e. gcc hoisted a
 * literal-pool load out of a loop containing a call.  This compiler
 * build never hoists a CONST_DOUBLE literal (no -O level does), but it
 * does hoist a `const' MEM; declaring them as incomplete extern const
 * arrays keeps them out of small data too, which reproduces
 * everything except the lui temp register ($at vs $a0).  See notes.md. */
extern const double mdHalfPi[];		/* 0x2a4bc8 = 1.5707963267948966 */
extern const double mdTabSize[];	/* 0x2a4bd0 = 16385.0		*/
extern void *memset(void *, int, int);
extern int scePrintf(const char *, ...);
extern void sceVu0UnitMatrix(FMATRIX);
extern void sceVu0MulMatrix(FMATRIX, FMATRIX, FMATRIX);
extern void sceVu0ApplyMatrix(FVECTOR, FMATRIX, FVECTOR);

void MatrixDrive_InitSinTable(void);	/* 0x22ff70 */
float MatrixDrive_Sin(short);		/* 0x230018 */
float MatrixDrive_Cos(short);		/* 0x230068 */
void MatrixDrive_Init(void);		/* 0x230090 */
void MatrixDrive_PushMatrix(void);	/* 0x2300b8 */
void MatrixDrive_PopMatrix(void);	/* 0x230138 */
float (*MatrixDrive_GetMatrix(void))[4];/* 0x230180 */
void MatrixDrive_RotX(short);		/* 0x230198 */
void MatrixDrive_TranslateV(FVECTOR);	/* 0x2303e8 */
void MatrixDrive_Translate(float, float, float);	/* 0x230440 */

#define TABSIZE 16385

/* 0x22ff70 */
void
MatrixDrive_InitSinTable(void)
{
	int i;
	float *p;

	if (sinTableReady)
		return;
	p = sinTable;
	for (i = 0; i < TABSIZE; i++) {
		*p = (float)sin(i * mdHalfPi[0] / mdTabSize[0]);
		p++;
	}
	sinTableReady = 1;
}

/* 0x230018 */
float
MatrixDrive_Sin(short a)
{
	int i, neg;
	float r;

	/* the `?:' abs is what keeps gcc from if-converting to movz, and
	 * the single float temp (rather than two `return' statements) is
	 * what makes it share the tail - see notes.md */
	i = a < 0 ? -a : a;
	neg = (unsigned int)a >> 31;
	if (i >= 16384)
		i = 0x8000 - i;
	r = sinTable[i];
	if (neg)
		r = -r;
	return r;
}

/* 0x230068 */
float
MatrixDrive_Cos(short a)
{
	return MatrixDrive_Sin((short)(a + 0x4000));
}

/* 0x230090 */
void
MatrixDrive_Init(void)
{
	mtxSp = 0;
	sceVu0UnitMatrix(mtxStack[0]);
	MatrixDrive_InitSinTable();
}

/* 0x2300b8 */
void
MatrixDrive_PushMatrix(void)
{
	if (++mtxSp == 16) {
		scePrintf("%s::MatrixDrive_PushMatrix: Matrix stack over flow!!\n",
			"matrixDrive.c");
		for (;;)
			;
	}
	/* four explicit quadword assignments beat the one-struct-assignment
	 * form here (15/32 aligned vs 11/32); see probe_push.c.  Two
	 * residuals remain, both independent of the panic-loop pad:
	 *  - the ROM RELOADS mtxSp from $gp after the branch, we keep the
	 *    incremented value live in v0;
	 *  - the ROM keeps two base pointers (base-64 and base) and uses
	 *    +0/+16/+32/+48 on both, we keep one base and materialise five
	 *    extra `move's for the negative-displacement loads. */
	*(u_long128 *)mtxStack[mtxSp][0] = *(u_long128 *)mtxStack[mtxSp - 1][0];
	*(u_long128 *)mtxStack[mtxSp][1] = *(u_long128 *)mtxStack[mtxSp - 1][1];
	*(u_long128 *)mtxStack[mtxSp][2] = *(u_long128 *)mtxStack[mtxSp - 1][2];
	*(u_long128 *)mtxStack[mtxSp][3] = *(u_long128 *)mtxStack[mtxSp - 1][3];
}

/* 0x230138 */
void
MatrixDrive_PopMatrix(void)
{
	if (--mtxSp < 0) {
		scePrintf("%s::MatrixDrive_PopMatrix: Matrix stack under flow!!\n",
			"matrixDrive.c");
		for (;;)
			;
	}
}

/* 0x230180 */
float (*
MatrixDrive_GetMatrix(void))[4]
{
	return mtxStack[mtxSp];
}

/* 0x230198 - row0 = (1,0,0,0), row3 = (0,0,0,1), so this is RotX */
void
MatrixDrive_RotX(short a)
{
	FMATRIX m;
	float c, s, one;

	c = MatrixDrive_Cos(a);
	one = 1.0f;
	s = MatrixDrive_Sin(a);
	/* statement order found by genperm.py's single-statement
	 * relocation sweep (49/50; the last word is an in-block schedule
	 * tie, the class matching/campaign/PENDING.md already documents) */
	memset(m[0], 0, 16);
	m[1][0] = 0.0f;
	m[1][3] = 0.0f;
	m[2][0] = 0.0f;
	m[2][3] = 0.0f;
	m[0][0] = one;
	m[1][2] = s;
	m[2][1] = -s;
	m[2][2] = c;
	m[1][1] = c;
	memset(m[3], 0, 16);
	m[3][3] = one;
	sceVu0MulMatrix(mtxStack[mtxSp], mtxStack[mtxSp], m);
}

/* 0x2303e8 */
void
MatrixDrive_TranslateV(FVECTOR v)
{
	FVECTOR t;

	sceVu0ApplyMatrix(t, mtxStack[mtxSp], v);
	*(u_long128 *)mtxStack[mtxSp][3] = *(u_long128 *)t;
}

/* 0x230440 */
void
MatrixDrive_Translate(float x, float y, float z)
{
	FVECTOR v;

	v[0] = x;
	v[1] = y;
	v[2] = z;
	v[3] = 1.0f;
	MatrixDrive_TranslateV(v);
}
