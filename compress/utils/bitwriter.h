#ifndef BITWRITER_H
#define BITWRITER_H

#include <stdint.h>
#include <stddef.h>

typedef struct BitWriter {
    int fd;
    unsigned char buffer;
    int bits_in_buffer;
    unsigned char chunk[4096];
    size_t chunk_pos;
} BitWriter;

void bitWriterInit(BitWriter *bw, int fd);
void bitWriterWrite(BitWriter *bw, uint64_t bits, int count);
int bitWriterFlush(BitWriter *bw);

#endif // BITWRITER_H