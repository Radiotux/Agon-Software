// arp.c - Native ARP cache display utility for Agon Light 2
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
    char gateway_ip[BUFFER_SIZE];
    char station_mac[BUFFER_SIZE];
    volatile uint8_t *sv = (volatile uint8_t *)mos_sysvars();

    strcpy(gateway_ip, "0.0.0.0");
    strcpy(station_mac, "00:00:00:00:00:00");

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

    // First, query the station MAC address
    send_esp("AT+CIPSTAMAC?\r\n");
    extract_token_value(sv, "+CIPSTAMAC:\"", station_mac, 400);
    flush_uart();

    // Second, query the configuration to get the Gateway IP
    send_esp("AT+CIPSTA?\r\n");
    extract_token_value(sv, "+CIPSTA:gateway:\"", gateway_ip, 400);

    flush_uart();
    mos_uclose();

    // Standard Unix-style ARP table layout rendering
    printf("\nAgon ARP Cache Table\n");
    printf("====================================================================\n");
    printf("%-18s %-20s %-12s %-10s\n", "Address", "HWaddress", "HWtype", "Interface");
    printf("--------------------------------------------------------------------\n");

    // Display the Access Point Gateway ARP entry
    // (Since standard AT firmware doesn't expose the remote MAC, we flag it or leave it as dynamic)
    if (strcmp(gateway_ip, "0.0.0.0") != 0) {
        printf("%-18s %-20s %-12s %-10s\n", gateway_ip, "<dynamic>", "ether", "wlan0");
    }

    // Display local station interface physical identity
    printf("%-18s %-20s %-12s %-10s\n", "static-if", station_mac, "ether", "wlan0");
    printf("====================================================================\n");

    return 0;
}
