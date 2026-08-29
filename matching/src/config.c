/* the system-configuration accessor cluster (0x203220..0x2069c8) -
 * compile with ee-gcc 2.9-ee-991111 -O2.
 *
 *	ee-gcc -O2 -c src/config.c -o build/config.o
 *	python3 check.py build/config.o <expanded.bin> config-functions.txt
 *
 * DATA MODEL (derived empirically, not assumed - see docs/osdsys-map.md
 * 3.3/3.4 for the semantics; this is the C SHAPE that reproduces the ROM):
 *
 *  - the packed settings live in two plain `unsigned int` words at the
 *    LITERAL addresses 0x2a8700/0x2a8704 - manual shift/mask, NOT a C
 *    bitfield struct.  A bitfield struct was tried first (per the task's
 *    own hint) and DISPROVED empirically: ee-gcc 2.9 allocates each
 *    bitfield's load/store width to the minimum needed to cover just
 *    that field's bit range (lbu for a 1-bit field in byte 0, lw only
 *    for a field spanning a byte boundary) - the ROM uses `lw` (a full
 *    32-bit load) for EVERY field, including the 1-bit ones, which only
 *    a plain int/manual-shift model reproduces.
 *  - CRITICAL address-materialisation finding: a raw integer-literal
 *    pointer cast, e.g. `*(int*)0x2a8700`, compiles two different ways
 *    depending on use:
 *      - a pure read (or pure write) folds the low half into the
 *        load/store's own displacement, reusing one register for
 *        hi-then-result (matches the ROM).
 *      - a read-MODIFY-write of the SAME literal address instead
 *        materialises the full 32-bit address via lui+ori (an unsigned,
 *        uncompensated split) and then loads/stores through it at
 *        displacement 0 - this does NOT match the ROM, which uses the
 *        compensated-hi/embedded-displacement form even for its setters.
 *    An `extern int x;` (plain scalar/pointer global) side-steps this
 *    but goes $gp-relative when small enough - also wrong, the ROM never
 *    uses $28 for this data.  The fix that reproduces the ROM in BOTH
 *    cases is codegen law 10's "far array" trick: `extern u32 x[];`
 *    (an INCOMPLETE array, so gcc can't assume it's small-data-eligible)
 *    accessed as `x[0]`.  That gives the real ROM's lui(+1 compensated)
 *    / embedded-displacement form whether the access is a read, a
 *    write, or a read-modify-write.  Applied uniformly below to every
 *    absolute-address global (the config words, the ps1drv/NVM scratch
 *    buffers, the module registry write pointer).
 *  - srl vs sra tells apart the unsigned fields from the signed
 *    11-bit timezone offset: plain `cfgword[0] >> n` (unsigned) for
 *    every field except tzOffset, which reads back via an explicit
 *    `(int)` cast + shift pair (sll then sra) to get the sign-extended
 *    value - both the read and the setter's return value use this.
 *  - the setter return-value idiom, confirmed by matching setSpdif and
 *    setTzOffset instruction-for-instruction: compute the masked/shifted
 *    new field value into a local FIRST, use that same local both for
 *    the return and for OR-ing into the read-modify-write (word first,
 *    local second, i.e. `word = (word & ~mask) | t;`) - reordering the
 *    OR operands or skipping the intermediate local changes the
 *    instruction schedule.
 *  - the "reject a default write while already out of range" guard
 *    (screenType, dateNotation) is short-circuit `if (raw >= LIMIT &&
 *    v == 0) return 0;` - only evaluates v==0 when the field is already
 *    invalid, matching the ROM's two-branch shape exactly.
 *  - the "invalid read clamps to a default" idiom (getScreenType,
 *    getDateNotation) is NOT a ternary (a plain `cond ? a : b` compiles
 *    to a DIFFERENT idiom in this compiler - li/slt/movn, per text.c's
 *    header comment) but an explicit named-result-conditionally-
 *    overwritten shape, which is what produces the ROM's slti+movz.
 *  - the language accessors gate on an external "region/version check"
 *    (0x204318, a different TU) and funnel three different tests into
 *    ONE physical store tail; reproduced with `goto`, since the ROM
 *    has exactly one copy of that tail reached three different ways.
 *
 * SCOPE: only the functions listed in config-functions.txt are meant to
 * match.  Everything this file calls but does not define (locking, the
 * CDVD SCMD RPC wrappers, the packed-word <-> NVM-record byte
 * converters, the seven module setup/getDesc/getVersion callbacks) is
 * declared `extern` and deliberately NOT implemented: check.py masks
 * jal/jalr targets and hi/lo relocations, so the callee's real address
 * never affects the match, only the call SHAPE (register setup) does.
 */

