#ifndef BITREADER_H
#define BITREADER_H

#include <stdint.h>
#include <stddef.h>

typedef struct BitReader {
    int fd;                  // File descriptor
    unsigned char buffer;    // Buffer de 1 byte
    int pos;                 // Posición actual en el buffer (0-7)
    int bits_available;      // Bits disponibles en el buffer
    int eof;                 // Flag de fin de archivo
    unsigned char *mapped;   // Región mapeada (si se usa mmap)
    size_t mapped_size;      // Tamaño total del mapeo
    size_t mapped_pos;       // Posición actual dentro del mapeo
    int use_mmap;            // Indicador de uso de mmap
} BitReader;

// Inicializa el BitReader
void bitReaderInit(BitReader *br, int fd);

// Lee un bit (devuelve 0 o 1, o -1 en error/EOF)
int bitReaderReadBit(BitReader *br);

// Libera recursos asociados al BitReader
void bitReaderClose(BitReader *br);

#endif // BITREADER_H
