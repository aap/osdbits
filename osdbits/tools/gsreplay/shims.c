/*
 * Compatibility shims to link Sony's 1998 g++ 2.7.2 / libc5-era libgpu2
 * objects against a modern 32-bit glibc (2.41, Void Linux).
 *
 * Covers:
 *  - gets()             removed from glibc 2.32 (symbol + declaration)
 *  - _IO_stderr_        old libio global; glibc now exports _IO_2_1_stderr_
 *  - __builtin_new etc. libg++ 2.7 operator new/delete ABI
 *  - __pure_virtual     libg++ vtable terminator
 *  - __divdi3 & co.     i386 64-bit divide helpers (no 32-bit libgcc here)
 *
 * The divide helpers are shift-subtract implementations written so that the
 * compiler emits no 64-bit divide/multiply libcalls (which would recurse into
 * the very functions being defined).
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* --- gets(): removed from glibc 2.32 --- */
char *gets(char *s)
{
	int c;
	char *p = s;

	while ((c = fgetc(stdin)) != EOF && c != '\n')
		*p++ = (char)c;
	if (p == s && c == EOF)
		return NULL;
	*p = '\0';
	return s;
}

/* --- old libio stderr ---
 * 1998 objects compiled against libc5/glibc-2.0 headers reference
 * _IO_stderr_ as a FILE OBJECT (code does fprintf(&_IO_stderr_, ...)).
 * It must therefore be an object-shaped symbol, not a pointer. glibc
 * no longer exports it, and glibc 2.41 validates the FILE vtable
 * (stored in the hidden _IO_FILE_plus wrapper just past the FILE
 * struct), so the whole allocation is copied: FILE + vtable pointer.
 * The constructor runs before main via DT_INIT_ARRAY. */
struct lg2_file_plus {
	FILE file;
	const void *vtable;
};

extern struct lg2_file_plus _IO_2_1_stderr_;
struct lg2_file_plus lg2_stderr_storage;

__asm__(".globl _IO_stderr_\n	.set _IO_stderr_, lg2_stderr_storage");

__attribute__((constructor))
static void
lg2_stderr_init(void)
{
	lg2_stderr_storage = _IO_2_1_stderr_;
}

/* --- libg++ 2.7 ABI ---
 * MODIFIED vs port/shims.c: __builtin_new records every allocation >= 4 MB.
 * GPU2::GPU2 makes exactly two of them (Memory and BitBLT, 0x400144 and
 * 0x4001c8 in some order), which is how the harness gets a pointer straight
 * into the model's 4 MB local memory without any exported API. */
void *lg2_bigalloc[8];
size_t lg2_bigsize[8];
int lg2_nbig;

void *__builtin_new(size_t sz)
{
	void *p = malloc(sz);

	if (sz >= 0x400000 && lg2_nbig < 8) {
		lg2_bigalloc[lg2_nbig] = p;
		lg2_bigsize[lg2_nbig] = sz;
		lg2_nbig++;
	}
	return p;
}

void __builtin_delete(void *p)
{
	if (p)
		free(p);
}

void *__builtin_vec_new(size_t sz)
{
	return malloc(sz);
}

void __builtin_vec_delete(void *p)
{
	if (p)
		free(p);
}

void __pure_virtual(void)
{
	fputs("pure virtual function called\n", stderr);
	abort();
}

/* --- i386 64-bit divide helpers --- */
typedef unsigned long long u64;
typedef long long s64;

static u64 udiv64(u64 n, u64 d)
{
	u64 q = 0, r = 0;
	int i;

	if (d == 0 || d > n)
		return 0;
	for (i = 63; i >= 0; i--) {
		r = (r << 1) | ((n >> i) & 1);
		if (r >= d) {
			r -= d;
			q |= (u64)1 << i;
		}
	}
	return q;
}

static u64 mag64(s64 v)
{
	return (v < 0) ? (u64)(~v) + 1u : (u64)v;
}

u64 __udivdi3(u64 n, u64 d)
{
	return udiv64(n, d);
}

u64 __umoddi3(u64 n, u64 d)
{
	return n - udiv64(n, d) * d;
}

s64 __divdi3(s64 n, s64 d)
{
	int neg = (n < 0) ^ (d < 0);
	s64 q = (s64)udiv64(mag64(n), mag64(d));

	return neg ? -q : q;
}

s64 __moddi3(s64 n, s64 d)
{
	int neg = (n < 0);
	s64 r = (s64)udiv64(mag64(n), mag64(d));

	return neg ? -r : r;
}
