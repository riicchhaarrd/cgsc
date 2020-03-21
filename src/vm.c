#if 1

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "common.h"

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
#include "dynstring.h"
#include "cstructparser.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <Windows.h>
#else

#include <dlfcn.h>
#include <sys/mman.h>
#endif

#include "asm.h"

int se_vv_container_size(vm_t *vm, varval_t *vv)
{
	switch (VV_TYPE(vv))
	{
		case VAR_TYPE_OBJECT:
		{
			switch (vv->as.obj->type)
			{
				case VT_OBJECT_BUFFER: {
					vt_buffer_t *vtb = (vt_buffer_t*)vv->as.obj->obj;// DYN_TYPE_HDR(vt_buffer_t, ((char*)vv_arr->as.obj->obj));
					return vtb->size;
				} break;
			}
		} break;
		case VAR_TYPE_ARRAY:
			return vector_count(&vv->as.obj->fields);//se_addint(vm, vv_arr->obj->numfields);
		case VAR_TYPE_STRING:
		case VAR_TYPE_INDEXED_STRING:
			return strlen(se_vv_to_string(vm, vv));
	}
	return -1;
}

int vv_integer_internal_size(varval_t *vv)
{
	if (VV_TYPE(vv) == VAR_TYPE_NULL)
		return 0;

	if (VV_IS_POINTER(vv))
		return sizeof(void*);

	if (VV_IS_STRING(vv))
		return sizeof(char*);

	switch (VV_TYPE(vv))
	{
	case VAR_TYPE_CHAR: return sizeof(vv->as.character);
	case VAR_TYPE_INT: return sizeof(vv->as.integer);
	case VAR_TYPE_SHORT: return sizeof(vv->as.shortint);
	case VAR_TYPE_LONG: return sizeof(vv->as.longint);
	case VAR_TYPE_DOUBLE: return sizeof(vv->as.dbl);
	case VAR_TYPE_FLOAT: return sizeof(vv->as.flt);
	case VAR_TYPE_VECTOR: return sizeof(vv->as.vec);
	case VAR_TYPE_OBJECT:
	{
		if (vv->as.obj->type == VT_OBJECT_BUFFER)
		{
			vt_buffer_t *vtb = (vt_buffer_t*)vv->as.obj->obj;
			return vtb->size;
		}
	} break;
	}
	return 0;
}
int se_vv_type(vm_t *vm, varval_t *vv)
{
	return VV_TYPE(vv);
}
vm_long_t vv_cast_long(vm_t *vm, varval_t *vv)
{
	switch (VV_TYPE(vv))
	{
	case VAR_TYPE_INT:
		return (vm_long_t)vv->as.integer;
	case VAR_TYPE_CHAR:
		return (vm_long_t)vv->as.character;
	case VAR_TYPE_SHORT:
		return (vm_long_t)vv->as.shortint;
	case VAR_TYPE_DOUBLE:
		return (vm_long_t)vv->as.dbl;
	case VAR_TYPE_FLOAT:
		return (vm_long_t)vv->as.flt;
	case VAR_TYPE_STRING:
	case VAR_TYPE_INDEXED_STRING:
		return atoll(se_vv_to_string(vm, vv));
	case VAR_TYPE_OBJECT:
	{
		if (vv->as.obj->type == VT_OBJECT_BUFFER)
		{
			vt_buffer_t *vtb = (vt_buffer_t*)vv->as.obj->obj;
			return vtb->data;
		}
	} break;
	}
	return vv->as.longint;
}

VM_INLINE double vv_cast_double(vm_t *vm, varval_t *vv)
{
	switch (VV_TYPE(vv))
	{
	case VAR_TYPE_INT:
		return (double)vv->as.integer;
	case VAR_TYPE_CHAR:
		return (double)vv->as.character;
	case VAR_TYPE_SHORT:
		return (double)vv->as.shortint;
	case VAR_TYPE_LONG:
		return (double)vv->as.longint;
	case VAR_TYPE_FLOAT:
		return (double)vv->as.flt;
	case VAR_TYPE_STRING:
	case VAR_TYPE_INDEXED_STRING:
		return atof(se_vv_to_string(vm, vv));
	}
	return vv->as.dbl;
}

double VM_INLINE vv_cast_scalar(vm_t *vm, varval_t *vv)
{
	return vv_cast_double(vm, vv);
}

//TODO fix this far from done, but had to go
varval_t *vv_cast(vm_t *vm, varval_t *vv, int desired_type)
{
	if (desired_type == VAR_TYPE_NULL || !vv)
		return NULL;

	varval_t *c = se_vv_create(vm, desired_type);
	switch (desired_type)
	{
	case VAR_TYPE_NULL:
		break;
	case VAR_TYPE_DOUBLE:
	case VAR_TYPE_FLOAT:
		if (desired_type == VAR_TYPE_FLOAT)
			c->as.flt = (float)vv_cast_double(vm, vv);
		else
			c->as.dbl = vv_cast_double(vm, vv);
		break;
	case VAR_TYPE_INT: c->as.integer = (int)vv_cast_long(vm, vv); break;
	case VAR_TYPE_CHAR: c->as.character = (char)vv_cast_long(vm, vv); break;
	case VAR_TYPE_SHORT: c->as.shortint = (short)vv_cast_long(vm, vv); break;
	case VAR_TYPE_LONG: c->as.longint = vv_cast_long(vm, vv); break;
	case VAR_TYPE_INDEXED_STRING: {
		int idx = (int)vv_cast_long(vm, vv);
		const char *str = se_index_to_string(vm, idx);
		if (str != NULL)
			c->as.index = idx;
		else {
			se_vv_free_force(vm, c);
			return NULL;
		}
	} break;

	case VAR_TYPE_STRING:
		if(vv->flags & VF_FFI) {
			c->as.string = NULL;
			vv_string_set(vm, c, vv->as.ptr); //actually is just c char *
		} else
		vv_string_set(vm, c, se_vv_to_string(vm, vv));
		break;
	case VAR_TYPE_VECTOR: {
		double d = vv_cast_double(vm, vv);
		c->as.vec[0] = d;
		c->as.vec[1] = d;
		c->as.vec[2] = d;
	} break;
	default:
		vm_printf("type %s is unsupported for conversion!!!\n", e_var_types_strings[desired_type]);
		se_vv_free_force(vm, c);
		return NULL;
		//memcpy(&c->as, &vv->as, sizeof(c->as));
		break;
	}
	return c;
}

cstruct_t *vm_get_struct(vm_t *vm, unsigned int ind);

cstructfield_t *vm_get_struct_field(vm_t *vm, cstruct_t *cs, const char *s)
{
	for (int i = cs->fields.size; i--;)
	{
		array_get(&cs->fields, cstructfield_t, field, i);
		if (!strcmp(field->name, s))
			return field;
	}
	return NULL;
}

varval_t *se_vv_create_with_ffi_flags(vm_t *vm, int type)
{
	varval_t *vv = se_vv_create(vm, type);
	vv->flags |= VF_FFI;
	return vv;
}

varval_t *vm_get_struct_field_value(vm_t *vm, cstruct_t *cs, cstructfield_t *field, vt_buffer_t *vtb)
{
	//vm_printf("field %s, cs = %s, type = %s\n", field->name, cs->name, ctypestrings[field->type]);
	varval_t *vv = 0;
	void *p = &vtb->data[field->offset];

	if (CTYPE_IS_ARRAY(field->type))
	{
		vv = se_vv_create_with_ffi_flags(vm, VAR_TYPE_LONG);
		vv->as.ptr = p;
		return vv;
	}

	switch (field->type)
	{
	case CTYPE_CHAR:
		vv = se_vv_create_with_ffi_flags(vm, VAR_TYPE_CHAR);
		vv->as.character = *(char*)p;
		return vv;
	case CTYPE_SHORT:
		vv = se_vv_create_with_ffi_flags(vm, VAR_TYPE_SHORT);
		vv->as.shortint = *(short*)p;
		return vv;
	case CTYPE_INT:
		vv = se_vv_create_with_ffi_flags(vm, VAR_TYPE_INT);
		vv->as.integer = *(int*)p;
		return vv;
	case CTYPE_LONG:
		vv = se_vv_create_with_ffi_flags(vm, VAR_TYPE_LONG);
		vv->as.longint = *(vm_long_t*)p;
		return vv;
	case CTYPE_POINTER:
		vv = se_vv_create_with_ffi_flags(vm, VAR_TYPE_LONG); //TODO fix this perhaps but should be fine for the next X years unless sizeof(void*) changes, besides i need to fix ffi x64 first anyways
		vv->as.ptr = *(void**)p; //key difference between array and ptr
		return vv;
	case CTYPE_FLOAT:
		vv = se_vv_create_with_ffi_flags(vm, VAR_TYPE_FLOAT);
		vv->as.flt = *(float*)p;
		return vv;
	case CTYPE_DOUBLE:
		vv = se_vv_create_with_ffi_flags(vm, VAR_TYPE_DOUBLE);
		vv->as.dbl = *(double*)p;
		return vv;
	}
	return NULL;
}

int vm_set_struct_field_value(vm_t *vm, cstruct_t *cs, cstructfield_t *field, vt_buffer_t *vtb, varval_t *vv)
{
	if (CTYPE_IS_ARRAY(field->type))
	{
		vm_printf("expression must be a modifiable lvalue\n");
		return 0;
	}
	switch (field->type)
	{
	case CTYPE_CHAR: {
		unsigned char *p = &vtb->data[field->offset];
		*p = vv_cast_long(vm, vv) & 0xff;
	} break;
	case CTYPE_SHORT: {
		unsigned short *p = &vtb->data[field->offset];
		*p = vv_cast_long(vm, vv) & 0xffff;
	} break;
	case CTYPE_INT: {
		unsigned int *p = &vtb->data[field->offset];
		*p = vv_cast_long(vm, vv) & 0xffffffff;
	} break;
	case CTYPE_LONG: {
		vm_long_t *p = &vtb->data[field->offset];
		*p = vv_cast_long(vm, vv);
	} break;
	case CTYPE_FLOAT: {
		float *p = &vtb->data[field->offset];
		*p = (float)vv_cast_double(vm, vv);
	} break;
	case CTYPE_DOUBLE: {
		double *p = &vtb->data[field->offset];
		*p = vv_cast_double(vm, vv);
	} break;
	case CTYPE_POINTER: {
		void **p = (void**)&vtb->data[field->offset];
		if (VV_IS_STRING(vv))
			*p = se_vv_to_string(vm, vv);
		else
			*p = vv_cast_long(vm, vv);
	} break;
	default:
		return 0;
	}
	return 1;
}

#define Q_MIN(a,b) ((a) > (b) ? (b) : (a))
#define Q_MAX(a,b) ((a) > (b) ? (a) : (b))

int vv_icmp(vm_t *vm, varval_t *a, varval_t *b)
{
	if (VV_IS_INTEGRAL(a) && VV_IS_INTEGRAL(b))
	{
		int sa = vv_integer_internal_size(a);
		int sb = vv_integer_internal_size(b);
		int smallest = Q_MIN(sa, sb);
		//vm_printf("smallest = %d\n", smallest);
		return !memcmp(&a->as, &b->as, smallest);
	}
	else {
		vm_scalar_t ca = vv_cast_double(vm, a);
		vm_scalar_t cb = vv_cast_double(vm, b);
		return ca == cb;
	}
}

static void print_registers(vm_t *vm) {
	int i;
	for (i = 0; i < e_vm_reg_len; i++)
		vm_printf("%s => %d\n", e_vm_reg_names[i], vm_registers[i]);
}

uint8_t VM_INLINE read_byte(vm_t *vm) {
	uint8_t op = vm->instr[vm_registers[REG_IP]];
	++vm_registers[REG_IP];
	return op;
}

int VM_INLINE read_int(vm_t *vm)
{
	int i = *(int*)(vm->instr + vm_registers[REG_IP]);
	vm_registers[REG_IP] += sizeof(int);
	return i;
}

float VM_INLINE read_float(vm_t *vm)
{
	float f = *(float*)(vm->instr + vm_registers[REG_IP]);
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
#if 0
static float se_vv_to_float(varval_t *vv) {
	float ret = 0;
	if (VV_TYPE(vv)== VAR_TYPE_INT)
		ret = (float)vv->as.integer;
	else if (VV_TYPE(vv)== VAR_TYPE_FLOAT)
		ret = vv->as.number;
	return ret;
}
#endif

static int se_vv_to_int(vm_t *vm, varval_t *vv) {
	int ret = vv->as.ptr; //for ffi pointers got back 0, so let's just make this default atm
	switch (VV_TYPE(vv)) {
	case VAR_TYPE_OBJECT:
		switch (vv->as.obj->type)
		{
			case VT_OBJECT_BUFFER:
			{
				vt_buffer_t *vtb = (vt_buffer_t*)vv->as.obj->obj;
				return (intptr_t)vtb->data;
			}
		}
		return 0;
	case VAR_TYPE_INT:
	case VAR_TYPE_FUNCTION_POINTER:
		return vv->as.integer;
	case VAR_TYPE_FLOAT:
		return (int)vv->as.number;
	case VAR_TYPE_STRING:
	case VAR_TYPE_INDEXED_STRING: {
		const char *str = se_vv_to_string(vm, vv);
		return atoi(str);
	} break;
	}
	return ret;
}

//returning 0 means usually an size of 128 -> 2048 should be sufficient or its a internal conversion from int to string format etc
int se_vv_get_string_length(vm_t *vm, varval_t *vv)
{
	switch (VV_TYPE(vv)) {
	case VAR_TYPE_INDEXED_STRING:
		return strlen(se_index_to_string(vm, vv->as.stringindex));
	case VAR_TYPE_STRING:
		return strlen(vv->as.string);
	case VAR_TYPE_VECTOR:
	case VAR_TYPE_LONG:
	case VAR_TYPE_SHORT:
	case VAR_TYPE_INT:
	case VAR_TYPE_FLOAT:
	case VAR_TYPE_DOUBLE:
	case VAR_TYPE_CHAR:
	case VAR_TYPE_NULL:
	case VAR_TYPE_ARRAY:
		return 0;
	case VAR_TYPE_OBJECT:
		switch (vv->as.obj->type)
		{
		case VT_OBJECT_BUFFER:
		{
			vt_buffer_t *vtb = (vt_buffer_t*)vv->as.obj->obj;
			return strlen(vtb->data);
		}
		}
		return 0;
	}
	return 0; //unhandled
}

//function expects that the len given does match the string kind of -> strcpy unsafe
int se_vv_to_string_s(vm_t *vm, varval_t *vv, char *str, size_t len) {
	switch (VV_TYPE(vv)) {
	//kinda didn't wanna resort to using snprintf, and just returning raw ptrs, but oh well here we are cuz we copy them now etc
	case VAR_TYPE_INDEXED_STRING:
	{
		char *ptr = se_index_to_string(vm, vv->as.stringindex);
		if (!ptr)
			ptr = "";
		strcpy(str, ptr);
	} break;
	case VAR_TYPE_STRING:
	{
		char *ptr = vv->as.string;
		if (!ptr)
			ptr = "";
		strcpy(str, ptr);
	} break;
	case VAR_TYPE_VECTOR:
		snprintf(str, len, "(%f, %f, %f)", vv->as.vec[0], vv->as.vec[1], vv->as.vec[2]);
		break;
	case VAR_TYPE_LONG:
		snprintf(str, len, "%lld", vv->as.longint);
		break;
	case VAR_TYPE_SHORT:
		snprintf(str, len, "%h", vv->as.shortint);
		break;
	case VAR_TYPE_INT:
		snprintf(str, len, "%d", vv->as.integer);
		//note %f and %lf are same, since float gets promoted to double when calling vm_printf
		break;
	case VAR_TYPE_FLOAT:
		snprintf(str, len, "%f", vv->as.number);
		break;
	case VAR_TYPE_DOUBLE:
		snprintf(str, len, "%lf", vv->as.dbl);
		break;
	case VAR_TYPE_CHAR:
		snprintf(str, len, "%c", vv->as.character);
		break;
	case VAR_TYPE_NULL:
		snprintf(str, len, "%s", "[null]");
		break;
	case VAR_TYPE_OBJECT:
		switch (vv->as.obj->type)
		{
			case VT_OBJECT_BUFFER:
			{
				vt_buffer_t *vtb = (vt_buffer_t*)vv->as.obj->obj;
				strcpy(str, vtb->data);
			} break;

			default:
				snprintf(str, len, "%s", "[object]");
				break;
		}
		break;
	case VAR_TYPE_ARRAY:
		snprintf(str, len, "%s", "[array]");
		break;
	default:
		snprintf(str, len, "%s", "[unhandled variable type]");
		break;
	}
	return 0;
}

const char *se_vv_to_string(vm_t *vm, varval_t *vv)
{
#if 0
	static char test[128] = { 0 };
	se_vv_to_string_s(vm, vv, test, sizeof(test));
	return test;
#endif

	if (vm->thrunner == NULL)
	{
		vm_printf("thrunner NULL fatal error\n");
		getchar();
		exit(0);
	}
	char *p = NULL;
	int len = se_vv_get_string_length(vm,vv);
	if (len == 0)
		len = 256; //should be enough
	//alloc new buffer with dynamic size
	p = (char*)vm_mem_alloc(vm, len + 1); //for \0
	memset(p, 0, len);
	se_vv_to_string_s(vm, vv, p, len);
	//vm_printf("type=%s, p=%s, len=%d\n", VV_TYPE_STRING(vv), p, len);
	vector_add(&vm->thrunner->strings, p);
	return p;
}

float se_getfloat(vm_t *vm, int i)
{
	varval_t *vv = se_argv(vm, i);
	float ret = (float)vv_cast_double(vm, vv);
	return ret;
}

const char *se_getstring(vm_t *vm, int i)
{
	varval_t *vv = se_argv(vm, i);
	return se_vv_to_string(vm, vv);
}

int se_getint(vm_t *vm, int i)
{
	varval_t *vv = se_argv(vm, i);
	int ret = se_vv_to_int(vm, vv);
	return ret;
}

vm_function_t se_getfunc(vm_t *vm, int i)
{
	varval_t *vv = se_argv(vm, i);
	vm_function_t ret = (vm_function_t)se_vv_to_int(vm, vv);
	return ret;
}

int se_getistring(vm_t *vm, int i) {
	varval_t *vv = se_argv(vm, i);
	if (VV_TYPE(vv) != VAR_TYPE_INDEXED_STRING) {
		vm_printf("'%s' is not an indexed string!\n", e_var_types_strings[VV_TYPE(vv)]);
		return 0;
	}
	return vv->as.stringindex;
}

void se_addint(vm_t *vm, int i) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_INT);
	vv->as.integer = i;
	stack_push_vv(vm, vv);
}

