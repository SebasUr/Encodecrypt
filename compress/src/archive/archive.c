#include "archive.h"
#include "../core/compress.h"
#include "../core/decompress.h"
#include "../huffman/huffman.h"
#include "../../utils/file_utils.h"
#include "../../utils/threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <libgen.h> 

static int compressToFile(const unsigned char *data, size_t size, const char *temp_path, size_t *compressed_size) {
    int f_s[256] = {0};
    for (size_t i = 0; i < size; i++) {
        f_s[data[i]]++;
    }
    
    huffmanNode **activeNodes = malloc(256 * sizeof(huffmanNode *));
    if (!activeNodes) {
        perror("malloc activeNodes");
        return -1;
    }
    
    Code codes[256];
    huffmanNode *root = huffmanAlgorithm(f_s, activeNodes, codes);
    
    if (!root) {
        fprintf(stderr, "Error: huffmanAlgorithm returned NULL\n");
        free(activeNodes); return -1;
    }
    
    int result = compressFile(data, size, temp_path, codes);
    
    freeHuffmanTree(root);
    free(activeNodes);
    
    if (result != 0) {
        fprintf(stderr, "Error in compressFile\n");
        return -1;
    }
    
    struct stat st;
    if (stat(temp_path, &st) != 0) {
        perror("stat");
        return -1;
    }
    
    *compressed_size = (size_t)st.st_size;
    return 0;
}

typedef struct {
    int index;
    const char *input_path;
    char temp_path[256];
    FileEntry entry;
    int status;
    pthread_mutex_t *print_mutex;
} CompressionJob;


static void compressFileWorker(void *arg) {
    CompressionJob *job = (CompressionJob*)arg;
    
    size_t original_size;
    unsigned char *data = readEntireFile(job->input_path, &original_size);
    if (!data) {
        pthread_mutex_lock(job->print_mutex);
        fprintf(stderr, "  Error reading: %s (skipping)\n", job->input_path);
        pthread_mutex_unlock(job->print_mutex);
        job->status = -1;
        return;
    }
    
    size_t compressed_size;
    if (compressToFile(data, original_size, job->temp_path, &compressed_size) != 0) {
        pthread_mutex_lock(job->print_mutex);
        fprintf(stderr, "  Error compressing: %s (skipping)\n", job->input_path);
        pthread_mutex_unlock(job->print_mutex);
        free(data);
        job->status = -1;
        return;
    }
    
    double ratio = 100.0 * compressed_size / original_size;
    pthread_mutex_lock(job->print_mutex);
    printf("  [%d] %s: %lu bytes -> %lu bytes (%.2f%%)\n", 
            job->index + 1, job->input_path, original_size, compressed_size, ratio);
    pthread_mutex_unlock(job->print_mutex);
    
    strncpy(job->entry.path, job->input_path, MAX_PATH_LENGTH - 1);
    job->entry.path[MAX_PATH_LENGTH - 1] = '\0';
    job->entry.original_size = original_size;
    job->entry.compressed_size = compressed_size;
    
    free(data);
    job->status = 0;
}


