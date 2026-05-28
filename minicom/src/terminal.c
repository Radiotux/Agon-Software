// terminal.c - Funciones de terminal

#include <stdio.h>
#include <stdlib.h>
#include <agon/vdp.h>
#include <agon/mos.h>
#include "common.h"
#include "terminal.h"
#include "ui.h"

TerminalState term;
uint8_t uart_buffer[UART_BUFFER_SIZE];
volatile uint16_t uart_buffer_head = 0;
volatile uint16_t uart_buffer_tail = 0;

void work_putchar(char c) {
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
    putchar(c);
    term.cursor_x++;
    if (term.cursor_x > 78) {
        term.cursor_x = 1;
        term.cursor_y++;
        if (term.cursor_y > WORK_END_ROW) {
            term.cursor_y = WORK_END_ROW;
        }
    }
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}

void work_newline(void) {
    term.cursor_x = 1;
    term.cursor_y++;
    if (term.cursor_y > WORK_END_ROW) {
        term.cursor_y = WORK_END_ROW;
    }
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}

void uart_read_all(void) {
    int24_t c;
    uint16_t next_head;
    while ((c = mos_ugetc_nb()) != -1) {
        next_head = (uart_buffer_head + 1) % UART_BUFFER_SIZE;
        if (next_head != uart_buffer_tail) {
            uart_buffer[uart_buffer_head] = (uint8_t)c;
            uart_buffer_head = next_head;
        }
    }
}

void process_uart_buffer(void) {
    while (uart_buffer_tail != uart_buffer_head) {
        uint8_t c = uart_buffer[uart_buffer_tail];
        uart_buffer_tail = (uart_buffer_tail + 1) % UART_BUFFER_SIZE;

        if (!term.connected) {
            term.connected = 1;
            update_status_line();
        }
        if (term.capture_mode && term.log_file) fputc(c, term.log_file);

        if (c == 13) {
            term.cursor_x = 1;
            vdp_cursor_tab(term.cursor_x, term.cursor_y);
        } else if (c == 10) {
            term.cursor_x = 1;
            term.cursor_y++;
            if (term.cursor_y > WORK_END_ROW) {
                term.cursor_y = WORK_END_ROW;
                vdp_cursor_tab(1, WORK_END_ROW);
                for (int x = 1; x <= 78; x++) putchar(' ');
            }
            vdp_cursor_tab(term.cursor_x, term.cursor_y);
        } else if (c >= 32 && c < 127) {
            work_putchar(c);
        }
    }
    if (term.capture_mode && term.log_file) fflush(term.log_file);
}

void terminal_init(void) {
    term.local_echo = 1;
    term.connected = 0;
    term.xmodem_crc = 1;
    term.capture_mode = 0;
    term.log_file = NULL;
    term.cursor_x = 1;
    term.cursor_y = WORK_START_ROW;   // Primera línea del área de trabajo (línea 5)

    UART uartSettings;
    uartSettings.baudRate = BAUD_RATE;
    uartSettings.dataBits = 8;
    uartSettings.stopBits = 1;
    uartSettings.parity = 0;
    uartSettings.flowcontrol = 0;
    uartSettings.eir = 0;

    int result = mos_uopen(&uartSettings);
    term.connected = (result == 0);

    terminal_clear();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}

void terminal_clear(void) {
    vdp_cls();
}