void se_addchar(vm_t *vm, int c)
{
	varval_t *vv = se_vv_create(vm, VAR_TYPE_CHAR);
	vv->as.character = c;
	stack_push_vv(vm, vv);
}

void se_addobject(vm_t *vm, varval_t *vv) {
	stack_push_vv(vm, vv);
}

varval_t *se_createobject(vm_t *vm, int type, vt_obj_field_custom_t* custom, void* constructor, void *deconstructor) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_OBJECT);
	vt_object_t *obj = vv->as.obj;
	obj->custom = custom;
	obj->constructor = constructor;
	obj->deconstructor = (void(*)(vm_t*,void*))deconstructor;
	obj->type = type;
	obj->obj = NULL;
	return vv;
}

//apparently unsafe? check later
//temporarily enabled again because i'm using this in my project for something
#if 1
//unsafe
void se_set_object_field(vm_t *vm, const char *key) {
	varval_t *val = (varval_t*)stack_pop(vm);
	varval_t *obj = (varval_t*)stack_pop(vm);

	vt_istring_t *istr = se_istring_find(vm, key);
	if (istr == NULL)
		istr = se_istring_create(vm, key);
	se_vv_set_field(vm, obj, istr->index, val);
	se_vv_free(vm, val); //free the value as the set_field creates a copy of the value
	//if (!se_vv_free(vm, obj)) //should free this aswell right if it's a temp obj)
		//stack_push(vm, 0);
	//else
		stack_push_vv(vm, obj); //put it back :D
}
#endif

varval_t *se_createarray(vm_t *vm) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_ARRAY);
	return vv;
}

void se_getvector(vm_t *vm, int i, float *vec) {
	vec[0] = vec[1] = vec[2] = 0.f;

	varval_t *vv = se_argv(vm, i);
	if (VV_TYPE(vv) != VAR_TYPE_VECTOR) {
		vm_printf("'%s' is not an vector!\n", e_var_types_strings[VV_TYPE(vv)]);
		return;
	}
	vec[0] = vv->as.vec[0];
	vec[1] = vv->as.vec[1];
	vec[2] = vv->as.vec[2];
}

void se_addvector(vm_t *vm, const float *vec) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_VECTOR);
	vv->as.vec[0] = vec[0];
	vv->as.vec[1] = vec[1];
	vv->as.vec[2] = vec[2];
	stack_push_vv(vm, vv);
}

void se_addvectorf(vm_t *vm, float x, float y, float z) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_VECTOR);
	vv->as.vec[0] = x;
	vv->as.vec[1] = y;
	vv->as.vec[2] = z;
	stack_push_vv(vm, vv);
}

void se_addnull(vm_t *vm) {
	stack_push(vm, NULL);
}

void se_addistring(vm_t *vm, int idx) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_INDEXED_STRING);
	vv->as.stringindex = idx;
	stack_push_vv(vm, vv);
}

varval_t *se_createstring(vm_t *vm, const char *s)
{
	varval_t *vv = se_vv_create(vm, VAR_TYPE_STRING);
	vv_string_set(vm, vv, s);
	return vv;
}

void se_addstring(vm_t *vm, const char *s) {
	varval_t *vv = se_createstring(vm, s);
	stack_push_vv(vm, vv);
}

void se_addfloat(vm_t *vm, float f) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_FLOAT);
	vv->as.number = f;
	stack_push_vv(vm, vv);
}

void se_addscalar(vm_t *vm, vm_scalar_t s) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_DOUBLE);
	vv->as.dbl = s;
	stack_push_vv(vm, vv);
}

void se_addiscalar(vm_t *vm, vm_iscalar_t s) {
	varval_t *vv = se_vv_create(vm, VAR_TYPE_LONG);
	vv->as.longint = s;
	stack_push_vv(vm, vv);
}

char *vm_strdup(vm_t *vm, const char *str)
{
	size_t new_size = strlen(str) + 1;
	char *new_str = vm_mem_alloc(vm, new_size);
	snprintf(new_str, new_size, "%s", str);
	return new_str;
}

void se_register_stockfunction_set(vm_t *vm, stockfunction_t *sf)
{
	vm->stockfunctionsets[vm->numstockfunctionsets++] = sf;
}

void se_register_stockmethod_set(vm_t *vm, int object_type, stockmethod_t *set) {
	vm->stockmethodobjecttypes[vm->numstockmethodsets] = object_type;
	vm->stockmethodsets[vm->numstockmethodsets] = set;
	++vm->numstockmethodsets;
}

static stockmethod_t *se_find_method_by_name(vm_t *vm, int object_type, const char *s) {
	stockmethod_t *sm = NULL;
	if (!s) return sm;

	for (int fff = 0; fff < vm->numstockmethodsets; fff++) {
		sm = vm->stockmethodsets[fff];
		if (vm->stockmethodobjecttypes[fff] == object_type)
		{
			for (int i = 0; sm[i].name; i++) {
				if (!strcmp(sm[i].name, s)) {
					return &sm[i];
				}
			}
			break;
		}
	}
	return NULL;
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

static vm_long_t stack_pop_long(vm_t *vm) {
	varval_t *vv = (varval_t*)stack_pop(vm);
	vm_long_t l = vv_cast_long(vm, vv);
	se_vv_free(vm, vv);
	return l;
}

static vm_scalar_t stack_pop_scalar(vm_t *vm) {
	varval_t *vv = (varval_t*)stack_pop(vm);
	vm_scalar_t s = vv_cast_scalar(vm, vv);
	se_vv_free(vm, vv);
	return s;
}

typedef struct
{
	union
	{
		vm_scalar_t value[16]; //4x4 matrix use this as max?
		vm_iscalar_t ivalue[16];
	};
	size_t nelements;
	bool integral;
} vm_vector_t;

VM_INLINE size_t vm_vector_num_elements(vm_vector_t *v) { return v->nelements; }
VM_INLINE vm_scalar_t vm_vector_dot(vm_vector_t *a, vm_vector_t *b) {
	vm_scalar_t total = 0.0;
	int na = vm_vector_num_elements(a);
	int nb = vm_vector_num_elements(b);
	int mx = Q_MAX(na, nb);
	for (int i = mx; i--;)
		total += (a->value[i % na] * b->value[i % nb]);
	return total;
}
VM_INLINE vm_scalar_t vm_vector_length2(vm_vector_t *v) { return vm_vector_dot(v, v); }
VM_INLINE vm_vector_length(vm_vector_t *v) { return sqrt(vm_vector_length2(v)); }

static vm_vector_t _vconst0;
static vm_vector_t _vconst1;
static vm_vector_t _vconst2;
static vm_vector_t _vconst3;

VM_INLINE bool vv_cast_vector(vm_t *vm, varval_t *vv, vm_vector_t *vec)
{
	bool success = true;

	if (VV_IS_NUMBER(vv))
	{
		vec->nelements = 1;
		vec->integral = VV_IS_INTEGRAL(vv);
		if (vec->integral)
			vec->ivalue[0] = vv_cast_long(vm, vv);
		else
			vec->value[0] = vv_cast_double(vm, vv);
	}
	else {
		switch (VV_TYPE(vv))
		{
		case VAR_TYPE_VECTOR:
			vec->integral = false;
			vec->nelements = 3;
			for (int i = 3; i--;)
				vec->value[i] = vv->as.vec[i];
			break;
		default:
			success = false;
			break;
		}
	}
	return success;
}

static bool stack_pop_vector(vm_t *vm, vm_vector_t *vec)
{
	varval_t *vv = (varval_t*)stack_pop(vm);
	bool b = vv_cast_vector(vm, vv, vec);
	se_vv_free(vm, vv);
	return b;
}

void se_addnvector(vm_t *vm, const vm_vector_t *vec) {
	switch (vec->nelements) {
		case 1:
			if (vec->integral)
				se_addiscalar(vm, vec->ivalue[0]);
			else
				se_addscalar(vm, vec->value[0]);
			break;
		case 3: {
			varval_t * vv = se_vv_create(vm, VAR_TYPE_VECTOR);
			vv->as.vec[0] = vec->value[0];
			vv->as.vec[1] = vec->value[1];
			vv->as.vec[2] = vec->value[2];
			stack_push_vv(vm, vv);
		} break;
		default:
			vm_printf("unsupported %d n vector\n", vec->nelements);
			se_addnull(vm);
		break;
	}
}

void vm_vector_math_op(vm_t *vm, vm_vector_t *va, vm_vector_t *vb, vm_vector_t *vc, int op)
{
	int na = va->nelements;
	int nb = vb->nelements;
	int nm = Q_MAX(na, nb);
	vm_scalar_t *a = (vm_scalar_t*)&va->value[0];
	vm_scalar_t *b = (vm_scalar_t*)&vb->value[0];
	vm_scalar_t *c = (vm_scalar_t*)&vc->value[0];

	vm_iscalar_t *ia = (vm_iscalar_t*)&va->ivalue[0];
	vm_iscalar_t *ib = (vm_iscalar_t*)&vb->ivalue[0];
	vm_iscalar_t *ic = (vm_iscalar_t*)&vc->ivalue[0];
	//vm_printf("na=%d,nb=%d,max=%d\n", na, nb, nm);

#define MATH_VEC_OP_MACRO(x, y) \
case x: \
for (int i = nm; i--;) \
c[i] = a[i % na] y b[i % nb]; \
break;
#define MATH_IVEC_OP_MACRO(x, y) \
case x: \
for (int i = nm; i--;) \
ic[i] = ia[i % na] y ib[i % nb]; \
break;
	
	if (va->integral && vb->integral)
	{
		switch (op)
		{
				MATH_IVEC_OP_MACRO(OP_SUB, -)
				MATH_IVEC_OP_MACRO(OP_ADD, +)
				MATH_IVEC_OP_MACRO(OP_DIV, / )
				MATH_IVEC_OP_MACRO(OP_MUL, *)
		}
		vc->integral = true;
	}
	else {
		switch (op)
		{
				MATH_VEC_OP_MACRO(OP_SUB, -)
				MATH_VEC_OP_MACRO(OP_ADD, +)
				MATH_VEC_OP_MACRO(OP_DIV, / )
				MATH_VEC_OP_MACRO(OP_MUL, *)
		}
		vc->integral = false;
	}
	vc->nelements = nm;
}

VM_INLINE float stack_pop_float(vm_t *vm) {
	varval_t *vv = (varval_t*)stack_pop(vm);
	float f = (float)vv_cast_double(vm, vv);
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
			vm_printf("%d: %s (%d ms)\n", i, e_opcodes_strings[pf[i].op], pf[i].duration);
		}
		Sleep(1000);
	}
#endif
	return r;
}
#endif

static inline int op_idiv(int a, int b) { return a / b; }
static inline float op_fdiv(float a, float b) { return a / b; }
static inline int op_iadd(int a, int b) { return a + b; }
static inline int op_isub(int a, int b) { return a - b; }
static inline float op_fsub(float a, float b) { return a - b; }
static inline float op_fadd(float a, float b) { return a + b; }
static inline int op_imul(int a, int b) { return a * b; }
static inline float op_fmul(float a, float b) { return a * b; }
static inline int op_ixor(int a, int b) { return a ^ b; }
static inline int op_ior(int a, int b) { return a | b; }
static inline int op_iand(int a, int b) { return a & b; }

typedef struct
{
	int op;
	int(*ifunc)(int, int);
	float(*ffunc)(float, float);
} math_oplist_t;

static const math_oplist_t g_math_oplist[] =
{
	{ OP_DIV, &op_idiv,&op_fdiv },
{ OP_ADD, &op_iadd,&op_fadd },
{ OP_SUB, &op_isub,&op_fsub },
{ OP_MUL, &op_imul,&op_fmul },
{ OP_XOR, &op_ixor,NULL },
{ OP_OR, &op_ior,NULL },
{ OP_AND, &op_iand,NULL },
{ OP_END_OF_LIST, 0, 0 }
};

typedef enum
{
	OPERANDFLAG_LVALUE_REAL = 1,
	OPERANDFLAG_RVALUE_REAL = 2,
	OPERANDFLAG_REAL = OPERANDFLAG_LVALUE_REAL | OPERANDFLAG_RVALUE_REAL,
} operandflags_t;

typedef enum {
	OP_HANDLER_NONE = 0,
	OP_HANDLER_FREE = 1,
	OP_HANDLER_PUSH_STACK = 2,
} ophandlerflags_t;

varval_t *vm_math_op_handler(vm_t *vm, int math_op, varval_t *a, varval_t *b, int flags)
{
	if (a == NULL)
		a = stack_pop_vv(vm);
	if (b == NULL)
		b = stack_pop_vv(vm);

	if (!VV_IS_NUMBER(a) || !VV_IS_NUMBER(b))
	{
		vm_printf("not a number!\n");
		return NULL;
	}

	int operandflags = 0;

	if (VV_TYPE(a) == VAR_TYPE_FLOAT)
		operandflags |= OPERANDFLAG_LVALUE_REAL;
	if (VV_TYPE(b) == VAR_TYPE_FLOAT)
		operandflags |= OPERANDFLAG_RVALUE_REAL;

	for (int i = 0; g_math_oplist[i].op != OP_END_OF_LIST; ++i)
	{
		math_oplist_t *entry = &g_math_oplist[i];
		varval_t *vv = 0;
		int ival1, ival2;
		if (!operandflags)
		{
			ival1 = a->as.integer;
			ival2 = b->as.integer;
		try_int:
			vv = se_vv_create(vm, VAR_TYPE_INT);
			vv->as.integer = entry->ifunc(ival1, ival2);
		}
		else {
			if (entry->ffunc == NULL)
			{
				ival1 = se_vv_to_int(vm, a);
				ival2 = se_vv_to_int(vm, b);
				goto try_int;
			}
			vv = se_vv_create(vm, VAR_TYPE_FLOAT);
			vv->as.number = entry->ffunc((float)vv_cast_double(vm, a), (float)vv_cast_double(vm, b));
		}
		if (flags & OP_HANDLER_PUSH_STACK)
			stack_push_vv(vm, vv);
		if (flags & OP_HANDLER_FREE)
		{
			se_vv_free(vm, a);
			se_vv_free(vm, b);
		}
		return vv;
	}
	vm_printf("opcode not supported! fix this\n");
	return NULL;
}

void vt_buffer_deconstructor(vm_t *vm, vt_buffer_t *vtb)
{
	//if(!vtb->managed)
		free(vtb);
}

#if 0
//TODO: later
static VM_INLINE int vm_jit(vm_t *vm, int instr, unsigned char *asm)
{
	//FINISH THIS SOMEDAY
	switch (instr)
	{
	case OP_RET:
		ret(asm, 0);
		break;
	case OP_PUSH:
	{
		int imm = read_int(vm);
		push_imm(asm, imm);
	} break;
	case OP_PUSH_STRING:
		push_imm(asm, vm->istringlist[read_int(vm)].string);
		break;
	case OP_LOAD:
		//TODO:
		break;
	}
}
#endif