int createArchive(const char *archive_path, const char **input_paths, int num_inputs) {
    printf("Creating archive: %s\n", archive_path);
    printf("Files to compress: %d\n\n", num_inputs);

    int archive_fd = open(archive_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (archive_fd < 0) { perror("open archive"); return -1;}
    
    ArchiveHeader header;
    memcpy(header.magic, ARCHIVE_MAGIC, 4);
    header.version = ARCHIVE_VERSION;
    header.num_files = (uint32_t)num_inputs;
    
    if (write(archive_fd, header.magic, 4) != 4) {
        perror("write magic");
        close(archive_fd); return -1; }
    if (write(archive_fd, &header.version, 1) != 1) {
        perror("write version");
        close(archive_fd); return -1; }
    if (write(archive_fd, &header.num_files, sizeof(uint32_t)) != sizeof(uint32_t)) {
        perror("write num_files");
        close(archive_fd); return -1; }
    
    off_t table_pos = lseek(archive_fd, 0, SEEK_CUR);
    FileEntry *entries = calloc(num_inputs, sizeof(FileEntry));
    if (!entries) {
        perror("calloc entries");
        close(archive_fd);
        return -1;
    }
    
    size_t table_size = sizeof(FileEntry) * num_inputs;
    if (write(archive_fd, entries, table_size) != (ssize_t)table_size) {
        perror("write table placeholder");
        free(entries); close(archive_fd); return -1;
    }
    
    long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus < 1) num_cpus = 4;
    
    int num_threads = (num_inputs < num_cpus) ? num_inputs : num_cpus;
    
    printf("Using %d worker threads for compression\n\n", num_threads);
    
    ThreadPool *pool = threadPoolCreate(num_threads);
    if (!pool) {
        fprintf(stderr, "Error creating thread pool\n");
        free(entries);
        close(archive_fd);
        return -1;
    }
    
    pthread_mutex_t print_mutex;
    pthread_mutex_init(&print_mutex, NULL);
    
    CompressionJob **jobs = malloc(sizeof(CompressionJob*) * num_inputs);
    if (!jobs) {
        perror("malloc jobs array");
        threadPoolDestroy(pool);
        pthread_mutex_destroy(&print_mutex);
        free(entries);
        close(archive_fd);
        return -1;
    }
    
    for (int i = 0; i < num_inputs; i++) {
        CompressionJob *job = malloc(sizeof(CompressionJob));
        if (!job) {
            perror("malloc job");
            for (int j = 0; j < i; j++) {
                free(jobs[j]);
            }
            free(jobs);
            threadPoolDestroy(pool);
            pthread_mutex_destroy(&print_mutex);
            free(entries);
            close(archive_fd);
            return -1;
        }
        
        jobs[i] = job;
        job->index = i;
        job->input_path = input_paths[i];
        snprintf(job->temp_path, sizeof(job->temp_path), "/tmp/huf_%ld_%d.tmp", 
                 (long)getpid(), i);
        memset(&job->entry, 0, sizeof(FileEntry));
        job->status = -1;
        job->print_mutex = &print_mutex;
        
        if (threadPoolAddJob(pool, compressFileWorker, job) != 0) {
            fprintf(stderr, "Error adding job to pool\n");
        }
    }
    
    threadPoolWait(pool);
    
    printf("\nCompression phase completed. Merging files...\n\n");
    
    threadPoolDestroy(pool);
    pthread_mutex_destroy(&print_mutex);
    
    int successful_files = 0;
    
    for (int i = 0; i < num_inputs; i++) {
        CompressionJob *job = jobs[i];
        
        if (job->status != 0) {
            free(job);
            continue;
        }
        
        entries[i] = job->entry;
        entries[i].data_offset = (uint64_t)lseek(archive_fd, 0, SEEK_CUR);
        
        int temp_fd = open(job->temp_path, O_RDONLY);
        if (temp_fd < 0) {
            perror("open temp file");
            free(job);
            continue;
        }
        
        unsigned char buffer[8192];
        ssize_t n;
        while ((n = read(temp_fd, buffer, sizeof(buffer))) > 0) {
            if (write(archive_fd, buffer, n) != n) {
                perror("write compressed data");
                close(temp_fd);
                free(job);
                break;
            }
        }
        
        close(temp_fd);
        unlink(job->temp_path);
        successful_files++;
        
        free(job);
    }
    
    free(jobs);
    
    printf("Successfully compressed %d/%d files\n\n", successful_files, num_inputs);
    
    off_t end_pos = lseek(archive_fd, 0, SEEK_CUR);
    lseek(archive_fd, table_pos, SEEK_SET);
    
    if (write(archive_fd, entries, table_size) != (ssize_t)table_size) {
        perror("write final table");
        free(entries);
        close(archive_fd);
        return -1;
    }
    
    lseek(archive_fd, end_pos, SEEK_SET);
    
    free(entries);
    close(archive_fd);
    
    printf("\nArchive created successfully: %s\n", archive_path);
    return 0;
}

