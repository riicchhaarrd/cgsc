#pragma once
#include "stdheader.h"

typedef enum {
	FI_TYPE_FILE,
	FI_TYPE_DIRECTORY,
} file_info_type_t;

typedef struct {
	char name[256];
	file_info_type_t type;
	unsigned long long size;
} file_info_t;

int sys_get_files_from_path(const char *path, file_info_t **files, size_t *outnumfiles);
void sys_sleep(int);

int read_text_file(const char *filename, char **buf, int *filesize);
unsigned long hash_string(const char *);