//NOTE
//we're using a default primitive types use copy instead of reference so any time that stack_pop is used, free the value (try to when the refs are <= 0)
//use se_vv_free(vm, vv);

int vm_execute(vm_t *vm, int instr) {
#if 0
	if (instr < OP_END_OF_LIST) {
		vm_printf("%s\n", e_opcodes_strings[instr]);
		//vm_printf("OP: %s (Location %d)\n", e_opcodes_strings[instr], vm->registers[REG_IP]);
		if (!vm->thrunner)
		{
			vm_printf("__init:\n");

		} else {
			vm_function_info_t *fi = NULL;
			int funcinfocount = vector_count(&vm->functioninfo);
			for (int i = funcinfocount; i--;) {
				vm_function_info_t *ffi = (vm_function_info_t*)vector_get(&vm->functioninfo, i);
				if (ffi->position == vm_registers[REG_IP]-1)
				{
					vm_printf("%s:\n", ffi->name);
					break;
				}
			}
		}
		vm_printf("\t%s\n", e_opcodes_strings[instr]);
	}
#endif
	//vm_printf("ins -> %s\n", e_opcodes_strings[instr]);

	switch (instr) {
		case OP_HALT: {
			vm->is_running = false;
			vm_printf("HALT\n");
		} break;

		case OP_SHOW_LOCAL_VARS: {
			int am = read_int(vm);
			if (am > MAX_LOCAL_VARS)
				am = MAX_LOCAL_VARS;
			for (int i = 0; i < am; i++) {
				vm_printf("var %d => %d\n", i, vm_stack[vm_registers[REG_BP] + i]);
			}
			getchar();
		} break;

		case OP_BRK: {
			vm_printf("Break at %d\n", vm_registers[REG_IP - 1]);
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
			vv->as.integer = i;
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
			se_addobject(vm, se_createobject(vm, VT_OBJECT_GENERIC, NULL, NULL, NULL));
		} break;

		case OP_PUSH_BUFFER: {
			int s_ind = read_short(vm);
			//vm_printf("s_ind=%d\n", s_ind);
			int sz = read_short(vm);

			cstruct_t *cs = vm_get_struct(vm, s_ind);
			if (cs == NULL)
			{
				se_addnull(vm);
			} else {

				varval_t *vv = se_createobject(vm, VT_OBJECT_BUFFER, NULL, NULL, (void*)vt_buffer_deconstructor); //todo add the file deconstructor? :D
				vt_buffer_t *vtb = (vt_buffer_t*)malloc(sizeof(vt_buffer_t) + sz);
				vtb->size = sz;
				vtb->data = ((char*)vtb) + sizeof(vt_buffer_t);
				//vm_printf("spawning %s\n", cs->name);
				vtb->type = s_ind;
				vv->as.obj->obj = vtb;
				stack_push_vv(vm, vv);
			}
		} break;

		case OP_PUSH_ARRAY: {
			int num_elements = read_short(vm);
			varval_t *arr = se_createarray(vm);
			//vm_printf("num_elements=%d\n", num_elements);
			for (int i = num_elements; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				//vm_printf("ADDED FIELD\n");
				se_vv_set_field(vm, arr, i, vv);
				se_vv_free(vm, vv);
			}
			stack_push_vv(vm, arr);
		} break;

		case OP_PUSH_VECTOR: {
			varval_t *vv = se_vv_create(vm, VAR_TYPE_VECTOR);
			for (int i = 3; i--;) {
				varval_t *fl = (varval_t*)stack_pop(vm);
				vv->as.vec[i] = (float)vv_cast_double(vm, fl);
				se_vv_free(vm, fl);
			}
			stack_push_vv(vm, vv);
		} break;

		case OP_STORE_ARRAY_INDEX: {
			varval_t *vv = (varval_t*)stack_pop(vm);
			varval_t *vv_index = (varval_t*)stack_pop(vm);
			varval_t *vv_arr = (varval_t*)stack_pop(vm);

			if (VV_TYPE(vv_arr) == VAR_TYPE_ARRAY) {
				just_normal_obj:
				se_vv_set_field(vm, vv_arr, se_vv_to_int(vm, vv_index), vv);
			}
			else if (VV_TYPE(vv_arr) == VAR_TYPE_OBJECT) {
				switch (vv_arr->as.obj->type)
				{
					case VT_OBJECT_BUFFER:
					{
						int ind_as_int = vv_cast_long(vm, vv_index);
						vt_buffer_t *vtb = (vt_buffer_t*)vv_arr->as.obj->obj;
						vtb->data[ind_as_int % vtb->size] = vv_cast_long(vm, vv) & 0xff;
					} break;
					default:
					{
						//if (VV_IS_STRING(vv_index)) //for this case, just convert them into a string type even if they're indexes/numbers or so
						{
							const char *si = se_vv_to_string(vm, vv_index);
							vt_istring_t *istr = se_istring_find(vm, si);
							if(istr==NULL)
								istr = se_istring_create(vm, si);
							stack_push_vv(vm, vv_arr);
							stack_push_vv(vm, vv);
							stack_push(vm, istr->index);
							//push these back up
							se_vv_free(vm, vv_index);
							return vm_execute(vm, OP_STORE_FIELD_VM);
						}
					} break;
				}
			}
			else {
				vm_printf("'%s' is not an array!\n", e_var_types_strings[VV_TYPE(vv_arr)]);
			}
			//changed to fit reqs, all stack_pops that operate on things need to free
			se_vv_free(vm, vv);
			se_vv_free(vm, vv_arr);
			se_vv_free(vm, vv_index);
			//vm_printf("str = %s\n", vm->istringlist[str_index].string);
		} break;

		case OP_GET_LEVEL: {
			se_addobject(vm, vm->level);
		} break;

		case OP_GET_SELF: {
			if (!vm->thrunner)
				stack_push(vm, 0);
			else
				stack_push_vv(vm, vm->thrunner->self);
		} break;

		case OP_GET_LENGTH: {
			varval_t *vv_arr = (varval_t*)stack_pop(vm);
			int len = se_vv_container_size(vm, vv_arr);
			if (len == -1)
			{
				vm_printf("'%s' is not an container type, cannot get length!\n", e_var_types_strings[VV_TYPE(vv_arr)]);
				se_addnull(vm);
			}
			se_addint(vm, len);
			se_vv_free(vm, vv_arr);
#if 0
			vm_printf("refs = %d\n", vv_arr->refs);
			int val = se_vv_free(vm, vv_arr); //mmhmm?
			vm_printf("freed=%d,%s\n", val, VV_TYPE_STRING(vv_arr));
#endif
		} break;

		case OP_LOAD_ARRAY_INDEX: {

			varval_t *arr_index = (varval_t*)stack_pop(vm);
			varval_t *arr = (varval_t*)stack_pop(vm);
			int idx = 0;
			if (VV_TYPE(arr) == VAR_TYPE_VECTOR) {
				idx = se_vv_to_int(vm, arr_index);
				if (idx < 0 || idx > 2) {
					vm_printf("vector index out of bounds!\n");
					idx = 0;
				}
				se_addfloat(vm, arr->as.vec[idx]);
			}
			else if (VV_TYPE(arr) == VAR_TYPE_OBJECT)
			{
				switch (arr->as.obj->type)
				{
				case VT_OBJECT_BUFFER:
				{
					int ind_as_int = vv_cast_long(vm, arr_index);
					vt_buffer_t *vtb = (vt_buffer_t*)arr->as.obj->obj;
					se_addchar(vm, vtb->data[ind_as_int%vtb->size]);
				} break;
				//default: goto just_normal_objload;
				default:
				{
					//if (VV_TYPE(arr_index) == VAR_TYPE_STRING) //even if it's not a string just make it a string and check if the key exists
					{
						const char *si = se_vv_to_string(vm, arr_index);
						vt_istring_t *istr = se_istring_find(vm, si);
						if (istr != NULL)
						{
							stack_push_vv(vm, arr);
							stack_push(vm, istr->index);
							//push these back up
							se_vv_free(vm, arr_index);
							return vm_execute(vm, OP_LOAD_FIELD_VM);
						}
						else goto just_normal_objload;
					}
					//else
						//goto just_normal_objload;
				}
				}
			}
			else if (VV_TYPE(arr) == VAR_TYPE_ARRAY) {
				just_normal_objload:
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
					//se_addstring(vm, ts);
					se_addchar(vm, str[idx]);
				}
			} else {
				vm_printf("%s is not an array!\n", e_var_types_strings[VV_TYPE(arr)]);
				se_addnull(vm);
			}
			se_vv_free(vm, arr_index);
			se_vv_free(vm, arr);
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
			vm_printf("no-operation\n");
		break;

		case OP_STORE_FIELD: {
			int str_index = read_short(vm);
			varval_t *vv = (varval_t*)stack_pop(vm);
			varval_t *vv_obj = (varval_t*)stack_pop(vm);

			if (VV_TYPE(vv_obj)== VAR_TYPE_OBJECT)
			{
				if (str_index >= vm->istringlistsize)
				{
					vm_printf("str_index out of bounds!!\n");
				} else {
					if (vv_obj->as.obj->type == VT_OBJECT_BUFFER)
					{
						vt_buffer_t *vtb = (vt_buffer_t*)vv_obj->as.obj->obj;// DYN_TYPE_HDR(vt_buffer_t, (char*)vv_obj->as.obj->obj);
						cstruct_t *cs = vm_get_struct(vm, vtb->type);
						if (cs)
						{
							const char *str = vm->istringlist[str_index].string;
							cstructfield_t *field = vm_get_struct_field(vm, cs, str);
							if (field)
							{
								//vm_printf("trying to set ptr on struct %s (%s-> type = %s, sz %d, off %d)\n", cs->name, field->name, ctypestrings[field->type], field->size, field->offset);
								vm_set_struct_field_value(vm, cs, field, vtb, vv);
							}
							else
								vm_printf("field '%s' not found for struct '%s'!\n", str, cs->name);
						}
						else
							vm_printf("cs is NULL! %d\n", vtb->type);
					}
					else {
						//const char *str = vm->istringlist[str_index].string; //just here for debug purpose
						se_vv_set_field(vm, vv_obj, str_index, vv);
					}
				}
			}
			else if (VV_TYPE(vv_obj)== VAR_TYPE_ARRAY) {
				se_vv_set_field(vm, vv_obj, str_index, vv);
			}
			se_vv_free(vm, vv);
			se_vv_free(vm, vv_obj);
#if 0
			if (!strcmp(vm->istringlist[str_index].string, "players")) {//storing [] players
				vv->flags |= VAR_FLAG_LEVEL_PLAYER;
				vm_printf("refs = %d, str = %s\n", vv->refs, vm->istringlist[str_index].string);
			}
#endif
		} break;

		case OP_STORE_FIELD_VM: {
			int str_index = stack_pop(vm);
			varval_t *vv = (varval_t*)stack_pop(vm);
			varval_t *vv_obj = (varval_t*)stack_pop(vm);

			if (VV_TYPE(vv_obj) == VAR_TYPE_OBJECT)
			{
				if (str_index >= vm->istringlistsize)
				{
					vm_printf("str_index out of bounds!!\n");
				}
				else {
					if (vv_obj->as.obj->type == VT_OBJECT_BUFFER)
					{
						vt_buffer_t *vtb = (vt_buffer_t*)vv_obj->as.obj->obj;// DYN_TYPE_HDR(vt_buffer_t, (char*)vv_obj->as.obj->obj);
						cstruct_t *cs = vm_get_struct(vm, vtb->type);
						if (cs)
						{
							const char *str = vm->istringlist[str_index].string;
							cstructfield_t *field = vm_get_struct_field(vm, cs, str);
							if (field)
							{
								//vm_printf("trying to set ptr on struct %s (%s-> type = %s, sz %d, off %d)\n", cs->name, field->name, ctypestrings[field->type], field->size, field->offset);
								vm_set_struct_field_value(vm, cs, field, vtb, vv);
							}
							else
								vm_printf("field not found for struct %d!\n", cs->name);
						}
						else
							vm_printf("cs is NULL! %d\n", vtb->type);
					}
					else {
						//const char *str = vm->istringlist[str_index].string; //just here for debug purpose
						se_vv_set_field(vm, vv_obj, str_index, vv);
					}
				}
			}
			else if (VV_TYPE(vv_obj) == VAR_TYPE_ARRAY) {
				se_vv_set_field(vm, vv_obj, str_index, vv);
			}
			se_vv_free(vm, vv);
			se_vv_free(vm, vv_obj);
#if 0
			if (!strcmp(vm->istringlist[str_index].string, "players")) {//storing [] players
				vv->flags |= VAR_FLAG_LEVEL_PLAYER;
				vm_printf("refs = %d, str = %s\n", vv->refs, vm->istringlist[str_index].string);
			}
#endif
		} break;

		case OP_LOAD_FIELD: {

			int str_index = read_short(vm);
			varval_t *vv_obj = (varval_t*)stack_pop(vm);

			if (str_index >= vm->istringlistsize)
				vm_printf("str_index out of bounds!!\n");
			if (VV_TYPE(vv_obj) == VAR_TYPE_OBJECT && vv_obj->as.obj->type == VT_OBJECT_BUFFER)
			{
				vt_buffer_t *vtb = (vt_buffer_t*)vv_obj->as.obj->obj;// DYN_TYPE_HDR(vt_buffer_t, (char*)vv_obj->as.obj->obj);
				cstruct_t *cs = vm_get_struct(vm, vtb->type);
				if (cs)
				{
					const char *str = vm->istringlist[str_index].string;
					cstructfield_t *field = vm_get_struct_field(vm, cs, str);
					if (field)
					{
						varval_t *vv = vm_get_struct_field_value(vm, cs, field, vtb); //will always be a copy because no references it doesn't exist it needs to be made
						stack_push_vv(vm, vv);
					}
					else
						vm_printf("field not found for struct %d!\n", cs->name);
				}
				else
					vm_printf("cs is NULL! %d\n", vtb->type);
			}
			else {
					const char *str = vm->istringlist[str_index].string;
					//vm_printf("LOAD_FIELD{%s}\n", vm->istringlist[str_index].string);
					if (VV_TYPE(vv_obj) == VAR_TYPE_OBJECT)
					{
						//se_vv_get_field also uses stack_pop but that is up to the compiler to provide with expressions that dont store to add a OP_POP
						varval_t *vv = se_vv_get_field(vm, vv_obj, str_index);
						if (NULL == vv)
							se_addnull(vm);
						else {
							varval_t *copy = vv;
							if (!VV_USE_REF(vv))
								copy = se_vv_copy(vm, vv);
							stack_push_vv(vm, copy);
						}
					}
					else if (VV_TYPE(vv_obj) == VAR_TYPE_VECTOR) {
						if (*str == 'x' || *str == 'y' || *str == 'z') {
							se_addfloat(vm, vv_obj->as.vec[*str - 'x']);
						}
						else {
							vm_printf("cannot get {%s} of vector!\n", str);
							se_addnull(vm);
						}
					}
					else
						se_addnull(vm);
			}
			//yep also need to free this e.g what if the compiler allowed new Object().objfield;
			se_vv_free(vm, vv_obj);
		} break;

		//copy of above, just for internal use atm
		case OP_LOAD_FIELD_VM: {

			int str_index = stack_pop(vm);
			varval_t *vv_obj = (varval_t*)stack_pop(vm);

			if (str_index >= vm->istringlistsize)
				vm_printf("str_index out of bounds!!\n");
			if (VV_TYPE(vv_obj) == VAR_TYPE_OBJECT && vv_obj->as.obj->type == VT_OBJECT_BUFFER)
			{
				vt_buffer_t *vtb = (vt_buffer_t*)vv_obj->as.obj->obj;// DYN_TYPE_HDR(vt_buffer_t, (char*)vv_obj->as.obj->obj);
				cstruct_t *cs = vm_get_struct(vm, vtb->type);
				if (cs)
				{
					const char *str = vm->istringlist[str_index].string;
					cstructfield_t *field = vm_get_struct_field(vm, cs, str);
					if (field)
					{
						varval_t *vv = vm_get_struct_field_value(vm, cs, field, vtb); //will always be a copy because no references it doesn't exist it needs to be made
						stack_push_vv(vm, vv);
					}
					else
						vm_printf("field not found for struct %d!\n", cs->name);
				}
				else
					vm_printf("cs is NULL! %d\n", vtb->type);
			}
			else {
				const char *str = vm->istringlist[str_index].string;
				//vm_printf("LOAD_FIELD{%s}\n", vm->istringlist[str_index].string);
				if (VV_TYPE(vv_obj) == VAR_TYPE_OBJECT)
				{
					//se_vv_get_field also uses stack_pop but that is up to the compiler to provide with expressions that dont store to add a OP_POP
					varval_t *vv = se_vv_get_field(vm, vv_obj, str_index);
					if (NULL == vv)
						se_addnull(vm);
					else {
						varval_t *copy = vv;
						if (!VV_USE_REF(vv))
							copy = se_vv_copy(vm, vv);
						stack_push_vv(vm, copy);
					}
				}
				else if (VV_TYPE(vv_obj) == VAR_TYPE_VECTOR) {
					if (*str == 'x' || *str == 'y' || *str == 'z') {
						se_addfloat(vm, vv_obj->as.vec[*str - 'x']);
					}
					else {
						vm_printf("cannot get {%s} of vector!\n", str);
						se_addnull(vm);
					}
				}
				else
					se_addnull(vm);
			}
			//yep also need to free this e.g what if the compiler allowed new Object().objfield;
			se_vv_free(vm, vv_obj);
		} break;

		case OP_STORE: {
			int loc = read_short(vm);
			//vm_printf("LOCAL STORE INDEX = %d\n", loc);
			varval_t *vv = (varval_t*)stack_pop(vm);
			varval_t *cur = (varval_t*)vm_stack[vm_registers[REG_BP] + loc];
			if (VV_USE_REF(vv))
				++vv->refs;
			//vm_printf("vvrefs=%d,currefs=%d\n", vv->refs, cur ? cur->refs : 0);
			se_vv_remove_reference(vm, cur);
			vm_stack[vm_registers[REG_BP] + loc] = (intptr_t)vv;
		} break;

		case OP_LOAD: {
			int loc = read_short(vm);
			//vm_printf("LOCAL LOAD INDEX = %d\n", loc);
			varval_t *vv = (varval_t*)vm_stack[vm_registers[REG_BP] + loc];
			if (vv == NULL) {
				//vm_printf("vv == NULL!\n");
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
			vm_printf("jumping to %d\n",val); \
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
		vm_scalar_t b = stack_pop_scalar(vm); \
		vm_scalar_t a = stack_pop_scalar(vm); \
		vm_registers[REG_COND] = (a MATH_OP b); \
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
			varval_t *b = stack_pop_vv(vm); 
			varval_t *a = stack_pop_vv(vm); 
			if (VV_IS_STRING(b) || VV_IS_STRING(a)) { 
				const char *sb = se_vv_to_string(vm, b); 
				const char *sa = se_vv_to_string(vm, a); 
				vm_registers[REG_COND] = !(strcmp(sb, sa));
			} else {
#if 0
				int fb = se_vv_to_int(vm, b); 
				int fa = se_vv_to_int(vm, a); 
				if (!(fb != fa)) 
					vm_registers[REG_COND] = 1;
#endif
				vm_registers[REG_COND] = vv_icmp(vm, a, b);
			} 
			se_vv_free(vm, b); 
			se_vv_free(vm, a); 
		} break;
		
		case OP_NEQ: { 
			varval_t *b = stack_pop_vv(vm); 
			varval_t *a = stack_pop_vv(vm); 
			if (VV_IS_STRING(b) || VV_IS_STRING(a)) { 
				const char *sb = se_vv_to_string(vm, b); 
				const char *sa = se_vv_to_string(vm, a); 
				vm_registers[REG_COND] = strcmp(sb, sa);
			} else {
#if 0
				int fb = se_vv_to_int(vm, b); 
				int fa = se_vv_to_int(vm, a); 
				if (!(fb == fa)) 
					vm_registers[REG_COND] = 1;
#endif
				vm_registers[REG_COND] = !vv_icmp(vm, a, b);
			} 
			se_vv_free(vm, b); 
			se_vv_free(vm, a); 
		} break;
		
		case OP_DEC: {
			vm_vector_t vec,result;
			if (!stack_pop_vector(vm, &vec)) //cannot cast to vector
				return E_VM_RET_ERROR;
			//vm_vector_sub(&vec, 1.f);
			vm_vector_math_op(vm, &vec, &_vconst1, &result, OP_SUB);
#if 0
			float a = stack_pop_float(vm);
			a -= 1.f;
			se_addfloat(vm, a);
#endif
			se_addnvector(vm, &result); //special case of vector not just generic xyz vector n indicates how many scalars
		} break;

		case OP_INC: {
			vm_vector_t vec,res;
			if (!stack_pop_vector(vm, &vec))
				return E_VM_RET_ERROR;
			//vm_vector_sub(&vec, 1.f);
			vm_vector_math_op(vm, &vec, &_vconst1, &res, OP_ADD);
			se_addnvector(vm, &res);
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

		case OP_DIV:
		case OP_SUB:
		case OP_MUL: {
			vm_vector_t vec,vec2,result;
			if (!stack_pop_vector(vm, &vec2))
				return E_VM_RET_ERROR;
			if (!stack_pop_vector(vm, &vec))
				return E_VM_RET_ERROR;
			//vm_vector_sub(&vec, 1.f);
			vm_vector_math_op(vm, &vec, &vec2, &result, instr);
			se_addnvector(vm, &result);
		} break;

		//SPECIFY_MATH_OP_SIMILAR(OP_SUB, -=)
		//SPECIFY_MATH_OP_SIMILAR(OP_MUL, *=)
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
					++vv->as.integer;
				else
					--vv->as.integer;
			}
			//still on stack no need to push/pop
		} break;

		/* did this whilst almost heading home from work so
			//TODO
			//marked unsafe
			//for now
		*/

		case OP_LOAD_REF: {
			int loc = read_short(vm);
			//vm_printf("LOCAL LOAD INDEX = %d\n", loc);
			varval_t *vv = (varval_t*)vm_stack[vm_registers[REG_BP] + loc];
			if (vv == NULL) {
				//vm_printf("vv == NULL!\n");
				se_addnull(vm);
			}
			else {
				varval_t *copy = vv;

				//if (!VV_USE_REF(vv))
				//copy = se_vv_copy(vm, vv);

				//stack_push(vm, vm->stack[vm->registers[REG_BP] + loc]);
				stack_push_vv(vm, copy);
			}
		} break;

		case OP_ADDRESS: {
			varval_t *vv = (varval_t*)stack_pop(vm);
			varval_t *nv = se_vv_create(vm, VV_TYPE(vv));
			nv->flags |= VF_POINTER;
			nv->as.ptr = vv;
			//++vv->refs;
			stack_push_vv(vm, nv);
			//se_vv_free(vm, vv);
			//let's assume it was loaded through OP_LOAD_REF so we don't have to free it
		} break;

		case OP_DEREFERENCE: {
			varval_t *ptr = (varval_t*)stack_pop(vm);
			if (!VV_IS_POINTER(ptr))
			{
				//vm_printf("not a pointer!\n");
				//return E_VM_RET_ERROR;
				//UNSAFE!!
				//just do a normal c pointer dereference
				varval_t *nv = se_vv_create(vm, VAR_TYPE_LONG); //should be VAR_TYPE_C_POINTER
				vm_long_t longval = vv_cast_long(vm, ptr);
				nv->as.intptr = *(intptr_t*)longval;
				stack_push_vv(vm, nv);
			}
			else
			{
				varval_t *vv = (varval_t*)ptr->as.ptr;
				stack_push_vv(vm, vv);
			}
#if 0
			varval_t *nv = se_vv_create(vm, VV_TYPE(ptr));
			nv->flags |= VF_POINTER;
			stack_push_vv(vm, nv);
#endif
			se_vv_free(vm, ptr);
		} break;

		case OP_CAST_STRUCT: {
			varval_t *vv = (varval_t*)stack_pop(vm);
			int s_ind = read_short(vm);

			cstruct_t *cs = vm_get_struct(vm, s_ind);
			if (cs == NULL)
			{
				se_addnull(vm);
				//failed to cast
			}
			else {

				varval_t *nvv = se_createobject(vm, VT_OBJECT_BUFFER, NULL, NULL, (void*)vt_buffer_deconstructor); //todo add the file deconstructor? :D
				vt_buffer_t *vtb = (vt_buffer_t*)malloc(sizeof(vt_buffer_t));
				vtb->size = cs->size;
				vtb->data = vv->as.ptr; //has to be a pointer that probably was returned by some c ffi func
				//vm_printf("spawning %s\n", cs->name);
				vtb->type = s_ind;
				nvv->as.obj->obj = vtb;
				stack_push_vv(vm, nvv);
			}
			//vm_printf("desired cast type = %s, current = %s\n", e_var_types_strings[cast_type], e_var_types_strings[VV_TYPE(vv)]);
			se_vv_free(vm, vv);
		} break;
		case OP_CAST: {
			uint16_t cast_type = read_short(vm);
			if (vm->cast_stack_ptr >= sizeof(vm->cast_stack) / sizeof(vm->cast_stack[0]))
			{
				vm_printf("fatal error cast_stack_ptr\n");
				getchar(); //shouldnt happen
				exit(0);
			}
			vm->cast_stack[vm->cast_stack_ptr++] = cast_type;
#if 0
			varval_t *vv = (varval_t*)stack_pop(vm);
			stack_push_vv(vm, vv_cast(vm, vv,cast_type));
			//vm_printf("desired cast type = %s, current = %s\n", e_var_types_strings[cast_type], e_var_types_strings[VV_TYPE(vv)]);
			se_vv_free(vm, vv);
#endif
		} break;
		case OP_ADD: {
			vm_vector_t vec, vec2, result;
			if (VV_IS_NUMBER(stack_current()) && VV_IS_NUMBER(stack_get(vm,1)))
			{
				unhandled:
#if 0
				int ib;
				int ia;

				if (VV_TYPE(a) == VAR_TYPE_INT && VV_TYPE(b) == VAR_TYPE_INT) {
					unhandled:
					ib = se_vv_to_int(vm, b);
					ia = se_vv_to_int(vm, a);
					ia += ib;
					se_addint(vm, ia);
				}
				else {
					float fb = (float)vv_cast_double(vm, b);
					float fa = (float)vv_cast_double(vm, a);
					fa += fb;
					se_addfloat(vm, fa);
				}
#endif
				if (!stack_pop_vector(vm, &vec2))
					return E_VM_RET_ERROR;
				if (!stack_pop_vector(vm, &vec))
					return E_VM_RET_ERROR;
				//vm_vector_sub(&vec, 1.f);
				vm_vector_math_op(vm, &vec, &vec2, &result, instr);
				se_addnvector(vm, &result);
			}
			else {
				varval_t *b = (varval_t*)stack_pop(vm);
				varval_t *a = (varval_t*)stack_pop(vm);
				if (
					(VV_TYPE(b) == VAR_TYPE_STRING || VV_TYPE(a) == VAR_TYPE_STRING)
					||
					(VV_TYPE(b) == VAR_TYPE_INDEXED_STRING || VV_TYPE(a) == VAR_TYPE_INDEXED_STRING)
					) {
					const char *sb = se_vv_to_string(vm, b);
					const char *sa = se_vv_to_string(vm, a);
					size_t newlen = strlen(sb) + strlen(sa) + 1;
					char *tmp = (char*)vm_mem_alloc(vm, newlen);
					snprintf(tmp, newlen, "%s%s", sa, sb);
					se_addstring(vm, tmp);
					vm_mem_free(vm, tmp);
				} //this could be done much cleaner but lazy
				else if (VV_TYPE(b) == VAR_TYPE_VECTOR || VV_TYPE(a) == VAR_TYPE_VECTOR) {

					float v[3] = { 0,0,0 };

					if (VV_TYPE(b) == VAR_TYPE_VECTOR) {
						v[0] += b->as.vec[0];
						v[1] += b->as.vec[1];
						v[2] += b->as.vec[2];
					}
					else {
						float fb = (float)vv_cast_double(vm, b);
						v[0] += fb;
						v[1] += fb;
						v[2] += fb;
					}

					if (VV_TYPE(a) == VAR_TYPE_VECTOR) {
						v[0] += a->as.vec[0];
						v[1] += a->as.vec[1];
						v[2] += a->as.vec[2];
					}
					else {
						float fa = (float)vv_cast_double(vm, a);
						v[0] += fa;
						v[1] += fa;
						v[2] += fa;
					}
					se_addvector(vm, v);
				}
				else
				{
					goto unhandled;
				}
				se_vv_free(vm, a);
				se_vv_free(vm, b);
			}
		} break;
#if 0
		case OP_DIV: {
			float b = stack_pop_float(vm);
			if (b == 0) {
				vm_printf("error divide by zero\n");
				b = 1;
			}
			float a = stack_pop_float(vm);
			a /= b;
			se_addfloat(vm, a);
		} break;
#endif
		case OP_PRINT: {
			int val = vm_stack[vm_registers[REG_BP] + 0];
			vm_printf("%d\n", val);
#if 0
			int stacktop = vm->stack[vm->registers[REG_SP]];
			vm_printf("PRINT: %d\n", stacktop);
			getchar();
#endif
		} break;

		case OP_JMP: {
			int jmp_loc = read_int(vm);
			vm_registers[REG_IP] = jmp_loc;
		} break;

		case OP_CALL_BUILTIN_METHOD: {

			int method_name_idx = read_int(vm);
			if (method_name_idx >= vm->istringlistsize)
			{
				vm_printf("internal method name error!\n");
				return E_VM_RET_ERROR;
			}
			int numargs = read_int(vm);
			varval_t *self = (varval_t*)stack_get(vm, numargs);
			//printf("self = %s, %d\n", VV_TYPE_STRING(self), self == vm->level);

			//vm_printf("func_name_idx=%d\n", func_name_idx);
			const char *method_name = vm->istringlist[method_name_idx].string;

			stockmethod_t *sm = se_find_method_by_name(vm, self->as.obj->type, method_name);

			if (method_name == NULL || sm == NULL) {
				vm_printf("built-in method '%s' does not exist! (%d) [%s, %d]\n", method_name, method_name_idx, VV_TYPE_STRING(self), self->as.obj->type);
				return E_VM_RET_ERROR;
			}
			vm->thrunner->numargs = numargs;
			int prev_bp = vm_registers[REG_BP];

			vm_registers[REG_BP] = vm_registers[REG_SP] - numargs + 1;

			//test all vars
#if 0
			for (int i = 0; i < numargs; ++i)
			{
				varval_t *vv = se_argv(vm, i);
				printf("vv %d = %s\n", i, VV_TYPE_STRING(vv));
			}
#endif

			//do the calling here
			varval_t *retval = NULL;
			if (sm->call(vm, self) != 0)
			{
				retval = (varval_t*)stack_pop(vm);
			}
			else {
				retval = NULL;
			}

			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm); //pop all local vars?
				//--vv->refs;
				se_vv_free(vm, vv);
			}
			//_self is the same as self, but now we're finally just popping it off the stack
			varval_t *_self = (varval_t*)stack_pop(vm); //pop self aswell
			vm_registers[REG_BP] = prev_bp;
			stack_push_vv(vm, retval);
			se_vv_free(vm, _self);
		} break;
