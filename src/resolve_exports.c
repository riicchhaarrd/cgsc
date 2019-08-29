#ifndef _WIN32
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "virtual_machine.h"
#include "common.h"
#include "dynarray.h"

extern void *dlsym(void *handle, const char *symbol);

typedef uint32_t	u32;
typedef unsigned char	u8;
typedef uint16_t	u16;
typedef const char*		string;
typedef unsigned char	byte;

typedef struct
{
	u32 p_type;
	//u32 p_flags; //only on x64
	u32 p_offset;
	u32 p_vaddr;
	u32 p_paddr;
	u32 p_filesz;
	u32 p_memsz;
	u32 p_flags; //only at this place on x86
	u32 p_align;
} elfx86_phdr_t;

typedef enum
{
	SHT_NULL,
	SHT_PROGBITS,
	SHT_SYMTAB,
	SHT_STRTAB,
	SHT_RELA,
	SHT_HASH,
	SHT_DYNAMIC,
	SHT_NOTE,
	SHT_NOBITS,
	SHT_REL,
	SHT_SHLIB,
	SHT_DYNSYM,
	SHT_INIT_ARRAY,
	SHT_FINI_ARRAY,
	SHT_PREINIT_ARRAY,
	SHT_GROUP,
	SHT_SYMTAB_SHNDX,
	SHT_NUM,
	SHT_LOOS = 0x60000000
} e_elf_section_header_type;

static const char *elf_section_header_type_strings[] =
{
	"null", "progbits", "symtab", "strtab", "rela", "hash", "dynamic", "note", "nobits", "rel", "shlib", "dynsym", "init_array",
	"fini_array", "preinit_array", "group", "symtab_shndx", "num", NULL
};

static void elf_get_section_header_type_string(int type, char *s, size_t sz)
{
	if (type <= SHT_NUM)
		snprintf(s, sz, "%s", elf_section_header_type_strings[type]);
	else if (type == SHT_LOOS)
		snprintf(s, sz, "loos");
	else
		snprintf(s, sz, "unknown type %02X", type);
}

#define SHF_WRITE (1<<0)
#define SHF_ALLOC (1<<1)
#define SHF_EXECINSTR (1<<2)
#define SHF_MERGE (1<<3)
#define SHF_STRINGS (1<<4)
#define SHF_INFO_LINK (1<<5)
#define SHF_LINK_ORDER (1<<6)
#define SHF_OS_NONCONFORMING (1<<7)
#define SHF_GROUP (1<<8)
#define SHF_TLS (1<<9)
#define SHF_MASKOS (1<<10)
#define SHF_MASKPROC (1<<11)
#define SHF_ORDERED (1<<12)
#define SHF_EXCLUDE (1<<13) //solaris

typedef struct
{
	u32 sh_name;
	u32 sh_type;
	u32 sh_flags;
	u32 sh_addr;
	u32 sh_offset;
	u32 sh_size;
	u32 sh_link;
	u32 sh_info;
	u32 sh_addralign;
	u32 sh_entsize;
} elfx86_shdr_t;

typedef struct
{
	char magic[4];
	u8 class;
	u8 endianness;
	u8 version;
	u8 osabi;
	u8 abiversion;
	u8 pad_unused[7];
	u16 e_type;
	u16 e_machine;
	u32 e_version;
	u32 e_entry;
	u32 e_phoff;
	u32 e_shoff;
	u32 e_flags;
	u16 e_ehsize;
	u16 e_phentsize;
	u16 e_phnum;
	u16 e_shentsize;
	u16 e_shnum;
	u16 e_shstrndx;
} elfx86_t;

typedef struct
{
	u32 st_name;
	u32 st_value;
	u32 st_size;
	u8 st_info;
	u8 st_other;
	u16 st_shndx;
} elfx86_sym_t;

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_LOOS 10
#define STB_HIOS 12
#define STB_LOPROC 13
#define STB_HIPROC 15

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_LOOS 10
#define STT_HIOS 12
#define STT_LOPROC 13
#define STT_HIPROC 15

/* http://www.sco.com/developers/gabi/1998-04-29/ch4.symtab.html */
#define ELF32_ST_BIND(i)   ((i)>>4)
#define ELF32_ST_TYPE(i)   ((i)&0xf)
#define ELF32_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

static int read_binary_file(const char *filename, u8 **buf, size_t *fs)
{
	FILE *fp = fopen(filename, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		*fs = ftell(fp);
		rewind(fp);
		*buf = (u8*)malloc(*fs);
		fread(*buf, 1, *fs, fp);
		return 0;
	}
	return 1;
}

static void free_binary_file(u8 **buf)
{
	free((void*)*buf);
}

