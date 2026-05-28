📦 locate-agon.zip – File Search Tools for Agon Light

A complete file search system for the Agon Light, inspired by Unix locate/updatedb.

✅ updatedb – Recursively scans your SD card and creates a searchable database
✅ locate – Fast file search using the pre-built database

Features:
• Wildcard support: *, ?, #
• Case-sensitive by default with -i for case-insensitive
• Multiple output formats: full paths, filenames only, or count only
• Low memory usage (~4 KB), no practical file limit (tested with 45,000+ files)
• Pre-compiled binaries included

Quick start:
  updatedb           # Create database (takes a few minutes)
  locate "*.bin"     # Find all .bin files
  locate -c "*.c"    # Count .c files

Includes:
  📁 bin/ – Pre-compiled .bin files (ready to use)
  📁 src/ – Full source code
  📄 README-usage.md – For users
  📄 README-build.md – For developers

Open source – free to use, modify, and share!