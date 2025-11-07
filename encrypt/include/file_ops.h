#ifndef FILE_OPS_H
#define FILE_OPS_H

#include <stdint.h>

void print_usage(const char *program_name);

void derive_key_from_password(const char *password, uint8_t *key);

int process_file_syscalls(const char *filename, const uint8_t *key, int encrypt_mode);
int process_directory(const char *dirpath, const char *password, int encrypt_mode);

int is_directory(const char *path);

#endif
