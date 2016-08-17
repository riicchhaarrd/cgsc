#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "xalloc.h"

#include "tokens.h"
#include "variable.h"

#define ENUM_BEGIN(typ) typedef enum {
#define ENUM(nam) nam
#define ENUM_END(typ) } typ;

#include "vm_opcodes.h"

#undef ENUM_BEGIN
#undef ENUM
#undef ENUM_END
#define ENUM_BEGIN(typ) static const char * typ ## _strings[] = {
#define ENUM(nam) #nam
#define ENUM_END(typ) };
#include "vm_opcodes.h"

#include "parser.h"

#ifndef _WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#define next (pp->curpos >= pp->scriptbuffersize ? 0 : pp->scriptbuffer[pp->curpos++])
#define next_chk(x) ( ( (pp->scriptbuffer[pp->curpos] == x) ? pp->scriptbuffer[pp->curpos++] : pp->scriptbuffer[pp->curpos] ) == x)

static int parser_append_to_parse_string(parser_t *pp, int c) {
#if 0
	if (!c)
		return 0;
#endif
	if (pp->stringindex >= sizeof(pp->string)) {
		printf("keyword string index overflow\n");
		return 0;
	}
	pp->string[pp->stringindex++] = c;
	return 1;
}

static int parser_read_string(parser_t *pp) {
	bool is_char = (pp->scriptbuffer[pp->curpos - 1] == '\'' ? true : false);
	memset(pp->string, 0, sizeof(pp->string));
	pp->stringindex = 0;
	int ch;
	while (1) {
		ch = next;

		if (!ch)
			break;

		if (is_char && ch == '\'')
			break;
		else if (ch == '"')
			break;

		if (ch == '\\') { //maybe fix this with actual stuff?
			switch (ch = next) {
			case 'n':
				ch = '\n';
				break;
			case 't':
				ch = '\t';
				break;
			case '"':
				ch = '"';
				break;
			case '\'':
				ch = '\'';
				break;
			default:
				if (isdigit(ch)) {
					ch -= '0';
				}
				break;
			}
		}
		if (!parser_append_to_parse_string(pp, ch))
			break;
	}
	return is_char ? TK_CHAR : TK_STRING;
}


static int parser_read_identifier(parser_t *pp) {
	memset(pp->string, 0, sizeof(pp->string));
	pp->stringindex = 0;
	--pp->curpos;
	int ch;
	while (isalpha((ch = next)) || isdigit(ch) || ch == '_') {
		if (!parser_append_to_parse_string(pp, tolower(ch)))
			break;
	}
	--pp->curpos;
	if (!strcmp(pp->string, "if"))
		return TK_IF;
	else if (!strcmp(pp->string, "push"))
		return TK_R_PUSH;
	else if (!strcmp(pp->string, "pop"))
		return TK_R_POP;
	else if (!strcmp(pp->string, "break"))
		return TK_R_BREAK;
	else if (!strcmp(pp->string, "else"))
		return TK_R_ELSE;
	else if (!strcmp(pp->string, "foreach"))
		return TK_R_FOREACH;
	else if (!strcmp(pp->string, "while"))
		return TK_WHILE;
	else if (!strcmp(pp->string, "for"))
		return TK_R_FOR;
	else if (!strcmp(pp->string, "var"))
		return TK_VAR;
	else if (!strcmp(pp->string, "return"))
		return TK_RETURN;
	else if (!strcmp(pp->string, "thread"))
		return TK_R_THREAD;
	else if (!strcmp(pp->string, "notify"))
		return TK_NOTIFY;
	else if (!strcmp(pp->string, "endon"))
		return TK_ENDON;
	else if (!strcmp(pp->string, "waittill"))
		return TK_WAITTILL;
	else if (!strcmp(pp->string, "wait"))
		return TK_R_WAIT;
	else if (!strcmp(pp->string, "true"))
		return TK_R_TRUE;
	else if (!strcmp(pp->string, "false"))
		return TK_R_FALSE;
	else if (!stricmp(pp->string, "null"))
		return TK_R_NULL;
	else if (!stricmp(pp->string, "undefined"))
		return TK_R_NULL;
	else if (!stricmp(pp->string, "new"))
		return TK_R_NEW;
	return TK_IDENT;
}

static int parser_read_number(parser_t *pp) {
	char str[128] = { 0 };
	int stri = 0;
	int ch = pp->scriptbuffer[pp->curpos - 1];
	int negative = 1;
	if (ch == '-') {
		negative = -1;
		ch = next;
	}
	bool is_ref = false;
	if (ch == '$')
		is_ref = true;
	else
		--pp->curpos;
	bool is_int = true;
	while ((ch = next)) {
		if (!isdigit(ch) && ch != '.')
			break;
		if (ch == '.')
			is_int = false;
		str[stri++] = ch;
	}
	--pp->curpos;
	if (is_int)
		pp->integer = atoi(str) * negative;
	else {
		pp->number = atof(str) * negative;
		//ipr->integer = ipr->number;//truncate to int anyway
	}

	if (is_ref)
		return TK_REF;

	if (!is_int)
		return TK_FLOAT;
	return TK_INT;
}

static int parser_read_number_hex(parser_t *pp) {
	memset(pp->string, 0, sizeof(pp->string));
	pp->stringindex = 0;
	--pp->curpos;
	int ch;
	while (isalpha((ch = next)) || isdigit(ch)) {
		if (!parser_append_to_parse_string(pp, tolower(ch)))
			break;
	}
	--pp->curpos;
	int number = (int)strtol(pp->string, NULL, 16);
	pp->integer = number;
	return TK_INT;
}

