#!/bin/sh
# Build the GS-dump replay harness against Sony's 1998 i386 libgpu2.a.
#
# Sony's archive is 32-bit i386 only, so everything here is -m32.  Void's gcc
# is not multilib, so clang cross-compiles to i386 and GNU ld links by hand
# (recipe lifted from /u/aap/othersrc/libgpu2/port/build.sh, which is aap's).
#
# Prerequisites (Void):  xbps-install -S glibc-devel-32bit libX11-32bit
set -e
cd "$(dirname "$0")"

LG2=/u/aap/othersrc/libgpu2
ROOT=$LG2/gpu2/i386-pc-linux-gnu
INC=$ROOT/include
LIB=$LG2/port/libgpu2-patched.a      # patched GS_SaveImage (4-byte pixels)

CC="clang --target=i386-linux-gnu -O2 -fno-pie -fno-stack-protector -g"

$CC -I"$INC" -include compat.h -c shims.c   -o shims.o
$CC -I"$INC" -include compat.h -c probe.c   -o probe.o
$CC -I"$INC" -include compat.h -c swz.c     -o swz.o
$CC -I"$INC" -include compat.h -c fmt.c     -o fmt.o
$CC -I"$INC" -include compat.h -c regprobe.c -o regprobe.o
$CC -I"$INC" -include compat.h -c gsreplay.c -o gsreplay.o

link() {
    out=$1; shift
    ld -m elf_i386 -o "$out" \
        --dynamic-linker=/lib/ld-linux.so.2 \
        /usr/lib32/crt1.o /usr/lib32/crti.o \
        "$@" shims.o \
        "$LIB" \
        -L/usr/lib32 -l:libX11.so.6 -lm -lc -l:libc_nonshared.a \
        /usr/lib32/crtn.o
}

link probe    probe.o
link swz      swz.o
link fmt      fmt.o
link regprobe regprobe.o
link gsreplay gsreplay.o
echo "built: $(pwd)/probe $(pwd)/gsreplay"
