#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stdint.h>
#include <stddef.h>

#define ARCHIVE_MAGIC "HUFF"
#define ARCHIVE_VERSION 0x01
#define MAX_PATH_LENGTH 4096

typedef struct {
    char path[MAX_PATH_LENGTH];
    uint64_t original_size;
    uint64_t compressed_size;
    uint64_t data_offset;
} FileEntry;

typedef struct {
    char magic[4];
    uint8_t version;
    uint32_t num_files;
} ArchiveHeader;

int createArchive(const char *archive_path, const char **input_paths, int num_inputs);
int listArchive(const char *archive_path);
int extractArchive(const char *archive_path, const char *output_dir);
int extractFile(const char *archive_path, const char *file_path, const char *output_path);

#endif // ARCHIVE_H
