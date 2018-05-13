#pragma once

#include <malloc.h>

#define DYN_INLINE inline
#define DYN_STATIC static
#define DYN_ARRAY_FUNCTION DYN_INLINE DYN_STATIC

typedef struct
{
	int capacity;
	union {
		int n_elements, size;
	};
	int element_size;
	char *data;
} dynarrayhdr_t;

typedef dynarrayhdr_t dynarray;

#define array_init(x,y) array_init_r(x,sizeof(y))
DYN_ARRAY_FUNCTION void array_init_r(dynarray *v, size_t elem_size)
{
	v->capacity = 0;
	v->data = 0;
	v->n_elements = 0;
	v->element_size = elem_size;
}

#define ARRAY_SIZE_MULTIPLIER(x) ( ( (x) * 100) / 66)
#define ARRAY_SIZE_BASE (10)

#define array_push(x,y) array_push_r(x,(char*)y)
DYN_ARRAY_FUNCTION void* array_push_r(dynarray *v, char *elem)
{
	if (!v->data || v->n_elements + 1 >= v->capacity)
	{
		int new_capacity = ARRAY_SIZE_MULTIPLIER(v->n_elements);
		if (new_capacity <= 0)
			new_capacity = ARRAY_SIZE_BASE;
		char *p = (char*)malloc(new_capacity * v->element_size);
		if (v->data)
		{
			memcpy(p, v->data, v->n_elements * v->element_size);
			free(v->data);
		}
		v->data = p;
		v->capacity = new_capacity;
	}

	char *element_ptr = &v->data[v->n_elements * v->element_size];
	memcpy(element_ptr, elem, v->element_size);
	++v->n_elements;
	return element_ptr;
	//printf("copying %d to %d size = %d\n", *(int*)elem, *(int*)element_ptr, v->element_size);
}

DYN_ARRAY_FUNCTION void array_resize(dynarray *v, size_t new_sz)
{
	if (!new_sz)
		return;

	char *p = (char*)malloc(new_sz * v->element_size);
	if (v->data && new_sz >= v->n_elements) //if we're shrinking the array we can't copy all of it, //TODO copy when shrinking aswell
	{
		memcpy(p, v->data, v->n_elements * v->element_size);
		free(v->data);
	}
	if (new_sz <= v->n_elements)
		v->n_elements = new_sz;
	v->data = p;
	v->capacity = new_sz;
}

#define array_value(x) *x
#define array_get(vec, type, var, ind) type* var = (type*)array_get_r(vec,ind)
DYN_ARRAY_FUNCTION char *array_get_r(dynarray *v, unsigned int i)
{
	if (i >= v->n_elements)
	{
		return NULL;
	}
	return &v->data[i * v->element_size];
}

DYN_ARRAY_FUNCTION void array_free(dynarray *v)
{
	free(v->data);
	array_init_r(v, 0);
}