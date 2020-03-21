#include "virtual_machine.h"
#include "common.h"
#include "asm.h"
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <winsock2.h>
#include <wsipx.h>
#include <Ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define SOCKET int
#include <dlfcn.h>
#include <sys/mman.h>
#endif
#include <signal.h>
#include "parser.h"
#include "dynstring.h"
//#pragma comment(lib, "SDL2.lib")

bool resolve_adr(const char *addr_str, struct sockaddr_in *adr) {
	char ip_str[256] = { 0 };
	int ip_stri = 0;
	const char *port_str = strrchr(addr_str, ':');

	if (port_str != NULL) {
		for (const char *c = addr_str; *c && c != port_str; c++) {
			if (ip_stri + 1 > sizeof(ip_str))
				break;
			ip_str[ip_stri++] = *c;
		}
	}
	else
		snprintf(ip_str, sizeof(ip_str), "%s", addr_str);

	struct hostent *host = 0;
	if ((host = gethostbyname(ip_str)) == NULL) {
		vm_printf("failed to resolve hostname '%s'\n", addr_str);
		return false;
	}
	adr->sin_family = AF_INET;
	adr->sin_addr = *(struct in_addr*)host->h_addr;

	if (port_str != NULL) {
		adr->sin_port = htons(atoi(port_str + 1));
	}
	memset(&adr->sin_zero, 0, sizeof(adr->sin_zero));
	//Common::Printf("Resolved address: %s:%s to %s\n", ip_str, (port_str + 1), adr->getIPString());
	return true;
}

static int sf_sendpacket(vm_t *vm)
{
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) {
		vm_printf("failed to create socket\n");
		return 0;
	}
	const char *ip = se_getstring(vm, 0);
	const char *buf = se_getstring(vm, 1);
	int bufsz = se_getint(vm, 2);
	struct sockaddr_in adr;
	if (!resolve_adr(ip, &adr)) {
		vm_printf("failed to resolve address '%s'\n", ip);
		return 0;
	}
	int ret = sendto(sock, buf, bufsz, 0, (struct sockaddr*)&adr, sizeof(adr));
	if (ret == SOCKET_ERROR) {
		vm_printf("sendto failed\n");
		return 0;
	}
	static char recvbuffer[16384];
	struct sockaddr_in from;
	memset(&from.sin_zero, 0, sizeof(from.sin_zero));
	int fromlen = sizeof(from);
	ret = recvfrom(sock, recvbuffer, sizeof(recvbuffer), 0, (struct sockaddr*)&from, (socklen_t*)&fromlen);
	if (ret == SOCKET_ERROR) {
		vm_printf("recvfrom failed\n");
		return 0;
	}
	se_addstring(vm, recvbuffer);
	return 1;
}

int sf_print(vm_t *vm) {
	for (int i = 0; i < se_argc(vm); i++)
		vm_printf("%s", se_getstring(vm, i));
	return 0;
}

int sf_println(vm_t *vm) {
	for (int i = 0; i < se_argc(vm); i++)
		vm_printf("%s\n", se_getstring(vm, i));
	return 0;
}

int sf_sprintf(vm_t *vm) {
	const char *s = se_getstring(vm, 0);
	static char string[1024] = { 0 };
	int stri = 0;
	int parmidx = 0;
	for (int i = 0; i < strlen(s); i++) {
		if (stri >= sizeof(string))
			break;

		if (s[i]=='%' || (s[i] == '{' && isdigit(s[i + 1]) && (i + 2) < sizeof(string) && s[i + 2] == '}')) {
			int parmnum = s[i + 1] - '0' + 1;
			if (s[i] == '%')
				parmnum = ++parmidx;
			const char *parm = se_getstring(vm, parmnum);
			//strncat(&string[stri], parm, sizeof(string) - strlen(string) - strlen(parm) + 1);
			int max_copy = sizeof(string) - strlen(string);
			if (strlen(parm) < max_copy)
				max_copy = strlen(parm);
			strncpy(&string[stri], parm, max_copy);

			stri += strlen(parm);
			if(s[i]!='%')
				i += 2;
			continue;
		}
		string[stri++] = s[i];
	}
	string[stri] = '\0';
	se_addstring(vm, string);
	return 1;
}

