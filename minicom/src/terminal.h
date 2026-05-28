#ifndef TERMINAL_H
#define TERMINAL_H

#include "common.h"

void terminal_init(void);
void terminal_clear(void);
void uart_read_all(void);
void process_uart_buffer(void);
void work_putchar(char c);
void work_newline(void);

#endif
