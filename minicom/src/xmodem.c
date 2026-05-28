// xmodem.c - Transferencia de archivos XMODEM
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <agon/vdp.h>
#include <agon/mos.h>

#include "common.h"
#include "xmodem.h"
#include "ui.h"
#include "terminal.h"

uint8_t xmodem_buffer[1024];

uint8_t xmodem_calc_checksum(uint8_t *data, int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) sum += data[i];
    return sum;
}

uint16_t xmodem_calc_crc16(uint8_t *data, int len) {
    uint16_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

int xmodem_get_char(uint32_t timeout_ms) {
    for (int i = 0; i < timeout_ms * 10; i++) {
        int24_t c = mos_ugetc_nb();
        if (c != -1) return c;
        for (volatile int j = 0; j < 50; j++);
    }
    return -1;
}

void xmodem_flush(void) {
    while (mos_ugetc_nb() != -1);
}

int read_filename(char *buffer, int max_len, const char *prompt) {
    int i = 0;
    uint8_t key;
    int start_col;

    uint8_t old_echo = term.local_echo;
    uint8_t old_capture = term.capture_mode;

    term.local_echo = 0;
    term.capture_mode = 0;

    clear_message_area();

    vdp_cursor_tab(2, 10);
    printf("%s: ", prompt);
    start_col = 2 + strlen(prompt) + 2;
    vdp_cursor_tab(start_col, 10);

    while (i < max_len - 1) {
        key = vdp_getKeyCode();
        if (key != 0) {
            if (key == 13) break;
            if (key == 8 && i > 0) {
                i--;
                vdp_cursor_tab(start_col + i, 10);
                printf(" ");
                vdp_cursor_tab(start_col + i, 10);
            } else if (key >= 32 && key < 127) {
                buffer[i++] = key;
                printf("%c", key);
            }
            while (vdp_getKeyCode() != 0);
        }
    }

    buffer[i] = '\0';
    term.local_echo = old_echo;
    term.capture_mode = old_capture;
    clear_message_area();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);

    return i;
}

