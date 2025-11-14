#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stddef.h>

/**
 * Lee un archivo completo en memoria
 * 
 * MEMORIA: El caller debe liberar el buffer retornado con free()
 * 
 * @param path Ruta del archivo a leer
 * @param out_size Puntero donde se guardará el tamaño leído
 * @return Buffer con los datos del archivo, o NULL si error
 * 
 * Ejemplo:
 *   size_t size;
 *   unsigned char *data = readEntireFile("file.txt", &size);
 *   if (data) {
 *       // usar data...
 *       free(data);  // IMPORTANTE: liberar memoria
 *   }
 */
unsigned char* readEntireFile(const char *path, size_t *out_size);

/**
 * Crea directorios necesarios para una ruta de archivo
 * Similar a "mkdir -p" en Linux
 * 
 * @param path Ruta del archivo (ej: "dir1/dir2/file.txt")
 * @return 0 si éxito, -1 si error
 * 
 * Ejemplo:
 *   // Crea dir1/ y dir1/dir2/ si no existen
 *   ensureDirectoryExists("dir1/dir2/file.txt");
 */
int ensureDirectoryExists(const char *path);

/**
 * Verifica si una ruta es un directorio
 * 
 * @param path Ruta a verificar
 * @return 1 si es directorio, 0 si es archivo u otro tipo, -1 si error
 */
int isDirectory(const char *path);

/**
 * Expande una lista de paths (archivos y/o carpetas) a solo archivos
 * Si un path es carpeta, recolecta recursivamente todos los archivos dentro
 * Si un path es archivo, lo mantiene tal cual
 * 
 * MEMORIA: El caller debe liberar:
 *  1. Cada string del array retornado con free()
 *  2. El array mismo con free()
 * 
 * @param input_paths Array de paths (pueden ser archivos o carpetas)
 * @param num_inputs Cantidad de paths en input_paths
 * @param out_count Puntero donde se guardará la cantidad total de archivos
 * @return Array de strings con paths de archivos únicamente, o NULL si error
 * 
 * Ejemplo:
 *   const char *inputs[] = {"file.txt", "mi_carpeta/", "otro.txt"};
 *   int total;
 *   char **files = expandPaths(inputs, 3, &total);
 *   // files contendrá: ["file.txt", "mi_carpeta/a.txt", "mi_carpeta/b.txt", "otro.txt"]
 *   // Liberar después:
 *   for (int i = 0; i < total; i++) free(files[i]);
 *   free(files);
 */
char** expandPaths(const char **input_paths, int num_inputs, int *out_count);

#endif // FILE_UTILS_H
