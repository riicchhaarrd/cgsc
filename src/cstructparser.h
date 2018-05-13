#pragma once
#include "dynstring.h"
#include "dynarray.h"
#include <stdbool.h>
#define CSTRUCTPARSER_FUNCTION static

#define SOURCE_STRING(...) #__VA_ARGS__
#if 1
static const char *def = SOURCE_STRING(
	typedef struct {
	qboolean	allowoverflow;	// if false, do a Com_Error
	qboolean	overflowed;		// set to true if the buffer size failed (with allowoverflow set)
	qboolean	oob;			// set to true if the buffer size failed (with allowoverflow set)
	byte	*data;
	int		maxsize;
	int		cursize;
	int		readcount;
	int		bit;				// for bitwise reads and writes
} msg_t;


typedef struct tagWNDCLASSA {
	UINT        style;
	WNDPROC     lpfnWndProc;
	int         cbClsExtra;
	int         cbWndExtra;
	HINSTANCE   hInstance;
	HICON       hIcon;
	HCURSOR     hCursor;
	HBRUSH      hbrBackground;
	LPCSTR      lpszMenuName;
	LPCSTR      lpszClassName;
} WNDCLASSA, *PWNDCLASSA, NEAR *NPWNDCLASSA, FAR *LPWNDCLASSA;
typedef struct tagWNDCLASSW {
	UINT        style;
	WNDPROC     lpfnWndProc;
	int         cbClsExtra;
	int         cbWndExtra;
	HINSTANCE   hInstance;
	HICON       hIcon;
	HCURSOR     hCursor;
	HBRUSH      hbrBackground;
	LPCWSTR     lpszMenuName;
	LPCWSTR     lpszClassName;
} WNDCLASSW, *PWNDCLASSW, NEAR *NPWNDCLASSW, FAR *LPWNDCLASSW;

typedef struct
{
	int size;
	int capacity;
	char buf[];
} dynstringhdr_t;

struct test
{
	int a, b, c, d, e, f, g;
};
struct  hostent {
	char    FAR * h_name;           /* official name of host */
	char    FAR * FAR * h_aliases;  /* alias list */
	short   h_addrtype;             /* host address type */
	short   h_length;               /* length of address */
	char    FAR * FAR * h_addr_list; /* list of addresses */
};
);
#else
static const char *def = SOURCE_STRING(
	typedef struct
{
	int size;
	char test[1];
	int capacity;
} kek_t;
);
#endif

typedef struct {
	char *buf;
	int pos;
} parsestate_t;

CSTRUCTPARSER_FUNCTION void init_parser(parsestate_t *ps, const char *buf)
{
	ps->buf = (char*)buf;
	ps->pos = 0;
}

typedef enum
{
	TOKEN_ILLEGAL,
	TOKEN_EOF,
	TOKEN_SEMICOLON,
	TOKEN_BRACE_LEFT,
	TOKEN_BRACE_RIGHT,
	TOKEN_IDENTIFIER,
	TOKEN_ASTERISK, //*
	TOKEN_BRACKET_LEFT, //[
	TOKEN_BRACKET_RIGHT, //]
	TOKEN_INT,
	TOKEN_COMMA,
	TOKEN_SLASH
} parse_tokens;

static const char *parse_token_strings[] =
{
	"illegal","eof", ";", "{", "}", "[identifier]", "*", ",", NULL
};

typedef enum
{
	CTYPE_NONE,
	CTYPE_VOID,
	CTYPE_CHAR,
	CTYPE_SHORT,
	CTYPE_INT,
	CTYPE_LONG,
	CTYPE_FLOAT,
	CTYPE_DOUBLE,

	CTYPE_UNSIGNED,
	CTYPE_POINTER
} ctype_t;

CSTRUCTPARSER_FUNCTION int ctype_get_size(int t)
{
	switch (t)
	{
	case CTYPE_NONE:
	case CTYPE_VOID:
		return 0;
	case CTYPE_CHAR:
		return sizeof(char);
	case CTYPE_SHORT:
		return sizeof(short);
	case CTYPE_INT:
	case CTYPE_FLOAT:
		return sizeof(int);
	case CTYPE_LONG:
	case CTYPE_DOUBLE:
		return sizeof(long long);
	case CTYPE_POINTER:
		return sizeof(void*);
	}
	return 0;
}

