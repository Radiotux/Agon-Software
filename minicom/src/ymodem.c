// ymodem.c - YMODEM file transfer with 1K blocks and filename support

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <agon/vdp.h>
#include <agon/mos.h>
#include "common.h"
#include "ymodem.h"
#include "xmodem.h"
#include "ui.h"
#include "terminal.h"

// Buffer for YMODEM operations
static uint8_t ymodem_buffer[1024];

// Manual string to long conversion (avoid dependency on atol)
static long str_to_long(const char *s) {
    long result = 0;
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result;
}

// Calculate CRC16 (same as XMODEM)
static uint16_t ymodem_calc_crc16(uint8_t *data, int len) {
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

// Send filename block (header block)
static int ymodem_send_filename(const char *filename, long file_size) {
    int i, len;
    uint8_t block[YMODEM_BLOCK_SIZE_128];
    uint16_t crc;
    int retries;
    int c;
    
    // Prepare block 0 with filename and size
    memset(block, 0, YMODEM_BLOCK_SIZE_128);
    
    // Format: filename\0filesize\0
    len = strlen(filename);
    for (i = 0; i < len && i < YMODEM_BLOCK_SIZE_128 - 2; i++) {
        block[i] = filename[i];
    }
    block[i++] = 0;  // Null separator
    
    // Add file size as string
    if (file_size >= 0) {
        char size_str[16];
        sprintf(size_str, "%ld", file_size);
        for (int j = 0; size_str[j] && i < YMODEM_BLOCK_SIZE_128 - 1; j++) {
            block[i++] = size_str[j];
        }
        block[i++] = 0;
    }
    
    // Calculate CRC
    crc = ymodem_calc_crc16(block, YMODEM_BLOCK_SIZE_128);
    
    // Send block 0 with retries
    for (retries = 0; retries < YMODEM_RETRIES; retries++) {
        // Send SOH for 128-byte block, block number 0, complement 0xFF
        mos_uputc(YMODEM_SOH);
        mos_uputc(0);
        mos_uputc(0xFF);
        
        // Send data block
        for (i = 0; i < YMODEM_BLOCK_SIZE_128; i++) {
            mos_uputc(block[i]);
        }
        
        // Send CRC
        mos_uputc(crc >> 8);
        mos_uputc(crc & 0xFF);
        
        // Wait for ACK
        c = xmodem_get_char(3000);
        if (c == YMODEM_ACK) {
            return 1;  // Success
        }
        if (c == YMODEM_CAN) {
            return 0;  // Cancelled
        }
    }
    return 0;  // Failed
}

// Send data block (128 or 1024 bytes)
static int ymodem_send_block(uint8_t *data, int block_size, uint8_t block_num) {
    uint16_t crc;
    int retries;
    int c;
    
    crc = ymodem_calc_crc16(data, block_size);
    
    for (retries = 0; retries < YMODEM_RETRIES; retries++) {
        // Send STX for 1K block, SOH for 128-byte block
        if (block_size == YMODEM_BLOCK_SIZE_1K) {
            mos_uputc(YMODEM_STX);
        } else {
            mos_uputc(YMODEM_SOH);
        }
        mos_uputc(block_num);
        mos_uputc(255 - block_num);
        
        // Send data
        for (int i = 0; i < block_size; i++) {
            mos_uputc(data[i]);
        }
        
        // Send CRC
        mos_uputc(crc >> 8);
        mos_uputc(crc & 0xFF);
        
        // Wait for ACK
        c = xmodem_get_char(3000);
        if (c == YMODEM_ACK) {
            return 1;
        }
        if (c == YMODEM_CAN) {
            return 0;
        }
    }
    return 0;
}

// Receive filename block
static int ymodem_receive_filename(char *filename, long *file_size) {
    uint8_t block[YMODEM_BLOCK_SIZE_128];
    uint16_t crc_received, crc_calc;
    int retries;
    int c;
    
    // Send 'C' to request CRC mode
    for (retries = 0; retries < YMODEM_RETRIES; retries++) {
        mos_uputc(YMODEM_C);
        c = xmodem_get_char(3000);
        if (c == YMODEM_SOH) break;
        if (c == YMODEM_CAN) return 0;
    }
    
    if (c != YMODEM_SOH) return 0;
    
    // Receive block number (should be 0)
    c = xmodem_get_char(1000);
    if (c != 0) return 0;
    c = xmodem_get_char(1000);
    if (c != 0xFF) return 0;
    
    // Receive data
    for (int i = 0; i < YMODEM_BLOCK_SIZE_128; i++) {
        c = xmodem_get_char(1000);
        if (c == -1) return 0;
        block[i] = (uint8_t)c;
    }
    
    // Receive CRC
    crc_received = (xmodem_get_char(1000) << 8) | xmodem_get_char(1000);
    crc_calc = ymodem_calc_crc16(block, YMODEM_BLOCK_SIZE_128);
    
    if (crc_received != crc_calc) {
        mos_uputc(YMODEM_NAK);
        return 0;
    }
    
    // Parse filename and size
    char *p = (char*)block;
    int i = 0;
    while (i < YMODEM_BLOCK_SIZE_128 && p[i]) {
        filename[i] = p[i];
        i++;
    }
    filename[i] = '\0';
    
    // Move past null terminator
    i++;
    if (i < YMODEM_BLOCK_SIZE_128 && p[i]) {
        *file_size = str_to_long(p + i);  // Use manual conversion
    } else {
        *file_size = -1;
    }
    
    // Send ACK
    mos_uputc(YMODEM_ACK);
    
    return 1;
}

// Receive data block
static int ymodem_receive_block(uint8_t *data, int *block_size, uint8_t *block_num) {
    int c;
    uint16_t crc_received, crc_calc;
    
    c = xmodem_get_char(3000);
    if (c == YMODEM_EOT) {
        *block_size = 0;
        return 1;  // EOT received
    }
    if (c == YMODEM_CAN) return 0;  // Cancel
    
    if (c == YMODEM_STX) {
        *block_size = YMODEM_BLOCK_SIZE_1K;
    } else if (c == YMODEM_SOH) {
        *block_size = YMODEM_BLOCK_SIZE_128;
    } else {
        return 0;
    }
    
    // Get block number and complement
    *block_num = xmodem_get_char(1000);
    c = xmodem_get_char(1000);
    if (c != (255 - *block_num)) return 0;
    
    // Receive data
    for (int i = 0; i < *block_size; i++) {
        c = xmodem_get_char(1000);
        if (c == -1) return 0;
        data[i] = (uint8_t)c;
    }
    
    // Receive CRC
    crc_received = (xmodem_get_char(1000) << 8) | xmodem_get_char(1000);
    crc_calc = ymodem_calc_crc16(data, *block_size);
    
    if (crc_received != crc_calc) {
        mos_uputc(YMODEM_NAK);
        return 0;
    }
    
    mos_uputc(YMODEM_ACK);
    return 1;
}

// ========== YMODEM SEND ==========
void ymodem_send(void) {
    char filename[64] = {0};
    FILE *fp;
    uint8_t block_num = 1;
    int bytes_read;
    int result = 0;
    int c;
    long file_size;
    int use_1k_blocks = 1;  // Prefer 1K blocks
    int block_size;
    int percent;
    int total_blocks;
    int current_block = 0;

    if (read_filename(filename, 60, "YMODEM Send - Name") == 0) return;

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
    
    block_size = use_1k_blocks ? YMODEM_BLOCK_SIZE_1K : YMODEM_BLOCK_SIZE_128;
    total_blocks = (file_size + block_size - 1) / block_size;

    clear_message_area();
    vdp_cursor_tab(2, 12);
    printf("YMODEM Send: %s", filename);
    vdp_cursor_tab(2, 13);
    printf("Size: %ld bytes", file_size);
    vdp_cursor_tab(2, 14);
    printf("Waiting for receiver...");

    xmodem_flush();
    
    // Wait for receiver to send 'C'
    c = xmodem_get_char(10000);
    if (c != YMODEM_C) {
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
    
    // Send filename header
    if (!ymodem_send_filename(filename, file_size)) {
        vdp_cursor_tab(2, 15);
        printf("Error: Header not accepted");
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
    
    vdp_cursor_tab(2, 15);
    printf("Header accepted, sending data...");

    // Send data blocks
    while (!feof(fp) && result == 0) {
        bytes_read = fread(ymodem_buffer, 1, block_size, fp);
        if (bytes_read == 0) break;
        
        // Pad last block with 0x1A
        for (int j = bytes_read; j < block_size; j++) {
            ymodem_buffer[j] = 0x1A;
        }
        
        if (!ymodem_send_block(ymodem_buffer, block_size, block_num)) {
            result = -1;
            break;
        }
        
        current_block++;
        percent = (current_block * 100) / total_blocks;
        vdp_cursor_tab(2, 16);
        printf("Progress: %3d%% [%d/%d blocks]", percent, current_block, total_blocks);
        
        block_num++;
        if (block_num == 0) block_num = 1;  // Wrap around
    }

    // Send EOT
    if (result == 0) {
        int retries = 0;
        while (retries < YMODEM_RETRIES) {
            mos_uputc(YMODEM_EOT);
            c = xmodem_get_char(3000);
            if (c == YMODEM_ACK) {
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

// ========== YMODEM RECEIVE ==========
void ymodem_receive(void) {
    char filename[64] = {0};
    FILE *fp;
    uint8_t expected_block = 1;
    int result = 0;
    long file_size = 0;
    int total_blocks = 0;
    int block_size;
    int current_block = 0;

    xmodem_flush();
    
    clear_message_area();
    vdp_cursor_tab(2, 12);
    printf("YMODEM Receive - Waiting for sender...");

    // Receive filename header
    if (!ymodem_receive_filename(filename, &file_size)) {
        vdp_cursor_tab(2, 13);
        printf("Error: No header received");
        vdp_cursor_tab(2, 15);
        printf("Press any key...");
        while(!vdp_getKeyCode());
        clear_message_area();
        draw_frame();
        update_status_line();
        vdp_cursor_tab(term.cursor_x, term.cursor_y);
        return;
    }
    
    vdp_cursor_tab(2, 13);
    printf("File: '%s'", filename);
    if (file_size > 0) {
        vdp_cursor_tab(2, 14);
        printf("Size: %ld bytes", file_size);
    }
    
    fp = fopen(filename, "wb");
    if (!fp) {
        vdp_cursor_tab(2, 15);
        printf("Error: Cannot create '%s'", filename);
        vdp_cursor_tab(2, 17);
        printf("Press any key...");
        while(!vdp_getKeyCode());
        clear_message_area();
        draw_frame();
        update_status_line();
        vdp_cursor_tab(term.cursor_x, term.cursor_y);
        return;
    }

    vdp_cursor_tab(2, 15);
    printf("Receiving data...");

    // Receive data blocks
    while (result == 0) {
        uint8_t data[YMODEM_BLOCK_SIZE_1K];
        uint8_t block_num;
        int received;
        
        received = ymodem_receive_block(data, &block_size, &block_num);
        
        if (received == 0) {
            result = -1;
            break;
        }
        
        if (block_size == 0) {  // EOT received
            mos_uputc(YMODEM_ACK);
            vdp_cursor_tab(2, 16);
            printf("✅ Transfer complete! %d blocks", total_blocks);
            break;
        }
        
        if (block_num != expected_block) {
            vdp_cursor_tab(2, 16);
            printf("Sequence error: expected %d, got %d", expected_block, block_num);
            mos_uputc(YMODEM_CAN);
            result = -1;
            break;
        }
        
        // Write data to file
        fwrite(data, 1, block_size, fp);
        total_blocks++;
        
        current_block++;
        if (file_size > 0) {
            int percent = (current_block * block_size * 100) / file_size;
            if (percent > 100) percent = 100;
            vdp_cursor_tab(2, 16);
            printf("Progress: %3d%% [%d blocks]", percent, current_block);
        } else {
            vdp_cursor_tab(2, 16);
            printf("Block %d received", current_block);
        }
        
        expected_block++;
        if (expected_block == 0) expected_block = 1;
    }

    fclose(fp);
    
    if (result != 0) {
        vdp_cursor_tab(2, 17);
        printf("❌ Transfer error");
        remove(filename);
    }

    vdp_cursor_tab(2, 19);
    printf("Press any key to continue...");
    while(!vdp_getKeyCode());
    clear_message_area();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}