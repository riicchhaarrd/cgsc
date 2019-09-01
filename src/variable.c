#include <stdio.h>
#include <string.h>
#include "virtual_machine.h"

static int vars_created = 0;

vt_object_t *se_obj_create(vm_t *vm) {
	vt_object_t *obj = (vt_object_t*)vm_mem_alloc(vm, sizeof(vt_object_t));
	//vm_printf("CREATED OBJ %02X\n", obj);
	obj->type = VT_OBJECT_GENERIC;
	obj->custom = NULL;
	obj->obj = NULL;
	obj->constructor = NULL;
	obj->deconstructor = NULL;
	/*
	obj->numfields = 0;
	obj->fields = NULL;
	*/
	vector_init(&obj->fields);
	return obj;
}

static VM_INLINE bool varcache_has_available(vm_t *vm)
{
	return (vm->varcachesize < MAX_CACHED_VARS);
}

//bit messy ye oh well
static VM_INLINE varval_t *vv_alloc_mem(vm_t *vm)
{
	if (!varcache_has_available(vm))
	{
		varval_t *vv = (varval_t*)vm_mem_alloc(vm, sizeof(varval_t));
		memset(vv, 0, sizeof(varval_t));
		return vv;
	}
	for (int i = MAX_CACHED_VARS; i--;)
	{
		varval_t *vv = &vm->varcache[i];
		if(vv->flags & VF_CACHED)
			continue;
		memset(vv, 0, sizeof(varval_t));
		vv->flags |= VF_CACHED;
		++vm->varcachesize;
		return vv;
	}
	return NULL;
}

static VM_INLINE void vv_free_mem(vm_t *vm, varval_t *vv)
{
	if (vv->flags & VF_CACHED)
	{
		vv->flags &= ~VF_CACHED;
		--vm->varcachesize;
		return;
	}
	vm_mem_free(vm, vv);
}

