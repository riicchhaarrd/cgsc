#include "asm.h"

void emit(u8 **p, u8 op)
{
	**p = op;
	++*p;
}

void nop(u8 **p)
{
	emit(p, 0x90);
}

void dd(u8 **p, u32 imm)
{
	void **mem = *(void**)p;
	*(u32*)mem = imm;
	*p += sizeof(u32);
}

void dw(u8 **p, u16 imm)
{
	void **mem = *(void**)p;
	*(u16*)mem = imm;
	*p += sizeof(u16);
}

void inc(u8 **p, x86_register_t reg)
{
	emit(p, reg + 0x40);
}

void dec(u8 **p, x86_register_t reg)
{
	emit(p, reg + 0x48);
}

void push_imm(u8 **p, u32 imm)
{
	if (imm <= 0xff)
	{
		emit(p, 0x6a);
		emit(p, imm & 0xff);
	}
	else {
		emit(p, 0x68);
		dd(p, imm);
	}
}

/*
77AD0DE4   2D 00500600      SUB EAX,65000
77AD0DE9   2D 00650000      SUB EAX,6500
//opcode 2d is also a sub for dwords?
*/

void push(u8 **p, x86_register_t reg)
{
	emit(p, reg + 0x50);
}

void pop(u8 **p, x86_register_t reg)
{
	emit(p, reg + 0x58);
}

void mov(u8 **p, x86_register_t dest, x86_register_t src)
{
	emit(p, 0x8b);
	emit(p, REG_MAX * dest + 0xc0 + src);
}

#define DECLARE_IMM_MATH_STUFF(instr, opcode_base) \
void instr(u8 **p, x86_register_t reg, u32 imm) \
{ \
	if (imm <= 0xff) { \
		emit(p, 0x83); \
		emit(p, reg + opcode_base); \
		emit(p, imm & 0xff); \
	} else { \
		emit(p, 0x81); \
		emit(p, reg + opcode_base); \
		dd(p, imm); \
	} \
}

DECLARE_IMM_MATH_STUFF(sub_imm, 0xe8)
DECLARE_IMM_MATH_STUFF(add_imm, 0xc0)

void ret(u8 **p, u16 n)
{
	if (!n)
		emit(p, 0xc3);
	else {
		emit(p, 0xc2);
		dw(p, n);
	}
}

void jmp(u8 **p, u32 n)
{
	if (n <= 0xff)
	{
		emit(p, 0xeb);
		emit(p, n & 0xff);
	}
	else {
		emit(p, 0xe9);
		dd(p, n);
	}
}