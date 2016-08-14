#if 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#include "virtual_machine.h"

#include "variable.h"

#define ENUM_BEGIN(typ) typedef enum {
#define ENUM(nam) nam
#define ENUM_END(typ) } typ;

#include "vm_opcodes.h"

#undef ENUM_BEGIN
#undef ENUM
#undef ENUM_END
#define ENUM_BEGIN(typ) static const char * typ ## _strings[] = {
#define ENUM(nam) #nam
#define ENUM_END(typ) };
#include "vm_opcodes.h"

static void print_registers(vm_t *vm) {
	int i;
	for (i = 0; i < e_vm_reg_len; i++)
		printf("%s => %d\n", e_vm_reg_names[i], vm_registers[i]);
}

static int get_opcode(vm_t *vm) {
	int op = vm->program[vm_registers[REG_IP]];
	++vm_registers[REG_IP];
	return op;
}

static int read_int(vm_t *vm) {
	int i = *(int*)(vm->program + vm_registers[REG_IP]);
	vm_registers[REG_IP] += sizeof(int);
	return i;
}

static float read_float(vm_t *vm) {
	float f = *(float*)(vm->program + vm_registers[REG_IP]);
	vm_registers[REG_IP] += sizeof(float);
	return f;
}

#if 0
static int read_short(vm_t *vm) {
#if 0
	short s = *(short*)(vm->program + vm_registers[REG_IP]);
	vm_registers[REG_IP] += sizeof(short);
	return s;
#else
	return read_int(vm); //cuz might be faster
#endif
}
#else
#define read_short read_int
#endif

int se_argc(vm_t *vm) {
	return vm->thrunner->numargs;
}

varval_t *se_argv(vm_t *vm, int i) {
	if (i >= se_argc(vm))
		return NULL;
	return (varval_t*)vm_stack[vm_registers[REG_BP] + i];
}

static float se_vv_to_float(varval_t *vv) {
	float ret = 0;
	if (VV_TYPE(vv)== VAR_TYPE_INT)
		ret = (float)vv->integer;
	else if (VV_TYPE(vv)== VAR_TYPE_FLOAT)
		ret = vv->number;
	return ret;
}

static int se_vv_to_int(vm_t *vm, varval_t *vv) {
	int ret = 0;
	switch (VV_TYPE(vv)) {
	case VAR_TYPE_INT:
	case VAR_TYPE_FUNCTION_POINTER:
		return vv->integer;
	case VAR_TYPE_FLOAT:
		return (int)vv->number;
	case VAR_TYPE_STRING:
	case VAR_TYPE_INDEXED_STRING: {
		const char *str = se_vv_to_string(vm, vv);
		return atoi(str);
	} break;
	}
	return ret;
}

const char *se_vv_to_string(vm_t *vm, varval_t *vv) {
	static char string[128];

	switch (VV_TYPE(vv)) {
	case VAR_TYPE_INDEXED_STRING:
		return se_index_to_string(vm, vv->stringindex);
	case VAR_TYPE_STRING:
		return vv->string;
	case VAR_TYPE_VECTOR:
		snprintf(string, sizeof(string), "(%f, %f, %f)", vv->vec[0], vv->vec[1], vv->vec[2]);
		return string;
	case VAR_TYPE_INT:
#ifdef _WIN32
		return itoa(vv->integer, string, 10);
#else
		snprintf(string, sizeof(string), "%d", vv->integer);
		return string;
#endif	
	case VAR_TYPE_FLOAT:
		snprintf(string, sizeof(string), "%f", vv->number);
		return string;
	case VAR_TYPE_NULL:
		return "[null]";
	case VAR_TYPE_OBJECT:
		return "[object]";
	case VAR_TYPE_ARRAY:
		return "[array]";
	}
	return "[unhandled variable type]";
}

float se_getfloat(vm_t *vm, int i) {
	varval_t *vv = se_argv(vm, i);
	float ret = se_vv_to_float(vv);
	return ret;
}

const char *se_getstring(vm_t *vm, int i) {
	varval_t *vv = se_argv(vm, i);
	return se_vv_to_string(vm,vv);
}

int se_getint(vm_t *vm, int i) {
	varval_t *vv = se_argv(vm, i);
	int ret = se_vv_to_int(vm, vv);
	return ret;
}

int se_getfunc(vm_t *vm, int i) {
	varval_t *vv = se_argv(vm, i);
	int ret = se_vv_to_int(vm, vv);
	return ret;
}

int se_getistring(vm_t *vm, int i) {
	varval_t *vv = se_argv(vm, i);
	if (VV_TYPE(vv) != VAR_TYPE_INDEXED_STRING) {
		printf("'%s' is not an indexed string!\n", e_var_types_strings[VV_TYPE(vv)]);
		return 0;
	}
	return vv->stringindex;
}

void se_addint(vm_t *vm, int i) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_INT);
	vv->integer = i;
	stack_push_vv(vm, vv);
}

void se_addobject(vm_t *vm, varval_t *vv) {
	stack_push_vv(vm, vv);
}

varval_t *se_createobject(vm_t *vm, int type, vt_obj_field_custom_t* custom, void* constructor, void *deconstructor) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_OBJECT);
	vt_object_t *obj = vv->obj;
	obj->custom = custom;
	obj->constructor = constructor;
	obj->deconstructor = (void(*)(void*))deconstructor;
	obj->type = type;
	obj->obj = NULL;
	return vv;
}

void se_set_object_field(vm_t *vm, const char *key) {
	varval_t *val = (varval_t*)stack_pop(vm);
	varval_t *obj = (varval_t*)stack_pop(vm);

	vt_istring_t *istr = se_istring_find(vm, key);
	if (istr == NULL)
		istr = se_istring_create(vm, key);
	se_vv_set_field(vm, obj, istr->index, val);
	se_vv_free(vm, val); //free the value as the set_field creates a copy of the value
	stack_push_vv(vm, obj); //put it back :D
}

