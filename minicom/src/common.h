#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>

// ========== CONFIGURACIÓN ==========
#define BAUD_RATE 115200
#define WORK_START_ROW 5
#define WORK_END_ROW 28        // Área de trabajo: líneas 5 a 28 (24 líneas)
#define UART_BUFFER_SIZE 65536   // 64KB buffer

// XMODEM Codes
#define XMODEM_SOH 0x01
#define XMODEM_EOT 0x04
#define XMODEM_ACK 0x06
#define XMODEM_NAK 0x15
#define XMODEM_CAN 0x18
#define XMODEM_C   0x43

#define XMODEM_TIMEOUT 3000
#define XMODEM_RETRIES 10
#define XMODEM_BLOCK_SIZE 128

// Ctrl Codes
#define CTRL_B 0x02
#define CTRL_C 0x03
#define CTRL_D 0x04
#define CTRL_E 0x05
#define CTRL_F 0x06
#define CTRL_H 0x08
#define CTRL_L 0x0C
#define CTRL_Q 0x11
#define CTRL_U 0x15   // Ctrl+U - YMODEM Receive
#define CTRL_Y 0x19   // Ctrl+Y - YMODEM Send
#define CTRL_Z 0x1A   // Ctrl+Z - ZMODEM (placeholder)

// ========== ESTRUCTURAS ==========
typedef struct {
    uint8_t local_echo;
    uint8_t connected;
    uint8_t xmodem_crc;
    uint8_t capture_mode;
    FILE* log_file;
    uint8_t cursor_x;
    uint8_t cursor_y;
} TerminalState;

// Variables globales
extern TerminalState term;
extern uint8_t xmodem_buffer[];
extern uint8_t uart_buffer[];
extern volatile uint16_t uart_buffer_head;
extern volatile uint16_t uart_buffer_tail;

#endif