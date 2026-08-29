/* the system-configuration accessor cluster (0x203220..0x2051f8) -
 * compile with ee-gcc 2.9-ee-991111 -O2.
 *
 *	ee-gcc -O2 -c src/config.c -o build/config.o
 *	python3 check.py build/config.o <expanded.bin> config-functions.txt
 *
 * SCOREBOARD: 25/26 functions attempted MATCH byte-exact.  The only
 * miss is appendModule (0x204450), a bonus function beyond the task's
 * required list (see its comment below) - every required function in
 * all three priorities matches, plus the language setter from the
 * stretch goal.  (0x2069c8, the other stretch item, was not attempted.)
 *
 * DATA MODEL (derived empirically, NOT assumed - a bitfield struct was
 * tried first per the task's own hint and DISPROVED; see below):
 *
 *  - the packed settings live in two plain `int` words (NOT `unsigned`,
 *    and NOT a bitfield struct) at the literal addresses 0x2a8700 /
 *    0x2a8700+4, accessed by manual shift/mask.
 *      - bitfield struct, disproved: ee-gcc 2.9 sizes each bitfield's
 *        load/store to the minimum needed for just that field's bit
 *        range (`lbu` for a 1-bit field living in byte 0, `lw` only for
 *        a field that spans a byte boundary).  The ROM uses `lw` (a
 *        full 32-bit load) for EVERY field, including the 1-bit ones -
 *        only a plain int/manual-shift model reproduces that.
 *      - `int`, not `unsigned`/`u32`, for the word's OWN declared type:
 *        an array of u32 makes `~1`-style mask constants (or ANY
 *        constant folded through a unary `~`/complement on an unsigned
 *        operand) materialise as a full lui+ori 32-bit pattern instead
 *        of the ROM's one-instruction `li -2` - purely because the
 *        *storage* is unsigned, independent of how the mask is spelled
 *        (0xfffffffeU has the exact same problem).  Declaring the array
 *        `int` fixes every mask constant at once; logical (unsigned)
 *        right shifts are then requested per-site with an explicit
 *        `(u32)` cast - `srl` for every field but tzOffset, which reads
 *        back with a bare `int` shift pair (sll then sra) to sign-
 *        extend the signed 11-bit value.
 *  - CRITICAL address-materialisation finding: a raw integer-literal
 *    pointer cast, e.g. `*(int*)0x2a8700`, compiles two different ways
 *    depending on use - a pure read (or pure write) folds the low half
 *    into the load/store's own displacement (matches the ROM), but a
 *    read-MODIFY-write of the SAME literal address instead materialises
 *    the full 32-bit address via lui+ori (an unsigned, uncompensated
 *    split) and loads/stores through it at displacement 0 - wrong, the
 *    ROM uses the compensated-hi/embedded-displacement form even in its
 *    setters.  A plain `extern int x;` side-steps that but goes
 *    $gp-relative when small enough - also wrong, the ROM never uses
 *    $28 for this data.  The fix, reproducing the ROM in EVERY case
 *    (read, write, or read-modify-write), is codegen law 10's "far
 *    array" trick: `extern int x[];` (an INCOMPLETE array, so gcc can't
 *    assume it's small-data-eligible) accessed as `x[0]`.  Applied to
 *    every absolute-address global below: the config words, the
 *    ps1drv/NVM scratch buffers, the module registry write pointer -
 *    even a `struct Module *` pointer variable needs the SAME trick
 *    (`extern struct Module *arr[]`, use `arr[0]`), since a plain
 *    extern pointer is itself small enough to go $gp-relative.
 *  - the "avoid a fresh copy register" finding, found by trial and
 *    error on setScreenType/setDateNotation/setTzIndex/setLanguage:
 *    once a parameter is tested in an `if` guard, RE-ASSIGNING that
 *    same parameter (`v &= 3;`) before its next use makes gcc allocate
 *    it a fresh register to preserve it across the branch (an extra
 *    `move` the ROM doesn't have) - using the masked expression INLINE
 *    at each use site instead (`v & 3`) lets gcc reuse the original
 *    parameter register, matching the ROM exactly.  setTzIndex's 127
 *    case is the one spot that DOES need reassignment-shaped code
 *    (restoring a different remembered value), and there the ROM's own
 *    shape needs a *fresh* local for the substitute value, not a
 *    reassignment of the parameter - see its comment.
 *  - a companion register-tie finding on getLanguage/setLanguage: storing
 *    a shift-extracted field into a NAMED `int` local before comparing
 *    it gets the signed comparison right (`slti` not `sltiu`) but pins
 *    the address computation to the wrong register; inlining the same
 *    cast-and-compare expression directly in the `if` (no named local)
 *    gets BOTH right at once - `(int)(((u32)cfgword[0]>>4)&0x1f) < 8`.
 *  - the setter return-value idiom, confirmed by matching setSpdif and
 *    setTzOffset instruction-for-instruction: compute the masked/shifted
 *    new field value into a local FIRST, use that same local both for
 *    the return and for OR-ing into the read-modify-write (word first,
 *    local second, i.e. `word = (word & ~mask) | t;`) - reordering the
 *    OR operands or skipping the intermediate local changes the
 *    instruction schedule (only matters for tzOffset/tzIndex, whose
 *    mask constants don't fit as immediates - the other setters are
 *    short enough that this doesn't bite).
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
 *  - the low RAM block at 0x1f0000: cfgDirty (+0x124) and the 15-byte
 *    ps1drv array (+0x1224) are two fields of ONE struct at a constant
 *    address, not two separate symbols - osdWriteConfig's ps1drv read
 *    and cfgDirty write share a single "lui s1,0x1f" in the ROM, which
 *    only happens if both go through the same base.
 *  - osdReadConfig's 15-byte zero loop is a hand-written DECREMENTING
 *    POINTER (`u8 *p = &low.ps1drv[14]; for(...) *p-- = 0;`), not plain
 *    `low.ps1drv[i] = 0` indexing - the latter keeps `i` and a separate
 *    base register and adds them every iteration (one extra `addu` the
 *    ROM's loop body doesn't have); the ROM instead pairs the loop
 *    counter with a pointer that walks down alongside it.
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

/* the low RAM block at 0x1f0000: cfgDirty (+0x124) and ps1drv (+0x1224)
 * are two fields of the SAME struct, not separate symbols - the ROM
 * shares one "lui s1,0x1f" between them (confirmed: osdWriteConfig's
 * ps1drv read and cfgDirty write both go through register s1). */
