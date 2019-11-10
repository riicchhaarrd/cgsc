#pragma once

#include <malloc.h>

#define DYN_INLINE inline
#define DYN_STATIC static
#define DYN_STACK_FUNCTION DYN_INLINE

#define DYN_STACK_DATA_TYPE void*

typedef struct
{
	size_t size;
	size_t capacity;
	DYN_STACK_DATA_TYPE *data;
} dynstackhdr_t;

typedef dynstackhdr_t dynstack;

DYN_STACK_FUNCTION void stk_init(dynstack *st)
{
	st->size = 0;
	st->data = 0;
	st->capacity = 0;
}
//#define STACK_SIZE_MULTIPLIER(x) ( ( (x) * 100) / 66)
#define STACK_SIZE_MULTIPLIER(x) (x << 1)
#define STACK_SIZE_BASE (10)

#define stk_push(x,y) stk_push_r(x,(size_t)y)
DYN_STACK_FUNCTION void stk_push_r(dynstack *st, DYN_STACK_DATA_TYPE val)
{
	if (!st->data || st->size + 1 >= st->capacity)
	{
		size_t new_capacity = STACK_SIZE_MULTIPLIER(st->capacity);
		if (new_capacity <= 0)
			new_capacity = STACK_SIZE_BASE;
		char *p = (char*)malloc(new_capacity * sizeof(DYN_STACK_DATA_TYPE));
		if (st->data)
		{
			memcpy(p, st->data, st->size * sizeof(DYN_STACK_DATA_TYPE));
			free(st->data);
		}
		st->data = p;
		st->capacity = new_capacity;
	}
	st->data[st->size++] = val;
}

DYN_STACK_FUNCTION DYN_STACK_DATA_TYPE stk_pop(dynstack *st)
{
	if (st->size <= 0)
		return 0;
	return st->data[--st->size];
}

DYN_STACK_FUNCTION char *stk_get(dynstack *st, size_t i)
{
	if (i >= st->size)
		return 0;
	return st->data[i];
}

DYN_STACK_FUNCTION void stk_free(dynstack *st)
{
	free(st->data);
	stk_init(st);
}