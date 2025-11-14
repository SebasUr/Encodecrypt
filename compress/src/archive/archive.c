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

/**
 * Función auxiliar para comprimir datos a un archivo temporal
 * 
 * @param data Buffer con datos a comprimir
 * @param size Tamaño del buffer
 * @param temp_path Ruta del archivo temporal donde guardar resultado
 * @param compressed_size Puntero donde guardar el tamaño comprimido
 * @return 0 si éxito, -1 si error
 */
static int compressToFile(const unsigned char *data, size_t size, 
                         const char *temp_path, size_t *compressed_size) {
    // Calcular frecuencias
    int f_s[256] = {0};
    for (size_t i = 0; i < size; i++) {
        f_s[data[i]]++;
    }
    
    // Construir el árbol de Huffman
    huffmanNode **activeNodes = malloc(256 * sizeof(huffmanNode *));
    if (!activeNodes) {
        perror("malloc activeNodes");
        return -1;
    }
    
    // Ejecutar el algoritmo de Huffman para generar el árbol y los códigos
    Code codes[256];
    huffmanNode *root = huffmanAlgorithm(f_s, activeNodes, codes);
    
    if (!root) {
        fprintf(stderr, "Error: huffmanAlgorithm returned NULL\n");
        free(activeNodes); return -1;
    }
    
    // comprimir
    // d! función escribe:
    // - Header mágico "HUF1"
    // - Tamaño original
    // - Tabla de longitudes de códigos
    // - Datos comprimidos
    int result = compressFile(data, size, temp_path, codes);
    
    freeHuffmanTree(root);
    free(activeNodes);
    
    if (result != 0) {
        fprintf(stderr, "Error in compressFile\n");
        return -1;
    }
    
    // obtener tamaño del archivo comprimido
    struct stat st;
    if (stat(temp_path, &st) != 0) {
        perror("stat");
        return -1;
    }
    
    *compressed_size = (size_t)st.st_size;
    return 0;
}

/**
 * Estructura para un job de compresión en el thread pool
 * Contiene toda la información necesaria para comprimir un archivo
 */
typedef struct {
    int index;                      // Índice del archivo en el array
    const char *input_path;         // Path del archivo a comprimir
    char temp_path[256];            // Path del archivo temporal de salida
    FileEntry entry;                // Entrada en la tabla (NO puntero, copia local)
    int status;                     // 0 = éxito, -1 = error
    pthread_mutex_t *print_mutex;   // Mutex para printf thread-safe
} CompressionJob;

/**
 * Función que ejecutará cada worker thread del pool
 * Esta es la función que se pasa a threadPoolAddJob()
 */
static void compressFileWorker(void *arg) {
    CompressionJob *job = (CompressionJob*)arg;
    
    // Leer archivo completo en memoria
    size_t original_size;
    unsigned char *data = readEntireFile(job->input_path, &original_size);
    if (!data) {
        pthread_mutex_lock(job->print_mutex);
        fprintf(stderr, "  Error reading: %s (skipping)\n", job->input_path);
        pthread_mutex_unlock(job->print_mutex);
        job->status = -1;
        return;
    }
    
    // Comprimir a archivo temporal
    size_t compressed_size;
    if (compressToFile(data, original_size, job->temp_path, &compressed_size) != 0) {
        pthread_mutex_lock(job->print_mutex);
        fprintf(stderr, "  Error compressing: %s (skipping)\n", job->input_path);
        pthread_mutex_unlock(job->print_mutex);
        free(data);
        job->status = -1;
        return;
    }
    
    // Calcular ratio y mostrar progreso (protegido por mutex)
    double ratio = 100.0 * compressed_size / original_size;
    pthread_mutex_lock(job->print_mutex);
    printf("  [%d] %s: %lu bytes -> %lu bytes (%.2f%%)\n", 
            job->index + 1, job->input_path, original_size, compressed_size, ratio);
    pthread_mutex_unlock(job->print_mutex);
    
    // Guardar metadata en la entrada LOCAL (cada job tiene su propia copia)
    strncpy(job->entry.path, job->input_path, MAX_PATH_LENGTH - 1);
    job->entry.path[MAX_PATH_LENGTH - 1] = '\0';
    job->entry.original_size = original_size;
    job->entry.compressed_size = compressed_size;
    // data_offset se llenará en la fase de merge
    
    free(data);
    job->status = 0;
}

/**
 * esta función crea un archive huf para uno o varios.
 * el formato del archivo es:
 * 
 * HEADER
*  - Magic: "HUFF" (4 bytes)
*  - Version: 0x01 (1 byte)
*  - Número de archivos: N (4 bytes)
 * 
 * TABLA DE LOS ARCHIVOS (FileEntry*N)
 *   porcada archivo:
 *  - Path (4096 bytes)
 *  - Tamaño original (8 bytes)
 *  - Tamaño comprimido (8 bytes)
 *  - Offset de datos (8 bytes)
 * 
 * Datos
 *   Cada archivo comprimido en formato HUF1 (de compress.c)
 */
