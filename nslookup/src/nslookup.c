// nslookup.c - Native DNS Query utility for Agon Light 2
// Strictly based on the non-blocking architecture of Radiotux's ifconfig.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <agon/mos.h>

#define BUFFER_SIZE 64

void wait_ms(uint32_t ms, volatile uint8_t *sv) {
    uint32_t start = *(volatile uint32_t *)(sv + 0x00);
    while ((*(volatile uint32_t *)(sv + 0x00) - start) < ms);
}

void flush_uart(void) {
    while (mos_ugetc_nb() >= 0);
}

void send_esp(const char *cmd) {
    while (*cmd) {
        mos_uputc(*cmd++);
    }
}

int scan_init_ok(volatile uint8_t *sv, uint32_t timeout_ms) {
    char match[] = "OK\r\n";
    int m_idx = 0;
    uint32_t start = *(volatile uint32_t *)(sv + 0x00);

    while ((*(volatile uint32_t *)(sv + 0x00) - start) < timeout_ms) {
        int c = mos_ugetc_nb();
        if (c >= 0) {
            if ((char)c == match[m_idx]) {
                m_idx++;
                if (match[m_idx] == '\0') {
                    return 1;
                }
            } else {
                m_idx = 0;
                if ((char)c == match[0]) { // Corregido para eliminar warning
                    m_idx = 1;
                }
            }
        }
    }
    return 0;
}

int init_esp(volatile uint8_t *sv) {
    send_esp("+++");
    wait_ms(400, sv);
    flush_uart();
    send_esp("AT\r\n");
    if (!scan_init_ok(sv, 1500)) {
        return 0;
    }
    flush_uart();
    send_esp("ATE0\r\n");
    wait_ms(100, sv);
    flush_uart();
    return 1;
}

int extract_token_value(volatile uint8_t *sv, const char *token, char *output, uint32_t timeout_ms) {
    int t_len = strlen(token);
    int t_idx = 0;
    int o_idx = 0;
    uint32_t start = *(volatile uint32_t *)(sv + 0x00);

    while ((*(volatile uint32_t *)(sv + 0x00) - start) < timeout_ms) {
        int c = mos_ugetc_nb();
        if (c >= 0) {
            if (t_idx < t_len) {
                if ((char)c == token[t_idx]) {
                    t_idx++;
                } else {
                    t_idx = 0;
                    if ((char)c == token[0]) { // Corregido para eliminar warning
                        t_idx = 1;
                    }
                }
            } else {
                if ((char)c == '\r' || (char)c == '\n') {
                    output[o_idx] = '\0';
                    return 1;
                }
                if (o_idx < (BUFFER_SIZE - 1)) {
                    output[o_idx++] = (char)c;
                }
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    UART settings;
    char ip_resolved[BUFFER_SIZE];
    char cmd_buffer[BUFFER_SIZE + 32];
    volatile uint8_t *sv = (volatile uint8_t *)mos_sysvars();

    // CORRECCIÓN CRUCIAL: Si faltan argumentos, mostramos la guía de uso
    // pero retornamos 0 para indicarle a MOS que el programa cerró en paz
    if (argc < 2) {
        printf("Usage: *nslookup <domain_name>\n");
        printf("Example: *nslookup google.com\n");
        return 0;
    }

    strcpy(ip_resolved, "0.0.0.0");

    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.stopBits = 1;
    settings.parity = 0;
    settings.flowcontrol = 0;
    settings.eir = 0;

    if (mos_uopen(&settings) != 0) {
        printf("Error: Could not open UART1.\n");
        return 1;
    }

    if (!init_esp(sv)) {
        flush_uart();
        mos_uclose();
        printf("Error: Could not initialize ESP-01S (AT commands).\n");
        return 1;
    }

    sprintf(cmd_buffer, "AT+CIPDOMAIN=\"%s\"\r\n", argv[1]);
    send_esp(cmd_buffer);

    extract_token_value(sv, "+CIPDOMAIN:", ip_resolved, 2500);

    flush_uart();
    mos_uclose();

    if (strcmp(ip_resolved, "0.0.0.0") == 0) {
        printf("\nError: Could not resolve host name '%s'. Check connectivity or DNS settings.\n", argv[1]);
        return 0; // También salimos con 0 aquí si deseas un fallo silencioso en MOS
    }

    printf("\nAgon DNS Resolution Utility\n");
    printf("====================================================================\n");
    printf("Server:         ESP-01S Gateway DNS\n");
    printf("Host Query:     %-30s\n", argv[1]);
    printf("Address:        %-30s\n", ip_resolved);
    printf("====================================================================\n");

    return 0;
}
