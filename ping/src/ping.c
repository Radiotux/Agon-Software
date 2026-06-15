// ping.c - PING utility for Agon with Multi-Port Fallback and metrics
// Emulates the classic operating system ping command look and feel

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <agon/mos.h>

#define BUFFER_SIZE 128
#define TOTAL_PINGS 4

// Common local network ports for cascade discovery fallback
const uint16_t fallback_ports[] = {80, 22, 443, 445, 139};
#define TOTAL_PORTS (sizeof(fallback_ports) / sizeof(fallback_ports[0]))

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

// Optimized serial analyzer: Only accepts tokens proving physical host existence.
// If an explicit network error is detected, it aborts the current port immediately.
int scan_serial_response(volatile uint8_t * sv, uint32_t timeout_ms) {
    uint32_t start_ms = *(volatile uint32_t *)(sv + 0x00);

    // State machines for valid existence responses
    int s_conn = 0, s_clsd = 0;
    // State machines for explicit failure responses (Unreachable host)
    int s_err = 0, s_fail = 0;

    while ((*(volatile uint32_t *)(sv + 0x00) - start_ms) < timeout_ms) {
        int c = mos_ugetc_nb();
        if (c >= 0) {
            // --- SUCCESS TOKENS (Host exists on the network) ---
            // Look for "CONNECT" (Open port)
            if (c == "CONNECT"[s_conn]) { s_conn++; if (s_conn == 7) return 1; } else s_conn = (c == 'C') ? 1 : 0;
            // Look for "CLOSED" (Port actively rejected by the target host)
            if (c == "CLOSED"[s_clsd]) { s_clsd++; if (s_clsd == 6) return 1; } else s_clsd = (c == 'C') ? 1 : 0;

            // --- FAILURE TOKENS (IP does not exist or is entirely dead) ---
            // Look for "ERROR"
            if (c == "ERROR"[s_err]) { s_err++; if (s_err == 5) return 0; } else s_err = (c == 'E') ? 1 : 0;
            // Look for "Fail" (e.g., "DNS Fail" or "Link Fail")
            if (c == "Fail"[s_fail]) { s_fail++; if (s_fail == 4) return 0; } else s_fail = (c == 'F') ? 1 : 0;
        }
        for (volatile int i = 0; i < 30; i++);
    }
    return 0; // Hardware timeout expired with no network activity
}

// Initialization scan: verifies the core "OK" token from the ESP firmware
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
    send_esp("AT+CIPMUX=0\r\n");
    wait_ms(100);
    send_esp("AT+CIPMODE=0\r\n");
    wait_ms(100);
    flush_uart();
    return 1;
}

int main(int argc, char * argv[]) {
    char host[BUFFER_SIZE];
    char cmd[BUFFER_SIZE];
    UART settings;

    int packets_sent = 0;
    int packets_received = 0;
    uint32_t total_time = 0;
    uint32_t min_time = 9999;
    uint32_t max_time = 0;

    if (argc < 2) {
        printf("Usage: ping <host_or_ip>\n");
        return 0;
    }

    strncpy(host, argv[1], sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';

    volatile uint8_t *sv = (volatile uint8_t *)mos_sysvars();

    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.stopBits = 1;
    settings.parity = 0;
    settings.flowcontrol = 0; // Set to 1 for the final ESP-12F hardware flow control version
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

    // Classic OS Header Look
    printf("\nPinging %s with native network emulation:\n\n", host);

    // Standard loop for the 4 execution bursts
    for (int p = 0; p < TOTAL_PINGS; p++) {
        packets_sent++;
        int success = 0;
        uint32_t elapsed = 0;

        // Cascade Port Loop: Tries alternative ports sequentially if previous ones time out
        for (int i = 0; i < TOTAL_PORTS; i++) {
            send_esp("AT+CIPCLOSE\r\n");
            flush_uart();

            snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, fallback_ports[i]);

            uint32_t start_tick = *(volatile uint32_t *)(sv + 0x00);

            send_esp(cmd);
            send_esp("\r\n");

            // Strict token tracking: discards ghost OK signals from failed commands
            if (scan_serial_response(sv, 1500)) {
                uint32_t end_tick = *(volatile uint32_t *)(sv + 0x00);
                elapsed = end_tick - start_tick;
                success = 1;
                break; // Target responded positively, exit the cascade loop
            }
        }

        if (success) {
            packets_received++;
            total_time += elapsed;
            if (elapsed < min_time) min_time = elapsed;
            if (elapsed > max_time) max_time = elapsed;

            printf("Reply from %s: bytes=4 time=%dms\n", host, (int)elapsed);
        } else {
            printf("Request timed out.\n");
        }

        send_esp("AT+CIPCLOSE\r\n");
        wait_ms(800);
        flush_uart();
    }

    mos_uclose();

    // End of execution telemetry report
    int packet_loss = ((packets_sent - packets_received) * 100) / packets_sent;
    printf("\nPing statistics for %s:\n", host);
    printf("    Packets: Sent = %d, Received = %d, Lost = %d (%d%% loss),\n",
           packets_sent, packets_received, packets_sent - packets_received, packet_loss);

    if (packets_received > 0) {
        uint32_t avg_time = total_time / packets_received;
        printf("Approximate round trip times in milli-seconds:\n");
        printf("    Minimum = %dms, Maximum = %dms, Average = %dms\n", (int)min_time, (int)max_time, (int)avg_time);
    }

    return 0;
}
