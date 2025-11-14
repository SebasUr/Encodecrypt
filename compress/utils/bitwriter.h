#ifndef BITWRITER_H
#define BITWRITER_H

#include <stdint.h>
#include <stddef.h>

typedef struct BitWriter {
    int fd;                  // descriptor
    unsigned char buffer;
    int bits_in_buffer;      // numero actual de bits en el buffer (0-7)
    unsigned char chunk[4096]; // Bloque para escrituras agrupadas
    size_t chunk_pos;        // Posición actual en el bloque
} BitWriter;

void bitWriterInit(BitWriter *bw, int fd);
void bitWriterWrite(BitWriter *bw, uint64_t bits, int count);
int bitWriterFlush(BitWriter *bw);

#endif // BITWRITER_H
