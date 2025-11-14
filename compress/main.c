#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "src/archive/archive.h"
#include "utils/file_utils.h"

void print_usage(const char *prog_name) {
    printf("Huffman Compression Tool\n\n");
    
    printf("Usage:\n");
    printf("  %s [OPTIONS] <archive.huf> <files...>\n\n", prog_name);
    
    printf("Options:\n");
    printf("  -c, --create    Create compressed archive\n");
    printf("  -x, --extract   Extract all files from archive\n");
    printf("  -l, --list      List archive contents\n");
    printf("  -f, --file      Extract specific file\n");
    printf("  -o, --output    Output directory (default: current)\n");
    printf("  -h, --help      Show this help\n\n");
    
    printf("Examples:\n");
    printf("  Compress single file:\n");
    printf("    %s -c data.huf bible.txt\n\n", prog_name);
    
    printf("  Compress multiple files:\n");
    printf("    %s -c backup.huf file1.txt file2.txt dir/file3.txt\n\n", prog_name);
    
    printf("  List contents:\n");
    printf("    %s -l data.huf\n\n", prog_name);
    
    printf("  Extract all:\n");
    printf("    %s -x data.huf\n", prog_name);
    printf("    %s -x data.huf -o output/\n\n", prog_name);
    
    printf("  Extract specific file:\n");
    printf("    %s -f bible.txt data.huf -o bible_restored.txt\n\n", prog_name);
}

int main(int argc, char *argv[]) {

    int opt;
    int create_mode = 0;      // flag para -c (create)
    int extract_mode = 0;     // flag para -x (extract)
    int list_mode = 0;        // flag para -l (list)
    char *file_to_extract = NULL;  // para -f (extract file)
    char *output_dir = ".";   // directorio de salida (default: actual)

    static struct option long_options[] = {
        {"create",  no_argument,       0, 'c'},
        {"extract", no_argument,       0, 'x'},
        {"list",    no_argument,       0, 'l'},
        {"file",    required_argument, 0, 'f'},
        {"output",  required_argument, 0, 'o'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    // Parsear opciones usando getopt_long
    while ((opt = getopt_long(argc, argv, "cxlf:o:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                create_mode = 1;
                break;
            case 'x':
                extract_mode = 1;
                break;
            case 'l':
                list_mode = 1;
                break;
            case 'f':
                file_to_extract = optarg;
                break;
            case 'o':
                output_dir = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // estp para erificar que se haya seleccionado al menos un modo
    if (!create_mode && !extract_mode && !list_mode && !file_to_extract) {
        fprintf(stderr, "Error: Must specify an operation (-c, -x, -l, or -f)\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Vverificar min args
    if (optind >= argc) {
        fprintf(stderr, "Error: Missing archive file\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // El primer argumento no-opción es el archivo .huf
    const char *archive_path = argv[optind];
    
    // Primer modo, crear archivo.
    if (create_mode) {
        // Los archivos a comprimir vienen después del nombre del archive
        int num_files = argc - optind - 1;
        
        if (num_files == 0) {
            fprintf(stderr, "Error: Specify at least one file to compress\n");
            fprintf(stderr, "Example: %s -c data.huf file1.txt file2.txt\n", argv[0]);
            return 1;
        }
        
        // Crear array de punteros a los nombres de archivos/carpetas
        const char **input_paths = (const char **)&argv[optind + 1];
        
        // Expandir carpetas a archivos individuales
        // Si son archivos, se mantienen igual. Si son carpetas, se exploran recursivamente
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
        
        printf("\nTotal files to compress: %d\n\n", total_files);
        
        // Llamar a la función de archive con los archivos expandidos
        int result = createArchive(archive_path, (const char **)expanded_files, total_files);
        
        // Liberar memoria de los paths expandidos
        for (int i = 0; i < total_files; i++) {
            free(expanded_files[i]);
        }
        free(expanded_files);
        
        if (result != 0) {
            fprintf(stderr, "Error creating archive\n");
            return 1;
        }
        
        return 0;
    }
    
    // modo list
    if (list_mode) {
        if (listArchive(archive_path) != 0) {
            fprintf(stderr, "Error listing archive\n");
            return 1;
        }
        
        return 0;
    }
    
    // modo extract all
    if (extract_mode) {
        if (extractArchive(archive_path, output_dir) != 0) {
            fprintf(stderr, "Error extracting archive\n");
            return 1;
        }
        
        return 0;
    }
    
    // modo solo archivo
    if (file_to_extract) {
        // si no se especificó -o, usar el mismo nombre del archivo
        const char *output_path = output_dir;
        if (strcmp(output_dir, ".") == 0) {
            output_path = file_to_extract;
        }
        
        if (extractFile(archive_path, file_to_extract, output_path) != 0) {
            fprintf(stderr, "Error extracting file\n");
            return 1;
        }
        
        return 0;
    }
    
    // en caso de que no4
    print_usage(argv[0]);
    return 1;
}

