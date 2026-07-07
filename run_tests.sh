#!/bin/sh
# Compile, build and run each sample, then diff it against its golden file.
set -e

TALC=./talc
[ -x ./talc.exe ] && TALC=./talc.exe

mkdir -p build
fail=0
for tal in samples/*.tal; do
	base=$(basename "$tal" .tal)
	want="samples/$base.expected"
	"$TALC" "$tal" -o "build/$base.c"
	gcc -std=c99 -Isrc "build/$base.c" -o "build/$base"
	./build/$base | tr -d '\r' > "build/$base.out"

	if [ ! -f "$want" ]; then
		echo "FAIL $base: no golden file at $want"
		fail=1
	else
		tr -d '\r' < "$want" > "build/$base.want"
		if diff -u "build/$base.want" "build/$base.out" >/dev/null; then
			echo "ok   $base"
		else
			echo "FAIL $base: output differs from $want"
			diff -u "build/$base.want" "build/$base.out" || true
			fail=1
		fi
	fi
done

if [ "$fail" -ne 0 ]; then
	echo "some samples failed"
	exit 1
fi
echo "all samples passed"
