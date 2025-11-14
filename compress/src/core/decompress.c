#include "decompress.h"
#include "../huffman/huffman.h"
#include "../../utils/bitreader.h"
#include "../../utils/decompress_utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// Función principal de descompresión
int decompressFile(const char *input_path, const char *output_path) {
    int fd_in = open(input_path, O_RDONLY);
    if (fd_in < 0) {
        return -1;
    }
    
    // Leer y verificar encabezado mágico
    char magic[4];
    if (read(fd_in, magic, 4) != 4 || memcmp(magic, "HUF1", 4) != 0) {
        close(fd_in);
        return -1;
    }
    
    // Leer tamaño original
    uint64_t original_size;
    if (read(fd_in, &original_size, sizeof(uint64_t)) != sizeof(uint64_t)) {
        close(fd_in);
        return -1;
    }
    
    // Leer longitudes de códigos
    unsigned char lens[256];
    if (read(fd_in, lens, 256) != 256) {
        close(fd_in);
        return -1;
    }
    
    // Leer trailing_bits
    unsigned char trailing_bits;
    if (read(fd_in, &trailing_bits, 1) != 1) {
        close(fd_in);
        return -1;
    }
    
    // Reconstruir códigos canónicos
    Code codes[256];
    memset(codes, 0, sizeof(codes));
    reconstructCanonicalCodes(lens, codes);

    CanonicalEntry entries[256];
    CanonicalRow rows[257];
    size_t max_len = 0;
    if (prepareCanonicalRows(codes, entries, rows, &max_len) < 0) {
        close(fd_in);
        return -1;
    }
    
    // Abrir archivo de salida
    int fd_out = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0) {
        close(fd_in);
        return -1;
    }
    
    // Inicializar BitReader
    BitReader br;
    bitReaderInit(&br, fd_in);

    int status = 0;

    ByteWriter out;
    byteWriterInit(&out, fd_out);

    // Decodificar datos
    uint64_t bytes_written = 0;
    uint64_t current_code = 0;
    int current_length = 0;
    
    while (bytes_written < original_size) {
        int bit = bitReaderReadBit(&br);
        if (bit < 0) {
            status = -1;
            break;
        }
        
        // Construir código bit a bit
        current_code = (current_code << 1) | bit;
        current_length++;
        
        if ((size_t)current_length > max_len) {
            status = -1;
            break;
        }

        CanonicalRow row = rows[current_length];
        if (row.count > 0 && current_code >= row.base_code) {
            uint64_t offset = current_code - row.base_code;
            if (offset < (uint64_t)row.count) {
                CanonicalEntry entry = entries[row.start_index + (int)offset];
                unsigned char symbol = (unsigned char)entry.symbol;
                if (byteWriterPut(&out, symbol) != 0) {
                status = -1;
                break;
            }
            
            bytes_written++;
            current_code = 0;
            current_length = 0;
            }
        }
    }

    bitReaderClose(&br);
    if (status == 0) {
        if (byteWriterFlush(&out) != 0) {
            status = -1;
        }
    }
    close(fd_in);
    close(fd_out);
    return status;
}