static int parser_read_next_token(parser_t *pp) {
	unsigned char ch;
scan:
	switch ((ch = next)) {
	case '\t':
	case ' ':
		goto scan;
	case '\r':
	case '\n':
		++pp->lineno;
		goto scan;
		break;
	case '\\':
		return TK_BACKSLASH;
	case '(': return TK_LPAREN;
	case ')': return TK_RPAREN;
	case '=':
		if (next_chk('='))
			return TK_EQUALS;
		return TK_ASSIGN;
	case '>':
		if (next_chk('='))
			return TK_GEQUAL;
		return TK_GREATER;
	case '<':
		if (next_chk('='))
			return TK_LEQUAL;
		return TK_LESS;
	case '+':
		if (next_chk('='))
			return TK_PLUS_ASSIGN;
		else if (next_chk('+'))
			return TK_PLUS_PLUS;
		return TK_PLUS;
	case '0':
		if (pp->scriptbuffer[pp->curpos] != 'x')
			goto get_num;
		return parser_read_number_hex(pp);
	case '-':
#if 1
		if (isdigit(pp->scriptbuffer[pp->curpos]))//if (isdigit(pp->scriptbuffer[pp->curpos + 1]))
			goto get_num;
#endif
		if (next_chk('='))
			return TK_MINUS_ASSIGN;
		else if (next_chk('-'))
			return TK_MINUS_MINUS;
		else if (next_chk('>'))
			return TK_MEMBER_SEPERATOR;
		return TK_MINUS;
	case '&':
		if (next_chk('&'))
			return TK_AND_AND;
		return TK_AND;
	case '|':
		if (next_chk('|'))
			return TK_OR_OR;
		return TK_OR;
	case '%': return TK_MODULO;
	case ',': return TK_COMMA;
	case ':': return TK_COLON;
	case ';': return TK_SEMICOLON;
	case '.':
		if(isdigit(pp->scriptbuffer[pp->curpos]))//if (isdigit(pp->scriptbuffer[pp->curpos + 1]))
			goto get_num;
		return TK_DOT;
	case '[': return TK_LBRACK;
	case ']': return TK_RBRACK;
	case '{': return TK_LBRACE;
	case '}': return TK_RBRACE;
	case '!':
		if (next_chk('='))
			return TK_NOTEQUAL;
		return TK_NOT;
	
	case '$':
		if (isdigit(pp->scriptbuffer[pp->curpos + 1]))
			goto get_num;
		return TK_DOLLAR;

	case '/':
		if (next_chk('/')) {
			while (ch && ch != '\n') {
				ch = next;
			}
			goto scan;

		}
		else if (next_chk('='))
			return TK_DIVIDE_ASSIGN;
		return TK_DIVIDE;
	case '*':
		if (next_chk('='))
			return TK_MULTIPLY_ASSIGN;
		return TK_MULTIPLY;
	case '\'':
	case '"':
		return parser_read_string(pp);
	default:
		if (isdigit(ch)) {
		get_num:
			return parser_read_number(pp);
		}
		else if (isalpha(ch) || ch == '_') {
			int ident = parser_read_identifier(pp);
			if (ident == TK_R_TRUE || ident == TK_R_FALSE) {
				pp->integer = (ident == TK_R_TRUE) ? 1 : 0;
				return TK_INT;
			}
			return ident;
		}
		return TK_ILLEGAL;

	case 0:
		return TK_EOF;
	}
	return TK_ILLEGAL;
}

#define pp_accept(x,y) (parser_accept_token(x,y))
#define pp_undo() (parser_undo())

static void parser_undo(parser_t *pp) {
	pp->curpos = pp->prevpos;
}

static int parser_accept_token(parser_t *pp, int token) {
	pp->prevpos = pp->curpos;
	int next_token = parser_read_next_token(pp);
	pp->token = next_token;
	//printf("pp_accept(%s), next_tkn = %s\n", lex_token_strings[token], lex_token_strings[next_token]);
	if (next_token == token) {
		return 1;
	}
	pp->curpos = pp->prevpos;
	return 0;
}

void parser_display_history(parser_t *pp) {
	int start_history;
#define HISTORY_LEN 30
	start_history = pp->curpos - HISTORY_LEN;
	if (start_history < 0)
		start_history = 0;
	int end_history = pp->curpos + HISTORY_LEN;

	if (end_history > pp->scriptbuffersize)
		end_history = pp->scriptbuffersize;

	for (int i = start_history; i < end_history; i++) {
		if (i == pp->curpos + 1)
			putchar('*');
		putchar(pp->scriptbuffer[i]);
		if (i == pp->curpos + 1)
			putchar('*');
	}
	putchar('\n');
}

#define pp_expect(x,y) (parser_expect_token(x,y))
static int parser_expect_token(parser_t *pp, int token) {
	if (parser_accept_token(pp, token))
		return 1;
	pp->error = 1;

	parser_display_history(pp);
	printf("error: unexpected symbol! expected %s, got %s\n", lex_token_strings[token], lex_token_strings[pp->token]);
	return 0;
}

static int parser_locate_token(parser_t *pp, int token, int *at_position, int block_start_tk) {

	int stored_pos = pp->curpos;
	int save_iterate_token_pos = stored_pos;
	int tk = parser_read_next_token(pp);

	int num_block_start_tokens = 1;
	int num_block_end_tokens = 0;

	while (num_block_end_tokens < num_block_start_tokens) {
		if (tk == TK_EOF)
			return TK_EOF;

		if (block_start_tk != TK_ILLEGAL) {
			if (tk == block_start_tk)
				++num_block_start_tokens;
		}

		if (tk == token) {
			if (block_start_tk == TK_ILLEGAL)
				break;
			else
				++num_block_end_tokens;
		}
		save_iterate_token_pos = pp->curpos;
		tk = parser_read_next_token(pp);
	}
	*at_position = save_iterate_token_pos;
	pp->curpos = stored_pos;
	return token;
}

static int parser_find_indexed_string(parser_t *pp, const char *str, scr_istring_t **out) {
	scr_istring_t *istr = NULL;
	*out = istr;
	for (int i = 0; i < MAX_ISTRINGS; i++) {
		istr = &pp->istrings[i];
		if (!istr->inuse)
			continue;
		if (!strcmp(istr->string, str)) {
			*out = istr;
			return i;
		}
	}
	return 0;
}

static int parser_create_indexed_string(parser_t *pp, const char *str) {
	size_t sz = strlen(str);
	char *new_str = (char*)xmalloc(sz + 1);
	strncpy(new_str, str, sz);
	new_str[sz] = '\0';
	int i;
	for (i = 0; i < MAX_ISTRINGS; i++) {
		if (!pp->istrings[i].inuse)
			break;
	}

	if (i == MAX_ISTRINGS) {
		//error max strings
		printf("MAX_ISTRINGS\n");
		return 0;
	}
	++pp->numistrings;
	pp->istrings[i].inuse = true;
	pp->istrings[i].string = new_str;
	return i;
}

static void parser_free_indexed_string(parser_t *pp, int index) {
	if (index > 0xffff) {
		printf("too high m8\n");
		return;
	}

	if (!pp->istrings[index].inuse)
		return;

	--pp->numistrings;
	xfree(pp->istrings[index].string);
	pp->istrings[index].string = NULL;
	pp->istrings[index].inuse = false;
}

