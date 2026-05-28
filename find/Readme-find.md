# find - File Search Tool for Agon Light

A powerful file search utility for the Agon Light, inspired by the Unix `find` command. Recursively searches directory trees for files matching specified criteria.

## Installation

1. Copy `find.bin` to your SD card
2. Run directly: `find`

## Quick Start

```bash
# Find all .bin files in current directory and subdirectories
find . -name "*.bin"

# Find all directories
find . -type d

# Find files larger than 10KB
find . -size +10k

# Case-insensitive search for readme files
find . -iname "readme*"

# List files with details (like ls -l)
find . -name "*.bin" -ls