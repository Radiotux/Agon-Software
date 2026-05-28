/*
 * touch.c - Create empty file or update timestamp
 * For Agon Light / Agondev
 * 
 * Usage: touch [options] file1 [file2 ...]
 * 
 * Options:
 *   -c, --no-create    Do not create file if it doesn't exist
 *   -h, --help         Show this help
 * 
 * Examples:
 *   touch file.txt                     # Create empty file.txt
 *   touch docs/readme.txt config.ini   # Create multiple files
 *   touch -c file1.txt file2.txt       # Only update if exists
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <agon/mos.h>

#define VERSION "1.0"

// Options
int opt_no_create = 0;
int opt_help = 0;
int error_count = 0;

/**
 * Display help
 */
void show_help(void) {
    printf("touch v%s - Create empty file or update timestamp\n", VERSION);
    printf("Usage: touch [options] file1 [file2 ...]\n");
    printf("\nOptions:\n");
    printf("  -c, --no-create    Do not create file if it doesn't exist\n");
    printf("  -h, --help         Show this help\n");
    printf("\nExamples:\n");
    printf("  touch file.txt              # Create empty file.txt\n");
    printf("  touch docs/readme.txt config.ini  # Create multiple files\n");
    printf("  touch -c existing.txt       # Update timestamp (if exists)\n");
    printf("\nNotes:\n");
    printf("  - Creates empty files (0 bytes)\n");
    printf("  - If file exists, does not modify content\n");
    printf("  - Creates parent directories automatically\n");
    printf("  - Multiple files supported (processed in order)\n");
}

/**
 * Create parent directories if needed
 */
int create_parent_dirs(const char *path) {
    char dir_path[256];
    char *last_slash;
    
    strcpy(dir_path, path);
    last_slash = strrchr(dir_path, '/');
    
    if (!last_slash) return 1;  // No directory path
    
    *last_slash = '\0';
    
    // Check if directory exists
    FILINFO finfo;
    uint8_t result = ffs_stat(&finfo, dir_path);
    
    if (result != 0 || !(finfo.fattrib & AM_DIR)) {
        // Create directory
        char cmd[300];
        snprintf(cmd, sizeof(cmd), "mkdir %s", dir_path);
        mos_oscli(cmd, NULL, 0);
    }
    
    return 1;
}

/**
 * Create/update a single file
 */
int touch_one_file(const char *filename) {
    FILE *fp;
    
    // Check if file exists
    fp = fopen(filename, "rb");
    if (fp) {
        // File exists - close it
        fclose(fp);
        
        if (opt_no_create) {
            return 1;  // Silent success
        }
        
        // File exists, we're done
        return 1;
    }
    
    // File doesn't exist
    if (opt_no_create) {
        return 1;  // Silent success (no creation)
    }
    
    // Create parent directories if needed
    create_parent_dirs(filename);
    
    // Create empty file
    fp = fopen(filename, "wb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    
    return 0;
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    int i;
    int file_count = 0;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--no-create") == 0) {
            opt_no_create = 1;
        }
        else if (argv[i][0] == '-') {
            printf("touch: unknown option '%s'\n", argv[i]);
            printf("Try 'touch -h' for help\n");
            return 1;
        }
        else {
            // It's a filename
            if (!touch_one_file(argv[i])) {
                printf("touch: cannot create '%s'\n", argv[i]);
                error_count++;
            }
            file_count++;
        }
    }
    
    // Need at least one file
    if (file_count == 0) {
        printf("touch: missing file operand\n");
        printf("Try 'touch -h' for help\n");
        return 1;
    }
    
    return error_count ? 1 : 0;
}