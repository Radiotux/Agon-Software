====================================================================
NETSTAT Utility for Agon Light / Agon Light 2
====================================================================

Description:
------------
NETSTAT is a native command-line network utility for Agon MOS that
lists all active internet connections (sockets) handled by the
ESP-01S Wi-Fi system. It passively displays socket identifiers,
protocols (TCP/UDP), remote IP addresses, and active remote ports.

Features:
---------
- 100% Non-blocking architecture using 'mos_ugetc_nb()'.
- Prevents system freezing when traversing asynchronous inputs.
- Safe chunk-buffering memory management for multi-line reports.
- Employs a robust token-matrix extractor safe for Clang eZ80 backend.

Usage:
------
From the Agon MOS prompt, execute:

*netstat

Output Example:
---------------
Agon Active Internet Connections (Sockets)
====================================================================
ID    Proto    Remote Address         Remote Port  State
--------------------------------------------------------------------
0     TCP      140.82.112.4           443          ESTABLISHED
====================================================================