static void parser_init(parser_t *pp) {

	memset(pp, 0, sizeof(parser_t));
	
	pp->execute = true;
	pp->curpos = 0;
	pp->scriptbuffer = NULL;
	pp->scriptbuffersize = 0;
	pp->lineno = 0;
	pp->token = TK_ILLEGAL;
	*pp->string = 0;
	pp->stringindex = 0;

	pp->integer = 0;
	pp->number = 0;

	pp->error = 0;
	pp->errstr[0] = 0;

	pp->not = 0;
}

static void parser_cleanup(parser_t *pp) {
	xfree(pp->scriptbuffer);
	pp->scriptbuffer = NULL;

	for (int i = 0; i < MAX_ISTRINGS; i++)
		parser_free_indexed_string(pp, i);

	xfree(pp);
}

//#define add_opcode(op) (pp->program[pp->program_counter++]=(int)op)

static void program_add_opcode(parser_t *pp, uint8_t opcode) {
	if (!pp->execute)
		return;

	pp->program[pp->program_counter++] = opcode;
	const char *opcode_str = opcode < OP_END_OF_LIST ? e_opcodes_strings[opcode] : "[not opcode]";
	//printf("program_add_opcode(%s) => %d (0x%x)\n", opcode_str, opcode, opcode);
}

static void program_add_int(parser_t *pp, uint32_t i) {
	if (!pp->execute)
		return;

#if 0
	pp->program[pp->program_counter++] = i & 0xff;
	pp->program[pp->program_counter++] = (i >> 8) & 0xff;
	pp->program[pp->program_counter++] = (i >> 16) & 0xff;
	pp->program[pp->program_counter++] = (i >> 24) & 0xff;
#else
	*(int*)(pp->program + pp->program_counter) = i;
	pp->program_counter += sizeof(int);
#endif
	//printf("program_add_int(%d) => %x, %x, %x, %x\n", i, i & 0xff, (i>>8)&0xff,(i>>16)&0xff,(i>>24)&0xff);
}

static void program_add_float(parser_t *pp, float i) {
	if (!pp->execute)
		return;

#if 0
	pp->program[pp->program_counter++] = i & 0xff;
	pp->program[pp->program_counter++] = (i >> 8) & 0xff;
	pp->program[pp->program_counter++] = (i >> 16) & 0xff;
	pp->program[pp->program_counter++] = (i >> 24) & 0xff;
#else
	*(float*)(pp->program + pp->program_counter) = i;
	pp->program_counter += sizeof(float);
#endif
	//printf("program_add_float(%f)\n", i);
}

static void program_add_short(parser_t *pp, int i) {
	program_add_int(pp, i);
	//int might be faster cuz lol
#if 0
	if (!pp->execute)
		return;

	*(short*)(pp->program + pp->program_counter) = i;
	pp->program_counter += sizeof(short);
	printf("program_add_short(%d) => %x, %x\n", i, i & 0xff, (i >> 8) & 0xff);
#endif
}

static var_t *parser_create_local_variable(parser_t *pp);
static int parser_variable(parser_t *pp, const char *id, bool load, bool create_if_not_exist, var_t **out) {
	if(out)
	*out = NULL;

	if (!strcmp(id, "level") || !strcmp(id, "self")) {
		program_add_opcode(pp, strcmp(id, "level") ? OP_GET_SELF : OP_GET_LEVEL);
	}
	else {

		var_t *v = parser_find_local_variable(pp, id);
		if(out)
		*out = v;

		if (!v) {
			if (!create_if_not_exist) {
				printf("variable '%s' does not exist!\n", id);
				return 1;
			}
			else {
				v = parser_create_local_variable(pp);
				strncpy(v->name, pp->string, sizeof(v->name) - 1);
				v->name[sizeof(v->name) - 1] = '\0';
			}
		}
		else {
			if (load) {
				program_add_opcode(pp, OP_LOAD);
				program_add_short(pp, v->index);
			}
		}
	}

	return 0;
}

/* TODO rewrite this stuff into e.g factor push stuff and then e.g TK_ASSIGN push the STORE_ stuff with states or so */

