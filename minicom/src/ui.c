// ui.c - Interfaz de usuario

#include <stdio.h>
#include <ctype.h>
#include <agon/vdp.h>
#include "common.h"
#include "ui.h"
#include "terminal.h"

void clear_message_area(void) {
    // Limpiar desde la línea 6 hasta la 27 (área de ayuda)
    for (int y = 6; y <= 27; y++) {
        vdp_cursor_tab(2, y);
        for (int x = 0; x < 76; x++) printf(" ");
    }
}

void draw_frame(void) {
    // Línea 0: Borde superior
    vdp_cursor_tab(0, 0);
    printf("+------------------------------------------------------------------------------+");
    
    // Línea 1: Título
    vdp_cursor_tab(0, 1);
    printf("|                         AGON TERMINAL v2.4 - X/YMODEM                        |");
    
    // Línea 2: Separador
    vdp_cursor_tab(0, 2);
    printf("+------------------------------------------------------------------------------+");
    
    // Línea 3: Barra de comandos principal
    vdp_cursor_tab(0, 3);
    printf("| Ctrl+H:Help Ctrl+B:Conf Ctrl+C:XSend Ctrl+D:XRecv Ctrl+Y:YSend Ctrl+U:YRecv  |");
    
    // Línea 4: Barra de comandos secundaria
    vdp_cursor_tab(0, 4);
    printf("| Ctrl+E:Echo Ctrl+F:Capture Ctrl+L:Clear Ctrl+Q:Exit                           |");
    vdp_cursor_tab(0, 4);
    printf("+------------------------------------------------------------------------------+");

    // Área de trabajo (líneas 5 a 28) - 24 líneas
    for (int i = WORK_START_ROW; i <= WORK_END_ROW; i++) {
        vdp_cursor_tab(0, i);
        printf("|                                                                              |");
    }

    // Barra de estado en línea 29
    vdp_cursor_tab(0, 29);
    printf("|                                                                              |");
}

void update_status_line(void) {
    vdp_cursor_tab(1, 29);
    printf(" 115200 8N1  |  Echo:%-3s  |  Capture:%-3s  |  XMODEM:%-4s  |  Connected:%-3s   ",
           term.local_echo ? "ON" : "OFF",
           term.capture_mode ? "ON" : "OFF",
           term.xmodem_crc ? "CRC" : "CHK",
           term.connected ? "YES" : "NO");
}

void show_full_help(void) {
    int start_y = 6;
    int start_x = 15;

    clear_message_area();

    vdp_cursor_tab(start_x, start_y);
    printf("+---------------------------------------------+");
    vdp_cursor_tab(start_x, start_y+1);
    printf("|              TERMINAL COMMANDS              |");
    vdp_cursor_tab(start_x, start_y+2);
    printf("+---------------------------------------------+");
    vdp_cursor_tab(start_x, start_y+3);
    printf("| Ctrl+H : Help          Ctrl+Q : Exit        |");
    vdp_cursor_tab(start_x, start_y+4);
    printf("| Ctrl+B : Config        Ctrl+L : Clear       |");
    vdp_cursor_tab(start_x, start_y+5);
    printf("| Ctrl+E : Echo          Ctrl+F : Capture     |");
    vdp_cursor_tab(start_x, start_y+6);
    printf("|---------------------------------------------|");
    vdp_cursor_tab(start_x, start_y+7);
    printf("| XMODEM: Ctrl+C Send    Ctrl+D Receive       |");
    vdp_cursor_tab(start_x, start_y+8);
    printf("| YMODEM: Ctrl+Y Send    Ctrl+U Receive       |");
    vdp_cursor_tab(start_x, start_y+9);
    printf("|---------------------------------------------|");
    vdp_cursor_tab(start_x, start_y+10);
    printf("| Line endings: Always CR+LF for ESP          |");
    vdp_cursor_tab(start_x, start_y+11);
    printf("| XMODEM: Press T in config to toggle CRC     |");
    vdp_cursor_tab(start_x, start_y+12);
    printf("| YMODEM: Uses 1K blocks, filename support    |");
    vdp_cursor_tab(start_x, start_y+13);
    printf("| Capture: Saves to capture.txt on SD         |");
    vdp_cursor_tab(start_x, start_y+14);
    printf("+---------------------------------------------+");
    vdp_cursor_tab(start_x, start_y+15);
    printf("  Press any key to continue");

    while(!vdp_getKeyCode());
    while(vdp_getKeyCode() != 0);

    clear_message_area();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}

void show_config(void) {
    int start_y = 10;  // Centrado en el área ampliada
    int start_x = 18;

    clear_message_area();

    vdp_cursor_tab(start_x, start_y);
    printf("+------------------------------+");
    vdp_cursor_tab(start_x, start_y+1);
    printf("|     CURRENT CONFIGURATION    |");
    vdp_cursor_tab(start_x, start_y+2);
    printf("+------------------------------+");
    vdp_cursor_tab(start_x, start_y+3);
    printf("| Baud rate : 115200           |");
    vdp_cursor_tab(start_x, start_y+4);
    printf("| Data bits : 8                |");
    vdp_cursor_tab(start_x, start_y+5);
    printf("| Stop bits : 1                |");
    vdp_cursor_tab(start_x, start_y+6);
    printf("| Parity     : None            |");
    vdp_cursor_tab(start_x, start_y+7);
    printf("| XMODEM     : %-4s            |", term.xmodem_crc ? "CRC" : "CHKSUM");
    vdp_cursor_tab(start_x, start_y+8);
    printf("| Status     : %-10s      |", term.connected ? "Connected" : "Disconnected");
    vdp_cursor_tab(start_x, start_y+9);
    printf("+------------------------------+");
    vdp_cursor_tab(start_x, start_y+10);
    printf("  T: Toggle CRC                ");
    vdp_cursor_tab(start_x, start_y+11);
    printf("  Any key: exit                ");

    while(1) {
        uint8_t key = vdp_getKeyCode();
        if (key != 0) {
            if (toupper(key) == 'T') {
                term.xmodem_crc = !term.xmodem_crc;
                update_status_line();
                vdp_cursor_tab(start_x, start_y+7);
                printf("| XMODEM     : %-4s             |", term.xmodem_crc ? "CRC" : "CHKSUM");
            } else {
                break;
            }
            while (vdp_getKeyCode() != 0);
        }
    }

    clear_message_area();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}
