#define _POSIX_C_SOURCE 200809L

#include "file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>

unsigned char* readEntireFile(const char *path, size_t *out_size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return NULL;
    }
    
    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("fstat");
        close(fd);
        return NULL;
    }
    
    size_t size = (size_t)st.st_size;
    
    unsigned char *buf = malloc(size ? size : 1);
    if (!buf) {
        perror("malloc");
        close(fd);
        return NULL;
    }
    
    ssize_t nread = read(fd, buf, size);
    close(fd);
    
    if (nread < 0 || (size_t)nread != size) {
        perror("read");
        free(buf);
        return NULL;
    }
    
    *out_size = size;
    return buf;
}

int ensureDirectoryExists(const char *path) {
    size_t path_len = strlen(path);
    char *path_copy = (char *)malloc(path_len + 1);
    if (!path_copy) {
        perror("malloc");
        return -1;
    }
    strcpy(path_copy, path);
    
    char *dir = dirname(path_copy);
    
    char tmp[4096];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    
    mkdir(tmp, 0755);
    
    free(path_copy);
    return 0;
}

int isDirectory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}


typedef struct {
    char **paths;
    int count;
    int capacity;
} FileList;

static void fileListInit(FileList *list) {
    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int fileListAdd(FileList *list, const char *path) {
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity ? list->capacity * 2 : 32;
        char **new_paths = realloc(list->paths, new_capacity * sizeof(char*));
        if (!new_paths) {
            perror("realloc");
            return -1;
        }
        list->paths = new_paths;
        list->capacity = new_capacity;
    }
    
    list->paths[list->count] = strdup(path);
    if (!list->paths[list->count]) {
        perror("strdup");
        return -1;
    }
    
    list->count++;
    return 0;
}

static void fileListFree(FileList *list) {
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void collectFilesRecursive(const char *dir_path, FileList *list) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Warning: Cannot open directory '%s': ", dir_path);
        perror("");
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char full_path[4096];
        int len = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        if (len >= (int)sizeof(full_path)) {
            fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n", dir_path, entry->d_name);
            continue;
        }
        
        struct stat st;
        if (stat(full_path, &st) != 0) {
            fprintf(stderr, "Warning: Cannot stat '%s': ", full_path);
            perror("");
            continue;
        }
        
        if (S_ISREG(st.st_mode)) {
            fileListAdd(list, full_path);
        } else if (S_ISDIR(st.st_mode)) {
            collectFilesRecursive(full_path, list);
        }
    }
    
    closedir(dir);
}

char** expandPaths(const char **input_paths, int num_inputs, int *out_count) {
    FileList list;
    fileListInit(&list);
    
    for (int i = 0; i < num_inputs; i++) {
        const char *path = input_paths[i];
        
        int is_dir = isDirectory(path);
        
        if (is_dir == 1) {
            printf("Scanning directory: %s\n", path);
            collectFilesRecursive(path, &list);
        } else if (is_dir == 0) {
            if (fileListAdd(&list, path) != 0) {
                for (int j = 0; j < list.count; j++) {
                    free(list.paths[j]);
                }
                fileListFree(&list);
                return NULL;
            }
        } else {
            fprintf(stderr, "Warning: Cannot access '%s', skipping\n", path);
        }
    }
    
    *out_count = list.count;
    
    char **result = list.paths;
    
    return result;
}