varval_t *se_createarray(vm_t *vm) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_ARRAY);
	return vv;
}

void se_getvector(vm_t *vm, int i, float *vec) {
	vec[0] = vec[1] = vec[2] = 0.f;

	varval_t *vv = se_argv(vm, i);
	if (VV_TYPE(vv) != VAR_TYPE_VECTOR) {
		printf("'%s' is not an vector!\n", e_var_types_strings[VV_TYPE(vv)]);
		return;
	}
	vec[0] = vv->vec[0];
	vec[1] = vv->vec[1];
	vec[2] = vv->vec[2];
}

void se_addvector(vm_t *vm, const float *vec) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_VECTOR);
	vv->vec[0] = vec[0];
	vv->vec[1] = vec[1];
	vv->vec[2] = vec[2];
	stack_push_vv(vm, vv);
}

void se_addvectorf(vm_t *vm, float x, float y, float z) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_VECTOR);
	vv->vec[0] = x;
	vv->vec[1] = y;
	vv->vec[2] = z;
	stack_push_vv(vm, vv);
}

void se_addnull(vm_t *vm) {
	stack_push(vm, NULL);
}

void se_addistring(vm_t *vm, int idx) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_INDEXED_STRING);
	vv->stringindex = idx;
	stack_push_vv(vm, vv);
}

void se_addstring(vm_t *vm, const char *s) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_STRING);
	size_t len = strlen(s);
	char *str = (char*)vm_mem_alloc(vm, len + 1);
	strncpy(str, s, len);
	str[len] = '\0';
	vv->string = str;
	stack_push_vv(vm, vv);
}

void se_addfloat(vm_t *vm, float f) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_FLOAT);
	vv->number = f;
	stack_push_vv(vm, vv);
}

void se_register_stockfunction_set(vm_t *vm, stockfunction_t *set) {
	vm->stockfunctionsets[vm->numstockfunctionsets++] = set;
}

static stockfunction_t *se_find_function_by_name(vm_t *vm, const char *s) {
	stockfunction_t *sf = NULL;
	if (!s) return sf;

	for (int fff = 0; fff < vm->numstockfunctionsets; fff++) {
		sf = vm->stockfunctionsets[fff];

		for (int i = 0; sf[i].name; i++) {
			if (!strcmp(sf[i].name, s)) {
				return &sf[i];
			}
		}
	}
	return NULL;
}

static int stack_pop_int(vm_t *vm) {
	varval_t *vv = (varval_t*)stack_pop(vm);
	int i = se_vv_to_int(vm, vv);
	se_vv_free(vm, vv);
	return i;
}

static float stack_pop_float(vm_t *vm) {
	varval_t *vv = (varval_t*)stack_pop(vm);
	float f = se_vv_to_float(vv);
	se_vv_free(vm, vv);
	return f;
}

#if 0
#include <windows.h>
typedef struct {
	int op;
	unsigned long long duration;
} opcode_performance_t;

static int vm_execute_r(vm_t *vm, int instr);
static int vm_execute(vm_t *vm, int instr) {

	static opcode_performance_t pf[5] = { 0 };

	unsigned long long start = GetTickCount64();
	int r = vm_execute_r(vm, instr);
	unsigned long long end = GetTickCount64();
	unsigned long long delta = end - start;

	for (int i = 0; i < sizeof(pf) / sizeof(pf[0]); i++) {
		if (delta > pf[i].duration) {
			pf[i].op = instr;
			pf[i].duration = delta;
			break;
		}
	}
#if 0
	if (GetAsyncKeyState(VK_LCONTROL)) {

		for (int i = 0; i < sizeof(pf) / sizeof(pf[0]); i++) {
			printf("%d: %s (%d ms)\n", i, e_opcodes_strings[pf[i].op], pf[i].duration);
		}
		Sleep(1000);
	}
#endif
	return r;
}
#endif

