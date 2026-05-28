/*
 * locate_db.h - Common definitions for locate/updatedb system
 * For Agon Light / Agondev
 * 
 * Author: Original implementation by Agon community
 * Version: 1.0
 */

#ifndef LOCATE_DB_H
#define LOCATE_DB_H

#include <stdint.h>

// Database structure
typedef struct {
    uint32_t magic;         // Magic number for format validation
    uint32_t version;       // Database format version
    uint32_t file_count;    // Number of indexed files
    uint32_t data_offset;   // Offset where entries begin
    uint32_t timestamp;     // Creation timestamp
    char     root_path[256]; // Root path that was indexed
} locate_db_header_t;

// Database entry structure
typedef struct {
    uint32_t path_offset;   // Offset to the full path string
    uint32_t size;          // File size in bytes
    uint16_t date;          // File date (FatFS format)
    uint16_t time;          // File time (FatFS format)
    uint8_t  attrib;        // File attributes (AM_DIR, AM_RDO, etc.)
    char     name[13];      // Short filename (8.3 format)
} locate_db_entry_t;

// Constants
#define LOCATE_DB_MAGIC     0x4C4F4341  // "LOCA" in ASCII
#define LOCATE_DB_VERSION   0x00010000  // Version 1.0
#define LOCATE_DB_PATH      "/locate.db" // Default database path

#endif /* LOCATE_DB_H */