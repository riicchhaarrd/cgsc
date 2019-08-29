#include "common.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#include <errno.h>
#endif

int(*g_printf_hook)(const char *, ...) = (int(*)(const char*, ...))printf;

#include "dynstring.h"

void dynaddn(dynstring *s, const char *str, size_t n)
{
        int sl = strlen(str);
        if (n >= sl)
                n = sl;

        if (*s == NULL)
                *s = dynalloc(n);

        for (int i = 0; i < n; ++i)
        {
                dynpush(s, *(char*)(str + i));
        }
}

void dynadd(dynstring *s, const char *str)
{
        dynaddn(s, str, strlen(str));
}

unsigned long hash_string(const char *str)
{
	unsigned long hash = 5381;
	int c;

	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

int sys_get_files_from_path(const char *path, file_info_t **files, size_t *outnumfiles) {
	*files = NULL;
	*outnumfiles = 0;
	size_t numfiles = 0;

#ifdef _WIN32

	WIN32_FIND_DATAA ffd;
	LARGE_INTEGER filesize;
	HANDLE hFind = INVALID_HANDLE_VALUE;

	DWORD dwError = 0;

	char sigh_path[MAX_PATH] = { 0 };
	snprintf(sigh_path, MAX_PATH, "%s\\*.*", path);

	hFind = FindFirstFileA(sigh_path, &ffd);

	if (hFind == INVALID_HANDLE_VALUE)
		return 1;

	do {
		//if (*ffd.cFileName == '.' || (*ffd.cFileName == '.' && ffd.cFileName[1] == '.'))
			//continue;

		*files = (file_info_t*)realloc(*files, sizeof(file_info_t) * ++numfiles);
		file_info_t *fi = &(*files)[numfiles - 1];

		snprintf(fi->name, sizeof(fi->name), "%s", ffd.cFileName);

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			fi->size = 0;
			fi->type = FI_TYPE_DIRECTORY;
		} else {
			filesize.LowPart = ffd.nFileSizeLow;
			filesize.HighPart = ffd.nFileSizeHigh;
			fi->size = filesize.QuadPart;
			fi->type = FI_TYPE_FILE;
		}
	} while (FindNextFileA(hFind, &ffd) != 0);
	FindClose(hFind);
#else

	DIR           *d = NULL;
	struct dirent *dir = NULL;

	d = opendir(path);
	if (d) {
		while ((dir = readdir(d)) != NULL) {

			*files = (file_info_t*)realloc(*files, sizeof(file_info_t) * ++numfiles);
			file_info_t *fi = &(*files)[numfiles - 1];

			snprintf(fi->name, sizeof(fi->name), "%s", dir->d_name);
			fi->size = 0; //use stat here someday
			if (dir->d_type == DT_REG)
				fi->type = FI_TYPE_FILE;
			else if (dir->d_type == DT_DIR)
				fi->type = FI_TYPE_DIRECTORY;
		}

		closedir(d);
	} else {
		vm_printf("failed opendir, error=%s\n", strerror(errno));
		return 1;
	}
#endif
	*outnumfiles = numfiles;
	return 0;
}

#ifdef _WIN32
#include <windows.h>
#elif _POSIX_C_SOURCE >= 199309L
#include <sys/time.h>
#include <time.h>   // for nanosleep
#else
#include <unistd.h> // for usleep
#endif

void sys_sleep(int msec) {
#ifdef WIN32
	Sleep(msec);
#elif _POSIX_C_SOURCE >= 199309L
	struct timespec ts;
	ts.tv_sec = msec / 1000;
	ts.tv_nsec = (msec % 1000) * 1000000;
	nanosleep(&ts, NULL);
#else
	usleep(msec * 1000);
#endif
}
