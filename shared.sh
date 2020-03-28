#!/bin/bash
cc="gcc -w -g -O3 -std=gnu99 -fPIC"
#cc="g++ -s -O2 -std=c++11"
cd src
mkdir -p obj
mkdir -p ../bin

$cc -c compiler.c -o obj/compiler.o
$cc -c scr_defaultlib.c -o obj/scr_defaultlib.o
$cc -c variable.c -o obj/variable.o
$cc -c vm.c -o obj/vm.o
$cc -c cvector.c -o obj/cvector.o
$cc -c common.c -o obj/common.o
$cc -c standalone.c -o obj/standalone.o
$cc -c asm.c -o obj/asm.o
$cc -c resolve_exports.c -o obj/resolve_exports.o
echo "Made all obj files."

obj="$(ls obj/*.o)"
#$cc -Wall $obj -static -o "../bin/libgsc.a" -ldl -lm
$cc -Wall $obj -shared -o "../bin/libgsc.so" -ldl -lm
echo "Done."
