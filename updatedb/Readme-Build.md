# Building locate and updatedb for Agon Light

This document explains how to compile the `locate` and `updatedb` tools from source using the Agondev toolchain.

## Prerequisites

- **Agondev** toolchain installed and configured
- **Make** build system

## Source Files

### `include/locate_db.h`
Common definitions used by both programs:
- Database header structure (`locate_db_header_t`)
- Database entry structure (`locate_db_entry_t`)
- Constants: `LOCATE_DB_MAGIC`, `LOCATE_DB_VERSION`, `LOCATE_DB_PATH`

### `src/updatedb.c`
Database creator that recursively scans the filesystem:
- Uses MOS FatFS functions (`ffs_dopen`, `ffs_dread`, `ffs_dclose`)
- Streams data directly to disk (no memory limit)
- Progress indicator every 100 files
- Saves database to `/locate.db`

### `src/locate.c`
Search tool that queries the database:
- Scans database for strings that look like filenames
- Pattern matching with wildcards (`*`, `?`, `#`)
- Options for case-sensitivity, output format, and counting
- Streaming read – minimal memory usage

## Compilation

### 1. Set up the directory structure