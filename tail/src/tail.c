/*
 * tail.c - Output the last part of files
 * For Agon Light / Agondev
 * 
 * Usage: tail [options] [file...]
 * 
 * Options:
 *   -n <lines>       Output the last NUM lines (default: 10)
 *   -c <bytes>       Output the last NUM bytes
 *   -q, --quiet      Never print headers
 *   -v, --verbose    Always print headers
 *   -h, --help       Show this help
 * 
 * Examples:
 *   tail file.txt
 *   tail -n 20 file.txt
 *   tail -c 100 file.txt
 *   tail -n 5 file1.txt file2.txt
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
int opt_help = 0;
int file_count = 0;

/**
 * Display help
 */
void show_help(void) {
    printf("tail v%s - Output the last part of files\n", VERSION);
    printf("Usage: tail [options] [file...]\n");
    printf("\nOptions:\n");
    printf("  -n <lines>       Output the last NUM lines (default: 10)\n");
    printf("  -c <bytes>       Output the last NUM bytes\n");
    printf("  -q, --quiet      Never print headers\n");
    printf("  -v, --verbose    Always print headers\n");
    printf("  -h, --help       Show this help\n");
    printf("\nExamples:\n");
    printf("  tail file.txt\n");
    printf("  tail -n 20 file.txt\n");
    printf("  tail -c 100 file.txt\n");
    printf("  tail -n 5 file1.txt file2.txt\n");
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
 * Output last N bytes of a file
 */
int tail_bytes(const char *filename, long bytes) {
    FILE *fp;
    long file_size;
    long start_pos;
    unsigned char buffer[BUFFER_SIZE];
    int bytes_read;
    long remaining;
    
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "tail: cannot open '%s' for reading\n", filename);
        return 0;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    
    if (file_size <= bytes) {
        // File is smaller or equal, output everything
        fseek(fp, 0, SEEK_SET);
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
            fwrite(buffer, 1, bytes_read, stdout);
        }
    } else {
        // Seek to position
        start_pos = file_size - bytes;
        fseek(fp, start_pos, SEEK_SET);
        remaining = bytes;
        while (remaining > 0 && (bytes_read = fread(buffer, 1, 
               (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE, fp)) > 0) {
            fwrite(buffer, 1, bytes_read, stdout);
            remaining -= bytes_read;
        }
    }
    
    fclose(fp);
    return 1;
}

/**
 * Output last N lines of a file
 */
int tail_lines(const char *filename, int lines) {
    FILE *fp;
    char buffer[BUFFER_SIZE];
    char *line_buffers[BUFFER_SIZE / 80];  // Store last N lines
    int line_count = 0;
    int i;
    long file_size;
    long pos;
    int c;
    int line_start;
    
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "tail: cannot open '%s' for reading\n", filename);
        return 0;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    
    if (file_size == 0) {
        fclose(fp);
        return 1;
    }
    
    // Allocate line buffers
    for (i = 0; i < lines; i++) {
        line_buffers[i] = malloc(BUFFER_SIZE);
        if (!line_buffers[i]) {
            for (int j = 0; j < i; j++) free(line_buffers[j]);
            fclose(fp);
            fprintf(stderr, "tail: out of memory\n");
            return 0;
        }
        line_buffers[i][0] = '\0';
    }
    
    // Read file from end, collecting lines
    pos = file_size - 1;
    line_start = 1;
    
    while (pos >= 0 && line_count < lines) {
        fseek(fp, pos, SEEK_SET);
        c = fgetc(fp);
        
        if (c == '\n' && !line_start) {
            // Found end of a line, read it
            long line_end = pos + 1;
            long line_begin = pos + 1;
            long read_pos;
            int idx = 0;
            
            // Find start of line
            for (read_pos = line_end - 2; read_pos >= 0; read_pos--) {
                fseek(fp, read_pos, SEEK_SET);
                if (fgetc(fp) == '\n') {
                    line_begin = read_pos + 1;
                    break;
                }
            }
            
            // Read line
            fseek(fp, line_begin, SEEK_SET);
            idx = 0;
            while (line_begin + idx < line_end && idx < BUFFER_SIZE - 1) {
                int ch = fgetc(fp);
                if (ch == EOF) break;
                line_buffers[line_count][idx++] = ch;
            }
            line_buffers[line_count][idx] = '\0';
            line_count++;
            line_start = 1;
        } else {
            line_start = 0;
        }
        
        pos--;
    }
    
    // If we didn't get enough lines, read from beginning
    if (line_count < lines) {
        fseek(fp, 0, SEEK_SET);
        line_count = 0;
        while (fgets(buffer, BUFFER_SIZE, fp) && line_count < lines) {
            // Store lines in circular buffer
            if (line_buffers[line_count % lines]) {
                strcpy(line_buffers[line_count % lines], buffer);
            }
            line_count++;
        }
        
        // Output lines in order
        int start = (line_count > lines) ? line_count - lines : 0;
        int out_count = (line_count > lines) ? lines : line_count;
        
        for (i = 0; i < out_count; i++) {
            int idx = (start + i) % lines;
            printf("%s", line_buffers[idx]);
        }
    } else {
        // Output collected lines in reverse order
        for (i = line_count - 1; i >= 0; i--) {
            printf("%s", line_buffers[i]);
        }
    }
    
    // Clean up
    for (i = 0; i < lines; i++) {
        if (line_buffers[i]) free(line_buffers[i]);
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
        return tail_bytes(filename, opt_bytes);
    } else {
        return tail_lines(filename, opt_lines);
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
            fprintf(stderr, "tail: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'tail -h' for help\n");
            return 1;
        }
        else {
            // It's a filename
            file_count++;
        }
    }
    
    // Reset argument parsing to process files
    if (file_count == 0) {
        fprintf(stderr, "tail: missing file operand\n");
        fprintf(stderr, "Try 'tail -h' for help\n");
        return 1;
    }
    
    // Process each file
    first_file = 1;
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // Skip options (already parsed)
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