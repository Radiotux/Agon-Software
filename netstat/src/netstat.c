// netstat.c - Native Network Statistics utility for Agon Light 2
// Strictly based on the non-blocking architecture of Radiotux's ifconfig.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <agon/mos.h>

#define RX_BUF_SIZE 1024
#define LINE_SIZE 128

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

void parse_and_display_status(char *buffer) {
    char *line = strtok(buffer, "\n");
    int connections_found = 0;

    printf("\nAgon Active Internet Connections (Sockets)\n");
    printf("====================================================================\n");
    printf("%-5s %-8s %-22s %-12s %-10s\n", "ID", "Proto", "Remote Address", "Remote Port", "State");
    printf("--------------------------------------------------------------------\n");

    while (line != NULL) {
        // Look for the +CIPSTATUS token in the current line
        char *status_ptr = strstr(line, "+CIPSTATUS:");
        if (status_ptr != NULL) {
            int id = 0;
            char proto[8];
            char remote_ip[32];
            int remote_port = 0;
            int local_port = 0;
            int type = 0;

            // Clear buffers safely
            memset(proto, 0, sizeof(proto));
            memset(remote_ip, 0, sizeof(remote_ip));

            // Parse the comma-separated parameters matching ESP-01S formatting
            // Format: +CIPSTATUS:id,"type","remote_ip",remote_port,local_port,type
            if (sscanf(status_ptr, "+CIPSTATUS:%d,\"%[^\"]\",\"%[^\"]\",%d,%d,%d",
                &id, proto, remote_ip, &remote_port, &local_port, &type) >= 4) {

                char remote_addr[64];
            sprintf(remote_addr, "%s", remote_ip);

            printf("%-5d %-8s %-22s %-12d %-10s\n", id, proto, remote_addr, remote_port, "ESTABLISHED");
            connections_found++;
                }
        }
        line = strtok(NULL, "\n");
    }

    if (connections_found == 0) {
        printf("(No active network connections detected)\n");
    }
    printf("====================================================================\n");
}

int main(void) {
    UART settings;
    char rx_buffer[RX_BUF_SIZE];
    int idx = 0;
    uint32_t inactivity = 0;
    volatile uint8_t *sv = (volatile uint8_t *)mos_sysvars();

    memset(rx_buffer, 0, RX_BUF_SIZE);

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

    // Query active connection status
    send_esp("AT+CIPSTATUS\r\n");

    // Asynchronous burst capture loop utilizing the non-blocking receiver
    uint32_t start_time = *(volatile uint32_t *)(sv + 0x00);
    // Allow up to 1000ms total for the ESP to dump the status matrix
    while (idx < (RX_BUF_SIZE - 1) && (*(volatile uint32_t *)(sv + 0x00) - start_time) < 1000) {
        int c = mos_ugetc_nb();
        if (c >= 0) {
            inactivity = 0;
            if ((char)c >= 32 || (char)c == '\r' || (char)c == '\n') {
                rx_buffer[idx++] = (char)c;
            }
            // Early break if the final trailing 'OK\r\n' is found
            if (idx >= 4 && strstr(rx_buffer, "OK\r\n") != NULL) {
                break;
            }
        } else {
            inactivity++;
            // Safety escape if the line clears completely for too long
            if (inactivity > 60000) {
                break;
            }
        }
    }
    rx_buffer[idx] = '\0';

    flush_uart();
    mos_uclose();

    if (idx == 0) {
        printf("Error: Timeout waiting for ESP-01S status response.\n");
        return 1;
    }

    // Pass the raw memory pool to our safe parsing logic
    parse_and_display_status(rx_buffer);

    return 0;
}