static const char *ctypestrings[] = {
	"none", "void", "char", "short", "int", "long", "float", "double", "unsigned", "pointer", NULL
};

typedef struct {
	int type;
	const char *keywords[12];
} ctypepair_t;

static const char *win32_handles[] =
{
	"HANDLE", "HBITMAP", "HBRUSH", "HCOLORSPACE", "HCONV", "HCONVLIST", "HCURSOR", "HDC", "HDDEDATA", "HDESK", "HDROP", "HDWP", "HENHMETAFILE", "HFILE", "HFONT",
	"HGDIOBJ", "HGLOBAL", "HHOOK", "HICON", "HINSTANCE", "HKEY", "HKL", "HLOCAL", "HMENU", "HMETAFILE", "HMODULE", "HMONITOR", "HPALETTE", "HPEN",
	"HRESULT", "HRGN", "HRSRC", "HSZ", "HWINSTA", "HWND", NULL
};

CSTRUCTPARSER_FUNCTION bool is_win32_handle(const char *s)
{
	for (int i = 0; win32_handles[i]; ++i)
	{
		if (!_stricmp(win32_handles[i], s))
			return true;
	}
	return false;
}

static const ctypepair_t ctypepairs[] =
{
	{ CTYPE_VOID,{ "void",0 } },
{ CTYPE_CHAR,{ "char","byte", "u8", "character", "int8_t", "uint8_t", "int8", "uint8", "i8", 0 } },
{ CTYPE_SHORT,{ "short", "int16_t", "uint16_t", "uint16", "int16", "i16","shortint",0 } },
{ CTYPE_INT,{ "int","uint", "int32_t", "dword", "uint32_t", "uint32", "int32", "integer", "i32", "qboolean", "long", 0 } },
{ CTYPE_LONG,{ "int64_t", "uint64_t", "qword", "long long", "longint",0 } },
{ CTYPE_FLOAT,{ "float",0 } },
{ CTYPE_DOUBLE,{ "double", "long float",0 } },
{ CTYPE_POINTER,{ "pointer", "ptr", "void*", "ULONG_PTR", "LONG_PTR", "UINT_PTR", "INT_PTR", "PVOID", "LPCSTR", "LPCWSTR", "WNDPROC",0 } },
{ CTYPE_NONE,{ 0 } }
};

CSTRUCTPARSER_FUNCTION bool isident(int c)
{
	if (isalnum(c) || c == '_' || c == '$')
		return true;
	return false;
}

CSTRUCTPARSER_FUNCTION int token(parsestate_t *ps, dynstring *s)
{
	dynfree(s);

#define nxt (ps->buf[ps->pos++])
#define pk (ps->buf[ps->pos])
	int c;
scan:
	switch ((c = nxt))
	{
	case '\r':
	case '\n':
	case ' ':
	case '\t':
		goto scan;
	case ',':
		return TOKEN_COMMA;
	case 0:
		return TOKEN_EOF;
	case ';':
		return TOKEN_SEMICOLON;
	case '*':
		return TOKEN_ASTERISK;
	case '{':
		return TOKEN_BRACE_LEFT;
	case '}':
		return TOKEN_BRACE_RIGHT;
	case ']':
		return TOKEN_BRACKET_RIGHT;
	case '[':
		return TOKEN_BRACKET_LEFT;
	case '/': {
		int peek = pk;
		if (peek != '/' && peek != '*')
			return TOKEN_SLASH;
		if (peek == '/')
		{
			while ((c = nxt) != '\n' && c)
				;
		}
		else
		{
		repeat:
			while ((c = nxt) != '*' && c)
				;
			if (pk != '/')
				goto repeat;
		}
		goto scan;
	} break;
	default:
	{
		if (c >= '0'&&c <= '9')
		{
			//it's a number
			if (!s)
				return TOKEN_INT;
			--ps->pos;
			while ((c = nxt) && c >= '0'&&c <= '9')
			{
				dynpush(s, c);
			}
			--ps->pos;
			return TOKEN_INT;
		}

		if (!s)
			return TOKEN_IDENTIFIER;
		--ps->pos;
		while (isident((c = nxt)))
		{
			dynpush(s, c);
		}
		--ps->pos;
		return TOKEN_IDENTIFIER;
	} break;
	}
	return TOKEN_EOF;
}

