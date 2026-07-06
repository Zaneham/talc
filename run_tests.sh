#!/bin/sh
# Take every sample TAL program the whole way: talc to C, C to a binary,
# then run it. A sample that fails to compile or run fails the suite.
set -e

TALC=./talc
[ -x ./talc.exe ] && TALC=./talc.exe

mkdir -p build
for tal in samples/*.tal; do
	base=$(basename "$tal" .tal)
	"$TALC" "$tal" -o "build/$base.c"
	gcc -std=c99 -Isrc "build/$base.c" -o "build/$base"
	printf '%-12s -> %s\n' "$base" "$(./build/$base)"
done
echo "all samples built and ran"
