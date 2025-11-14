// Necesario para strdup en algunos sistemas
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

/**
 * Implementación de readEntireFile
 * Abre el archivo, lee todo su contenido en memoria y retorna un buffer
 */
unsigned char* readEntireFile(const char *path, size_t *out_size) {
    // Abrir archivo en modo solo lectura
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return NULL;
    }
    
    // Obtener tamaño del archivo con stat()
    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("fstat");
        close(fd);
        return NULL;
    }
    
    size_t size = (size_t)st.st_size;
    
    // Alojar memoria: si size es 0, alojar 1 byte para evitar malloc(0)
    unsigned char *buf = malloc(size ? size : 1);
    if (!buf) {
        perror("malloc");
        close(fd);
        return NULL;
    }
    
    // Leer todo el archivo en el buffer
    ssize_t nread = read(fd, buf, size);
    close(fd);
    
    // Verificar que se leyó correctamente
    if (nread < 0 || (size_t)nread != size) {
        perror("read");
        free(buf);
        return NULL;
    }
    
    *out_size = size;
    return buf;
}

/**
 * Crea todos los directorios padre necesarios para una ruta
 */
int ensureDirectoryExists(const char *path) {
    // Crear copia de la ruta porque dirname() puede modificarla
    size_t path_len = strlen(path);
    char *path_copy = (char *)malloc(path_len + 1);
    if (!path_copy) {
        perror("malloc");
        return -1;
    }
    strcpy(path_copy, path);
    
    // Obtener el directorio padre
    char *dir = dirname(path_copy);
    
    // Crear el directorio recursivamente
    // mkdir -p comportamiento: crear todos los padres necesarios
    char tmp[4096];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    
    // Remover trailing slash si existe
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    // Crear directorios uno por uno
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            // Crear directorio, ignorar si ya existe (errno == EEXIST)
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    
    // Crear el último directorio
    mkdir(tmp, 0755);
    
    free(path_copy);
    return 0;
}

/**
 * Verifica si un path es directorio
 */
int isDirectory(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;  // Error al hacer stat
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

/**
 * Estructura dinámica para almacenar lista de archivos
 */
typedef struct {
    char **paths;      // Array dinámico de strings
    int count;         // Cantidad actual de elementos
    int capacity;      // Capacidad del array
} FileList;

/**
 * Inicializa una FileList vacía
 */
static void fileListInit(FileList *list) {
    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
}

/**
 * Agrega un path a la FileList, expandiendo el array si es necesario
 */
static int fileListAdd(FileList *list, const char *path) {
    // Si llegamos al límite, duplicar capacidad
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
    
    // Duplicar el string y agregarlo
    list->paths[list->count] = strdup(path);
    if (!list->paths[list->count]) {
        perror("strdup");
        return -1;
    }
    
    list->count++;
    return 0;
}

/**
 * Libera la memoria de una FileList (pero no el array final)
 */
static void fileListFree(FileList *list) {
    // No liberamos los strings individuales porque se los pasamos al caller
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
}

/**
 * Función recursiva que explora un directorio y agrega todos los archivos a la lista
 */
static void collectFilesRecursive(const char *dir_path, FileList *list) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Warning: Cannot open directory '%s': ", dir_path);
        perror("");
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar '.' y '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Construir path completo
        char full_path[4096];
        int len = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        if (len >= (int)sizeof(full_path)) {
            fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n", dir_path, entry->d_name);
            continue;
        }
        
        // Verificar tipo de entrada
        struct stat st;
        if (stat(full_path, &st) != 0) {
            fprintf(stderr, "Warning: Cannot stat '%s': ", full_path);
            perror("");
            continue;
        }
        
        if (S_ISREG(st.st_mode)) {
            // Es un archivo regular, agregarlo a la lista
            fileListAdd(list, full_path);
        } else if (S_ISDIR(st.st_mode)) {
            // Es un directorio, explorar recursivamente
            collectFilesRecursive(full_path, list);
        }
        // Ignoramos symlinks, sockets, pipes, etc.
    }
    
    closedir(dir);
}

/**
 * Expande paths (archivos y carpetas) a solo archivos
 */
char** expandPaths(const char **input_paths, int num_inputs, int *out_count) {
    FileList list;
    fileListInit(&list);
    
    // Procesar cada input path
    for (int i = 0; i < num_inputs; i++) {
        const char *path = input_paths[i];
        
        int is_dir = isDirectory(path);
        
        if (is_dir == 1) {
            // Es un directorio: recolectar archivos recursivamente
            printf("Scanning directory: %s\n", path);
            collectFilesRecursive(path, &list);
        } else if (is_dir == 0) {
            // Es un archivo (o algo que no es directorio): agregarlo directamente
            if (fileListAdd(&list, path) != 0) {
                // Error al agregar, liberar todo y retornar NULL
                for (int j = 0; j < list.count; j++) {
                    free(list.paths[j]);
                }
                fileListFree(&list);
                return NULL;
            }
        } else {
            // Error en stat, reportar pero continuar
            fprintf(stderr, "Warning: Cannot access '%s', skipping\n", path);
        }
    }
    
    *out_count = list.count;
    
    // Retornar el array (el caller debe liberar cada string y el array)
    char **result = list.paths;
    
    // No llamamos fileListFree() porque transferimos ownership al caller
    return result;
}