CSTRUCTPARSER_FUNCTION int get_ctype_for_keywords(dynstring *keywords, int n)
{
	int curtype = CTYPE_NONE;
	for (int i = 0; i < n; ++i)
	{
		if (is_win32_handle(keywords[i]))
			curtype = CTYPE_POINTER;
		else
		{
			for (int j = 0; ctypepairs[j].type != CTYPE_NONE; ++j)
			{
				for (int k = 0; ctypepairs[j].keywords[k]; ++k)
				{
					if (!_stricmp(ctypepairs[j].keywords[k], keywords[i]))
					{
						curtype = ctypepairs[j].type;
					}
				}
			}
		}

		if (!strcmp(keywords[i], "long"))
		{
			switch (curtype)
			{
			case CTYPE_INT:
			case CTYPE_LONG:
				curtype = CTYPE_LONG;
				break;
			case CTYPE_SHORT:
				curtype = CTYPE_INT;
				break;
			case CTYPE_FLOAT:
			case CTYPE_DOUBLE:
				curtype = CTYPE_DOUBLE;
				break;
			}
		}
		else if (!strcmp(keywords[i], "*"))
		{
			curtype = CTYPE_POINTER;
		}
	}
	return curtype;
}

CSTRUCTPARSER_FUNCTION bool ps_accept(parsestate_t *ps, int t, dynstring *s)
{
	if (s)
		*s = NULL;
	int pos = ps->pos;
	int tk = token(ps, s);
	if (t == tk)
	{
		return true;
	}
	else
	{
		;// printf("got %s\n", parse_token_strings[tk]);
	}
	dynfree(s);
	ps->pos = pos;
	return false;
}

CSTRUCTPARSER_FUNCTION int ps_peek(parsestate_t *ps)
{
	int pos = ps->pos;
	int tk = token(ps, NULL);
	ps->pos = pos;
	return tk;
}

CSTRUCTPARSER_FUNCTION int ps_locate(parsestate_t *ps, int t)
{
	int save = ps->pos;
	int tk = token(ps, NULL);
	int at = ps->pos;
	ps->pos = save;
	return at;
}

#if 1 //TODO
typedef struct {
	char name[256];
	int type;
	int offset;
	int size;
} cstructfield_t;

typedef struct {
	char name[64];
	int size;
	//cstructfield_t *fields;
	dynarray fields; //cstructfield_t
} cstruct_t;
#endif

CSTRUCTPARSER_FUNCTION int parse_err_free_strings(dynstring *strings, int n, dynarray *structs, const char *s)
{

	for (int i = 0; i < structs->size; i++)
	{
		array_get(structs, cstruct_t, cs, i);

		//printf("struct %s -> size = %d\n", cs->name, cs->size);

		array_free(&cs->fields);
	}
	array_free(structs);

	if (strings != NULL)
	{
		for (int i = 0; i < n; ++i)
			dynfree(&strings[i]);
	}
	printf("%s", s);
	return 0;
}