int sf_printf(vm_t *vm) {
	sf_sprintf(vm);
	varval_t *vv = stack_pop_vv(vm);
	vm_printf("%s", se_vv_to_string(vm, vv));
	se_vv_free(vm, vv);
	return 0;
}

int sf_get_time(vm_t *vm) {
	se_addint(vm, time(0));
	return 1;
}

int sf_getchar(vm_t *vm) {
	int c = getchar();
	char s[2];
	if (c == EOF) {
		se_addnull(vm);
		return 1;
	}

	s[0] = c;
	s[1] = 0;
	se_addstring(vm, s);
	return 1;
}

int sf_get_in_string(vm_t *vm) {
	char string[128] = { 0 };

	if (fgets(string, sizeof(string), stdin) != NULL) {
		if (strlen(string) > 0 && string[strlen(string) - 1] == '\n')
			string[strlen(string) - 1] = 0;
		se_addstring(vm, string);
	}
	else
		se_addnull(vm);
	return 1;
}

int sf_isdefined(vm_t *vm) {
	varval_t *vv = se_argv(vm, 0);
	se_addbool(vm, VV_TYPE(vv) != VAR_TYPE_NULL);
	return 1;
}

int sf_typeof(vm_t *vm) {
	varval_t *vv = se_argv(vm, 0);
	se_addstring(vm, VV_TYPE_STRING(vv));
	return 1;
}

int sf_cstring(vm_t *vm) {
	intptr_t ptr = se_getint(vm, 0);
	se_addstring(vm, (const char*)ptr);
	return 1;
}

int sf_sizeof(vm_t *vm) {

	int vv_integer_internal_size(varval_t *vv);
	varval_t *vv = se_argv(vm, 0);
	se_addint(vm, vv_integer_internal_size(vv));
	return 1;
}

int sf_randomint(vm_t *vm) {
	int max = se_getint(vm, 0);
	if (max <= 0)
		max = 1;
	int rnd = rand() % max;
	se_addint(vm, rnd);
	return 1;
}

//https://stackoverflow.com/questions/5289613/generate-random-float-between-two-floats
float RandomFloat(float a, float b) {
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = b - a;
	float r = random * diff;
	return a + r;
}

int sf_randomfloat(vm_t *vm) {
	float max = se_getfloat(vm, 0);
	float f = RandomFloat(0.f, max);
	se_addfloat(vm, f);
	return 1;
}

#define ADD_CSTD_MATH_LIB_FUNC(name) \
int sf_m_##name(vm_t *vm) { \
	float v = se_getfloat(vm, 0); \
	se_addfloat(vm, (float)name(v)); \
	return 1; \
}

#define ADD_CSTD_LIB_FUNC_INT(name) \
int sf_##name(vm_t *vm) { \
	int v = se_getint(vm, 0); \
	se_addint(vm, name(v)); \
	return 1; \
}

ADD_CSTD_LIB_FUNC_INT(abs)
ADD_CSTD_MATH_LIB_FUNC(sinf)
ADD_CSTD_MATH_LIB_FUNC(cosf)

int sf_spawnstruct(vm_t *vm) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_OBJECT);
	stack_push_vv(vm, vv);
	return 1;
}

static int _get_obj_vars_cb(vm_t *vm, const char *field_name, vt_object_field_t *field, void *userdata, vm_flags_t flags)
{
	VV_CAST_VAR(arr, userdata);
	se_addstring(vm, field_name);
	VV_CAST_VAR(av, stack_pop(vm));
	se_vv_set_field(vm, arr, se_vv_container_size(vm,arr), av);
	return 0;
}

int sf_get_object_vars(vm_t *vm)
{
	varval_t *vv = se_argv(vm, 0);
	if (VV_TYPE(vv) != VAR_TYPE_OBJECT)
		return se_error(vm, "not an object");

	varval_t *arr = se_createarray(vm);
	vt_object_t *obj = vv->as.obj;
	se_vv_enumerate_fields(vm, vv, _get_obj_vars_cb, arr);
	stack_push_vv(vm, arr);
	return 1;
}

