//fun segmentation fault because was compiling for x86_64
//i guess just find your libc or desired library path and change it
//just compile cidscropt and do ldd bin/cidscropt for libc atleast

std() { set_ffi_lib("/lib/i386-linux-gnu/i686/cmov/libc.so.6"); }

main()
{
std();
//the $ in front of printf means don't call the builtin script function but call the version through FFI
$printf("Hello World!\n");

i = atoi("12345");
printf("i = %\n", i);

p = malloc(1024);
$snprintf(p,1024,"%d %s\n", i, "ok");
$printf("%s", p);
free(p);
}