#define ELF_SECTION(hdr, index) \
(elfx86_shdr_t*)( ((u32)hdr) + hdr->e_shoff + index * sizeof(elfx86_shdr_t) )

static void *elf_get_section(elfx86_t *hdr, int section)
{
	elfx86_shdr_t *shdr = ELF_SECTION(hdr, section);
	return (void*)(((u32)hdr) + shdr->sh_offset);
}

static int elf_find_section_by_name(elfx86_t *hdr, const char *name)
{
	char *string_table = (char*)elf_get_section(hdr, hdr->e_shstrndx);
	for (u32 i = 0; i < hdr->e_shnum; ++i)
	{
		elfx86_shdr_t *shdr = ELF_SECTION(hdr, i);
		char *section_name = string_table + shdr->sh_name;

		if (!strcmp(section_name, name))
			return i;
	}
	return -1;
}

void read_elf_exported_symbols(vm_t *vm, vm_ffi_lib_t *l, const char* filename)
{
	u8 *filebuf;
	size_t filelen;
	if (read_binary_file(filename, &filebuf, &filelen) != 0)
	{
		vm_printf("failed to open filename %s\n", filename);
		return;
	}

	elfx86_t *hdr = (elfx86_t*)filebuf;
	if (*(unsigned int*)hdr->magic == 0x464c457f) //won't work for big endian, oh well
	{
		//vm_printf("shstrndx = %d\n", hdr->e_shstrndx);
		char *string_table = (char*)elf_get_section(hdr, hdr->e_shstrndx);
		for (u32 i = 0; i < hdr->e_shnum; ++i)
		{
			elfx86_shdr_t *shdr = ELF_SECTION(hdr, i);
			char *section_name = string_table + shdr->sh_name;
			char typestr[256] = { 0 };
			elf_get_section_header_type_string(shdr->sh_type, typestr, sizeof(typestr));
#if 0
			if (i == hdr->e_shstrndx)
				vm_printf("\tsection: %d %s (type=%s)\n", shdr->sh_name, section_name, typestr);
			else
				vm_printf("section: %d %s (type=%s)\n", shdr->sh_name, section_name, typestr);
#endif

			if (!strcmp(section_name, ".dynsym"))
			{
				int idx = elf_find_section_by_name(hdr, ".dynstr");
				if (idx != -1)
				{
					char *dyn_string_table = (char*)elf_get_section(hdr, idx);
					//vm_printf("section: %d %s (type=%s)\n", shdr->sh_name, section_name, typestr);
					if (shdr->sh_size == 0)
					{
						vm_printf("empty sh_size!\n");
					}
					else
					{
						u32 numsymbols = shdr->sh_size / sizeof(elfx86_sym_t);
						if (shdr->sh_entsize != sizeof(elfx86_sym_t))
						{
							vm_printf("error, sizes don't match!\n");
							break;
						}
						//vm_printf("numsymbols = %d, sh_size = %d\n", numsymbols, shdr->sh_size);
						for (u32 k = 1; k < numsymbols; ++k) //first is always reserved SHN_UNDEF
						{
							elfx86_sym_t *sym = (elfx86_sym_t*)elf_get_section(hdr, i) + k;
							if (
								1//ELF32_ST_BIND(sym->st_info) == STB_GLOBAL
								//|| ELF32_ST_BIND(sym->st_info) == STB_WEAK
								//&& ELF32_ST_TYPE(sym->st_info) == STT_FUNC
								)
							{
								const char *symbol_name = sym->st_name + dyn_string_table;
#if 0
								vm_printf("\tsymbol: %s, value: %02X, size: %02X, info: %d, other: %d, shndx: %d\n", symbol_name,
									sym->st_value,
									sym->st_size,
									sym->st_info,
									sym->st_other,
									sym->st_shndx
								);
#endif
								//vm_printf("\tsymbol: %s\n", symbol_name);
								vm_ffi_lib_func_t func;
								snprintf(func.name, sizeof(func.name), "%s", symbol_name);
								func.address = (void*)dlsym(l->handle, symbol_name);
								if (!func.address)
								{
									//vm_printf("failed to load function '%s' from '%s'\n", symbol_name, l->name);
									continue;
								}
								//vm_printf("Export: %s\n", (BYTE *)lib + (int)names[i]);
								func.hash = hash_string(func.name);
								array_push(&l->functions, &func);
							}
						}
					}
				}
				else
					vm_printf("unable to find .dynstr!\n");
			}
		}
	}
	else
		vm_printf("invalid elf header!\n");

	free_binary_file(&filebuf);
}

#if 0
int main(int argc, char **argv)
{
	const char *path = "/lib/i386-linux-gnu/libc.so.6";
	if (argc > 1)
		path = argv[1];

	read_elf_header(path);
	return 0;
}
#endif
#endif