static int parser_factor(parser_t *pp) {
	if (pp_accept(pp, TK_NOT)) {
		++pp->not;
		return 0;
	} else if(pp_accept(pp,TK_COLON)) {
		if (!pp_expect(pp, TK_COLON)) return 1;
		if (!pp_expect(pp, TK_IDENT)) return 1;

		bool found = false;
		for (int i = 0; i <= pp->code_segment_size; i++) {
			code_segment_t *seg = &pp->code_segments[i];
			if (!strcmp(seg->id, pp->string)) {
				found = true;
				program_add_opcode(pp, OP_PUSH_FUNCTION_POINTER);
				pp->current_segment->relocations[pp->current_segment->relocation_size++] = pp->program_counter;
				program_add_int(pp, seg->original_loc);
				break;
			}
		}

		if (!found) {
			printf("function '%s' was not found\n", pp->string);
			return 1;
		}
	} else if (pp_accept(pp, TK_INT)) {
		program_add_opcode(pp, OP_PUSH);
		program_add_int(pp, pp->integer);
	} else if (pp_accept(pp, TK_LBRACK)) {
		if (pp_accept(pp, TK_RBRACK)) {
			program_add_opcode(pp, OP_PUSH_ARRAY);
			program_add_short(pp, 0);
		}
		else {
			int arr_len = 0;
			do {
				++arr_len;
				if (parser_expression(pp))
					return 1;
			} while (pp_accept(pp, TK_COMMA));
			program_add_opcode(pp, OP_PUSH_ARRAY);
			program_add_short(pp, arr_len);
			pp_expect(pp, TK_RBRACK);
		}
	} else if (pp_accept(pp, TK_R_NEW)) {
		if (!pp_expect(pp, TK_IDENT))
			return 1;
		if (!stricmp(pp->string, "object") || !stricmp(pp->string, "obj")) {
			program_add_opcode(pp, OP_PUSH_OBJECT);
		}
		else if (!stricmp(pp->string, "array")) {
			program_add_opcode(pp, OP_PUSH_ARRAY);
			program_add_short(pp, 0);
		}
		else
			return 1;
	} else if(pp_accept(pp,TK_R_NULL)) {
		program_add_opcode(pp, OP_PUSH_NULL);
	} else if (pp_accept(pp, TK_FLOAT)) {
		program_add_opcode(pp, OP_PUSHF);
		program_add_float(pp, pp->number);
	}
	else if (pp_accept(pp, TK_STRING)) {
		program_add_opcode(pp, OP_PUSH_STRING);
		scr_istring_t *istr = NULL;
		int index = parser_find_indexed_string(pp, pp->string, &istr);

		if (istr == NULL)
			index = parser_create_indexed_string(pp, pp->string);

		program_add_int(pp, index); //the index of string
	} else if(pp_accept(pp,TK_IDENT)) {
		char id[128] = { 0 };
		snprintf(id, sizeof(id) - 1, "%s", pp->string);

		if (pp_accept(pp, TK_LPAREN)) {
				
				int numargs = 0;

				if (pp_accept(pp, TK_RPAREN))
					goto no_args_lol;
				do {
					++numargs;
					if (parser_expression(pp)) //auto pushes
						return 1;
				} while (pp_accept(pp, TK_COMMA));

				pp_expect(pp, TK_RPAREN);

			no_args_lol:

				//printf("function call with RETURN !!!! %s()\n", id);
				{
					bool found = false;
					for (int i = 0; i <= pp->code_segment_size; i++) {
						code_segment_t *seg = &pp->code_segments[i];
						if (!strcmp(seg->id, id)) {
							found = true;
							program_add_opcode(pp, OP_CALL);
							pp->current_segment->relocations[pp->current_segment->relocation_size++] = pp->program_counter;
							program_add_int(pp, seg->original_loc);
							program_add_int(pp, numargs);
							break;
						}
					}

					if (!found) {

						//just add it to the 'builtin' funcs and maybe that works? :D
						//printf("function '%s' does not exist!\n", id);
						program_add_opcode(pp, OP_CALL_BUILTIN);

						scr_istring_t *istr = NULL;
						int index = parser_find_indexed_string(pp, id, &istr);

						if (istr == NULL)
							index = parser_create_indexed_string(pp, id);
						program_add_int(pp, index); //the index of string
						program_add_int(pp, numargs);
					}
				}
		} else {

			if (parser_variable(pp, pp->string, true, false, NULL))
				return 1;

			while (1) {
				if (pp_accept(pp, TK_DOT)) {
					do {
						if (!pp_expect(pp, TK_IDENT))
							return 1;
						if (stricmp(pp->string, "size")) {
							scr_istring_t *istr = NULL;
							int index = parser_find_indexed_string(pp, pp->string, &istr);

							if (istr == NULL)
								index = parser_create_indexed_string(pp, pp->string);
							program_add_opcode(pp, OP_LOAD_FIELD);
							program_add_short(pp, index);
						}
						else
							program_add_opcode(pp, OP_GET_LENGTH);
					} while (pp_accept(pp, TK_DOT));
				}
				else if (pp_accept(pp, TK_LBRACK)) {
					if (parser_expression(pp))
						return 1;
					program_add_opcode(pp, OP_LOAD_ARRAY_INDEX);
					pp_expect(pp, TK_RBRACK);
				}
				else
					break;
			}
		}
	} else if(pp_accept(pp,TK_LPAREN)) {
		int lparen_savepos = pp->curpos;

		if (parser_expression(pp))
			return 1;

		if (pp_accept(pp, TK_COMMA)) {
			int i = 0; 
			do {
				if (parser_expression(pp))
					return 1;
				++i;
			} while (pp_accept(pp, TK_COMMA));
			if (i != 2) {
				printf("invalid vector!\n");
				return 1;
			}
			program_add_opcode(pp, OP_PUSH_VECTOR);
		}


		pp_expect(pp, TK_RPAREN);
	} else {
		printf("factor: syntax error got %s\n", lex_token_strings[pp->token]);
		return 1;
	}

	for (int i = 0; i < pp->not; i++)
		program_add_opcode(pp, OP_NOT);
	pp->not = 0;

	return 0;
}

static int parser_term(parser_t *pp) {

	if (pp_accept(pp, TK_MINUS))
		program_add_opcode(pp, OP_DEC);

	if (parser_factor(pp))
		return 1;

	while (pp_accept(pp, TK_DIVIDE) || pp_accept(pp, TK_MULTIPLY)) {
		int tk = pp->token;

		if (pp_accept(pp, TK_MINUS))
			program_add_opcode(pp, OP_DEC);

		if (parser_factor(pp))
			return 1;

		if (tk == TK_MULTIPLY)
			program_add_opcode(pp, OP_MUL);
		else if (tk == TK_DIVIDE)
			program_add_opcode(pp, OP_DIV);

	}
	return 0;
}

static int parser_expression(parser_t *pp) {

	if (parser_term(pp))
		return 1;

	while (1) {
		if (!pp_accept(pp, TK_PLUS) && !pp_accept(pp, TK_MODULO) && !pp_accept(pp, TK_MINUS) && !pp_accept(pp,TK_OR) && !pp_accept(pp,TK_AND))
			break;

		int tk = pp->token;
	
		if (parser_term(pp))
			return 1;

		if (tk == TK_PLUS)
			program_add_opcode(pp, OP_ADD);
		else if (tk == TK_MINUS)
			program_add_opcode(pp, OP_SUB);
		else if (tk == TK_OR)
			program_add_opcode(pp, OP_OR);
		else if (tk == TK_AND)
			program_add_opcode(pp, OP_AND);
		else if (tk == TK_MODULO)
			program_add_opcode(pp, OP_MOD);
	}

	return 0;
}

/*
typedef struct {
	int type;
	int modif_location;
	int jump_location;
} case_jump_t;

int parser_condition(parser_t *pp, case_jump_t *jumps, int *num_jumps) {
	int sep_tk = TK_OR_OR;

	int numjumps = 0;

	do {
		if (pp->token == TK_OR_OR || pp->token == TK_AND_AND)
			sep_tk = pp->token;

		if (pp_accept(pp, TK_NOT)) {
			if (parser_expression(pp))
				return 1;
			program_add_opcode(pp, OP_NOT);
		} else {
			if (parser_expression(pp))
				return 1;

			if (pp_accept(pp, TK_NOTEQUAL) || pp_accept(pp, TK_GEQUAL) || pp_accept(pp, TK_LEQUAL) || pp_accept(pp, TK_EQUALS) || pp_accept(pp, TK_LESS) || pp_accept(pp, TK_GREATER)) {
				int tk = pp->token;

				if (parser_expression(pp))
					return 1;

				jumps[numjumps].jump_location = pp->program_counter;

				if (tk == TK_GEQUAL)
					program_add_opcode(pp, OP_JLE);
				else if (tk == TK_LEQUAL)
					program_add_opcode(pp, OP_JGE);
				else if (tk == TK_EQUALS)
					program_add_opcode(pp, OP_JNE);
				else if (tk == TK_GREATER)
					program_add_opcode(pp, OP_JL);
				else if (tk == TK_LESS)
					program_add_opcode(pp, OP_JG);
				else if (tk == TK_NOTEQUAL)
					program_add_opcode(pp, OP_JE);

				jumps[numjumps].type = sep_tk;
				jumps[numjumps].modif_location = pp->program_counter;
				++numjumps;
				program_add_int(pp, 0); //temporary location
			}
		}
	} while (pp_accept(pp, TK_AND_AND) || pp_accept(pp, TK_OR_OR));
	
	*num_jumps = numjumps;

	return 0;
}
*/

