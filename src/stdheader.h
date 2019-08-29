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
