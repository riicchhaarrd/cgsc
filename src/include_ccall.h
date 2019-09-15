#ifndef INCLUDE_CCALL_H
#define INCLUDE_CCALL_H

#ifdef __cplusplus
	#define EXTERNC extern "C"
#else
	#define EXTERNC extern
#endif

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
	#define CCALL EXTERNC EMSCRIPTEN_KEEPALIVE
#else
	#if defined(_MSC_VER)
	#	define CCALL EXTERNC __declspec(dllexport)
	#else
	#	define CCALL EXTERNC __attribute__((visibility("default")))
	#endif
#endif

#endif