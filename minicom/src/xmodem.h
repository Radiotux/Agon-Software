#ifndef XMODEM_H
#define XMODEM_H

#include "common.h"

// XMODEM funciones
void xmodem_send(void);
void xmodem_receive(void);
uint8_t xmodem_calc_checksum(uint8_t *data, int len);
uint16_t xmodem_calc_crc16(uint8_t *data, int len);
int xmodem_get_char(uint32_t timeout_ms);
void xmodem_flush(void);
int read_filename(char *buffer, int max_len, const char *prompt);

#endif // XMODEM_H
