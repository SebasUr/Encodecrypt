#include "bitreader.h"
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

void bitReaderInit(BitReader *br, int fd) {
    br->fd = fd;
    br->buffer = 0;
    br->pos = 0;
    br->bits_available = 0;
    br->eof = 0;
    br->mapped = NULL;
    br->mapped_size = 0;
    br->mapped_pos = 0;
    br->use_mmap = 0;

    struct stat st;
    if (fstat(fd, &st) == 0 && st.st_size > 0) {
        off_t current = lseek(fd, 0, SEEK_CUR);
        if (current >= 0 && current < st.st_size) {
            void *addr = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (addr != MAP_FAILED) {
                br->mapped = (unsigned char *)addr;
                br->mapped_size = (size_t)st.st_size;
                br->mapped_pos = (size_t)current;
                br->use_mmap = 1;
            }
        }
    }
}

int bitReaderReadBit(BitReader *br) {
    if (br->bits_available == 0) {
        if (br->use_mmap) {
            if (br->mapped_pos >= br->mapped_size) {
                br->eof = 1;
                return -1;
            }
            br->buffer = br->mapped[br->mapped_pos++];
        } else {
            ssize_t n = read(br->fd, &br->buffer, 1);
            if (n <= 0) {
                br->eof = 1;
                return -1; 
            }
        }
        br->bits_available = 8;
        br->pos = 0;
    }

    int bit = (br->buffer >> (7 - br->pos)) & 1;
    br->pos++;
    br->bits_available--;

    return bit;
}

void bitReaderClose(BitReader *br) {
    if (br->use_mmap && br->mapped != NULL && br->mapped_size > 0) {
        munmap(br->mapped, br->mapped_size);
    }
    br->mapped = NULL;
    br->mapped_size = 0;
    br->mapped_pos = 0;
    br->use_mmap = 0;
}
