/*
 * sort.c - Sort lines of text files
 * For Agon Light / Agondev
 * 
 * Usage: sort [options] file...
 * 
 * Options:
 *   -r              Reverse the result of comparisons
 *   -n              Compare according to numeric value
 *   -u              Output only the first of an equal run
 *   -o <file>       Write result to file instead of stdout
 *   -k <pos1[,pos2]> Sort by key starting at pos1 and ending at pos2
 *   -t <char>       Use char as field separator (default: whitespace)
 *   -h, --help      Show this help
 * 
 * Examples:
 *   sort file.txt
 *   sort -r file.txt
 *   sort -n numbers.txt
 *   sort -u file.txt
 *   sort -o output.txt file.txt
 *   sort -t: -k2 /etc/passwd
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <agon/mos.h>

#define VERSION "1.0"
#define MAX_LINES 10000
#define MAX_LINE_LEN 1024

// Options
int opt_reverse = 0;
int opt_numeric = 0;
int opt_unique = 0;
int opt_key = 0;
int opt_key_start = 1;
int opt_key_end = 0;
char opt_separator = 0;
char *output_file = NULL;
int opt_help = 0;

// Storage
char **lines = NULL;
int line_count = 0;
int line_capacity = 0;

/**
 * Display help
 */
void show_help(void) {
    printf("sort v%s - Sort lines of text files\n", VERSION);
    printf("Usage: sort [options] file...\n");
    printf("\nOptions:\n");
    printf("  -r              Reverse the result of comparisons\n");
    printf("  -n              Compare according to numeric value\n");
    printf("  -u              Output only the first of an equal run\n");
    printf("  -o <file>       Write result to file instead of stdout\n");
    printf("  -k <pos1[,pos2]> Sort by key starting at pos1 and ending at pos2\n");
    printf("  -t <char>       Use char as field separator (default: whitespace)\n");
    printf("  -h, --help      Show this help\n");
    printf("\nNote: At least one file must be specified\n");
    printf("\nExamples:\n");
    printf("  sort file.txt\n");
    printf("  sort -r file.txt\n");
    printf("  sort -n numbers.txt\n");
    printf("  sort -u file.txt\n");
    printf("  sort -o output.txt file.txt\n");
    printf("  sort -t: -k2 /etc/passwd\n");
}

/**
 * Get the key field from a line
 */
char *get_key(const char *line, char *buffer, int max_len) {
    char *start, *end;
    int field = 1;
    int i;
    
    if (!opt_key) {
        strncpy(buffer, line, max_len - 1);
        buffer[max_len - 1] = '\0';
        return buffer;
    }
    
    // Find start of key field
    start = (char *)line;
    while (*start && field < opt_key_start) {
        if (opt_separator) {
            if (*start == opt_separator) field++;
        } else {
            if (isspace(*start)) {
                while (*start && isspace(*start)) start++;
                field++;
                continue;
            }
        }
        start++;
    }
    
    // Skip leading separators
    if (opt_separator) {
        while (*start && *start == opt_separator) start++;
    } else {
        while (*start && isspace(*start)) start++;
    }
    
    // Find end of key field
    end = start;
    while (*end) {
        if (opt_key_end && field >= opt_key_end) break;
        if (opt_separator) {
            if (*end == opt_separator) break;
        } else {
            if (isspace(*end)) break;
        }
        end++;
        if (!opt_key_end && *end && (opt_separator ? *end == opt_separator : isspace(*end))) break;
    }
    
    // Copy key to buffer
    i = 0;
    while (start < end && i < max_len - 1) {
        buffer[i++] = *start++;
    }
    buffer[i] = '\0';
    
    return buffer;
}

/**
 * Compare two strings (numeric or lexicographic)
 */
int compare_strings(const char *a, const char *b) {
    char key_a[MAX_LINE_LEN];
    char key_b[MAX_LINE_LEN];
    const char *str_a, *str_b;
    
    if (opt_key) {
        get_key(a, key_a, MAX_LINE_LEN);
        get_key(b, key_b, MAX_LINE_LEN);
        str_a = key_a;
        str_b = key_b;
    } else {
        str_a = a;
        str_b = b;
    }
    
    if (opt_numeric) {
        double num_a = atof(str_a);
        double num_b = atof(str_b);
        if (num_a < num_b) return -1;
        if (num_a > num_b) return 1;
        return 0;
    } else {
        return strcmp(str_a, str_b);
    }
}