int listArchive(const char *archive_path) {
    int archive_fd = open(archive_path, O_RDONLY);
    if (archive_fd < 0) {
        perror("open");
        return -1;
    }
    
    ArchiveHeader header;
    if (read(archive_fd, header.magic, 4) != 4) {
        perror("read magic");
        close(archive_fd);
        return -1;
    }
    
    if (memcmp(header.magic, ARCHIVE_MAGIC, 4) != 0) {
        fprintf(stderr, "Error: Invalid .huf file\n");
        close(archive_fd);
        return -1;
    }
    
    if (read(archive_fd, &header.version, 1) != 1) {
        perror("read version");
        close(archive_fd);
        return -1;
    }
    
    if (read(archive_fd, &header.num_files, sizeof(uint32_t)) != sizeof(uint32_t)) {
        perror("read num_files");
        close(archive_fd);
        return -1;
    }
    
    printf("\nArchive: %s\n", archive_path);
    printf("Version: %d\n", header.version);
    printf("Files: %u\n\n", header.num_files);
    
    printf("%-50s %15s %15s %10s\n", "File", "Original", "Compressed", "Ratio");
    printf("--------------------------------------------------------------------------------\n");
    
    FileEntry entry;
    uint64_t total_original = 0;
    uint64_t total_compressed = 0;
    
    for (uint32_t i = 0; i < header.num_files; i++) {
        if (read(archive_fd, &entry, sizeof(FileEntry)) != sizeof(FileEntry)) {
            perror("read entry");
            close(archive_fd);
            return -1;
        }
        
        double ratio = entry.original_size > 0 
            ? 100.0 * entry.compressed_size / entry.original_size 
            : 0.0;
        
        printf("%-50s %12lu B %12lu B %9.2f%%\n", entry.path, entry.original_size, entry.compressed_size, ratio);
        
        total_original += entry.original_size;
        total_compressed += entry.compressed_size;
    }
    
    printf("--------------------------------------------------------------------------------\n");
    
    double total_ratio = total_original > 0 
        ? 100.0 * total_compressed / total_original 
        : 0.0;
    
    printf("%-50s %12lu B %12lu B %9.2f%%\n", "TOTAL", total_original, total_compressed, total_ratio);
    printf("\n");
    
    close(archive_fd);
    return 0;
}

int extractArchive(const char *archive_path, const char *output_dir) {
    int archive_fd = open(archive_path, O_RDONLY);
    if (archive_fd < 0) {
        perror("open");
        return -1;
    }
    
    ArchiveHeader header;
    if (read(archive_fd, header.magic, 4) != 4) {
        perror("read magic");
        close(archive_fd); return -1;
    }
    
    if (memcmp(header.magic, ARCHIVE_MAGIC, 4) != 0) {
        fprintf(stderr, "Error: Invalid .huf file\n");
        close(archive_fd); return -1;
    }
    
    if (read(archive_fd, &header.version, 1) != 1) {
        perror("read version");
        close(archive_fd); return -1;
    }
    
    if (read(archive_fd, &header.num_files, sizeof(uint32_t)) != sizeof(uint32_t)) {
        perror("read num_files");
        close(archive_fd); return -1;
    }
    
    printf("\nExtracting archive: %s\n", archive_path);
    printf("Files to extract: %u\n\n", header.num_files);
    
    FileEntry *entries = malloc(header.num_files * sizeof(FileEntry));
    if (!entries) {
        perror("malloc");
        close(archive_fd);
        return -1;
    }
    
    size_t table_size = header.num_files * sizeof(FileEntry);
    if (read(archive_fd, entries, table_size) != (ssize_t)table_size) {
        perror("read table");
        free(entries);
        close(archive_fd);
        return -1;
    }
    
    // extrae cada archivo
    for (uint32_t i = 0; i < header.num_files; i++) {
        printf("Extracting [%u/%u]: %s\n", i + 1, header.num_files, entries[i].path);
        
        char temp_compressed[256];
        snprintf(temp_compressed, sizeof(temp_compressed), "/tmp/huf_extract_%u.huf", i);
        
        if (lseek(archive_fd, entries[i].data_offset, SEEK_SET) < 0) {
            perror("lseek");
            continue;
        }
        
        int temp_fd = open(temp_compressed, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (temp_fd < 0) {
            perror("open temp");
            continue;
        }
        
        unsigned char buffer[8192];
        size_t remaining = entries[i].compressed_size;
        while (remaining > 0) {
            size_t to_read = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            ssize_t n = read(archive_fd, buffer, to_read);
            if (n <= 0) {
                perror("read compressed data");
                break;
            }
            if (write(temp_fd, buffer, n) != n) {
                perror("write temp");
                break;
            }
            remaining -= n;
        }
        close(temp_fd);
        
        char output_path[MAX_PATH_LENGTH];
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, entries[i].path);
        
        ensureDirectoryExists(output_path);
        
        if (decompressFile(temp_compressed, output_path) != 0) {
            fprintf(stderr, "  Error decompressing: %s\n", entries[i].path);
        } else {
            printf("  Extracted: %s (%lu bytes)\n", output_path, entries[i].original_size);
        }
        
        unlink(temp_compressed);
    }
    
    free(entries);
    close(archive_fd);
    
    printf("\nExtraction complete\n");
    return 0;
}

