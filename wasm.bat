if not exist "obj" mkdir obj
if not exist "bin" mkdir bin

echo "Compile sources..."

call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/compiler.c -o obj/compiler.bc
call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/scr_defaultlib.c -o obj/scr_defaultlib.bc
call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/variable.c -o obj/variable.bc
call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/vm.c -o obj/vm.bc
call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/cvector.c -o obj/cvector.bc
call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/common.c -o obj/common.bc
call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/standalone.c -o obj/standalone.bc
call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/asm.c -o obj/asm.bc
call emcc -w -Wall -m32 -std=gnu99 -DCIDSCROPT_STANDALONE -c src/resolve_exports.c -o obj/resolve_exports.bc

echo "Link objects..."

call emcc obj/compiler.bc obj/scr_defaultlib.bc obj/variable.bc obj/vm.bc obj/cvector.bc obj/common.bc obj/standalone.bc obj/asm.bc obj/resolve_exports.bc -o "bin/cidscropt.html" -ldl -lm -s EXTRA_EXPORTED_RUNTIME_METHODS="["ccall", "cwrap"]" -s ALLOW_MEMORY_GROWTH=1

echo "Done."
