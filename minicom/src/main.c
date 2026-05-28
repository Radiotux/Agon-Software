// main.c - Con soporte para XMODEM, YMODEM Send/Receive

#include <stdlib.h>
#include <stdio.h>
#include <agon/vdp.h>
#include <agon/mos.h>
#include "common.h"
#include "terminal.h"
#include "ui.h"
#include "xmodem.h"
#include "ymodem.h"
#include "capture.h"

void process_key(uint8_t key) {
    if (key == CTRL_Q) {
        if (term.capture_mode && term.log_file) fclose(term.log_file);
        mos_uclose();
        terminal_clear();
        printf("Terminal closed.\n");
        exit(0);
    }

    if (key == CTRL_H) {
        show_full_help();
        return;
    }

    if (key == CTRL_B) {
        show_config();
        return;
    }

    if (key == CTRL_C) {
        xmodem_send();
        return;
    }

    if (key == CTRL_D) {
        xmodem_receive();
        return;
    }

    if (key == CTRL_Y) {  // Ctrl+Y = YMODEM Send
        ymodem_send();
        return;
    }

    if (key == CTRL_U) {  // Ctrl+U = YMODEM Receive
        ymodem_receive();
        return;
    }

    if (key == CTRL_Z) {  // Ctrl+Z = ZMODEM (placeholder)
        clear_message_area();
        vdp_cursor_tab(2, 12);
        printf("ZMODEM - Coming soon!");
        vdp_cursor_tab(2, 14);
        printf("Press any key...");
        while(!vdp_getKeyCode());
        clear_message_area();
        draw_frame();
        update_status_line();
        vdp_cursor_tab(term.cursor_x, term.cursor_y);
        return;
    }

    if (key == CTRL_E) {
        term.local_echo = !term.local_echo;
        update_status_line();
        return;
    }

    if (key == CTRL_F) {
        if (!term.capture_mode) capture_start();
        else capture_stop();
        return;
    }

    if (key == CTRL_L) {
        terminal_clear();
        draw_frame();
        update_status_line();
        term.cursor_x = 1;
        term.cursor_y = WORK_START_ROW;
        vdp_cursor_tab(term.cursor_x, term.cursor_y);
        return;
    }

    if (key == 8 || key == 127) {
        mos_uputc(8);
        if (term.local_echo && term.cursor_x > 1) {
            term.cursor_x--;
            vdp_cursor_tab(term.cursor_x, term.cursor_y);
            putchar(' ');
            vdp_cursor_tab(term.cursor_x, term.cursor_y);
        }
        return;
    }

    if (key == 13) {
        mos_uputc(13);
        mos_uputc(10);
        if (term.local_echo) {
            work_newline();
        }
        return;
    }

    if (key >= 32 && key < 127) {
        mos_uputc(key);
        if (term.local_echo) {
            work_putchar(key);
        }
        return;
    }
}

int main(void) {
    terminal_init();
    uint8_t last_key = 0;
    int repeat_count = 0;

    while(1) {
        uart_read_all();
        process_uart_buffer();

        if (uart_buffer_head == uart_buffer_tail) {
            uint8_t key = vdp_getKeyCode();
            if (key != 0) {
                if (key == last_key) {
                    repeat_count++;
                    if (repeat_count > 2) { last_key = 0; repeat_count = 0; key = 0; }
                } else { repeat_count = 0; last_key = key; }
            }
            if (key != 0) {
                process_key(key);
                while (vdp_getKeyCode() != 0) {
                    uart_read_all();
                    process_uart_buffer();
                }
                last_key = 0; repeat_count = 0;
            }
        }
    }
    return 0;
}