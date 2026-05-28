#include <stdio.h>
#include <agon/vdp.h>
#include "common.h"
#include "capture.h"
#include "ui.h"
#include "terminal.h"

void capture_start(void) {
    term.capture_mode = 1;
    term.log_file = fopen("capture.txt", "wb");
    update_status_line();
    clear_message_area();
    vdp_cursor_tab(2, 10);
    printf("Capture started - saving to capture.txt");
    for (volatile int d = 0; d < 2000; d++);
    clear_message_area();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}

void capture_stop(void) {
    term.capture_mode = 0;
    if (term.log_file) {
        fflush(term.log_file);
        fclose(term.log_file);
        term.log_file = NULL;
    }
    update_status_line();
    clear_message_area();
    vdp_cursor_tab(2, 10);
    printf("Capture stopped - file saved");
    for (volatile int d = 0; d < 2000; d++);
    clear_message_area();
    draw_frame();
    update_status_line();
    vdp_cursor_tab(term.cursor_x, term.cursor_y);
}
