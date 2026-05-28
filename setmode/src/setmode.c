/*
 * setmode.c - Change the Agon Light screen resolution.
 * For VDP 2.13.0+ / MOS 3.0+
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <agon/mos.h>

typedef struct {
    int mode;
    int width;
    int height;
    int colors;
    const char *desc;
} screen_mode_t;

screen_mode_t modes[] = {
    {0, 640, 480, 16, "640x480"},
    {1, 640, 480, 4, "640x480"},
    {2, 640, 480, 2, "640x480"},
    {3, 640, 240, 64, "640x240"},
    {4, 640, 240, 16, "640x240"},
    {5, 640, 240, 4, "640x240"},
    {6, 640, 240, 2, "640x240"},
    {7, 0, 0, 16, "Teletext"},
    {8, 320, 240, 64, "320x240"},
    {9, 320, 240, 16, "320x240"},
    {10, 320, 240, 4, "320x240"},
    {11, 320, 240, 2, "320x240"},
    {12, 320, 200, 64, "320x200"},
    {13, 320, 200, 16, "320x200"},
    {14, 320, 200, 4, "320x200"},
    {15, 320, 200, 2, "320x200"},
    {16, 800, 600, 4, "800x600"},
    {17, 800, 600, 2, "800x600"},
    {18, 1024, 768, 2, "1024x768"},
    {19, 1024, 768, 4, "1024x768"},
    {20, 512, 384, 64, "512x384"},
    {21, 512, 384, 16, "512x384"},
    {22, 512, 384, 4, "512x384"},
    {23, 512, 384, 2, "512x384"},
    {24, 640, 512, 16, "640x512"},
    {25, 640, 512, 4, "640x512"},
    {26, 640, 512, 2, "640x512"},
    {27, 640, 256, 64, "640x256"},
    {28, 640, 256, 16, "640x256"},
    {29, 640, 256, 4, "640x256"},
    {30, 640, 256, 2, "640x256"},
    {0, 0, 0, 0, NULL}
};

void show_help(void) {
    printf("setmode - Change the Agon Light screen resolution\n");
    printf("Usage: setmode [mode] | setmode -l | setmode -h\n\n");
    printf("Options:\n");
    printf("  -l, --list     List all available screen modes\n");
    printf("  -h, --help     Show this help\n\n");
    printf("Modes with +128 (129-158) enable double-buffering\n");
    printf("Example: setmode 129 (double-buffered version of mode 1)\n");
}

void list_modes(void) {
    int i;
    int total = 0;
    int half;
    char res_left[16], res_right[16];
    
    while (modes[total].desc != NULL) total++;
    half = (total + 1) / 2;
    
    printf("\n");
    printf("| Mode | Resolution | Colours |   | Mode | Resolution | Colours |\n");
    printf("|------|------------|---------|   |------|------------|---------|\n");
    
    for (i = 0; i < half; i++) {
        // Left column - con Teletext alineado usando el mismo ancho que las resoluciones
        if (modes[i].mode == 7) {
            snprintf(res_left, sizeof(res_left), "Teletext");
        } else {
            snprintf(res_left, sizeof(res_left), "%dx%d", modes[i].width, modes[i].height);
        }
        printf("|  %2d  | %9s  |   %2d    |   ", modes[i].mode, res_left, modes[i].colors);
        
        // Right column
        if (i + half < total) {
            if (modes[i + half].mode == 7) {
                snprintf(res_right, sizeof(res_right), "Teletext");
            } else {
                snprintf(res_right, sizeof(res_right), "%dx%d", 
                         modes[i + half].width, modes[i + half].height);
            }
            printf("|  %2d  | %9s  |   %2d    |\n",
                   modes[i + half].mode, res_right, modes[i + half].colors);
        } else {
            printf("|      |            |         |\n");
        }
    }
    
    printf("\nNote: Double-buffered modes = mode + 128 (129 to 158)\n");
}

int main(int argc, char *argv[]) {
    int mode;
    char cmd[50];

    if (argc < 2) {
        fprintf(stderr, "setmode: missing mode. Try 'setmode -h'.\n");
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        show_help();
        return 0;
    }

    if (strcmp(argv[1], "-l") == 0 || strcmp(argv[1], "--list") == 0) {
        list_modes();
        return 0;
    }

    mode = atoi(argv[1]);

    if ((mode < 0 || mode > 30) && (mode < 129 || mode > 158)) {
        fprintf(stderr, "setmode: invalid mode '%d'. Valid modes are 0-30 and 129-158.\n", mode);
        fprintf(stderr, "Use 'setmode -l' to list all modes.\n");
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "VDU 22 %d", mode);
    mos_oscli(cmd, NULL, 0);
    mos_oscli("CLS", NULL, 0);

    for (int i = 0; modes[i].desc != NULL; i++) {
        if (modes[i].mode == mode || modes[i].mode + 128 == mode) {
            if (modes[i].mode == 7) {
                printf("Screen mode changed to %d: Teletext, %d colours\n", mode, modes[i].colors);
            } else {
                printf("Screen mode changed to %d: %dx%d, %d colours\n", 
                       mode, modes[i].width, modes[i].height, modes[i].colors);
            }
            break;
        }
    }

    return 0;
}
