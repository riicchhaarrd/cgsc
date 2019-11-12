#ifdef CIDSCROPT_STANDALONE
#include "virtual_machine.h"
#include "common.h"
#include <signal.h>
#ifdef _WIN32
#include <Windows.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
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

//#define FPS_DELTA (1000 / 20)
volatile unsigned int fps_delta = (1000 / 20);

vm_t *gvm = NULL;

void one_iter()
{
	if (vm_get_num_active_threadrunners(gvm) == 0)
	{
		vm_free(gvm);
		gvm = NULL;
#ifdef __EMSCRIPTEN__
		emscripten_cancel_main_loop();
#else
		exit(0);
#endif
		return;
	}
	vm_run_active_threads(gvm, fps_delta);
}

#ifdef __EMSCRIPTEN__

void script_command(const char *cmd)
{
	emscripten_cancel_main_loop();
	if (gvm)
	{
		vm_free(gvm);
		gvm = NULL;
	}
}

void run_script(const char *script)
{
	srand(time(0));
	signal(SIGINT, signal_int);
	compiler_t compiler;
	//vm_printf("compiler def\n");
	compiler_init(&compiler, NULL);
	//vm_printf("c init\n");
	compiler_add_source(&compiler, script, "script");
	//vm_printf("add src\n");
	if (compiler_execute(&compiler))
	{
		vm_printf("Failed compiling script.");
		return;
	}
	//vm_printf("vm_create before\n");
	vm = vm_create();
	gvm = vm; //set the global vm for emscripten loop
	vm_add_program(vm, compiler.program, compiler.program_size, "script");
	compiler_cleanup(&compiler);
	//vm_printf("exec thread main before\n");
	vm_exec_thread(vm, "main", 0);
	//vm_free(vm);
	//return;
#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(one_iter, 20, 1);
#else
	while (1)
	{
		one_iter();
		sys_sleep(fps_delta);
	}
#endif
}
#endif

int main(int argc, char **argv, char **envp)
{
	//SDL_Init(SDL_INIT_VIDEO);
#ifdef __EMSCRIPTEN__
	return 0;
#endif

	if (argc > 1 && !strcmp(argv[1], "-v"))
	{
		printf("cidscropt standalone version (https://github.com/riicchhaarrd/cidscropt)\n");
		return 0;
	}

#if 1
	if (argc < 2) {
		vm_printf("No script specified.\n");
		goto _wait_and_exit;
	}
	const char *filename = argv[1];
#else
	//const char *filename = "../examples/bench.gcx";
	const char *filename = "C:/Users/R/Desktop/ore/deps/cidscropt/examples/bench.gsc";
#ifdef _WIN32
	SetCurrentDirectoryA("C:/Users/R/Desktop/ore/deps/cidscropt/examples/");
#endif
#endif
#ifdef _WIN32
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

	vm_printf("] Starting benchmark for file '%s'\n", filename);
#endif
	srand(time(0));
	signal(SIGINT, signal_int);
	compiler_t compiler;
	compiler_init(&compiler, NULL);
	compiler_add_source_file(&compiler, filename);

	if (compiler_execute(&compiler))
	{
		vm_printf("Failed compiling '%s'.\n", filename);
		goto _wait_and_exit;
	}
#if 0
	FILE *fp = fopen("program.bin", "wb");
	fwrite(script, 1, script_size, fp);
	fclose(fp);
#endif
	vm = vm_create();
	vm_add_program(vm, compiler.program, compiler.program_size, filename);
	compiler_cleanup(&compiler);
	se_addint(vm, argc);
	varval_t *args_array = se_createarray(vm);
	for (unsigned int i = 0; i < argc; ++i)
	{
		se_addstring(vm, argv[i]);
		varval_t *av = (varval_t*)stack_pop(vm);
		se_vv_set_field(vm, args_array, i++, av);
	}
	stack_push_vv(vm, args_array);

	//push environment variables
	unsigned int i = 0;
	varval_t *env_array = se_createarray(vm);
	for (char **env = envp; *env != 0; env++)
	{
		se_addstring(vm, *env);
		varval_t *av = (varval_t*)stack_pop(vm);
		se_vv_set_field(vm, env_array, i++, av);
	}
	stack_push_vv(vm, env_array);

	vm_exec_thread(vm, "main", 3);

	while (vm_get_num_active_threadrunners(vm) > 0) {
		vm_run_active_threads(vm, fps_delta);
		sys_sleep(fps_delta);
	}
	vm_free(vm);
#ifdef _WIN32
	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);

	double interval = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;
	vm_printf("Finished in %g seconds\n", interval);
#endif
_wait_and_exit:
#ifdef _WIN32
	//vm_printf("Press [ENTER] to continue.\n");
	//getchar();
#endif
	return 0;
}
#endif