#if 0
		case OP_CALL_BUILTIN_METHOD: {
			int method_name_idx = read_int(vm);
			if (method_name_idx >= vm->istringlistsize)
				method_name_idx = 0; //meh should be NULL or so now it calls smth random from first
			const char *method_name = vm->istringlist[method_name_idx].string;
			//vm_printf("func_name=%s\n", func_name);
			int numargs = read_int(vm);
			varval_t *self = (varval_t*)stack_get(vm,numargs);
#if 0
			if (self == vm->level)
				vm_printf("self is LEVEL\n");
			vm_printf("self type = %s\n", VV_TYPE_STRING(self));
#endif
			if (VV_TYPE(self) != VAR_TYPE_OBJECT)
			{
				vm_printf("not an object! type is %s\n", VV_TYPE_STRING(self));
				return E_VM_RET_ERROR;
			}
			stockmethod_t *sm = se_find_method_by_name(vm, self->as.obj->type, method_name);

			if (method_name == NULL || sm == NULL) {
				vm_printf("built-in method '%s' does not exist! (%d)\n", method_name, method_name_idx); //methods don't have ffi mhm?
				return E_VM_RET_ERROR;
			}
			vm->thrunner->numargs = numargs;
			int prev_bp = vm_registers[REG_BP];
			//not rlly need for pushing to stack cuz it's a internal func lol and i don't think u'll call another func from there in the script again
			//Would be terrible

#if 0
			for (int i = 0; i < numargs; i++) {
				int val = vm->stack[vm->registers[REG_SP] - i];
				vm_printf("loc=%d, val=%d\n", vm->registers[REG_SP] - i, val);
			}
#endif
			vm_registers[REG_BP] = vm_registers[REG_SP] - numargs + 1;
			//vm_printf("REG_BP=%d\n", vm_registers[REG_BP]);

			//do the calling here
			varval_t *retval = NULL;
			
			if (sm->call(vm, self) != 0) {
				retval = (varval_t*)stack_pop(vm);
			}
			else {
				retval = NULL;
			}

			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm); //pop all local vars?
				//--vv->refs;
				se_vv_free(vm, vv);
			}
			se_vv_free(vm, self);

			vm_registers[REG_BP] = prev_bp;

			stack_push_vv(vm, retval);
		} break;