static int vm_execute(vm_t *vm, int instr) {
	if (instr < OP_END_OF_LIST) {
		//printf("OP: %s (Location %d)\n", e_opcodes_strings[instr], vm->registers[REG_IP]);
	}

	switch (instr) {
		case OP_HALT: {
			vm->is_running = false;
			printf("HALT\n");
		} break;

		case OP_SHOW_LOCAL_VARS: {
			int am = read_int(vm);
			if (am > MAX_LOCAL_VARS)
				am = MAX_LOCAL_VARS;
			for (int i = 0; i < am; i++) {
				printf("var %d => %d\n", i, vm_stack[vm_registers[REG_BP] + i]);
			}
			getchar();
		} break;

		case OP_BRK: {
			printf("Break at %d\n", vm_registers[REG_IP - 1]);
			print_registers(vm);
			getchar();
		} break;

		case OP_PUSH: {
			int pushee = read_int(vm);
			se_addint(vm, pushee);
		} break;

		case OP_PUSH_FUNCTION_POINTER: {
			int i = read_int(vm);
			varval_t *vv = se_vv_create(vm, VAR_TYPE_FUNCTION_POINTER);
			vv->integer = i;
			stack_push_vv(vm, vv);
		} break;

		case OP_PUSHF: {
			float f = read_float(vm);
			se_addfloat(vm, f);
		} break;

		case OP_PUSH_NULL: {
			se_addnull(vm);
		} break;

		case OP_PUSH_OBJECT: {
			se_addobject(vm, se_createobject(vm, VT_OBJECT_GENERIC,NULL,NULL,NULL));
		} break;

		case OP_PUSH_ARRAY: {
			int num_elements = read_short(vm);
			varval_t *arr = se_createarray(vm);
			//printf("num_elements=%d\n", num_elements);
			for (int i = 0; i < num_elements; i++) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				//printf("ADDED FIELD\n");
				se_vv_set_field(vm, arr, i, vv);
				se_vv_free(vm, vv);
			}
			stack_push_vv(vm, arr);
		} break;

		case OP_PUSH_VECTOR: {
			varval_t *vv = se_vv_create(vm, VAR_TYPE_VECTOR);
			for (int i = 0; i < 3; i++) {
				varval_t *fl = (varval_t*)stack_pop(vm);
				vv->vec[2-i] = se_vv_to_float(fl);
				se_vv_free(vm, fl);
			}
			stack_push_vv(vm, vv);
		} break;

		case OP_STORE_ARRAY_INDEX: {
			varval_t *vv = (varval_t*)stack_pop(vm);
			varval_t *vv_index = (varval_t*)stack_pop(vm);
			varval_t *vv_arr = (varval_t*)stack_pop(vm);

			if (VV_TYPE(vv_arr) == VAR_TYPE_ARRAY) {
				se_vv_set_field(vm, vv_arr, se_vv_to_int(vm, vv_index), vv);
				se_vv_free(vm, vv);
			}
			else {
				printf("'%s' is not an array!\n", e_var_types_strings[VV_TYPE(vv_arr)]);
			}
			se_vv_free(vm, vv_index);
			//printf("str = %s\n", vm->istringlist[str_index].string);
		} break;

		case OP_GET_LEVEL: {
			se_addobject(vm, vm->level);
		} break;

		case OP_GET_SELF: {
			stack_push_vv(vm, vm->self);
		} break;

		case OP_GET_LENGTH: {
			varval_t *vv_arr = (varval_t*)stack_pop(vm);
			{
				if (VV_TYPE(vv_arr) == VAR_TYPE_ARRAY)
					se_addint(vm, vector_count(&vv_arr->obj->fields));//se_addint(vm, vv_arr->obj->numfields);
				else if(VV_TYPE(vv_arr)==VAR_TYPE_STRING || VV_TYPE(vv_arr)==VAR_TYPE_INDEXED_STRING)
					se_addint(vm, strlen(se_vv_to_string(vm,vv_arr)));
				else {
					printf("'%s' is not an array, cannot get length!\n", e_var_types_strings[VV_TYPE(vv_arr)]);
					se_addnull(vm);
				}
			}
		} break;

		case OP_LOAD_ARRAY_INDEX: {

			varval_t *arr_index = (varval_t*)stack_pop(vm);
			varval_t *arr = (varval_t*)stack_pop(vm);
			int idx = 0;
			if (VV_TYPE(arr)== VAR_TYPE_VECTOR) {
				idx = se_vv_to_int(vm, arr_index);
				if (idx < 0 || idx > 2) {
					printf("vector index out of bounds!\n");
					idx = 0;
				}
				se_addfloat(vm, arr->vec[idx]);
			}
			else if (VV_TYPE(arr)== VAR_TYPE_ARRAY) {
				idx = se_vv_to_int(vm, arr_index);
				varval_t *vv = se_vv_get_field(vm, arr, idx);
				if (NULL == vv)
					se_addnull(vm);
				else {
					varval_t *copy = vv;
					if (!VV_USE_REF(vv))
						copy = se_vv_copy(vm, vv);
					stack_push_vv(vm, copy);
				}
			} else if(VV_TYPE(arr)==VAR_TYPE_STRING|| VV_TYPE(arr)==VAR_TYPE_INDEXED_STRING) {
				idx = se_vv_to_int(vm, arr_index);
				const char *str = se_vv_to_string(vm, arr);
				if (idx < 0 || idx > strlen(str))
					se_addnull(vm);
				else {
					char ts[2];
					ts[0] = str[idx];
					ts[1] = 0;
					se_addstring(vm, ts);
				}
			} else {
				printf("%s is not an array!\n", e_var_types_strings[VV_TYPE(arr)]);
				se_addnull(vm);
			}
			se_vv_free(vm, arr_index);
		} break;

		case OP_PUSH_STRING: {
			int str_index = read_int(vm);
			if (str_index >= vm->istringlistsize)
				str_index = 0;
			se_addistring(vm, str_index);
		} break;

		case OP_POP: {
			varval_t *vv = (varval_t*)stack_pop(vm);
			se_vv_free(vm, vv);
		} break;

		case OP_NOP:
			printf("no-operation\n");
		break;

		case OP_STORE_FIELD: {
			int str_index = read_short(vm);
			varval_t *vv = (varval_t*)stack_pop(vm);
			varval_t *vv_obj = (varval_t*)stack_pop(vm);

			if (VV_TYPE(vv_obj)== VAR_TYPE_OBJECT) {
				if (str_index >= vm->istringlistsize)
					printf("str_index out of bounds!!\n");
				else {
					se_vv_set_field(vm, vv_obj, str_index, vv);
					se_vv_free(vm, vv);
				}
			}
			else if (VV_TYPE(vv_obj)== VAR_TYPE_ARRAY) {
				se_vv_set_field(vm, vv_obj, str_index, vv);
				se_vv_free(vm, vv);
			}
#if 0
			if (!strcmp(vm->istringlist[str_index].string, "players")) {//storing [] players
				vv->flags |= VAR_FLAG_LEVEL_PLAYER;
				printf("refs = %d, str = %s\n", vv->refs, vm->istringlist[str_index].string);
			}
#endif
		} break;

		case OP_LOAD_FIELD: {

			int str_index = read_short(vm);
			varval_t *vv_obj = (varval_t*)stack_pop(vm);

			if (str_index >= vm->istringlistsize)
				printf("str_index out of bounds!!\n");
			else {
				const char *str = vm->istringlist[str_index].string;
				//printf("LOAD_FIELD{%s}\n", vm->istringlist[str_index].string);
				if (VV_TYPE(vv_obj)== VAR_TYPE_OBJECT) {
					varval_t *vv = se_vv_get_field(vm, vv_obj, str_index);
					if (NULL == vv)
						se_addnull(vm);
					else {
						varval_t *copy = vv;
						if (!VV_USE_REF(vv))
							copy = se_vv_copy(vm, vv);
						stack_push_vv(vm, copy);
					}
				} else if (VV_TYPE(vv_obj)== VAR_TYPE_VECTOR) {
					if (*str == 'x' || *str == 'y' || *str == 'z') {
						se_addfloat(vm, vv_obj->vec[*str-'x']);
					}
					else {
						printf("cannot get {%s} of vector!\n", str);
						se_addnull(vm);
					}
				}
				else
					se_addnull(vm);
			}
		} break;

		case OP_STORE: {
			int loc = read_short(vm);
			//printf("LOCAL STORE INDEX = %d\n", loc);
			varval_t *vv = (varval_t*)stack_pop(vm);
			varval_t *cur = (varval_t*)vm_stack[vm_registers[REG_BP] + loc];
			if (VV_USE_REF(vv))
				++vv->refs;
			//printf("vvrefs=%d,currefs=%d\n", vv->refs, cur ? cur->refs : 0);
			se_vv_remove_reference(vm, cur);
			vm_stack[vm_registers[REG_BP] + loc] = (intptr_t)vv;
		} break;

		case OP_LOAD: {
			int loc = read_short(vm);
			//printf("LOCAL LOAD INDEX = %d\n", loc);
			varval_t *vv = (varval_t*)vm_stack[vm_registers[REG_BP] + loc];
			if (vv == NULL) {
				//printf("vv == NULL!\n");
				se_addnull(vm);
			} else {
				varval_t *copy = vv;

				if (!VV_USE_REF(vv))
					copy = se_vv_copy(vm, vv);

				//stack_push(vm, vm->stack[vm->registers[REG_BP] + loc]);
				stack_push_vv(vm, copy);
			}
		} break;

			//all these extra -1 is because at end the REG_IP will be increased anyway with + 1
			//so to get the right addresses

#define printf_hide

#define SPECIFY_OP_SIMILAR(SPECIFIED_OPERATOR, MATH_OP) \
	case SPECIFIED_OPERATOR: { \
		int val = read_int(vm); \
		int b = stack_pop_int(vm); \
		int a = stack_pop_int(vm); \
		printf_hide("IF(%d %s %d)\n", a, #MATH_OP, b); \
		if (a MATH_OP b) {\
			vm_registers[REG_IP] = val; \
			printf("jumping to %d\n",val); \
		} \
	} break;

		SPECIFY_OP_SIMILAR(OP_JE, ==)
		SPECIFY_OP_SIMILAR(OP_JGE, >=)
		SPECIFY_OP_SIMILAR(OP_JG, >)
		SPECIFY_OP_SIMILAR(OP_JL, <)
		SPECIFY_OP_SIMILAR(OP_JLE, <=)
		SPECIFY_OP_SIMILAR(OP_JNE, !=)

		case OP_JUMP_RELATIVE: {
			int relpos = read_short(vm);
			vm_registers[REG_IP] += relpos;
		} break;

		case OP_JUMP_ON_FALSE: {
			int relpos = read_short(vm);
			if (vm_registers[REG_COND] == 0) {
				vm_registers[REG_IP] += relpos;
			}
		} break;

		case OP_JUMP_ON_TRUE: {
			int relpos = read_short(vm);
			if (vm_registers[REG_COND] != 0) {
				vm_registers[REG_IP] += relpos;
			}
		} break;


#define SPECIFY_OP_SIMILAR_COND(SPECIFIED_OPERATOR, MATH_OP) \
	case SPECIFIED_OPERATOR: { \
		vm_registers[REG_COND] = 0; \
		int b = stack_pop_int(vm); \
		int a = stack_pop_int(vm); \
		if (a MATH_OP b) \
			vm_registers[REG_COND] = 1; \
	} break;


			SPECIFY_OP_SIMILAR_COND(OP_GEQUAL, >= )
				SPECIFY_OP_SIMILAR_COND(OP_LEQUAL, <= )
				SPECIFY_OP_SIMILAR_COND(OP_LESS, <)
				SPECIFY_OP_SIMILAR_COND(OP_GREATER, >)

//cygwin gcc complaing about some stuff, and cba to look into it so just copied the block twice
#define SPECIFY_OP_EQUALITY(SPECIFIED_OPERATOR, MATH_OP) \
		case SPECIFIED_OPERATOR: { \
			vm_registers[REG_COND] = 0; \
			varval_t *b = stack_pop_vv(vm); \
			varval_t *a = stack_pop_vv(vm); \
			if (VV_IS_STRING(b) || VV_IS_STRING(a)) { \
				const char *sb = se_vv_to_string(vm, b); \
				const char *sa = se_vv_to_string(vm, a); \
				if (MATH_OP (strcmp(sb, sa))) { \
					vm_registers[REG_COND] = 1; \
				} \
			} else { \
				int fb = se_vv_to_int(vm, b); \
				int fa = se_vv_to_int(vm, a); \
				if (!(fb MATH_OP= fa)) \
					vm_registers[REG_COND] = 1; \
			} \
			se_vv_free(vm, b); \
			se_vv_free(vm, a); \
		} break;

		//SPECIFY_OP_EQUALITY(OP_EQ, !)
		//SPECIFY_OP_EQUALITY(OP_NEQ,)

		case OP_EQ: { 
			vm_registers[REG_COND] = 0; 
			varval_t *b = stack_pop_vv(vm); 
			varval_t *a = stack_pop_vv(vm); 
			if (VV_IS_STRING(b) || VV_IS_STRING(a)) { 
				const char *sb = se_vv_to_string(vm, b); 
				const char *sa = se_vv_to_string(vm, a); 
				if (!(strcmp(sb, sa))) { 
					vm_registers[REG_COND] = 1; 
				} 
			} else { 
				int fb = se_vv_to_int(vm, b); 
				int fa = se_vv_to_int(vm, a); 
				if (!(fb != fa)) 
					vm_registers[REG_COND] = 1; 
			} 
			se_vv_free(vm, b); 
			se_vv_free(vm, a); 
		} break;
		
		case OP_NEQ: { 
			vm_registers[REG_COND] = 0; 
			varval_t *b = stack_pop_vv(vm); 
			varval_t *a = stack_pop_vv(vm); 
			if (VV_IS_STRING(b) || VV_IS_STRING(a)) { 
				const char *sb = se_vv_to_string(vm, b); 
				const char *sa = se_vv_to_string(vm, a); 
				if ((strcmp(sb, sa))) { 
					vm_registers[REG_COND] = 1; 
				} 
			} else { 
				int fb = se_vv_to_int(vm, b); 
				int fa = se_vv_to_int(vm, a); 
				if (!(fb == fa)) 
					vm_registers[REG_COND] = 1; 
			} 
			se_vv_free(vm, b); 
			se_vv_free(vm, a); 
		} break;
		
		case OP_DEC: {
			float a = stack_pop_float(vm);
			a -= 1.f;
			se_addfloat(vm, a);
		} break;

		case OP_INC: {
			float a = stack_pop_float(vm);
			a += 1.f;
			se_addfloat(vm, a);
		} break;

		case OP_NOT: {
			int a = stack_pop_int(vm);
			a ^= 1;
			se_addint(vm, a);
		} break;

#define SPECIFY_MATH_OP_SIMILAR(SPECIFIED_OPERATOR, MATH_OP) \
	case SPECIFIED_OPERATOR: { \
		float b = stack_pop_float(vm); \
		float a = stack_pop_float(vm); \
		a MATH_OP b; \
		se_addfloat(vm, a); \
	} break;
#define SPECIFY_MATH_OP_SIMILAR_INT(SPECIFIED_OPERATOR, MATH_OP) \
	case SPECIFIED_OPERATOR: { \
		int b = stack_pop_int(vm); \
		int a = stack_pop_int(vm); \
		a MATH_OP b; \
		se_addint(vm, a); \
	} break;

		SPECIFY_MATH_OP_SIMILAR(OP_SUB, -=)
		SPECIFY_MATH_OP_SIMILAR(OP_MUL, *=)
		SPECIFY_MATH_OP_SIMILAR_INT(OP_MOD, %=)
		SPECIFY_MATH_OP_SIMILAR_INT(OP_XOR, ^=)
		SPECIFY_MATH_OP_SIMILAR_INT(OP_OR, |=)
		SPECIFY_MATH_OP_SIMILAR_INT(OP_AND, &=)

		//special cases //e.g divide zero check and strings appending via +=

		case OP_POST_DECREMENT:
		case OP_POST_INCREMENT: {
			varval_t *vv = (varval_t*)stack_current();
			if (VV_TYPE(vv) == VAR_TYPE_INT) {
				if (instr == OP_POST_INCREMENT)
					++vv->integer;
				else
					--vv->integer;
			}
			//still on stack no need to push/pop
		} break;

		case OP_ADD: {
			varval_t *b = (varval_t*)stack_pop(vm);
			varval_t *a = (varval_t*)stack_pop(vm);
			if (
				(VV_TYPE(b)== VAR_TYPE_STRING || VV_TYPE(a)== VAR_TYPE_STRING)
				||
				(VV_TYPE(b)== VAR_TYPE_INDEXED_STRING || VV_TYPE(a)== VAR_TYPE_INDEXED_STRING)
				) {
				const char *sb = se_vv_to_string(vm, b);
				const char *sa = se_vv_to_string(vm, a);
				size_t newlen = strlen(sb) + strlen(sa) + 1;
				char *tmp = (char*)vm_mem_alloc(vm, newlen);
				snprintf(tmp, newlen, "%s%s", sa, sb);
				se_addstring(vm, tmp);
				vm_mem_free(vm, tmp);
			} //this could be done much cleaner but lazy
			else if (VV_TYPE(b)== VAR_TYPE_VECTOR || VV_TYPE(a)== VAR_TYPE_VECTOR) {
				
				float v[3] = { 0,0,0 };

				if (VV_TYPE(b)== VAR_TYPE_VECTOR) {
					v[0] += b->vec[0];
					v[1] += b->vec[1];
					v[2] += b->vec[2];
				}
				else {
					float fb = se_vv_to_float(b);
					v[0] += fb;
					v[1] += fb;
					v[2] += fb;
				}

				if (VV_TYPE(a)== VAR_TYPE_VECTOR) {
					v[0] += a->vec[0];
					v[1] += a->vec[1];
					v[2] += a->vec[2];
				}
				else {
					float fa = se_vv_to_float(a);
					v[0] += fa;
					v[1] += fa;
					v[2] += fa;
				}
				se_addvector(vm, v);
			}
			else {
				if (VV_TYPE(a) == VAR_TYPE_INT && VV_TYPE(b) == VAR_TYPE_INT) {
					int ib = se_vv_to_int(vm, b);
					int ia = se_vv_to_int(vm, a);
					ia += ib;
					se_addint(vm, ia);
				}
				else {
					float fb = se_vv_to_float(b);
					float fa = se_vv_to_float(a);
					fa += fb;
					se_addfloat(vm, fa);
				}
			}
			se_vv_free(vm, a);
			se_vv_free(vm, b);
		} break;

		case OP_DIV: {
			float b = stack_pop_float(vm);
			if (b == 0) {
				printf("error divide by zero\n");
				b = 1;
			}
			float a = stack_pop_float(vm);
			a /= b;
			se_addfloat(vm, a);
		} break;

		case OP_PRINT: {
			int val = vm_stack[vm_registers[REG_BP] + 0];
			printf("%d\n", val);
#if 0
			int stacktop = vm->stack[vm->registers[REG_SP]];
			printf("PRINT: %d\n", stacktop);
			getchar();
#endif
		} break;

		case OP_JMP: {
			int jmp_loc = read_int(vm);
			vm_registers[REG_IP] = jmp_loc;
		} break;

		case OP_CALL_BUILTIN: {

			int func_name_idx = read_int(vm);
			if (func_name_idx >= vm->istringlistsize)
				func_name_idx = 0; //meh should be NULL or so now it calls smth random from first
			//printf("func_name_idx=%d\n", func_name_idx);
			const char *func_name = vm->istringlist[func_name_idx].string;
			//printf("func_name=%s\n", func_name);
			stockfunction_t *sf = se_find_function_by_name(vm, func_name);
			if (func_name==NULL || sf == NULL) {
				printf("built-in function '%s' does not exist! (%d)\n", func_name, func_name_idx);
				return E_VM_RET_ERROR;
			}
			int numargs = read_int(vm);
			vm->thrunner->numargs = numargs;
			int prev_bp = vm_registers[REG_BP];
			//not rlly need for pushing to stack cuz it's a internal func lol and i don't think u'll call another func from there in the script again
			//Would be terrible
#if 0
			for (int i = 0; i < numargs; i++) {
				int val=vm->stack[vm->registers[REG_SP] - i];
				printf("loc=%d, val=%d\n", vm->registers[REG_SP] - i, val);
			}
#endif
			vm_registers[REG_BP] = vm_registers[REG_SP] - numargs + 1;
			//printf("REG_BP=%d\n", vm_registers[REG_BP]);

			//do the calling here
			varval_t *retval = NULL;
			if (sf->call(vm) != 0) {
				retval = (varval_t*)stack_pop(vm);
			}
			else {
				retval = NULL;
			}

			for (int i = 0; i < numargs; i++) {
				varval_t *vv = (varval_t*)stack_pop(vm); //pop all local vars?
				//--vv->refs;
				se_vv_free(vm, vv);
			}

			vm_registers[REG_BP] = prev_bp;

			stack_push_vv(vm, retval);
		} break;


		case OP_CALL_THREAD: {
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define START_PERF(x)
#define END_PERF(x)
//#define START_PERF(x) unsigned long long x = GetTickCount64();
//#define END_PERF(x) printf("END_PERF %llu for block %s (%d, %s)\n", GetTickCount64() - x, TOSTRING(x), __LINE__, __FILE__);
			START_PERF(start)

			int jmp_loc = stack_pop_int(vm);
			int numargs = stack_pop_int(vm);
			
			START_PERF(args);

			for (int i = 0; i < numargs; i++) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				if (VV_USE_REF(vv))
					vv->refs++;
				vm->tmpstack[i] = (intptr_t)vv;
			}
			END_PERF(args);

			/* create new thread */

			START_PERF(find_thread);
			vm_thread_t *thr = NULL;
			for(int i = 0; i < MAX_SCRIPT_THREADS; i++) {
				if(!vm->threadrunners[i].active) {
					thr=&vm->threadrunners[i];
					break;
				}
				
			}
			END_PERF(find_thread);
			
			if(thr==NULL) {
				printf("MAX SCRIPT THREADS\n");
				return E_VM_RET_ERROR;
			}
			else {
				//printf("num threads = %d\n", vm->numthreadrunners);
			}
			++vm->numthreadrunners;
			//don't clear the stack/and stacksize
			memset(&thr->registers, 0, sizeof(thr->registers));
			//prob just bottleneck anyway \/
			//memset(thr->stack,0,thr->stacksize * sizeof(intptr_t)); //stack full empty ayy
			thr->wait=0;
			thr->numargs=0;
			thr->active = true;
			
			//add new thread to list of threads for next frame if wait occurs etc
			
			vm_thread_t *saverunner = vm->thrunner; //save current runner
			
			vm->thrunner = thr;
		
			//do all the call mimic stuff and sadly has to be same ish as normal call cuz the RET expects this and cba to change it
			
			int curpos = vm_registers[REG_IP];
			stack_push(vm, curpos);
			//printf("call jmp to %d, returning to %d\n", jmp_loc, curpos);

			stack_push(vm, vm_registers[REG_BP]); //save the previous stack frame bp
			vm_registers[REG_BP] = vm_registers[REG_SP] + 1;

			memset(&vm_stack[vm_registers[REG_BP]], 0, sizeof(intptr_t) * MAX_LOCAL_VARS);

			for (int i = 0; i < numargs; i++)
				stack_push(vm, vm->tmpstack[numargs-i-1]);

			//alloc minimum of MAX_LOCAL_VARS values on stack for locals?
			vm_registers[REG_SP] += MAX_LOCAL_VARS - numargs;
			stack_push(vm, numargs);

			vm_registers[REG_IP] = jmp_loc;
			/* end of normal call stuff */

			START_PERF(run_thread);
			while (vm->is_running && vm_registers[REG_IP] != 0) {
				int thr_instr = vm->program[vm_registers[REG_IP]++];

				int vm_ret = vm_execute(vm, thr_instr);
				if (vm_ret == E_VM_RET_ERROR)
					return vm_ret;
				else if (vm_ret == E_VM_RET_WAIT) {
					break;
				}
			}
			END_PERF(run_thread);

			if (thr->wait <= 0) {
				vm_execute(vm, OP_POP); //retval
				thr->active = false;
				--vm->numthreadrunners;
			}

			END_PERF(start)

			vm->thrunner=saverunner; //restore prev thread
		} break;

		case OP_WAIT: {
			float a = stack_pop_float(vm) * 1000.f;
			if (a == 0) {
				printf("invalid wait amount zero!\n");
				a = 1;
			}

			int b = (int)a;
			vm->thrunner->wait = b;

			return E_VM_RET_WAIT;
		} break;

		case OP_CALL: {
			int jmp_loc = read_int(vm);
			int numargs = read_int(vm);

			for (int i = 0; i < numargs; i++) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				if (VV_USE_REF(vv))
					vv->refs++;
				vm->tmpstack[i] = (intptr_t)vv;
			}

			int curpos = vm_registers[REG_IP];
			stack_push(vm, curpos);
			//printf("call jmp to %d, returning to %d\n", jmp_loc, curpos);

			stack_push(vm, vm_registers[REG_BP]); //save the previous stack frame bp
			vm_registers[REG_BP] = vm_registers[REG_SP] + 1;

			memset(&vm_stack[vm_registers[REG_BP]], 0, sizeof(intptr_t) * MAX_LOCAL_VARS);

			for (int i = 0; i < numargs; i++)
				stack_push(vm, vm->tmpstack[numargs-i-1]);

			//alloc minimum of MAX_LOCAL_VARS values on stack for locals?
			vm_registers[REG_SP] += MAX_LOCAL_VARS - numargs;
			stack_push(vm, numargs);

			vm_registers[REG_IP] = jmp_loc;
		} break;

		case OP_RET: {
			varval_t *retval = stack_pop_vv(vm);
			int numargs = stack_pop(vm);

			for (int i = 0; i < MAX_LOCAL_VARS; i++) {
				varval_t *vv=(varval_t*)stack_pop(vm); //pop all local vars?
				if (vv != NULL) {
					//se_vv_remove_reference(vm, vv);
					//can't call because the gc stuff at end rofl
					--vv->refs;
					if (vv->refs > 0) {
						//printf("CANNOT FREE VV BECAUSE REF = %d\n", vv->refs);
						continue;
					}
					if (vv == retval) continue; //dont free the return value rofl
					se_vv_free(vm, vv);
				}
			}

			int bp = stack_pop(vm); //set old stack frame bp again
			vm_registers[REG_BP] = bp;

			int ret_addr = stack_pop(vm);
			//printf("ret_addr=%d\n", ret_addr);
			vm_registers[REG_IP] = ret_addr;

			stack_push_vv(vm, retval);
		} break;

		default: {
			printf("%s not supported!\n", e_opcodes_strings[instr]);
			se_addnull(vm);
		} break;
	}

	return 0;
}

