#!/bin/sh
# try.sh SRC.c [FUNCTAB] - compile a candidate with the retail compiler
# and score it against the ROM.  Same criterion as matching/check.py.
#
#   ./try.sh src/matrixdrive.c matrixdrive-functions.txt
#
# Compiler: freesce's ee-gcc 2.9-ee-991111 (the retail one).  Binutils:
# the SCE 3.0.3 set, because freesce's objcopy has no -j (this is what
# matching/campaign/README.md says about EETOOLS).
#
# The build goes through .s so unpad.py can strip 991111's R5900
# short-loop nop padding, which retail does not have - see unpad.py.
# Set NOUNPAD=1 to compare against the raw compiler output.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
GCC=/usr/local/freesce/ee/gcc/bin/ee-gcc
IMG=${IMG:-/u/aap/src/osdsys/expanded.bin}
export EETOOLS=/usr/local/sce/ee/gcc/bin

SRC=$1
TAB=${2:-$HERE/functions.txt}
B=$(basename "$SRC" .c)
OBJ=$HERE/build/$B.o
ASM=$HERE/build/$B.s
mkdir -p "$HERE/build"

# osdbits' flags; -Wall stays on because implicit-declaration warnings
# have twice been the thing that found a mismatch (see notes.md).
$GCC -O2 -Wall -fno-common -fno-strict-aliasing -S "$SRC" -o "$ASM"
[ -n "$NOUNPAD" ] || python3 "$HERE/unpad.py" "$ASM"
$GCC -c "$ASM" -o "$OBJ"
exec python3 "$HERE/check.py" "$OBJ" "$IMG" "$TAB"
