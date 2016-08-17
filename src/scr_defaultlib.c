#include "virtual_machine.h"
#include "common.h"

int sf_print(vm_t *vm) {
	for (int i = 0; i < se_argc(vm); i++)
		printf("%s", se_getstring(vm, i));
	return 0;
}

int sf_println(vm_t *vm) {
	for (int i = 0; i < se_argc(vm); i++)
		printf("%s\n", se_getstring(vm, i));
	return 0;
}

//not sure if this works properly ;D
int sf_printf(vm_t *vm) {
	const char *base_str = se_getstring(vm, 0);
	static char string[1024] = { 0 };
	int stri = 0;

	int parmnum = 1;

	for (int i = 0; i < strlen(base_str); i++) {
		if (stri >= sizeof(string))
			break;

		if (i > 0 && base_str[i] == '%' && base_str[i-1]!='\\') {
			const char *part = se_getstring(vm, parmnum++);
			if (stri + strlen(part) >= sizeof(string))
				break; //too long m8
			
			//strncat(&string[stri], part, sizeof(string) - strlen(string) - 1);

			strncpy(&string[i], part, strlen(part) + 1);
			stri += strlen(part);
			continue;
		}

		string[stri++] = base_str[i];
	}
	string[sizeof(string) - 1] = '\0';
	printf("%s", string);
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

static void obj_file_deconstructor(FILE *fp) {
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
	vv->obj->obj = (void*)fp;
	stack_push_vv(vm, vv);
	return 1;
}

int sf_fwritevalue(vm_t *vm) {
	varval_t *vv = se_argv(vm, 0);
	if (VV_TYPE(vv) != VAR_TYPE_OBJECT)
		return 0;
	varval_t *val = se_argv(vm, 1);
	int sz = se_getint(vm, 2);

	FILE *fp = (FILE*)vv->obj->obj;
	
	size_t written = 0;

	switch (VV_TYPE(val)) {
	case VAR_TYPE_INT:
		if(!sz || sz > 4)
		written = fwrite(&val->integer, sizeof(val->integer), 1, fp);
		else
		{
			written = fwrite(&val->integer, sz, 1, fp);
		}
		break;
	case VAR_TYPE_FLOAT:
		written = fwrite(&val->number, sizeof(val->number), 1, fp);
		break;
	case VAR_TYPE_STRING:
	case VAR_TYPE_INDEXED_STRING: {
		const char *as_str = se_vv_to_string(vm, val);
		written = fwrite(as_str, sz==0?strlen(as_str):strlen(as_str)+1, 1, fp);
	} break;
	default:
		printf("'%s' unsupported value type! no fwrite available\n", VV_TYPE_STRING(val));
		break;
	}
	se_addint(vm, written);
	return 1;
}

int sf_read_text_file(vm_t *vm) {
	const char *filename = se_getstring(vm, 0);
	int filesize;
	char *buf;
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
	const char *str = se_getstring(vm, 0);
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
	printf("this function has no support for linux at the moment.\n");
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
	}
	free(files);
	stack_push_vv(vm, arr);
	return 1;
}

stockfunction_t std_scriptfunctions[] = {
#ifdef _WIN32
	{"set_pixel", sf_setpixel},
#endif
	{ "abs",sf_abs },
	{ "sinf",sf_m_sinf },
	{"cosf",sf_m_cosf},

	{ "int",sf_int },
	{ "float",sf_float },
	{"string",sf_string},
	{ "read_text_file", sf_read_text_file },
	{ "write_text_file", sf_write_text_file },
	{ "rename", sf_rename },
	{ "remove", sf_remove },
	{ "listdir", sf_listdir },

	{ "print", sf_print },
	{ "println", sf_println },
	{ "fopen", sf_fopen },
	{ "fwritevalue", sf_fwritevalue },
	{ "strpos", sf_strpos },
	{ "tolower", sf_tolower },
	{ "toupper", sf_toupper },
	{ "strtok", sf_strtok },
	{"isdigit", sf_isdigit},
	{"isalnum", sf_isalnum},
	{"islower", sf_islower},
	{"isupper", sf_isupper},
	{"isalpha", sf_isalpha},
	{ "substr", sf_substr },
	{ "get_in_string", sf_get_in_string },
	{ "printf", sf_printf },
	{ "getchar", sf_getchar },
	{"isdefined", sf_isdefined},
	{ "get_time", sf_get_time },
	{ "randomint", sf_randomint },
	{"randomfloat", sf_randomfloat},
	{"spawnstruct", sf_spawnstruct},
	{"typeof",sf_typeof},
	{NULL,NULL},
};
