#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stddef.h>

unsigned char* readEntireFile(const char *path, size_t *out_size);
int ensureDirectoryExists(const char *path);
int isDirectory(const char *path);
char** expandPaths(const char **input_paths, int num_inputs, int *out_count);

#endif // FILE_UTILS_H