typedef unsigned char  u8;
typedef unsigned int   u32;

/* ---- absolute-address globals (see the "far array" note above) ---- */

extern int cfgword[];		/* 0x2a8700: [0]=packed settings, [1]=date notation */
extern u8  ps1drv[];		/* 0x1f1224: 15-byte PS1 driver config */
extern u32 cfgDirty[];		/* 0x1f0124: "config write pending" flag */
extern u8  cfgRecordBuf[];	/* 0x2a86f0: 15-byte NVM record scratch */
extern u8  cdHandleBuf[];	/* 0x2a8680: the real CDVD config handle */
extern int lastBadTzIndexArr[]; /* 0x26e528: last out-of-range tz index */

/* ---- out-of-scope helpers (see the file header) ---- */

extern void LockConfig(void);				/* 0x201008 */
extern void UnlockConfig(void);			/* 0x201020 */
extern int  copyFromHandle15(void *dst, void *src);	/* 0x2031c0 */
extern int  copyToHandle15(void *dst, void *src);	/* 0x2031f0 */
extern int  unpackConfigFromHandle(void *words, void *handle); /* 0x202f20 */
extern int  packConfigToHandle(void *handle, void *words, int flag); /* 0x2030b8 */
extern int  finalizeConfigHandle(void *handle);	/* 0x2032b8 */
extern int  cdConfigOpen(int, int, int, void *);	/* 0x251670 */
extern int  cdConfigRead(void *, void *);		/* 0x251840 */
extern int  cdConfigClose(void *);			/* 0x251780 */
extern int  RomVersionCheck(void);			/* 0x204318 */

int transportOpenReadClose(void *handle);
int readConfigReal(void *rec, void *words);
int writeConfigReal(void *rec, void *words, int flag);

void osdWriteConfig(u8 flag);
int  osdReadConfig(void);

int getSpdif(void);
int setSpdif(int v);
int getScreenType(void);
int setScreenType(int v);
int getVideoOutput(void);
int setVideoOutput(int v);
int getLanguage(void);
int setLanguage(int v);
int getTzOffset(void);
int setTzOffset(int v);
int getTzIndex(void);
int setTzIndex(int v);
int getDst(void);
int setDst(int v);
int getNotation12h(void);
int setNotation12h(int v);
int getDateNotation(void);
int setDateNotation(int v);

/* ==================================================================== */
/* Priority 2 - NVM persistence path                                    */
/* ==================================================================== */

/* 0x203220 - open/read/close the CDVD config channel, each step retried
 * while the RPC's own status word has bit 0 or bit 3 set, or the call
 * itself reports failure (v0 == 0). */
int
transportOpenReadClose(void *handle)
{
	int status;

	do
		;
	while (cdConfigOpen(1, 0, 2, &status) == 0 || (status & 9) != 0);
	do
		;
	while (cdConfigRead(handle, &status) == 0 || (status & 9) != 0);
	do
		;
	while (cdConfigClose(&status) == 0 || (status & 9) != 0);
	return 0;
}

/* 0x203390 - read the NVM config into *rec (15 raw bytes) and *words
 * (the packed settings), returning unpackConfigFromHandle's status. */
int
readConfigReal(void *rec, void *words)
{
	int result;

	transportOpenReadClose(cdHandleBuf);
	result = unpackConfigFromHandle(words, cdHandleBuf);
	copyFromHandle15(rec, cdHandleBuf);
	return result;
}

/* 0x2033f8 - write *words (masked flag) back into the NVM config,
 * seeded from the 15 raw bytes at *rec. */
