
#include "cstructparser.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
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

typedef struct
{
	int offset;
	int size;
	union
	{
		uint32_t u32;
		uint16_t u16;
		uint8_t u8[4];
	} u;
	dynarray buffer;
} tinystream_t;

void ts_init(tinystream_t *ts)
{
	ts->offset = 0;
	ts->size = 0;
	array_init(&ts->buffer, char);
}

void ts_free(tinystream_t *ts)
{
	array_free(&ts->buffer);
	ts_init(ts);
}

void ts8(tinystream_t *ts, uint8_t i)
{
	unsigned char c = i & 0xff;
	array_push(&ts->buffer, &c);
	++ts->offset;
}
void ts16(tinystream_t *ts, uint16_t n)
{
	char *p = (char*)&n;
	array_push(&ts->buffer, p);
	array_push(&ts->buffer, p + 1);
	ts->offset += 2;
}

void ts32(tinystream_t *ts, uint32_t n)
{
	char *p = (char*)&n;
	array_push(&ts->buffer, p);
	array_push(&ts->buffer, p + 1);
	array_push(&ts->buffer, p + 2);
	array_push(&ts->buffer, p + 3);
	ts->offset += 4;
}

#define next (pp->curpos >= pp->scriptbuffersize ? 0 : pp->scriptbuffer[pp->curpos++])
#define next_chk(x) ( ( (pp->scriptbuffer[pp->curpos] == x) ? pp->scriptbuffer[pp->curpos++] : pp->scriptbuffer[pp->curpos] ) == x)

static const char *c_keywords_strings[] = { "unsigned", "char", "short", "int", "long", "float", "double", NULL };

static int parser_append_to_parse_string(parser_t *pp, int c) {
#if 0
	if (!c)
		return 0;
#endif
	if (pp->stringindex >= sizeof(pp->string)) {
		vm_printf("keyword string index overflow\n");
		return 0;
	}
	pp->string[pp->stringindex++] = c;
	return 1;
}

bool isHexChar(int ch, int *value) {
	*value = 0;
	if (ch >= 'a' && ch <= 'f') {
		*value = 15 - ('f' - ch);
		return true;
	}
	if (ch >= 'A' && ch <= 'F') {
		*value = 15 - ('F' - ch);
		return true;
	}
	if (ch >= '0' && ch <= '9') {
		*value = ch - '0';
		return true;
	}
	return false;
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
			case 'x': {
				int nibble1, nibble2;
				int first = next;
				if (!isHexChar(first, &nibble1))
					goto break_out;
				int second = next;
				if (!isHexChar(second, &nibble2)) goto break_out;
				int value = ((nibble1 << 4) | nibble2) & 255;
				ch = value;
			} break;
			case 'r':
				ch = '\r';
				break;
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
break_out:
	return is_char ? TK_CHAR : TK_STRING;
}

static bool is_ident_char(int c)
{
	return (isalnum(c) || c == '$' || c == '_');
}

static bool pp_is_token_enabled(parser_t *pp, int t) {
	if (!pp->enabledtokens)
		return true;
	for (int i = 0; pp->enabledtokens[i] != TK_EOF; ++i) {
		if (pp->enabledtokens[i] == t)
			return true;
	}
	return false;
}

static int parser_read_identifier(parser_t *pp) {
	memset(pp->string, 0, sizeof(pp->string));
	pp->stringindex = 0;
	--pp->curpos;
	int ch;
	while (is_ident_char((ch = next))) {
		if (!parser_append_to_parse_string(pp, ch))
			break;
	}
	--pp->curpos;

	for (int i = 0; c_keywords_strings[i]; ++i)
	{
		if (!strcmp(c_keywords_strings[i], pp->string))
			return TK_KEYWORD_START + i;
	}

	if (!strcmp(pp->string, "if"))
		return TK_IF;
	else if (!strcmp(pp->string, "typedef"))
		return TK_TYPEDEF;
	else if (!strcmp(pp->string, "struct"))
		return TK_STRUCT;
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
		if (pp->flags & PARSER_FLAG_TOKENIZE_WHITESPACE)
			return ch == ' ' ? TK_SPACE : TK_TAB;
		goto scan;
	case '\n':
		if (pp->flags & PARSER_FLAG_TOKENIZE_NEWLINE)
			return TK_NEWLINE;
	case '\r':
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
			return TK_DOT;// return TK_MEMBER_SEPERATOR;
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
		if (isdigit(pp->scriptbuffer[pp->curpos]))//if (isdigit(pp->scriptbuffer[pp->curpos + 1]))
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
#if 0
	case '$':
		if (isdigit(pp->scriptbuffer[pp->curpos + 1]))
			goto get_num;
		return TK_DOLLAR;
#endif
	case '/':
		if (next_chk('/')) {
			while (ch && ch != '\n') {
				ch = next;
			}
			goto scan;

		}
		else if (next_chk('*')) {
			int prev = 0;
			while (ch) {
				if (ch == '/' && prev == '*')
					break;
				prev = ch;
				ch = next;
			}
			goto scan;
		}
		else if (next_chk('='))
			return TK_DIVIDE_ASSIGN;
		return TK_DIVIDE;
	case '#':
		return TK_SHARP;
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
		else
		{
			if (is_ident_char(ch))
			{
				int ident = parser_read_identifier(pp);
				if (!pp_is_token_enabled(pp, ident))
					return TK_IDENT;
				if (ident == TK_R_TRUE || ident == TK_R_FALSE) {
					pp->integer = (ident == TK_R_TRUE) ? 1 : 0;
					return TK_INT;
				}
				return ident;
			}
		}
		return TK_ILLEGAL;

	case 0:
		return TK_EOF;
	}
	return TK_ILLEGAL;
}

#define pp_accept(x,y) (parser_accept_token(x,y))
#define pp_undo() (parser_undo())
#define pp_save(x) int sav=x->curpos
#define pp_restore(x) x->curpos=sav
#define pp_goto(x,y) x->curpos=y;
#define pp_tell(x) x->curpos

static void parser_undo(parser_t *pp) {
	pp->curpos = pp->prevpos;
}

static int parser_accept_token(parser_t *pp, int token) {
	pp->prevpos = pp->curpos;
	int next_token = parser_read_next_token(pp);
	pp->token = next_token;
	//vm_printf("pp_accept(%s), next_tkn = %s\n", lex_token_strings[token], lex_token_strings[next_token]);
	if (next_token == token) {
		return 1;
	}
	pp->curpos = pp->prevpos;
	return 0;
}

