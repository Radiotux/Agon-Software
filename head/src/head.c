/*
 * head.c - Output the first part of files
 * For Agon Light / Agondev
 * 
 * Usage: head [options] [file...]
 * 
 * Options:
 *   -n <lines>       Output the first NUM lines (default: 10)
 *   -c <bytes>       Output the first NUM bytes
 *   -q, --quiet      Never print headers
 *   -v, --verbose    Always print headers
 *   -h, --help       Show this help
 * 
 * Examples:
 *   head file.txt
 *   head -n 20 file.txt
 *   head -c 100 file.txt
 *   head -n 5 file1.txt file2.txt
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <agon/mos.h>

#define VERSION "1.0"
#define BUFFER_SIZE 4096
#define DEFAULT_LINES 10

// Options
int opt_lines = DEFAULT_LINES;
int opt_bytes = 0;
int opt_quiet = 0;
int opt_verbose = 0;
int file_count = 0;

/**
 * Display help
 */
void show_help(void) {
    printf("head v%s - Output the first part of files\n", VERSION);
    printf("Usage: head [options] [file...]\n");
    printf("\nOptions:\n");
    printf("  -n <lines>       Output the first NUM lines (default: 10)\n");
    printf("  -c <bytes>       Output the first NUM bytes\n");
    printf("  -q, --quiet      Never print headers\n");
    printf("  -v, --verbose    Always print headers\n");
    printf("  -h, --help       Show this help\n");
    printf("\nExamples:\n");
    printf("  head file.txt\n");
    printf("  head -n 20 file.txt\n");
    printf("  head -c 100 file.txt\n");
    printf("  head -n 5 file1.txt file2.txt\n");
    printf("\nNotes:\n");
    printf("  - If multiple files are given, a header is printed before each\n");
    printf("  - Use -q to suppress headers, -v to force them\n");
}

/**
 * Print file header (when multiple files)
 */
void print_header(const char *filename, int first_file) {
    if (opt_quiet) return;
    if (opt_verbose || file_count > 1) {
        if (!first_file) printf("\n");
        printf("==> %s <==\n", filename);
    }
}

/**
 * Output first N bytes of a file
 */
int head_bytes(const char *filename, long bytes) {
    FILE *fp;
    unsigned char buffer[BUFFER_SIZE];
    int bytes_read;
    long remaining = bytes;
    
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "head: cannot open '%s' for reading\n", filename);
        return 0;
    }
    
    while (remaining > 0 && (bytes_read = fread(buffer, 1, 
           (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE, fp)) > 0) {
        fwrite(buffer, 1, bytes_read, stdout);
        remaining -= bytes_read;
    }
    
    fclose(fp);
    return 1;
}

/**
 * Output first N lines of a file
 */
int head_lines(const char *filename, int lines) {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    int line_count = 0;
    
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "head: cannot open '%s' for reading\n", filename);
        return 0;
    }
    
    while (line_count < lines && fgets(buffer, BUFFER_SIZE, fp)) {
        printf("%s", buffer);
        line_count++;
    }
    
    fclose(fp);
    return 1;
}

/**
 * Process a single file
 */
int process_file(const char *filename, int first_file) {
    print_header(filename, first_file);
    
    if (opt_bytes > 0) {
        return head_bytes(filename, opt_bytes);
    } else {
        return head_lines(filename, opt_lines);
    }
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    int i;
    int first_file = 1;
    int error = 0;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            opt_quiet = 1;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            opt_verbose = 1;
        }
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            opt_lines = atoi(argv[++i]);
            if (opt_lines <= 0) opt_lines = DEFAULT_LINES;
            opt_bytes = 0;
        }
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            opt_bytes = atoi(argv[++i]);
            if (opt_bytes <= 0) opt_bytes = 0;
            opt_lines = 0;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "head: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'head -h' for help\n");
            return 1;
        }
        else {
            file_count++;
        }
    }
    
    // Need at least one file
    if (file_count == 0) {
        fprintf(stderr, "head: missing file operand\n");
        fprintf(stderr, "Try 'head -h' for help\n");
        return 1;
    }
    
    // Process each file
    first_file = 1;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // Skip options
            if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "-c") == 0) i++;
            continue;
        }
        
        if (!process_file(argv[i], first_file)) {
            error = 1;
        }
        first_file = 0;
    }
    
    return error;
}