int vm_run_active_threads(vm_t *vm, int frametime) {
	vm_thread_t *thr = NULL;
	int err = 0;

	for(int i = 0; i < MAX_SCRIPT_THREADS; i++) {
		thr=&vm->threadrunners[i];
		
		if(!thr->active)
			continue;
	
		//printf("thr->wait (%d) -= %d\n", thr->wait, frametime);
		if (thr->wait > 0) {
			thr->wait -= frametime;
			continue;
		}
		{
			vm->thrunner = thr;
			while (vm->is_running && vm_registers[REG_IP] != 0) {
				int instr = vm->program[vm->thrunner->registers[REG_IP]++];
				if ((err = vm_execute(vm, instr)) != E_VM_RET_NONE) {
					if (err == E_VM_RET_WAIT)
						break;
					else
						return err;
				}
			}
		}
		if (thr->wait <= 0) {
			vm_execute(vm, OP_POP); //retval
			thr->active=false; //enough to clear it rofl
			--vm->numthreadrunners;
		}
	}
	vm->thrunner = NULL;
	return E_VM_RET_NONE;
}

int vm_exec_thread_pointer(vm_t *vm, int fp, int numargs) {
	vm->thrunner = NULL;

	se_addint(vm, numargs);
	se_addint(vm, fp);
	return vm_execute(vm, OP_CALL_THREAD);
}

