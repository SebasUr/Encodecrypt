#ifndef AES_H
#define AES_H

#include <stdint.h>
#include "key.h"

typedef enum {
    AES_SUCCESS = 0,
    AES_ERROR   = 1,
} aes_code_t;

typedef uint8_t state_t[4][4];

void plaintext_to_state(const uint8_t *plaintext, state_t *state);
void state_to_ciphertext(const state_t *state, uint8_t *ciphertext);
void sub_bytes(state_t *state);
void shift_rows(state_t *state);
void mix_columns(state_t *state);
void add_round_key(state_t *state, const word* words, const int round);

aes_code_t encrypt(const uint8_t *plaintext, const uint8_t *key, uint8_t *ciphertext);

#endif