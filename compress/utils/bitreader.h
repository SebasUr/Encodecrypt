#ifndef BITREADER_H
#define BITREADER_H

#include <stdint.h>
#include <stddef.h>

typedef struct BitReader {
    int fd;
    unsigned char buffer;
    int pos;
    int bits_available;
    int eof;
    unsigned char *mapped;
    size_t mapped_size;
    size_t mapped_pos;
    int use_mmap;
} BitReader;

void bitReaderInit(BitReader *br, int fd);
int bitReaderReadBit(BitReader *br);
void bitReaderClose(BitReader *br);

#endif // BITREADER_H