#endif

		case OP_CALL_BUILTIN: {

			int func_name_idx = read_int(vm);
			if (func_name_idx >= vm->istringlistsize)
				func_name_idx = 0; //meh should be NULL or so now it calls smth random from first
			//vm_printf("func_name_idx=%d\n", func_name_idx);
			const char *func_name = vm->istringlist[func_name_idx].string;
			const char *lookupname = func_name;
			bool is_ffi_call = false;

			if (*func_name == '$')
			{
				is_ffi_call = true;
				lookupname = &func_name[1];
			}
			//vm_printf("func_name=%s\n", func_name);
			stockfunction_t *sf;
			if (!is_ffi_call)
			{
				sf = se_find_function_by_name(vm, func_name);

				if (func_name == NULL || sf == NULL) {
					//vm_printf("built-in function '%s' does not exist! (%d)\n", func_name, func_name_idx);
					//return E_VM_RET_ERROR;
					is_ffi_call = true;
					//fallback to ffi calls
				}
			}
			int numargs = read_int(vm);
			vm->thrunner->numargs = numargs;
			int prev_bp = vm_registers[REG_BP];
			//not rlly need for pushing to stack cuz it's a internal func lol and i don't think u'll call another func from there in the script again
			//Would be terrible
#if 0
			for (int i = 0; i < numargs; i++) {
				int val=vm->stack[vm->registers[REG_SP] - i];
				vm_printf("loc=%d, val=%d\n", vm->registers[REG_SP] - i, val);
			}
#endif
			vm_registers[REG_BP] = vm_registers[REG_SP] - numargs + 1;
			//vm_printf("REG_BP=%d\n", vm_registers[REG_BP]);

			//do the calling here
			varval_t *retval = NULL;
			if (is_ffi_call)
			{
				//if (vm->thrunner->ffi_libname[0] == '\0')
					//sprintf(vm->thrunner->ffi_libname, "msvcrt.dll");
				vm_ffi_lib_t *which = NULL;
				vm_ffi_lib_func_t *libfunc = vm_library_function_get_any(vm, lookupname, &which);
				//if this is too slow, could just make a array lookup table with indices of strings of the functions customized istring of some sorts and just
				//directly access the array then but for now let's do this
				int vm_do_ffi(vm_t *vm, vm_ffi_lib_func_t*);
				if (libfunc == NULL || vm_do_ffi(vm, libfunc) != 0)
				{
					vm_printf("ffi function '%s' not found!\n", lookupname);
					retval = NULL;
					return E_VM_RET_ERROR;
				} else
					retval = (varval_t*)stack_pop(vm);
			}
			else {
				if (sf->call(vm) != 0) {
					retval = (varval_t*)stack_pop(vm);
				}
				else {
					retval = NULL;
				}
			}

			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm); //pop all local vars?
				//--vv->refs;
				se_vv_free(vm, vv);
			}

			vm_registers[REG_BP] = prev_bp;

			stack_push_vv(vm, retval);
		} break;

		case OP_CALL_METHOD_THREAD: {
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define START_PERF(x)
#define END_PERF(x)
			//#define START_PERF(x) unsigned long long x = GetTickCount64();
			//#define END_PERF(x) vm_printf("END_PERF %llu for block %s (%d, %s)\n", GetTickCount64() - x, TOSTRING(x), __LINE__, __FILE__);
			START_PERF(start)

			int jmp_loc = stack_pop_int(vm);
			int numargs = stack_pop_int(vm);

			START_PERF(args);

			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				if (VV_USE_REF(vv))
					vv->refs++;
				vm->tmpstack[i] = (intptr_t)vv;
			}
			END_PERF(args);
			varval_t *new_self = stack_pop_vv(vm);
			if (VV_USE_REF(new_self))
				new_self->refs++;

			/* create new thread */

			START_PERF(find_thread);
			vm_thread_t *thr = NULL;
			for (int i = MAX_SCRIPT_THREADS; i--;) {
				if (!vm->threadrunners[i].active) {
					thr = &vm->threadrunners[i];
					break;
				}

			}
			END_PERF(find_thread);

			if (thr == NULL) {
				vm_printf("MAX SCRIPT THREADS\n");
				return E_VM_RET_ERROR;
			}
			else {
				//vm_printf("num threads = %d\n", vm->numthreadrunners);
			}
			++vm->numthreadrunners;
			//don't clear the stack/and stacksize
			memset(&thr->registers, 0, sizeof(thr->registers));
			//prob just bottleneck anyway \/
			//memset(thr->stack,0,thr->stacksize * sizeof(intptr_t)); //stack full empty ayy
			thr->wait = 0;
			thr->numargs = 0;
			thr->active = true;
			thr->instr = vm->instr;

			//add new thread to list of threads for next frame if wait occurs etc

			vm_thread_t *saverunner = vm->thrunner; //save current runner

			vm->thrunner = thr;

			//do all the call mimic stuff and sadly has to be same ish as normal call cuz the RET expects this and cba to change it

			int curpos = vm_registers[REG_IP];
			stack_push(vm, curpos);
			//vm_printf("call jmp to %d, returning to %d\n", jmp_loc, curpos);

			stack_push(vm, vm_registers[REG_BP]); //save the previous stack frame bp
			vm_registers[REG_BP] = vm_registers[REG_SP] + 1;

			memset(&vm_stack[vm_registers[REG_BP]], 0, sizeof(intptr_t) * MAX_LOCAL_VARS);

			for (int i = numargs; i--;)
				stack_push(vm, vm->tmpstack[numargs - i - 1]);

			//alloc minimum of MAX_LOCAL_VARS values on stack for locals?
			vm_registers[REG_SP] += MAX_LOCAL_VARS - numargs;
			stack_push(vm, numargs);
			stack_push(vm, NULL); //self is NULL in this case
			vm->thrunner->self = new_self;

			vm_registers[REG_IP] = jmp_loc;
			/* end of normal call stuff */

			START_PERF(run_thread);
			while (vm->is_running && vm_registers[REG_IP] != 0) {
				int thr_instr = vm->instr[vm_registers[REG_IP]++];

				int vm_ret = vm_execute(vm, thr_instr);
				if (vm_ret == E_VM_RET_ERROR)
					return vm_ret;
				else if (vm_ret == E_VM_RET_WAIT) {
					break;
				}
			}
			END_PERF(run_thread);

			if (!vm_thread_is_stalled(vm, thr)) {
				vm_execute(vm, OP_POP); //retval
				thr->active = false;
				--vm->numthreadrunners;
				vm_thread_reset_events(vm, thr); //should kind of already have no events, otherwise we wouldn't end up here mhm?
			}

			END_PERF(start)

			vm->thrunner = saverunner; //restore prev thread
		} break;

		case OP_CALL_THREAD: {
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define START_PERF(x)
#define END_PERF(x)
//#define START_PERF(x) unsigned long long x = GetTickCount64();
//#define END_PERF(x) vm_printf("END_PERF %llu for block %s (%d, %s)\n", GetTickCount64() - x, TOSTRING(x), __LINE__, __FILE__);
			START_PERF(start)

			int jmp_loc = stack_pop_int(vm);
			int numargs = stack_pop_int(vm);
			
			START_PERF(args);

			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				if (VV_USE_REF(vv))
					vv->refs++;
				vm->tmpstack[i] = (intptr_t)vv;
			}
			END_PERF(args);

			/* create new thread */

			START_PERF(find_thread);
			vm_thread_t *thr = NULL;
			for (int i = MAX_SCRIPT_THREADS; i--;) {
				if(!vm->threadrunners[i].active) {
					thr=&vm->threadrunners[i];
					break;
				}
				
			}
			END_PERF(find_thread);
			
			if(thr==NULL) {
				vm_printf("MAX SCRIPT THREADS\n");
				return E_VM_RET_ERROR;
			}
			else {
				//vm_printf("num threads = %d\n", vm->numthreadrunners);
			}
			++vm->numthreadrunners;
			//don't clear the stack/and stacksize
			memset(&thr->registers, 0, sizeof(thr->registers));
			//prob just bottleneck anyway \/
			//memset(thr->stack,0,thr->stacksize * sizeof(intptr_t)); //stack full empty ayy
			thr->wait=0;
			thr->numargs=0;
			thr->active = true;
			thr->instr = vm->instr;
			
			//add new thread to list of threads for next frame if wait occurs etc
			
			vm_thread_t *saverunner = vm->thrunner; //save current runner
			
			vm->thrunner = thr;
		
			//do all the call mimic stuff and sadly has to be same ish as normal call cuz the RET expects this and cba to change it
			
			int curpos = vm_registers[REG_IP];
			stack_push(vm, curpos);
			//vm_printf("call jmp to %d, returning to %d\n", jmp_loc, curpos);

			stack_push(vm, vm_registers[REG_BP]); //save the previous stack frame bp
			vm_registers[REG_BP] = vm_registers[REG_SP] + 1;

			memset(&vm_stack[vm_registers[REG_BP]], 0, sizeof(intptr_t) * MAX_LOCAL_VARS);

			for (int i = numargs; i--;)
				stack_push(vm, vm->tmpstack[numargs-i-1]);

			//alloc minimum of MAX_LOCAL_VARS values on stack for locals?
			vm_registers[REG_SP] += MAX_LOCAL_VARS - numargs;
			stack_push(vm, numargs);
			if (saverunner) //when calling main from _init or something thrunner may not exist yet
			{
				if (VV_USE_REF(saverunner->self))
					saverunner->self->refs++;
				stack_push(vm, saverunner->self);
			}
			else
				stack_push(vm, NULL); //self is NULL in this case

			vm_registers[REG_IP] = jmp_loc;
			/* end of normal call stuff */

			START_PERF(run_thread);
			while (vm->is_running && vm_registers[REG_IP] != 0) {
				int thr_instr = vm->instr[vm_registers[REG_IP]++];

				int vm_ret = vm_execute(vm, thr_instr);
				if (vm_ret == E_VM_RET_ERROR)
					return vm_ret;
				else if (vm_ret == E_VM_RET_WAIT) {
					break;
				}
			}
			END_PERF(run_thread);

			if (!vm_thread_is_stalled(vm,thr)) {
				vm_execute(vm, OP_POP); //retval
				thr->active = false;
				--vm->numthreadrunners;
				vm_thread_reset_events(vm, thr); //should kind of already have no events, otherwise we wouldn't end up here mhm?
			}

			END_PERF(start)

			vm->thrunner=saverunner; //restore prev thread
		} break;

		case OP_ENDON_EVENT_STRING:
		{
			//note if you reload e.g player script objects and don't reload the vm fully, then you may end up deadlocking that thread/coroutine..
			varval_t *object = (varval_t*)stack_pop(vm);
			if (VV_TYPE(object) != VAR_TYPE_OBJECT)
			{
				vm_printf("not an object %s", VV_TYPE_STRING(object));
				se_vv_free(vm, object);
				return E_VM_RET_ERROR;
			}
#if 0
			if (se_vv_is_freeable(vm, object))
			{
				vm_printf("object is freed");
				se_vv_free(vm, object);
				return E_VM_RET_ERROR;
			}
#endif
			if (VV_USE_REF(object))
				++object->refs;
			int event_string_index = read_int(vm);
			if (event_string_index >= vm->istringlistsize)
			{
				vm_printf("ev > max\n");
				return E_VM_RET_ERROR;
			}
			const char *event_string = vm->istringlist[event_string_index].string;
			unsigned int start = vm->thrunner->eventstring % VM_MAX_EVENTS;

			for (unsigned int i = 0; i < VM_MAX_EVENTS; ++i)
			{
				unsigned int offset = (start + i) % VM_MAX_EVENTS;
				if (offset == start && vm->thrunner->numeventstrings > 0) break; //can't use current event string
				vm_event_string_t *ev = &vm->thrunner->eventstrings[offset];
				if (!ev->inuse)
				{
					ev->string = event_string_index;
					ev->type = 1;
					ev->object = object;
					ev->inuse = true;
					break;
				}
			}
		} break;

		case OP_NOTIFY_EVENT_STRING:
		{
			varval_t *object = (varval_t*)stack_pop(vm);
			if (VV_TYPE(object) != VAR_TYPE_OBJECT)
			{
				vm_printf("not an object %s", VV_TYPE_STRING(object));
				se_vv_free(vm, object);
				return E_VM_RET_ERROR;
			}
#if 0
			if (se_vv_is_freeable(vm, object))
			{
				vm_printf("object is freed");
				se_vv_free(vm, object);
				return E_VM_RET_ERROR;
			}
#endif
			if (VV_USE_REF(object))
				++object->refs;
			int numargs = read_int(vm);
			int event_string_index = read_int(vm);
			if (event_string_index >= vm->istringlistsize)
			{
				vm_printf("ev > max\n");
				return E_VM_RET_ERROR;
			}
			vm_event_t n;
			n.name = event_string_index;
			n.object = object;
			n.numargs = numargs;
			memset(&n.arguments, 0, sizeof(n.arguments));
			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				if (VV_USE_REF(vv))
					vv->refs++;
				n.arguments[i] = vv;
			}
			array_push(&vm->events, &n);
		} break;

		case OP_WAIT_EVENT_STRING:
		{
			//note if you reload e.g player script objects and don't reload the vm fully, then you may end up deadlocking that thread/coroutine..
			varval_t *object = (varval_t*)stack_pop(vm);
			if (VV_TYPE(object) != VAR_TYPE_OBJECT)
			{
				vm_printf("not an object %s", VV_TYPE_STRING(object));
				se_vv_free(vm, object);
				return E_VM_RET_ERROR;
			}
#if 0
			if (se_vv_is_freeable(vm, object))
			{
				vm_printf("object is freed");
				se_vv_free(vm, object);
				return E_VM_RET_ERROR;
			}
#endif
			if (VV_USE_REF(object))
				++object->refs;
			int loc = read_int(vm);
			int event_string_index = read_int(vm);
			if (event_string_index >= vm->istringlistsize)
			{
				vm_printf("ev > max\n");
				return E_VM_RET_ERROR;
			}
			int numargs = read_int(vm);
			const char *event_string = vm->istringlist[event_string_index].string;
			unsigned int start = vm->thrunner->eventstring % VM_MAX_EVENTS;

			for (unsigned int i = 0; i < VM_MAX_EVENTS; ++i)
			{
				unsigned int offset = (start + i) % VM_MAX_EVENTS;
				if (offset == start && vm->thrunner->numeventstrings > 0) break; //can't use current event string
				vm_event_string_t *ev = &vm->thrunner->eventstrings[offset];
				if (!ev->inuse)
				{
					ev->string = event_string_index;
					ev->stackoffset = loc;
					ev->object = object;
					ev->inuse = true;
					ev->type = 0;
					ev->numargs = numargs;
					++vm->thrunner->numeventstrings;
					break;
				}
			}
			return E_VM_RET_WAIT;
		} break;

		case OP_WAIT: {
			float a = stack_pop_float(vm) * 1000.f;
			if (a == 0) {
				vm_printf("invalid wait amount zero!\n");
				a = 1;
			}

			int b = (int)a;
			vm->thrunner->wait += b;

			return E_VM_RET_WAIT;
		} break;

		case OP_CALL_METHOD: {
			int jmp_loc = read_int(vm);
			int numargs = read_int(vm);

			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				if (VV_USE_REF(vv))
					vv->refs++;
				vm->tmpstack[i] = (intptr_t)vv;
			}

			varval_t *self = (varval_t*)stack_pop(vm);
			if (VV_USE_REF(self))
				self->refs++;

			int curpos = vm_registers[REG_IP];
			stack_push(vm, curpos);
			//vm_printf("call jmp to %d, returning to %d\n", jmp_loc, curpos);

			stack_push(vm, vm_registers[REG_BP]); //save the previous stack frame bp
			vm_registers[REG_BP] = vm_registers[REG_SP] + 1;

			memset(&vm_stack[vm_registers[REG_BP]], 0, sizeof(intptr_t) * MAX_LOCAL_VARS);

			for (int i = numargs; i--;)
				stack_push(vm, vm->tmpstack[numargs - i - 1]);

			//alloc minimum of MAX_LOCAL_VARS values on stack for locals?
			vm_registers[REG_SP] += MAX_LOCAL_VARS - numargs;
			stack_push(vm, numargs);
			//as of now there should always be a threadrunner anyways so
			stack_push(vm, vm->thrunner->self); //push previous self so we can restore it
			vm->thrunner->self = self; //set new self to the threadrunner so we can access it easily
			vm_registers[REG_IP] = jmp_loc;
		} break;

		case OP_CALL_FUNCTION_POINTER:
		{
			//int jmp_loc = read_int(vm);
			int numargs = read_int(vm);

			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				if (VV_USE_REF(vv))
					vv->refs++;
				vm->tmpstack[i] = (intptr_t)vv;
			}
			int jmp_loc = stack_pop_int(vm);

			int curpos = vm_registers[REG_IP];
			stack_push(vm, curpos);
			//vm_printf("call jmp to %d, returning to %d\n", jmp_loc, curpos);

			stack_push(vm, vm_registers[REG_BP]); //save the previous stack frame bp
			vm_registers[REG_BP] = vm_registers[REG_SP] + 1;

			memset(&vm_stack[vm_registers[REG_BP]], 0, sizeof(intptr_t) * MAX_LOCAL_VARS);

			for (int i = numargs; i--;)
				stack_push(vm, vm->tmpstack[numargs - i - 1]);

			//alloc minimum of MAX_LOCAL_VARS values on stack for locals?
			vm_registers[REG_SP] += MAX_LOCAL_VARS - numargs;
			stack_push(vm, numargs);
			if (vm->thrunner) //when calling main from _init or something thrunner may not exist yet
			{
				if (VV_USE_REF(vm->thrunner->self))
					vm->thrunner->self->refs++;
				stack_push(vm, vm->thrunner->self);
			}
			else
				stack_push(vm, NULL); //self is NULL in this case

			vm_registers[REG_IP] = jmp_loc;
		} break;

		case OP_CALL: {
			int jmp_loc = read_int(vm);
			int numargs = read_int(vm);

			for (int i = numargs; i--;) {
				varval_t *vv = (varval_t*)stack_pop(vm);
				if (VV_USE_REF(vv))
					vv->refs++;
				vm->tmpstack[i] = (intptr_t)vv;
			}

			int curpos = vm_registers[REG_IP];
			stack_push(vm, curpos);
			//vm_printf("call jmp to %d, returning to %d\n", jmp_loc, curpos);

			stack_push(vm, vm_registers[REG_BP]); //save the previous stack frame bp
			vm_registers[REG_BP] = vm_registers[REG_SP] + 1;

			memset(&vm_stack[vm_registers[REG_BP]], 0, sizeof(intptr_t) * MAX_LOCAL_VARS);

			for (int i = numargs; i--;)
				stack_push(vm, vm->tmpstack[numargs-i-1]);

			//alloc minimum of MAX_LOCAL_VARS values on stack for locals?
			vm_registers[REG_SP] += MAX_LOCAL_VARS - numargs;
			stack_push(vm, numargs);
			if (vm->thrunner) //when calling main from _init or something thrunner may not exist yet
			{
				if (VV_USE_REF(vm->thrunner->self))
					vm->thrunner->self->refs++;
				stack_push(vm, vm->thrunner->self);
			}
			else
				stack_push(vm, NULL); //self is NULL in this case

			vm_registers[REG_IP] = jmp_loc;
		} break;

		case OP_RET: {
			varval_t *retval = stack_pop_vv(vm);
			varval_t *oldself = stack_pop_vv(vm);
			if (vm->thrunner)
			{
				se_vv_remove_reference(vm, vm->thrunner->self);
				vm->thrunner->self = oldself;
			}
			int numargs = stack_pop(vm);

			for (int i = MAX_LOCAL_VARS; i--;) {
				varval_t *vv=(varval_t*)stack_pop(vm); //pop all local vars?
				if (vv != NULL) {
					//se_vv_remove_reference(vm, vv);
					//can't call because the gc stuff at end rofl
#if 0
					--vv->refs;
					if (vv->refs > 0) {
						//vm_printf("CANNOT FREE VV BECAUSE REF = %d\n", vv->refs);
						continue;
					}
					if (vv == retval) continue; //dont free the return value rofl
					se_vv_free(vm, vv);
#endif
					if (vv == retval)
					{
						--vv->refs;
						//vm_printf("retval = %02X\n", retval);
						continue;
					}
					se_vv_remove_reference(vm, vv);
				}
			}

			int bp = stack_pop(vm); //set old stack frame bp again
			vm_registers[REG_BP] = bp;

			int ret_addr = stack_pop(vm);
			//vm_printf("ret_addr=%d\n", ret_addr);
			vm_registers[REG_IP] = ret_addr;

			stack_push_vv(vm, retval);
			//clear all threadrunner strings
			if (vm->thrunner == NULL)
			{
				vm_printf("fatal error thrunner NULL\n");
				getchar(); //shouldnt happen
				exit(0);
			}
			else
			{
#if 1
				for (int i = vector_count(&vm->thrunner->strings); i--;)
				{
					const char *dstr = (const char*)vector_get(&vm->thrunner->strings, i);
					vm_mem_free(vm, dstr);
				}
				vector_free(&vm->thrunner->strings);
#endif
			}
		} break;

		default: {
			vm_printf("%s not supported!\n", e_opcodes_strings[instr]);
			se_addnull(vm);
		} break;
	}

	return 0;
}