int extractFile(const char *archive_path, const char *file_to_extract, const char *output_dir) {
    int archive_fd = open(archive_path, O_RDONLY);
    if (archive_fd < 0) {
        perror("open");
        return -1;
    }
    
    ArchiveHeader header;
    if (read(archive_fd, header.magic, 4) != 4) {
        perror("read magic");
        close(archive_fd);
        return -1;
    }
    
    if (memcmp(header.magic, ARCHIVE_MAGIC, 4) != 0) {
        fprintf(stderr, "Error: Invalid .huf file\n");
        close(archive_fd);
        return -1;
    }
    
    if (read(archive_fd, &header.version, 1) != 1) {
        perror("read version");
        close(archive_fd);
        return -1;
    }
    
    if (read(archive_fd, &header.num_files, sizeof(uint32_t)) != sizeof(uint32_t)) {
        perror("read num_files");
        close(archive_fd);
        return -1;
    }
    
    FileEntry entry;
    int found = 0;
    
    for (uint32_t i = 0; i < header.num_files; i++) {
        if (read(archive_fd, &entry, sizeof(FileEntry)) != sizeof(FileEntry)) {
            perror("read entry");
            close(archive_fd);
            return -1;
        }
        
        if (strcmp(entry.path, file_to_extract) == 0) {
            found = 1;
            break;
        }
    }
    
    if (!found) {
        fprintf(stderr, "Error: File '%s' not found in archive\n", file_to_extract);
        close(archive_fd);
        return -1;
    }
    
    printf("Extracting: %s\n", entry.path);
    
    char temp_compressed[] = "/tmp/huf_extract_single.huf";
    
    if (lseek(archive_fd, entry.data_offset, SEEK_SET) < 0) {
        perror("lseek");
        close(archive_fd);
        return -1;
    }
    
    int temp_fd = open(temp_compressed, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (temp_fd < 0) {
        perror("open temp");
        close(archive_fd);
        return -1;
    }
    
    unsigned char buffer[8192];
    size_t remaining = entry.compressed_size;
    while (remaining > 0) {
        size_t to_read = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        ssize_t n = read(archive_fd, buffer, to_read);
        if (n <= 0) {
            perror("read compressed data");
            close(temp_fd);
            close(archive_fd);
            return -1;
        }
        if (write(temp_fd, buffer, n) != n) {
            perror("write temp");
            close(temp_fd);
            close(archive_fd);
            return -1;
        }
        remaining -= n;
    }
    close(temp_fd);
    close(archive_fd);
    
    char output_path[4096];
    if (output_dir) {
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, basename((char*)entry.path));
        ensureDirectoryExists(output_dir);
    } else {
        snprintf(output_path, sizeof(output_path), "%s", basename((char*)entry.path));
    }
    
    if (decompressFile(temp_compressed, output_path) != 0) {
        fprintf(stderr, "Error: Decompression failed for %s\n", entry.path);
        unlink(temp_compressed);
        return -1;
    }
    
    unlink(temp_compressed);
    
    printf("Extraction complete\n");
    return 0;
}
