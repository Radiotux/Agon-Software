/*
 * wc.c - Word, line, character, and byte count
 * For Agon Light / Agondev
 * 
 * Usage: wc [options] [file...]
 * 
 * Options:
 *   -l              Count lines
 *   -w              Count words
 *   -c              Count bytes
 *   -m              Count characters (same as -c for ASCII)
 *   -L              Print the length of the longest line
 *   -h, --help      Show this help
 * 
 * If no option is given, -l -w -c is assumed
 * 
 * Examples:
 *   wc file.txt
 *   wc -l file.txt
 *   wc -l -w file1.txt file2.txt
 *   wc -L archivo.txt
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <agon/mos.h>

#define VERSION "1.0"
#define BUFFER_SIZE 4096

// Options
int opt_lines = 0;
int opt_words = 0;
int opt_bytes = 0;
int opt_chars = 0;
int opt_max_line = 0;
int opt_help = 0;
int file_count = 0;

// Default: all options if none specified
int opt_all = 1;

/**
 * Display help
 */
void show_help(void) {
    printf("wc v%s - Word, line, character, and byte count\n", VERSION);
    printf("Usage: wc [options] [file...]\n");
    printf("\nOptions:\n");
    printf("  -l              Count lines\n");
    printf("  -w              Count words\n");
    printf("  -c              Count bytes\n");
    printf("  -m              Count characters (same as -c for ASCII)\n");
    printf("  -L              Print the length of the longest line\n");
    printf("  -h, --help      Show this help\n");
    printf("\nIf no option is given, -l -w -c is assumed\n");
    printf("\nExamples:\n");
    printf("  wc file.txt\n");
    printf("  wc -l file.txt\n");
    printf("  wc -l -w file1.txt file2.txt\n");
    printf("  wc -L archivo.txt\n");
}

/**
 * Count statistics for a single file
 */
int count_file(const char *filename, long *lines, long *words, long *bytes, long *max_line) {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    int in_word = 0;
    int line_len = 0;
    int c;
    
    *lines = 0;
    *words = 0;
    *bytes = 0;
    *max_line = 0;
    
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "wc: cannot open '%s' for reading\n", filename);
        return 0;
    }
    
    while ((c = fgetc(fp)) != EOF) {
        (*bytes)++;
        
        if (c == '\n') {
            (*lines)++;
            if (line_len > *max_line) *max_line = line_len;
            line_len = 0;
        } else {
            line_len++;
        }
        
        // Word counting: non-space characters after space or at start
        if (isspace(c)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            (*words)++;
        }
    }
    
    // Handle last line without newline
    if (line_len > *max_line) *max_line = line_len;
    if (*bytes > 0 && line_len > 0) {
        // File ended without newline, count as one line
        if (*lines == 0) (*lines)++;
    }
    
    fclose(fp);
    return 1;
}

/**
 * Print statistics for a file
 */
void print_stats(long lines, long words, long bytes, long max_line, const char *filename, int is_total) {
    if (opt_lines || opt_all) printf("%8ld ", lines);
    if (opt_words || opt_all) printf("%8ld ", words);
    if (opt_bytes || opt_all) printf("%8ld ", bytes);
    if (opt_max_line) printf("%8ld ", max_line);
    
    if (!is_total) {
        printf("%s\n", filename);
    } else {
        printf("total\n");
    }
}

/**
 * Process a single file
 */
int process_file(const char *filename, long *total_lines, long *total_words, long *total_bytes, long *total_max) {
    long lines, words, bytes, max_line;
    
    if (!count_file(filename, &lines, &words, &bytes, &max_line)) {
        return 0;
    }
    
    print_stats(lines, words, bytes, max_line, filename, 0);
    
    if (total_lines) *total_lines += lines;
    if (total_words) *total_words += words;
    if (total_bytes) *total_bytes += bytes;
    if (total_max && max_line > *total_max) *total_max = max_line;
    
    return 1;
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    int i;
    long total_lines = 0;
    long total_words = 0;
    long total_bytes = 0;
    long total_max = 0;
    int error = 0;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-l") == 0) {
            opt_lines = 1;
            opt_all = 0;
        }
        else if (strcmp(argv[i], "-w") == 0) {
            opt_words = 1;
            opt_all = 0;
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-m") == 0) {
            opt_bytes = 1;
            opt_all = 0;
        }
        else if (strcmp(argv[i], "-L") == 0) {
            opt_max_line = 1;
            opt_all = 0;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "wc: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'wc -h' for help\n");
            return 1;
        }
        else {
            file_count++;
        }
    }
    
    // If no options, enable default ones
    if (opt_all && !opt_lines && !opt_words && !opt_bytes && !opt_max_line) {
        opt_lines = 1;
        opt_words = 1;
        opt_bytes = 1;
    }
    
    // Process files
    if (file_count == 0) {
        // Read from stdin
        fprintf(stderr, "wc: reading from stdin not supported yet\n");
        fprintf(stderr, "Please specify a file\n");
        return 1;
    }
    
    // Process each file
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        
        if (!process_file(argv[i], &total_lines, &total_words, &total_bytes, &total_max)) {
            error = 1;
        }
    }
    
    // Print total if more than one file
    if (file_count > 1) {
        print_stats(total_lines, total_words, total_bytes, total_max, NULL, 1);
    }
    
    return error;
}