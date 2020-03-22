#pragma once

#include "stdheader.h"
#include "dynarray.h"

typedef struct {
	int original_loc, assigned_loc;
	int size;
	int flags;
	size_t numargs;
#define MAX_CODE_SEGMENT_DEPENDS 1024
	int relocations[MAX_CODE_SEGMENT_DEPENDS];
	int relocation_size;
	char id[256];
	int new_location;
	int needed_pops;
} code_segment_t;

typedef struct {
	int inuse;
	char *string;
	unsigned long hash;
} scr_istring_t;

typedef struct {
	int inuse;
	int name;
	int codepos;
	int numargs;
} scr_function_t;

typedef enum {
	PARSER_FLAG_NONE= 0,
	PARSER_FLAG_TOKENIZE_NEWLINE = (1 << 0),
	PARSER_FLAG_TOKENIZE_WHITESPACE = (1<<1),
} e_parser_flags_t;

typedef struct {
	int curpos;
	int prevpos;
	char *scriptbuffer;
	int scriptbuffersize;
	int lineno;
	bool execute;
	int flags;
	int *enabledtokens;
	//size_t numenabledtokens;
	var_t *local_vars[MAX_LOCAL_VARS]; //max of short

	int token;

	union {
		char string[4096];
		char character;
	};
	int stringindex;

	int integer;
	float number;

	int error;
	char errstr[128];

	int main_segment;

#define MAX_CODE_SEGMENTS 1024
	code_segment_t code_segments[MAX_CODE_SEGMENTS];
	int code_segment_size;

	char *program;
	int program_counter;
	code_segment_t *current_segment;

#define MAX_ISTRINGS 0xffff

	scr_istring_t istrings[MAX_ISTRINGS];

#define MAX_SCR_FUNCTIONS 1024

	scr_function_t functions[MAX_SCR_FUNCTIONS];

	int numistrings;
	int numfunctions;
	int not;
	void *userdata;
	int last_member_token, last_member_index;
#define SCOPE_STACK_SIZE (128)
	int scopestack[SCOPE_STACK_SIZE];
	int ssp; //scope stack pointer
	dynarray structs;

	FILE *logfile;

	int last_opcode_index;
	int internalflags;
} parser_t;

#define PARSER_IFLAG_STDCALL (1<<0)

#define SCOPE_POP(pp) pp->scopestack[--pp->ssp]
#define SCOPE_PUSH(pp, v) pp->scopestack[pp->ssp++] = v

static var_t *parser_find_local_variable(parser_t*, const char*);
static int parser_block(parser_t*,int,int);
static int parser_expression(parser_t*);