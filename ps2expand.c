#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define nil NULL
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef struct ExpandState ExpandState;
struct ExpandState
{
	u32 size;
	u32 blockDesc;
	u32 n;
	u32 shift;
	u32 mask;
	u8 *ptr;
};
ExpandState exstate;

void
ExpandInit(u8 *data)
{
	exstate.size = *(u32*)data;
	exstate.ptr = data+4;
}

void
ExpandSetBlock(void)
{
	exstate.blockDesc = *exstate.ptr++;
	exstate.blockDesc = (exstate.blockDesc << 8) | *exstate.ptr++;
	exstate.blockDesc = (exstate.blockDesc << 8) | *exstate.ptr++;
	exstate.blockDesc = (exstate.blockDesc << 8) | *exstate.ptr++;
	exstate.n = exstate.blockDesc & 3;
	exstate.shift = 14 - exstate.n;
	exstate.mask = 0x3FFF >> exstate.n;
}

void
ExpandMain(u8 *dst)
{
	int n, m;
	u8 b;
	u8 *start, *src;

	start = dst;
	for(n = 0; dst < start + exstate.size; n--, exstate.blockDesc <<= 1) {
		if(n == 0) {
			n = 30;
			ExpandSetBlock();
		}
		b = *exstate.ptr++;
		if(exstate.blockDesc & 0x80000000) {
			u16 h = (u16)b<<8 | *exstate.ptr++;
			src = dst-1 - (h & exstate.mask);
			m = 2 + (h >> exstate.shift);
			*dst++ = *src++;
			while(m--)
				*dst++ = *src++;
		} else
			*dst++ = b;
	}
}

u32
Expand(u8 *src, u8 *dst)
{
	ExpandInit(src);
	ExpandMain(dst);
	return exstate.size;
}

u8 data[32*1024*1024];	// whatever - not gonna be bigger than PS2 memory...
u8 extracted[32*1024*1024];

int
main(int argc, char *argv[])
{
	if(argc < 2) {
		fprintf(stderr, "usage: %s input [output]\n", argv[0]);
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");
	if(f == nil) {
		fprintf(stderr, "can't open file %s\n", argv[1]);
		return 1;
	}
	fread(data, 1, sizeof(data), f);
	fclose(f);

	u32 sz = Expand(data, extracted);

	if(argc > 2)
		f = fopen(argv[2], "wb");
	else
		f = stdout;
	if(f == nil) {
		fprintf(stderr, "can't open file %s\n", argv[2]);
		return 1;
	}
	fwrite(extracted, 1, sz, f);

	return 0;
}
