/*
 * locate.c - Fast file search using pre-built database
 * For Agon Light / Agondev
 * 
 * Author: Original implementation by Agon community
 * Version: 1.0
 * 
 * Compilation: make locate
 * Usage: locate [options] <pattern>
 * 
 * Description:
 *   Searches for files matching the given pattern using the database
 *   created by 'updatedb'. Supports wildcards * and ?.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "locate_db.h"

#define BUFFER_SIZE 4096

/**
 * Pattern matching with case sensitivity option
 */
int match_pattern_case(const char *pattern, const char *filename, int case_insensitive) {
    const char *p = pattern;
    const char *f = filename;
    
    while (*p && *f) {
        if (*p == '*') {
            p++;
            if (!*p) return 1;
            while (*f) {
                if (match_pattern_case(p, f, case_insensitive)) return 1;
                f++;
            }
            return 0;
        }
        else if (*p == '?') {
            p++;
            f++;
        }
        else {
            char pc = *p;
            char fc = *f;
            
            if (case_insensitive) {
                pc = (pc >= 'A' && pc <= 'Z') ? pc + 32 : pc;
                fc = (fc >= 'A' && fc <= 'Z') ? fc + 32 : fc;
            }
            
            if (pc != fc) return 0;
            p++;
            f++;
        }
    }
    if (*p == '*') p++;
    return !*p && !*f;
}

/**
 * Search database for matching files
 */
int search_database(const char *db_path, const char *pattern, 
                    int show_paths, int count_only, int case_insensitive) {
    FILE *fp;
    unsigned char buffer[BUFFER_SIZE];
    int bytes_read;
    int matches = 0;
    long file_size;
    long pos = 0;
    int i;
    
    fp = fopen(db_path, "rb");
    if (!fp) {
        printf("Error: Could not open database %s\n", db_path);
        printf("Run 'updatedb' first to create the database\n");
        return -1;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    // Skip header (first 276 bytes)
    fseek(fp, 276, SEEK_SET);
    pos = 276;
    
    int processed = 0;
    
    while (pos < file_size) {
        // Read a block
        int to_read = BUFFER_SIZE;
        if (pos + to_read > file_size) {
            to_read = file_size - pos;
        }
        
        bytes_read = fread(buffer, 1, to_read, fp);
        if (bytes_read <= 0) break;
        
        // Find strings in this block
        for (i = 0; i < bytes_read - 4; i++) {
            if (buffer[i] >= 32 && buffer[i] < 127) {
                // Find end of string
                int start = i;
                int end = i;
                while (end < bytes_read && buffer[end] >= 32 && buffer[end] < 127) {
                    end++;
                }
                
                // Only process strings with at least 4 characters
                if (end - start >= 4) {
                    // Extract string
                    char str[512];
                    int len = end - start;
                    if (len > 511) len = 511;
                    memcpy(str, &buffer[start], len);
                    str[len] = '\0';
                    
                    // Check if it looks like a filename (contains . or /)
                    int is_filename = 0;
                    int j;
                    for (j = 0; j < len; j++) {
                        if (str[j] == '.' || str[j] == '/') {
                            is_filename = 1;
                            break;
                        }
                    }
                    
                    if (is_filename) {
                        processed++;
                        
                        // Extract just the filename
                        const char *name = strrchr(str, '/');
                        if (!name) name = str;
                        else name++;
                        
                        // Show progress every 5000 files
                        if (!count_only && processed % 5000 == 0) {
                            printf("Processing: %d files\r", processed);
                            fflush(stdout);
                        }
                        
                        if (match_pattern_case(pattern, name, case_insensitive)) {
                            matches++;
                            if (!count_only) {
                                if (show_paths) {
                                    printf("%s\n", str);
                                } else {
                                    printf("%s\n", name);
                                }
                            }
                        }
                    }
                }
                i = end; // Skip this string
            }
        }
        
        pos += bytes_read;
        fseek(fp, pos, SEEK_SET);
    }
    
    fclose(fp);
    
    if (!count_only) {
        printf("\n");
    }
    
    if (count_only) {
        printf("%d\n", matches);
    } else if (matches == 0) {
        printf("No files found matching '%s'%s\n", 
               pattern, case_insensitive ? " (case-insensitive)" : " (case-sensitive)");
    } else {
        printf("\nTotal: %d file(s) found\n", matches);
    }
    
    return matches;
}

/**
 * Display help
 */
void show_help(void) {
    printf("Usage: locate [options] <pattern>\n");
    printf("\nOptions:\n");
    printf("  -h, --help      Show this help message\n");
    printf("  -p, --paths     Show full paths (default)\n");
    printf("  -n, --names     Show only filenames\n");
    printf("  -c, --count     Show only the number of matches\n");
    printf("  -i, --ignore-case Case-insensitive search\n");
    printf("\nWildcards:\n");
    printf("  *  - Any sequence of characters\n");
    printf("  ?  - Any single character\n");
    printf("\nExamples:\n");
    printf("  locate \"*.bin\"           - Find all .bin files (case-sensitive)\n");
    printf("  locate -i \"*.BIN\"        - Find .bin, .BIN, .Bin (case-insensitive)\n");
    printf("  locate -n \"test*\"        - Show filenames starting with 'test'\n");
    printf("  locate -c \"*.c\"          - Count .c files\n");
    printf("  locate \"*\"               - List all files\n");
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    char *pattern = NULL;
    int show_paths = 1;
    int count_only = 0;
    int case_insensitive = 0;
    int i;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                show_help();
                return 0;
            }
            else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--paths") == 0) {
                show_paths = 1;
            }
            else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--names") == 0) {
                show_paths = 0;
            }
            else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--count") == 0) {
                count_only = 1;
            }
            else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--ignore-case") == 0) {
                case_insensitive = 1;
            }
        } else {
            pattern = argv[i];
        }
    }
    
    if (!pattern) {
        printf("Error: You must specify a search pattern\n");
        show_help();
        return 1;
    }
    
    printf("Searching: %s%s\n", pattern, case_insensitive ? " (case-insensitive)" : "");
    if (!count_only && show_paths) {
        printf("----------------------------------------\n");
    }
    
    search_database(LOCATE_DB_PATH, pattern, show_paths, count_only, case_insensitive);
    
    return 0;
}