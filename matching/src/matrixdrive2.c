/* matrixdrive2.c - the two matrixDrive.c rotators matching/src/
 * matrixdrive.c had not yet attempted (m-leaf agent).  Extends the
 * matched RotX (0x230198) family; conventions from matrixdrive.c.
 *
 *	sh ref/try.sh src/matrixdrive2.c matrixdrive2-functions.txt
 *
 * Row layouts read straight off the ROM stores:
 *   0x230198  row0=(1,0,0,0) rows1/2=(0,c,s,0)/(0,-s,c,0)  -> RotX
 *   0x230260  rows0/2=(c,0,-s,0)/(s,0,c,0) row1=(0,1,0,0)  -> RotY
 *   0x230328  rows0/1=(c,s,0,0)/(-s,c,0,0) row2=(0,0,1,0)  -> RotZ
 * i.e. osdbits' mdRotX/mdRotY/mdRotZ naming (menu.c, inc.h) is CORRECT
 * - there is NO axis-naming swap.
 */

typedef float FMATRIX[4][4];

extern FMATRIX mtxStack[];		/* 0x368200, 16 deep	*/
extern int mtxSp;			/* gp-30300 = 0x2a7a14	*/

extern void *memset(void *, int, int);
extern void sceVu0MulMatrix(FMATRIX, FMATRIX, FMATRIX);
extern float MatrixDrive_Sin(short);	/* 0x230018 */
extern float MatrixDrive_Cos(short);	/* 0x230068 */

/* 0x230260 - close (6 diffs of 50): 4 words in the first memset call
 * block (the two row-0 zero stores are woven into the call setup in the
 * ROM, emitted ahead of it here) + 2 in the second (the one/c stores
 * swap).  All 2880 permutations of the movable statements and every
 * RTL-shape lever tried (chained/via-temp zeros, row pointers, literal
 * 1.0f, ns temp, call order) plateau here - the in-block scheduling-tie
 * class campaign/PENDING.md documents. */
void
MatrixDrive_RotY(short a)
{
	FMATRIX m;
	float c, s, one;

	c = MatrixDrive_Cos(a);
	one = 1.0f;
	s = MatrixDrive_Sin(a);
	m[0][0] = c;
	m[0][2] = -s;
	m[0][3] = 0.0f;
	m[0][1] = 0.0f;
	memset(m[1], 0, 16);
	m[1][1] = one;
	m[2][1] = 0.0f;
	m[2][2] = c;
	m[2][0] = s;
	m[2][3] = 0.0f;
	memset(m[3], 0, 16);
	m[3][3] = one;
	sceVu0MulMatrix(mtxStack[mtxSp], mtxStack[mtxSp], m);
}

/* 0x230328 - MATCH.  The winning statement order (genperm sweep) puts
 * m[0][0]=c late, between m[1][1]=c and the row-1 zeros. */
void
MatrixDrive_RotZ(short a)
{
	FMATRIX m;
	float c, s, one;

	c = MatrixDrive_Cos(a);
	one = 1.0f;
	s = MatrixDrive_Sin(a);
	m[0][1] = s;
	m[0][2] = 0.0f;
	m[0][3] = 0.0f;
	m[1][0] = -s;
	m[1][1] = c;
	m[0][0] = c;
	m[1][2] = 0.0f;
	m[1][3] = 0.0f;
	memset(m[2], 0, 16);
	m[2][2] = one;
	memset(m[3], 0, 16);
	m[3][3] = one;
	sceVu0MulMatrix(mtxStack[mtxSp], mtxStack[mtxSp], m);
}
