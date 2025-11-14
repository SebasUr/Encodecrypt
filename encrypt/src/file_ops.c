#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>

#include "aes.h"
#include "file_ops.h"
#include "threadpool.h"

#define BUFFER_SIZE 4096
#define AES_BLOCK_SIZE 16
#define MIN_BLOCKS_FOR_PARALLEL 32
#define MAX_THREADPOOL_SIZE 32

typedef struct {
    char *filepath;
    uint8_t key[16];
    int encrypt_mode;
    int *error_counter;
} FileTaskArg;

typedef struct {
    uint8_t *block_ptr;
    size_t block_size;
    const uint8_t *key;
    int encrypt_mode;
} BlockTaskArg;

static int recommended_thread_count(void) {
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 2) {
        return 2;
    }
    if (cpu_count > MAX_THREADPOOL_SIZE) {
        cpu_count = MAX_THREADPOOL_SIZE;
    }
    return (int)cpu_count;
}

static void process_block_immediate(uint8_t *block_ptr, size_t block_size, const uint8_t *key, int encrypt_mode) {
    uint8_t block[AES_BLOCK_SIZE] = {0};
    uint8_t processed_block[AES_BLOCK_SIZE] = {0};

    memcpy(block, block_ptr, block_size);

    if (encrypt_mode) {
        encrypt(block, key, processed_block);
    } else {
        decrypt(block, key, processed_block);
    }

    memcpy(block_ptr, processed_block, AES_BLOCK_SIZE);
}

static void block_task_runner(void *arg) {
    BlockTaskArg *task = (BlockTaskArg*)arg;
    process_block_immediate(task->block_ptr, task->block_size, task->key, task->encrypt_mode);
    free(task);
}

static void directory_file_task(void *arg) {
    FileTaskArg *task = (FileTaskArg*)arg;
    int result = process_file_syscalls(task->filepath, task->key, task->encrypt_mode);
    if (result != 0) {
        __sync_fetch_and_add(task->error_counter, 1);
    }
    free(task->filepath);
    free(task);
}

void print_usage(const char *program_name) {
    printf("Usage: %s -e <file|directory> -p <password>  (for encryption)\n", program_name);
    printf("       %s -d <file|directory> -p <password>  (for decryption)\n", program_name);
    printf("Options:\n");
    printf("  -e <path>      Encrypt file or directory\n");
    printf("  -d <path>      Decrypt file or directory\n");
    printf("  -p <password>  Password for encryption/decryption\n");
}

void derive_key_from_password(const char *password, uint8_t *key) {
    size_t len = strlen(password);
    for (size_t i = 0; i < 16; i++) {
        key[i] = (i < len) ? (uint8_t)password[i] : 0;
        key[i] ^= (uint8_t)((i * 0x1B) % 256);
        key[i] = (uint8_t)((key[i] << 3) | (key[i] >> 5));
    }
}