/* do not use rofl */

static int parser_condition(parser_t *pp) {
	int sep_tk = TK_OR_OR;

	do {
		if (pp->token == TK_OR_OR || pp->token == TK_AND_AND)
			sep_tk = pp->token;

		if (pp_accept(pp, TK_NOT)) {
			if (parser_expression(pp))
				return 1;
			program_add_opcode(pp, OP_NOT);
		}
		else {
			if (parser_expression(pp))
				return 1;

			if (pp_accept(pp, TK_NOTEQUAL) || pp_accept(pp, TK_GEQUAL) || pp_accept(pp, TK_LEQUAL) || pp_accept(pp, TK_EQUALS) || pp_accept(pp, TK_LESS) || pp_accept(pp, TK_GREATER)) {
				int tk = pp->token;

				if (parser_expression(pp))
					return 1;

				if (tk == TK_GEQUAL)
					program_add_opcode(pp, OP_JLE);
				else if (tk == TK_LEQUAL)
					program_add_opcode(pp, OP_JGE);
				else if (tk == TK_EQUALS)
					program_add_opcode(pp, OP_JNE);
				else if (tk == TK_GREATER)
					program_add_opcode(pp, OP_JL);
				else if (tk == TK_LESS)
					program_add_opcode(pp, OP_JG);
				else if (tk == TK_NOTEQUAL)
					program_add_opcode(pp, OP_JE);

				program_add_int(pp, 0); //temporary location
			}
		}
	} while (pp_accept(pp, TK_AND_AND) || pp_accept(pp, TK_OR_OR));

	return 0;
}

static var_t *parser_create_local_variable(parser_t *pp) {
	var_t *v = NULL;
	for (int i = 0; i < MAX_LOCAL_VARS; i++) {
		v = pp->local_vars[i];
		if (v == NULL) {
			v = (var_t*)xmalloc(sizeof(var_t));
			v->value.type = VAR_TYPE_NULL;
			v->index = i;
			pp->local_vars[i] = v;
			return v;
		}
	}
	printf("MAX LOCAL VARIABLES REACHED!!!\n");
	return NULL;
}

static void parser_clear_local_variables(parser_t *pp) {
	var_t *v;
	for (int i = 0; i < MAX_LOCAL_VARS; i++) {
		v = pp->local_vars[i];
		if (!v)
			continue;

		xfree(v);
		pp->local_vars[i] = NULL;
	}
}

static var_t *parser_find_local_variable(parser_t *pp, const char* n) {
	var_t *v;
	for (int i = 0; i < MAX_LOCAL_VARS; i++) {
		v = pp->local_vars[i];
		if (!v)
			continue;

		if (!strcmp(v->name, n)) {
			return v;
		}
	}
	return NULL;
}

static int parser_encapsulated_block(parser_t *pp) {
	int at;
	if (TK_EOF == parser_locate_token(pp, TK_RBRACE, &at, TK_LBRACE)) {
		printf("BLOCK: could not find TK_RBRACE\n");
		return 1;
	}
	else {
		int start = pp->curpos;
		int end = at - 2;
		int block = parser_block(pp, start, end);
		if (block)
			return block;
	}
	return 0;
}

