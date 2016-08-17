#ifdef CIDSCROPT_STANDALONE
#include "virtual_machine.h"
#include "common.h"
#include <signal.h>

vm_t *vm = NULL;

void signal_int(int parm) {
	if (!vm)
		return;
	vm->is_running = false;
}

#include <windows.h>


#define FPS_DELTA (1000/20)
int main(int argc, char **argv) {
	if (argc < 2) {
		printf("No script specified.\n");
		goto _wait_and_exit;
	}

	srand(time(0));
	signal(SIGINT, signal_int);

	char *script;
	int script_size;
	if (parser_compile(argv[1], &script, &script_size)) {
		printf("Failed compiling '%s'.\n", argv[1]);
		goto _wait_and_exit;
	}

	vm = vm_create(script, script_size);
	free(script);
	vm_exec_thread(vm, "main", 0);

	while (vm_get_num_active_threadrunners(vm) > 0) {
		vm_run_active_threads(vm, FPS_DELTA);
		sys_sleep(FPS_DELTA);
	}
	vm_free(vm);
_wait_and_exit:
#ifdef _WIN32
	printf("Press [ENTER] to continue.\n");
	getchar();
#endif
	return 0;
}
#endif