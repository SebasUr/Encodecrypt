#include "compress.h"
#include "../../utils/bitwriter.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int compressFile(const unsigned char *input, size_t input_size, const char *output_path, const Code codes[256]) {
    int fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644); // fd es el file descriptor del archivo de salida
    if (fd < 0) {
        return -1;
    }

    // Encabezado mágico por estándar unix
    if (write(fd, "HUF1", 4) != 4) {
        close(fd);
        return -1;
    }

    // se guardan 8 bytes del tamaño original
    uint64_t original_size = (uint64_t)input_size;
    if (write(fd, &original_size, sizeof(uint64_t)) != sizeof(uint64_t)) {
        close(fd);
        return -1;
    }

    // se guardan 256 bytes con las longitudes de los códigos
    unsigned char lens[256];
    for (int i = 0; i < 256; i++) {
        lens[i] = (unsigned char)codes[i].length;
    }
    if (write(fd, lens, 256) != 256) {
        close(fd);
        return -1;
    }

    // ** para guardar la posición del byte de trailing bits.
    off_t trailing_pos = lseek(fd, 0, SEEK_CUR);
    unsigned char trailing_placeholder = 0;
    if (write(fd, &trailing_placeholder, 1) != 1) {
        close(fd);
        return -1;
    }

    BitWriter bw;
    bitWriterInit(&bw, fd);

    for (size_t i = 0; i < input_size; i++) {
        unsigned char byte = input[i]; // leemos byte del archivo original
        Code code = codes[byte]; // busca el codigo huffman
        
        // vamos escribiendo si los codigos están bien generados.
        if (code.length > 0) {
            bitWriterWrite(&bw, code.bits, (int)code.length);
        }
    }

    int trailing_bits = bitWriterFlush(&bw);

    if (lseek(fd, trailing_pos, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }
    
    unsigned char trailing_byte = (unsigned char)trailing_bits;
    if (write(fd, &trailing_byte, 1) != 1) {
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}
