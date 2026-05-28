/*
 * updatedb.c - Create/update file database for fast searching
 * For Agon Light / Agondev
 * 
 * Author: Original implementation by Agon community
 * Version: 1.0
 * 
 * Compilation: make updatedb
 * Usage: updatedb [options] [root_path]
 * 
 * Description:
 *   Recursively scans the SD card filesystem and creates a database
 *   of all files found. The database is used by the 'locate' command
 *   for fast file searching.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mos.h>
#include "locate_db.h"

// File attribute definitions (from FatFS)
#define AM_RDO   0x01    // Read only
#define AM_HID   0x02    // Hidden
#define AM_SYS   0x04    // System
#define AM_DIR   0x10    // Directory
#define AM_ARC   0x20    // Archive

#define MAX_PATH 256
#define BUFFER_SIZE 4096  // Write buffer size

// Database file handle
FILE *db_file = NULL;
uint32_t current_offset = 0;
uint32_t file_count = 0;

// Temporary buffer for entries before writing
typedef struct {
    char path[MAX_PATH];
    uint32_t size;
    uint16_t date;
    uint16_t time;
    uint8_t attrib;
} temp_entry_t;

temp_entry_t *buffer = NULL;
int buffer_count = 0;
int buffer_capacity = 0;

/**
 * Flush buffer to disk
 */
int flush_buffer(void) {
    int i;
    locate_db_entry_t entry;
    
    if (buffer_count == 0) return 1;
    
    for (i = 0; i < buffer_count; i++) {
        entry.path_offset = current_offset;
        entry.size = buffer[i].size;
        entry.date = buffer[i].date;
        entry.time = buffer[i].time;
        entry.attrib = buffer[i].attrib;
        
        // Extract short name (last component of path)
        char *last_slash = strrchr(buffer[i].path, '/');
        if (last_slash) {
            strncpy(entry.name, last_slash + 1, 12);
            entry.name[12] = '\0';
        } else {
            strncpy(entry.name, buffer[i].path, 12);
            entry.name[12] = '\0';
        }
        
        // Write entry
        if (fwrite(&entry, sizeof(entry), 1, db_file) != 1) {
            return 0;
        }
        
        // Write full path string
        uint32_t path_len = strlen(buffer[i].path) + 1;
        if (fwrite(buffer[i].path, path_len, 1, db_file) != 1) {
            return 0;
        }
        
        current_offset += path_len;
        file_count++;
    }
    
    buffer_count = 0;
    return 1;
}

/**
 * Add an entry to buffer
 */
int add_entry(const char *path, uint32_t size, uint16_t date, uint16_t time, uint8_t attrib) {
    // Expand buffer if needed
    if (buffer_count >= buffer_capacity) {
        if (!flush_buffer()) {
            return 0;
        }
        
        int new_capacity = buffer_capacity == 0 ? 100 : buffer_capacity * 2;
        temp_entry_t *new_buffer = realloc(buffer, new_capacity * sizeof(temp_entry_t));
        if (!new_buffer) return 0;
        buffer = new_buffer;
        buffer_capacity = new_capacity;
    }
    
    // Copy to buffer
    strncpy(buffer[buffer_count].path, path, MAX_PATH - 1);
    buffer[buffer_count].path[MAX_PATH - 1] = '\0';
    buffer[buffer_count].size = size;
    buffer[buffer_count].date = date;
    buffer[buffer_count].time = time;
    buffer[buffer_count].attrib = attrib;
    
    buffer_count++;
    return 1;
}

/**
 * Recursively scan a directory
 */
