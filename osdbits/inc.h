#include <eekernel.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <libdev.h>
#include <eeregs.h>
#include <libgraph.h>
#include <libdma.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>
#include <sifrpc.h>
#include <libpad.h>

#define nil NULL
typedef u_long128 u128;
typedef u_long u64;
typedef unsigned int u32;
typedef int i32;
typedef unsigned short u16;
typedef short i16;
typedef unsigned char u8;
typedef signed char i8;

#define ALIGN16 __attribute__ ((aligned(16)))

#define PI 3.1415927f
#define TAU (2.0f*PI)

#define UNCACHED(a) ((void*)(((u32)a)|0x20000000U))
#define ACCEL(a) ((void*)(((u32)a)|0x30000000U))
#define NORMALMEM(a) ((void*)(((u32)a)&0x0FFFFFFFU))
#define DMASPR(a) ((void*)((((u32)a)&0x0FFFFFFFU)|0x80000000U))

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define clamp(a, lo, hi) ((a) < (lo) ? (lo) : (a) > (hi) ? (hi) : (a))

typedef struct Rect Rect;
struct Rect
{
	int x, y;
	int w, h;
};

typedef struct Color Color;
struct Color
{
	u32 r, g, b, a;
};

int IsPAL(void);
int GetLanguage(void);

extern sceGsDBuff db;
extern int evenOddFrame;
extern int evenOddField;
extern int screenW, screenH;

extern int waitFrameSema;
extern int drawEndSema;
extern int swapSema;
extern int drawStartSema;

void StartFrame(void);
void WaitNextFrame(void);

int MakeOpeningThread(void);
