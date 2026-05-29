# BAT - Smart Code Viewer for Agon Light

`BAT` is a modern, lightweight, high-performance terminal text viewer tailored for the **Agon Light** 8-bit retrocomputer (eZ80 CPU running MOS). Heavily inspired by the modern Linux utility `bat`, this software features built-in sequential file streaming, dynamic syntax highlighting, context-aware layout formatting, and zero-flicker hardware pagination.

---

## 🌟 Key Features

*   **Multiformat Syntax Highlighting**: Smart parsing engine supports **C Source**, **Z80/eZ80 Assembly listings**, and **BBC BASIC** code syntax natively.
*   **Context-Aware File Parsing**: Automatically detects formats based on filename extensions (`.bas`, `.bbc`) or dynamic evaluation of internal layout footprints (such as parsing `tpasm` listing matrices).
*   **Pure Hardware Control Palette**: Direct communication via native **BBC BASIC VDU 17** system byte vectors to drive video chip (VDP) operations on retro platforms without overhead.
*   **Optimized Resource Allocation**: Single-pass data streaming requires fixed RAM constraints (approx. 512 bytes), allowing safe and fluid execution when scanning large documents on a microcomputer.
*   **Clean Retro Pagination**: Freezes line rendering accurately at standard 30-line increments in silent mode. Hit `ENTER` to scroll down cleanly or tap `Q` to instantly abort and return safely to the MOS operating system prompt.

---

## 🛠️ Requirements & Setup

To compile and pack the program, you will need the standard cross-compiler toolchain for the eZ80 architecture:
*   **[agondev toolchain](https://github.com)** (which features the `ez80-none-elf-clang` tool wrapper and matching linking runtimes).

---

## ⚙️ Compilation & Installation

### 1. Build the Binary
Navigate to your project folder inside your development workstation (Linux/WSL environment) and issue the build environment targets via `make`:

```bash
make clean
make
```

Upon successful target tracking, the enlazador will yield a packed static architecture binary under `bin/bat.bin`.

### 2. Deployment onto Agon Light
1. Unmount your Agon Light's microSD card and attach it to your desktop workstation.
2. Copy the newly compiled executable file `bat.bin` directly into the system commands folder of your SD card (typically located in the `/bin/` path of your storage hierarchy).
3. Safely eject the card, reinsert it into your hardware, and boot your machine.

### 3. Usage Syntax
Invoke the command dynamically from your global MOS environment:

```bash
bat main.c
bat system.bas
bat asembler.lst
bat report.asm
```

---

## 📜 Open Source Software Statement

This application is **Free and Open Source Software (FOSS)**. It has been developed out of passion for the retro-computing ecosystem and is shared freely to help expand native development utilities for the Agon platform. 

You are free to use, study, modify, adapt, or redistribute the source code for personal, academic, or professional projects under the conditions of the open computing guidelines.
