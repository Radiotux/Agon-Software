====================================================================
ROUTE Utility for Agon Light / Agon Light 2
====================================================================

Description:
------------
ROUTE is a native command-line network utility for Agon MOS that
displays the current IP routing table of the system. It queries
the ESP-01S Wi-Fi module passively to read the current network
configuration and displays both the Default Gateway route (0.0.0.0)
and the directly connected local link subnet route.

Features:
---------
- 100% Non-blocking architecture using 'mos_ugetc_nb()'.
- Prevents CPU deadlocks or screen freezing when reading the UART.
- Strict chronological token parsing matching ESP8266 AT firmware.
- Disables command echoing (ATE0) dynamically to prevent buffer
  overflows or junk characters.
- Displays Unix-style routing table format with zero overhead.

Hardware Requirements:
----------------------
- Agon Light or Agon Light 2 microcomputer.
- ESP-01S (ESP8266) Wi-Fi module connected to the physical UART1 port.
- Standard Espressif AT Firmware running on the ESP-01S at 115200 bps
  without physical flow control (CTS/RTS).

Compilation & Installation:
---------------------------
Developed and tested under the official AgonDev CE/eZ80 toolchain.

1. Place 'route.c' inside your project's 'src/' folder.
2. Ensure your Makefile is configured to include '<agon/mos.h>'.
3. Open your Linux/Ubuntu terminal and run:

   make clean
   make

4. Copy the compiled 'route.bin' file to your Agon SD card
   (typically inside the /bin/ or any directory).

Usage:
------
From the Agon MOS command prompt, simply execute:

*route

Output Example:
---------------
Agon IP Routing Table (ESP-01S @ 115200 bps)
====================================================================
Destination        Gateway            Genmask            Interface
--------------------------------------------------------------------
0.0.0.0            192.168.0.1        255.255.255.0      wlan0
192.168.0.0        0.0.0.0            255.255.255.0      wlan0
====================================================================

Credits:
--------
Based on the asynchronous UART timing models and design patterns
pioneered by Radiotux's Agon-Software network utilities (ping/ifconfig).
