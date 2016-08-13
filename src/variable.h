#pragma once
#include "include/cidscropt.h"

#include "stdheader.h"

#define MAX_VARIABLES 1024
#define MAX_LOCAL_VARS (255)

#define VV_USE_REF(x) ((x)&&(x->type==VAR_TYPE_OBJECT || x->type==VAR_TYPE_ARRAY))
#define VV_IS_STRING(x) ((x)&&(x->type==VAR_TYPE_STRING||x->type==VAR_TYPE_INDEXED_STRING))
#define VV_TYPE(x) (x ? x->type : VAR_TYPE_NULL)
#define VV_TYPE_STRING(x) (e_var_types_strings[VV_TYPE(x)])

static const char *e_var_types_strings[] = {
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
	0
};

#define VF_LOCAL (1<<0)

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