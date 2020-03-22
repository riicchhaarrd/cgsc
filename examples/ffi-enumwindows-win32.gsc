#pragma comment(lib, "msvcrt.dll")
#pragma comment(lib, "user32.dll")

__stdcall test(hwnd, lparam)
{
	p = malloc(1024);
	GetWindowTextA(hwnd,p,1024);
	printf("text = %\n", cstring(p));
	free(p);
	level.n = level.n + 1;

	return true;
}

main()
{	level.n = 0;
    EnumWindows(::test, 78);
	
	MessageBoxA(0,level.n + " windows",0,0);
}