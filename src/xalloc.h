#pragma once

#include <malloc.h>
#include <string.h>

#if 1
int __xmcheck();
void __xfree(void*,int,const char*);
void *__xmalloc(size_t,int,const char*);

#if 1
#define xmalloc malloc
#define xfree free
#define xmcheck(a)
#define xrealloc realloc
#else
#define xmalloc(x) __xmalloc(x,__LINE__,__FILE__)
#define xfree(x) __xfree(x,__LINE__,__FILE__)
#define xmcheck __xmcheck
#define xrealloc realloc
#endif
#endif