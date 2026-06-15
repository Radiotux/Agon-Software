// ifconfig.c - Native Network Interface Configuration utility for Agon Light 2
// Queries the Wi-Fi coprocessor to render network parameters with a classic Linux look

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <agon/mos.h>

#define BUFFER_SIZE 64

void send_esp(const char * cmd) {
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
}

// Scans the serial buffer dynamically with safe timing to protect VDP stability
int extract_token_value(volatile uint8_t * sv, const char * token, char * output, uint32_t timeout_ms) {
    uint32_t start_ms = *(volatile uint32_t *)(sv + 0x00);
    int state = 0;
    int token_len = strlen(token);
    int out_idx = 0;
    int capturing = 0;

    while ((*(volatile uint32_t *)(sv + 0x00) - start_ms) < timeout_ms) {
        int c = mos_ugetc_nb();
        if (c >= 0) {
            if (!capturing) {
                if (c == token[state]) {
                    state++;
                    if (state == token_len) {
                        capturing = 1;
                    }
                } else {
                    state = (c == token[0]) ? 1 : 0; // Corrected array index type warning
                }
            } else {
                if (c == '"') {
                    output[out_idx] = '\0';
                    return 1;
                }
                if (out_idx < BUFFER_SIZE - 1) {
                    output[out_idx++] = (char)c;
                }
            }
        }
        // Restored a balanced micro-pause to maintain hardware sync and prevent VDP crashes
        for (volatile int i = 0; i < 60; i++);
    }
    return 0;
}

int scan_init_ok(volatile uint8_t * sv, uint32_t timeout_ms) {
    uint32_t start_ms = *(volatile uint32_t *)(sv + 0x00);
    int s_ok = 0;
    while ((*(volatile uint32_t *)(sv + 0x00) - start_ms) < timeout_ms) {
        int c = mos_ugetc_nb();
        if (c >= 0) {
            if (c == "OK"[s_ok]) { s_ok++; if (s_ok == 2) return 1; } else s_ok = (c == 'O') ? 1 : 0;
        }
        for (volatile int i = 0; i < 30; i++);
    }
    return 0;
}

int init_esp(volatile uint8_t * sv) {
    send_esp("+++");
    wait_ms(400);
    flush_uart();

    send_esp("AT\r\n");
    if (!scan_init_ok(sv, 1500)) return 0;
    flush_uart();

    send_esp("ATE0\r\n");
    wait_ms(100);
    flush_uart();
    return 1;
}

int main(void) {
    UART settings;
    char ip_addr[BUFFER_SIZE] = "0.0.0.0";
    char mac_addr[BUFFER_SIZE] = "00:00:00:00:00:00";
    char netmask[BUFFER_SIZE] = "255.255.255.0";
    char gateway[BUFFER_SIZE] = "0.0.0.0";

    volatile uint8_t *sv = (volatile uint8_t *)mos_sysvars();

    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.stopBits = 1;
    settings.parity = 0;
    settings.flowcontrol = 0;
    settings.eir = 0;

    if (mos_uopen(&settings) != 0) {
        printf("Error: UART1 is locked.\n");
        return 0;
    }

    if (!init_esp(sv)) {
        flush_uart();
        mos_uclose();
        printf("Error: ESP module not responding.\n");
        return 0;
    }

    // --- STREAMLINED EXECUTION ---

    // 1. Query CIFSR: Reduced timeouts to 400ms. Since the stream contains both values,
    // they are captured rapidly. If one fails, it drops quickly without waiting 1 second.
    send_esp("AT+CIFSR\r\n");
    extract_token_value(sv, "+CIFSR:STAIP,\"", ip_addr, 400);
    extract_token_value(sv, "+CIFSR:STAMAC,\"", mac_addr, 400);
    flush_uart();
    wait_ms(100); // Safe breathing room for the VDP and serial layers

    // 2. Query CIPSTA
    send_esp("AT+CIPSTA?\r\n");
    extract_token_value(sv, "+CIPSTA:netmask,\"", netmask, 400);
    extract_token_value(sv, "+CIPSTA:gateway,\"", gateway, 400);
    flush_uart();

    mos_uclose();

    // Calculate broadcast IP string dynamically
    char broadcast[BUFFER_SIZE];
    char *last_dot = strrchr(ip_addr, '.');
    if (last_dot) {
        int len = last_dot - ip_addr + 1;
        strncpy(broadcast, ip_addr, len);
        broadcast[len] = '\0';
        strcat(broadcast, "255");
    } else {
        strcpy(broadcast, "0.0.0.0");
    }

    // --- REPLICATE CLASSIC LINUX IFCONFIG OUTPUT FORMAT ---
    printf("\nwlo1: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500\n");
    printf("        inet %s  netmask %s  broadcast %s\n", ip_addr, netmask, broadcast);
    printf("        ether %s  txqueuelen 1000  (Wireless Ethernet via ESP)\n", mac_addr);
    printf("        gateway %s\n\n", gateway);

    return 0;
}
