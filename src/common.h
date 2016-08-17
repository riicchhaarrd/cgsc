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