void xmodem_send(void) {
    char filename[64] = {0};
    FILE *fp;
    uint8_t block_num = 1;
    int bytes_read;
    int retries;
    int result = 0;
    int c;
    long file_size;
    int total_blocks;
    int percent;

    if (read_filename(filename, 60, "Send file - Name") == 0) return;

    fp = fopen(filename, "rb");
    if (!fp) {
        clear_message_area();
        vdp_cursor_tab(2, 12);
        printf("Error: Cannot open '%s'", filename);
        vdp_cursor_tab(2, 14);
        printf("Press any key...");
        while(!vdp_getKeyCode());
        clear_message_area();
        draw_frame();
        update_status_line();
        vdp_cursor_tab(term.cursor_x, term.cursor_y);
        return;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    total_blocks = (file_size + XMODEM_BLOCK_SIZE - 1) / XMODEM_BLOCK_SIZE;

    clear_message_area();
    vdp_cursor_tab(2, 12);
    printf("Sending: %s", filename);
    vdp_cursor_tab(2, 13);
    printf("Size: %ld bytes (%d blocks)", file_size, total_blocks);
    vdp_cursor_tab(2, 14);
    printf("Waiting for receiver...");

    xmodem_flush();
    c = xmodem_get_char(10000);

    if (c == XMODEM_NAK) {
        term.xmodem_crc = 0;
        vdp_cursor_tab(2, 15);
        printf("Using Checksum mode");
    } else if (c == XMODEM_C) {
        term.xmodem_crc = 1;
        vdp_cursor_tab(2, 15);
        printf("Using CRC mode");
    } else {
        vdp_cursor_tab(2, 15);
        printf("Error: No response from receiver");
        fclose(fp);
        vdp_cursor_tab(2, 17);
        printf("Press any key...");
        while(!vdp_getKeyCode());
        clear_message_area();
        draw_frame();
        update_status_line();
        vdp_cursor_tab(term.cursor_x, term.cursor_y);
        return;
    }

    while (!feof(fp) && block_num <= total_blocks) {
        bytes_read = fread(xmodem_buffer, 1, XMODEM_BLOCK_SIZE, fp);
        if (bytes_read == 0) break;

        for (int j = bytes_read; j < XMODEM_BLOCK_SIZE; j++) {
            xmodem_buffer[j] = 0x1A;
        }

        retries = 0;
        int block_ok = 0;

        while (retries < XMODEM_RETRIES && !block_ok) {
            mos_uputc(XMODEM_SOH);
            mos_uputc(block_num);
            mos_uputc(255 - block_num);

            for (int j = 0; j < XMODEM_BLOCK_SIZE; j++) {
                mos_uputc(xmodem_buffer[j]);
            }

            if (term.xmodem_crc) {
                uint16_t crc = xmodem_calc_crc16(xmodem_buffer, XMODEM_BLOCK_SIZE);
                mos_uputc(crc >> 8);
                mos_uputc(crc & 0xFF);
            } else {
                mos_uputc(xmodem_calc_checksum(xmodem_buffer, XMODEM_BLOCK_SIZE));
            }

            c = xmodem_get_char(3000);
            if (c == XMODEM_ACK) {
                block_ok = 1;
                break;
            }
            retries++;
        }

        if (!block_ok) {
            mos_uputc(XMODEM_CAN);
            mos_uputc(XMODEM_CAN);
            result = -1;
            break;
        }

        percent = (block_num * 100) / total_blocks;
        vdp_cursor_tab(2, 16);
        printf("Progress: %3d%% [%3d/%d blocks]", percent, block_num, total_blocks);

        block_num++;
    }

    if (result == 0) {
        retries = 0;
        while (retries < XMODEM_RETRIES) {
            mos_uputc(XMODEM_EOT);
            c = xmodem_get_char(3000);
            if (c == XMODEM_ACK) {
                vdp_cursor_tab(2, 17);
                printf("✅ Transfer complete!");
                break;
            }
            retries++;
        }
    }

    fclose(fp);

    if (result != 0) {
        vdp_cursor_tab(2, 17);
        printf("❌ Transfer error");
    }

    vdp_cursor_tab(2, 19);
    printf("Press any key to continue...");
    while(!vdp_getKeyCode());
    clear_message_area();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}

void xmodem_receive(void) {
    char filename[64] = {0};
    FILE *fp;
    uint8_t expected_block = 1;
    int c;
    int retries;
    int result = 0;
    int total_blocks = 0;
    long file_size = 0;

    if (read_filename(filename, 60, "Receive file - Name") == 0) return;

    fp = fopen(filename, "wb");
    if (!fp) {
        clear_message_area();
        vdp_cursor_tab(2, 12);
        printf("Error: Cannot create '%s'", filename);
        vdp_cursor_tab(2, 14);
        printf("Press any key...");
        while(!vdp_getKeyCode());
        clear_message_area();
        draw_frame();
        update_status_line();
        vdp_cursor_tab(term.cursor_x, term.cursor_y);
        return;
    }

    clear_message_area();
    vdp_cursor_tab(2, 11);
    printf("File: '%s'", filename);
    vdp_cursor_tab(2, 12);
    printf("Receiving: %s", filename);
    vdp_cursor_tab(2, 13);
    printf("Mode: %s", term.xmodem_crc ? "CRC" : "Checksum");

    xmodem_flush();

    retries = 0;
    while (retries < XMODEM_RETRIES) {
        vdp_cursor_tab(2, 14);
        printf("Waiting for sender... Attempt %d/%d", retries + 1, XMODEM_RETRIES);

        if (term.xmodem_crc) mos_uputc(XMODEM_C);
        else mos_uputc(XMODEM_NAK);

        c = xmodem_get_char(3000);
        if (c == XMODEM_SOH) break;
        if (c == XMODEM_CAN) {
            result = -1;
            break;
        }
        retries++;
    }

    while (c == XMODEM_SOH && result == 0) {
        uint8_t block_num, block_comp;
        uint8_t data[XMODEM_BLOCK_SIZE];
        uint8_t checksum;
        uint16_t crc_received, crc_calc;
        int block_ok = 0;

        retries = 0;
        while (retries < XMODEM_RETRIES && !block_ok) {
            block_num = xmodem_get_char(1000);
            block_comp = xmodem_get_char(1000);

            if (block_num != expected_block) {
                vdp_cursor_tab(2, 15);
                printf("Sequence error: expected %d, got %d", expected_block, block_num);
                mos_uputc(XMODEM_CAN);
                mos_uputc(XMODEM_CAN);
                result = -1;
                break;
            }

            for (int j = 0; j < XMODEM_BLOCK_SIZE; j++) {
                data[j] = xmodem_get_char(1000);
            }

            if (term.xmodem_crc) {
                crc_received = (xmodem_get_char(1000) << 8) | xmodem_get_char(1000);
                crc_calc = xmodem_calc_crc16(data, XMODEM_BLOCK_SIZE);
                if (crc_received == crc_calc) {
                    block_ok = 1;
                } else {
                    vdp_cursor_tab(2, 15);
                    printf("CRC error block %d, retry %d", expected_block, retries + 1);
                    mos_uputc(XMODEM_NAK);
                }
            } else {
                checksum = xmodem_get_char(1000);
                if (checksum == xmodem_calc_checksum(data, XMODEM_BLOCK_SIZE)) {
                    block_ok = 1;
                } else {
                    vdp_cursor_tab(2, 15);
                    printf("Checksum error block %d, retry %d", expected_block, retries + 1);
                    mos_uputc(XMODEM_NAK);
                }
            }
            retries++;
        }

        if (!block_ok) {
            mos_uputc(XMODEM_CAN);
            mos_uputc(XMODEM_CAN);
            result = -1;
            break;
        }

        fwrite(data, 1, XMODEM_BLOCK_SIZE, fp);
        mos_uputc(XMODEM_ACK);

        total_blocks++;
        file_size += XMODEM_BLOCK_SIZE;

        vdp_cursor_tab(2, 16);
        printf("Block %d received", expected_block);
        vdp_cursor_tab(2, 17);
        printf("Current size: %ld KB", file_size / 1024);

        expected_block++;

        c = xmodem_get_char(3000);
        if (c == XMODEM_EOT) {
            mos_uputc(XMODEM_ACK);
            vdp_cursor_tab(2, 18);
            printf("✅ Transfer complete! %d blocks, %ld KB",
                   total_blocks, file_size / 1024);
            break;
        }
    }

    fclose(fp);

    if (result != 0) {
        vdp_cursor_tab(2, 18);
        printf("❌ Transfer error");
        remove(filename);
    }

    vdp_cursor_tab(2, 20);
    printf("Press any key to continue...");
    while(!vdp_getKeyCode());
    clear_message_area();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}
