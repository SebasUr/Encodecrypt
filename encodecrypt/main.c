#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <getopt.h>
#include <unistd.h>

#include "../compress/src/archive/archive.h"
#include "../compress/utils/file_utils.h"
#include "../encrypt/include/file_ops.h"

static void print_global_usage(const char *prog_name);
static int handle_compress(int argc, char **argv);
static int handle_encrypt(int argc, char **argv);
static int handle_pipeline(int argc, char **argv);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_global_usage(argv[0]);
        return 1;
    }

    const char *command = argv[1];
    if (strcmp(command, "compress") == 0) {
        return handle_compress(argc - 1, argv + 1);
    } else if (strcmp(command, "encrypt") == 0) {
        return handle_encrypt(argc - 1, argv + 1);
    } else if (strcmp(command, "pipeline") == 0) {
        return handle_pipeline(argc - 1, argv + 1);
    }

    fprintf(stderr, "Unknown command: %s\n\n", command);
    print_global_usage(argv[0]);
    return 1;
}

// -------------------------------------------------
// Global usage
// -------------------------------------------------

static void print_global_usage(const char *prog_name) {
    printf("Encodecrypt unified CLI\n\n");
    printf("Usage:\n");
    printf("  %s <command> [options]\n\n", prog_name);
    printf("Commands:\n");
    printf("  compress   Huffman compressor (create/list/extract)\n");
    printf("  encrypt    AES-128 encrypt/decrypt for files or directories\n");
    printf("  pipeline   End-to-end workflows (compress+encrypt or decrypt+extract)\n\n");
    printf("Examples:\n");
    printf("  %s compress -c backup.huf folder/\n", prog_name);
    printf("  %s encrypt -e folder -p secret\n", prog_name);
    printf("  %s pipeline --pack backup.huf folder/ --password secret --cleanup\n", prog_name);
    printf("  %s pipeline --unpack backup.huf.enc --password secret --output restore/\n", prog_name);
}

// -------------------------------------------------
// Compress command
// -------------------------------------------------

static void print_compress_usage(const char *prog_name) {
    printf("Usage: %s compress [OPTIONS] <archive.huf> <files...>\n\n", prog_name);
    printf("Options:\n");
    printf("  -c, --create      Create compressed archive\n");
    printf("  -x, --extract     Extract all files from archive\n");
    printf("  -l, --list        List archive contents\n");
    printf("  -f, --file <name> Extract specific file\n");
    printf("  -o, --output <dir>Output directory (default: current)\n");
    printf("  -h, --help        Show this help\n");
}