#define ADD_IS_STR_IDX_TYPE_STD_FUNC(STANDARD_LIB_NAME) \
int sf_##STANDARD_LIB_NAME(vm_t *vm) { \
	const char *str = se_getstring(vm, 0); \
	bool is = true; \
	for (int i = 0; i < strlen(str); i++) { \
		if (!STANDARD_LIB_NAME(str[i])) { \
			is = false; \
			break; \
		} \
	} \
	se_addbool(vm, is); \
	return 1; \
}

ADD_IS_STR_IDX_TYPE_STD_FUNC(isdigit)
ADD_IS_STR_IDX_TYPE_STD_FUNC(isalnum)
ADD_IS_STR_IDX_TYPE_STD_FUNC(islower)
ADD_IS_STR_IDX_TYPE_STD_FUNC(isupper)
ADD_IS_STR_IDX_TYPE_STD_FUNC(isalpha)

int sf_tolower(vm_t *vm) {
	const char *str = se_getstring(vm, 0);
	char *copy = (char*)malloc(strlen(str) + 1);
	snprintf(copy, strlen(str) + 1, "%s", str);
	for (int i = 0; i < strlen(str); i++) {
		copy[i] = tolower(str[i]);
	}
	se_addstring(vm, copy);
	free(copy);
	return 1;
}

int sf_toupper(vm_t *vm) {
	const char *str = se_getstring(vm, 0);
	char *copy = (char*)malloc(strlen(str) + 1);
	snprintf(copy, strlen(str) + 1, "%s", str);
	for (int i = 0; i < strlen(str); i++) {
		copy[i] = toupper(str[i]);
	}
	se_addstring(vm, copy);
	free(copy);
	return 1;
}

int sf_substr(vm_t *vm) {
	const char *str = se_getstring(vm, 0);
	unsigned sub = se_getint(vm, 1);
	unsigned end = se_getint(vm, 2);
	
	if(end!=0)
		end += sub;

	char *copy = (char*)malloc(strlen(str) + 1);
	snprintf(copy, strlen(str) + 1, "%s", str);

	if (end != 0 && end != sub && !(end > strlen(str)))
		copy[end] = 0;

	if (sub > strlen(str))
		se_addstring(vm, copy); //prob should warn that the sub is too long
	else
		se_addstring(vm, &copy[sub]);
	free(copy);
	return 1;
}

static int _glue_array_fields(vm_t *vm, const char *field_name, vt_object_field_t *field, void **userdata, vm_flags_t flags)
{
	dynstring* s = (dynstring*)userdata[0];
	const char *glue = (const char*)userdata[1];
	const char *value_as_string = se_vv_to_string(vm, field->value);
	dynadd(s, value_as_string);
	if(!flags) //if not last element then add glue
	dynadd(s, glue);
	return 0;
}

int sf_implode(vm_t *vm)
{
	const char *glue = se_getstring(vm, 0);
	VV_CAST_VAR(arr, se_argv(vm, 1));

	if (VV_TYPE(arr) != VAR_TYPE_ARRAY)
		return se_error(vm, "not an array");
	dynstring s = dynalloc(32);
	void *ud[2] = {
		(void*)&s,
		(void*)glue
	};
	se_vv_enumerate_fields(vm, arr, _glue_array_fields, ud);
	se_addstring(vm, s);
	dynfree(&s);
	return 1;
}

int sf_explode(vm_t *vm) {
	const char *str = se_getstring(vm, 1);
	const char *delim = se_getstring(vm, 0);
	varval_t *arr = se_createarray(vm);

	char *copy = (char*)malloc(strlen(str) + 1);
	snprintf(copy, strlen(str) + 1, "%s", str);
	char *tok = strtok(copy, delim);
	int i = 0;
	while (tok != NULL) {
		se_addstring(vm, tok);
		varval_t *av = (varval_t*)stack_pop(vm);
		se_vv_set_field(vm, arr, i++, av);
		tok = strtok(NULL, delim);
	}
	free(copy);
	stack_push_vv(vm, arr);
	return 1;
}

int sf_strtok(vm_t *vm) {
	const char *str = se_getstring(vm, 0);
	const char *delim = se_getstring(vm, 1);
	varval_t *arr = se_createarray(vm);

	char *copy = (char*)malloc(strlen(str) + 1);
	snprintf(copy, strlen(str) + 1, "%s", str);
	char *tok = strtok(copy, delim);
	int i = 0;
	while (tok != NULL) {
		se_addstring(vm, tok);
		varval_t *av = (varval_t*)stack_pop(vm);
		se_vv_set_field(vm, arr, i++, av);
		tok = strtok(NULL, delim);
	}
	free(copy);
	stack_push_vv(vm, arr);
	return 1;
}

