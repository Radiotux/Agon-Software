========================================================================
             AGON LIGHT BITONAL ALARM CLOCK UTILITY
========================================================================

An autonomous, non-blocking bitonal alarm clock tool written in C for
the Agon Light 8-bit microcomputer using the AgonDev toolchain.

------------------------------------------------------------------------
1. REQUIREMENTS
------------------------------------------------------------------------
* Agon Light or compatible microcomputer (e.g., Olimex AgonLight2).
* System clock synchronized at boot. Supports both the native VDP (ESP32)
  internal clock or an external I2C RTC module (such as the DS3231 connected
  via the UEXT port) initialized in your !boot.obey script or autoexec.txt.
* Agon MOS 3.0 or later installed.

------------------------------------------------------------------------
2. COMPILATION INSTRUCTIONS
------------------------------------------------------------------------
To compile this project locally from your AgonDev environment, navigate
to the source directory on your workstation terminal and run:

  make clean
  make

The toolchain will build the final executable bin inside the local
binary directory path: 'bin/alarm.bin'.

------------------------------------------------------------------------
3. DEPLOYMENT AND USAGE
------------------------------------------------------------------------
1. Transfer the compiled 'alarm.bin' file to the '/bin/' folder
   inside your MicroSD card.
2. Execute the tool from the MOS prompt by specifying the hours and
   minutes separated strictly by SPACES (do NOT use colons):

   Usage:   alarm [hours] [minutes]
   Example: alarm 07 30

3. To view the embedded command line manual directly on your Agon:

   alarm -h

------------------------------------------------------------------------
4. HOW TO OPERATE
------------------------------------------------------------------------
* Once running, the utility will display the current RTC time on
  screen with absolute 1-second accuracy.
* As soon as the designated time arrives, it triggers an endless,
  intense bitonal "pi-po" audio sequence (880 Hz and 660 Hz).
* Press 'Q' on the physical keyboard to instantly kill the sound and
  exit back to the MOS terminal cleanly.

------------------------------------------------------------------------
5. LICENSE (OPEN SOURCE)
------------------------------------------------------------------------
This program is free and open-source software. You can redistribute it
and/or modify it under the terms of the MIT License:

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files, to deal
in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.
========================================================================
