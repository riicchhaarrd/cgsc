#pragma once
#include <malloc.h>

typedef struct {
	int capacity;
	int size;
	char buf[];
} dynstringhdr_t;

#define DYN_HDR(s) (dynstringhdr_t *)( ( s ) - sizeof(dynstringhdr_t) )

typedef char* dynstring;

#define DYN_INLINE inline
#define DYN_STATIC static

DYN_INLINE DYN_STATIC dynstring dynalloc(int n)
{
	dynstringhdr_t *d = (dynstringhdr_t*)malloc(sizeof(dynstringhdr_t) + n + 1);
	d->capacity = n;
	d->size = 0;
	return (dynstring)&d->buf[0];
}

DYN_INLINE DYN_STATIC dynstring dynnew(const char* s)
{
	int strsz = strlen(s);
	dynstringhdr_t *d = (dynstringhdr_t*)malloc(sizeof(dynstringhdr_t) + strsz + 1);
	d->capacity = strsz;
	d->size = d->capacity;
	memcpy(d->buf, s, strsz + 1);
	return d->buf;
}

DYN_INLINE DYN_STATIC void dynfree(dynstring *s)
{
	if (!s)
		return;
	if (!*s)
		return;
	dynstringhdr_t *hdr = DYN_HDR(*s);
	free(hdr);
	*s = NULL;
}

DYN_INLINE DYN_STATIC size_t dyncapacity(dynstring *s)
{
	dynstringhdr_t *hdr = DYN_HDR(*s);
	return hdr->capacity;
}

DYN_INLINE DYN_STATIC size_t dynlen(dynstring *s)
{
	dynstringhdr_t *hdr = DYN_HDR(*s);
	return hdr->size;
}

extern void dynadd(dynstring *s, const char *str);

DYN_INLINE DYN_STATIC void dynpush(dynstring *s, int i)
{
	dynstringhdr_t *hdr = 0;
	if(*s)
		hdr = DYN_HDR(*s);
	int newsize = 4;
	if (*s)
		newsize = hdr->size;

	if (!*s || newsize + 1 >= hdr->capacity)
	{
		dynstring n = dynalloc(newsize * 2 + 1);

		if (*s)
		{
			dynadd(&n, *s);
		}
		dynpush(&n, i);
		dynfree(s);
		*s = n;
	}
	else {
		hdr->buf[hdr->size] = i & 0xff;
		++hdr->size;
		hdr->buf[hdr->size] = '\0';
	}
}

DYN_INLINE DYN_STATIC void dynaddn(dynstring *s, const char *str, size_t n)
{
	int sl = strlen(str);
	if (n >= sl)
		n = sl;

	if (*s == NULL)
		*s = dynalloc(n);

	for (int i = 0; i < n; ++i)
	{
		dynpush(s, *(char*)(str + i));
	}
}

DYN_INLINE DYN_STATIC void dynadd(dynstring *s, const char *str)
{
	dynaddn(s, str, strlen(str));
}

DYN_INLINE DYN_STATIC void dynaddf(dynstring *s, const char *fmt, ...)
{
#define NBUF (10)
	static char buffer[NBUF][1024] = { 0 };
	static int bufIndex = 0;

	char *buf = &buffer[bufIndex++ % NBUF][0];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buffer[0]), fmt, args);
	dynadd(s, buf);
	va_end(args);
}