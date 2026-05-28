#ifndef YMODEM_H
#define YMODEM_H

#include "common.h"

// YMODEM specific codes
#define YMODEM_SOH 0x01      // 128-byte block
#define YMODEM_STX 0x02      // 1024-byte block
#define YMODEM_EOT 0x04
#define YMODEM_ACK 0x06
#define YMODEM_NAK 0x15
#define YMODEM_CAN 0x18
#define YMODEM_C   0x43

#define YMODEM_TIMEOUT 5000
#define YMODEM_RETRIES 10
#define YMODEM_BLOCK_SIZE_128 128
#define YMODEM_BLOCK_SIZE_1K 1024

// YMODEM functions
void ymodem_send(void);
void ymodem_receive(void);

#endif