static int parser_statement(parser_t *pp) {
	bool is_thread_call = false;

	if (pp_accept(pp, TK_EOF)) {
		return 1;
	} else if(pp_accept(pp,TK_SEMICOLON)) {
		return 0;
	} else if (pp_accept(pp, TK_LBRACE)) {
		if (parser_encapsulated_block(pp))
			goto unexpected_tkn;
	} else if(pp_accept(pp,TK_R_WAIT)) {
		if (parser_expression(pp))
			return 1;

		program_add_opcode(pp, OP_WAIT);
		pp_expect(pp, TK_SEMICOLON);
	} else if(pp_accept(pp,TK_IF) || pp_accept(pp,TK_WHILE) || pp_accept(pp,TK_R_FOR)) {
		int tk = pp->token;

		pp_expect(pp, TK_LPAREN);

		if (tk == TK_R_FOR) {
			if (parser_statement(pp))
				return 1;
		}

		int cond_pos=pp->program_counter;
		
		do {
			if (parser_expression(pp))
				return 1;

			if (pp_accept(pp, TK_NOTEQUAL) || pp_accept(pp, TK_GEQUAL) || pp_accept(pp, TK_LEQUAL) || pp_accept(pp, TK_EQUALS) || pp_accept(pp, TK_LESS) || pp_accept(pp, TK_GREATER)) {
				int ctkn = pp->token;

				if (parser_expression(pp))
					return 1;
				if(ctkn == TK_EQUALS)
					program_add_opcode(pp, OP_EQ);
				else if (ctkn == TK_NOTEQUAL)
					program_add_opcode(pp, OP_NEQ);
				else if (ctkn == TK_GEQUAL)
					program_add_opcode(pp, OP_GEQUAL);
				else if (ctkn == TK_LEQUAL)
					program_add_opcode(pp, OP_LEQUAL);
				else if (ctkn == TK_LESS)
					program_add_opcode(pp, OP_LESS);
				else if (ctkn == TK_GREATER)
					program_add_opcode(pp, OP_GREATER);
			}
			else {
				program_add_opcode(pp, OP_PUSH);
				program_add_int(pp, 1);
				program_add_opcode(pp, OP_GEQUAL);
			}

		} while (pp_accept(pp, TK_AND_AND) || pp_accept(pp, TK_OR_OR));

		program_add_opcode(pp, OP_JUMP_ON_FALSE);
		int from_jmp_relative = pp->program_counter;
		program_add_short(pp, 0); //temporarily

		int tmp_at_this_loc = 0;
		if (tk == TK_R_FOR) {
			pp->execute = false;
			pp_accept(pp, TK_SEMICOLON);
			tmp_at_this_loc = pp->curpos;
			if (parser_statement(pp))
				return 1;
			pp->execute = true;
		}

		pp_expect(pp, TK_RPAREN);

		if (pp_accept(pp, TK_LBRACE)) {
			if (parser_encapsulated_block(pp))
				goto unexpected_tkn;
		}
		else {
			if (parser_statement(pp))
				return 1;
		}
		if (tk == TK_R_FOR) {
			int svps = pp->curpos;
			pp->curpos = tmp_at_this_loc;
			if (parser_statement(pp))
				return 1;
			pp->curpos = svps;
		}


		if (tk == TK_WHILE || tk == TK_R_FOR) {
			program_add_opcode(pp, OP_JUMP_RELATIVE);
			program_add_short(pp, (cond_pos - pp->program_counter - sizeof(int)));
		}

		int end_cond_pos = pp->program_counter;
		*(int*)(pp->program + from_jmp_relative) = (end_cond_pos - from_jmp_relative - sizeof(int));

		if (pp_accept(pp, TK_R_ELSE) && tk == TK_IF) {
			program_add_opcode(pp, OP_JUMP_ON_TRUE);
			from_jmp_relative = pp->program_counter;
			program_add_short(pp, 0); //temporarily
			if (pp_accept(pp, TK_LBRACE)) {
				if (parser_encapsulated_block(pp))
					goto unexpected_tkn;
			}
			else {
				if (parser_statement(pp))
					return 1;
			}
			end_cond_pos = pp->program_counter;
			*(int*)(pp->program + from_jmp_relative) = (end_cond_pos - from_jmp_relative - sizeof(int));
		}
#if 0
	} else if(pp_accept(pp,TK_WHILE)) {

		int tk = pp->token;

		pp_expect(pp, TK_LPAREN);

		int cond_start = pp->program_counter;

		int num_jumps = 0;
		case_jump_t *jumps = (case_jump_t*)malloc(sizeof(case_jump_t) * 256); //max OR AND cases? :D
		memset(jumps, 0, sizeof(case_jump_t) * 256);

		if (parser_condition(pp, jumps, &num_jumps)) {
			free(jumps);//free em all!
			goto unexpected_tkn;
		}
		
		pp_expect(pp, TK_RPAREN);
		pp_expect(pp, TK_LBRACE);

		int at;

		if (TK_EOF == parser_locate_token(pp, TK_RBRACE, &at, TK_LBRACE)) {
			printf("could not find rbrace!\n");
			goto unexpected_tkn;
		} else {
			int start = pp->curpos;
			int end = at - 2;
			int block = parser_block(pp, start, end);

			if (block) {
				printf("BLOCK=%d,curpos=%d,at=%d\n", block, start, end);
				return block;
			}

			if (tk == TK_WHILE) {
				program_add_opcode(pp, OP_JMP);
				program_add_int(pp, cond_start);
			}
			//set all the temporary jumps (0) to the actual over skipping location
			printf("num_jumps=%d\n", num_jumps);
			for (int i = 0; i < num_jumps; i++) {
				printf("jump [type: %s], modif_loc=%d, jmp_to=%d\n", lex_token_strings[jumps[i].type], jumps[i].modif_location, jumps[i].jump_location);
#if 1
				if(jumps[i].type==TK_AND_AND)
					*(int*)(pp->program + jumps[i].modif_location) = pp->program_counter;
				else {
					if (i > 1 && i < num_jumps)
						*(int*)(pp->program + jumps[i].modif_location) = jumps[i + 1].jump_location;
					else
						*(int*)(pp->program + jumps[i].modif_location) = pp->program_counter;
				}
#else
				*(int*)(pp->program + jumps[i].modif_location) = pp->program_counter;
#endif
			}
			free(jumps); //free 'em all! memory leakomon
		}
#endif
	} else if (pp_accept(pp, TK_INT)) {
		pp->current_segment->size = pp->program_counter - pp->current_segment->original_loc;
		pp->current_segment = &pp->code_segments[++pp->code_segment_size];
		pp->current_segment->original_loc = pp->program_counter;
		pp->current_segment->assigned_loc = pp->integer;

		snprintf(pp->current_segment->id, sizeof(pp->current_segment->id) - 1, "label_%d", pp->integer);

		pp_expect(pp, TK_COLON);
	} else if(pp_accept(pp, TK_R_THREAD)) {
		is_thread_call = true;
		pp_accept(pp,TK_IDENT);
		goto accept_ident;
	}
	else if (pp_accept(pp, TK_IDENT)) {

		char id[128] = { 0 };
		char id_path[128] = { 0 };
	
	accept_ident:
	
		snprintf(id,sizeof(id),"%s",pp->string);
		snprintf(id_path,sizeof(id),"%s",id);

		bool is_file_ref = false;

		while (1) {
			if (!pp_accept(pp, TK_BACKSLASH))
				break;
			is_file_ref = true;
			if (!pp_accept(pp, TK_IDENT))
				return 1;
			strncat(id_path, "/", sizeof(id) - strlen(id) - 1);
			strncat(id_path, pp->string, sizeof(id) - strlen(id) - 1);
			//maps\mp\_load::main();
		}

		if (is_file_ref) {
			if (!pp_expect(pp, TK_COLON))
				return 1;
			if (!pp_accept(pp, TK_COLON))
				return 1;
			if (!pp_expect(pp, TK_IDENT))
				return 1;
			printf("%s\n", id_path);
			snprintf(id,sizeof(id),"%s",pp->string);
		}

		var_t *v = NULL;
		int index=0; //if it's like level/self it won't be used anyway

		bool is_obj = false;
		bool is_array = false;
		if (pp_accept(pp, TK_DOT) || pp_accept(pp,TK_LBRACK)) {
			int obj_tk = pp->token;
			//printf("TK_DOT ACCEPTED IS_OBJ=TRUE\n");
			is_obj = true;
			var_t *v;

			if (parser_variable(pp, id, true, true, &v))
				return 1;
			if(v)
			index = v->index;
			if (obj_tk==TK_DOT) {

				do {
					if (!pp_expect(pp, TK_IDENT))
						return 1;

					scr_istring_t *istr = NULL;
					index = parser_find_indexed_string(pp, pp->string, &istr);

					if (istr == NULL)
						index = parser_create_indexed_string(pp, pp->string);
					if (!pp_accept(pp, TK_DOT))
						break;

					program_add_opcode(pp, OP_LOAD_FIELD);
					program_add_short(pp, index);
				} while (1);
			} else {
				is_array = true;
				do {
					if (parser_expression(pp))
						return 1;
					if (pp_accept(pp, TK_RBRACK))
						break;
					program_add_opcode(pp, OP_LOAD_ARRAY_INDEX);
					pp_expect(pp, TK_RBRACK);
				} while (pp_accept(pp, TK_LBRACK));
			}
		}

		if (pp_accept(pp, TK_ASSIGN)
			||
			pp_accept(pp,TK_PLUS_PLUS) ||
			pp_accept(pp,TK_MINUS_MINUS) ||
			pp_accept(pp, TK_PLUS_ASSIGN) ||
			pp_accept(pp, TK_MINUS_ASSIGN) ||
			pp_accept(pp, TK_DIVIDE_ASSIGN) ||
			pp_accept(pp, TK_MULTIPLY_ASSIGN)
			) {
			int tk = pp->token;

			if (!is_obj) {
				v = parser_find_local_variable(pp, id);

				if (!v) {
					v = parser_create_local_variable(pp);
					strncpy(v->name, id, sizeof(v->name) - 1);
					v->name[sizeof(v->name) - 1] = '\0';
				}
				index = v->index;
			}
			if (tk != TK_PLUS_PLUS&&tk != TK_MINUS_MINUS) {
				if (parser_expression(pp))
					goto unexpected_tkn;
			}
			else {
				/*
				program_add_opcode(pp, OP_PUSH);
				program_add_int(pp, 1);
				*/ //unneeded cuz of the extra opcodes added
			}

			/* add the opcodes for the special assign types */
			if (tk != TK_ASSIGN) { //should probably fix someday that also for arr/objs +=/-= etc works

				if (parser_variable(pp, id, true, false, NULL))
					return 1;

				if (tk == TK_PLUS_ASSIGN)
					program_add_opcode(pp, OP_ADD);
				else if (tk == TK_MINUS_ASSIGN)
					program_add_opcode(pp, OP_SUB);
				else if (tk == TK_DIVIDE_ASSIGN)
					program_add_opcode(pp, OP_DIV);
				else if (tk == TK_MULTIPLY_ASSIGN)
					program_add_opcode(pp, OP_MUL);
				else if (tk == TK_PLUS_PLUS)
					program_add_opcode(pp, OP_POST_INCREMENT);
				else if (tk == TK_MINUS_MINUS)
					program_add_opcode(pp, OP_POST_DECREMENT);
			}

			if (is_array)
				program_add_opcode(pp, OP_STORE_ARRAY_INDEX);
			else {
				if (is_obj)
					program_add_opcode(pp, OP_STORE_FIELD);
				else
					program_add_opcode(pp, OP_STORE);
				program_add_short(pp, index);
			}
			pp_accept(pp, TK_SEMICOLON);
		}
		else if (pp_accept(pp, TK_LBRACK)) {
			if (!pp_expect(pp, TK_STRING))
				return 1;
			pp_expect(pp, TK_RBRACK);

			pp_expect(pp, TK_ASSIGN);
			pp_expect(pp, TK_STRING);
			pp_accept(pp, TK_SEMICOLON);
		} else if (pp_accept(pp, TK_LPAREN)) {
			//definition :)

			bool is_call = false;
			int savepos = pp->curpos;
			int at;

			if (TK_EOF == parser_locate_token(pp, TK_RPAREN, &at, TK_LPAREN)) {
				printf("could not find rparen!\n");
				goto unexpected_tkn;
			}
			else {

				pp->curpos = at;
				//temp to try to get the 
				if (!pp_accept(pp, TK_LBRACE))
					is_call = true;

				pp->curpos = savepos; //reset to after the first lparen and catch the args

				int numargs = 0;

				if (pp_accept(pp, TK_RPAREN))
					goto no_args_lol;
				do {
					++numargs;
					if (is_call) {
						if (parser_expression(pp)) //auto pushes
							return 1;
						continue;
					}

					if (!pp_accept(pp, TK_IDENT))
						return 1;

					var_t *v = parser_find_local_variable(pp, pp->string);
					if (!v) {
						v = parser_create_local_variable(pp);
						strncpy(v->name, pp->string, sizeof(v->name) - 1);
						v->name[sizeof(v->name) - 1] = '\0';
					}
				} while (pp_accept(pp, TK_COMMA));

			no_args_lol:

				pp->curpos = at;
				//hehe
				//printf("cur ch=%c\n", pp->scriptbuffer[pp->curpos]);

				if (!pp_accept(pp, TK_LBRACE)) {
					//func call
					//printf("function call %s()\n", id);

					bool found = false;
					for (int i = 0; i <= pp->code_segment_size; i++) {
						code_segment_t *seg = &pp->code_segments[i];
						if (!strcmp(seg->id, id)) {
							found = true;
							if(is_thread_call) {
								program_add_opcode(pp, OP_PUSH);
								program_add_int(pp, numargs);

								program_add_opcode(pp, OP_PUSH);
								pp->current_segment->relocations[pp->current_segment->relocation_size++] = pp->program_counter;
								program_add_int(pp, seg->original_loc);
								program_add_opcode(pp, OP_CALL_THREAD);
							} else {
								program_add_opcode(pp, OP_CALL);
								pp->current_segment->relocations[pp->current_segment->relocation_size++] = pp->program_counter;
								program_add_int(pp, seg->original_loc);
								program_add_int(pp, numargs);
								program_add_opcode(pp, OP_POP);
							}
							break;
						}
					}

					if (!found) {
						//just add it to the 'builtin' funcs and maybe that works? :D
						//printf("function '%s' does not exist!\n", id);
						program_add_opcode(pp, OP_CALL_BUILTIN);

						scr_istring_t *istr=NULL;
						int index = parser_find_indexed_string(pp, id, &istr);

						if(istr==NULL)
							index = parser_create_indexed_string(pp, id);
						program_add_int(pp, index); //the index of string
						program_add_int(pp, numargs);
						program_add_opcode(pp, OP_POP);
					}
				}
				else {

					//printf("func definition %s\n", id);

					if (TK_EOF == parser_locate_token(pp, TK_RBRACE, &at, TK_LBRACE)) {
						printf("could not find rbrace :D\n");
						goto unexpected_tkn;
					}
					else {

						//printf("orig_loc=%d,pc=%d\n", seg->original_loc, pc);
						if (!strcmp(id, "main"))
							pp->main_segment = pp->code_segment_size + 1;

						pp->current_segment = &pp->code_segments[++pp->code_segment_size];
						pp->current_segment->original_loc = pp->program_counter;
						snprintf(pp->current_segment->id, sizeof(pp->current_segment->id), "%s", id);

						int start = pp->curpos;
						int end = at - 2;
						int block = parser_block(pp, start, end);

						if (block) {
							printf("BLOCK=%d,curpos=%d,at=%d\n", block, start, end);
							return block;
						}

						parser_clear_local_variables(pp);

						program_add_opcode(pp, OP_PUSH_NULL);
						program_add_opcode(pp, OP_RET);

						pp->current_segment->size = pp->program_counter - pp->current_segment->original_loc;
					}
				}
				pp_accept(pp, TK_SEMICOLON);
			}
		}
	} else if(pp_accept(pp,TK_RETURN)) {
		if (!pp_accept(pp, TK_SEMICOLON)) {
			if (parser_expression(pp))
				goto unexpected_tkn;
			pp_expect(pp, TK_SEMICOLON);
		} else {
			program_add_opcode(pp, OP_PUSH_NULL);
		}
		program_add_opcode(pp, OP_RET);
	} else {
		unexpected_tkn:
		parser_display_history(pp);

		printf("unexpected token: %s at %d (%c)\n", lex_token_strings[pp->token], pp->curpos, pp->scriptbuffer[pp->curpos]);
		return 1;
	}
	return 0;
}

