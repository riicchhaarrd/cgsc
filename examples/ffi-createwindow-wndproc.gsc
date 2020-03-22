#pragma comment(lib, "msvcrt.dll")
#pragma comment(lib, "user32.dll")
#pragma comment(lib, "kernel32.dll")
#pragma comment(lib, "gdi32.dll")

#define CS_HREDRAW 0x2
#define CS_VREDRAW 0x1
#define IDC_ARROW 32512
#define CW_USEDEFAULT (-2147483648)

#define WS_OVERLAPPED       0x00000000
#define WS_POPUP            0x80000000
#define WS_CHILD            0x40000000
#define WS_MINIMIZE         0x20000000
#define WS_VISIBLE          0x10000000
#define WS_DISABLED         0x08000000
#define WS_CLIPSIBLINGS     0x04000000
#define WS_CLIPCHILDREN     0x02000000
#define WS_MAXIMIZE         0x01000000
#define WS_CAPTION          0x00C00000
#define WS_BORDER           0x00800000
#define WS_DLGFRAME         0x00400000
#define WS_VSCROLL          0x00200000
#define WS_HSCROLL          0x00100000
#define WS_SYSMENU          0x00080000
#define WS_THICKFRAME       0x00040000
#define WS_GROUP            0x00020000
#define WS_TABSTOP          0x00010000

#define WS_MINIMIZEBOX      0x00020000
#define WS_MAXIMIZEBOX      0x00010000

#define WS_TILED            0x00000000

#define WS_OVERLAPPEDWINDOW (13565952)

typedef struct {
  UINT      style;
  WNDPROC   lpfnWndProc;
  int       cbClsExtra;
  int       cbWndExtra;
  HINSTANCE hInstance;
  HICON     hIcon;
  HCURSOR   hCursor;
  HBRUSH    hbrBackground;
  LPCSTR    lpszMenuName;
  LPCSTR    lpszClassName;
} WNDCLASSA;

typedef struct {
    HWND        hwnd;
    UINT        message;
    WPARAM      wParam;
    LPARAM      lParam;
    DWORD       time;
    int pt_x;
	int pt_y;
} MSG;

typedef struct {
    HDC         hdc;
    BOOL        fErase;
    RECT        rcPaint;
    BOOL        fRestore;
    BOOL        fIncUpdate;
    BYTE        rgbReserved[32];
} PAINTSTRUCT;

#define WM_DESTROY 2
#define WM_PAINT 15

wnd_proc(wnd,msg,wp,lp)
{
	if(msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(wnd,msg,wp,lp);
}

create_window()
{
	wc = new WNDCLASSA;
	memset(wc,0,wc.size);
	wc->style = CS_HREDRAW | CS_VREDRAW;
	wc->lpfnWndProc = ::wnd_proc;
	wc->hInstance = NULL;
	wc->hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc->lpszClassName = "window_class";
	wc->hbrBackground = GetStockObject(0);
	RegisterClassA(wc);
	
	wnd = CreateWindowExA(0, "window_class", "title", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
	0,0,800,600,
	NULL, NULL, NULL, NULL);
	printf("wnd = %\n", wnd);
	return wnd;
}

main()
{
	wnd = create_window();
	ShowWindow(wnd,1);
	
	msg = new MSG;
	while(GetMessageA(msg, 0, 0, 0))
	{
		TranslateMessage(msg);
		DispatchMessageA(msg);
	}
	
}