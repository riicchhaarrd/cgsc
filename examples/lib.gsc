
//../bin/gsc lib.gcx
lib() { set_ffi_lib("lib/lib.so"); }

main()
{
lib();

s = (string)$welcome_message();
printf("%\n",s);
}
