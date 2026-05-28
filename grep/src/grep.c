/*
 * grep.c - Search for patterns in files
 * For Agon Light / Agondev
 * 
 * Usage: grep [options] pattern [file...]
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mos.h>
#include <ctype.h>

#define MAX_PATH 512
#define MAX_LINE 4096
#define VERSION "1.1"

// Options
int opt_ignore_case = 0;
int opt_invert = 0;
int opt_line_number = 0;
int opt_count_only = 0;
int opt_files_with_matches = 0;
int opt_files_without_match = 0;
int opt_recursive = 0;
int opt_word_regexp = 0;
int opt_line_regexp = 0;
int opt_fixed_strings = 0;

// Pattern
char *pattern = NULL;

// File attribute definitions
#define AM_DIR   0x10

// Counter
int file_count = 0;

/**
 * Simple case-insensitive substring search
 */
int stristr(const char *haystack, const char *needle) {
    const char *h = haystack;
    const char *n = needle;
    
    while (*h) {
        const char *hs = h;
        const char *ns = n;
        
        while (*hs && *ns) {
            char hc = (*hs >= 'A' && *hs <= 'Z') ? *hs + 32 : *hs;
            char nc = (*ns >= 'A' && *ns <= 'Z') ? *ns + 32 : *ns;
            if (hc != nc) break;
            hs++;
            ns++;
        }
        
        if (!*ns) return 1;  // Found
        h++;
    }
    return 0;
}

/**
 * Check if line matches pattern
 */
int line_matches(const char *line) {
    int result = 0;
    
    // Basic substring search
    if (opt_ignore_case) {
        result = stristr(line, pattern);
    } else {
        result = (strstr(line, pattern) != NULL);
    }
    
    // Whole word matching (simplified)
    if (opt_word_regexp && result) {
        const char *pos = strstr(line, pattern);
        if (pos) {
            int start_ok = (pos == line || !isalnum(*(pos-1)));
            int end_ok = (!isalnum(*(pos + strlen(pattern))));
            result = start_ok && end_ok;
        } else {
            result = 0;
        }
    }
    
    // Whole line matching
    if (opt_line_regexp && result) {
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') len--;
        result = (strlen(pattern) == len);
    }
    
    // Invert match
    if (opt_invert) {
        result = !result;
    }
    
    return result;
}

/**
 * Print line with optional number
 */
void print_line(const char *line, int line_num, const char *filename) {
    if (opt_count_only) return;
    if (opt_files_with_matches || opt_files_without_match) return;
    
    if (filename && file_count > 1) {
        printf("%s:", filename);
    }
    
    if (opt_line_number && line_num > 0) {
        printf("%d:", line_num);
    }
    
    printf("%s", line);
}

/**
 * Process a single file
 */
int grep_file(const char *filename) {
    FILE *fp;
    char line[MAX_LINE];
    int line_num = 0;
    int matches_in_file = 0;
    int first_match = 1;
    
    fp = fopen(filename, "r");
    if (!fp) {
        printf("grep: %s: No such file\n", filename);
        return 0;
    }
    
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        if (line_matches(line)) {
            matches_in_file++;
            
            if (opt_files_with_matches) {
                if (first_match) {
                    printf("%s\n", filename);
                    first_match = 0;
                }
            }
            
            if (opt_count_only) {
                // Continue to count
            } else if (!opt_files_with_matches && !opt_files_without_match) {
                print_line(line, line_num, filename);
            }
        }
    }
    
    fclose(fp);
    
    if (opt_count_only && matches_in_file > 0) {
        if (file_count > 1) {
            printf("%s:", filename);
        }
        printf("%d\n", matches_in_file);
    }
    
    if (opt_files_without_match && matches_in_file == 0) {
        printf("%s\n", filename);
    }
    
    return matches_in_file;
}

/**
 * Recursively search directory
 */
