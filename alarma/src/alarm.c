#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <agon/mos.h>

#define CMD_SIZE 64
#define STR_SIZE 32
#define RTC_SIZE 128  // FIXED: Increased from 64 to prevent stack overflow

// --- Core VDU Sound Functions (VDP Control) ---
void send_vdu(const char *cmd) {
    char *argv_vdu[] = { NULL };
    mos_oscli((char *)cmd, argv_vdu, 0);
}

void activate_channel(int volume) {
    char cmd_buffer[CMD_SIZE];
    snprintf(cmd_buffer, CMD_SIZE, "VDU 23 0 133 0 2 %d", volume);
    send_vdu(cmd_buffer);
}

void set_frequency(int frequency) {
    char cmd_buffer[CMD_SIZE];
    snprintf(cmd_buffer, CMD_SIZE, "VDU 23 0 133 0 3 &%X", frequency);
    send_vdu(cmd_buffer);
}

void silence(void) {
    send_vdu("VDU 23 0 133 0 2 0");
}

// --- Help Screen ---
void show_help(void) {
    printf("Agon Light Alarm Clock Utility\n");
    printf("Usage: alarm [hours] [minutes]\n");
    printf("Note : Time parameters must be separated by spaces, NOT colons.\n");
    printf("Example: alarm 07 30\n\n");
    printf("Options:\n");
    printf("  -h   Show this help message\n");
}

// --- Input Validation Function ---
// FIXED: Robust time parsing with proper error handling
int parse_time_input(const char *str, int max_val, int *value) {
    if (!str || !*str) {
        return -1;
    }
    
    // Validate that string contains only digits
    for (const char *p = str; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            return -1;
        }
    }
    
    // Convert to integer
    char *endptr;
    long val = strtol(str, &endptr, 10);
    
    // Validate range
    if (val < 0 || val > max_val) {
        return -1;
    }
    
    *value = (int)val;
    return 0;
}

// --- Main Application ---
int main(int argc, char *argv[]) {
    char alarm_str[STR_SIZE];
    char rtc_buffer[RTC_SIZE];
    volatile int audio_timer;

    char last_sec_char = '\0';
    int rtc_len = 0;
    int hours = 0;
    int minutes = 0;

    // FIXED: Explicit array indices to avoid compiler validation bypass
    if (argc >= 2 && strcmp(argv[1], "-h") == 0) {
        show_help();
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "Error: Missing arguments.\n");
        show_help();
        return 1;
    }

    // FIXED: Strict validation of hour (argv[1]) and minutes (argv[2])
    if (parse_time_input(argv[1], 23, &hours) != 0) {
        fprintf(stderr, "Error: Invalid hours. Must be 0-23.\n");
        show_help();
        return 1;
    }

    if (parse_time_input(argv[2], 59, &minutes) != 0) {
        fprintf(stderr, "Error: Invalid minutes. Must be 0-59.\n");
        show_help();
        return 1;
    }

    // FIXED: Strict formatting with validated hour and minute values
    snprintf(alarm_str, STR_SIZE, " %02d:%02d:", hours, minutes);

    printf("Alarm set for %02d:%02d. Monitoring RTC...\n", hours, minutes);
    printf("Press 'Q' (or any key) at any time to cancel and exit.\n\n");

    // Capture the initial keyboard state BEFORE starting the main loop
    uint8_t exit_key_count = sys_vars->vkeycount;

    // Main real-time clock monitoring loop
    while (1) {
        // Check if the user wants to abort the program while waiting for the alarm
        if (sys_vars->vkeycount != exit_key_count) {
            printf("\nAlarm monitoring canceled by user.\n");
            break;
        }

        // Fetch current RTC string directly from the hardware
        mos_getrtc(rtc_buffer);
        rtc_len = strlen(rtc_buffer);

        // Standard Agon MOS RTC strings end with the seconds digits.
        if (rtc_len > 0 && rtc_buffer[rtc_len - 1] != last_sec_char) {
            last_sec_char = rtc_buffer[rtc_len - 1]; // Catch the new second step

            // Prints the running clock with absolute 1-second precision
            printf("\rCurrent Time: %s", rtc_buffer);
            fflush(stdout);

            // Scan the RTC string for our isolated time substring
            if (strstr(rtc_buffer, alarm_str) != NULL) {
                printf("\n\n*** ALARM REACHED ***\n");
                printf("Press 'Q' to turn off the alarm.\n");

                // Turn on target sound channel
                activate_channel(100);

                // Snapshot the virtual keyboard interrupt counter for the audio loop
                uint8_t initial_key_count = sys_vars->vkeycount;

                // Autonomous bitonal audio loop
                while (1) {
                    if (sys_vars->vkeycount != initial_key_count) {
                        break;
                    }

                    // ==================== TONE 1: HIGH (880 Hz) ====================
                    set_frequency(880);
                    for (audio_timer = 0; audio_timer < 350000; audio_timer++) { }

                    if (sys_vars->vkeycount != initial_key_count) {
                        break;
                    }

                    // ==================== TONE 2: MEDIUM (660 Hz) ====================
                    set_frequency(660);
                    for (audio_timer = 0; audio_timer < 350000; audio_timer++) { }
                }
                break;
            }
        }
    }

    // Kill the audio wave completely on exit
    silence();
    printf("Returning to MOS.\n");
    return 0;
}