int sf_strpos(vm_t *vm) {
	const char *haystack = se_getstring(vm, 0);
	const char *needle = se_getstring(vm, 1);

	int loc = -1;
	
	const char *d = strstr(haystack, needle);
	if (d != NULL)
		loc = d - haystack + 1;
	se_addint(vm, loc);

	return 1;
}

static void obj_file_deconstructor(vm_t *vm, FILE *fp) {
	if(fp!=NULL)
		fclose(fp);
}

int sf_fopen(vm_t *vm) {
	const char *filename = se_getstring(vm, 0);
	const char *mode = se_getstring(vm, 1);

	FILE *fp = fopen(filename, mode);
	if (NULL == fp) {
		se_addnull(vm);
		return 1;
	}
	varval_t *vv = se_createobject(vm, VT_OBJECT_FILE,NULL,NULL,obj_file_deconstructor); //todo add the file deconstructor? :D
	vv->as.obj->obj = (void*)fp;
	stack_push_vv(vm, vv);
	return 1;
}

int sf_fwritevalue(vm_t *vm) {
	varval_t *vv = se_argv(vm, 0);
	if (VV_TYPE(vv) != VAR_TYPE_OBJECT)
		return 0;
	varval_t *val = se_argv(vm, 1);
	int sz = se_getint(vm, 2);

	FILE *fp = (FILE*)vv->as.obj->obj;
	
	size_t written = 0;

	switch (VV_TYPE(val)) {
	case VAR_TYPE_INT:
		if(!sz || sz > 4)
		written = fwrite(&val->as.integer, sizeof(val->as.integer), 1, fp);
		else
		{
			written = fwrite(&val->as.integer, sz, 1, fp);
		}
		break;
	case VAR_TYPE_FLOAT:
		written = fwrite(&val->as.number, sizeof(val->as.number), 1, fp);
		break;
	case VAR_TYPE_STRING:
	case VAR_TYPE_INDEXED_STRING: {
		const char *as_str = se_vv_to_string(vm, val);
		written = fwrite(as_str, sz==0?strlen(as_str):strlen(as_str)+1, 1, fp);
	} break;
	default:
		vm_printf("'%s' unsupported value type! no fwrite available\n", VV_TYPE_STRING(val));
		break;
	}
	se_addint(vm, written);
	return 1;
}

int sf_read_text_file(vm_t *vm) {
	const char *filename = se_getstring(vm, 0);
	int filesize = 0;
	char *buf = NULL;
	if (!read_text_file(filename, &buf, &filesize)) {
		se_addstring(vm, buf);
		free(buf);
		return 1;
	}
	return 0;
}

int sf_write_text_file(vm_t *vm) {
	const char *filename = se_getstring(vm, 0);
	const char *type = se_getstring(vm, 1);
	const char *txt = se_getstring(vm, 2);
	FILE *fp = fopen(filename, type);
	fprintf(fp, "%s", txt);
	fclose(fp);
	return 0;
}

int sf_int(vm_t *vm) {
	int i = se_getint(vm, 0);
	se_addint(vm, i);
	return 1;
}

int sf_float(vm_t *vm) {
	float f = se_getfloat(vm, 0);
	se_addfloat(vm, f);
	return 1;
}

int sf_string(vm_t *vm) {
	const char *str;
	if (se_getnumparams(vm) < 1)
		str = "";
	else
		str = se_getstring(vm, 0);

	se_addstring(vm, str);
	return 1;
}

//some windows only functions
#ifdef _WIN32
#include <Windows.h>
#endif

int sf_setpixel(vm_t *vm) {
	float xyz[3];
	se_getvector(vm, 0, xyz);
	float rgb[3];
	se_getvector(vm, 1, rgb);
#ifdef _WIN32
	HWND console_hwnd = GetConsoleWindow();
	HDC console_hdc = GetDC(console_hwnd);

	SetPixelV(console_hdc, (int)xyz[0], (int)xyz[1], RGB((int)rgb[0], (int)rgb[1], (int)rgb[2]));
	ReleaseDC(console_hwnd, console_hdc);
#else
	vm_printf("this function has no support for linux at the moment.\n");
#endif
	return 0;
}