int parser_get_location(parser_t *pp) {
	char *end = &pp->scriptbuffer[pp->curpos];
	char *p = pp->scriptbuffer;
	int lineno = 0;
	while (*p && p < end) {
		if (*p == '\n' || *p == '\r') {
			p += (p[0] + p[1] == '\n' + '\r') ? 2 : 1;
			++lineno;
		}
		else
			++p;
	}
	return lineno;
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
	int lineno = parser_get_location(pp);
	parser_display_history(pp);
	vm_printf("error: unexpected symbol! expected %s, got %s at line %d\n", lex_token_strings[token], lex_token_strings[pp->token], lineno);
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
		vm_printf("MAX_ISTRINGS\n");
		return 0;
	}
	++pp->numistrings;
	pp->istrings[i].inuse = true;
	pp->istrings[i].string = new_str;
	return i;
}

static void parser_free_indexed_string(parser_t *pp, int index) {
	if (index > 0xffff) {
		vm_printf("too high m8\n");
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
	array_init(&pp->structs, cstruct_t);
	pp->logfile = NULL;
	//pp->logfile = fopen("parser.log", "w");
	pp->last_opcode_index = -1;
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
	//xfree(pp->scriptbuffer);
	pp->scriptbuffer = NULL;

	if (pp->logfile)
		fclose(pp->logfile);

	for (int i = 0; i < MAX_ISTRINGS; i++)
		parser_free_indexed_string(pp, i);
	array_free(&pp->structs);
	xfree(pp);
}

//#define add_opcode(op) (pp->program[pp->program_counter++]=(int)op)

static int program_previous_opcode(parser_t *pp)
{
	if (pp->last_opcode_index == -1)
		return -1;
	return pp->program[pp->last_opcode_index];
}

static void program_add_opcode(parser_t *pp, uint8_t opcode) {
	if (!pp->execute)
		return;
	pp->last_opcode_index = pp->program_counter;
	pp->program[pp->program_counter++] = opcode;
	const char *opcode_str = opcode < OP_END_OF_LIST ? e_opcodes_strings[opcode] : "[not opcode]";
	if(pp->logfile)
		fprintf(pp->logfile, "\t%d: %s\n", pp->program_counter - 1, opcode_str);
	//fprintf(pp->logfile, "program_add_opcode(%s) => %d (0x%x)\n", opcode_str, opcode, opcode);
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
	//if(pp->logfile)
	//fprintf(pp->logfile, "program_add_int(%d) => %x, %x, %x, %x\n", i, i & 0xff, (i>>8)&0xff,(i>>16)&0xff,(i>>24)&0xff);
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
	//vm_printf("program_add_float(%f)\n", i);
}

static void program_add_short(parser_t *pp, int i) {
	program_add_int(pp, i);
	//int might be faster cuz lol
#if 0
	if (!pp->execute)
		return;

	*(short*)(pp->program + pp->program_counter) = i;
	pp->program_counter += sizeof(short);
	vm_printf("program_add_short(%d) => %x, %x\n", i, i & 0xff, (i >> 8) & 0xff);
#endif
}

static var_t *parser_create_local_variable(parser_t *pp);
static int parser_variable(parser_t *pp, const char *id, bool load, bool create_if_not_exist, var_t **out, int op) {
	if (out)
		*out = NULL;

	if (!strcmp(id, "level") || !strcmp(id, "self")) {
		program_add_opcode(pp, strcmp(id, "level") ? OP_GET_SELF : OP_GET_LEVEL);
	}
	else {

		var_t *v = parser_find_local_variable(pp, id);
		if (out)
			*out = v;

		if (!v) {
			if (!create_if_not_exist) {
				vm_printf("variable '%s' does not exist!\n", id);
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
				program_add_opcode(pp, op);
				program_add_short(pp, v->index);
			}
		}
	}

	return 0;
}

cstruct_t *parser_get_struct(parser_t *pp, const char *name, int *index)
{
	for (int i = pp->structs.size; i--;)
	{
		array_get(&pp->structs, cstruct_t, cs, i);
		if (!strcmp(cs->name, name))
		{
			if (index != NULL)
				*index = i;
			return cs;
		}
	}
	return NULL;
}

int parser_process_variable_member_fields(parser_t *pp, const char *str)
{
	while (1)
	{
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
	return 0;
}

/* TODO rewrite this stuff into e.g factor push stuff and then e.g TK_ASSIGN push the STORE_ stuff with states or so */

static int parser_factor(parser_t *pp) {
	if (pp_accept(pp, TK_NOT)) {
		++pp->not;
		return 0;
	}
	else if (pp_accept(pp, TK_COLON)) {
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
			vm_printf("function '%s' was not found\n", pp->string);
			return 1;
		}
	}
	else if (pp_accept(pp, TK_AND)) {
		//get address of
		if (!pp_accept(pp, TK_IDENT))
			return 1;
		if (parser_variable(pp, pp->string, true, false, NULL, OP_LOAD_REF))
			return 1;
		program_add_opcode(pp, OP_ADDRESS);
	}
	else if (pp_accept(pp, TK_MULTIPLY)) {
		//dereference

		if (!pp_accept(pp, TK_IDENT))
			return 1;
		if (parser_variable(pp, pp->string, true, false, NULL, OP_LOAD_REF))
			return 1;
		program_add_opcode(pp, OP_DEREFERENCE);
	}
	else if (pp_accept(pp, TK_CHAR)) {
		program_add_opcode(pp, OP_PUSH);
		program_add_int(pp, *(int32_t*)&pp->string[0]);
	}
	else if (pp_accept(pp, TK_INT)) {
		program_add_opcode(pp, OP_PUSH);
		program_add_int(pp, pp->integer);
	}
	else if (pp_accept(pp, TK_LBRACK)) {
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
	}
	else if (pp_accept(pp, TK_R_NEW)) {
		if (!pp_expect(pp, TK_IDENT))
			return 1;
#if 0
		if (!stricmp(pp->string, "object") || !stricmp(pp->string, "obj")) {
			program_add_opcode(pp, OP_PUSH_OBJECT);
		}
		else if (!stricmp(pp->string, "array")) {
			program_add_opcode(pp, OP_PUSH_ARRAY);
			program_add_short(pp, 0);
		}
#endif
		int s_index;
		cstruct_t *cs = parser_get_struct(pp, pp->string, &s_index);
		if (cs != NULL)
		{
			program_add_opcode(pp, OP_PUSH_BUFFER);
			program_add_short(pp, s_index);
			program_add_short(pp, cs->size);
		}
		else
			return 1;
	}
	else if (pp_accept(pp, TK_R_NULL)) {
		program_add_opcode(pp, OP_PUSH_NULL);
	}
	else if (pp_accept(pp, TK_FLOAT)) {
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
	}
	else if (pp_accept(pp, TK_IDENT)) {
		char id[128] = { 0 };
		snprintf(id, sizeof(id) - 1, "%s", pp->string);

		bool is_method = false;

		if (pp_accept(pp, TK_IDENT))
		{
			//e.g self method(); //method call

			if (parser_variable(pp, id, true, false, NULL, OP_LOAD))
				return 1;

			if (parser_process_variable_member_fields(pp, pp->string))
				return 1;
			//copy the method name
			snprintf(id, sizeof(id) - 1, "%s", pp->string);

			pp_expect(pp, TK_LPAREN); //has to be a function call
			is_method = true;
			goto _func_call;
		} else if (pp_accept(pp, TK_LPAREN)) {
		_func_call:
			{
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

				//vm_printf("function call with RETURN !!!! %s()\n", id);
				{
					bool found = false;
					for (int i = 0; i <= pp->code_segment_size; i++) {
						code_segment_t *seg = &pp->code_segments[i];
						if (!strcmp(seg->id, id)) {
							found = true;
							program_add_opcode(pp, is_method ? OP_CALL_METHOD : OP_CALL);
							pp->current_segment->relocations[pp->current_segment->relocation_size++] = pp->program_counter;
							program_add_int(pp, seg->original_loc);
							program_add_int(pp, numargs);
							break;
						}
					}

					if (!found) {

						//just add it to the 'builtin' funcs and maybe that works? :D
						//vm_printf("function '%s' does not exist!\n", id);
						program_add_opcode(pp, is_method ? OP_CALL_BUILTIN_METHOD : OP_CALL_BUILTIN);

						scr_istring_t *istr = NULL;
						int index = parser_find_indexed_string(pp, id, &istr);

						if (istr == NULL)
							index = parser_create_indexed_string(pp, id);
						program_add_int(pp, index); //the index of string
						program_add_int(pp, numargs);
					}
				}
			}
		}
		else {

			if (parser_variable(pp, pp->string, true, false, NULL, OP_LOAD))
				return 1;

			if (parser_process_variable_member_fields(pp, pp->string))
				return 1;
		}
	}
	else if (pp_accept(pp, TK_LPAREN)) {
		int lparen_savepos = pp->curpos;
		bool is_cast = false;

		for (int i = TK_KEYWORD_CHAR; i <= TK_KEYWORD_DOUBLE; ++i)
		{
			if (pp_accept(pp, i))
			{
				//doesnt matter these are reserved keywords anyways
				pp_expect(pp, TK_RPAREN);
				if (parser_expression(pp))
					return 1;
#if 1
				int prevop = program_previous_opcode(pp);
				if (prevop == -1 || prevop == OP_CAST)
				{
					vm_printf("we need a prevop to do a cast!\n");
					return 1;
				}

				int prevopsz = pp->program_counter - pp->last_opcode_index; //size of the operands of that opcode + opcode (byte)
				char *copy = (char*)xmalloc(prevopsz);
				pp->program_counter -= prevopsz;
				memcpy(copy, &pp->program[pp->program_counter], prevopsz);
				//save contents of prev op + operands

				//overwrite it with op_cast -> so it's executed before the op above
				program_add_opcode(pp, OP_CAST);
				program_add_short(pp, i - TK_KEYWORD_CHAR);

				for (int k = 0; k < prevopsz; ++k)
					pp->program[pp->program_counter++] = copy[k];

				xfree(copy);
#else
				//op_cast is 5 in size
				program_add_opcode(pp, OP_CAST);
				program_add_short(pp, i - TK_KEYWORD_CHAR);
#endif
				is_cast = true;
				break;
			}
		}

		if (!is_cast) {
			int savepos = pp->curpos;
			if (pp_accept(pp, TK_IDENT)) {
				int s_ind;
				cstruct_t *cs = parser_get_struct(pp, pp->string, &s_ind);
				if (cs == NULL)
				{
					for (int i = 0; e_var_types_strings[i]; ++i) {
						if (!stricmp(pp->string, e_var_types_strings[i])) {
							pp_expect(pp, TK_RPAREN);
							if (parser_expression(pp))
								return 1;
							program_add_opcode(pp, OP_CAST);
							program_add_short(pp, i);
							is_cast = true;
							break;
						}
					}
				}
				else
				{
					pp_expect(pp, TK_RPAREN);
					if (parser_expression(pp))
						return 1;
					program_add_opcode(pp, OP_CAST_STRUCT);
					program_add_short(pp, s_ind);
					is_cast = true;
				}
				if (!is_cast)
					pp->curpos = savepos;
			}
		}

		if (!is_cast)
		{

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
					vm_printf("invalid vector!\n");
					return 1;
				}
				program_add_opcode(pp, OP_PUSH_VECTOR);
			}


			pp_expect(pp, TK_RPAREN);
		}
	}
	else {
		int lineno = parser_get_location(pp);
		vm_printf("factor: syntax error got %s at %d\n", lex_token_strings[pp->token], lineno);
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
		if (!pp_accept(pp, TK_PLUS) && !pp_accept(pp, TK_MODULO) && !pp_accept(pp, TK_MINUS) && !pp_accept(pp, TK_OR) && !pp_accept(pp, TK_AND))
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
	vm_printf("MAX LOCAL VARIABLES REACHED!!!\n");
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
		vm_printf("BLOCK: could not find TK_RBRACE\n");
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

static int parser_function_call(parser_t *pp, const char *ident, bool is_thread_call, bool can_declare)
{
	char id[128] = { 0 };
	char id_path[128] = { 0 };

accept_ident:

	snprintf(id, sizeof(id), "%s", ident);
	snprintf(id_path, sizeof(id), "%s", id);

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
		vm_printf("%s\n", id_path);
		snprintf(id, sizeof(id), "%s", pp->string);
	}

	var_t *v = NULL;
	int index = 0; //if it's like level/self it won't be used anyway

	bool is_obj = false;
	bool is_array = false;
	if (pp_accept(pp, TK_DOT) || pp_accept(pp, TK_LBRACK)) {
		int obj_tk = pp->token;
		//vm_printf("TK_DOT ACCEPTED IS_OBJ=TRUE\n");
		is_obj = true;
		var_t *v;

		if (parser_variable(pp, id, true, true, &v, OP_LOAD))
			return 1;
		if (v)
			index = v->index;
		if (obj_tk == TK_DOT) {

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
		}
		else {
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
	if (pp_accept(pp, TK_ENDON))
	{
		pp_expect(pp, TK_LPAREN);
		pp_expect(pp, TK_STRING);

		scr_istring_t *istr = NULL;
		index = parser_find_indexed_string(pp, pp->string, &istr);

		if (istr == NULL)
			index = parser_create_indexed_string(pp, pp->string);

		if (!pp_accept(pp, TK_RPAREN))
		{
			vm_printf("unexpected arguments");
			return 1;
		}
		if (!is_obj && !is_array)
		{
			if (parser_variable(pp, id, true, false, NULL, OP_LOAD))
				return 1;
		}
		program_add_opcode(pp, OP_ENDON_EVENT_STRING);
		program_add_int(pp, index);
	} else if (pp_accept(pp, TK_NOTIFY))
	{
		pp_expect(pp, TK_LPAREN);
		pp_expect(pp, TK_STRING);

		scr_istring_t *istr = NULL;
		index = parser_find_indexed_string(pp, pp->string, &istr);

		if (istr == NULL)
			index = parser_create_indexed_string(pp, pp->string);

		int numargs = 0;
		if (!pp_accept(pp, TK_COMMA) && pp_accept(pp, TK_RPAREN))
		{
			//no args
		}
		else
		{
			do {
				++numargs;
				if (parser_expression(pp)) //auto pushes
					return 1;
			} while (pp_accept(pp, TK_COMMA));
			pp_expect(pp, TK_RPAREN);
		}
		if (!is_obj && !is_array)
		{
			if (parser_variable(pp, id, true, false, NULL, OP_LOAD))
				return 1;
		}
		program_add_opcode(pp, OP_NOTIFY_EVENT_STRING);
		program_add_int(pp, numargs);
		program_add_int(pp, index);
	} else if (pp_accept(pp, TK_WAITTILL))
	{
		pp_expect(pp, TK_LPAREN);
		pp_expect(pp, TK_STRING);
		
		scr_istring_t *istr = NULL;
		index = parser_find_indexed_string(pp, pp->string, &istr);

		if (istr == NULL)
			index = parser_create_indexed_string(pp, pp->string);

		int numargs = 0;
		int startindex = -1;
		if (!pp_accept(pp, TK_COMMA) && pp_accept(pp,TK_RPAREN))
		{
			//accepting no local vars
		}
		else
		{
			do {
				++numargs;

				if (!pp_accept(pp, TK_IDENT))
					return 1;

				var_t *v = parser_find_local_variable(pp, pp->string);
				if (!v) {
					v = parser_create_local_variable(pp);
					strncpy(v->name, pp->string, sizeof(v->name) - 1);
					v->name[sizeof(v->name) - 1] = '\0';
				}
				if (startindex == -1)
					startindex = v->index;
			} while (pp_accept(pp, TK_COMMA));
			pp_expect(pp, TK_RPAREN);
		}
		if (!is_obj && !is_array)
		{
			if (parser_variable(pp, id, true, false, NULL, OP_LOAD))
				return 1;
		}
		program_add_opcode(pp, OP_WAIT_EVENT_STRING);
		program_add_int(pp, startindex);
		program_add_int(pp, index);
		program_add_int(pp, numargs);
	} else if (pp_accept(pp, TK_ASSIGN)
		||
		pp_accept(pp, TK_PLUS_PLUS) ||
		pp_accept(pp, TK_MINUS_MINUS) ||
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
		if (tk != TK_PLUS_PLUS && tk != TK_MINUS_MINUS) {
			if (parser_expression(pp))
				return 1;
		}
		else {
			/*
			program_add_opcode(pp, OP_PUSH);
			program_add_int(pp, 1);
			*/ //unneeded cuz of the extra opcodes added
		}

		/* add the opcodes for the special assign types */
		if (tk != TK_ASSIGN) { //should probably fix someday that also for arr/objs +=/-= etc works

			if (parser_variable(pp, id, true, false, NULL, OP_LOAD))
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
	}
	else if (pp_accept(pp, TK_LPAREN)) {
		//definition :)

		bool is_call = false;
		int savepos = pp->curpos;
		int at;

		if (TK_EOF == parser_locate_token(pp, TK_RPAREN, &at, TK_LPAREN)) {
			vm_printf("could not find rparen!\n");
			return 1;
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
			//vm_printf("cur ch=%c\n", pp->scriptbuffer[pp->curpos]);

			if (!pp_accept(pp, TK_LBRACE)) {
				//func call
				//vm_printf("function call %s()\n", id);

				bool found = false;
				for (int i = 0; i <= pp->code_segment_size; i++) {
					code_segment_t *seg = &pp->code_segments[i];
					if (!strcmp(seg->id, id)) {
						found = true;
						if (is_thread_call) {
							program_add_opcode(pp, OP_PUSH);
							program_add_int(pp, numargs);

							program_add_opcode(pp, OP_PUSH);
							pp->current_segment->relocations[pp->current_segment->relocation_size++] = pp->program_counter;
							program_add_int(pp, seg->original_loc);
							program_add_opcode(pp, OP_CALL_THREAD);
						}
						else {
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
					//vm_printf("function '%s' does not exist!\n", id);
					program_add_opcode(pp, OP_CALL_BUILTIN);

					scr_istring_t *istr = NULL;
					int index = parser_find_indexed_string(pp, id, &istr);

					if (istr == NULL)
						index = parser_create_indexed_string(pp, id);
					program_add_int(pp, index); //the index of string
					program_add_int(pp, numargs);
					program_add_opcode(pp, OP_POP);
				}
			}
			else {

				//vm_printf("func definition %s\n", id);

				if (TK_EOF == parser_locate_token(pp, TK_RBRACE, &at, TK_LBRACE)) {
					vm_printf("could not find rbrace :D\n");
					return 1;
				}
				else {

					//vm_printf("orig_loc=%d,pc=%d\n", seg->original_loc, pc);
					if (!strcmp(id, "main"))
						pp->main_segment = pp->code_segment_size + 1;

					pp->current_segment = &pp->code_segments[++pp->code_segment_size];
					pp->current_segment->original_loc = pp->program_counter;
					if(pp->logfile)
						fprintf(pp->logfile, "[%s]\n", id);
					snprintf(pp->current_segment->id, sizeof(pp->current_segment->id), "%s", id);

					int start = pp->curpos;
					int end = at - 2;
					int block = parser_block(pp, start, end);

					if (block) {
						vm_printf("BLOCK=%d,curpos=%d,at=%d\n", block, start, end);
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

	return 0;
}

static void parser_log(parser_t *pp, const char *s)
{
	if (pp->logfile)
		fprintf(pp->logfile, "%s", s);
}

typedef struct
{
	int from;
	int to;
	int jmprel;
	int type;
} cond_jump_t;

static int parser_statement(parser_t *pp) {
	bool is_thread_call = false;
	int before = pp->curpos;

	dynarray structs;

	int np;
	if (pp_accept(pp, TK_EOF)) {
		return 1;
	}
	else if (pp_accept(pp, TK_TYPEDEF)) {
		goto struct_define;
	}
	else if (pp_accept(pp, TK_STRUCT)) {
	struct_define:
		array_init(&structs, cstruct_t);
		np = cstructparser(&pp->scriptbuffer[before], &structs, false);
		if (!np)
		{
			return 1;
		}
		pp->curpos = np + before;
		for (int i = 0; i < structs.size; i++)
		{
			array_get(&structs, cstruct_t, cs, i);
			//vm_printf("cs size = %d %s\n", cs->size, cs->name);
			array_push(&pp->structs, cs);
		}
		array_free(&structs);
	}
	else if (pp_accept(pp, TK_SEMICOLON)) {
		return 0;
	}
	else if (pp_accept(pp, TK_LBRACE)) {
		if (parser_encapsulated_block(pp))
			goto unexpected_tkn;
	}
	else if (pp_accept(pp, TK_R_WAIT)) {
		if (parser_expression(pp))
			return 1;

		program_add_opcode(pp, OP_WAIT);
		pp_expect(pp, TK_SEMICOLON);
	}
	else if (pp_accept(pp, TK_IF) || pp_accept(pp, TK_WHILE) || pp_accept(pp, TK_R_FOR)) {
		int tk = pp->token;

		pp_expect(pp, TK_LPAREN);

		if (tk == TK_R_FOR) {
			if (parser_statement(pp))
				return 1;
		}

		int cond_pos = pp->program_counter;

		int which = TK_OR_OR; //if it's a single statement e.g if(a > 10) then it's an or case

		vector jumps;
		vector_init(&jumps);
		bool has_added_ops = false;
		do {
			int from_jmp_relative = pp->program_counter;
			unsigned int cur = pp->curpos;
			if (tk == TK_R_FOR && pp_accept(pp, TK_SEMICOLON) && !has_added_ops)
			{
				program_add_opcode(pp, OP_PUSH);
				program_add_int(pp, 0);
				program_add_opcode(pp, OP_PUSH);
				program_add_int(pp, 0);
				program_add_opcode(pp, OP_EQ);
				pp->curpos = cur;
				break;
			}
			has_added_ops = true;
			if (pp_accept(pp, TK_NOT)) {
				if (parser_expression(pp))
					return 1;
				program_add_opcode(pp, OP_NOT);
			}
			else
			{
				if (parser_expression(pp))
					return 1;
			}

			if (pp_accept(pp, TK_NOTEQUAL) || pp_accept(pp, TK_GEQUAL) || pp_accept(pp, TK_LEQUAL) || pp_accept(pp, TK_EQUALS) || pp_accept(pp, TK_LESS) || pp_accept(pp, TK_GREATER)) {
				int ctkn = pp->token;

				if (parser_expression(pp))
					return 1;
				if (ctkn == TK_EQUALS)
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

			cond_jump_t *jmp = (cond_jump_t*)xmalloc(sizeof(cond_jump_t));
			jmp->from = pp->program_counter;
			jmp->to = from_jmp_relative; //tmp
			jmp->type = which;
			program_add_opcode(pp, OP_JUMP_ON_FALSE);
			program_add_short(pp, 0); //temporarily

			jmp->jmprel = pp->program_counter;
			program_add_opcode(pp, OP_JUMP_RELATIVE); //incase we do get through we just jump to the actual code (TODO: fix this)
			program_add_short(pp, 0); //temporarily

			vector_add(&jumps, jmp);
			if (pp_accept(pp, TK_OR_OR))
				which = TK_OR_OR;
			else if (pp_accept(pp, TK_AND_AND))
				which = TK_AND_AND;
			else
				break;

		} while (
			1//pp_accept(pp, TK_AND_AND) || pp_accept(pp, TK_OR_OR)
			);

		int tmp_at_this_loc = 0;
		bool got_rparen = false;
		if (tk == TK_R_FOR) {
			pp_accept(pp, TK_SEMICOLON);
			tmp_at_this_loc = pp->curpos;
			if (!pp_accept(pp, TK_RPAREN))
			{
				pp->execute = false;
				if (parser_statement(pp))
					return 1;
				pp->execute = true;
			}
			else
			{
				tmp_at_this_loc = pp->curpos;
				got_rparen = true;
			}
		}
		if(!got_rparen)
			pp_expect(pp, TK_RPAREN);

		int codeblockpos = pp->program_counter;
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


		int end_cond_pos = pp->program_counter;
		if (tk == TK_WHILE || tk == TK_R_FOR) {
			program_add_opcode(pp, OP_JUMP_RELATIVE);
			program_add_short(pp, (cond_pos - pp->program_counter - sizeof(int)));
			if(pp->logfile)
				fprintf(pp->logfile, "JUMP RELATIVE (WHILE/FOR) from %d to %d\n", pp->program_counter, cond_pos);
			end_cond_pos += 5;
		}

		for (int i = 0; i < vector_count(&jumps); i++)
		{
			cond_jump_t *nxtjmp = NULL;
			if (i + 1 < vector_count(&jumps))
				nxtjmp = (cond_jump_t*)vector_get(&jumps, i + 1);
			cond_jump_t *jump = (cond_jump_t*)vector_get(&jumps, i);
			//end jump pos for last when doing a &&
			int jmp_pos = (end_cond_pos - jump->from);
			{
				*(int*)(pp->program + jump->from) = OP_JUMP_ON_FALSE;
				*(int*)(pp->program + jump->from + 1) = (end_cond_pos - jump->from);
				if (vector_count(&jumps) == 1 || (nxtjmp&&nxtjmp->type == TK_OR_OR))
				{
					if(nxtjmp)
					*(int*)(pp->program + jump->from + 1) = (nxtjmp->to - jump->from);
					*(int*)(pp->program + jump->jmprel + 1) = (codeblockpos - jump->jmprel);
				}
				else
					*(int*)(pp->program + jump->jmprel + 1) = 5;
			}
			*(int*)(pp->program + jump->from + 1) -= 5;
			*(int*)(pp->program + jump->jmprel + 1) -= 5;
			if (pp->logfile)
			{
				fprintf(pp->logfile, "JUMP %d -> %d\n", jump->from, jmp_pos - 5 + jump->from);
				fprintf(pp->logfile, "JUMP RELATIVE %d -> %d\n", jump->from + 1 + 4, *(int*)(pp->program + jump->jmprel + 1) + jump->jmprel);
			}
			xfree(jump);
		}
		vector_free(&jumps);
		if (pp_accept(pp, TK_R_ELSE) && tk == TK_IF) {
			program_add_opcode(pp, OP_JUMP_ON_TRUE);
			int from_jmp_relative = pp->program_counter;
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
	}
	else if (pp_accept(pp, TK_WHILE)) {

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
			vm_printf("could not find rbrace!\n");
			goto unexpected_tkn;
		}
		else {
			int start = pp->curpos;
			int end = at - 2;
			int block = parser_block(pp, start, end);

			if (block) {
				vm_printf("BLOCK=%d,curpos=%d,at=%d\n", block, start, end);
				return block;
			}

			if (tk == TK_WHILE) {
				program_add_opcode(pp, OP_JMP);
				program_add_int(pp, cond_start);
			}
			//set all the temporary jumps (0) to the actual over skipping location
			vm_printf("num_jumps=%d\n", num_jumps);
			for (int i = 0; i < num_jumps; i++) {
				vm_printf("jump [type: %s], modif_loc=%d, jmp_to=%d\n", lex_token_strings[jumps[i].type], jumps[i].modif_location, jumps[i].jump_location);
#if 1
				if (jumps[i].type == TK_AND_AND)
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
	}
	else if (pp_accept(pp, TK_R_THREAD)) {
		is_thread_call = true;
		pp_accept(pp, TK_IDENT);
		goto accept_ident;
	}
	else if (pp_accept(pp, TK_IDENT)) {
	accept_ident:
		parser_function_call(pp, pp->string, is_thread_call, false); //we shouldn't be allowed to make functions here right
	}
	else if (pp_accept(pp, TK_RETURN)) {
		if (!pp_accept(pp, TK_SEMICOLON)) {
			if (parser_expression(pp))
				goto unexpected_tkn;
			pp_expect(pp, TK_SEMICOLON);
		}
		else {
			program_add_opcode(pp, OP_PUSH_NULL);
		}
		program_add_opcode(pp, OP_RET);
	}
	else {
	unexpected_tkn:
		parser_display_history(pp);
		int lineno = parser_get_location(pp);

		vm_printf("unexpected token: %s at %d (%c) at line %d\n", lex_token_strings[pp->token], pp->curpos, pp->scriptbuffer[pp->curpos], lineno);
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

int read_text_file(const char *filename, char **buf, int *filesize) {
#if 1
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL)
		return 1;
	fseek(fp, 0, SEEK_END);
	*filesize = ftell(fp);
	rewind(fp);
	char *p = (char*)malloc(*filesize + 1);
	size_t res = fread(p, 1, *filesize, fp);
	fclose(fp);
	p[*filesize] = '\0';
	*buf = p;
	return 0;
#endif
	return 0;
}

typedef struct {
	char *buffer;
	size_t capacity;
	size_t size;
} kstring_t; //kinda string

void kstring_resize(kstring_t *k, int spaceneeded)
{
	int nn = k->size + spaceneeded;
	if (nn >= k->capacity || !k->buffer) {
		//we need a resize
		int nc = k->capacity + (nn * 2) + 1;
		char *n = xmalloc(nc);
		if (!n)
			return;
		if (k->buffer)
			memcpy(n, k->buffer, k->size);
		xfree(k->buffer);
		k->buffer = n;
		k->capacity = nc;
	}
}

void kstring_push_back(kstring_t *k, int c) {
	kstring_resize(k, 1);
	k->buffer[k->size] = c;
	++k->size;
	k->buffer[k->size] = '\0';
}

void kstring_add(kstring_t *k, const char *s) {
	size_t sz = strlen(s);
	kstring_resize(k, sz);

	strncpy(&k->buffer[k->size], s, sz);
	k->size += sz;
	k->buffer[k->size] = 0;
}

void kstring_addn(kstring_t *k, const char *s, int i) {
	size_t sz = strlen(s);
	if (i <= sz)
		sz = i;

	kstring_resize(k, sz);

	strncpy(&k->buffer[k->size], s, sz);
	k->size += sz;
	k->buffer[k->size] = 0;
}

void kstring_init(kstring_t *k) {
	k->buffer = 0;
	k->size = 0;
	k->capacity = 0;
	kstring_resize(k, 32);
}

void kstring_free(kstring_t *k)
{
	xfree(k->buffer);
	k->buffer = NULL;
}

void kstring_clear(kstring_t *k)
{
	k->size = 0;
	k->capacity = 0;
	kstring_free(k);
}

size_t kstring_length(kstring_t *k) { return k->size; }

void kstring_set(kstring_t *k, const char *s)
{
	kstring_clear(k);
	kstring_add(k, s);
}

const char *kstring_get(kstring_t *k)
{
	return k->buffer;
}

void kstring_addk(kstring_t *k, kstring_t *s) {
	size_t sz = kstring_length(s);
	kstring_resize(k, sz);

	strncpy(&k->buffer[k->size], kstring_get(s), sz);
	k->size += sz;
	k->buffer[k->size] = 0;
}

typedef enum {
	PRE_ERR_EXPECTED_IDENTIFIER,
} e_pre_err;

typedef struct {
	char name[256];
	kstring_t value;
} def_t;

typedef struct {
	kstring_t macro;
	kstring_t contents;
} pre_t;

void pre_clear_libstrings(vector *libs)
{
	for (int i = 0; i < vector_count(libs); i++)
	{
		dynstring s = (dynstring)vector_get(libs, i);
		dynfree(&s);
	}
}

void pre_clear_defines(vector *def)
{
	for (int i = 0; i < vector_count(def); ++i) {
		def_t *d = (def_t*)vector_get(def, i);
		kstring_free(&d->value);
		xfree(d);
	}
	vector_free(def);
}

int pre_err(parser_t *pp, const char *errstr, ...) {

	char dest[1024 * 16];
	va_list argptr;
	if (errstr != NULL) {
		va_start(argptr, errstr);
		vsprintf(dest, errstr, argptr);
		va_end(argptr);
		vm_printf("%s\n", dest);
	}
	pre_t *pre = (pre_t*)pp->userdata;
	kstring_free(&pre->macro);
	kstring_free(&pre->contents);

	parser_cleanup(pp);
	return errstr == NULL ? 0 : 1;
}

int pp_locate(parser_t *pp, int token, int *invalid_tokens)
{
	int ret = -1;
	pp_save(pp);
	int prev = 0;
	while (1)
	{
		prev = pp->curpos;
		int t = parser_read_next_token(pp);

		bool brk = false;
		for (int i = 0; invalid_tokens && invalid_tokens[i] != TK_EOF; ++i)
		{
			if (invalid_tokens[i] == t) { brk = true; break; }
		}
		if (brk || t == TK_EOF) break;

		if (t == token) {
			ret = prev;
			break;
		}
	}
	pp_restore(pp);
	return ret;
}

static int pre_buf(vm_compiler_opts_t *opts, char *buf, size_t sz, kstring_t *out, vector* defines, vector *libs)
{
	parser_t *pp = (parser_t*)xmalloc(sizeof(parser_t));
	parser_init(pp);
	pp->flags |= PARSER_FLAG_TOKENIZE_NEWLINE;
	static int etokens[] = { TK_IDENT,TK_SHARP,TK_EOF };
	pp->enabledtokens = &etokens;
	pp->scriptbuffer = buf;
	pp->scriptbuffersize = sz;

	pre_t pre;
	kstring_init(&pre.contents);
	kstring_init(&pre.macro);

	pp->userdata = &pre;

	while (1) //TODO FIX THIS
	{
		if (pp_accept(pp, TK_EOF))
		{
			//vm_printf("breaking already\n");
			break;
		}
		kstring_clear(&pre.contents);
		kstring_clear(&pre.macro);
		//if (pp_accept(pp, TK_SHARP))
		int pft = pp_tell(pp); //position before token

		int token = parser_read_next_token(pp);
		//vm_printf("token = %s %s\n", lex_token_strings[token], pp->string);
		switch (token)
		{

		def_behav:
		default: {
			//just copy the tokens i guess
			//vm_printf("token=%s\n", lex_token_strings[token]);
			int cur = pp_tell(pp);
			for (int i = pft; i < cur; ++i)
				kstring_push_back(out, buf[i]);
		} break;

		case TK_IDENT: {
			//first go through our definitions and replace ones that we have
			int nd = vector_count(defines);
			bool ff = false;
			for (int i = nd; i--;)
			{
				def_t *d = (def_t*)vector_get(defines, i);
				if (!strcmp(d->name, pp->string))
				{
					//vm_printf("found definition %s\n", d->name);
					//add the definition
					kstring_addk(out, &d->value);
					ff = true;
				}
			}
			if (!ff)
				goto def_behav;
		} break;

		case TK_SHARP:
			if (!pp_accept(pp, TK_IDENT))
			{
				token = parser_read_next_token(pp);
				return pre_err(pp, "expected identifier got %s %d\n%s", lex_token_strings[token], token, &buf[pft]);
			}
			if (!strcmp(pp->string, "endif")) {

			}
			else if (!strcmp(pp->string, "else")) {

			}
			else if (!strcmp(pp->string, "pragma"))
			{
				if (!pp_accept(pp, TK_IDENT))
					return pre_err(pp, "expected identifier!\n");
				if (!strcmp(pp->string, "comment"))
				{
					if (!pp_accept(pp, TK_LPAREN))
						return pre_err(pp, "lparen!\n");

					if (!pp_accept(pp, TK_IDENT))
						return pre_err(pp, "comment type!\n");

					if (strcmp(pp->string, "lib"))
						return pre_err(pp, "unknown comment!\n");

					if (!pp_accept(pp, TK_COMMA))
						return pre_err(pp, "comma!\n");


					if (!pp_accept(pp, TK_STRING))
						return pre_err(pp, "libname!\n");

					const char *libname = pp->string;
					vector_add(libs, dynnew(libname));

					if (!pp_accept(pp, TK_RPAREN))
						return pre_err(pp, "rparen!\n");
				}
				else return pre_err(pp, "expected comment!\n");
			}
			else if (!strcmp(pp->string, "if")) {
				//skip till end of line
				int loc = pp_locate(pp, TK_NEWLINE, NULL);
				if (loc == -1)
					return pre_err(pp, "unexpected end of file");
				pp_goto(pp, loc);
			}
			else if (!strcmp(pp->string, "define"))
			{
				if (!pp_accept(pp, TK_IDENT))
					return pre_err(pp, "expected identifier, bad macro name!");

				kstring_set(&pre.macro, pp->string);

				//take everything of the rest till newline so we can copy it
				pp_save(pp);
				while (1) {
					int bs = pp_locate(pp, TK_BACKSLASH, NULL);
					int nl = pp_locate(pp, TK_NEWLINE, NULL);
					if (bs == -1 && nl == -1)
						return pre_err(pp, "unexpected end of file!");
					int low = nl;
					if (bs != -1 && bs < nl)
						low = bs;
					for (int i = sav; i < low; ++i) {
						kstring_push_back(&pre.contents, buf[i]);
					}
					if (bs != -1)
						sav = nl + 1;
					else
						sav = low + 1;
					pp_goto(pp, sav);
					if (bs == -1)
						break;
					if (nl < bs) //the newline comes before the backslash so we break
						break;
				}
				//fix maybe later require a space in the define?
				//vm_printf("DEFINE %s = %s\n", kstring_get(&pre.macro), kstring_get(&pre.contents));
				//pp_restore(pp);

				def_t *d = (def_t*)xmalloc(sizeof(def_t));
				snprintf(d->name, sizeof(d->name), "%s", kstring_get(&pre.macro));
				kstring_init(&d->value);
				kstring_set(&d->value, kstring_get(&pre.contents));//uhh okay
				vector_add(defines, d);
				//pp_goto(pp, loc); //cuz newline is 1 character here
			}
			else if (!strcmp(pp->string, "include")) {
				if (!pp_accept(pp, TK_LESS))
					return pre_err(pp, "expected <");
				//get everything inbetween here
				pp_save(pp);
				int illegal[2] = { TK_NEWLINE,TK_EOF };
				int loc = pp_locate(pp, TK_GREATER, illegal);
				if (loc == -1)
					return pre_err(pp, "error expecting greater, can't find it!");

				for (int i = sav; i < loc; i++) {
					kstring_push_back(&pre.contents, buf[i]);
				}

				//pp_restore(pp);
				pp_goto(pp, loc);
				if (!pp_accept(pp, TK_GREATER))
					return pre_err(pp, "expected >");

				int cur = pp_tell(pp);

				//vm_printf("contents=%s\n", kstring_get(&pre.contents));
				char *fbuf = NULL;
				size_t fsize = 0;
				if (opts->read_file(kstring_get(&pre.contents), &fbuf, (int*)&fsize))
				{
					return pre_err(pp, "failed to open include file '%s'!", kstring_get(&pre.contents));
				}
				kstring_t tmp;
				kstring_init(&tmp);

				//process this file aswell recursively
				if (pre_buf(opts, fbuf, fsize, &tmp, defines, libs))
				{
					kstring_free(&tmp);
					free(fbuf); //free mem
					return pre_err(pp, "failed to pre_buf recursively!");
				}
				free(fbuf); //free mem
				//vm_printf("tmp = %s\n", kstring_get(&tmp));
				kstring_addk(out, &tmp);
				kstring_free(&tmp);
			}
			else {
				return pre_err(pp, "unknown directive '%s'!", pp->string);
			}
			break;
		}
	}
	return pre_err(pp, NULL);
}

#if 0
int parser_compile_string(const char *scriptbuf, char **out_program, int *out_program_size, vm_compiler_opts_t *opts)
{
	return 1;
}

int parser_compile(const char *filename, char **out_program, int *out_program_size)
{
	char *sb;
	size_t sbs;
	if (read_text_file(filename, &sb, (int*)&sbs)) {
		vm_printf("failed to read file '%s'\n", filename);
		return 1;
	}
	int ret = parser_compile_string(sb, out_program, out_program_size, NULL);
	free(sb); //free mem
	return ret;
}
#endif

int compiler_init(compiler_t *c, vm_compiler_opts_t *opts)
{
	static vm_compiler_opts_t defaultopts = {
		.read_file = read_text_file
	};
	memset(c, 0, sizeof(*c));
	array_init(&c->sources, source_t);
	if (!opts)
		c->opts = defaultopts;
	else
		c->opts = *opts;
	c->program = NULL;
	c->program_size = 0;
	//ts_init(&c->stream);
	return 0;
}

void compiler_cleanup(compiler_t *c)
{
	//ts_free(&c->stream);
	xfree(c->program);
	for (unsigned int i = 0; i < c->sources.size; ++i)
	{
		array_get(&c->sources, source_t, src, i);
		free(src->buffer);
	}
	array_free(&c->sources);
}

int compiler_add_source(compiler_t *c, const char *scriptbuf, const char *src_tag)
{
	source_t src;
	src.buffer = strdup(scriptbuf);
	src.size = strlen(src.buffer) + 1;
	src.tag[0] = '\0';
	if (src_tag)
		snprintf(src.tag, sizeof(src.tag), "%s", src_tag);
	array_push(&c->sources, &src);
	return 0;
}

int compiler_add_source_file(compiler_t *c, const char *filename)
{
	char *sb;
	size_t sbs;
	if (read_text_file(filename, &sb, (int*)&sbs)) {
		vm_printf("failed to read file '%s'\n", filename);
		return 1;
	}
	int status = compiler_add_source(c, sb, filename);
	free(sb); //free mem
	return status;
}

int compiler_execute(compiler_t *c)
{
	int retcode = 0;

	parser_t *pp = (parser_t*)xmalloc(sizeof(parser_t));
	parser_init(pp);
	
	unsigned int totalsrcsize = 0;
	for (unsigned int i = 0; i < c->sources.size; ++i)
	{
		array_get(&c->sources, source_t, src, i);
		totalsrcsize += src->size;
	}
	pp->program = (char*)xmalloc(totalsrcsize * 4); //i don't think the actual program will be this size lol
	memset(pp->program, OP_NOP, totalsrcsize * 4);

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

	vector libstrings;
	vector_init(&libstrings);

	for (unsigned int i = 0; i < c->sources.size; ++i)
	{
		array_get(&c->sources, source_t, src, i);
		pp->curpos = 0;
		pp->scriptbuffer = src->buffer;
		pp->scriptbuffersize = src->size;

		vector defines;
		vector_init(&defines);
		kstring_t processed;
		kstring_init(&processed);

		if (pre_buf(&c->opts, pp->scriptbuffer, pp->scriptbuffersize, &processed, &defines, &libstrings)) {
			xfree(pp->program);
			kstring_free(&processed);
			pre_clear_defines(&defines);
			pre_clear_libstrings(&libstrings);
			vector_free(&libstrings);
			return 1;
		}

		//TODO this is leaking memory when it fails
		//FIX libstrings
		//* fixed guess just cleared when pre_buf fails

		pre_clear_defines(&defines);
		//xfree(pp->scriptbuffer);
		pp->scriptbuffer = kstring_get(&processed);
		//vm_printf("processed = %s\n", pp->scriptbuffer);
		pp->scriptbuffersize = kstring_length(&processed);
		//return 1;
		//vm_printf("Compiling '%s'\n", pp->scriptbuffer);
		//vm_printf("scriptbuffer size = %d\n",pp->scriptbuffersize);
		retcode = parser_block(pp, pp->curpos, pp->scriptbuffersize);
		if (retcode)
		{
			xfree(pp->program);
			kstring_free(&processed);
			pre_clear_defines(&defines);
			pre_clear_libstrings(&libstrings);
			vector_free(&libstrings);
			return 1;
		}
		kstring_free(&processed);
	}

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
	size_t program_size;
	size_t *out_program_size = &program_size;
	//*out_program_size = total_size + sizeof(int) + sizeof(int) + sizeof(int);
	*out_program_size = total_size + sizeof(int) + sizeof(int) + sizeof(int);
#if 1
	tinystream_t ts;
	ts_init(&ts);
	//vm_printf("structs size = %d\n", pp->structs.size);
	ts32(&ts, vector_count(&libstrings));
	for (int i = 0; i < vector_count(&libstrings); i++)
	{
		dynstring s = (dynstring)vector_get(&libstrings, i);
		//vm_printf("adding s = %s\n", s);
		for (int k = 0; k < dynlen(&s); k++)
		{
			ts8(&ts, s[k]);
		}
		ts8(&ts, 0);
		dynfree(&s);
	}
	ts32(&ts, pp->structs.size);
	for (int i = 0; i < pp->structs.size; i++)
	{
		array_get(&pp->structs, cstruct_t, cs, i);
		for (int j = 0; j < strlen(cs->name); j++)
		{
			ts8(&ts, cs->name[j]);
		}
		ts8(&ts, 0); //end of name
		ts32(&ts, cs->size);
		ts16(&ts, cs->fields.size); //numfields
		for (int k = 0; k < cs->fields.size; k++)
		{
			array_get(&cs->fields, cstructfield_t, field, k);
			for (int j = 0; j < strlen(field->name); j++)
			{
				ts8(&ts, field->name[j]);
			}
			ts8(&ts, 0);
			ts32(&ts, field->offset);
			ts32(&ts, field->size);
			//vm_printf("field->type=%d\n", field->type);
			ts32(&ts, field->type);
			ts32(&ts, 0); //TODO FIX THIS
		}
	}
	pre_clear_libstrings(&libstrings);
	vector_free(&libstrings);
	*out_program_size += (ts.buffer.size + sizeof(int));
#endif
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

			rearranged_program[rearranged_program_pc++] = opcode;
		}
	}

	*(int*)(&rearranged_program[0] + 1) = rearrange_main;//call <loc> replace?
	int start_structs = rearranged_program_pc;
	memcpy(&rearranged_program[rearranged_program_pc], ts.buffer.data, ts.buffer.size);
	rearranged_program_pc += ts.buffer.size;

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
	rearranged_program_pc += sizeof(int);
	*(int*)(rearranged_program + rearranged_program_pc) = start_structs;
	rearranged_program_pc += sizeof(int);

	ts_free(&ts);
	xfree(pp->program);

	//*out_program = rearranged_program;
	c->program = rearranged_program;
	c->program_size = rearranged_program_pc;
	if (pp->error)
		retcode = 1;
	parser_cleanup(pp);
	return retcode;
}