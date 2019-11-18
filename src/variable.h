#pragma once
#include "include/cidscropt.h"

#include "stdheader.h"

#define MAX_VARIABLES 1024
#define MAX_LOCAL_VARS (32)

#define VV_CAST(x) ((varval_t*)x)
#define VV_CAST_VAR(x,y) varval_t* x = VV_CAST(y)
#define VV_INTEGER uintptr_t
#define VV_USE_REF(x) (x != NULL && (x->type==VAR_TYPE_OBJECT || x->type==VAR_TYPE_ARRAY))
#define VV_IS_STRING(x) ((x)&&(x->type==VAR_TYPE_STRING||x->type==VAR_TYPE_INDEXED_STRING))
#define VV_TYPE(x) (x ? x->type : VAR_TYPE_NULL)

//#define VV_TYPE_STRING(x) (e_var_types_strings[VV_TYPE(x)])
#define VV_TYPE_STRING(x) vv_get_type_string(x)
//#define VV_IS_NUMBER(x) (VV_TYPE(x) == VAR_TYPE_INT || VV_TYPE(x) == VAR_TYPE_FLOAT)
VM_INLINE bool VV_IS_NUMBER(varval_t *vv)
{
	switch (VV_TYPE(vv))
	{
	case VAR_TYPE_LONG:
	case VAR_TYPE_INT:
	case VAR_TYPE_SHORT:
	case VAR_TYPE_CHAR:
	case VAR_TYPE_FLOAT:
	case VAR_TYPE_DOUBLE:
		return true;
	}
	return false;
}
VM_INLINE bool VV_IS_INTEGRAL(varval_t *vv)
{
	switch (VV_TYPE(vv))
	{
	case VAR_TYPE_DOUBLE:
	case VAR_TYPE_FLOAT:
		return false;
	}
	return true;
}
#define VV_IS_POINTER(x) (x != NULL && (x->flags & VF_POINTER) == VF_POINTER)
#define VV_IS_UNSIGNED(x) (x && (x->flags & VF_UNSIGNED) == VF_UNSIGNED)

typedef long long vm_long_t;

static const char *e_var_types_strings[] = {
#if 0
	"int",
	"float",
	"char",
	"string",
	"indexed string",
	"vector",
	"array",
	"object",
	"object reference",
	"null",
#endif

	"CHAR",
	"SHORT",
	"INT",
	"LONG",
	"FLOAT",
	"DOUBLE",

	"STRING",
	"INDEXED_STRING",
	"VECTOR",
	"ARRAY",
	"OBJECT",
	"OBJECT_REFERENCE",
	"FUNCTION_POINTER",
	"NULL", //the vv is NULL itself
	0
};

VM_INLINE const char *vv_get_type_string(varval_t *vv) {
	static char typestring[256] = { 0 };
	snprintf(typestring, sizeof(typestring), "%s%s%c", VV_IS_UNSIGNED(vv) ? "unsigned": "", e_var_types_strings[VV_TYPE(vv)], VV_IS_POINTER(vv) ? '*' : '\0');
	return typestring;
}

typedef struct {
	char name[256];
	int flags;
	varval_t value;
	int index;
	int refs;
} var_t;

vt_istring_t *se_istring_create(vm_t *vm, const char *str);
vt_istring_t *se_istring_find(vm_t *vm, const char *str);
void se_istring_delete(vm_t *vm, vt_istring_t *istr);