int vm_exec_ent_thread_pointer(vm_t *vm, varval_t *new_self, int fp, int numargs) {
	vm->thrunner = NULL;
	varval_t *saveself = vm->self;
	vm->self = new_self;
	se_addint(vm, numargs);
	se_addint(vm, fp);
	int ret = vm_execute(vm, OP_CALL_THREAD);
	vm->self = saveself;
	return ret;
}

int vm_exec_thread(vm_t *vm, const char *func_name, int numargs) {
	
	vm_function_info_t *fi = NULL;

	for (int i = 0; i < vector_count(&vm->functioninfo); i++) {
		vm_function_info_t *ffi = (vm_function_info_t*)vector_get(&vm->functioninfo, i);
		if (!strcmp(ffi->name, func_name)) {
			fi = ffi;
			break;
		}
	}

	if (fi == NULL)
		return 1;
	return vm_exec_thread_pointer(vm, fi->position, numargs);
}

static int vm_read_functions(vm_t *vm) {

	char id[128] = { 0 };
	int id_len = 0;

	int start = *(int*)(vm->program + vm->program_size - sizeof(int));
	int num_funcs = *(int*)(vm->program + vm->program_size - sizeof(int) - sizeof(int));

	int at = start;

	for (int i = 0; i < num_funcs; i++) {
		int loc = *(int*)(vm->program + at);
		at += sizeof(int);


		id_len = 0;
		int ch = vm->program[at];
		while (at < vm->program_size && ch) {
			id[id_len++] = ch = vm->program[at++];
		}
		id[id_len++] = 0;

		vm_function_info_t *fi = (vm_function_info_t*)vm_mem_alloc(vm, sizeof(vm_function_info_t));
		fi->position = loc;
		strncpy(fi->name, id, sizeof(fi->name));
		fi->name[sizeof(fi->name) - 1] = '\0';

		vector_add(&vm->functioninfo, fi);
		//printf("id=%s at %d\n", id, loc);
		//usefull for if u wanna call specific function and this has the locations etc
	}

	//read int of num refs called to builtins
	int num_calls_to_builtin = *(int*)(vm->program + at);
	at += sizeof(int);
	//printf("num_calls_to_builtin=%d\n", num_calls_to_builtin);
	vt_istring_t *istr = NULL;
	for (int i = 0; i < num_calls_to_builtin; i++) {

		id_len = 0;
		int ch = vm->program[at];
		while (at < vm->program_size && ch) {
			id[id_len++] = ch = vm->program[at++];
		}
		id[id_len++] = 0;
		//printf("ID=%s\n", id);
		istr = se_istring_find(vm, id);
		if(istr==NULL)
			se_istring_create(vm, id);
	}

	return 0;
}

