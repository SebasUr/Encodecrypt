/*
    Implementation of the AES-128 cipher algorithm
    By: Sebastián Andrés Uribe Ruiz & Daniel Santana Meza
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include "include/aes.h"
#include "include/file_ops.h"

#define MAX_PASSWORD_LEN 128

int main(int argc, char *argv[]) {
    char *input_path = NULL;
    char *password = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "e:p:")) != -1) {
        switch (opt) {
            case 'e':
                input_path = optarg;
                break;
            case 'p':
                password = optarg;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (!input_path || !password) {
        printf("Error: Both path and password are required\n");
        print_usage(argv[0]);
        return 1;
    }

    if (is_directory(input_path)) {
        return encrypt_directory(input_path, password);
    } else {
        uint8_t key[16];
        derive_key_from_password(password, key);
        return encrypt_file(input_path, key);
    }
}