int
writeConfigReal(void *rec, void *words, int flag)
{
	copyToHandle15(cdHandleBuf, rec);
	packConfigToHandle(cdHandleBuf, words, flag & 0xff);
	finalizeConfigHandle(cdHandleBuf);
	return 0;
}

/* ==================================================================== */
/* Priority 1 - the ~20 console configuration accessors                 */
/* ==================================================================== */

/* 0x203570 - write-config wrapper: called by ThreadY on cmds 16/18/26. */
void
osdWriteConfig(u8 flag)
{
	LockConfig();
	cfgRecordBuf[0] = ps1drv[0] & 0x11;
	writeConfigReal(cfgRecordBuf, cfgword, flag);
	cfgDirty[0] = 1;
	UnlockConfig();
}

/* 0x2035d0 - read-config wrapper. */
int
osdReadConfig(void)
{
	int result;
	int i;

	LockConfig();
	result = readConfigReal(cfgRecordBuf, cfgword);
	for (i = 14; i >= 0; i--)
		ps1drv[i] = 0;
	ps1drv[0] = cfgRecordBuf[0] & 0x11;
	UnlockConfig();
	return result;
}

/* 0x203658 */
int
getSpdif(void)
{
	return cfgword[0] & 1;
}

/* 0x203668 */
int
setSpdif(int v)
{
	v &= 1;
	cfgword[0] = (cfgword[0] & ~1) | v;
	return v;
}

/* 0x203690 */
int
getScreenType(void)
{
	int t = ((u32)cfgword[0] >> 1) & 3;
	int result = t;

	if (t >= 3)
		result = 0;
	return result;
}

/* 0x2036b0 */
int
setScreenType(int v)
{
	int t = ((u32)cfgword[0] >> 1) & 3;

	if (t >= 3 && v == 0)
		return 0;
	v &= 3;
	cfgword[0] = (cfgword[0] & ~(3 << 1)) | (v << 1);
	return v;
}

/* 0x2036f8 */
int
getVideoOutput(void)
{
	return ((u32)cfgword[0] >> 3) & 1;
}

/* 0x203710 */
int
setVideoOutput(int v)
{
	int t = (v & 1) << 3;
	cfgword[0] = (cfgword[0] & ~(1 << 3)) | t;
	return v & 1;
}

/* 0x203738 - the real language getter; GetLanguage (0x2040d0) is a
 * trivial one-line wrapper around this. */
int
getLanguage(void)
{
	int lang;

	if (!RomVersionCheck()) {
		lang = ((u32)cfgword[0] >> 4) & 0x1f;
		return lang == 1;
	}

	lang = ((u32)cfgword[0] >> 4) & 0x1f;
	if (lang == 0) {
		cfgword[0] = (cfgword[0] & ~0x1f0) | 0x10;
		goto out;
	}
	if (lang < 8)
		goto out;
	return 1;
out:
	lang = ((u32)cfgword[0] >> 4) & 0x1f;
	return lang;
}

/* 0x2037b8 */
int
setLanguage(int v)
{
	int cur;

	if (!RomVersionCheck()) {
		cur = ((u32)cfgword[0] >> 4) & 0x1f;
		if (cur < 2)
			goto store;
		if (v == 0)
			return 0;
		goto store;
	}
	cur = ((u32)cfgword[0] >> 4) & 0x1f;
	if (cur < 8)
		goto store;
	if (v == 1)
		return 1;
store:
	v &= 0x1f;
	cfgword[0] = (cfgword[0] & ~0x1f0) | (v << 4);
	return v;
}

/* 0x203848 */
int
getTzOffset(void)
{
	return (int)cfgword[0] << 12 >> 21;
}

/* 0x203860 */
int
setTzOffset(int v)
{
	int t = (v & 0x7ff) << 9;
	cfgword[0] = (cfgword[0] & ~(0x7ffU << 9)) | t;
	return (v << 21) >> 21;
}

/* 0x203890 */
int
getTzIndex(void)
{
	int idx = ((u32)cfgword[0] >> 20) & 0x1ff;

	if (idx >= 127) {
		lastBadTzIndexArr[0] = idx;
		return 127;
	}
	return idx;
}

