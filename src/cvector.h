#ifndef VECTOR_H__
#define VECTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vector_ {
	void **data;
	int size;
	int count;
} vector;

//void vector_delete_element(vector *v, void *p);
//int vector_locate(vector*, void*);
void vector_init(vector*);
int vector_count(vector*);
void vector_add(vector*, void*);
void vector_set(vector*, int, void*);
void *vector_get(vector*, int);
void vector_delete(vector*, int);
void vector_free(vector*);

#ifdef __cplusplus
}
#endif
#endif