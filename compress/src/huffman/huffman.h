#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

typedef struct huffmanNode {
    int symbol;              // Character/byte value (-1 for internal nodes)
    uint64_t weight;         // Frequency/weight
    unsigned long order;     // Order for tie-breaking
    struct huffmanNode* left;
    struct huffmanNode* right;
} huffmanNode;

typedef struct Code {
    uint64_t bits;     // Bits del código (máximo 64 bits)
    size_t length;     // Longitud en bits
} Code;

// Creates a new Huffman node
huffmanNode* createNode(int symbol, uint64_t weight, unsigned long order);

// Initializes the tree with leaf nodes from frequency array
// f_s: frequency array of size 256
// activeNodes: array to store created nodes (must be pre-allocated: malloc(256 * sizeof(huffmanNode*)))
// Returns: number of nodes created
size_t initializeTree(const int f_s[], huffmanNode* activeNodes[]);

// Comparator function for qsort
int nodeComparator(const void *a, const void *b);

// Assign codes
// based on huffmanTree assigns in a code list  0 for left decisions, 1 for right decisions.
void assignCodes(const huffmanNode* node, uint64_t cur_bits, int cur_len, Code codes[256]);


// Builds the Huffman tree from frequency array
// f_s: frequency array of size 256
// activeNodes: working array (must be pre-allocated: malloc(256 * sizeof(huffmanNode*)))
// Returns: root node of the Huffman tree, or NULL on error
huffmanNode* huffmanAlgorithm(const int f_s[], huffmanNode* activeNodes[], Code codes[256]);

// Frees the entire Huffman tree
void freeHuffmanTree(huffmanNode* root);

#endif // HUFFMAN_H