int sf_rename(vm_t *vm) {
	const char *path = se_getstring(vm, 0);
	const char *new_path = se_getstring(vm, 1);
	int ret = rename(path, new_path);
	se_addint(vm, ret);
	return 1;
}

int sf_remove(vm_t *vm) {
	const char *path = se_getstring(vm, 0);
	int ret = remove(path);
	se_addint(vm, ret);
	return 1;
}

int sf_listdir(vm_t *vm) {
	const char *path = se_getstring(vm, 0);
	file_info_t *files;
	size_t numfiles = 0;
	sys_get_files_from_path(path, &files, &numfiles);

	varval_t *arr = se_createarray(vm);

	for (int i = 0; i < numfiles; i++) {
		se_addstring(vm, files[i].name);
		varval_t *av = (varval_t*)stack_pop(vm);
		se_vv_set_field(vm, arr, i, av);
		se_vv_free(vm, av); //important forgot this because set_field is duplicating it we need to free the one we made
	}
	free(files);
	stack_push_vv(vm, arr);
	return 1;
}

int sf_buffer(vm_t *vm)
{
	int sz = se_getint(vm, 0);

	void vt_buffer_deconstructor(vm_t *vm, vt_buffer_t *vtb);
	varval_t *vv = se_createobject(vm, VT_OBJECT_BUFFER, NULL, NULL, (void*)vt_buffer_deconstructor); //todo add the file deconstructor? :D
	vt_buffer_t *vtb = (vt_buffer_t*)malloc(sizeof(vt_buffer_t) + sz);
	vtb->size = sz;
	vtb->data = ((char*)vtb) + sizeof(vt_buffer_t);
	//vm_printf("spawning %s\n", cs->name);
	vtb->type = -1;
	vv->as.obj->obj = vtb;
	stack_push_vv(vm, vv);
	return 1;
}

int sf_exit(vm_t *vm)
{
	vm->is_running = false;
	return 0;
}

#ifdef GSC_STANDALONE
extern unsigned int fps_delta;
int sf_set_fps(vm_t *vm)
{
	fps_delta = se_getint(vm, 0);
	return 0;
}
int sf_thread_run(vm_t *vm)
{
	vm_run_active_threads(vm, fps_delta);
	return 0;
}
#endif
#if 0
int sf_eval(vm_t *vm)
{
	const char *code = se_getstring(vm, 0);
	//char *str = vm_mem_alloc(vm,strlen(code) + 100);
	//snprintf(str, strlen(code) + 100, "main()\n{\n%s\n}", code);
	char *str = code;

	char *script;
	int script_size;
	if (parser_compile_string(str, &script, &script_size, NULL)) {
		vm_printf("Failed compiling '%s'.\n", str);
		//vm_mem_free(vm, str);
		return 0;
	}
	//vm_mem_free(vm, str);
	vm_t *nvm = vm_create(script, script_size);
	free(script);

	varval_t *arr = se_createarray(nvm);
	for (int i = 1; i < se_argc(vm); ++i)
	{
		const char *s = se_getstring(vm, i);
		se_addstring(nvm, s);
		varval_t *av = (varval_t*)stack_pop(nvm);
		se_vv_set_field(nvm, arr, i - 1, av);
	}
	stack_push_vv(nvm, arr);

	vm_exec_thread(nvm, "main", 1);
	varval_t *ret_val = stack_pop_vv(nvm);
	varval_t *copy = se_vv_copy(vm, ret_val);

#if 1
	while (vm_get_num_active_threadrunners(nvm) > 0) {
		vm_run_active_threads(nvm, 1000 / 20);
		sys_sleep(1000 / 20);
	}
#endif
	vm_free(nvm);

	stack_push_vv(vm, copy);
	return 1;
}
#endif

int sm_test(vm_t *vm, varval_t *self)
{
	printf("sm_test\n");
	se_addint(vm, 123);
	return 1;
}

int sf_is_file(vm_t *vm)
{
	const char *path = se_getstring(vm, 0);
	struct stat s;
	if (stat(path, &s) != 0)
	{
		se_addbool(vm, false);
		return 1;
	}
	se_addbool(vm, (s.st_mode & S_IFREG) == S_IFREG);
	return 1;
}