void grep_recursive(const char *path) {
    DIR dir;
    FILINFO finfo;
    char full_path[MAX_PATH];
    uint8_t result;
    
    result = ffs_stat(&finfo, path);
    if (result != 0) return;
    
    if (!(finfo.fattrib & AM_DIR)) {
        file_count++;
        grep_file(path);
        return;
    }
    
    result = ffs_dopen(&dir, path);
    if (result != 0) return;
    
    while (1) {
        result = ffs_dread(&dir, &finfo);
        if (result != 0 || finfo.fname[0] == 0) break;
        
        if (strcmp(finfo.fname, ".") == 0 || strcmp(finfo.fname, "..") == 0) {
            continue;
        }
        
        if (strlen(path) + strlen(finfo.fname) + 2 < MAX_PATH) {
            if (strcmp(path, ".") == 0) {
                snprintf(full_path, MAX_PATH, "%s", finfo.fname);
            } else if (strcmp(path, "/") == 0) {
                snprintf(full_path, MAX_PATH, "/%s", finfo.fname);
            } else {
                snprintf(full_path, MAX_PATH, "%s/%s", path, finfo.fname);
            }
        } else {
            continue;
        }
        
        grep_recursive(full_path);
    }
    
    ffs_dclose(&dir);
}

/**
 * Display help
 */
void show_help(void) {
    printf("grep v%s - Search for patterns in files\n", VERSION);
    printf("Usage: grep [options] pattern [file...]\n");
    printf("\nSearch patterns:\n");
    printf("  text           Exact text (substring search)\n");
    printf("\nOptions:\n");
    printf("  -i             Ignore case\n");
    printf("  -v             Show non-matching lines\n");
    printf("  -w             Match whole words only\n");
    printf("  -x             Match whole lines only\n");
    printf("  -n             Show line numbers\n");
    printf("  -c             Show count of matches\n");
    printf("  -l             Show filenames with matches\n");
    printf("  -L             Show filenames without matches\n");
    printf("  -r             Search directories recursively\n");
    printf("  -h             This help\n");
    printf("\nExamples:\n");
    printf("  grep \"error\" log.txt\n");
    printf("  grep -i \"warning\" *.c\n");
    printf("  grep -n \"main\" *.c\n");
    printf("  grep -c \"error\" *.log\n");
    printf("  grep -l \"TODO\" *.c\n");
    printf("\nNotes:\n");
    printf("  - Use quotes for patterns with spaces\n");
    printf("  - Combine with find: find . -name \"*.c\" -exec grep -l \"main\" {} \\;\n");
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    int i;
    int file_start = 1;
    
    // Check for help
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            show_help();
            return 0;
        }
    }
    
    // Parse options
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-i") == 0) opt_ignore_case = 1;
            else if (strcmp(argv[i], "-v") == 0) opt_invert = 1;
            else if (strcmp(argv[i], "-n") == 0) opt_line_number = 1;
            else if (strcmp(argv[i], "-c") == 0) opt_count_only = 1;
            else if (strcmp(argv[i], "-l") == 0) opt_files_with_matches = 1;
            else if (strcmp(argv[i], "-L") == 0) opt_files_without_match = 1;
            else if (strcmp(argv[i], "-r") == 0) opt_recursive = 1;
            else if (strcmp(argv[i], "-w") == 0) opt_word_regexp = 1;
            else if (strcmp(argv[i], "-x") == 0) opt_line_regexp = 1;
            else {
                printf("grep: unknown option '%s'\n", argv[i]);
                printf("Try 'grep -h' for help\n");
                return 1;
            }
        } else {
            file_start = i;
            break;
        }
    }
    
    // Need pattern
    if (file_start >= argc) {
        printf("grep: missing pattern\n");
        printf("Try 'grep -h' for help\n");
        return 1;
    }
    
    pattern = argv[file_start];
    file_start++;
    
    // Need files
    if (file_start >= argc) {
        printf("grep: no files specified\n");
        printf("Try 'grep -h' for help\n");
        return 1;
    }
    
    // Process files
    for (i = file_start; i < argc; i++) {
        file_count++;
        
        if (opt_recursive) {
            grep_recursive(argv[i]);
        } else {
            grep_file(argv[i]);
        }
    }
    
    return 0;
}