CSTRUCTPARSER_FUNCTION int cstructparser(const char *string, dynarray *structs, bool multiple)
{
	parsestate_t ps;
	init_parser(&ps, string);
	dynstring s = NULL;
	int tk;
	bool is_typedef = false;
	while ((tk = token(&ps, &s)) != TOKEN_EOF)
	{
		switch (tk)
		{
		case TOKEN_IDENTIFIER: {
			if (!strcmp(s, "typedef"))
			{
				is_typedef = true;
			}
			else if (!strcmp(s, "struct"))
			{
				cstruct_t cs;
				cstructfield_t field;
				array_init(&cs.fields, cstructfield_t);
				dynstring structName;
				int psa_sn = ps_accept(&ps, TOKEN_IDENTIFIER, &structName);

				if (!psa_sn && !is_typedef)
				{
					return parse_err_free_strings(NULL, 0, structs, "expected struct name identifier\n");
				}
				snprintf(cs.name, sizeof(cs.name), "%s", structName == NULL ? "unnamed struct" : structName);

				dynfree(&structName);

				is_typedef = false;

				if (!ps_accept(&ps, TOKEN_BRACE_LEFT, NULL))
				{
					return parse_err_free_strings(NULL, 0, structs, "expected left brace\n");
				}
				//parse members
				int loc = ps_locate(&ps, TOKEN_BRACE_RIGHT);
				if (loc == -1)
				{
					return parse_err_free_strings(NULL, 0, structs, "error missing }");
				}
				int mt;
				int memberoffset = 0;
				while (1) {
					//array_init(&cs.fields, cstructfield_t);
					mt = ps_peek(&ps);
					if (mt == TOKEN_BRACE_RIGHT)
						break;

					int scloc = ps_locate(&ps, TOKEN_SEMICOLON);

					if (scloc == -1)
					{
						return parse_err_free_strings(NULL, 0, structs, "could not find semicolon!");
					}

					dynstring idents[16] = { 0 };
					int n_ident = 0;
					bool is_array = false;
					int arraysize = 1;
					while (1)
					{
						if (n_ident >= 16)
							return parse_err_free_strings(idents, n_ident, structs, "n_ident >= sizeof(idents[0])\n");
						int gt = token(&ps, &idents[n_ident]);

						if (gt == TOKEN_SEMICOLON)
							break;

						if (gt == TOKEN_BRACKET_LEFT)
						{
							is_array = true;
							int lit = token(&ps, &idents[n_ident]);
							if (lit == TOKEN_BRACKET_RIGHT)
							{
								arraysize = 0;
								continue; //empty array
							}
							if (lit != TOKEN_INT)
								return parse_err_free_strings(idents, n_ident, structs, "expected int literal!\n");
							arraysize = atoi(idents[n_ident]);
							if (!ps_accept(&ps, TOKEN_BRACKET_RIGHT, NULL))
								return parse_err_free_strings(idents, n_ident, structs, "expected bracket right!\n");
							dynfree(&idents[n_ident]);
							continue;
						}
						if (gt == TOKEN_COMMA && n_ident > 1)
						{
							//add the var and free it and decrease n_ident
							--n_ident;
							dynstring varName = idents[n_ident];
							//printf("varName = %s\n", varName);

							int ctype = get_ctype_for_keywords(idents, n_ident);
							int sz = ctype_get_size(ctype) * arraysize;
							//printf("-> %s %s : %d %d\n", ctypestrings[ctype], varName, memberoffset, sz);
							snprintf(field.name, sizeof(field.name), "%s", varName);
							field.type = ctype;
							field.size = sz;
							field.offset = memberoffset;
							array_push(&cs.fields, &field);

							memberoffset += sz;

							dynfree(&idents[n_ident]);
							idents[n_ident] = NULL;
							continue;
						}

						if (gt != TOKEN_IDENTIFIER && gt != TOKEN_ASTERISK)
						{
							return parse_err_free_strings(idents, n_ident, structs, "expected identifier!");
						}
						if (gt == TOKEN_ASTERISK)
							idents[n_ident] = dynnew("*");
						//printf("got ident %s\n", idents[n_ident]);
						++n_ident;
					}
					dynstring variableName = idents[n_ident - 1];
					int ctype = get_ctype_for_keywords(idents, n_ident - 1);
					int sz = ctype_get_size(ctype) * arraysize;
					//printf("%s %s : %d %d\n", ctypestrings[ctype], variableName, memberoffset, sz);
					snprintf(field.name, sizeof(field.name), "%s", variableName);
					field.type = ctype;
					field.size = sz;
					field.offset = memberoffset;
					array_push(&cs.fields, &field);
					for (int i = 0; i < n_ident; ++i)
						dynfree(&idents[i]);
					memberoffset += sz;
				}
				if (!ps_accept(&ps, TOKEN_BRACE_RIGHT, NULL))
				{
					return parse_err_free_strings(NULL, 0, structs, "expected brace right!\n");
				}
				int lt = token(&ps, &structName);
				if (lt == TOKEN_IDENTIFIER)
				{
					snprintf(cs.name, sizeof(cs.name), "%s", structName);
				}
				dynfree(&structName);
				cs.size = memberoffset;
				array_push(structs, &cs);
				int sc = ps_locate(&ps, TOKEN_SEMICOLON);
				if (sc == -1)
				{
					return parse_err_free_strings(NULL, 0, structs, "expected semicolon!\n");
				}
				ps.pos = sc;
				if (!multiple)
				{
					dynfree(&s);
					return ps.pos;
				}
				//printf("struct size = %d %d\n", memberoffset, sizeof(kek_t));
			}
		} break;
		}
	}
	dynfree(&s);
	return 1;
}