int createArchive(const char *archive_path, const char **input_paths, int num_inputs) {
    printf("Creating archive: %s\n", archive_path);
    printf("Files to compress: %d\n\n", num_inputs);

    int archive_fd = open(archive_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (archive_fd < 0) { perror("open archive"); return -1;}
    

    // escribir header
    ArchiveHeader header;
    memcpy(header.magic, ARCHIVE_MAGIC, 4);
    header.version = ARCHIVE_VERSION;
    header.num_files = (uint32_t)num_inputs;
    
    // Escribir header usando write() de POSIX
    if (write(archive_fd, header.magic, 4) != 4) {
        perror("write magic");
        close(archive_fd); return -1; }
    if (write(archive_fd, &header.version, 1) != 1) {
        perror("write version");
        close(archive_fd); return -1; }
    if (write(archive_fd, &header.num_files, sizeof(uint32_t)) != sizeof(uint32_t)) {
        perror("write num_files");
        close(archive_fd); return -1; }
    
    // reserva de espacio para la tabla de archivos
    // se escribe al final después de saber los offset
    off_t table_pos = lseek(archive_fd, 0, SEEK_CUR);
    FileEntry *entries = calloc(num_inputs, sizeof(FileEntry));
    if (!entries) {
        perror("calloc entries");
        close(archive_fd);
        return -1;
    }
    
    // Escribir tabla vacía
    size_t table_size = sizeof(FileEntry) * num_inputs;
    if (write(archive_fd, entries, table_size) != (ssize_t)table_size) {
        perror("write table placeholder");
        free(entries); close(archive_fd); return -1;
    }
    
    // Compresión paralela con threadpool
    
    long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);  // Obtener # de CPUs
    if (num_cpus < 1) num_cpus = 4;  // Fallback si falla sysconf
    
    int num_threads = (num_inputs < num_cpus) ? num_inputs : num_cpus;
    
    printf("Using %d worker threads for compression\n\n", num_threads);
    
    // Crear thread pool
    ThreadPool *pool = threadPoolCreate(num_threads);
    if (!pool) {
        fprintf(stderr, "Error creating thread pool\n");
        free(entries);
        close(archive_fd);
        return -1;
    }
    
    // Mutex para printf thread-safe 
    pthread_mutex_t print_mutex;
    pthread_mutex_init(&print_mutex, NULL);
    
    // C** HEAP ALLOCATION porque sino da segfault con archivos grandes
    CompressionJob **jobs = malloc(sizeof(CompressionJob*) * num_inputs);
    if (!jobs) {
        perror("malloc jobs array");
        threadPoolDestroy(pool);
        pthread_mutex_destroy(&print_mutex);
        free(entries);
        close(archive_fd);
        return -1;
    }
    
    // Preparar y encolar todos los jobs
    // IMPORTANTE: Cada job se aloca individualmente en el heap porque el worker
    // puede ejecutarse después de que el loop continúe
    for (int i = 0; i < num_inputs; i++) {
        CompressionJob *job = malloc(sizeof(CompressionJob));
        if (!job) {
            perror("malloc job");
            // Limpiar jobs creados hasta ahora
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
        // Usar índice + timestamp ppara evitar colisiones
        snprintf(job->temp_path, sizeof(job->temp_path), "/tmp/huf_%ld_%d.tmp", 
                 (long)getpid(), i);
        // Inicializar entry local (cada job tiene su copia)
        memset(&job->entry, 0, sizeof(FileEntry));
        job->status = -1;  // Inicializar como error
        job->print_mutex = &print_mutex;
        
        // Añadir job al pool (se ejecutará cuando un worker esté disponible)
        if (threadPoolAddJob(pool, compressFileWorker, job) != 0) {
            fprintf(stderr, "Error adding job to pool\n");
        }
    }
    
    // Esperar a que TODOS los jobs terminen
    threadPoolWait(pool);
    
    printf("\nCompression phase completed. Merging files...\n\n");
    
    // Destruir thread pool (ya no se necesita)
    threadPoolDestroy(pool);
    pthread_mutex_destroy(&print_mutex);
    
    // Merge secuencia, ir copiando todo al archive
    int successful_files = 0;
    
    for (int i = 0; i < num_inputs; i++) {
        CompressionJob *job = jobs[i];
        
        // Saltar archivos que fallaron en la compresión
        if (job->status != 0) {
            free(job);  // Liberar el job aunque haya fallado
            continue;
        }
        
        // Copiar metadata del job local al array entries
        entries[i] = job->entry;
        
        // Guardar el offset donde comenzarán los datos de este archivo
        entries[i].data_offset = (uint64_t)lseek(archive_fd, 0, SEEK_CUR);
        
        // Abrir archivo temporal
        int temp_fd = open(job->temp_path, O_RDONLY);
        if (temp_fd < 0) {
            perror("open temp file");
            free(job);
            continue;
        }
        
        // Copiar contenido del temp al archive
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
        unlink(job->temp_path);  // Eliminar archivo temporal
        successful_files++;
        
        free(job);  // Liberar memoria del job
    }
    
    free(jobs);  // Liberar array de punteros
    
    printf("Successfully compressed %d/%d files\n\n", successful_files, num_inputs);
    
    // Actualizar tablas y offsets 
    // se vuelve al inicio para actualizar la tabla.
    off_t end_pos = lseek(archive_fd, 0, SEEK_CUR);  // Guardar posición actual
    lseek(archive_fd, table_pos, SEEK_SET);           // Volver a la tabla
    
    if (write(archive_fd, entries, table_size) != (ssize_t)table_size) {
        perror("write final table");
        free(entries);
        close(archive_fd);
        return -1;
    }
    
    lseek(archive_fd, end_pos, SEEK_SET);  // Volver al final
    
    free(entries);
    close(archive_fd);
    
    printf("\nArchive created successfully: %s\n", archive_path);
    return 0;
}

/**
 * Muestra el contenido de un archive .huf de forma legible
 */
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
        fprintf(stderr, "Error: No es un archivo .huf válido\n");
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

/**
 * Extrae todos los archivos de un archive .huf
 */
int extractArchive(const char *archive_path, const char *output_dir) {
    int archive_fd = open(archive_path, O_RDONLY);
    if (archive_fd < 0) {
        perror("open");
        return -1;
    }
    
    // Leer header
    ArchiveHeader header;
    if (read(archive_fd, header.magic, 4) != 4) {
        perror("read magic");
        close(archive_fd); return -1;
    }
    
    if (memcmp(header.magic, ARCHIVE_MAGIC, 4) != 0) {
        fprintf(stderr, "Error: No es un archivo .huf válido\n");
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
        
        // crea archivo temporal con datos comprimidos
        char temp_compressed[256];
        snprintf(temp_compressed, sizeof(temp_compressed), "/tmp/huf_extract_%u.huf", i);
        
        // Vamos ala posición de los datos
        if (lseek(archive_fd, entries[i].data_offset, SEEK_SET) < 0) {
            perror("lseek");
            continue;
        }
        
        // Copiar datos comprimidos a archivo temporal.
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
        
        // Construir ruta de salida
        char output_path[MAX_PATH_LENGTH];
        snprintf(output_path, sizeof(output_path), "%s/%s", output_dir, entries[i].path);
        
        // Crear directorios si es necesario
        ensureDirectoryExists(output_path);
        
        // Descomprimir usando la función existente decompressFile()
        if (decompressFile(temp_compressed, output_path) != 0) {
            fprintf(stderr, "  Error decompressing: %s\n", entries[i].path);
        } else {
            printf("  Extracted: %s (%lu bytes)\n", output_path, entries[i].original_size);
        }
        
        unlink(temp_compressed);  // Eliminar temporal
    }
    
    free(entries);
    close(archive_fd);
    
    printf("\nExtraction complete\n");
    return 0;
}

/**
 * Extrae un solo archivo específico de un archive .huf
 */
int extractFile(const char *archive_path, const char *file_to_extract, const char *output_dir) {
    // Abrir archivo usando POSIX
    int archive_fd = open(archive_path, O_RDONLY);
    if (archive_fd < 0) {
        perror("open");
        return -1;
    }
    
    // Leer header
    ArchiveHeader header;
    if (read(archive_fd, header.magic, 4) != 4) {
        perror("read magic");
        close(archive_fd);
        return -1;
    }
    
    if (memcmp(header.magic, ARCHIVE_MAGIC, 4) != 0) {
        fprintf(stderr, "Error: No es un archivo .huf válido\n");
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
    
    // Buscar archivo en la tabla usando POSIX
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
    
    // Extraer el archivo
    printf("Extracting: %s\n", entry.path);
    
    // Crear archivo temporal con datos comprimidos
    char temp_compressed[] = "/tmp/huf_extract_single.huf";
    
    // Ir a la posición de los datos comprimidos usando lseek
    if (lseek(archive_fd, entry.data_offset, SEEK_SET) < 0) {
        perror("lseek");
        close(archive_fd);
        return -1;
    }
    
    // Copiar datos comprimidos a archivo temporal usando POSIX
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
    
    // Descomprimir temporal -> archivo final
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
    
    unlink(temp_compressed);  // Eliminar temporal
    
    printf("Extraction complete\n");
    return 0;
}