bool vm_thread_is_stalled(vm_t *vm, vm_thread_t *thr)
{
	return thr->wait > 0 || thr->numeventstrings > 0;
}

void vm_thread_reset_events(vm_t *vm, vm_thread_t *thr)
{
	thr->numeventstrings = 0;
	thr->eventstring = 0;
	for (int ii = 0; ii < VM_MAX_EVENTS; ++ii)
	{
		if (thr->eventstrings[ii].inuse)
			se_vv_remove_reference(vm, thr->eventstrings[ii].object);
		thr->eventstrings[ii].inuse = false;
	}
}

int vm_run_active_threads(vm_t *vm, int frametime) {
	vm_thread_t *thr = NULL;
	int err = 0;

	for (int evi = vm->events.size; evi--;)
	{
		array_get(&vm->events, vm_event_t, ev, evi);
		//prob should swap the loops around 1024 once and potential 32 each instead of 32 potential and 1024 guaranteed each
		for (int i = MAX_SCRIPT_THREADS; i--;) {
			thr = &vm->threadrunners[i];

			if (!thr->active)
				continue;
			if (thr->numeventstrings == 0)
				continue;
			for (unsigned int threvi = 0; threvi < VM_MAX_EVENTS; ++threvi)
			{
				vm_event_string_t *threadev = &thr->eventstrings[threvi];
				if (!threadev->inuse)
					continue;
				if (threadev->object != ev->object)
				{
					if(threadev->object->as.obj->obj != ev->object->as.obj->obj)
						continue;
				}
				if (threadev->string != ev->name)
					continue;

				vm_thread_t *tmp = vm->thrunner;
				vm->thrunner = thr;
				threadev->inuse = false;
				se_vv_remove_reference(vm, threadev->object);
				if (threadev->type == 1)
				{
					vm_execute(vm, OP_POP); //retval
					thr->active = false;
					--vm->numthreadrunners;
					vm_thread_reset_events(vm, thr);
					break;
				}
				else
				{
					int loc = threadev->stackoffset;
					unsigned int maxargs = ev->numargs;
					if (ev->numargs > threadev->numargs)
						maxargs = threadev->numargs;
					for (unsigned s_i = 0; s_i < maxargs; ++s_i)
					{
						varval_t *vv = ev->arguments[s_i];
						varval_t *cur = (varval_t*)vm_stack[vm_registers[REG_BP] + loc + s_i];
						if (VV_USE_REF(vv))
							++vv->refs;
						else
							vv = se_vv_copy(vm, vv);
						//vm_printf("vvrefs=%d,currefs=%d\n", vv->refs, cur ? cur->refs : 0);
						se_vv_remove_reference(vm, cur);
						vm_stack[vm_registers[REG_BP] + loc + s_i] = (intptr_t)vv;
					}
					--thr->numeventstrings;
					++thr->eventstring; //move to next event
				}
				vm->thrunner = tmp;
			}
		}
		se_vv_remove_reference(vm, ev->object);
		for (int argi = 0; argi < ev->numargs; ++argi)
		{
			se_vv_remove_reference(vm, ev->arguments[argi]);
		}
	}

	array_free(&vm->events);
	array_init(&vm->events, vm_event_t);

	for (int i = MAX_SCRIPT_THREADS; i--;) {
		thr=&vm->threadrunners[i];
		
		if(!thr->active)
			continue;
	
		//vm_printf("thr->wait (%d) -= %d\n", thr->wait, frametime);
		if (thr->wait > 0) {
			thr->wait -= frametime;
			continue;
		}
		if (thr->numeventstrings > 0)
		{
			vm_event_string_t *ev = &thr->eventstrings[thr->eventstring];
			//printf("waiting for %s\n", vm->istringlist[ev->string].string);
			continue;
		}
		{
			vm->instr = thr->instr;
			vm->thrunner = thr;
			while (vm->is_running && vm_registers[REG_IP] != 0) {
				int instr = vm->instr[vm->thrunner->registers[REG_IP]++];
				if ((err = vm_execute(vm, instr)) != E_VM_RET_NONE) {
					if (err == E_VM_RET_WAIT)
						break;
					else
						return err;
				}
			}
		}
		if (!vm_thread_is_stalled(vm, thr)) {
			vm_execute(vm, OP_POP); //retval
			thr->active=false; //enough to clear it rofl
			--vm->numthreadrunners;
		}
	}
	vm->thrunner = NULL;
	return E_VM_RET_NONE;
}

VM_INLINE int vm_exec_thread_pointer(vm_t *vm, int fp, int numargs) {
	vm->thrunner = NULL;

	se_addint(vm, numargs);
	se_addint(vm, fp);
	return vm_execute(vm, OP_CALL_THREAD);
}

int vm_exec_ent_thread_pointer(vm_t *vm, varval_t *new_self, int fp, int numargs)
{
	vm->thrunner = NULL;
	//move every argument up one place, then place new_self at the front
	//instead of doing this, could've just doine REG_BP inside vm->stack with for(varval_t *cur next ptr setting
	for (int i = numargs; i--;) {
		varval_t *vv = (varval_t*)stack_pop(vm);
		//no need for refs, just moving ptrs
		vm->tmpstack[i] = (intptr_t)vv;
	}
	se_addobject(vm, new_self); //push self (at the begining)

	for (int i = numargs; i--;)
		stack_push(vm, vm->tmpstack[numargs - i - 1]);
	//push all the arguments back on the stack for the correct order we expect

	se_addint(vm, numargs);
	se_addint(vm, fp);
	return vm_execute(vm, OP_CALL_METHOD_THREAD);
}

int vm_notify(vm_t *vm, varval_t *object, int stringindex, size_t numargs)
{
	if (VV_TYPE(object) != VAR_TYPE_OBJECT)
	{
		vm_printf(vm, "vm_notify can only be used on types of objects\n");
		return 1;
	}
	if (se_vv_is_freeable(vm, object))
	{
		//vm_printf("object would be freed");
		//return 1;
	}
	if (VV_USE_REF(object))
		++object->refs;

	vm_event_t n;
	n.name = stringindex;
	n.object = object;
	n.numargs = numargs;
	memset(&n.arguments, 0, sizeof(n.arguments));
	for (int i = numargs; i--;) {
		varval_t *vv = (varval_t*)stack_pop(vm);
		if (VV_USE_REF(vv))
			vv->refs++;
		n.arguments[i] = vv;
	}
	array_push(&vm->events, &n);
	return 0;
}

int vm_notify_string(vm_t *vm, varval_t *vv, const char *str, size_t numargs)
{
	unsigned long strhash = hash_string(str);
	for (unsigned int i = 0; i < vm->istringlistsize; i++) {
		vt_istring_t *istr = &vm->istringlist[i];
		if (!istr->string)
			continue;
		unsigned long hash = hash_string(istr->string);
		if (hash == strhash && !strcmp(str, istr->string))
			return vm_notify(vm, vv, i, numargs);
	}
	//unable to find string
	return 1;
}

int vm_exec_thread(vm_t *vm, const char *func_name, int numargs) {
	if (!vm)
		return 1;
	int status = 0;

	vm_function_info_t *fi = NULL;
	int funcinfocount = vector_count(&vm->functioninfo);
	for (int i = funcinfocount; i--;) {
		vm_function_info_t *ffi = (vm_function_info_t*)vector_get(&vm->functioninfo, i);
		if (!strcmp(ffi->name, func_name)) {
			fi = ffi;
			vm_use_program(vm, fi->program);
			status = vm_exec_thread_pointer(vm, fi->position, numargs);
			if (status) break;
		}
	}

	if (fi == NULL)
		return E_VM_RET_FUNCTION_NOT_FOUND;
	return status;
}

static vm_ffi_callback_t g_ffi_callbacks[VM_MAX_FFI_CALLBACKS] = { 0 };

size_t mem_get_page_size()
{
	size_t page_size = 0;
#ifdef _WIN32
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	page_size = system_info.dwPageSize;
#else
	page_size = getpagesize();
#endif
	return page_size;
}

