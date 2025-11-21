#ifndef COMPRESS_H
#define COMPRESS_H

#include <stdint.h>
#include <stddef.h>
#include "../huffman/huffman.h"

int compressFile(const unsigned char *input, size_t input_size, const char *output_path, const Code codes[256]);

#endif // COMPRESS_H