//TODO add to global GC so it can delete/clear them up?
#ifdef MEMORY_DEBUG
varval_t *se_vv_create_r(vm_t *vm, e_var_types_t type, const char *_file, int _line) {
#else
varval_t *se_vv_create(vm_t *vm, e_var_types_t type) {
#endif
	++vars_created;

	varval_t *vv = vv_alloc_mem(vm);
	memset(&vv->as, 0, sizeof(vv->as));

#ifdef MEMORY_DEBUG
	snprintf(vv->debugstring, sizeof(vv->debugstring), "%s [%d]", _file, _line);
#endif
	vv->type = type;
	vv->refs = 0;

	if (VV_USE_REF(vv))
		vv->as.obj = se_obj_create(vm);
	
	vector_add(&vm->vars, vv);

	//vm_printf("adding vv(%02X)\n", vv);

	//vm_printf("vars size =%d\n", vector_count(&vm->vars));
#ifdef MEMORY_DEBUG

	vm_printf("VAR INIT\n");
	for (int i = 0; i < vector_count(&vm->vars); i++) {
		vm_printf("v %d = %02X\n", i, vector_get(&vm->vars, i));
	}
	vm_printf("VAR INIT END\n");
#endif
	return vv;
}

varval_t *se_vv_copy(vm_t *vm, varval_t *vv) {
	if (vv == NULL)
		return vv;

	if (VV_USE_REF(vv))
		return vv; //objects can't make copies lol

	varval_t *copy = se_vv_create(vm, VV_TYPE(vv));
	memcpy(&copy->as, &vv->as, sizeof(vv->as));
	copy->refs = 0; //already done in create lol
	copy->flags |= vv->flags & (VF_UNSIGNED | VF_POINTER);

	if (vv->type == VAR_TYPE_STRING) {
		size_t len = strlen(vv->as.string) + 1;
		char *str = (char*)vm_mem_alloc(vm, len);
		snprintf(str, len, "%s", vv->as.string);
		copy->as.string = str;
	}
	return copy;
}

void se_vv_gc_nullify_references(vm_t *vm, varval_t *dangler) {
	if (vm->thrunner == NULL)
		return;

	if (dangler->refs > 0)
		return;

	for (int i = 0; i < vector_count(&vm->level->as.obj->fields); i++) {
		varval_t *level_field = (varval_t*)vector_get(&vm->level->as.obj->fields, i);
		if (level_field == dangler) {
			vector_set(&vm->level->as.obj->fields, i, NULL);
		}
	}

	for (int i = 0; i < MAX_LOCAL_VARS; i++) { //hardcoded 256 may change (local vars limit)
		varval_t *lvar = (varval_t*)vm_stack[vm_registers[REG_BP] + i];
		if (lvar == NULL)
			continue;

		if (lvar == dangler) {
			vm_stack[vm_registers[REG_BP] + i] = (intptr_t)NULL;
		}
		else if (VV_USE_REF(lvar)) {
			vt_object_t *obj = lvar->as.obj;
			vt_object_field_t *field = NULL;

			for (int i = 0; i < vector_count(&obj->fields); i++) {
				field = (vt_object_field_t*)vector_get(&obj->fields, i);
/*
			for (int i = 0; i < obj->numfields; i++) {
				field = &obj->fields[i];
*/
				if (field->value == dangler) {
#if 0
					varval_t *null_vv = se_vv_create(VAR_TYPE_NULL);
					null_vv->refs = 1;
					field->value = null_vv;
#endif
					field->value = NULL;
				}
			}
		}
	}
}

void se_vv_remove_reference(vm_t *vm, varval_t *vv) {
	if (vv == NULL)
		return;
	static int removed = 0;
	--vv->refs;
	//vm_printf("REMOVE_REFERENCE (%02X) %d <refs=%d, %s>\n", vv, removed++, vv->refs, e_var_types_strings[vv->type]);

	se_vv_free(vm, vv);
	se_vv_gc_nullify_references(vm, vv);
}

void vv_string_set(vm_t *vm, varval_t *vv, const char *s)
{
	if (VV_TYPE(vv) != VAR_TYPE_STRING)
		return;
	vm_mem_free(vm, vv->as.string);

	size_t len = strlen(s);
	char *str = (char*)vm_mem_alloc(vm, len + 1);
	strncpy(str, s, len);
	str[len] = '\0';
	vv->as.string = str;
}

void se_vv_string_free(vm_t *vm, varval_t *vv) {
	if (vv->type != VAR_TYPE_STRING)
		return;
	vm_mem_free(vm, vv->as.string);
	vv->as.string = NULL;
}

varval_t *se_vv_get_field(vm_t *vm, varval_t *vv, int key) {
	if (!VV_USE_REF(vv)) {
		vm_printf("tried to get key on non-object\n");
		return NULL;
	}

	vt_object_t *obj = vv->as.obj;
	vt_object_field_t *field = NULL;
	//vm_printf("GET_FIELD{numfields=%d}\n", obj->numfields);
	if (vv->type == VAR_TYPE_OBJECT) {

		if (obj->custom != NULL) {
			for (int i = 0; obj->custom[i].name; i++) {
				const char *as_str = se_index_to_string(vm, key);
				if (as_str == NULL) {
					vm_printf("as_str is null, key = %d, custom name = %s\n", key, obj->custom[i].name);
					break;
				}
				if (!strcmp(obj->custom[i].name, as_str)) {
					vt_obj_getter_prototype gttr = (vt_obj_getter_prototype)obj->custom[i].getter;
					if (gttr) {
						int result = gttr(vm, vv->as.obj->obj);
						varval_t *ret = NULL;
						if (result)
							ret = (varval_t*)stack_pop(vm);
						return ret;
					}
				}
			}
		}
		/*
		for (int i = 0; i < obj->numfields; i++) {
			if (obj->fields[i].index == key) {
				field = &obj->fields[i];
				break;
			}
		}
		*/
	get_arr_idx_new:
		for (int i = 0; i < vector_count(&obj->fields); i++) {
			vt_object_field_t *ff = (vt_object_field_t*)vector_get(&obj->fields, i);
			if (ff->stringindex == key) {
				field = ff;
				break;
			}
		}
	}
	else {
		goto get_arr_idx_new;
#if 0
		if (key < obj->numfields)
			field = &obj->fields[key];
#endif
	}
	if (NULL == field)
		return NULL;
	return field->value;
}

void se_vv_set_field(vm_t *vm, varval_t *vv, int key, varval_t *value) {
	if (!VV_USE_REF(vv)) {
		vm_printf("tried to set key on non-object/array\n");
		return;
	}
	vt_object_t *obj = vv->as.obj;
	vt_object_field_t *field = NULL;
	if (vv->type == VAR_TYPE_OBJECT) {

		if (obj->custom != NULL) {
			for (int i = 0; obj->custom[i].name; i++) {
				const char *as_str = se_index_to_string(vm, key);
				if (as_str == NULL) {
					//vm_printf("as_str is null, key = %d, custom name = %s\n", key, obj->custom[i].name);
					break;
				}
				if (!strcmp(obj->custom[i].name, as_str)) {
					vt_obj_setter_prototype sttr = (vt_obj_setter_prototype)obj->custom[i].setter;
					if (sttr) {
						int savenumargs = vm->thrunner->numargs;
						vm->thrunner->numargs = 1;
						intptr_t savebp = vm_registers[REG_BP];
						vm_registers[REG_BP] = vm_registers[REG_SP] + 1;
						stack_push_vv(vm, value);
						sttr(vm, vv->as.obj->obj);
						stack_pop(vm);
						vm_registers[REG_BP] = savebp;
						vm->thrunner->numargs = savenumargs;
						//se_vv_remove_reference(vm, value); the caller will free this, and don't need to remove reference?
						return;
					}
				}
			}
		}
		/*
		for (int i = 0; i < obj->numfields; i++) {
			if (obj->fields[i].stringindex == key) {
				field = &obj->fields[i];
				break;
			}
		}
		*/
		get_arr_idx_new:
		for (int i = 0; i < vector_count(&obj->fields); i++) {
			vt_object_field_t *ff = (vt_object_field_t*)vector_get(&obj->fields, i);
			if (ff->stringindex == key) {
				field = ff;
				break;
			}
		}
	}
	else {
		goto get_arr_idx_new;
		//scrapped for now
		//get array index
#if 0
		if (key < obj->numfields)
			field = &obj->fields[key];
#endif
	}

	if (field == NULL) { //not found create new one
		/*
		++obj->numfields;

		vt_object_field_t *n = (vt_object_field_t*)vm_mem_realloc(vm, obj->fields, sizeof(vt_object_t) * obj->numfields);
		if (n == NULL) {
			vm_printf("out of memory!\n");
			return;
		}
		obj->fields = n;
		field=&obj->fields[obj->numfields - 1];
		field->value = NULL;
		*/
		vt_object_field_t *ff = (vt_object_field_t*)vm_mem_alloc(vm, sizeof(vt_object_field_t));
		if (!ff) {
			perror("out of memory\n");
			return;
		}
		ff->value = NULL;
		vector_add(&obj->fields, ff);
		field = ff;
	}

	//don't make copy? :s
	varval_t *copy = value;
	if (VV_USE_REF(value))
		++copy->refs;
	else
		copy = se_vv_copy(vm, value);
	se_vv_remove_reference(vm, field->value);
	field->value = copy;
	field->index = key;

	//vm_printf("key(%s) value ptr %02X (new refs = %d)\n", se_index_to_string(vm, key), value, value->refs);
}

static void se_vv_object_free_fields(vm_t *vm, varval_t *vv) {
	/*
	if (!obj->fields) //if empty object lol
		return;

	for (int i = 0; i < obj->numfields; i++) {
		se_vv_remove_reference(vm, obj->fields[i].value);
		obj->fields[i].value = NULL;
	}
	vm_mem_free(vm, obj->fields);
	obj->fields = NULL;
	*/
	vt_object_t *obj = vv->as.obj;

	//vm_printf("FREEING OBJECT %02X, numfields=%d (%s)\n", obj, vector_count(&obj->fields), VV_TYPE_STRING(vv));

	int nf = vector_count(&obj->fields);
	//vm_printf("numfields=%d\n", nf);
	for (int i = 0; i < vector_count(&obj->fields); i++) {
		vt_object_field_t *of = (vt_object_field_t*)vector_get(&obj->fields, i);
		//vm_printf("freeing field %s\n", se_index_to_string(vm, of->stringindex));
		if (of->value == NULL)
			continue;
		//vm_printf("DELETING OBJECT FIELD %d\n", i);
		se_vv_remove_reference(vm, of->value);
		of->value = NULL;
	}
	vector_free(&obj->fields);
}

void se_vv_object_free(vm_t *vm, varval_t *vv) {
	if (!VV_USE_REF(vv))
		return;
	//vm_printf("FREE OBJ [%02X] numfields=%d\n", vv->obj, vector_count(&vv->obj->fields));
	se_vv_object_free_fields(vm, vv);
	if (vv->as.obj->deconstructor != NULL)
		vv->as.obj->deconstructor(vm, vv->as.obj->obj);
	vm_mem_free(vm, vv->as.obj);
	vv->as.obj = NULL;
}

void se_vv_free_force(vm_t *vm, varval_t *vv) {
	if (!vv)
		return;
	vv->refs = 0;
	se_vv_free(vm, vv);
}

bool se_vv_is_freeable(vm_t *vm, varval_t *vv)
{
	if (!vv)
		return false;

	if (VV_HAS_FLAG(vv, VF_NOFREE))
		return false;

	if (vv->refs > 0)
		return false;
	return true;
}

int se_vv_free_r(vm_t *vm, varval_t *vv, const char *_file, int _line)
{
#if 0
	if (!vv) {
		//vm_printf("no need to free null!\n");
		return 1;
	}

	if (VV_HAS_FLAG(vv, VF_NOFREE))
		return 1;

	if (vv->refs > 0) {
		//vm_printf("VV->REFS > 0 (%d, TYPE=%s)\n", vv->refs, e_var_types_strings[vv->type]);
		return 1;
	}
#endif

	if (!se_vv_is_freeable(vm, vv))
		return 1;

	--vars_created;
#if 0
	if(vars_created == 0)
		vm_printf("vars freed=%d\n", vars_created);
#endif
	if (!VV_IS_POINTER(vv))
	{
		if (vv->type == VAR_TYPE_STRING)
			se_vv_string_free(vm, vv);
		else if (VV_USE_REF(vv))
			se_vv_object_free(vm, vv);
	}
	int loc = vector_locate(&vm->vars, vv);

	if (loc != -1) {
#if 0
		if (vv->flags & VAR_FLAG_LEVEL_PLAYER) {
			//__asm int 3 //interupt m8
			vm_printf("vv->flags & VAR_FLAG_LEVEL_PLAYER\n");
			getchar();
		}
		vm_printf("deleting %02X from vm->vars FROM %s:%d\n", vv, _file, _line);
#endif
		vector_delete(&vm->vars, loc);
		//vm_printf("vars size %d\n", vector_count(&vm->vars));
	} else {
		//vm_printf("vv (%02X) not found in vars!\n", vv);
	}

#ifdef MEMORY_DEBUG
	vm_printf("VAR FREE\n");
	for (int i = 0; i < vector_count(&vm->vars); i++) {
		varval_t *ivv = (varval_t*)vector_get(&vm->vars, i);
		vm_printf("v %d (refs = %d) (type=%s) = %02X debugstr=%s (%s)\n", i, ivv->refs, VV_TYPE_STRING(ivv), ivv, ivv->debugstring, se_vv_to_string(vm,ivv));
	}
	vm_printf("VAR FREE END\n");
#endif

	//add here the strings/objects/array other type of bs to clean aswell
	//vm_mem_free(vm, vv);
	vv_free_mem(vm, vv);
	return 0;
}

vt_istring_t *se_istring_create(vm_t *vm, const char *str) {
	vt_istring_t *istr = NULL;
	for (int i = 0; i < vm->istringlistsize; i++) {
		istr = &vm->istringlist[i];
		if (!istr->string) {
			size_t str_len = strlen(str);
			istr->index = i;
			istr->string = (char*)vm_mem_alloc(vm, str_len + 1);
			strncpy(istr->string, str, str_len);
			istr->string[str_len] = '\0';
			return istr;
		}
	}
	return NULL;
}

vt_istring_t *se_istring_find(vm_t *vm, const char *str) {
	vt_istring_t *istr = NULL;
	for (int i = 0; i < vm->istringlistsize; i++) {
		istr = &vm->istringlist[i];
		if (!istr->string)
			continue;
		if (!strcmp(istr->string, str)) {
			return istr;
		}
	}
	return NULL;
}

const char *se_index_to_string(vm_t *vm, int i) {
	if (i >= vm->istringlistsize) {
		vm_printf("index to string too high %d, list size = %d\n", i, vm->istringlistsize);
		return NULL;
	}
	return vm->istringlist[i].string;
}

int se_istring_from_string(vm_t *vm, const char *str) {
	vt_istring_t *istr = se_istring_find(vm, str);
	if (istr == NULL)
		istr = se_istring_create(vm, str);
	return istr->index;
}

void se_istring_delete(vm_t *vm, vt_istring_t *istr) {
	vm_mem_free(vm, istr->string);
	istr->string = NULL;
}