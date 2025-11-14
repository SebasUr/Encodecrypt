#ifndef DECOMPRESS_H
#define DECOMPRESS_H

#include <stdint.h>
#include <stddef.h>

// Decompress a Huffman-coded file
// input_path: path to compressed file
// output_path: path to write decompressed file
// Returns: 0 on success, -1 on error
int decompressFile(const char *input_path, const char *output_path);

#endif // DECOMPRESS_H
