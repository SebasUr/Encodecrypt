#include "bitwriter.h"
#include <unistd.h>
#include <string.h>

void bitWriterInit(BitWriter *bw, int fd) {
    bw->fd = fd;
    bw->buffer = 0;
    bw->bits_in_buffer = 0;
    bw->chunk_pos = 0;
}

void bitWriterWrite(BitWriter *bw, uint64_t bits, int count) {
    for (int i = count - 1; i >= 0; i--) {
        unsigned char bit = (bits >> i) & 1;
        
        bw->buffer = (bw->buffer << 1) | bit;
        bw->bits_in_buffer++;
        
        if (bw->bits_in_buffer == 8) {
            bw->chunk[bw->chunk_pos++] = bw->buffer;
            if (bw->chunk_pos == sizeof(bw->chunk)) {
                write(bw->fd, bw->chunk, sizeof(bw->chunk));
                bw->chunk_pos = 0;
            }
            bw->buffer = 0;
            bw->bits_in_buffer = 0;
        }
    }
}

int bitWriterFlush(BitWriter *bw) {
    if (bw->bits_in_buffer > 0) {
        int trailing_bits = 8 - bw->bits_in_buffer;
        bw->buffer = (unsigned char)(bw->buffer << trailing_bits);
        bw->chunk[bw->chunk_pos++] = bw->buffer;
        bw->buffer = 0;
        bw->bits_in_buffer = 0;
        if (bw->chunk_pos == sizeof(bw->chunk)) {
            write(bw->fd, bw->chunk, sizeof(bw->chunk));
            bw->chunk_pos = 0;
        }
        if (bw->chunk_pos > 0) {
            write(bw->fd, bw->chunk, bw->chunk_pos);
            bw->chunk_pos = 0;
        }
        return trailing_bits;
    }

    if (bw->chunk_pos > 0) {
        write(bw->fd, bw->chunk, bw->chunk_pos);
        bw->chunk_pos = 0;
    }
    return 0;
}
