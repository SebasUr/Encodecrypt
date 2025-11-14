#ifndef DECOMPRESS_UTILS_H
#define DECOMPRESS_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include "../src/huffman/huffman.h"

// Estructura para decodificación canónica
typedef struct {
    int symbol;
    uint64_t code;
    size_t length;
} CanonicalEntry;

typedef struct {
    uint64_t base_code;
    int start_index;
    int count;
} CanonicalRow;

// Estructura para escritura eficiente de bytes
typedef struct {
    int fd;
    unsigned char buffer[4096];
    size_t pos;
} ByteWriter;

// Reconstruir códigos canónicos desde las longitudes
void reconstructCanonicalCodes(const unsigned char lens[256], Code codes[256]);

// Preparar filas canónicas para decodificación rápida
int prepareCanonicalRows(const Code codes[256], CanonicalEntry entries[256], CanonicalRow rows[257], size_t *out_max_len);

// Funciones del ByteWriter
void byteWriterInit(ByteWriter *bw, int fd);
int byteWriterFlush(ByteWriter *bw);
int byteWriterPut(ByteWriter *bw, unsigned char byte);

#endif // DECOMPRESS_UTILS_H
