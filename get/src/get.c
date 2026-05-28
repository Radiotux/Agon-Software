// get.c - AGON GET v2.2 (Official Stock ESP-AT Edition) - PART 1
// Wireless File Manager & Bit-Perfect Downloader for factory ESP-AT firmware
// Features: Auto-Paging Network Stream, Zero-RAM Overflow & Linear FSM Parser
// License: GNU General Public License v3.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <agon/mos.h>

#define MAX_FILES 50
#define MAX_FILENAME 64
#define MAX_LINE 256
#define DEFAULT_PORT 2000
#define UART_BUFFER_SIZE 65536
#define WRITE_BLOCK 64

// --- PROTOTYPES ---
void init_esp(void);
void wait_ms(int ms);
void flush_uart(void);
void uart_read_all(void);
void cerrar_conexion_segura(void);
void download_file(const char *ip, int port, int idx);
int fetch_file_list(const char *ip, int port);
void show_grid_menu(void);
void print_file_entry(int idx);

typedef struct {
    char name[MAX_FILENAME];
    uint32_t size;
} FileInfo;

static FileInfo files[MAX_FILES];
static int file_count = 0;
static uint8_t uart_ring[UART_BUFFER_SIZE];
static uint8_t block_buffer[WRITE_BLOCK];
static volatile uint16_t uart_head = 0;
static volatile uint16_t uart_tail = 0;

static char esp_detected_version[32] = "Unknown";
static char gmr_buf[512];

// --- NATIVE UART ENGINE ---
void send_esp(const char *cmd) {
    while (*cmd) {
        mos_uputc(*cmd++);
        for (volatile int i = 0; i < 100; i++);
    }
}

void wait_ms(int ms) {
    for (volatile int i = 0; i < ms * 500; i++);
}

void flush_uart(void) {
    while (mos_ugetc_nb() != -1);
    uart_head = uart_tail = 0;
}

void uart_read_all(void) {
    int24_t c;
    while ((c = mos_ugetc_nb()) != -1) {
        uint16_t next_head = (uart_head + 1) % UART_BUFFER_SIZE;
        if (next_head != uart_tail) {
            uart_ring[uart_head] = (uint8_t)c;
            uart_head = next_head;
        }
    }
}

// Atomic Hardware Initialization with On-The-Fly Firmware Detection
void init_esp(void) {
    flush_uart();
    printf("Resetting Wi-Fi adapter hardware...\n");

    send_esp("AT+RST\r\n");
    wait_ms(4500);
    flush_uart();

    send_esp("ATE0\r\n");
    wait_ms(200);

    send_esp("AT+GMR\r\n");
    wait_ms(300);

    uint16_t gmr_head = 0;
    int24_t vc;
    while ((vc = mos_ugetc_nb()) != -1 && gmr_head < 510) {
        gmr_buf[gmr_head++] = (char)vc;
    }
    gmr_buf[gmr_head] = '\0';

    char *ver_ptr = strstr(gmr_buf, "AT version:");
    if (ver_ptr) {
        char *end_line = strchr(ver_ptr, '\r');
        if (end_line) *end_line = '\0';
        strncpy(esp_detected_version, ver_ptr + 11, sizeof(esp_detected_version) - 1);
    } else {
        strcpy(esp_detected_version, "Stock AT");
    }

    send_esp("AT+CIPMUX=0\r\n");
    wait_ms(200);
    send_esp("AT+CIPMODE=0\r\n");
    wait_ms(200);
    flush_uart();

    printf("Wi-Fi adapter initialized successfully: AT v%s\n", esp_detected_version);
    wait_ms(800);
}

void cerrar_conexion_segura(void) {
    send_esp("AT+CIPCLOSE\r\n");
    wait_ms(200);
    flush_uart();
}

// get.c - AGON GET v2.2 (Official Stock ESP-AT Edition) - PART 2

