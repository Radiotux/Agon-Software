# ntpsync – NTP Time Synchronizer for Agon Light

Synchronize your Agon Light's real‑time clock (RTC) using an ESP‑01S module with 
the **standard AT firmware** (not Zimodem). The program is silent (no screen output) 
and writes a detailed log to `/ntp.log` on the SD card.

## Features
- Uses **UDP** to query a public NTP server (`pool.ntp.org` by default).
- Supports a **UTC offset** (e.g., `-3` for Chile/Argentina/Uruguay winter time).
- **No screen output** – perfect for running from `autoexec.txt` or `!boot.obey`.
- **Robust communication** with the ESP‑01S (tested on firmware `AT+GMR`).
- **Handles leap years** correctly until the year 2099.

## Requirements
- Agon Light with MOS 1.04 or later.
- ESP‑01S module connected to **UART1** (RX/TX only).  
- WiFi network configured on the ESP‑01S (must be able to reach the internet).

## Installation
1. Compile the program:
   run make
2. Copy ntpsync.bin to your Agon’s SD card.
3. Usage: ntpsync [UTC_OFFSET]
ntpsync -h
UTC_OFFSET : offset in hours from UTC (e.g., -3, +2, 0).

-h or --help : show this help message and exit.

## Examples
ntpsync -3      # Set RTC to UTC-3 (Chile winter time)
ntpsync +2      # Set RTC to UTC+2 (South Africa)
ntpsync 0       # Set RTC to UTC

## Automatic startup

Add to your autoexec.txt or !boot.obey
ntpsync -3
