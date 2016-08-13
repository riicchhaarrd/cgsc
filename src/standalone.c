#ifndef _WIN32 //for win32 visual studio cba to exclude it somehow, and for linux just exclude it from the library build script
#include "virtual_machine.h"

#if 1 //not using sleep atm just for testing

#ifdef WIN32
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
#endif

#if 1

#include <signal.h>

volatile sig_atomic_t signaled = 0;

vm_t *glob_vm = NULL;

void signal_int(int parm) {
	if (glob_vm == NULL)
		return;
	glob_vm->is_running = false;
	printf("Stopping...\n");
}

int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Drag the script file onto here.\n");
		return 0;
	}

	srand(time(0));
	signal(SIGINT, signal_int);

	char *script;
	int script_size;
	if (parser_compile(argv[1], &script, &script_size)) {
		goto _exit_this_now_pls;
	}

	vm_t *vm = vm_create(script, script_size);
	glob_vm = vm;
	free(script);
	{

		vm_exec_thread(vm, "main", 0);

		while (vm_get_num_active_threadrunners(vm) > 0) {
			vm_run_active_threads(vm, 1000 / 20);
			sys_sleep(1000 / 20);
		}
	}
	vm_free(vm);

_exit_this_now_pls:
	return 0;
}
#endif

#endif