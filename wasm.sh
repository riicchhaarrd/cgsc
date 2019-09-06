#!/bin/bash
cc="emcc -w -Wall -m32 -s ASSERTIONS=1 -std=gnu99 -DCIDSCROPT_STANDALONE -s ALLOW_MEMORY_GROWTH=1"
cd src
mkdir -p obj
mkdir -p ../bin

$cc -c compiler.c -o obj/compiler.bc
$cc -c scr_defaultlib.c -o obj/scr_defaultlib.bc
$cc -c variable.c -o obj/variable.bc
$cc -c vm.c -o obj/vm.bc
$cc -c cvector.c -o obj/cvector.bc
$cc -c common.c -o obj/common.bc
$cc -c standalone.c -o obj/standalone.bc
$cc -c asm.c -o obj/asm.bc
$cc -c resolve_exports.c -o obj/resolve_exports.bc
echo "Made all bc files."

obj="$(ls obj/*.bc)"
$cc $obj -o "../bin/cidscropt.html" -ldl -lm -s EXPORTED_FUNCTIONS='["_main", "_run_script", "_script_command"]' -s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'
echo "Done."