int process_file_syscalls(const char *filename, const uint8_t *key, int encrypt_mode) {
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

    size_t file_size = (size_t)st.st_size;
    size_t padded_size = (file_size > 0)
        ? ((file_size + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE
        : AES_BLOCK_SIZE;

    // Allocate memory for file content (zero-padded to block size)
    file_content = (uint8_t*)calloc(padded_size, sizeof(uint8_t));
    if (!file_content) {
        printf("Error: Memory allocation failed\n");
        close(fd_in);
        return -1;
    }

    // Read file content using read() syscall
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
    int num_blocks = (int)((total_bytes + AES_BLOCK_SIZE - 1) / AES_BLOCK_SIZE);
    int block_threads = recommended_thread_count();
    bool parallel_blocks = (num_blocks >= MIN_BLOCKS_FOR_PARALLEL) && (block_threads > 1);
    ThreadPool *block_pool = NULL;

    if (parallel_blocks) {
        block_pool = threadPoolCreate(block_threads);
        if (!block_pool) {
            parallel_blocks = false;
        }
    }

    for (int i = 0; i < num_blocks; i++) {
        uint8_t *current_block = file_content + i * AES_BLOCK_SIZE;
        size_t block_size = (i == num_blocks - 1 && total_bytes % AES_BLOCK_SIZE != 0)
            ? (size_t)(total_bytes % AES_BLOCK_SIZE)
            : AES_BLOCK_SIZE;

        if (!parallel_blocks) {
            process_block_immediate(current_block, block_size, key, encrypt_mode);
            continue;
        }

        BlockTaskArg *task = (BlockTaskArg*)malloc(sizeof(BlockTaskArg));
        if (!task) {
            process_block_immediate(current_block, block_size, key, encrypt_mode);
            continue;
        }

        task->block_ptr = current_block;
        task->block_size = block_size;
        task->key = key;
        task->encrypt_mode = encrypt_mode;

        if (threadPoolAddJob(block_pool, block_task_runner, task) != 0) {
            free(task);
            process_block_immediate(current_block, block_size, key, encrypt_mode);
        }
    }

    if (block_pool) {
        threadPoolWait(block_pool);
        threadPoolDestroy(block_pool);
    }

    // Create output filename
    char output_filename[256];
    if (encrypt_mode) {
        snprintf(output_filename, sizeof(output_filename), "%s.enc", filename);
    } else {
        // Remove .enc extension if present
        size_t len = strlen(filename);
        if (len > 4 && strcmp(filename + len - 4, ".enc") == 0) {
            snprintf(output_filename, sizeof(output_filename), "%.*s", (int)(len - 4), filename);
        } else {
            snprintf(output_filename, sizeof(output_filename), "%s.dec", filename);
        }
    }

    // Write processed file using write() syscall
    fd_out = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        perror("Error creating output file");
        free(file_content);
        return -1;
    }

    ssize_t bytes_written = write(fd_out, file_content, (size_t)num_blocks * AES_BLOCK_SIZE);
    if (bytes_written == -1) {
        perror("Error writing file");
        free(file_content);
        close(fd_out);
        return -1;
    }

    close(fd_out);
    free(file_content);

    printf("File %s successfully: %s\n", encrypt_mode ? "encrypted" : "decrypted", output_filename);
    return 0;
}

int process_directory(const char *dirpath, const char *password, int encrypt_mode) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char fullpath[1024];
    uint8_t key[16];
    int error_counter = 0;

    derive_key_from_password(password, key);

    dir = opendir(dirpath);
    if (!dir) {
        perror("Error opening directory");
        return -1;
    }

    printf("%s files in directory: %s\n", encrypt_mode ? "Encrypting" : "Decrypting", dirpath);

    int worker_threads = recommended_thread_count();
    ThreadPool *pool = NULL;
    bool pool_active = false;

    if (worker_threads > 1) {
        pool = threadPoolCreate(worker_threads);
        if (pool) {
            pool_active = true;
        }
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full path
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        // Check if it's a regular file
        if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
            printf("%s: %s\n", encrypt_mode ? "Encrypting" : "Decrypting", entry->d_name);
            bool dispatched_async = false;

            if (pool_active) {
                size_t path_len = strlen(fullpath) + 1;
                char *path_copy = (char*)malloc(path_len);
                FileTaskArg *task = (FileTaskArg*)malloc(sizeof(FileTaskArg));

                if (path_copy && task) {
                    memcpy(path_copy, fullpath, path_len);
                    memcpy(task->key, key, sizeof(task->key));
                    task->filepath = path_copy;
                    task->encrypt_mode = encrypt_mode;
                    task->error_counter = &error_counter;

                    if (threadPoolAddJob(pool, directory_file_task, task) == 0) {
                        dispatched_async = true;
                    } else {
                        free(path_copy);
                        free(task);
                        pool_active = false;
                    }
                } else {
                    free(path_copy);
                    free(task);
                }
            }

            if (!dispatched_async) {
                if (process_file_syscalls(fullpath, key, encrypt_mode) != 0) {
                    __sync_fetch_and_add(&error_counter, 1);
                }
            }
        }
    }

    if (pool) {
        threadPoolWait(pool);
        threadPoolDestroy(pool);
    }

    closedir(dir);
    return (error_counter == 0) ? 0 : -1;
}

int is_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}