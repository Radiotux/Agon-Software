
================================================================================
          AGON GET v2.2 - Official Factory ESP-AT Wireless Downloader
================================================================================
Authors: @rafd_electrotux (Agon Community colaborator)
License: GNU General Public License v3.0 (GPL-3.0)
Target: Agon Light / Agon Light 2 Retrocomputers running MOS & AgonDev ADL

1. OVERVIEW & SOFTWARE ARCHITECTURE
--------------------------------------------------------------------------------
AGON GET v2.2 is a bit-perfect, network-buffered wireless file management utility
designed to transfer files of ANY size from a modern PC directly to the Agon Light
microSD Card.

Due to the physical absence of hardware flow control lines (RTS/CTS) on stock
ESP-01/S modules, traditional real-time handshaking protocols fail during heavy
SD Card writing cycles. AGON GET solves this limitation by introducing a unique
Server-Side Block Sliding Window mechanism.

The software splits large files into safe 32 KB page segments. The Agon client
ingests each block entirely into RAM at hardware limit, shuts down the socket
session to isolate serial noise, performs offline clean sector writing via MOS,
and automatically re-opens the channel to request the next block sequentially.

2. TECHNICAL SPECIFICATIONS & PERFORMANCE
--------------------------------------------------------------------------------
* Baud Rate: 115200 bps (Standard Stock UART Profile).
* Operational Net Speed: ~1.1 KB/s to ~2.0 KB/s net throughput.
* Memory Footprint: Max 65,536 bytes static allocation (Zero Stack Overflow).
* Paging Window Size: 32,768 bytes per network block burst.
* Error Mitigation: Native Finite State Machine (FSM) linear metadata stripper
  immune to binary null bytes (0x00) and line drop protection.
* UI Capability: Auto-formatting 2-column interactive catalog supporting
  up to 50 local storage files simultaneously.

3. PREREQUISITES & FIRMWARE REQUIREMENTS
--------------------------------------------------------------------------------
* Hardware Platform: Agon Light or Agon Light 2 computer.
* Onboard Module: Standard ESP-01/S (ESP8266 or ESP32 variant).
* Wi-Fi Firmware: Factory Stock Espressif ESP-AT v1.7.4.0 (SDK v3.0.5 or recent).
* Compiling Toolchain: AgonDev (Recent edition compiled under Clang ADL Mode).

4. HOW TO USE THE SYSTEM
--------------------------------------------------------------------------------
STEP 1: HOST PC PREPARATION
  Place 'agon_server.py' in the folder containing the games/files you wish to share.
  Run the script from your terminal:
     python3 agon_server.py
  The script will parse your files, auto-update 'lista.txt' and print its Local IP.

STEP 2: AGON CLIENT COMPILATION
  Compile 'get.c' using your AgonDev environment layout:
     make clean && make
  Copy the compiled binary 'get.bin' over your Agon microSD Card.

STEP 3: NETWORK INITIATION
  Turn on your Agon Light, type the terminal launch string using your server IP:
     get 192.168.0.X
  (If utilizing an alternate custom port use syntax format: 'get 192.168.0.X:2000')

STEP 4: INTERACTIVE SELECTION
  The system layout will draw a catalog table. Punch your desired file index row
  number (e.g., 16 for escape_mansion.bin) and hit ENTER. The sleek inline single-
  row progress ticker will monitor byte sector writing until reaching a bit-perfect
  [OK] file system flush state.

5. LICENSING & OPEN-SOURCE TERMS (GNU GPL v3)
--------------------------------------------------------------------------------
This program is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for full structural detail.
================================================================================
