#ifdef CIDSCROPT_STANDALONE
#include "virtual_machine.h"
#include "common.h"
#include <signal.h>
#ifdef _WIN32
#include <Windows.h>
#endif
/* used this to test C SDL ffi */
//#include <SDL.h>
//#undef main

vm_t *vm = NULL;

void signal_int(int parm) {
	if (!vm)
		return;
	vm->is_running = false;
}

#define FPS_DELTA (1000/20)
int main(int argc, char **argv) {
	//SDL_Init(SDL_INIT_VIDEO);

#if 0
	if (argc < 2) {
		printf("No script specified.\n");
		goto _wait_and_exit;
	}
	const char *filename = argv[1];
#else
	const char *filename = "../examples/bench.gcx";
#endif
#ifdef _WIN32
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

	printf("] Starting benchmark for file '%s'\n", filename);
#endif
	srand(time(0));
	signal(SIGINT, signal_int);

	char *script;
	int script_size;
	if (parser_compile(filename, &script, &script_size)) {
		printf("Failed compiling '%s'.\n", filename);
		goto _wait_and_exit;
	}
#if 0
	FILE *fp = fopen("program.bin", "wb");
	fwrite(script, 1, script_size, fp);
	fclose(fp);
#endif
	vm = vm_create(script, script_size);
	free(script);
	vm_exec_thread(vm, "main", 0);

	while (vm_get_num_active_threadrunners(vm) > 0) {
		vm_run_active_threads(vm, FPS_DELTA);
		sys_sleep(FPS_DELTA);
	}
	vm_free(vm);
#ifdef _WIN32
	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);

	double interval = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
	printf("Finished in %g seconds\n", interval);
#endif
_wait_and_exit:
#ifdef _WIN32
	printf("Press [ENTER] to continue.\n");
	getchar();
#endif
	return 0;
}
#endif
