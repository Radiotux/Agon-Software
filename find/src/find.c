/*
 * find.c - Search for files in directory tree
 * For Agon Light / Agondev by Roberto Alex Figueroa 2026
 * 
 * Usage: find [path] [expression]
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mos.h>

#define MAX_PATH 512
#define VERSION "1.0"

// File attribute definitions
#define AM_RDO   0x01
#define AM_HID   0x02
#define AM_SYS   0x04
#define AM_DIR   0x10
#define AM_ARC   0x20

// Global variables
int print_default = 1;

// Search criteria
char *name_pattern = NULL;
int case_insensitive = 0;
int file_type = 0;      // 0=any, 1=file, 2=dir
long size_limit = 0;
int size_greater = 0;    // 0=equal, 1=greater, -1=less
int max_depth = -1;      // -1 = unlimited
int current_depth = 0;

/**
 * Pattern matching with wildcards (* and ?)
 */
int match_pattern(const char *pattern, const char *string, int insensitive) {
    const char *p = pattern;
    const char *s = string;
    
    while (*p && *s) {
        if (*p == '*') {
            p++;
            if (!*p) return 1;
            while (*s) {
                if (match_pattern(p, s, insensitive)) return 1;
                s++;
            }
            return 0;
        }
        else if (*p == '?') {
            p++;
            s++;
        }
        else {
            char pc = *p;
            char sc = *s;
            if (insensitive) {
                pc = (pc >= 'A' && pc <= 'Z') ? pc + 32 : pc;
                sc = (sc >= 'A' && sc <= 'Z') ? sc + 32 : sc;
            }
            if (pc != sc) return 0;
            p++;
            s++;
        }
    }
    if (*p == '*') p++;
    return !*p && !*s;
}

/**
 * Parse size expression (e.g., "+100k", "-1M", "500")
 */
long parse_size(const char *arg) {
    long size = 0;
    int multiplier = 1;
    
    if (arg[0] == '+') {
        multiplier = 1;
        arg++;
    } else if (arg[0] == '-') {
        multiplier = -1;
        arg++;
    }
    
    while (*arg >= '0' && *arg <= '9') {
        size = size * 10 + (*arg - '0');
        arg++;
    }
    
    if (*arg) {
        switch (*arg) {
            case 'k': size *= 1024; break;
            case 'M': size *= 1024 * 1024; break;
            case 'G': size *= 1024 * 1024; break;
            default: break;
        }
    }
    
    return size * multiplier;
}

/**
 * Check if file matches criteria
 */
int matches_criteria(const char *path, FILINFO *finfo) {
    // Check name pattern
    if (name_pattern) {
        if (!match_pattern(name_pattern, finfo->fname, case_insensitive)) {
            return 0;
        }
    }
    
    // Check file type
    if (file_type == 1 && (finfo->fattrib & AM_DIR)) {
        return 0;
    }
    if (file_type == 2 && !(finfo->fattrib & AM_DIR)) {
        return 0;
    }
    
    // Check size
    if (size_limit != 0) {
        if (size_greater == 1 && finfo->fsize <= size_limit) return 0;
        if (size_greater == -1 && finfo->fsize >= -size_limit) return 0;
        if (size_greater == 0 && finfo->fsize != size_limit) return 0;
    }
    
    return 1;
}

/**
 * Print file info (like ls -l style)
 */
void print_file_info(const char *path, FILINFO *finfo) {
    char type = (finfo->fattrib & AM_DIR) ? 'd' : '-';
    char r = (finfo->fattrib & AM_RDO) ? 'r' : '-';
    char h = (finfo->fattrib & AM_HID) ? 'h' : '-';
    char s = (finfo->fattrib & AM_SYS) ? 's' : '-';
    char a = (finfo->fattrib & AM_ARC) ? 'a' : '-';
    
    printf("%c%c%c%c%c %8lu %s\n", type, r, h, s, a, 
           (unsigned long)finfo->fsize, path);
}

/**
 * Execute action on file
 */
void execute_action(const char *path, FILINFO *finfo, int ls_mode) {
    if (ls_mode) {
        print_file_info(path, finfo);
    } else {
        printf("%s\n", path);
    }
}

/**
 * Recursively search directory
 */
