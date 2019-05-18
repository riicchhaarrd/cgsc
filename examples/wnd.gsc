#pragma comment(lib, "user32.dll")
#pragma comment(lib, "kernel32.dll")


#define WS_EX_APPWINDOW 0x40000
#define WS_OVERLAPPEDWINDOW 0xcf0000
#define WS_CAPTION 0xc00000
#define SW_SHOWNORMAL 1
#define SW_SHOW 5
#define CS_HREDRAW 2
#define CS_VREDRAW 1
#define CW_USEDEFAULT 0x80000000
#define WM_DESTROY 2
#define WHITE_BRUSH 0

main()
{
wnd = CreateWindowExA(
    0,"static","test",
    WS_OVERLAPPEDWINDOW | WS_CAPTION,
    CW_USEDEFAULT, CW_USEDEFAULT,
300,300,0,0,0,0);

ShowWindow(wnd,SW_SHOWNORMAL);
}