#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "huffman.h"

static _Thread_local Code *g_codes_for_sort = NULL;

huffmanNode* createNode(int symbol, uint64_t weight, unsigned long order) {
    huffmanNode* NewNode = malloc(sizeof *NewNode);
    if (NewNode == NULL) { return NULL; }

    NewNode->weight = weight;
    NewNode->symbol = symbol; 
    NewNode->left = NULL;
    NewNode->right = NULL;
    NewNode->order = order;

    return NewNode;
}

size_t initializeTree(const int f_s[], huffmanNode* activeNodes[]) {
    size_t count = 0;
    unsigned long next_order = 0;
    for (int i = 0; i < 256; i++){
        if (f_s[i] > 0){
            huffmanNode *n = createNode(i, (uint64_t)f_s[i], next_order++);
            if (!n) {
                for (size_t k = 0; k < count; ++k) free(activeNodes[k]);
                return 0;
            }
            activeNodes[count++] = n;
        }
    }
    return count;
}

int nodeComparator(const void *a, const void *b) {
    const huffmanNode *nodeA = *(const huffmanNode **)a;
    const huffmanNode *nodeB = *(const huffmanNode **)b;

    if(nodeA->weight < nodeB->weight) return -1;
    if(nodeA->weight > nodeB->weight) return 1;

    if(nodeA->order < nodeB->order) return -1;
    if(nodeA->order > nodeB->order) return 1;
    return 0;
}

int codeSymbolComparator(const void *pa, const void *pb) {
    const int a = *(const int*)pa;
    const int b = *(const int*)pb;
    size_t la = g_codes_for_sort[a].length;
    size_t lb = g_codes_for_sort[b].length;
    if (la < lb) return -1;
    if (la > lb) return 1;
    return a - b;
}

void buildCanonicalCodes(Code codes[256], const int symbols[], int m) {
    
    int max_len = 0;
    for (int i = 0; i < m; ++i){
        if ((int)codes[symbols[i]].length > max_len){
            max_len = (int)codes[symbols[i]].length;
        }
    }

    if (max_len == 0) { return; }
    int *bl_count = calloc(max_len + 1, sizeof(int)); 
    for (int i = 0; i < m; ++i) {
        bl_count[codes[symbols[i]].length] += 1;
    }

    uint32_t *first_code = calloc(max_len + 1, sizeof(uint32_t));
    uint32_t code = 0;
    for (int bits = 1; bits <= max_len; ++bits) {
        code = (code + (uint32_t)bl_count[bits - 1]) << 1;
        first_code[bits] = code;
    }

    for (int i = 0; i < m; ++i) {
        int s = symbols[i];
        int len = (int)codes[s].length;
        uint32_t assigned = first_code[len];
        first_code[len] += 1;
        codes[s].bits = (uint64_t)assigned;
    }

    free(bl_count);
    free(first_code);
}

void assignCodes(const huffmanNode* node, uint64_t cur_bits, int cur_len, Code codes[256]){
    if(!node){
        return;
    }

    if(node->symbol != -1){
        codes[node->symbol].bits = cur_bits;
        codes[node->symbol].length = cur_len;
    }

    assignCodes(node->left, (cur_bits << 1) | 0ULL, cur_len+1, codes);
    assignCodes(node->right, (cur_bits << 1) | 1ULL, cur_len+1, codes);
}

huffmanNode* huffmanAlgorithm(const int f_s[], huffmanNode* activeNodes[], Code codes[256]) {
    size_t count = initializeTree(f_s, activeNodes);

    if (count == 0) return NULL;
    if (count == 1) { return activeNodes[0];}

    qsort(activeNodes, count, sizeof(huffmanNode*), nodeComparator);

    unsigned long next_order = 256;
    while(count > 1){
        huffmanNode* firstA = activeNodes[0];
        huffmanNode* firstB = activeNodes[1];

        huffmanNode* p_node = createNode(-1, firstA->weight + firstB->weight, next_order++);
        if (!p_node) { return NULL; }

        p_node->left = firstA;
        p_node->right = firstB;

        activeNodes[0] = p_node;
        activeNodes[1] = activeNodes[count-1];
        count -=1;
        qsort(activeNodes, count, sizeof(huffmanNode*), nodeComparator);
    }

    huffmanNode* root = activeNodes[0];

    memset(codes, 0, 256 * sizeof(Code));
    assignCodes(root, 0ULL, 0, codes);

    int symbols[256]; int m = 0;
    for (int s = 0; s < 256; ++s) { if (codes[s].length > 0) symbols[m++] = s; }

    g_codes_for_sort = codes;
    qsort(symbols, m, sizeof(int), codeSymbolComparator);
    g_codes_for_sort = NULL;

    buildCanonicalCodes(codes, symbols, m);

    return root;
}

void freeHuffmanTree(huffmanNode* root) {
    if (root == NULL) {
        return;
    }
    freeHuffmanTree(root->left);
    freeHuffmanTree(root->right);
    free(root);
}