static int handle_compress(int argc, char **argv) {
    int opt;
    int create_mode = 0;
    int extract_mode = 0;
    int list_mode = 0;
    char *file_to_extract = NULL;
    char *output_dir = ".";

    static struct option long_options[] = {
        {"create",  no_argument,       0, 'c'},
        {"extract", no_argument,       0, 'x'},
        {"list",    no_argument,       0, 'l'},
        {"file",    required_argument, 0, 'f'},
        {"output",  required_argument, 0, 'o'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    optind = 1;
    while ((opt = getopt_long(argc, argv, "cxlf:o:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c': create_mode = 1; break;
            case 'x': extract_mode = 1; break;
            case 'l': list_mode = 1; break;
            case 'f': file_to_extract = optarg; break;
            case 'o': output_dir = optarg; break;
            case 'h':
                print_compress_usage("encodecrypt");
                return 0;
            default:
                print_compress_usage("encodecrypt");
                return 1;
        }
    }

    if (!create_mode && !extract_mode && !list_mode && !file_to_extract) {
        fprintf(stderr, "Error: Must specify an operation (-c, -x, -l, or -f)\n\n");
        print_compress_usage("encodecrypt");
        return 1;
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Missing archive file\n\n");
        print_compress_usage("encodecrypt");
        return 1;
    }

    const char *archive_path = argv[optind];

    if (create_mode) {
        int num_files = argc - optind - 1;
        if (num_files == 0) {
            fprintf(stderr, "Error: Specify at least one file to compress\n");
            return 1;
        }

        const char **input_paths = (const char **)&argv[optind + 1];
        int total_files = 0;
        char **expanded_files = expandPaths(input_paths, num_files, &total_files);

        if (!expanded_files || total_files == 0) {
            fprintf(stderr, "Error: No files to compress after expansion\n");
            if (expanded_files) {
                for (int i = 0; i < total_files; i++) {
                    free(expanded_files[i]);
                }
                free(expanded_files);
            }
            return 1;
        }

        int result = createArchive(archive_path, (const char **)expanded_files, total_files);

        for (int i = 0; i < total_files; i++) {
            free(expanded_files[i]);
        }
        free(expanded_files);

        return result;
    }

    if (list_mode) {
        return listArchive(archive_path);
    }

    if (extract_mode) {
        return extractArchive(archive_path, output_dir);
    }

    if (file_to_extract) {
        const char *output_path = output_dir;
        if (strcmp(output_dir, ".") == 0) {
            output_path = file_to_extract;
        }
        return extractFile(archive_path, file_to_extract, output_path);
    }

    print_compress_usage("encodecrypt");
    return 1;
}

// -------------------------------------------------
// Encrypt command
// -------------------------------------------------

static void print_encrypt_usage(const char *prog_name) {
    printf("Usage: %s encrypt [-e <path> | -d <path>] -p <password>\n\n", prog_name);
    printf("Options:\n");
    printf("  -e <path>   Encrypt file or directory\n");
    printf("  -d <path>   Decrypt file or directory\n");
    printf("  -p <pass>   Password\n");
    printf("  -h          Show this help\n");
}

static int handle_encrypt(int argc, char **argv) {
    char *input_path = NULL;
    char *password = NULL;
    int encrypt_mode = -1;
    int opt;

    optind = 1;
    while ((opt = getopt(argc, argv, "e:d:p:h")) != -1) {
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
            case 'h':
                print_encrypt_usage("encodecrypt");
                return 0;
            default:
                print_encrypt_usage("encodecrypt");
                return 1;
        }
    }

    if (encrypt_mode == -1 || !input_path || !password) {
        fprintf(stderr, "Error: Mode, path and password are required\n\n");
        print_encrypt_usage("encodecrypt");
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

// -------------------------------------------------
// Pipeline command
// -------------------------------------------------

static void print_pipeline_usage(const char *prog_name) {
    printf("Usage: %s pipeline [MODE] [OPTIONS]\n\n", prog_name);
    printf("Modes:\n");
    printf("  --pack     Compress files into <archive.huf> and encrypt result\n");
    printf("             Example: %s pipeline --pack backup.huf dir/ -p secret\n", prog_name);
    printf("  --unpack   Decrypt <archive.huf.enc> and extract files\n");
    printf("             Example: %s pipeline --unpack backup.huf.enc -p secret -o out/\n\n", prog_name);
    printf("Options:\n");
    printf("  -p, --password <pass>   Required in both modes\n");
    printf("  -o, --output <dir>      Extraction directory for --unpack (default: .)\n");
    printf("      --cleanup           Remove intermediate .huf after encrypt/decrypt\n");
    printf("  -h, --help              Show this help\n");
}

static char *derive_plain_from_enc(const char *enc_path) {
    size_t len = strlen(enc_path);
    if (len > 4 && strcmp(enc_path + len - 4, ".enc") == 0) {
        size_t new_len = len - 4;
        char *result = (char*)malloc(new_len + 1);
        if (!result) return NULL;
        memcpy(result, enc_path, new_len);
        result[new_len] = '\0';
        return result;
    }
    size_t new_len = len + 4;
    char *result = (char*)malloc(new_len + 1);
    if (!result) return NULL;
    snprintf(result, new_len + 1, "%s.dec", enc_path);
    return result;
}

static int handle_pipeline(int argc, char **argv) {
    bool pack_mode = false;
    bool unpack_mode = false;
    bool cleanup = false;
    char *password = NULL;
    char *output_dir = ".";
    int opt;

    static struct option long_options[] = {
        {"pack",     no_argument,       0, 1000},
        {"unpack",   no_argument,       0, 1001},
        {"password", required_argument, 0, 'p'},
        {"output",   required_argument, 0, 'o'},
        {"cleanup",  no_argument,       0, 1002},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    optind = 1;
    while ((opt = getopt_long(argc, argv, "p:o:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 1000: pack_mode = true; break;
            case 1001: unpack_mode = true; break;
            case 1002: cleanup = true; break;
            case 'p': password = optarg; break;
            case 'o': output_dir = optarg; break;
            case 'h':
                print_pipeline_usage("encodecrypt");
                return 0;
            default:
                print_pipeline_usage("encodecrypt");
                return 1;
        }
    }

    if (pack_mode == unpack_mode) {
        fprintf(stderr, "Error: Specify exactly one mode (--pack or --unpack).\n\n");
        print_pipeline_usage("encodecrypt");
        return 1;
    }

    if (!password) {
        fprintf(stderr, "Error: --password is required.\n\n");
        print_pipeline_usage("encodecrypt");
        return 1;
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Missing archive path.\n\n");
        print_pipeline_usage("encodecrypt");
        return 1;
    }

    uint8_t key[16];
    derive_key_from_password(password, key);

    if (pack_mode) {
        const char *archive_path = argv[optind];
        int num_files = argc - optind - 1;
        if (num_files <= 0) {
            fprintf(stderr, "Error: Provide at least one input file/directory to pack.\n");
            return 1;
        }

        const char **input_paths = (const char **)&argv[optind + 1];
        int total_files = 0;
        char **expanded_files = expandPaths(input_paths, num_files, &total_files);
        if (!expanded_files || total_files == 0) {
            fprintf(stderr, "Error: No files to compress after expansion.\n");
            if (expanded_files) {
                for (int i = 0; i < total_files; i++) {
                    free(expanded_files[i]);
                }
                free(expanded_files);
            }
            return 1;
        }

        int result = createArchive(archive_path, (const char **)expanded_files, total_files);
        for (int i = 0; i < total_files; i++) {
            free(expanded_files[i]);
        }
        free(expanded_files);

        if (result != 0) {
            fprintf(stderr, "Error creating archive.\n");
            return result;
        }

        result = process_file_syscalls(archive_path, key, 1);
        if (result != 0) {
            fprintf(stderr, "Error encrypting archive.\n");
            return result;
        }

        if (cleanup) {
            remove(archive_path);
        }

        printf("Pack workflow completed: %s.enc\n", archive_path);
        return 0;
    }

    // unpack mode
    const char *encrypted_archive = argv[optind];
    int result = process_file_syscalls(encrypted_archive, key, 0);
    if (result != 0) {
        fprintf(stderr, "Error decrypting archive.\n");
        return result;
    }

    char *decrypted_path = derive_plain_from_enc(encrypted_archive);
    if (!decrypted_path) {
        fprintf(stderr, "Error allocating memory for filenames.\n");
        return 1;
    }

    result = extractArchive(decrypted_path, output_dir);
    if (result != 0) {
        fprintf(stderr, "Error extracting archive %s.\n", decrypted_path);
        if (cleanup) {
            remove(decrypted_path);
        }
        free(decrypted_path);
        return result;
    }

    if (cleanup) {
        remove(decrypted_path);
    }

    printf("Unpack workflow completed into %s.\n", output_dir);
    free(decrypted_path);
    return 0;
}
