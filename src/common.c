#include "common.h"

#ifdef _WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <sys/time.h>
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sys_sleep(int msec) {
#ifdef WIN32
	Sleep(msec);
#elif _POSIX_C_SOURCE >= 199309L
	struct timespec ts;
	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;
	nanosleep(&ts, NULL);
#else
	usleep(msec * 1000);
#endif
}