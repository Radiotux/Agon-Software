====================================================================
ARP Utility for Agon Light / Agon Light 2
====================================================================

Description:
------------
ARP is a native command-line network utility for Agon MOS that
displays the hardware (MAC) address mappings detected by the system.
It queries the ESP-01S module via UART1 to retrieve the local station's
physical address and maps the current Gateway IP connection layout.

Features:
---------
- 100% Non-blocking architecture utilizing 'mos_ugetc_nb()'.
- Prevents CPU freezing during asynchronous serial operations.
- Displays standard Unix-style ARP table format columns.
- Safely targets the ESP-01S firmware running at 115200 bps.

Usage:
------
From the Agon MOS prompt, execute:

*arp

Output Example:
---------------
Agon ARP Cache Table
====================================================================
Address            HWaddress            HWtype       Interface
--------------------------------------------------------------------
192.168.0.1        <dynamic>            ether        wlan0
static-if          48:55:19:01:00:ef    ether        wlan0
====================================================================
