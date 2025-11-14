#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <stdint.h>
#include <stddef.h>

// magic num
#define ARCHIVE_MAGIC "HUFF"
#define ARCHIVE_VERSION 0x01
#define MAX_PATH_LENGTH 4096

// Representación del archivo
typedef struct {
    char path[MAX_PATH_LENGTH];     // Ruta relativa del archivo
    uint64_t original_size;         // Tamaño del archivo sin comprimir
    uint64_t compressed_size;       // Tamaño después de comprimir
    uint64_t data_offset;           // Posición en el archivo donde empiezan los datos
} FileEntry;

// Descripciónd el contenido
typedef struct {
    char magic[4];                  // "HUFF"
    uint8_t version;                // Versión del formato (0x01)
    uint32_t num_files;             // Cantidad de archivos en el archive
} ArchiveHeader;

/**
 * Crea un archivo comprimido (.huf) con uno o varios archivos
 * 
 * IMPORTANTE: Esta función maneja tanto UN archivo como VARIOS
 * - Si recibe 1 archivo: crea archive con ese archivo
 * - Si recibe N archivos: crea archive con todos ellos
 * 
 * @param archive_path Ruta del archivo .huf a crear (ej: "datos.huf")
 * @param input_paths Array con rutas de archivos a comprimir
 * @param num_inputs Cantidad de archivos en input_paths
 * @return 0 si éxito, -1 si error
 * 
 * Ejemplo:
 *   const char *files[] = {"file1.txt", "file2.txt"};
 *   createArchive("datos.huf", files, 2);
 */
int createArchive(const char *archive_path, const char **input_paths, int num_inputs);

/**
 * Lista el contenido de un archivo .huf mostrando:
 * - Nombre de cada archivo
 * - Tamaño original
 * - Tamaño comprimido
 * - Ratio de compresión
 * 
 * @param archive_path Ruta del archivo .huf
 * @return 0 si éxito, -1 si error
 */
int listArchive(const char *archive_path);

/**
 * Extrae todos los archivos de un archive .huf
 * 
 * @param archive_path Ruta del archivo .huf
 * @param output_dir Directorio donde extraer (ej: "." o "output/")
 * @return 0 si éxito, -1 si error
 */
int extractArchive(const char *archive_path, const char *output_dir);

/**
 * Extrae un archivo específico del archive
 * 
 * @param archive_path Ruta del archivo .huf
 * @param file_path Nombre del archivo a extraer (debe coincidir con el path en el archive)
 * @param output_path Ruta donde guardar el archivo extraído
 * @return 0 si éxito, -1 si error
 */
int extractFile(const char *archive_path, const char *file_path, const char *output_path);

#endif // ARCHIVE_H