void download_file(const char *ip, int port, int idx) {
    char cmd[MAX_LINE], http_req[MAX_LINE];
    FILE *f;
    uint32_t expected = files[idx].size;
    uint32_t total_guardado = 0;
    uint16_t b_ptr = 0;
    int primera_pagina = 1;
    int es_primer_bloque_del_archivo = 1;

    // Control variables for the linear +IPD state machine parser
    uint8_t ipd_state = 0; // 0=Searching '+', 1='I', 2='P', 3='D', 4=',', 5=Length
    uint32_t ipd_block_remaining = 0;
    char ipd_buf[16];
    uint8_t ipd_pos = 0;

    // Opening physical file pointer descriptor under MOS file system
    if (primera_pagina) {
        f = fopen(files[idx].name, "wb");
    }

    if (!f) {
        printf("\n[ERROR] Could not open SD Card destination file for writing.\n");
        return;
    }
    fclose(f); // Safe close to allow append mode in the paging loop natively

    // SEQUENTIAL PACKET AUTO-PAGING NET LOOP
    while (total_guardado < expected) {
        snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ip, port);
        send_esp(cmd);
        wait_ms(1500);

        snprintf(http_req, sizeof(http_req), "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n", files[idx].name, ip);
        int req_len = strlen(http_req);
        snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", req_len);
        flush_uart();
        send_esp(cmd);

        uint32_t prompt_timeout = 0;
        while (prompt_timeout < 150000) {
            int24_t pr = mos_ugetc_nb();
            if (pr == '>') break;
            prompt_timeout++;
        }
        flush_uart();
        send_esp(http_req);

        // HIGH-SPEED MEMORY RAW INGESTION BARS
        uint32_t silence_counter = 0;
        uint16_t local_head = 0;
        while (silence_counter < 800000 && local_head < (UART_BUFFER_SIZE - 2)) {
            int24_t c = mos_ugetc_nb();
            if (c != -1) {
                uart_ring[local_head++] = (uint8_t)c;
                silence_counter = 0;
            } else {
                silence_counter++;
            }
        }
        uart_ring[local_head] = '\0';

        cerrar_conexion_segura();

        // Appending blocks linearly to protect cluster maps on the FAT filesystem
        f = fopen(files[idx].name, "ab");
        if (!f) {
            printf("\n[ERROR] SD Card write descriptor lost mid-transfer.\n");
            return;
        }

        char *data_ptr = (char *)uart_ring;

        // HTTP HEADER CLEANER (First packet block entry only)
        if (es_primer_bloque_del_archivo) {
            uint16_t scan_idx = 0;
            while (scan_idx < local_head) {
                if (scan_idx + 15 < local_head && memcmp(&uart_ring[scan_idx], "HTTP/1.0 200 OK", 15) == 0) {
                    while (scan_idx + 4 <= local_head) {
                        if (uart_ring[scan_idx] == '\r' && uart_ring[scan_idx+1] == '\n' &&
                            uart_ring[scan_idx+2] == '\r' && uart_ring[scan_idx+3] == '\n') {
                            scan_idx += 4;
                        break;
                            }
                            scan_idx++;
                    }
                    data_ptr = (char *)&uart_ring[scan_idx];
                    break;
                }
                scan_idx++;
            }
            while (*data_ptr && *data_ptr < 32 && *data_ptr != '\t') {
                data_ptr++;
            }
            es_primer_bloque_del_archivo = 0;
        }

        uint16_t uart_ring_offset = (uint8_t *)data_ptr - uart_ring;
        uint32_t guardado_en_esta_pagina = 0;

        // MULTIPAGE FSM METADATA STRIPPER
        ipd_state = 0;
        ipd_block_remaining = 0;

        while (uart_ring_offset < local_head) {
            uint8_t c = uart_ring[uart_ring_offset++];

            if (ipd_block_remaining == 0) {
                if (ipd_state == 0) {
                    if (c == '+') ipd_state = 1;
                } else if (ipd_state == 1) { ipd_state = (c == 'I') ? 2 : 0; }
                else if (ipd_state == 2) { ipd_state = (c == 'P') ? 3 : 0; }
                else if (ipd_state == 3) { ipd_state = (c == 'D') ? 4 : 0; }
                else if (ipd_state == 4) {
                    if (c == ',') {
                        ipd_state = 5;
                        ipd_pos = 0;
                    } else {
                        ipd_state = 0;
                    }
                } else if (ipd_state == 5) {
                    if (c == ':') {
                        ipd_buf[ipd_pos] = '\0';
                        ipd_block_remaining = atol(ipd_buf);
                        ipd_state = 0;
                    } else if (c >= '0' && c <= '9' && ipd_pos < 14) {
                        ipd_buf[ipd_pos++] = (char)c;
                    } else {
                        ipd_state = 0;
                    }
                }
                continue;
            }

            if (ipd_block_remaining > 0) {
                ipd_block_remaining--;

                // STRICT SIZE LOCK PROTECTION
                if ((total_guardado + guardado_en_esta_pagina) < expected) {
                    block_buffer[b_ptr++] = c;
                    guardado_en_esta_pagina++;

                    if (b_ptr == WRITE_BLOCK) {
                        fwrite(block_buffer, 1, WRITE_BLOCK, f);
                        b_ptr = 0;
                    }
                } else {
                    break;
                }
            }
        }

        if (b_ptr > 0) {
            fwrite(block_buffer, 1, b_ptr, f);
            b_ptr = 0;
        }

        fclose(f);

        if (guardado_en_esta_pagina == 0) {
            printf("\n[ERROR] Transfer stream aborted. No network data received.\n");
            break;
        }

        total_guardado += guardado_en_esta_pagina;

        // ESTÉTICA COMPACTA: Impresión limpia en una sola línea usando retorno de carro
        printf("\r-> Progress: %lu / %lu bytes written to SD Card.", total_guardado, expected);
    }
}

// get.c - AGON GET v2.2 (Official Stock ESP-AT Edition) - PART 3