/**
 * Comparison function for qsort
 */
int compare(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    int result = compare_strings(sa, sb);
    
    if (opt_reverse) {
        return -result;
    }
    return result;
}

/**
 * Add a line to the array
 */
int add_line(const char *line) {
    char *copy;
    
    if (line_count >= line_capacity) {
        int new_capacity = line_capacity == 0 ? 100 : line_capacity * 2;
        char **new_lines = realloc(lines, new_capacity * sizeof(char *));
        if (!new_lines) return 0;
        lines = new_lines;
        line_capacity = new_capacity;
    }
    
    copy = malloc(strlen(line) + 1);
    if (!copy) return 0;
    strcpy(copy, line);
    lines[line_count++] = copy;
    
    return 1;
}

/**
 * Read lines from a file
 */
int read_file(const char *filename) {
    FILE *fp;
    char buffer[MAX_LINE_LEN];
    
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "sort: cannot open '%s' for reading\n", filename);
        return 0;
    }
    
    while (fgets(buffer, MAX_LINE_LEN, fp)) {
        // Remove trailing newline
        int len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        if (!add_line(buffer)) {
            fclose(fp);
            return 0;
        }
    }
    
    fclose(fp);
    return 1;
}

/**
 * Write sorted lines to output
 */
void write_output(void) {
    FILE *fp = stdout;
    int i;
    
    if (output_file) {
        fp = fopen(output_file, "wb");
        if (!fp) {
            fprintf(stderr, "sort: cannot open '%s' for writing\n", output_file);
            return;
        }
    }
    
    for (i = 0; i < line_count; i++) {
        if (opt_unique && i > 0 && compare_strings(lines[i], lines[i-1]) == 0) {
            continue;
        }
        fprintf(fp, "%s\n", lines[i]);
    }
    
    if (fp != stdout) fclose(fp);
}

/**
 * Clean up memory
 */
void cleanup(void) {
    int i;
    if (lines) {
        for (i = 0; i < line_count; i++) {
            free(lines[i]);
        }
        free(lines);
        lines = NULL;
    }
}

/**
 * Parse key option (-k)
 */
int parse_key_option(const char *arg) {
    const char *p = arg;
    char *endptr;
    
    opt_key_start = strtol(p, &endptr, 10);
    if (p == endptr) return 0;
    
    if (*endptr == ',') {
        opt_key_end = strtol(endptr + 1, &endptr, 10);
    }
    
    opt_key = 1;
    return 1;
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    int i;
    int files = 0;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-r") == 0) {
            opt_reverse = 1;
        }
        else if (strcmp(argv[i], "-n") == 0) {
            opt_numeric = 1;
        }
        else if (strcmp(argv[i], "-u") == 0) {
            opt_unique = 1;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
        else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            if (!parse_key_option(argv[++i])) {
                fprintf(stderr, "sort: invalid key specification\n");
                return 1;
            }
        }
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            opt_separator = argv[++i][0];
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "sort: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'sort -h' for help\n");
            return 1;
        }
        else {
            files++;
        }
    }
    
    // Need at least one file
    if (files == 0) {
        fprintf(stderr, "sort: missing file operand\n");
        fprintf(stderr, "Try 'sort -h' for help\n");
        return 1;
    }
    
    // Read all files
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // Skip options
            if (strcmp(argv[i], "-o") == 0 || 
                strcmp(argv[i], "-k") == 0 || 
                strcmp(argv[i], "-t") == 0) i++;
            continue;
        }
        if (!read_file(argv[i])) {
            cleanup();
            return 1;
        }
    }
    
    if (line_count == 0) {
        cleanup();
        return 0;
    }
    
    // Sort
    qsort(lines, line_count, sizeof(char *), compare);
    
    // Output
    write_output();
    
    cleanup();
    return 0;
}