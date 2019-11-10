#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <malloc.h>
#include <string.h>
#ifndef _WIN32
#define stricmp strcasecmp
#endif
extern int(*g_printf_hook)(const char *, ...);
#define vm_printf g_printf_hook

#if 1
#ifdef __EMSCRIPTEN__
#define VM_INLINE
#else
#ifdef _WIN32
#define VM_INLINE __forceinline inline
#else
#define VM_INLINE inline
#endif
#endif
#else
#define VM_INLINE static
#endif