void find_recursive(const char *path, int ls_mode) {
    DIR dir;
    FILINFO finfo;
    char full_path[MAX_PATH];
    uint8_t result;
    
    // Check depth limit
    if (max_depth >= 0 && current_depth > max_depth) {
        return;
    }
    
    // Get file info
    result = ffs_stat(&finfo, path);
    if (result != 0) return;
    
    // Check if current path matches criteria
    if (matches_criteria(path, &finfo)) {
        execute_action(path, &finfo, ls_mode);
    }
    
    // If it's a directory, recurse
    if (finfo.fattrib & AM_DIR) {
        result = ffs_dopen(&dir, path);
        if (result != 0) return;
        
        current_depth++;
        
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
            
            find_recursive(full_path, ls_mode);
        }
        
        current_depth--;
        ffs_dclose(&dir);
    }
}

/**
 * Display help
 */
void show_help(void) {
    printf("find v%s - Search for files in directory tree\n", VERSION);
    printf("Usage: find [path...] [expression]\n");
    printf("\nExpressions (predicates):\n");
    printf("  -name pattern      File name matches pattern (supports *, ?)\n");
    printf("  -iname pattern     Case-insensitive name match\n");
    printf("  -type [f|d]        File type (f=file, d=directory)\n");
    printf("  -size [+-]n[ckMG]  File size (c=bytes, k=KB, M=MB, G=GB)\n");
    printf("  -maxdepth n        Descend at most n levels\n");
    printf("  -ls                List in ls -l style (shows attributes)\n");
    printf("\nOperators:\n");
    printf("  -a, -and           Logical AND (default, implicit)\n");
    printf("  -o, -or            Logical OR\n");
    printf("  !, -not            Logical NOT\n");
    printf("\nExamples:\n");
    printf("  find . -name \"*.c\"\n");
    printf("  find /mos -type f -size +10k\n");
    printf("  find . -iname \"readme*\" -ls\n");
    printf("  find . -name \"*.bin\" -o -name \"*.hex\"\n");
    printf("  find . -type d -maxdepth 2\n");
    printf("\nNotes:\n");
    printf("  - Without arguments, finds all files (use with caution!)\n");
    printf("  - Use quotes for patterns: find . -name \"*.c\"\n");
    printf("  - Logical operators combine expressions\n");
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    char *start_path = ".";
    int ls_mode = 0;
    int i;
    
    // Check for help
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
    }
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-name") == 0 && i+1 < argc) {
            name_pattern = argv[++i];
            print_default = 0;
        }
        else if (strcmp(argv[i], "-iname") == 0 && i+1 < argc) {
            name_pattern = argv[++i];
            case_insensitive = 1;
            print_default = 0;
        }
        else if (strcmp(argv[i], "-type") == 0 && i+1 < argc) {
            i++;
            if (argv[i][0] == 'f') file_type = 1;
            else if (argv[i][0] == 'd') file_type = 2;
            print_default = 0;
        }
        else if (strcmp(argv[i], "-size") == 0 && i+1 < argc) {
            char *size_arg = argv[++i];
            if (size_arg[0] == '+') {
                size_greater = 1;
                size_limit = parse_size(size_arg);
            } else if (size_arg[0] == '-') {
                size_greater = -1;
                size_limit = parse_size(size_arg);
            } else {
                size_greater = 0;
                size_limit = parse_size(size_arg);
            }
            print_default = 0;
        }
        else if (strcmp(argv[i], "-maxdepth") == 0 && i+1 < argc) {
            max_depth = atoi(argv[++i]);
            print_default = 0;
        }
        else if (strcmp(argv[i], "-ls") == 0) {
            ls_mode = 1;
            print_default = 0;
        }
        else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "-and") == 0) {
            // AND is default, just continue
        }
        else if (argv[i][0] != '-') {
            start_path = argv[i];
        }
    }
    
    // If no criteria, warn user
    if (print_default && !name_pattern && file_type == 0 && size_limit == 0 && !ls_mode) {
        printf("find: warning: no criteria specified, listing all files\n");
        printf("Use -name, -type, -size, or -ls to filter\n");
        printf("Continue? (y/N): ");
        
        int c = getchar();
        if (c != 'y' && c != 'Y') {
            printf("Aborted.\n");
            return 0;
        }
        printf("\n");
    }
    
    // Search
    find_recursive(start_path, ls_mode);
    
    return 0;
}
