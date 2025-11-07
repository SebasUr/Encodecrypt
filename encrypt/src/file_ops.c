#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include "aes.h"
#include "file_ops.h"

#define BUFFER_SIZE 4096

void print_usage(const char *program_name) {
    printf("Usage: %s -e <file|directory> -p <password>\n", program_name);
    printf("Options:\n");
    printf("  -e <path>      File or directory to encrypt\n");
    printf("  -p <password>  Password for encryption\n");
}

void derive_key_from_password(const char *password, uint8_t *key) {
    size_t len = strlen(password);
    for (size_t i = 0; i < 16; i++) {
        key[i] = (i < len) ? (uint8_t)password[i] : 0;
        key[i] ^= (uint8_t)((i * 0x1B) % 256);
        key[i] = (uint8_t)((key[i] << 3) | (key[i] >> 5));
    }
}

int encrypt_file(const char *filename, const uint8_t *key) {
    int fd_in, fd_out;
    struct stat st;
    uint8_t *file_content;
    ssize_t bytes_read, total_bytes = 0;

    // Open input file
    fd_in = open(filename, O_RDONLY);
    if (fd_in == -1) {
        perror("Error opening input file");
        return -1;
    }

    // Get file size
    if (fstat(fd_in, &st) == -1) {
        perror("Error getting file stats");
        close(fd_in);
        return -1;
    }

    // Allocate memory for file content
    file_content = (uint8_t*)malloc((size_t)st.st_size);
    if (!file_content) {
        printf("Error: Memory allocation failed\n");
        close(fd_in);
        return -1;
    }

    // Read file content
    while ((bytes_read = read(fd_in, file_content + total_bytes, BUFFER_SIZE)) > 0) {
        total_bytes += bytes_read;
    }

    if (bytes_read == -1) {
        perror("Error reading file");
        free(file_content);
        close(fd_in);
        return -1;
    }

    close(fd_in);

    // Calculate number of blocks
    int num_blocks = (int)((total_bytes + 15) / 16);

    // Encrypt each block
    for (int i = 0; i < num_blocks; i++) {
        uint8_t block[16] = {0};
        uint8_t encrypted_block[16] = {0};
        int block_size = (i == num_blocks - 1 && total_bytes % 16 != 0) ? (int)(total_bytes % 16) : 16;

        memcpy(block, file_content + i * 16, (size_t)block_size);
        encrypt(block, key, encrypted_block);
        memcpy(file_content + i * 16, encrypted_block, 16);
    }

    // Create output filename
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "%s.enc", filename);

    // Write encrypted file
    fd_out = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        perror("Error creating output file");
        free(file_content);
        return -1;
    }

    ssize_t bytes_written = write(fd_out, file_content, (size_t)num_blocks * 16);
    if (bytes_written == -1) {
        perror("Error writing file");
        free(file_content);
        close(fd_out);
        return -1;
    }

    close(fd_out);
    free(file_content);

    printf("File encrypted successfully: %s\n", output_filename);
    return 0;
}

int encrypt_directory(const char *dirpath, const char *password) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char fullpath[1024];
    uint8_t key[16];

    derive_key_from_password(password, key);

    dir = opendir(dirpath);
    if (!dir) {
        perror("Error opening directory");
        return -1;
    }

    printf("Encrypting files in directory: %s\n", dirpath);

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        // Check if it's a regular file
        if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
            printf("Encrypting: %s\n", entry->d_name);
            encrypt_file(fullpath, key);
        }
    }

    closedir(dir);
    return 0;
}

int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}
