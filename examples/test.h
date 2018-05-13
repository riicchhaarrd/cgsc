#define NULL 0

gl() { set_ffi_lib("opengl32.dll"); }
std() { set_ffi_lib("msvcrt.dll"); }
u32() { set_ffi_lib("user32.dll"); }
k32() { set_ffi_lib("kernel32.dll"); }
s32() { set_ffi_lib("shell32.dll"); }
sdl2() { set_ffi_lib("SDL2.dll"); }

test() { std(); $printf("test!\n"); }