static char* mem_page_alloc(/*size_t n*/)
{
	size_t page_size = mem_get_page_size();
#ifdef _WIN32
	char *buf = VirtualAlloc(0, page_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
	char *buf = (char*)mmap(NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
	memset(buf, 0x90, page_size);
	return buf;
}

static void mem_page_free(void *buf)
{
#ifdef _WIN32
	VirtualFree(buf, 0, MEM_RELEASE);
#else
	munmap(buf, mem_get_page_size());
#endif
}

static vm_thread_t *vm_set_active_thread(vm_t *vm, vm_thread_t *thread)
{
	vm_thread_t *old = vm->thrunner;
	vm->thrunner = thread;
	return old;
}

static vm_thread_t *vm_request_thread(vm_t *vm)
{
	vm_thread_t *thr = NULL;
	for (int i = MAX_SCRIPT_THREADS; i--;) {
		if (!vm->threadrunners[i].active) {
			thr = &vm->threadrunners[i];
			break;
		}

	}

	if (thr == NULL)
	{
		vm_printf("MAX SCRIPT THREADS\n");
		return E_VM_RET_ERROR;
	}
	else {
		//vm_printf("num threads = %d\n", vm->numthreadrunners);
	}
	++vm->numthreadrunners;
	//don't clear the stack/and stacksize
	memset(&thr->registers, 0, sizeof(thr->registers));
	//prob just bottleneck anyway \/
	//memset(thr->stack,0,thr->stacksize * sizeof(intptr_t)); //stack full empty ayy
	thr->wait = 0;
	thr->numargs = 0;
	thr->active = true;
	thr->instr = vm->instr;
	return thr;
}

//expecting a function call of any sort before this, e.g method call, normal func call, function ptr call that's why we pop at end
static int vm_call_function_pointer_thread(vm_t *vm, vm_thread_t *thr, int jmp_loc, int numargs)
{
	vm_thread_t *saverunner = vm->thrunner; //save current runner

	vm->thrunner = thr;

	//do all the call mimic stuff and sadly has to be same ish as normal call cuz the RET expects this and cba to change it

	int curpos = vm_registers[REG_IP];
	stack_push(vm, curpos);
	//vm_printf("call jmp to %d, returning to %d\n", jmp_loc, curpos);

	stack_push(vm, vm_registers[REG_BP]); //save the previous stack frame bp
	vm_registers[REG_BP] = vm_registers[REG_SP] + 1;

	memset(&vm_stack[vm_registers[REG_BP]], 0, sizeof(intptr_t) * MAX_LOCAL_VARS);

	for (int i = numargs; i--;)
		stack_push(vm, vm->tmpstack[numargs - i - 1]);

	//alloc minimum of MAX_LOCAL_VARS values on stack for locals?
	vm_registers[REG_SP] += MAX_LOCAL_VARS - numargs;
	stack_push(vm, numargs);
	if (saverunner) //when calling main from _init or something thrunner may not exist yet
	{
		if (VV_USE_REF(saverunner->self))
			saverunner->self->refs++;
		stack_push(vm, saverunner->self);
	}
	else
		stack_push(vm, NULL); //self is NULL in this case

	vm_registers[REG_IP] = jmp_loc;
	/* end of normal call stuff */

	START_PERF(run_thread);
	while (vm->is_running && vm_registers[REG_IP] != 0) {
		int thr_instr = vm->instr[vm_registers[REG_IP]++];

		int vm_ret = vm_execute(vm, thr_instr);
		if (vm_ret == E_VM_RET_ERROR)
			return vm_ret;
		else if (vm_ret == E_VM_RET_WAIT) {
			break;
		}
	}
	END_PERF(run_thread);

	if (!vm_thread_is_stalled(vm, thr)) {
		vm_execute(vm, OP_POP); //retval
		thr->active = false;
		--vm->numthreadrunners;
		vm_thread_reset_events(vm, thr); //should kind of already have no events, otherwise we wouldn't end up here mhm?
	}

	END_PERF(start)

	vm->thrunner = saverunner; //restore prev thread
}

static void _simple_ffi_callback(vm_function_t fp, vm_t *vm)
{
	//printf("_simple_ffi_callback(fp=%d,vm=%02X)\n", fp, vm);

	vm_thread_t *new_thr = vm_request_thread(vm);
	vm_call_function_pointer_thread(vm, new_thr, fp, 0);
}

static char *ffi_create_c_callback(vm_t *vm, vm_function_t fp)
{
	char *mem = mem_page_alloc();
	intptr_t *addr = (intptr_t*)(mem + 100); //should be fine for now, we don't have 100 opcodes here yet
	*addr = (intptr_t)_simple_ffi_callback;
	char *ptr = mem;
	//emit(&ptr, 0xcc); //__asm int 3
	
	push(&ptr, REG_EBP);
	mov(&ptr, REG_EBP, REG_ESP);

	push_imm(&ptr, vm);
	push_imm(&ptr, fp);

	emit(&ptr, 0xff);
	emit(&ptr, 0x15);
	dd(&ptr, addr);
#if 0
	emit(&ptr, 0x83);
	emit(&ptr, 0xc4);
	emit(&ptr, 8);
#endif
	emit(&ptr, 0xc9); //leave
	ret(&ptr, 0);
	return mem;
}

void vm_unregister_c_ffi_callbacks(vm_t *vm)
{
	for (int i = 0; i < VM_MAX_FFI_CALLBACKS; ++i)
	{
		if (!g_ffi_callbacks[i].inuse || g_ffi_callbacks[i].vm != vm) continue;
		mem_page_free(g_ffi_callbacks[i].cfunc);
		g_ffi_callbacks[i].inuse = false;
	}
}

bool vm_register_c_ffi_callback(vm_t *vm, char *cfunc)
{
	for (int i = 0; i < VM_MAX_FFI_CALLBACKS; ++i)
	{
		if (!g_ffi_callbacks[i].inuse)
		{
			vm_ffi_callback_t *cb = &g_ffi_callbacks[i];
			//is set in the cfunc (pushed as arg to the callback wrapper)
			//cb->callback = fp;
			cb->cfunc = cfunc;
			cb->inuse = true;
			cb->numargs = 0;
			cb->vm = vm;
			return true;
		}
	}
	return false;
}

typedef enum
{
	FFI_SUCCESS,
	FFI_GENERIC_ERROR,
	FFI_LIBRARY_NOT_FOUND,
	FFI_FUNCTION_NOT_FOUND,
} ffi_call_result_t;

int vm_do_ffi(vm_t *vm, vm_ffi_lib_func_t *lf)
{
	int status = FFI_GENERIC_ERROR;

#ifndef _WIN32
#define HMODULE void*
#define DWORD void*
#define GetProcAddress dlsym
#define FreeLibrary dlclose
#endif

#if 0

#ifdef _WIN32
	HMODULE lib = LoadLibraryA(libname);
#else
	void *lib = dlopen(libname, RTLD_LAZY);
#endif
	if (!lib)
		return FFI_LIBRARY_NOT_FOUND;
	DWORD addr = GetProcAddress(lib, funcname);
	if (!addr)
	{
		return FFI_FUNCTION_NOT_FOUND;
	}

#endif

	DWORD addr = lf->address;

	int funcsize = 2000;
	size_t page_size = 0;
#ifdef _WIN32
	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	page_size = system_info.dwPageSize;

	// prepare the memory in which the machine code will be put (it's not executable yet):
	char *buf = VirtualAlloc(0, page_size, MEM_COMMIT, PAGE_READWRITE);
#else
	page_size = getpagesize();
	char *buf = (char*)mmap(NULL, page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANON, -1, 0);
#endif
	char *jit = buf;
	jit += 32;

	//char *jit = malloc(2000);

	//vm_printf("jit loc = %02X\n", jit);
	//int j = 0;
	memset(buf, 0x90, page_size);

#if 1
	push(&jit, REG_EBP);
	mov(&jit, REG_EBP, REG_ESP);
#endif
	//sub_imm(&jit, REG_ESP, 0x12);
	//sub_imm(&jit, REG_ESP, 0x10);

	int sc = 0; //stack cleanup

	//jit[j++] = 0xcc; //int 3
	//emit(&jit, 0xcd); //int 3
	//emit(&jit, 0x3);

	int typecount[VAR_TYPE_NULL + 1] = { 0 };
	int tcp[VAR_TYPE_NULL + 1] = { 0 };

	int numargs = se_argc(vm);
	for (int i = 0; i < numargs; i++)
	{
		varval_t *arg = se_argv(vm, numargs - i - 1);
		if (!VV_IS_POINTER(arg))
			++typecount[VV_TYPE(arg)];
	}
	sub_imm(&jit, REG_ESP, typecount[VAR_TYPE_FLOAT] * sizeof(double));
	double *ds = (double*)vm_mem_alloc(vm, typecount[VAR_TYPE_FLOAT] * sizeof(double));

	for (int i = 0; i < numargs; i++)
	{
		varval_t *arg = se_argv(vm, numargs - i - 1);
		if (VV_TYPE(arg) == VAR_TYPE_NULL)
		{
			push_imm(&jit, NULL);
		}
		else if (VV_TYPE(arg) == VAR_TYPE_FLOAT)
		{
			//fld qword ptr offset
			emit(&jit, 0xdd);
			emit(&jit, 0x05);
			ds[tcp[VAR_TYPE_FLOAT]] = (double)arg->as.flt;
			//static double test = 3.14;
			//test = (double)arg->as.flt;
			dd(&jit, (intptr_t)&ds[tcp[VAR_TYPE_FLOAT]]);
			++tcp[VAR_TYPE_FLOAT];

			//lea esp, [esp - 8]
			emit(&jit, 0x8d);
			emit(&jit, 0x64);
			emit(&jit, 0x24);
			emit(&jit, 0xf8);

			//fstp qword ptr [esp]
			emit(&jit, 0xdd);
			emit(&jit, 0x1c);
			emit(&jit, 0x24);
			/*
			emit(&jit, 0xdd);
			emit(&jit, 0x5c);
			emit(&jit, 0x24);
			emit(&jit, 0xfc + (i+1) * sizeof(float)); //should be 4
			*/

			sc += sizeof(float); //should be 4
		}
		else
		{
			void *p = arg->as.ptr;
			if (VV_IS_POINTER(arg))
			{
				varval_t *val = (varval_t*)arg->as.ptr;
				push_imm(&jit, &val->as);
			}
			else if (VV_IS_STRING(arg))
				push_imm(&jit, se_vv_to_string(vm, arg));
			else
			{
				if (VV_TYPE(arg) == VAR_TYPE_OBJECT)
				{
					switch (arg->as.obj->type)
					{
					case VT_OBJECT_BUFFER:
					{
						vt_buffer_t *vtb = (vt_buffer_t*)arg->as.obj->obj;
						push_imm(&jit, vtb->data);
					} break;
					default: goto _ffi_end;
					}
				}
				else if (VV_TYPE(arg) == VAR_TYPE_FUNCTION_POINTER)
				{
					//register it in the global map
					char *cfunc = ffi_create_c_callback(vm, arg->as.integer);
					//printf("created cfunc %02X for %d\n", cfunc, arg->as.integer);
					vm_register_c_ffi_callback(vm, cfunc);
					push_imm(&jit, cfunc);
				}
				else
					push_imm(&jit, p);
			}
		}
	}
#if 1
	//special case of call API 
	//emit(&jit, 0x36);
	//segment override mhm
	emit(&jit, 0xff);
	emit(&jit, 0x15);
	dd(&jit, &addr);
#if 0
	//asm int 3
	emit(&jit, 0xcd);
	emit(&jit, 0x03);
#endif
#endif

	emit(&jit, 0x83);
	emit(&jit, 0xc4);
	emit(&jit, sc);
#if 0
	jit[j++] = 0x83;
	jit[j++] = 0xc4;
	jit[j++] = sc; //add esp, X
#endif

	//xor(&jit, EAX, EAX);
#if 1
	emit(&jit, 0xc9); //leave
#endif
	ret(&jit, 0);
	//JIT_EMIT(X86_LEAVE);
	//JIT_EMIT(X86_RET);

	//j = jit - buf;
	//vm_printf("j=%02X\n", j);
#ifdef _WIN32
	DWORD old;
	VirtualProtect(buf, page_size, PAGE_EXECUTE_READ, &old);
#else
	mprotect(buf, page_size, PROT_READ | PROT_EXEC | PROT_WRITE);
#endif

	//__asm int 3


	if (vm->cast_stack_ptr > 0)
	{
		int cast_type = vm->cast_stack[vm->cast_stack_ptr - 1];
		//vm_printf("we want to cast to %d\n", cast_type);
		switch (cast_type)
		{
		case VAR_TYPE_FLOAT:
		case VAR_TYPE_DOUBLE:
		{
			double(*call)() = (double(*)())buf;
			varval_t *vv = se_vv_create(vm, VAR_TYPE_FLOAT);
			vv->as.flt = (float)call();
			vv->flags |= VF_FFI;
			stack_push(vm, vv);
		} break;

		default:
		{
			void*(*call)() = (void*(*)())buf;
			void *retval;
			retval = call();
			varval_t *vv = se_vv_create(vm, VAR_TYPE_INT); //should be fine for x86
			vv->as.integer = retval;
			vv->flags |= VF_FFI;
			stack_push(vm, vv);
		} break;
		}
		--vm->cast_stack_ptr;
	}
	else
	{
		void*(*call)() = (void*(*)())buf;
		void *retval;
		retval = call();
		varval_t *vv = se_vv_create(vm, VAR_TYPE_INT); //should be fine for x86
		vv->as.integer = retval;
		vv->flags |= VF_FFI;
		stack_push(vm, vv);
	}


	vm_mem_free(vm, ds);
	//vm_printf("retval=%d\n", retval);
	//vm_printf("err = %s\n", strerror(errno));

	//se_addint(vm, retval);
	status = FFI_SUCCESS;

_ffi_end:
#ifdef _WIN32
	VirtualFree(buf, 0, MEM_RELEASE);
#else
	munmap(jit, page_size);
#endif
	return status;
}

typedef struct
{
	int offset;
	int size;
	char *buffer;
} tinystreamreader_t;

void tsr_init(tinystreamreader_t *ts, char *buf, size_t sz)
{
	ts->offset = 0;
	ts->buffer = buf;
	ts->size = sz;
}
#define TSR_READ_FUNCTION(n, type) \
type tsr ## n(tinystreamreader_t *ts) \
{ \
	type *b1 = (type*)&ts->buffer[ts->offset]; \
	ts->offset += sizeof(type); \
	return *b1; \
}

TSR_READ_FUNCTION(16, uint16_t)
TSR_READ_FUNCTION(32, uint32_t)
TSR_READ_FUNCTION(8, uint8_t)

vm_ffi_lib_t *vm_library_get(vm_t *vm, const char *n)
{
	vm_hash_t h = hash_string(n);
	for (int i = vm->libs.size; i--;)
	{
		array_get(&vm->libs, vm_ffi_lib_t, lib, i);
		if (lib->hash == h)
			return lib;
	}
	return NULL;
}

vm_ffi_lib_func_t *vm_library_function_get_any(vm_t *vm, const char *n, vm_ffi_lib_t **which_lib)
{
	vm_hash_t h = hash_string(n);
	for (int k = vm->libs.size; k--;)
	{
		array_get(&vm->libs, vm_ffi_lib_t, lib, k);
		for (int i = lib->functions.size; i--;)
		{
			array_get(&lib->functions, vm_ffi_lib_func_t, f, i);
			if (f->hash == h)
			{
				if (which_lib != NULL)
					*which_lib = lib;
				return f;
			}
		}
	}
	return NULL;
}

vm_ffi_lib_func_t *vm_library_function_get(vm_t *vm, vm_ffi_lib_t *lib, const char *n)
{
	vm_hash_t h = hash_string(n);
	for (int i = lib->functions.size; i--;)
	{
		array_get(&lib->functions, vm_ffi_lib_func_t, f, i);
		if (f->hash == h)
			return f;
	}
	return NULL;
}

static int vm_read_functions(vm_t *vm, unsigned int program) {
	array_get(&vm->programs, vm_program_t, prog, program);
	int structs_start = *(int*)(prog->data + prog->size - sizeof(int));
#if 1

	//read the structs first
	tinystreamreader_t tsr;
	tsr_init(&tsr, &prog->data[structs_start], prog->size);

	int numlibs = tsr32(&tsr);
	for (int i = 0; i < numlibs; i++)
	{
		//read string
		dynstring ds = dynalloc(32);
		while (1)
		{
			uint8_t ch = tsr8(&tsr);
			if (!ch)break;
			dynpush(&ds, ch);
		}
		//vm_printf("ds = %s\n", ds);
		vm_ffi_lib_t lib;
		snprintf(lib.name, sizeof(lib.name), "%s", ds);
		lib.hash = hash_string(lib.name);
		lib.handle = vm_library_handle_open(ds);
		if (!lib.handle)
		{
			fail_lib:
			vm_printf("failed to load library '%s'\n", ds);
			dynfree(&ds);
			return 1;
		}
		array_init(&lib.functions, vm_ffi_lib_func_t);
		int vm_library_read_functions(vm_t *vm, vm_ffi_lib_t *l);
		if (vm_library_read_functions(vm, &lib))
		{
			goto fail_lib;
		}
		array_push(&vm->libs, &lib);
		//vm_printf("added lib %s with %d funcs\n", lib.name, lib.functions.size);
		dynfree(&ds);
	}
	int numstructs = tsr32(&tsr);
	//vm_printf("numstructs=%d\n", numstructs);
	for (int i = 0; i < numstructs; i++)
	{
		cstruct_t cs;
		array_init(&cs.fields, cstructfield_t);

		dynstring ds = dynalloc(32);

		while (1)
		{
			uint8_t ch = tsr8(&tsr);
			if (!ch)break;
			dynpush(&ds, ch);
		}
		//getchar();

		int size = tsr32(&tsr);
		int nf = tsr16(&tsr);
		//vm_printf("struct %s (%d), nf(%d)\n", ds, size, nf);
		snprintf(cs.name, sizeof(cs.name), "%s", ds);
		cs.size = size;
		dynfree(&ds);
		for (int j = 0; j < nf; j++)
		{
			cstructfield_t fld;
			dynstring ds = dynalloc(32);

			while (1)
			{
				uint8_t ch = tsr8(&tsr);
				if (!ch)break;
				dynpush(&ds, ch);
			}
			//vm_printf("field %s\n", ds);
			snprintf(fld.name, sizeof(fld.name), "%s", ds);
			dynfree(&ds);
			fld.offset = tsr32(&tsr);
			fld.size = tsr32(&tsr);
			fld.type = tsr32(&tsr);
			tsr32(&tsr); //TODO FIX THIS
			array_push(&cs.fields, &fld);
			//vm_printf("\toff %d\n", field_offset);
			//vm_printf("\tsz %d\n", field_size);
			//vm_printf("\ttype %d\n", field_type);
		}
		array_push(&vm->structs, &cs);
	}
#endif

	char id[4096] = { 0 };
	int id_len = 0;

	int start = *(int*)(prog->data + prog->size - sizeof(int) - sizeof(int));
	int num_funcs = *(int*)(prog->data + prog->size - sizeof(int) - sizeof(int) - sizeof(int));

	int at = start;

	for (int i = 0; i < num_funcs; i++) {
		int loc = *(int*)(prog->data + at);
		at += sizeof(int);


		id_len = 0;
		int ch = prog->data[at];
		while (at < prog->size && ch) {
			id[id_len++] = ch = prog->data[at++];
		}
		id[id_len++] = 0;

		vm_function_info_t *fi = (vm_function_info_t*)vm_mem_alloc(vm, sizeof(vm_function_info_t));
		fi->program = program;

		//fi->numlocalvars = *(uint16_t*)(vm->program + at);
		//at += sizeof(uint16_t);


		fi->position = loc;
		strncpy(fi->name, id, sizeof(fi->name));
		fi->name[sizeof(fi->name) - 1] = '\0';

		vector_add(&vm->functioninfo, fi);
		//vm_printf("id=%s at %d\n", id, loc);
		//usefull for if u wanna call specific function and this has the locations etc
	}

	//read int of num refs called to builtins
	int num_calls_to_builtin = *(int*)(prog->data + at);
	at += sizeof(int);
	//vm_printf("num_calls_to_builtin=%d\n", num_calls_to_builtin);
	vt_istring_t *istr = NULL;
	for (int i = 0; i < num_calls_to_builtin; i++) {

		id_len = 0;
		id[id_len] = '\0';
		int ch = prog->data[at];
		while (at < prog->size && ch) {
			id[id_len++] = ch = prog->data[at++];
		}
		if (id_len == 0)
			++at;//if it's a empty string, still eat up the \0 character
		id[id_len++] = 0;
		//vm_printf("ID=%s\n", id);
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
	//vector_add(&vm->__mem_allocations, p);
	return p;
}

#ifdef MEMORY_DEBUG
void vm_mem_free_r(vm_t *vm, void *block, const char *_file, int _line) {
#else
void vm_mem_free_r(vm_t *vm, void *block) {
#endif
	if (!block)
		return;
#if 0
	int loc = vector_locate(&vm->__mem_allocations, block);

	if (loc != -1)
		vector_delete(&vm->__mem_allocations, loc);
	else {
#ifdef MEMORY_DEBUG
		vm_printf("could not find block in allocs! %s;%d", _file, _line);
#endif
	}
#endif
	free(block);
}

void vm_error(vm_t *vm, int errtype, const char *fmt, ...) {
	vm_printf("VM ERROR! %d\n", errtype);
	exit(-1);
}

cstruct_t *vm_get_struct(vm_t *vm, unsigned int ind)
{
	array_get(&vm->structs, cstruct_t, cs, ind);
	return cs;
}

void vm_use_program(vm_t *vm, unsigned int program)
{
	array_get(&vm->programs, vm_program_t, p, program);
	vm->instr = p->data;
}

int vm_add_program(vm_t *vm, unsigned char *buffer, size_t sz, const char *tag)
{
	vm_program_t prog;
	if (tag)
		snprintf(prog.tag, sizeof(prog.tag), "%s", tag);
	else
		strcpy(prog.tag, "none");
	prog.data = (char*)vm_mem_alloc(vm, sz);
	memcpy(prog.data, (void*)&buffer[0], sz);
	prog.size = sz;
	array_push(&vm->programs, &prog);
	if (vm_read_functions(vm, vm->programs.size - 1))
	{
		vm_free(vm);
		return 1;
	}
	return 0;
}

int se_error(vm_t *vm, const char *errstr, ...)
{
	char dest[1024 * 16];
	va_list argptr;
	if (errstr != NULL) {
		va_start(argptr, errstr);
		vsprintf(dest, errstr, argptr);
		va_end(argptr);
		vm_printf("%s\n", dest);
	}
	vm_error(vm, E_VM_ERR_ERROR, "%s", dest);
	return 0;
}

vm_t *vm_create() {
	vm_t *vm = NULL;
	vm = (vm_t*)malloc(sizeof(vm_t));
	if (vm == NULL) {
		vm_printf("vm is NULL\n");
		return vm;
	}

	vm->instr = NULL;

	_vconst0.value[0] = (vm_scalar_t)0.0;
	_vconst1.value[0] = (vm_scalar_t)1.0;
	_vconst2.value[0] = (vm_scalar_t)2.0;
	_vconst3.value[0] = (vm_scalar_t)3.0;

	_vconst0.nelements = 1;
	_vconst1.nelements = 1;
	_vconst2.nelements = 1;
	_vconst3.nelements = 1;
	_vconst0.integral = false;
	_vconst1.integral = false;
	_vconst2.integral = false;
	_vconst3.integral = false;

	memset(vm, 0, sizeof(vm_t));
	vm->m_printf_hook = printf;

	//clear the var cache
	//memset(&vm->varcache, 0, sizeof(vm->varcache));
	//vm->varcachesize = 0;
	//vector_init(&vm->varcachearray);
	stk_init(&vm->varcacheavail);
	vm->varcacheindex = 0;
	vm->varcachemax = MAX_CACHED_VARIABLES;
	//initial empty variables allocation
	for (size_t i = 0; i < vm->varcachemax; ++i)
	{
		varval_t *vv = (varval_t*)vm_mem_alloc(vm, sizeof(varval_t));
		memset(vv, 0, sizeof(varval_t));
		//vector_add(&vm->varcachearray, vv);
		stk_push(&vm->varcacheavail, vv);
		//printf("stk cap:%d,sz:%d\n", vm->varcacheavail.capacity, vm->varcacheavail.size);
	}

	array_init(&vm->structs, cstruct_t);
	array_init(&vm->libs, vm_ffi_lib_t);
	array_init(&vm->events, vm_event_t);
	vector_init(&vm->ffi_callbacks);

	vector_init(&vm->__mem_allocations);
	//vector_init(&vm->vars);

	vector_init(&vm->functioninfo);

	vm->level = se_createobject(vm, VT_OBJECT_LEVEL, NULL, NULL, NULL);
	vm->level->refs = 1337; //will only free after vm frees yup
	//vm->self = NULL;

	//vm->program = (char*)vm_mem_alloc(vm, programsize);
	//memcpy(vm->program, (void*)&program[0], programsize);
	//vm->program_size = programsize;
	vm->cast_stack_ptr = 0;

#define INITIAL_ISTRING_LIST_SIZE (4096)

	vm->istringlist = (vt_istring_t*)vm_mem_alloc(vm, sizeof(vt_istring_t) * INITIAL_ISTRING_LIST_SIZE);
	memset(vm->istringlist, 0, sizeof(vt_istring_t)*INITIAL_ISTRING_LIST_SIZE);
	vm->istringlistsize = INITIAL_ISTRING_LIST_SIZE;

	extern stockfunction_t std_scriptfunctions[];
	se_register_stockfunction_set(vm, std_scriptfunctions);

	extern stockmethod_t std_scriptmethods[];
	se_register_stockmethod_set(vm, VT_OBJECT_LEVEL, std_scriptmethods);

	vm->threadrunners = (vm_thread_t*)vm_mem_alloc(vm, sizeof(vm_thread_t) * MAX_SCRIPT_THREADS);
	memset(vm->threadrunners, 0, sizeof(vm_thread_t)*MAX_SCRIPT_THREADS);
	//tmp because of bottleneck everytime allocating new thread stacksize lol
	for (int i = MAX_SCRIPT_THREADS; i--;) {
		vm_thread_t *thr = &vm->threadrunners[i];
		thr->stacksize = VM_STACK_SIZE;
		thr->stack = (intptr_t*)vm_mem_alloc(vm, thr->stacksize * sizeof(intptr_t));
		vector_init(&thr->strings);
		vm_thread_reset_events(vm, thr);
		//memset(&thr->eventstrings, 0, sizeof(thr->eventstrings));
	}
	vm->numthreadrunners=0;
	vm->thrunner = NULL;

	array_init(&vm->programs, vm_program_t);
	vm->is_running = true; //not needed anymore?
	return vm;
}

int vm_exec_ent_thread(vm_t *vm, varval_t *new_self, const char *func_name, int numargs) {

	vm_function_info_t *fi = NULL;

	for (int i = 0; i < vector_count(&vm->functioninfo); i++) {
		vm_function_info_t *ffi = (vm_function_info_t*)vector_get(&vm->functioninfo, i);
		if (!stricmp(ffi->name, func_name)) {
			fi = ffi;
			break;
		}
	}

	if (fi == NULL)
		return E_VM_RET_FUNCTION_NOT_FOUND;
	vm_use_program(vm, fi->program);
	return vm_exec_ent_thread_pointer(vm, new_self, fi->position, numargs);
}

void * vm_get_user_pointer(vm_t *vm)
{
	return vm->m_userpointer;
}

void vm_set_user_pointer(vm_t *vm, void *ptr)
{
	vm->m_userpointer = ptr;
}

varval_t *vm_get_level_object(vm_t *vm)
{
	return vm->level;
}

void vm_set_printf_hook(vm_t *vm, void *hook)
{
	vm->m_printf_hook = hook;
	*(void**)&g_printf_hook = (int(*)(const char*,...))hook;
}

void* vm_library_handle_open(const char *libname)
{
	void *p = 0;
#ifdef _WIN32
	p = LoadLibraryA(libname);
#else
	p = dlopen(libname, RTLD_LAZY);
#endif
	return p;
}

void vm_library_handle_close(void *p)
{
#ifdef _WIN32
	FreeLibrary((HMODULE)p);
#else
	dlclose(p);
#endif
}

#ifdef _WIN32
dynstring GetLastErrorAsString()
{
	//Get the error message, if any.
	DWORD errorMessageID = GetLastError();
	if (errorMessageID == 0)
		return NULL;

	LPSTR messageBuffer = NULL;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	dynstring ds = dynnew(messageBuffer);
	//Free the buffer.
	LocalFree(messageBuffer);
	return ds;
}
#endif

int vm_library_read_functions(vm_t *vm, vm_ffi_lib_t *l)
{
	if (!l->handle)
		return 1;
#if 0
	HMODULE lib = LoadLibraryExA(l->name, NULL, DONT_RESOLVE_DLL_REFERENCES);
	if (!lib)
	{
		dynstring errstr = GetLastErrorAsString();
		vm_printf("'%s' -> %s\n", l->name, errstr);
		dynfree(&errstr);
		return 1;
	}
#endif
#ifdef _WIN32
	HMODULE lib = (HMODULE)l->handle;
	//assert(((PIMAGE_DOS_HEADER)lib)->e_magic == IMAGE_DOS_SIGNATURE);
	PIMAGE_NT_HEADERS header = (PIMAGE_NT_HEADERS)((BYTE *)lib + ((PIMAGE_DOS_HEADER)lib)->e_lfanew);
	//assert(header->Signature == IMAGE_NT_SIGNATURE);
	//assert(header->OptionalHeader.NumberOfRvaAndSizes > 0);
	PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE *)lib + header->
		OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	//assert(exports->AddressOfNames != 0);
	BYTE** names = (BYTE**)((int)lib + exports->AddressOfNames);
	for (int i = 0; i < exports->NumberOfNames; i++)
	{
		const char *exportName = (BYTE *)lib + (int)names[i];
		vm_ffi_lib_func_t func;
		snprintf(func.name, sizeof(func.name), "%s", exportName);
		func.address = (void*)GetProcAddress(lib, func.name);
		if (!func.address)
		{
			vm_printf("failed to load function '%s' from '%s'\n", exportName, l->name);
			continue;
		}
		//vm_printf("Export: %s\n", (BYTE *)lib + (int)names[i]);
		func.hash = hash_string(func.name);
		array_push(&l->functions, &func);
	}
#else

	void read_elf_exported_symbols(vm_t *vm, vm_ffi_lib_t *l, const char* filename);
	read_elf_exported_symbols(vm, l, l->name);
#endif
	return 0;
}

void vm_free(vm_t *vm) {
	if (!vm)
		return;
	vm_unregister_c_ffi_callbacks(vm);
	vm->thrunner = NULL;

	//int num_vars_left = vector_count(&vm->vars);
	//vm_printf("num vars left =%d\n", num_vars_left);

	for (int evi = vm->events.size; evi--;)
	{
		array_get(&vm->events, vm_event_t, ev, evi);
		se_vv_remove_reference(vm, ev->object);
		for (int argi = 0; argi < ev->numargs; ++argi)
		{
			se_vv_remove_reference(vm, ev->arguments[argi]);
		}
	}
	array_free(&vm->events);
	//first free the thread local vars so the refs get -1

	vm_thread_t *thr = NULL;
#if 0
	varval_t nullval;
	nullval.refs = (1 << (sizeof(nullval.refs) * 8) - 1); //max refs
	nullval.type = VAR_TYPE_NULL;
	//unneeded atm cuz NULL serves fine
#endif
	//basically see it as
	/*
	run() {
		an active thread
		we would be adding a return; statement here
		<--
		while(1) { ; }
	}
	*/
	for (int i = MAX_SCRIPT_THREADS; i--;) {
		thr = &vm->threadrunners[i];

		//if (!thr->active)
			//continue;
		vm_thread_reset_events(vm, thr);

		if (thr->registers[REG_SP] == 0)
			continue;

		vm->thrunner = thr;
		//rollback the whole stack
		int nrb = 0;
		//vm_printf("thread sp = %d\n", vm_registers[REG_SP]);
		while (1)
		{
			if (vm_registers[REG_SP] <= 2) //remaining one is the NULL we pushed
				break;
			//vm_printf("nrb = %d, sp = %d\n", nrb++, vm_registers[REG_SP]);
			stack_push_vv(vm, NULL);
			vm_execute(vm, OP_RET);
		}
		
		thr->active = false; //enough to clear it rofl
		--vm->numthreadrunners;
	}
	vm->thrunner = NULL;

#if 0
	for (int i = 0; i < MAX_SCRIPT_THREADS; i++) {
		vm_thread_t *thr = &vm->threadrunners[i];

		if (!thr->active)
			continue;
#if 0
		for (int jj = 0; jj < MAX_LOCAL_VARS; jj++) {
			varval_t *vv = (varval_t*)thr->stack[thr->registers[REG_BP] + jj]; //pop all local vars?
			if (vv != NULL) {
				//vm_printf("removed reference for %s\n", VV_TYPE_STRING(vv));
				se_vv_remove_reference(vm, vv);
			}
		}
#endif
		vm_execute(vm, OP_RET);
	}
#endif

	se_vv_free_force(vm, vm->level);

	//not sure about this, after long time not working on this forgot really what i intended, but i think this should be ok ish
	//try normally
#if 0
	while (vector_count(&vm->vars) > 0)
	{
		for (int i = 0; i < vector_count(&vm->vars); i++) {
			varval_t *vv = (varval_t*)vector_get(&vm->vars, i);
			if (vv->refs > 0) {
				int nf = 0;
				if (VV_USE_REF(vv))
					nf = vector_count(&vv->as.obj->fields);
				//vm_printf("skipping vv %d (%s), refs = %d, fields = %d\n", i, VV_TYPE_STRING(vv), vv->refs, nf);
				for (int j = 0; j < nf; j++) {
					vt_object_field_t *ff = (vt_object_field_t*)vector_get(&vv->as.obj->fields, j);
					//vm_printf("field %s\n", vm->istringlist[ff->stringindex].string);
				}
				continue;
			}
			//vm_printf("freeing leftover var %02X (%s)\n", vv, VV_TYPE_STRING(vv));
			//se_vv_free(vm, vv);
			se_vv_remove_reference(vm, vv);
		}
		//vm_printf("vec count = %d\n", vector_count(&vm->vars));
	}
#endif
#if 0
	for (int i = vector_count(&vm->vars) - 1; i > -1; i--) {
		varval_t *vv = (varval_t*)vector_get(&vm->vars, i);
		/*if (vv->refs > 0) {
			int loc = vector_locate(&vm->vars, vv);
			if (loc != -1)
				vector_delete(&vm->vars, loc);
		} else */
		vm_printf("vv (%s), ref = %d\n", VV_TYPE_STRING(vv), vv->refs);
		if (!VV_USE_REF(vv))
			se_vv_free_force(vm, vv);
	}
#endif

	for (int i = 0; i < vector_count(&vm->ffi_callbacks); ++i)
	{
		vm_ffi_callback_t *cb = (vm_ffi_callback_t*)vector_get(&vm->ffi_callbacks, i);
		vm_mem_free(vm, cb);
	}
	vector_free(&vm->ffi_callbacks);
	//vector_free(&vm->vars);

	//free all the variables
#if 0
	for (size_t i = 0; i < vm->varcachemax; ++i)
	{
		varval_t *vv = (varval_t*)vector_get(&vm->varcachearray, i);
		vm_mem_free(vm, vv);
	}
#endif

	//printf("stk cap:%d,sz:%d\n", vm->varcacheavail.capacity, vm->varcacheavail.size);

	while(vm->varcacheavail.size > 0)
	{
		varval_t *vv = (varval_t*)stk_pop(&vm->varcacheavail);
		//printf("stk cap:%d,sz:%d\n", vm->varcacheavail.capacity, vm->varcacheavail.size);
		vm_mem_free(vm, vv);
	}

	stk_free(&vm->varcacheavail);
	//vector_free(&vm->varcachearray);

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

	for (int i = vm->structs.size; i--;)
	{
		array_get(&vm->structs, cstruct_t, cs, i);
		array_free(&cs->fields);
	}
	array_free(&vm->structs);

	//vm_mem_free(vm, vm->program);

	for (int i = 0; i < vector_count(&vm->__mem_allocations); i++) {
		void *p = vector_get(&vm->__mem_allocations, i);
		free(p);
	}
	vector_free(&vm->__mem_allocations);

	/* cleanup the loaded libraries and their functions arrays and stuff */
	for (int i = 0; i < vm->libs.size; i++)
	{
		array_get(&vm->libs, vm_ffi_lib_t, lib, i);
		vm_library_handle_close(lib->handle);
		array_free(&lib->functions);
	}
	array_free(&vm->libs);
	for (unsigned int i = 0; i < vm->programs.size; ++i)
	{
		array_get(&vm->programs, vm_program_t, prog, i);
		vm_mem_free(vm, prog->data);
	}
	array_free(&vm->programs);

	free(vm);
}

int vm_get_num_active_threadrunners(vm_t *vm) {
	if (!vm) return 0;
	return vm->numthreadrunners;
}

#endif
