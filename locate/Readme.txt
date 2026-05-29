# File Search Tools for Agon Light

A complete file search system for the Agon Light platform, inspired by Unix `locate`/`updatedb`. Recursively indexes all files on your SD card and provides instant search capabilities.

## Features

- **updatedb** – Recursively scans the SD card and creates a searchable database
- **locate** – Fast file search using the pre-built database
- **Wildcard support**: `*` (any sequence), `?` (any single character), `#` (any digit)
- **Case-sensitive by default** with `-i` option for case-insensitive search
- **Low memory usage**: Streaming read/write, no practical file limit
- **Multiple output formats**: Full paths, filenames only, or count only
- **Progress indicator** during database creation and search

## Installation

1. Compile using Agondev:
   ```bash
   make all