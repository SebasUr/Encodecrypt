#ifndef KEY_H
#define KEY_H

#include <stdint.h>

#define Nk 4
#define Nb 4
#define Nr 10

typedef uint8_t word[4];

extern const uint8_t sbox[256];
void key_expansion(const uint8_t *key, word* words);

#endif