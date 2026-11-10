#!/bin/bash
# Syntax-checks every 3DS source file on a PC (tools/host/shim + tools/host/stub stand in for libctru,
# citro3d and citro2d). Prints the errors, exits non-zero if there are any.
cd "$(dirname "$0")/../.."
fail=0
# render/Renderer.cpp is the citro3d GPU layer: CI compiles it
for f in $(find source -name '*.cpp' ! -path source/render/Renderer.cpp); do
  out=$(g++ -std=gnu++17 -fsyntax-only -fno-rtti -fno-exceptions -w -D__3DS__ -include tools/host/stub/host3ds_extra.h \
        -Isource -Itools/host/stub -Itools/host/shim "$f" 2>&1 | grep -E 'error')
  if [ -n "$out" ]; then echo "$out"; fail=1; fi
done
exit $fail
