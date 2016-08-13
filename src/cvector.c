#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cvector.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VECTOR_GROWTH_MULTIPLIER 1.5

void vector_init(vector *v) {
	v->data = NULL;
	v->size = 0;
	v->count = 0;
}

int vector_count(vector *v) {
	return v->count;
}

void vector_add(vector *v, void *e) {
	if (v->size == 0) {
		v->size = 12;
		v->data = (void**)malloc(sizeof(void*) * v->size);
		memset(v->data, '\0', sizeof(void*) * v->size);
	}

	if (v->size == v->count) {
		v->size *= VECTOR_GROWTH_MULTIPLIER;
		v->data = (void**)realloc(v->data, sizeof(void*) * v->size);
	}

	v->data[v->count] = e;
	v->count++;
}

void vector_set(vector *v, int index, void *e) {
	if (index >= v->count) {
		return;
	}
	v->data[index] = e;
}

void *vector_get(vector *v, int index) {
	if (index >= v->count) {
		return NULL;
	}
	return v->data[index];
}

int vector_locate(vector *v, void *p) {
	for (int i = 0; i < v->count; i++) {
		if (v->data[i] == p)
			return i;
	}
	return -1;
}

void vector_delete_element(vector *v, void *p) {
	int loc = vector_locate(v, p);
	if (loc != -1)
		vector_delete(v, loc);
}

void vector_delete(vector *v, int index) {
	if (index >= v->count) {
		return;
	}

	for (int i = index + 1, j = index; i < v->count; i++) {
		v->data[j] = v->data[i];
		j++;
	}

	v->count--;
}

void vector_free(vector *v) {
	free(v->data);
	v->data = NULL;
	v->count = 0;
	v->size = 0;
}

#ifdef __cplusplus
}
#endif