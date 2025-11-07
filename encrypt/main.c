/*
    Implementation of the AES-128 cipher algorithm
    By: Sebastián Andrés Uribe Ruiz & Daniel Santana Meza
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include "include/aes.h"
#include "include/file_ops.h"

#define MAX_PASSWORD_LEN 128
#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    char *input_path = NULL;
    char *password = NULL;
    int encrypt_mode = -1; // -1 = not set, 1 = encrypt, 0 = decrypt
    int opt;

    while ((opt = getopt(argc, argv, "e:d:p:")) != -1) {
        switch (opt) {
            case 'e':
                input_path = optarg;
                encrypt_mode = 1;
                break;
            case 'd':
                input_path = optarg;
                encrypt_mode = 0;
                break;
            case 'p':
                password = optarg;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (encrypt_mode == -1 || !input_path || !password) {
        printf("Error: Mode, path and password are required\n");
        print_usage(argv[0]);
        return 1;
    }

    if (is_directory(input_path)) {
        return process_directory(input_path, password, encrypt_mode);
    } else {
        uint8_t key[16];
        derive_key_from_password(password, key);
        return process_file_syscalls(input_path, key, encrypt_mode);
    }
}