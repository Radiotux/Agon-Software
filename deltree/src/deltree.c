/*
 * deltree.c - Remove files and directories recursively
 * For Agon Light / Agondev
 * 
 * Usage: deltree [options] path...
 * 
 * Options:
 *   -f, --force      Ignore nonexistent files, never prompt
 *   -i, --interactive Prompt before every removal
 *   -v, --verbose    Explain what is being done
 *   -h, --help       Show this help
 * 
 * WARNING: This command is destructive. Use with caution!
 * 
 * Examples:
 *   deltree tempdir
 *   deltree -f /path/to/dir
 *   deltree -i file.txt dir/
 *   deltree -v -f backup_old/
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <agon/mos.h>

#define VERSION "1.0"
#define MAX_PATH 256

// Options
int opt_force = 0;
int opt_interactive = 0;
int opt_verbose = 0;
int opt_help = 0;

// Statistics
int files_removed = 0;
int dirs_removed = 0;
int errors = 0;

/**
 * Display help
 */
void show_help(void) {
    printf("deltree v%s - Remove files and directories recursively\n", VERSION);
    printf("Usage: deltree [options] path...\n");
    printf("\nOptions:\n");
    printf("  -f, --force      Ignore nonexistent files, never prompt\n");
    printf("  -i, --interactive Prompt before every removal\n");
    printf("  -v, --verbose    Explain what is being done\n");
    printf("  -h, --help       Show this help\n");
    printf("\nWARNING: This command is destructive. Use with caution!\n");
    printf("\nExamples:\n");
    printf("  deltree tempdir\n");
    printf("  deltree -f /path/to/dir\n");
    printf("  deltree -i file.txt dir/\n");
    printf("  deltree -v -f backup_old/\n");
}

/**
 * Check if path is a directory
 */
int is_directory(const char *path) {
    FILINFO finfo;
    uint8_t result = ffs_stat(&finfo, path);
    return (result == 0 && (finfo.fattrib & AM_DIR));
}

/**
 * Check if path exists
 */
int path_exists(const char *path) {
    FILINFO finfo;
    return (ffs_stat(&finfo, path) == 0);
}

/**
 * Remove a file or directory using delete command
 */
int remove_item(const char *path) {
    char cmd[MAX_PATH + 20];
    
    if (opt_verbose) {
        printf("Removing: %s\n", path);
    }
    
    snprintf(cmd, sizeof(cmd), "delete %s", path);
    mos_oscli(cmd, NULL, 0);
    
    // Check if removed
    if (!path_exists(path)) {
        if (is_directory(path)) {
            dirs_removed++;
        } else {
            files_removed++;
        }
        return 1;
    } else {
        fprintf(stderr, "deltree: cannot remove '%s'\n", path);
        errors++;
        return 0;
    }
}

/**
 * Prompt user for confirmation
 */
int confirm(const char *path, int is_dir) {
    char response[10];
    
    if (opt_force) return 1;
    
    printf("remove %s '%s'? (y/n) ", is_dir ? "directory" : "file", path);
    fflush(stdout);
    
    if (fgets(response, sizeof(response), stdin) == NULL) return 0;
    
    return (response[0] == 'y' || response[0] == 'Y');
}

/**
 * Recursively remove directory contents
 */
int remove_directory(const char *path) {
    DIR dir;
    FILINFO finfo;
    char full_path[MAX_PATH];
    uint8_t result;
    int success = 1;
    int has_contents = 0;
    
    if (!is_directory(path)) {
        return remove_item(path);
    }
    
    // Open directory
    result = ffs_dopen(&dir, path);
    if (result != 0) {
        fprintf(stderr, "deltree: cannot open directory '%s'\n", path);
        errors++;
        return 0;
    }
    
    // Remove all contents
    while (1) {
        result = ffs_dread(&dir, &finfo);
        if (result != 0 || finfo.fname[0] == 0) break;
        
        // Skip "." and ".."
        if (strcmp(finfo.fname, ".") == 0 || strcmp(finfo.fname, "..") == 0) {
            continue;
        }
        
        has_contents = 1;
        
        // Build full path
        if (strlen(path) + strlen(finfo.fname) + 2 < MAX_PATH) {
            if (strcmp(path, ".") == 0) {
                snprintf(full_path, MAX_PATH, "%s", finfo.fname);
            } else {
                snprintf(full_path, MAX_PATH, "%s/%s", path, finfo.fname);
            }
        } else {
            continue;
        }
        
        // Recursively remove
        if (finfo.fattrib & AM_DIR) {
            if (opt_interactive && !confirm(full_path, 1)) {
                continue;
            }
            if (!remove_directory(full_path)) {
                success = 0;
            }
        } else {
            if (opt_interactive && !confirm(full_path, 0)) {
                continue;
            }
            if (!remove_item(full_path)) {
                success = 0;
            }
        }
    }
    
    ffs_dclose(&dir);
    
    // Remove the directory itself
    if (has_contents) {
        // Directory had contents, we already removed them
        // Now remove the empty directory
        if (opt_interactive && !confirm(path, 1)) {
            return success;
        }
        if (!remove_item(path)) {
            success = 0;
        }
    } else {
        // Empty directory, remove directly
        if (opt_interactive && !confirm(path, 1)) {
            return 1;
        }
        if (!remove_item(path)) {
            success = 0;
        }
    }
    
    return success;
}

/**
 * Process a single path
 */
int process_path(const char *path) {
    if (!path_exists(path)) {
        if (!opt_force) {
            fprintf(stderr, "deltree: cannot remove '%s': No such file or directory\n", path);
        }
        errors++;
        return 0;
    }
    
    if (opt_interactive && !confirm(path, is_directory(path))) {
        return 1;
    }
    
    if (is_directory(path)) {
        return remove_directory(path);
    } else {
        return remove_item(path);
    }
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    int i;
    int paths = 0;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--force") == 0) {
            opt_force = 1;
        }
        else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            opt_interactive = 1;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            opt_verbose = 1;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "deltree: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'deltree -h' for help\n");
            return 1;
        }
        else {
            paths++;
        }
    }
    
    // Need at least one path
    if (paths == 0) {
        fprintf(stderr, "deltree: missing operand\n");
        fprintf(stderr, "Try 'deltree -h' for help\n");
        return 1;
    }
    
    // Warning for safety
    if (!opt_force && !opt_interactive) {
        printf("WARNING: This will permanently delete files and directories.\n");
        printf("Continue? (y/n) ");
        fflush(stdout);
        
        char response[10];
        if (fgets(response, sizeof(response), stdin) == NULL) return 1;
        if (response[0] != 'y' && response[0] != 'Y') {
            printf("Aborted.\n");
            return 0;
        }
    }
    
    // Process each path
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // Skip options
            continue;
        }
        process_path(argv[i]);
    }
    
    // Summary
    if (opt_verbose) {
        printf("\nSummary: %d files removed, %d directories removed\n", 
               files_removed, dirs_removed);
        if (errors > 0) {
            printf("Errors: %d\n", errors);
        }
    }
    
    return errors ? 1 : 0;
}