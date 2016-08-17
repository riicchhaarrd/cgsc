#pragma once

#include "stdheader.h"

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