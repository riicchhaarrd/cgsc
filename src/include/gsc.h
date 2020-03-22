#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "../cvector.h"
#include "../dynarray.h"

//#define MEMORY_DEBUG

//should be here probably, so the program incorporating this can react more accordingly
typedef enum {
	E_VM_RET_NONE,
	E_VM_RET_WAIT,
	E_VM_RET_FUNCTION_NOT_FOUND,
	E_VM_RET_ERROR,
} e_vm_return_codes;

typedef enum {
	E_VM_ERR_NONE,
	E_VM_ERR_ERROR,
	E_VM_ERR_DIVIDE_BY_ZERO,
} e_vm_error_codes;

typedef struct
{
	uint32_t code_offset;
} bprogramhdr_t;

typedef unsigned int vm_flags_t;
typedef struct vm_s vm_t;
typedef struct varval_s varval_t;
typedef struct vm_thread_s vm_thread_t;

typedef struct
{
	unsigned char *buffer;
	size_t size;
	char tag[128];
} source_t;

typedef struct
{
	int(*read_file)(const char *filename, char **buf, int *filesize);
} vm_compiler_opts_t;

typedef struct
{
	dynarray sources;
	vm_compiler_opts_t opts;
	char *program;
	size_t program_size;
} compiler_t;

#if 0
int parser_compile(const char *filename, char **out_program, int *program_size);
int parser_compile_string(const char *str, char **out_program, int *program_size, vm_compiler_opts_t* opts);
#endif
int compiler_init(compiler_t *c, vm_compiler_opts_t *opts);
void compiler_cleanup(compiler_t *c);
int compiler_add_source(compiler_t *c, const char *scriptbuf, const char *src_tag);
int compiler_add_source_file(compiler_t *c, const char *filename);
int compiler_execute(compiler_t *c);

int vm_get_num_active_threadrunners(vm_t *vm);

int vm_run_active_threads(vm_t *vm, int);
int vm_exec_thread_pointer(vm_t *vm, int fp, int numargs);
int vm_exec_ent_thread_pointer(vm_t *vm, varval_t *new_self, int fp, int numargs);
int vm_exec_thread(vm_t *vm, const char *func_name, int numargs);
int vm_notify(vm_t *vm, varval_t *object, int stringindex, size_t numargs);
int vm_notify_string(vm_t *vm, varval_t *vv, const char *str, size_t numargs);
int vm_exec_ent_thread(vm_t *vm, varval_t *new_self, const char *func_name, int numargs);
void *vm_get_user_pointer(vm_t*);
void vm_set_user_pointer(vm_t*, void*);
varval_t *vm_get_level_object(vm_t *vm);
void vm_set_printf_hook(vm_t *vm, void *hook);

void vm_free(vm_t *vm);
int vm_add_program(vm_t *vm, unsigned char *buffer, size_t sz, const char *tag);
vm_t *vm_create();
void vm_error(vm_t*, int, const char *, ...);
int se_error(vm_t*, const char *, ...);

typedef struct {
	const char *name;
	int(*call)(vm_t*);
} stockfunction_t;

typedef struct {
	const char *name;
	int(*call)(vm_t*,void*);
} stockmethod_t;

varval_t *se_argv(vm_t*, int);
int se_argc(vm_t*);
#define se_getnumparams(vm) se_argc(vm)

int se_getint(vm_t*, int);
typedef intptr_t vm_function_t;
vm_function_t se_getfunc(vm_t *vm, int i);
float se_getfloat(vm_t*, int);
const char *se_getstring(vm_t*, int);
void se_getvector(vm_t*, int, float*);
int se_getistring(vm_t*, int);

void se_set_object_field(vm_t *vm, const char *key);

typedef enum {
	VAR_TYPE_CHAR,
	VAR_TYPE_SHORT,
	VAR_TYPE_INT,
	VAR_TYPE_LONG,
	VAR_TYPE_FLOAT,
	VAR_TYPE_DOUBLE,

	VAR_TYPE_STRING,
	VAR_TYPE_INDEXED_STRING,
	VAR_TYPE_VECTOR,
	VAR_TYPE_ARRAY,
	VAR_TYPE_OBJECT,
	VAR_TYPE_OBJECT_REFERENCE,
	VAR_TYPE_FUNCTION_POINTER,
	VAR_TYPE_NULL //the vv is NULL itself
} e_var_types_t;

#define BIT(x) ((1<<x))

typedef enum {
	VAR_FLAG_NONE = 0,
	//VAR_FLAG_LEVEL_PLAYER = BIT(0),
} e_var_flags_t;

typedef struct {
	int index;
	char *string;
	unsigned long hash;
} vt_istring_t;

typedef struct {
	varval_t *value;
	union {
		int stringindex, index;
	};
} vt_object_field_t;

