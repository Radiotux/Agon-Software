// ntpsync.c - Synchronize Agon Light's RTC with NTP using ESP-01S (standard AT firmware)
// Compile: agondevc with make
// Usage: ntpsync [utc_offset_hours]   or   ntpsync -h
// Example: ntpsync -3   (for UTC-3)
// Log written to /ntp.log

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <agon/mos.h>
#include <agon/vdp.h>

#define NTP_PACKET_SIZE 48
#define EPOCH_OFFSET 2208988800U
#define DEFAULT_NTP_SERVER "pool.ntp.org"
#define DEFAULT_UTC_OFFSET 0
#define TIMEOUT_MS 3000

FILE *logfile;

void log_msg(const char *msg) {
    if (logfile) {
        fprintf(logfile, "%s\n", msg);
        fflush(logfile);
    }
}

void log_int(const char *label, int val) {
    if (logfile) {
        fprintf(logfile, "%s: %d\n", label, val);
        fflush(logfile);
    }
}

void log_hex(const char *label, uint32_t val) {
    if (logfile) {
        fprintf(logfile, "%s: %lu (0x%lX)\n", label, val, val);
        fflush(logfile);
    }
}

// ------------------------------------------------------------------
// Help screen (safe: uses only printf, no UART, no log)
// ------------------------------------------------------------------
void show_help(void) {
    printf("ntpsync - Synchronize Agon Light RTC with NTP\n");
    printf("Usage: ntpsync [UTC_OFFSET]   or   ntpsync -h\n");
    printf("  UTC_OFFSET: integer offset in hours from UTC.\n");
    printf("  Examples:\n");
    printf("    ntpsync -3       Set RTC to UTC-3 (e.g., Chile winter)\n");
    printf("    ntpsync +2       Set RTC to UTC+2 (e.g., South Africa)\n");
    printf("    ntpsync 0        Set RTC to UTC\n");
    printf("  The program logs details to /ntp.log on the SD card.\n");
    printf("  Requires ESP-01S with standard AT firmware on UART1.\n");
}

// ------------------------------------------------------------------
// UART1 communication (ESP-01S)
// ------------------------------------------------------------------

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
}

int uart_getc_timeout(int timeout_ms) {
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        int c = mos_ugetc_nb();
        if (c >= 0) return c;
        wait_ms(1);
        elapsed++;
    }
    return -1;
}

// ------------------------------------------------------------------
// NTP packet
// ------------------------------------------------------------------

void build_ntp_packet(uint8_t *packet) {
    memset(packet, 0, NTP_PACKET_SIZE);
    packet[0] = 0x23; // LI=0, Version=4, Mode=3 (client)
}

// ------------------------------------------------------------------
// Parse +IPD response from ESP
// ------------------------------------------------------------------

int read_ntp_response(uint8_t *data, int expected_len) {
    int state = 0;
    int len = 0;
    int pos = 0;
    int c;

    while (1) {
        c = uart_getc_timeout(TIMEOUT_MS);
        if (c == -1) return -1;

        switch (state) {
            case 0: if (c == '+') state = 1; break;
            case 1: if (c == 'I') state = 2; else state = 0; break;
            case 2: if (c == 'P') state = 3; else state = 0; break;
            case 3: if (c == 'D') state = 4; else state = 0; break;
            case 4: if (c == ',') state = 5; else state = 0; break;
            case 5:
                if (c >= '0' && c <= '9') {
                    len = len * 10 + (c - '0');
                } else if (c == ':') {
                    state = 6;
                    if (len != expected_len) return -1;
                } else {
                    return -1;
                }
                break;
            case 6:
                if (pos < expected_len) {
                    data[pos++] = (uint8_t)c;
                    if (pos == expected_len) return pos;
                }
                break;
            default: return -1;
        }
    }
}

// ------------------------------------------------------------------
// Convert Unix timestamp (seconds since 1970) to date/time
// Properly handles leap years
// ------------------------------------------------------------------

void unix_to_date(uint32_t timestamp, int *year, int *month, int *day, int *hour, int *min, int *sec) {
    *sec = timestamp % 60; timestamp /= 60;
    *min = timestamp % 60; timestamp /= 60;
    *hour = timestamp % 24; timestamp /= 24;
    uint32_t days = timestamp;

    *year = 1970;
    while (1) {
        uint32_t days_in_year = ((*year % 4 == 0 && *year % 100 != 0) || *year % 400 == 0) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        (*year)++;
    }

    const uint8_t month_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = ((*year % 4 == 0 && *year % 100 != 0) || *year % 400 == 0);
    for (*month = 1; *month <= 12; (*month)++) {
        uint8_t dim = month_days[*month - 1];
        if (*month == 2 && leap) dim = 29;
        if (days < dim) break;
        days -= dim;
    }
    *day = days + 1;
}

// ------------------------------------------------------------------
// Update Agon's RTC via VDU command (using putch)
// ------------------------------------------------------------------

void update_rtc(int year, int month, int day, int hour, int minute, int second) {
    int year_adj = year - 1980;
    if (year_adj < 0) year_adj = 0;

    const uint8_t cmd[] = {
        23, 0, 0x87, 1,
        (uint8_t)year_adj,
        (uint8_t)month,
        (uint8_t)day,
        (uint8_t)hour,
        (uint8_t)minute,
        (uint8_t)second
    };
    size_t cmd_len = sizeof(cmd);

    log_msg("Executing VDU command (putch)");
    for (size_t i = 0; i < cmd_len; i++) {
        putch(cmd[i]);
        wait_ms(1);
    }
    log_msg("VDU command sent");
}