struct LowBlock {
	u8  pad0[0x124];
	u32 cfgDirty;		/* +0x124: "config write pending" flag */
	u8  pad1[0x1224 - 0x128];
	u8  ps1drv[15];		/* +0x1224: 15-byte PS1 driver config */
};
#define low (*(struct LowBlock *)0x1f0000)

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

void transportOpenReadClose(void *handle);
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
void
transportOpenReadClose(void *handle)
{
	int status;
	int r;

	do
		r = cdConfigOpen(1, 0, 2, &status);
	while ((status & 9) != 0 || r == 0);
	do
		r = cdConfigRead(handle, &status);
	while ((status & 9) != 0 || r == 0);
	do
		r = cdConfigClose(&status);
	while ((status & 9) != 0 || r == 0);
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
	int f = flag & 0xff;

	copyToHandle15(cdHandleBuf, rec);
	packConfigToHandle(cdHandleBuf, words, f);
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
	cfgRecordBuf[0] = low.ps1drv[0] & 0x11;
	writeConfigReal(cfgRecordBuf, cfgword, flag);
	low.cfgDirty = 1;
	UnlockConfig();
}

/* 0x2035d0 - read-config wrapper. */
int
osdReadConfig(void)
{
	int result;
	int i;
	u8 *p;

	LockConfig();
	result = readConfigReal(cfgRecordBuf, cfgword);
	p = &low.ps1drv[14];
	for (i = 14; i >= 0; i--)
		*p-- = 0;
	low.ps1drv[0] = cfgRecordBuf[0] & 0x11;
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
	cfgword[0] = (cfgword[0] & ~(3 << 1)) | ((v & 3) << 1);
	return v & 3;
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
		if ((((u32)cfgword[0] >> 4) & 0x1f) == 1)
			return 1;
		return 0;
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
	if (!RomVersionCheck()) {
		if ((int)(((u32)cfgword[0] >> 4) & 0x1f) < 2)
			goto store;
		if (v == 0)
			return 0;
		goto store;
	}
	if ((int)(((u32)cfgword[0] >> 4) & 0x1f) < 8)
		goto store;
	if (v == 1)
		return 1;
store:
	cfgword[0] = (cfgword[0] & ~0x1f0) | ((v & 0x1f) << 4);
	return v & 0x1f;
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
		int idx = lastBadTzIndexArr[0] & 0x1ff;
		cfgword[0] = (cfgword[0] & ~(0x1ff << 20)) | (idx << 20);
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
	cfgword[1] = (cfgword[1] & ~3) | (v & 3);
	return v & 3;
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
extern struct Module moduleArray[64];		/* 0x2b8b40, limit 0x2b9240 */

extern int defaultPrepare(void);		/* 0x2043e8 */
extern int defaultF16(void);			/* 0x2043f0 */
extern int defaultF20(void);			/* 0x2043f8 */
extern int defaultF24(void);			/* 0x204400 */

extern int appendModule(struct Module *desc);	/* 0x204450 */
int osdRegisterModule(struct Module *desc);
int osdRegisterAllModules(void);

/* 0x204450 - BONUS, not in the task's required list (which attributes
 * the append logic to 0x204408 itself; the real ROM splits validation
 * at 0x204408, MATCHES above, from this separate appender).  Residual:
 * 48/55 instructions aligned.  Two findings, one fixed and one not:
 *   - FIXED: the bounds check `p >= (Module*)0x2b9240` (a bare literal)
 *     compiles as `p > 0x2b923f` (gcc rewrites >= to a decremented >,
 *     same "materialise via ori" issue as the address-materialisation
 *     note above, but for a COMPARISON constant, which no amount of
 *     `extern int x[]` on the POINTER side fixes since the constant is
 *     still a bare literal).  The fix: compare against a real
 *     relocatable symbol instead - `extern struct Module moduleArray[64]`
 *     and `p >= &moduleArray[64]` - which resolves through R_MIPS_LO16
 *     (masked by check.py) instead of gcc's own literal-constant
 *     folder, and reproduces the ROM's sltu/operand order exactly.
 *   - NOT FIXED: the ROM reloads `moduleWritePtr` from memory 3 times
 *     (before the f20 check, before the f24 check, and before the final
 *     increment - but NOT before the f16 check right after the first
 *     reload) rather than keeping it live in one register the whole
 *     function, and its f24-default store uses the ALREADY-INCREMENTED
 *     pointer (writing 28 bytes past where it reads from - a latent ROM
 *     bug, reproduced nowhere here since it only shows up on that one
 *     reload pattern).  Dereferencing through the global pointer
 *     variable directly (`moduleWritePtr->prepare` etc., instead of a
 *     cached local) gets ONE reload (right after the struct copy) but
 *     gcc still proves the value stable across the remaining three
 *     checks and never reloads again; marking the pointer `volatile`
 *     over-corrects (reloads on every single access, most of which the
 *     ROM does NOT reload, and it also kills the ROM's `bnezl`
 *     branch-likely form).  Reads as a delay-slot-filling scheduler
 *     artifact tied to the exact live-range shape gcc's register
 *     allocator produced for THIS run, not a source-level idiom - a
 *     good candidate for the campaign/ permuter mentioned in
 *     matching/README.md rather than further hand-editing. */
int
appendModule(struct Module *desc)
{
	struct Module *p = moduleWritePtr;

	if (p >= &moduleArray[64])
		return -110;
	*p = *desc;
	if (moduleWritePtr->prepare == 0)
		moduleWritePtr->prepare = (int (*)(void))defaultPrepare;
	if (moduleWritePtr->f16 == 0)
		moduleWritePtr->f16 = defaultF16;
	if (moduleWritePtr->f20 == 0)
		moduleWritePtr->f20 = defaultF20;
	if (moduleWritePtr->f24 == 0)
		moduleWritePtr->f24 = defaultF24;
	moduleWritePtr++;
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