int sf_is_dir(vm_t *vm)
{
	const char *path = se_getstring(vm, 0);
	struct stat s;
	if (stat(path, &s) != 0)
	{
		se_addbool(vm, false);
		return 1;
	}
	se_addbool(vm, (s.st_mode & S_IFDIR) == S_IFDIR);
	return 1;
}

typedef struct
{
	const char *name;
	int id;
} signal_map_value_t;

bool vm_register_c_ffi_callback(vm_t *vm, char *cfunc, int numargs);
char *ffi_create_c_callback(vm_t *vm, vm_function_t fp, size_t numargs);

static int sf_set_signal_handler(vm_t *vm)
{
	static const signal_map_value_t map[] = {
		{"int", SIGINT},
		{"ill", SIGILL},
		{"fpe", SIGFPE},
		{"segv", SIGSEGV},
		{"term", SIGTERM},
		{"break", SIGBREAK},
		{"abrt", SIGABRT},
		{0,0}
	};
	const char *name = se_getstring(vm, 0);
	vm_function_t fp = se_getfunc(vm, 1);
	for (signal_map_value_t *p = map; p->name; p++)
	{
		if (!strcmp(p->name, name))
		{
			printf("signal registered %d %s, %d\n", p->id, name, fp);
			char *cfunc = ffi_create_c_callback(vm, fp, 1);
			//printf("created cfunc %02X for %d\n", cfunc, arg->as.integer);
			vm_register_c_ffi_callback(vm, cfunc, 1); //1 signal
			signal(p->id, cfunc);
			se_addbool(vm, true);
			return 1;
		}
	}
	se_addbool(vm, false);
	return 1;
}

stockmethod_t std_scriptmethods[] = {
	{"testabc", sm_test},
	{NULL,0}
};

stockfunction_t std_scriptfunctions[] = {
#ifdef _WIN32
	{"set_pixel", sf_setpixel},
#endif
#ifdef GSC_STANDALONE
	{"__internal_thread_set_sleep_time", sf_set_fps},
	//{"__internal_thread_run", sf_thread_run}, //don't use this
#endif
	//{ "abs",sf_abs },
	//{ "sinf",sf_m_sinf },
	//{"cosf",sf_m_cosf},
	//{"eval", sf_eval},
	//{"exit", sf_exit},
	{"is_dir", sf_is_dir},
	{"is_file", sf_is_dir},
	{ "buffer", sf_buffer },
	{ "set_signal_handler", sf_set_signal_handler },
	{ "int",sf_int },
	{ "float",sf_float },
	{"string",sf_string},
	{ "read_text_file", sf_read_text_file },
	{ "write_text_file", sf_write_text_file },
	//{ "rename", sf_rename },
	//{ "remove", sf_remove },
	{ "listdir", sf_listdir },

	{ "sendpacket", sf_sendpacket },

	{ "print", sf_print },
	{ "println", sf_println },
	{ "fopen", sf_fopen },
	{ "fwritevalue", sf_fwritevalue },
	{ "strpos", sf_strpos },
	//{ "tolower", sf_tolower },
	//{ "toupper", sf_toupper },
	{ "strtok", sf_strtok },
	{ "implode", sf_implode },
	{ "explode", sf_explode },
	//{"isdigit", sf_isdigit},
	//{"isalnum", sf_isalnum},
	//{"islower", sf_islower},
	//{"isupper", sf_isupper},
	//{"isalpha", sf_isalpha},
	{ "substr", sf_substr },
	{"get_object_vars",sf_get_object_vars},
	{"getarraykeys",sf_get_object_vars}, //alternate naming
	{ "get_in_string", sf_get_in_string },
	{ "printf", sf_printf },
	{ "sprintf", sf_sprintf },
	{ "getchar", sf_getchar },
	{"isdefined", sf_isdefined},
	{ "get_time", sf_get_time },
	{ "randomint", sf_randomint },
	{"randomfloat", sf_randomfloat},
	{"spawnstruct", sf_spawnstruct},
	{ "typeof",sf_typeof },
	{"cstring",sf_cstring},
	{"sizeof",sf_sizeof},
	{NULL,NULL},
};