// ------------------------------------------------------------------
// Initialize ESP-01S: command mode, no echo, single connection
// ------------------------------------------------------------------

int init_esp(void) {
    log_msg("init_esp: starting");
    send_esp("+++");
    wait_ms(500);
    flush_uart();

    send_esp("AT\r\n");
    wait_ms(500);

    int ok = 0;
    for (int i = 0; i < 20; i++) {
        int c = mos_ugetc_nb();
        if (c == 'O') {
            if (mos_ugetc_nb() == 'K') {
                ok = 1;
                break;
            }
        }
        wait_ms(100);
    }
    flush_uart();
    if (!ok) {
        log_msg("init_esp: ESP not responding to AT");
        return 0;
    }
    log_msg("init_esp: AT OK");

    send_esp("ATE0\r\n");
    wait_ms(200);
    flush_uart();

    send_esp("AT+CIPMUX=0\r\n");
    wait_ms(200);
    flush_uart();

    send_esp("AT+CIPMODE=0\r\n");
    wait_ms(200);
    flush_uart();

    log_msg("init_esp: configured");
    return 1;
}

// ------------------------------------------------------------------
// Perform NTP query and update RTC
// ------------------------------------------------------------------

int query_ntp(const char *server, int utc_offset_sec) {
    uint8_t packet[NTP_PACKET_SIZE];
    uint8_t response[NTP_PACKET_SIZE];
    char cmd[128];
    int c;

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"UDP\",\"%s\",123,1234", server);
    log_msg("query_ntp: sending CIPSTART");
    send_esp(cmd);
    send_esp("\r\n");

    char resp[64];
    int pos = 0;
    int timeout = 0;
    int connected = 0;
    while (timeout < 300) {
        c = uart_getc_timeout(10);
        if (c >= 0) {
            resp[pos++] = c;
            if (pos > 60) pos = 60;
            if (c == 'C' || c == 'c') {
                connected = 1;
                break;
            }
            timeout = 0;
        } else {
            timeout++;
        }
    }
    resp[pos] = '\0';
    log_msg(resp);

    if (!connected) {
        log_msg("query_ntp: no CONNECT");
        return 0;
    }
    flush_uart();
    log_msg("query_ntp: connected");

    build_ntp_packet(packet);
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d", NTP_PACKET_SIZE);
    send_esp(cmd);
    send_esp("\r\n");
    wait_ms(200);

    int prompt = 0;
    for (int i = 0; i < 20; i++) {
        c = uart_getc_timeout(200);
        if (c == '>') {
            prompt = 1;
            break;
        }
    }
    if (!prompt) {
        log_msg("query_ntp: no prompt >");
        return 0;
    }
    flush_uart();
    log_msg("query_ntp: prompt received");

    for (int i = 0; i < NTP_PACKET_SIZE; i++) {
        mos_uputc(packet[i]);
        wait_ms(5);
    }
    log_msg("query_ntp: packet sent");

    int len = read_ntp_response(response, NTP_PACKET_SIZE);
    if (len < 0) {
        log_msg("query_ntp: error reading response");
        return 0;
    }
    log_msg("query_ntp: response received");

    send_esp("AT+CIPCLOSE\r\n");
    wait_ms(500);
    flush_uart();

    uint32_t ntp_time = ((uint32_t)response[40] << 24) |
    ((uint32_t)response[41] << 16) |
    ((uint32_t)response[42] <<  8) |
    ((uint32_t)response[43]);
    log_hex("ntp_time", ntp_time);
    if (ntp_time == 0) {
        log_msg("query_ntp: zero timestamp");
        return 0;
    }

    uint32_t unix_time = ntp_time - EPOCH_OFFSET + utc_offset_sec;
    int year, month, day, hour, minute, second;
    unix_to_date(unix_time, &year, &month, &day, &hour, &minute, &second);

    char date_buf[64];
    sprintf(date_buf, "Computed date: %04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    log_msg(date_buf);

    update_rtc(year, month, day, hour, minute, second);
    log_msg("RTC updated");
    return 1;
}

// ------------------------------------------------------------------
// Main
// ------------------------------------------------------------------

int main(int argc, char *argv[]) {
    // Show help if requested or if no arguments
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        show_help();
        return 0;
    }
    if (argc < 2) {
        show_help();
        return 0;
    }

    // Normal execution: open log, etc.
    logfile = fopen("/ntp.log", "w");
    if (!logfile) {
        return 1;
    }
    log_msg("NTPSync started");

    int utc_offset = DEFAULT_UTC_OFFSET;
    const char *server = DEFAULT_NTP_SERVER;
    if (argc >= 2) {
        utc_offset = atoi(argv[1]);
    }
    log_int("utc_offset", utc_offset);
    log_msg(server);

    UART settings;
    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.stopBits = 1;
    settings.parity = 0;
    settings.flowcontrol = 0;
    settings.eir = 0;

    if (mos_uopen(&settings) != 0) {
        log_msg("ERROR: Cannot open UART1");
        fclose(logfile);
        return 1;
    }
    log_msg("UART1 opened");

    if (!init_esp()) {
        log_msg("init_esp failed");
        mos_uclose();
        fclose(logfile);
        return 1;
    }

    int result = query_ntp(server, utc_offset * 3600);
    mos_uclose();

    if (result == 1) {
        log_msg("Success: NTP time obtained and RTC updated");
        fclose(logfile);
        return 0;
    } else {
        log_msg("Failure: could not obtain NTP time");
        fclose(logfile);
        return 1;
    }
}
