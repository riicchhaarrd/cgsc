#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_MEAN_AND_LEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#pragma comment(lib, "user32.lib")
#define LIB_EXPORT __declspec(dllexport)
LIB_EXPORT void msgbox(const char *msg, int type)
{
	MessageBoxA(NULL, msg, "Message", type);
}
#else
#define LIB_EXPORT	
#endif


LIB_EXPORT const char *welcome_message()
{
	return "center of gravity";
}