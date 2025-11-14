#include "decompress_utils.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Función auxiliar para comparar entradas canónicas
static int compareEntries(const void *a, const void *b) {
    const CanonicalEntry *ea = (const CanonicalEntry *)a;
    const CanonicalEntry *eb = (const CanonicalEntry *)b;
    
    if (ea->length != eb->length) {
        return (int)ea->length - (int)eb->length;
    }
    return ea->symbol - eb->symbol;
}

// Reconstruir códigos canónicos desde las longitudes
void reconstructCanonicalCodes(const unsigned char lens[256], Code codes[256]) {
    // Crear lista de símbolos con longitud > 0
    CanonicalEntry entries[256];
    int count = 0;
    
    for (int i = 0; i < 256; i++) {
        if (lens[i] > 0) {
            entries[count].symbol = i;
            entries[count].length = lens[i];
            count++;
        }
    }
    
    if (count == 0) return;
    
    // Ordenar por longitud, luego por símbolo
    qsort(entries, count, sizeof(CanonicalEntry), compareEntries);
    
    // Generar códigos canónicos
    uint64_t code = 0;
    size_t prev_len = entries[0].length;
    
    for (int i = 0; i < count; i++) {
        if (entries[i].length > prev_len) {
            code <<= (entries[i].length - prev_len);
            prev_len = entries[i].length;
        }
        
        entries[i].code = code;
        codes[entries[i].symbol].bits = code;
        codes[entries[i].symbol].length = entries[i].length;
        code++;
    }
}

// Preparar filas canónicas 
int prepareCanonicalRows(const Code codes[256], CanonicalEntry entries[256], CanonicalRow rows[257], size_t *out_max_len) {
    int count = 0;
    size_t max_len = 0;

    for (int i = 0; i < 256; i++) {
        if (codes[i].length > 0) {
            entries[count].symbol = i;
            entries[count].length = codes[i].length;
            entries[count].code = codes[i].bits;
            if (codes[i].length > max_len) {
                max_len = codes[i].length;
            }
            count++;
        }
    }

    if (count == 0) {
        *out_max_len = 0;
        return 0;
    }

    qsort(entries, count, sizeof(CanonicalEntry), compareEntries);

    for (size_t i = 0; i < 257; ++i) {
        rows[i].base_code = 0;
        rows[i].start_index = -1;
        rows[i].count = 0;
    }

    int idx = 0;
    while (idx < count) {
        size_t len = entries[idx].length;
        int start = idx;
        int end = idx;
        while (end < count && entries[end].length == len) {
            end++;
        }
        rows[len].base_code = entries[start].code;
        rows[len].start_index = start;
        rows[len].count = end - start;
        idx = end;
    }

    *out_max_len = max_len;
    return count;
}

// Funciones del ByteWriter
void byteWriterInit(ByteWriter *bw, int fd) {
    bw->fd = fd;
    bw->pos = 0;
}

int byteWriterFlush(ByteWriter *bw) {
    if (bw->pos == 0) {
        return 0;
    }
    ssize_t written = write(bw->fd, bw->buffer, bw->pos);
    if (written != (ssize_t)bw->pos) {
        return -1;
    }
    bw->pos = 0;
    return 0;
}

int byteWriterPut(ByteWriter *bw, unsigned char byte) {
    bw->buffer[bw->pos++] = byte;
    if (bw->pos == sizeof(bw->buffer)) {
        if (byteWriterFlush(bw) != 0) {
            return -1;
        }
    }
    return 0;
}
