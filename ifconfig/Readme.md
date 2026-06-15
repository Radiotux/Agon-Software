========================================================================
            NATIVE IFCONFIG UTILITY FOR AGON LIGHT 2 (eZ80)
========================================================================

Author: Radiotux/Rafd_electrotux (Agon Software Project)
Environment: AgonDev SDK
Target Hardware: Agon Light 2 & ESP8266 Standard AT Firmware

DESCRIPTION
-----------
This tool implements a standalone 'ifconfig' command for the Agon Light 2. 
It queries the Wi-Fi network coprocessor in real-time, parsing deep network 
parameters and rendering them into the native MOS CLI shell, matching the 
exact look and feel of a Linux wireless terminal interface ('wlo1').

KEY FEATURES
------------
* Balanced Stream Extraction Filter:
  Instead of dumping messy raw AT output blocks onto the terminal, the utility 
  implements selective stream extraction filters. It isolates network value 
  strings directly from quotes, collecting IP, Subnet Mask, Gateway, and 
  Hardware MAC fields smoothly.

* Dynamic Broadcast Resolution:
  Calculates the subnet broadcast string coordinates dynamically based on the 
  active IP interface response.

* VDP-Safe Sychronization Timings:
  Features balanced micro-pauses within the serial polling loops to prevent 
  CPU-to-VDP buffer flooding, ensuring rock-solid operating system stability 
  and preventing unexpected hardware resets during data bursts.

* Native OS Framework Integration:
  Relies strictly on MOS API abstraction structures, providing safe 24-bit 
  pointer alignments and zero warnings under strict Clang compiler checks.

HOW TO BUILD & EXECUTE
----------------------
1. Save the file inside your project structure as `src/ifconfig.c`
2. Compile clean using the master toolchain:
   
   $ make clean
   $ make

3. Load `ifconfig.bin` onto your Agon SD card and test it straight from CLI:

MOS> ifconfig

COMPATIBILITY ROADMAP
---------------------
The current firmware release is fully compatible with standard factory ESP-01S 
layouts using basic unshielded links. 

To bridge this engine with the upcoming ESP-12F customized adapter variant, 
simply toggle the configuration flag inside the main structure function:
    
    settings.flowcontrol = 1; // Switches native MOS hardware RTS/CTS flow regulation ON

========================================================================