/* 0x2038c0 */
int
setTzIndex(int v)
{
	if (v == 127) {
		v = lastBadTzIndexArr[0];
		cfgword[0] = (cfgword[0] & ~(0x1ff << 20)) | ((v & 0x1ff) << 20);
		return 127;
	}
	cfgword[0] = (cfgword[0] & ~(0x1ff << 20)) | ((v & 0x1ff) << 20);
	return v & 0x1ff;
}

/* 0x203928 */
int
getDst(void)
{
	return ((u32)cfgword[0] >> 29) & 1;
}

/* 0x203940 */
int
setDst(int v)
{
	int t = (v & 1) << 29;
	cfgword[0] = (cfgword[0] & ~(1U << 29)) | t;
	return v & 1;
}

/* 0x203968 */
int
getNotation12h(void)
{
	return ((u32)cfgword[0] >> 30) & 1;
}

/* 0x203980 */
int
setNotation12h(int v)
{
	int t = (v & 1) << 30;
	cfgword[0] = (cfgword[0] & ~(1U << 30)) | t;
	return v & 1;
}

/* 0x2039a8 */
int
getDateNotation(void)
{
	int t = cfgword[1] & 3;
	int result = t;

	if (t >= 3)
		result = 0;
	return result;
}

/* 0x2039c0 */
int
setDateNotation(int v)
{
	int t = cfgword[1] & 3;

	if (t >= 3 && v == 0)
		return 0;
	v &= 3;
	cfgword[1] = (cfgword[1] & ~3) | v;
	return v;
}

/* ==================================================================== */
/* Priority 3 - the module registry                                     */
/* ==================================================================== */

struct Module {
	int   (*setup)(void);
	int   (*prepare)(void);
	char *(*getDesc)(int);
	char *(*getVersion)(int);
	int   (*f16)(void);
	int   (*f20)(void);
	int   (*f24)(void);
};

extern struct Module *moduleWriteArr[];	/* 0x26ecf8 */
#define moduleWritePtr moduleWriteArr[0]

extern int defaultPrepare(void);		/* 0x2043e8 */
extern int defaultF16(void);			/* 0x2043f0 */
extern int defaultF20(void);			/* 0x2043f8 */
extern int defaultF24(void);			/* 0x204400 */

extern int appendModule(struct Module *desc);	/* 0x204450 */
int osdRegisterModule(struct Module *desc);
int osdRegisterAllModules(void);

/* 0x204450 - bonus: the real appender behind osdRegisterModule. */
int
appendModule(struct Module *desc)
{
	struct Module *p = moduleWritePtr;

	if (p >= (struct Module *)0x2b9240)
		return -110;
	*p = *desc;
	if (p->prepare == 0)
		p->prepare = (int (*)(void))defaultPrepare;
	if (p->f16 == 0)
		p->f16 = defaultF16;
	if (p->f20 == 0)
		p->f20 = defaultF20;
	if (p->f24 == 0)
		p->f24 = defaultF24;
	p++;
	moduleWritePtr = p;
	return 0;
}

/* 0x204408 - validate a descriptor and append it. */
int
osdRegisterModule(struct Module *desc)
{
	if (desc->setup == 0 || desc->getDesc == 0)
		return -111;
	if (desc->getVersion == 0)
		return -111;
	return appendModule(desc);
}

extern int registerConsoleModule(void);	/* 0x204818 */
extern int makeOpeningThread(void);		/* 0x211ce0 */
extern int makeThreadU(void);			/* 0x21c980 */
extern int makeThreadV(void);			/* 0x23fb68 */
extern int registerCdPlayerModule(void);	/* 0x204898 */
extern int registerPs1DriverModule(void);	/* 0x204938 */
extern int registerDvdPlayerModule(void);	/* 0x205158 */

/* 0x2051a8 - registers the seven modules, called from main. */
int
osdRegisterAllModules(void)
{
	registerConsoleModule();
	makeOpeningThread();
	makeThreadU();
	makeThreadV();
	registerCdPlayerModule();
	registerPs1DriverModule();
	registerDvdPlayerModule();
	return 0;
}