// --- VISUAL INTERFACE FILE ENTRY ---
void print_file_entry(int idx) {
    if (files[idx].size < 1024) {
        printf("[%2d] %-25.25s %5luB", idx + 1, files[idx].name, files[idx].size);
    } else {
        printf("[%2d] %-25.25s %5luK", idx + 1, files[idx].name, files[idx].size / 1024);
    }
}

// --- VISUAL INTERFACE GRID MENU ---
void show_grid_menu(void) {
    printf("\x0C"); // Native MOS console Clear Screen command
    printf("--- AGON GET v2.2 - Wireless File Manager (ESP-AT v%s) ---\n\n", esp_detected_version);
    int half = (file_count + 1) / 2;
    for (int i = 0; i < half; i++) {
        print_file_entry(i);
        printf(" | ");
        if (i + half < file_count) print_file_entry(i + half);
        printf("\n");
    }
    printf("\n 0. Exit\n\nSelect a number and press ENTER: ");
}

// --- FILE LIST PROCESSING ---
int fetch_file_list(const char *ip, int port) {
    char cmd[MAX_LINE], http_req[MAX_LINE];
    int num = 0;
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", ip, port);
    send_esp(cmd);
    wait_ms(1500);
    snprintf(http_req, sizeof(http_req), "GET /lista.txt HTTP/1.0\r\nHost: %s\r\n\r\n", ip);
    int req_len = strlen(http_req);
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d\r\n", req_len);
    flush_uart();
    send_esp(cmd);
    uint32_t prompt_timeout = 0;
    while (prompt_timeout < 150000) {
        int24_t pr = mos_ugetc_nb();
        if (pr == '>') break;
        prompt_timeout++;
    }
    flush_uart();
    send_esp(http_req);
    printf("\nFetching file grid from server...\n");
    uint32_t silence_counter = 0;
    uint16_t local_head = 0;
    while (silence_counter < 600000 && local_head < (UART_BUFFER_SIZE - 2)) {
        int24_t c = mos_ugetc_nb();
        if (c != -1) {
            uart_ring[local_head++] = (uint8_t)c;
            silence_counter = 0;
        } else {
            silence_counter++;
        }
    }
    uart_ring[local_head] = '\0';
    send_esp("AT+CIPCLOSE\r\n");
    wait_ms(150);
    flush_uart();
    char *search_ptr = (char *)uart_ring;
    char *pipe_ptr;
    while ((pipe_ptr = strchr(search_ptr, '|')) != NULL && num < MAX_FILES) {
        *pipe_ptr = '\0';
        uint32_t parsed_size = atol(pipe_ptr + 1);
        char *name_start = pipe_ptr - 1;
        while (name_start > search_ptr && *name_start > 32 && *name_start != ':' && *name_start != ',') {
            name_start--;
        }
        if (name_start != search_ptr || *name_start <= 32 || *name_start == ':' || *name_start == ',') {
            name_start++;
        }
        if (strlen(name_start) > 0 && strchr(name_start, '.') != NULL && parsed_size > 0) {
            if (strstr(name_start, "HTTP") == NULL && strstr(name_start, "OK") == NULL) {
                strncpy(files[num].name, name_start, MAX_FILENAME);
                files[num].size = parsed_size;
                num++;
            }
        }
        search_ptr = pipe_ptr + 1;
        while (*search_ptr && *search_ptr != '\n' && *search_ptr != '\r') {
            search_ptr++;
        }
    }
    file_count = num;
    return num;
}

// --- SYSTEM MAIN ENTRY POINT ---
int main(int argc, char *argv[]) {
    UART settings = {115200, 8, 1, 0, 0, 0};
    char ip_buffer[64]; // Secure local explicit size static character array allocation
    int port = DEFAULT_PORT;

    if (argc < 2) {
        printf("Usage: get <server_ip> or get <server_ip>:<port>\n");
        return 0;
    }
    if (mos_uopen(&settings) != 0) return 0;
    init_esp();

    const char *raw_arg = *(argv + 1);
    char *colon_ptr = strchr(raw_arg, ':');
    if (colon_ptr) {
        int ip_len = colon_ptr - raw_arg;
        if (ip_len > 63) ip_len = 63;
        strncpy(ip_buffer, raw_arg, ip_len);
        ip_buffer[ip_len] = '\0';
        port = atoi(colon_ptr + 1);
    } else {
        strncpy(ip_buffer, raw_arg, sizeof(ip_buffer) - 1);
        ip_buffer[sizeof(ip_buffer) - 1] = '\0';
    }

    if (fetch_file_list(ip_buffer, port) > 0) {
        while (1) {
            show_grid_menu();
            char buf[MAX_LINE];
            if (fgets(buf, sizeof(buf), stdin) == NULL) continue;
            int op = atoi(buf);
            if (op == 0) break;
            if (op > 0 && op <= file_count) download_file(ip_buffer, port, op - 1);
        }
    } else {
        printf("Error: Connection failed or file grid could not be parsed from server.\n");
    }

    mos_uclose();
    return 0;
}
