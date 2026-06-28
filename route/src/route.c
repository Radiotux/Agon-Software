// route.c - Native IP Routing Table utility for Agon Light 2
// Strictly based on the non-blocking architecture of Electrotux/Radiotux's ifconfig.c

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
                if ((char)c == match[0]) {
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
                    if ((char)c == token[0]) {
                        t_idx = 1;
                    }
                }
            } else {
                if ((char)c == '"' || (char)c == '\r' || (char)c == '\n') {
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

int main(void) {
    UART settings;
    char local_ip[BUFFER_SIZE];
    char netmask[BUFFER_SIZE];
    char gateway[BUFFER_SIZE];
    char subnet[BUFFER_SIZE];
    volatile uint8_t *sv = (volatile uint8_t *)mos_sysvars();

    strcpy(local_ip, "0.0.0.0");
    strcpy(netmask, "0.0.0.0");
    strcpy(gateway, "0.0.0.0");
    strcpy(subnet, "0.0.0.0");

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

    send_esp("AT+CIPSTA?\r\n");

    // Strict chronological extraction according to the ESP-01S AT firmware stream
    extract_token_value(sv, "+CIPSTA:ip:\"", local_ip, 400);
    extract_token_value(sv, "+CIPSTA:gateway:\"", gateway, 400);
    extract_token_value(sv, "+CIPSTA:netmask:\"", netmask, 400);

    flush_uart();
    mos_uclose();

    // Passive subnet calculation assuming a standard Class C network layout
    int o1 = 0, o2 = 0, o3 = 0, o4 = 0;
    if (sscanf(local_ip, "%d.%d.%d.%d", &o1, &o2, &o3, &o4) == 4) {
        sprintf(subnet, "%d.%d.%d.0", o1, o2, o3);
    }

    // Modern Unix-style routing table rendering
    printf("\nAgon IP Routing Table (ESP-01S @ 115200 bps)\n");
    printf("====================================================================\n");
    printf("%-18s %-18s %-18s %-10s\n", "Destination", "Gateway", "Genmask", "Interface");
    printf("--------------------------------------------------------------------\n");

    // Default Gateway Route
    printf("%-18s %-18s %-18s %-10s\n", "0.0.0.0", gateway, netmask, "wlan0");

    // Directly connected local link network route
    if (strcmp(subnet, "0.0.0.0") != 0) {
        printf("%-18s %-18s %-18s %-10s\n", subnet, "0.0.0.0", netmask, "wlan0");
    }
    printf("====================================================================\n");

    return 0;
}
