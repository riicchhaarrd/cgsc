#pragma once
#include <stdint.h>

typedef enum
{
	REG_EAX,
	REG_ECX,
	REG_EDX,
	REG_EBX,
	REG_ESP,
	REG_EBP,
	REG_ESI,
	REG_EDI,
	REG_MAX
} x86_register_t;

typedef uint32_t u32;
typedef uint16_t u16;
typedef unsigned char u8;

void emit(u8 **p, u8 op);
void nop(u8 **p);
void dd(u8 **p, u32 imm);
void dw(u8 **p, u16 imm);
void inc(u8 **p, x86_register_t reg);
void dec(u8 **p, x86_register_t reg);
void push_imm(u8 **p, u32 imm);
void push(u8 **p, x86_register_t reg);
void pop(u8 **p, x86_register_t reg);
void ret(u8 **p, u16 n);
void jmp(u8 **p, u32 n);

void add_imm(u8 **p, x86_register_t reg, u32 imm);
void sub_imm(u8 **p, x86_register_t reg, u32 imm);