typedef enum {
	VT_OBJECT_GENERIC,
	VT_OBJECT_FILE,
	VT_OBJECT_BUFFER,
	VT_OBJECT_STRUCT,
	VT_OBJECT_LEVEL,
	VT_OBJECT_SELF,
	VT_OBJECT_CUSTOM
} e_vt_object_types;
//rest is up to the application i think

typedef struct {
	int type;
	bool managed;
	size_t size;
	char* data;
} vt_buffer_t;

//#define DYN_TYPE_HDR(type, s) (type *)( ( s ) - sizeof(type) )

typedef int(*vt_obj_getter_prototype)(vm_t*, void*);
typedef void(*vt_obj_setter_prototype)(vm_t*, void*);

typedef struct {
	const char *name;
	void *getter;
	void *setter;
} vt_obj_field_custom_t;

typedef struct {
	//int key;
	//int numfields;
	int internal_size; //maybe later
	//vt_object_field_t *fields;
	vector fields;
	vt_obj_field_custom_t *custom;
	void *constructor;
	void(*deconstructor)(vm_t*, void*);
	void *obj; //ptr to the class/struct object
	int type;
	/* works for just simple notifications, but for stack / passing things moved to its own type */
	//int notifiers[MAX_NOTIFIER_STRINGS];
	//uint32_t notified; //not needed atm
} vt_object_t;

typedef double vm_scalar_t;
typedef int64_t vm_iscalar_t;

typedef union {
	double dbl;
	float flt;
	float number;

	char character;
	int integer;
	short shortint;
	long long longint;
	intptr_t intptr;

	//intptr_t ptr;
	char *string;
	void *ptr;
	vt_object_t *obj;
	int index; //more generic
	int stringindex;
	vm_scalar_t vec[3]; //largest imo
	intptr_t ivec[3];
} varval_internal_representation_t;

struct varval_s {
	e_var_types_t type;
	unsigned int flags, refs;
#ifdef MEMORY_DEBUG
	char debugstring[256];
#endif
	varval_internal_representation_t as;
};

#define VF_LOCAL (1<<0)
//#define VF_CACHED (1<<1)
#define VF_POINTER (1<<2)
#define VF_UNSIGNED (1<<3)
#define VF_FFI (1<<4)
#define VF_NOFREE (1<<5) //DO_NOT_FREE
#define VF_INUSE (1<<6)
#define VV_HAS_FLAG(x,y) ( (x->flags & y) == y )

const char *se_vv_to_string(vm_t *vm, varval_t *vv);
const char *se_index_to_string(vm_t *vm, int i);

void se_vv_object_free(vm_t *vm, varval_t *vv);
int se_vv_type(vm_t *vm, varval_t *vv);

#ifdef MEMORY_DEBUG
varval_t *se_vv_create_r(vm_t*, e_var_types_t type,const char *,int);
#define se_vv_create(a,b) (se_vv_create_r(a,b,__FILE__,__LINE__))
#else
varval_t *se_vv_create(vm_t*, e_var_types_t);
#endif
varval_t *se_vv_copy(vm_t*, varval_t *vv);
void se_vv_remove_reference(vm_t *vm, varval_t *vv);
void se_vv_set_field(vm_t *vm, varval_t *vv, int key, varval_t *value);
bool se_vv_is_freeable(vm_t *vm, varval_t *vv);
#define se_vv_free(a,b) (se_vv_free_r(a,b,__FILE__,__LINE__))
int se_vv_free_r(vm_t*, varval_t *vv,const char *,int);
void se_vv_free_force(vm_t *vm, varval_t *vv);
varval_t *se_vv_get_field(vm_t *, varval_t *vv, int key);

int se_vv_enumerate_fields(vm_t *vm, varval_t *vv, int(*callback_fn)(vm_t*, const char *field_name, vt_object_field_t*, void *ud, vm_flags_t fl), void *userdata);
int se_vv_container_size(vm_t *vm, varval_t *vv);
varval_t *se_createarray(vm_t*);
varval_t *se_createobject(vm_t *vm, int type, vt_obj_field_custom_t*, void *constructor, void *deconstructor);
void se_addobject(vm_t *vm, varval_t*);
void se_addnull(vm_t*);
void se_addistring(vm_t*, int);
void se_addvector(vm_t*, const float*);
void se_addvectorf(vm_t *vm, float x, float y, float z);
varval_t *se_createstring(vm_t *vm, const char *s);
void vv_string_set(vm_t *vm, varval_t *vv, const char *s);
void se_addstring(vm_t*, const char*);
void se_addint(vm_t*, int);
#define se_addbool(a,b) se_addint(a,((b==true) ? 1 : 0))
void se_addfloat(vm_t*, float);
void se_register_stockfunction_set(vm_t*, stockfunction_t*);
void se_register_stockmethod_set(vm_t *vm, int object_type, stockmethod_t *set);

int se_istring_from_string(vm_t *vm, const char *str);

#ifdef __cplusplus
}
#endif