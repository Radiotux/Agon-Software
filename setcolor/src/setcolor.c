/*
 * setcolor.c - Change text and background colour of the Agon Light console.
 * 
 * Usage: setcolor [text_colour] [on background_colour]
 *        setcolor -h
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <agon/mos.h>

// Mapeo de nombres de colores a números de paleta (0-7)
typedef struct {
    char *name;
    int code;
} colour_map_t;

colour_map_t colours[] = {
    {"black", 0},
    {"red", 1},
    {"green", 2},
    {"yellow", 3},
    {"blue", 4},
    {"magenta", 5},
    {"cyan", 6},
    {"white", 7},
    {NULL, -1}
};

// Mapeo de nombres a valores RGB (0-255)
typedef struct {
    char *name;
    int r;
    int g;
    int b;
} rgb_map_t;

rgb_map_t rgb_colours[] = {
    {"black",   0, 0, 0},
    {"red",     255, 0, 0},
    {"green",   0, 255, 0},
    {"yellow",  255, 255, 0},
    {"blue",    0, 0, 255},
    {"magenta", 255, 0, 255},
    {"cyan",    0, 255, 255},
    {"white",   255, 255, 255},
    {"orange",  255, 128, 0},
    {NULL, 0, 0, 0}
};

int get_colour_code(char *name) {
    for (int i = 0; colours[i].name != NULL; i++) {
        if (strcmp(colours[i].name, name) == 0) {
            return colours[i].code;
        }
    }
    return -1;
}

int get_rgb(char *name, int *r, int *g, int *b) {
    for (int i = 0; rgb_colours[i].name != NULL; i++) {
        if (strcmp(rgb_colours[i].name, name) == 0) {
            *r = rgb_colours[i].r;
            *g = rgb_colours[i].g;
            *b = rgb_colours[i].b;
            return 1;
        }
    }
    return 0;
}

void send_vdu(const char *cmd) {
    mos_oscli((char *)cmd, NULL, 0);
}

void set_text_colour(int colour_code) {
    char cmd[20];
    snprintf(cmd, sizeof(cmd), "VDU 17 %d", colour_code);
    send_vdu(cmd);
}

void set_background_colour(char *colour_name) {
    int r, g, b;
    if (get_rgb(colour_name, &r, &g, &b)) {
        char cmd[50];
        // Formato correcto: VDU 19, l, -1, r, g, b
        snprintf(cmd, sizeof(cmd), "VDU 19 0 -1 %d %d %d", r, g, b);
        send_vdu(cmd);
    } else {
        fprintf(stderr, "setcolor: invalid background colour '%s'.\n", colour_name);
    }
}

void clear_screen(void) {
    send_vdu("CLS");
}

void reset_colours(void) {
    set_background_colour("black");
    set_text_colour(7);  // White
    clear_screen();
}

void show_help(void) {
    printf("setcolor - Change text and background colour\n");
    printf("Usage: setcolor [text_colour] [on background_colour]\n\n");
    printf("Text colours:\n");
    printf("  black, red, green, yellow, blue, magenta, cyan, white\n\n");
    printf("Background colours:\n");
    printf("  black, red, green, yellow, blue, magenta, cyan, white, orange\n\n");
    printf("Examples:\n");
    printf("  setcolor green\n");
    printf("  setcolor white on blue\n");
    printf("  setcolor cyan on black\n");
    printf("  setcolor white on orange\n");
    printf("  setcolor -r               (reset to default)\n");
}

int main(int argc, char *argv[]) {
    int fg_code;

    if (argc < 2) {
        fprintf(stderr, "setcolor: missing colour. Try 'setcolor -h'.\n");
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        show_help();
        return 0;
    }

    if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--reset") == 0) {
        reset_colours();
        return 0;
    }

    fg_code = get_colour_code(argv[1]);
    if (fg_code == -1) {
        fprintf(stderr, "setcolor: invalid colour '%s'.\n", argv[1]);
        return 1;
    }

    // Cambiar fondo si se especifica
    if (argc > 2 && strcmp(argv[2], "on") == 0 && argc > 3) {
        set_background_colour(argv[3]);
    }

    // Cambiar color del texto
    set_text_colour(fg_code);

    // Aplicar los cambios limpiando la pantalla
    clear_screen();

    return 0;
}