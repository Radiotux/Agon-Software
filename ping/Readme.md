========================================================================
             NATIVE PING UTILITY FOR AGON LIGHT 2 (eZ80)
========================================================================

Author: Radiotux/Rafd_Electrotux (Agon Software Project)
Environment: AgonDev SDK 
Target Hardware: Agon Light 2 & ESP8266 Standard AT Firmware

DESCRIPTION
-----------
This application introduces a robust, standalone 'ping' utility tailored 
specifically for the Agon Light 2 platform running the MOS operating system. 
It emulates the classic operating system look, feel, and telemetry output 
without relying on external script listeners or host-side echo daemons.

KEY FEATURES
------------
* Multi-Port Cascade Fallback (Patch Mode): 
  Since standard ICMP Layer 3 ping commands are handled differently by Wi-Fi 
  coprocessors, and Layer 4 UDP Echo (Port 7) is blocked by modern operating 
  systems, this utility implements an automated port scanner cascade. It tries 
  common open local network ports (80, 22, 443, 445, 139) to detect host activity.

* Strict Serial Token Parsing (Zero False Positives): 
  Unlike naive serial reading functions that trigger false positive metrics on 
  command finalization tags ("OK"), this utility features an parallelized 
  state machine that actively filters network failure tokens ("ERROR", "Fail") 
  from positive connectivity acknowledgments ("CONNECT", "CLOSED"). 

* Real-Time Hardware Timing Metrics: 
  By accessing the native MOS system variable pointers (sv + 0x00 clock ticks), 
  the program measures packet round-trip time (RTT) with millisecond precision, 
  rendering minimum, maximum, and average latency graphs.

* High Performance / Zero SD Contention:
  Disk-heavy telemetry logging operations have been completely removed from 
  the runtime loop, saving valuable CPU clock cycles on the 18.432MHz eZ80.

HOW TO BUILD
------------
1. Place the source file in your workspace under: `src/ping.c`
2. Open your terminal in the main directory.
3. Run the following clean and build execution routine:
   
   $ make clean
   $ make

4. Transfer the resulting compiled executable binary `ping.bin` into your 
   Agon Light 2 microSD card storage structure.

USAGE
-----
Invoke the command directly from the native MOS CLI shell using either local 
IP nodes or external web domain coordinates:

MOS> ping 192.168.0.1
MOS> ping ://google.com

COMPATIBILITY ROADMAP
---------------------
The current firmware release is fully compatible with standard factory ESP-01S 
layouts using basic unshielded links. 

To bridge this engine with the upcoming ESP-12S customized adapter variant, 
simply toggle the configuration flag inside the main structure function:
    
    settings.flowcontrol = 1; // Switches native MOS hardware RTS/CTS flow regulation ON

========================================================================
