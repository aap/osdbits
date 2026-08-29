#!/bin/sh
# gen_all.sh SRC OUTDIR FIRSTLINE LASTLINE
# run every workbench sweep generator over every site of SRC into OUTDIR/*
set -e
SRC=$1; OUT=$2; LO=$3; HI=$4
export PYTHONPATH=/u/aap/othersrc/n64-decomp-workbench/src
W="python3 -m decomp_workbench"
rm -rf "$OUT"; mkdir -p "$OUT"
$W sweep commute "$SRC" --write "$OUT/C" >/dev/null 2>&1 || true
$W sweep copies  "$SRC" --write "$OUT/K" >/dev/null 2>&1 || true
L=$LO
while [ $L -le $HI ]; do
  $W sweep hoist "$SRC" --line $L --class H,O,P,A --write "$OUT/H$L" >/dev/null 2>&1 || true
  L=$((L+1))
done
for t in $($W sweep donors "$SRC" --target x --json 2>/dev/null >/dev/null; echo); do :; done