int scan_directory(const char *path) {
    DIR dir;
    FILINFO finfo;
    char full_path[MAX_PATH];
    uint8_t result;
    static int processed = 0;
    
    memset(&finfo, 0, sizeof(finfo));
    
    result = ffs_dopen(&dir, path);
    if (result != 0) return 0;
    
    while (1) {
        result = ffs_dread(&dir, &finfo);
        if (result != 0 || finfo.fname[0] == 0) break;
        
        // Skip "." and ".."
        if (strcmp(finfo.fname, ".") == 0 || strcmp(finfo.fname, "..") == 0) {
            continue;
        }
        
        // Build full path
        if (strlen(path) + strlen(finfo.fname) + 2 < MAX_PATH) {
            if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
                snprintf(full_path, MAX_PATH, "%s", finfo.fname);
            } else {
                snprintf(full_path, MAX_PATH, "%s/%s", path, finfo.fname);
            }
        } else {
            continue;
        }
        
        if (!add_entry(full_path, finfo.fsize, finfo.fdate, finfo.ftime, finfo.fattrib)) {
            break;
        }
        
        processed++;
        if (processed % 100 == 0) {
            printf("Processed: %d files (DB: %lu KB)\r", 
                   processed, (unsigned long)(current_offset / 1024));
            fflush(stdout);
        }
        
        // Recursively scan subdirectories
        if (finfo.fattrib & AM_DIR) {
            scan_directory(full_path);
        }
    }
    
    ffs_dclose(&dir);
    return file_count;
}

/**
 * Initialize database file
 */
int start_database(const char *db_path, const char *root_path) {
    locate_db_header_t header;
    
    db_file = fopen(db_path, "wb");
    if (!db_file) return 0;
    
    // Write temporary header (will be updated at end)
    header.magic = LOCATE_DB_MAGIC;
    header.version = LOCATE_DB_VERSION;
    header.file_count = 0;
    header.data_offset = sizeof(locate_db_header_t);
    header.timestamp = 0;
    strncpy(header.root_path, root_path, 255);
    header.root_path[255] = '\0';
    
    if (fwrite(&header, sizeof(header), 1, db_file) != 1) {
        fclose(db_file);
        return 0;
    }
    
    current_offset = header.data_offset;
    return 1;
}

/**
 * Finalize database (update header)
 */
int finish_database(void) {
    locate_db_header_t header;
    
    if (!flush_buffer()) return 0;
    
    // Update header with final values
    fseek(db_file, 0, SEEK_SET);
    header.magic = LOCATE_DB_MAGIC;
    header.version = LOCATE_DB_VERSION;
    header.file_count = file_count;
    header.data_offset = sizeof(locate_db_header_t);
    header.timestamp = 0;
    
    if (fwrite(&header, sizeof(header), 1, db_file) != 1) {
        fclose(db_file);
        return 0;
    }
    
    fclose(db_file);
    return 1;
}

/**
 * Clean up resources
 */
void cleanup(void) {
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }
}

/**
 * Display help
 */
void show_help(void) {
    printf("Usage: updatedb [options] [root_path]\n");
    printf("\nOptions:\n");
    printf("  -h, --help      Show this help message\n");
    printf("  -v, --verbose   Verbose output\n");
    printf("\nDescription:\n");
    printf("  Recursively scans the filesystem and creates a database\n");
    printf("  at %s for fast searching with 'locate'\n", LOCATE_DB_PATH);
    printf("\nNote: No file limit (streaming write to disk)\n");
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    char *root_path = ".";
    int verbose = 0;
    int i;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                show_help();
                return 0;
            }
            else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
                verbose = 1;
            }
        } else {
            root_path = argv[i];
        }
    }
    
    printf("=== updatedb for Agon Light ===\n");
    printf("Indexing from: %s\n", root_path);
    printf("Mode: Streaming write (no file limit)\n\n");
    
    if (!start_database(LOCATE_DB_PATH, root_path)) {
        printf("Error: Could not create database\n");
        return 1;
    }
    
    printf("Scanning...\n");
    scan_directory(root_path);
    
    if (!finish_database()) {
        printf("\nError: Could not finalize database\n");
        cleanup();
        return 1;
    }
    
    printf("\n\nDatabase created: %lu files indexed\n", (unsigned long)file_count);
    printf("Size: %lu bytes (%.2f KB)\n", 
           (unsigned long)current_offset, 
           (float)current_offset / 1024.0);
    
    cleanup();
    return 0;
}