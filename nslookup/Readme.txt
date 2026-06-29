====================================================================
NSLOOKUP Utility for Agon Light / Agon Light 2
====================================================================

Description:
------------
NSLOOKUP is a native command-line network utility for Agon MOS that
queries Internet Domain Name System (DNS) servers to find the IP
address associated with any text-based hostname or domain argument.

Features:
---------
- 100% Non-blocking architecture utilizing 'mos_ugetc_nb()'.
- Dynamic command-line argument processing using argc/argv strings.
- 2500ms custom timeout guarding remote DNS server queries.
- Clean standard console tabular output reporting for the community.

Usage:
------
From the Agon MOS prompt, execute:

*nslookup <domain_name>

Example:
--------
*nslookup github.com

Output Example:
---------------
Agon DNS Resolution Utility
====================================================================
Server:         ESP-01S Gateway DNS
Host Query:     github.com
Address:        140.82.112.4
====================================================================
