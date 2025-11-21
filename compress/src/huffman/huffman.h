#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

typedef struct huffmanNode {
    int symbol;
    uint64_t weight;
    unsigned long order;
    struct huffmanNode* left;
    struct huffmanNode* right;
} huffmanNode;

typedef struct Code {
    uint64_t bits;
    size_t length;
} Code;

huffmanNode* createNode(int symbol, uint64_t weight, unsigned long order);
size_t initializeTree(const int f_s[], huffmanNode* activeNodes[]);
int nodeComparator(const void *a, const void *b);
void assignCodes(const huffmanNode* node, uint64_t cur_bits, int cur_len, Code codes[256]);
huffmanNode* huffmanAlgorithm(const int f_s[], huffmanNode* activeNodes[], Code codes[256]);
void freeHuffmanTree(huffmanNode* root);

#endif // HUFFMAN_H