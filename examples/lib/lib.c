#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

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

typedef struct
{
	char test[1024];
	int a,b,c,d;
} msg_t;

LIB_EXPORT msg_t *get_message() {
	msg_t *m = (msg_t*)malloc(sizeof(msg_t));
	m->a=1337;
	m->b=2048;
	m->c=1024;
	m->d=4096;
	return m;
}

LIB_EXPORT void test_ptr(int *p)
{
	*p = 212121;
}