static int parser_block(parser_t *pp, int start, int end) {
	while (start < end) {
		if (pp_accept(pp, TK_EOF))
			break;

		if (pp_accept(pp, TK_RBRACE)) {
			break;
		}

		int stmt = parser_statement(pp);

		if (stmt)
			return stmt;

	}

	return 0;
}

static int read_text_file(const char *filename, char **buf, int *filesize) {
#if 1
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
		return 1;
	fseek(fp, 0, SEEK_END);
	*filesize = ftell(fp);
	rewind(fp);
	*buf = (char*)malloc(*filesize + 1);
	memset(*buf, 0, *filesize);
	size_t res = fread(*buf, 1, *filesize, fp);
	fclose(fp);
	return 0;
#endif
	return 0;
}

int parser_compile(const char *filename, char **out_program, int *out_program_size) {
	*out_program = NULL;
	*out_program_size = 0;

	int retcode = 0;
	
	parser_t *pp = (parser_t*)xmalloc(sizeof(parser_t));
	parser_init(pp);

	if (read_text_file(filename, &pp->scriptbuffer, (int*)&pp->scriptbuffersize)) {
		printf("failed to read file '%s'\n", filename);
		return 1;
	}

	//printf("scriptbuffer size = %d\n",pp->scriptbuffersize);

	pp->program = (char*)xmalloc(pp->scriptbuffersize * 4); //i don't think the actual program will be this size lol
	memset(pp->program, OP_NOP, pp->scriptbuffersize);

	pp->current_segment = &pp->code_segments[0];
	pp->current_segment->assigned_loc = 0;
	pp->current_segment->original_loc = 0;
	pp->current_segment->size = sizeof(int) + sizeof(int) + 3;
	strcpy(pp->current_segment->id, "__init");

	program_add_opcode(pp, OP_CALL);
	program_add_int(pp, 0); //tmp
	program_add_int(pp, 0); //numargs
	program_add_opcode(pp, OP_POP); //retval
	program_add_opcode(pp, OP_HALT);

	retcode = parser_block(pp, pp->curpos, pp->scriptbuffersize);

	//rearrange the code structure :)

	char *rearranged_program = NULL;

	int total_size = 0;

	for (int i = 0; i <= pp->code_segment_size; i++) {
		code_segment_t *seg = &pp->code_segments[i];
		total_size += seg->size + strlen(seg->id) + 1 + sizeof(int);
	}

	scr_istring_t *istr = NULL;
	for (int i = 0; i < MAX_ISTRINGS; i++) {
		istr = &pp->istrings[i];
		if (!istr->inuse)
			continue;
		total_size += strlen(istr->string) + 1;
	}

	*out_program_size = total_size + sizeof(int) + sizeof(int) + sizeof(int);

	rearranged_program = (char*)xmalloc(*out_program_size);
	int rearranged_program_pc = 0;

	int rearrange_main = 0;

	for (int i = 0; i <= pp->code_segment_size; i++) {
		code_segment_t *seg = &pp->code_segments[i];
		if (pp->main_segment == i) {
			rearrange_main = rearranged_program_pc;
		}
		seg->new_location = rearranged_program_pc;

		for (pp->program_counter = seg->original_loc; pp->program_counter < seg->size + seg->original_loc; pp->program_counter++) {
			int opcode = pp->program[pp->program_counter];

			for (int k = 0; k < seg->relocation_size; k++) {
				if (seg->relocations[k] == pp->program_counter) {
					//change the location lol

					int new_pos = 0;
					for (int f = 0; f <= pp->code_segment_size; f++) {
						//printf("assigned_loc=%d,seg->reloc=%d\n", pp->code_segments[f].assigned_loc, seg->relocations[k]);

						if (pp->code_segments[f].assigned_loc == pp->program[seg->relocations[k]]) {
							//if not found ouch
							opcode = new_pos;
							//printf("new_opcode=%d\n", new_pos);
						}
						new_pos += pp->code_segments[f].size;
					}
				}
			}
			rearranged_program[rearranged_program_pc++] = opcode;
		}
	}

	*(int*)(rearranged_program + 1) = rearrange_main;//call <loc> replace?
	int start_defs = rearranged_program_pc;

#if 1
	for (int i = 0; i <= pp->code_segment_size; i++) {
		code_segment_t *seg = &pp->code_segments[i];

		*(int*)(rearranged_program + rearranged_program_pc) = seg->new_location;
		rearranged_program_pc += sizeof(int);
		strncpy(rearranged_program + rearranged_program_pc, seg->id, strlen(seg->id));
		rearranged_program_pc += strlen(seg->id);
		rearranged_program[rearranged_program_pc++] = 0;
	}
#endif
	int total_num_ref_funcs = 0;

	istr = NULL;
	for (int i = 0; i < MAX_ISTRINGS; i++) {
		istr = &pp->istrings[i];
		if (!istr->inuse)
			continue;
		++total_num_ref_funcs;
	}

	//write num of referenced builtin funcs
	*(int*)(rearranged_program + rearranged_program_pc) = total_num_ref_funcs;
	rearranged_program_pc += sizeof(int);

	for (int i = 0; i < MAX_ISTRINGS; i++) {
		istr = &pp->istrings[i];
		if (!istr->inuse)
			continue;
		strncpy(rearranged_program + rearranged_program_pc, istr->string, strlen(istr->string));
		rearranged_program_pc += strlen(istr->string);
		rearranged_program[rearranged_program_pc++] = 0;
	}

	//write amount of functions

	*(int*)(rearranged_program + rearranged_program_pc) = pp->code_segment_size + 1;//call <loc> replace?
	rearranged_program_pc += sizeof(int);
	*(int*)(rearranged_program + rearranged_program_pc) = start_defs;

	xfree(pp->program);

	*out_program = rearranged_program;
	if (pp->error)
		retcode = 1;
	parser_cleanup(pp);
	return retcode;
}