#ifdef MEMORY_DEBUG
void *vm_mem_alloc_r(vm_t *vm, size_t sz, const char *_file, int _line) {
#else
void *vm_mem_alloc_r(vm_t *vm, size_t sz) {
#endif
	void *p = malloc(sz);
	vector_add(&vm->__mem_allocations, p);
	return p;
}

#ifdef MEMORY_DEBUG
void vm_mem_free_r(vm_t *vm, void *block, const char *_file, int _line) {
#else
void vm_mem_free_r(vm_t *vm, void *block) {
#endif
	if (!block)
		return;

	int loc = vector_locate(&vm->__mem_allocations, block);

	if (loc != -1)
		vector_delete(&vm->__mem_allocations, loc);
	else {
#ifdef MEMORY_DEBUG
		printf("could not find block in allocs! %s;%d", _file, _line);
#endif
	}
	free(block);
}

vm_t *vm_create(const char *program, int programsize) {
	vm_t *vm = NULL;
	vm = (vm_t*)malloc(sizeof(vm_t));
	if (vm == NULL) {
		printf("vm is NULL\n");
		return vm;
	}

	memset(vm, 0, sizeof(vm_t));

	vector_init(&vm->__mem_allocations);
	vector_init(&vm->vars);

	vector_init(&vm->functioninfo);

	vm->level = se_createobject(vm, VT_OBJECT_LEVEL, NULL, NULL, NULL);
	vm->level->refs = 1337; //will only free after vm frees yup
	vm->self = NULL;

	vm->program = (char*)vm_mem_alloc(vm, programsize);
	memcpy(vm->program, program, programsize);
	vm->program_size = programsize;

#define INITIAL_ISTRING_LIST_SIZE (4096)

	vm->istringlist = (vt_istring_t*)vm_mem_alloc(vm, sizeof(vt_istring_t) * INITIAL_ISTRING_LIST_SIZE);
	memset(vm->istringlist, 0, sizeof(vt_istring_t)*INITIAL_ISTRING_LIST_SIZE);
	vm->istringlistsize = INITIAL_ISTRING_LIST_SIZE;

	extern stockfunction_t std_scriptfunctions[];
	se_register_stockfunction_set(vm, std_scriptfunctions);

	vm->threadrunners = (vm_thread_t*)vm_mem_alloc(vm, sizeof(vm_thread_t) * MAX_SCRIPT_THREADS);
	memset(vm->threadrunners, 0, sizeof(vm_thread_t)*MAX_SCRIPT_THREADS);
	//tmp because of bottleneck everytime allocating new thread stacksize lol
	for (int i = 0; i < MAX_SCRIPT_THREADS; i++) {
		vm_thread_t *thr = &vm->threadrunners[i];
		thr->stacksize = VM_STACK_SIZE;
		thr->stack = (intptr_t*)vm_mem_alloc(vm, thr->stacksize * sizeof(intptr_t));
	}
	vm->numthreadrunners=0;
	vm->thrunner = NULL;

	vm_read_functions(vm);
	vm->is_running = true; //not needed anymore?
	return vm;
}

void vm_free(vm_t *vm) {
	vm->thrunner = NULL;

	int num_vars_left = vector_count(&vm->vars);
	printf("num vars left =%d\n", num_vars_left);

	se_vv_free_force(vm, vm->level);

	//not sure about this, after long time not working on this forgot really what i intended, but i think this should be ok ish
	//try normally
	for (int i = vector_count(&vm->vars) - 1; i > -1; i--) {
		varval_t *vv = (varval_t*)vector_get(&vm->vars, i);
		/*if (vv->refs > 0) {
			int loc = vector_locate(&vm->vars, vv);
			if (loc != -1)
				vector_delete(&vm->vars, loc);
		} else */
			se_vv_free_force(vm, vv);
	}

	vector_free(&vm->vars);

	for (int i = 0; i < MAX_SCRIPT_THREADS; i++) {
		if (vm->threadrunners[i].stack != NULL)
			vm_mem_free(vm, vm->threadrunners[i].stack);
	}
	vm_mem_free(vm, vm->threadrunners);
	vm->thrunner = NULL;

	for (int i = 0; i < vector_count(&vm->functioninfo); i++) {
		vm_function_info_t *fi = (vm_function_info_t*)vector_get(&vm->functioninfo, i);
		vm_mem_free(vm, fi);
	}
	vector_free(&vm->functioninfo);

	vt_istring_t *istr = NULL;
	for (int i = 0; i < vm->istringlistsize; i++) {
		istr = &vm->istringlist[i];
		if (!istr->string)
			continue;
		se_istring_delete(vm, istr);
		istr->index = 0;
	}
	vm_mem_free(vm, vm->istringlist);
	vm->istringlistsize = 0;

	vm_mem_free(vm, vm->program);

	for (int i = 0; i < vector_count(&vm->__mem_allocations); i++) {
		void *p = vector_get(&vm->__mem_allocations, i);
		free(p);
	}
	vector_free(&vm->__mem_allocations);

	free(vm);
}

int vm_get_num_active_threadrunners(vm_t *vm) {
	return vm->numthreadrunners;
}

#endif