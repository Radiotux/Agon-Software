// ping.c - Comando PING definitivo para Agon Light 2 con Multi-Port Fallback y métricas
// Diseñado para emular el comportamiento clásico del sistema operativo

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <agon/mos.h>

#define BUFFER_SIZE 128
#define TOTAL_PINGS 4

// Puertos comunes en redes locales para el parche de descubrimiento en cascada
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

// Analizador serie corregido: Solo acepta tokens que demuestren existencia física del host.
// Si detecta un error de red explícito, descarta el intento inmediatamente.
int scan_serial_response(volatile uint8_t * sv, uint32_t timeout_ms) {
    uint32_t start_ms = *(volatile uint32_t *)(sv + 0x00);

    // Máquinas de estado para respuestas válidas de existencia
    int s_conn = 0, s_clsd = 0;
    // Máquinas de estado para respuestas explícitas de fallo (Host inexistente)
    int s_err = 0, s_fail = 0;

    while ((*(volatile uint32_t *)(sv + 0x00) - start_ms) < timeout_ms) {
        int c = mos_ugetc_nb();
        if (c >= 0) {
            // --- TOKENS DE ÉXITO (El host existe en la red) ---
            // Buscar "CONNECT" (Puerto abierto)
            if (c == "CONNECT"[s_conn]) { s_conn++; if (s_conn == 7) return 1; } else s_conn = (c == 'C') ? 1 : 0;
            // Buscar "CLOSED" (Puerto cerrado de forma activa por el host)
            if (c == "CLOSED"[s_clsd]) { s_clsd++; if (s_clsd == 6) return 1; } else s_clsd = (c == 'C') ? 1 : 0;

            // --- TOKENS DE FRACASO (La IP no existe o es inalcanzable) ---
            // Buscar "ERROR"
            if (c == "ERROR"[s_err]) { s_err++; if (s_err == 5) return 0; } else s_err = (c == 'E') ? 1 : 0;
            // Buscar "Fail" (ej. "DNS Fail" o "Link Fail")
            if (c == "Fail"[s_fail]) { s_fail++; if (s_fail == 4) return 0; } else s_fail = (c == 'F') ? 1 : 0;
        }
        for (volatile int i = 0; i < 30; i++);
    }
    return 0; // Se agotó el tiempo de espera sin respuestas
}

// Función exclusiva para el encendido: aquí sí nos interesa el "OK" básico de confirmación del chip
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
        printf("Uso: ping <host_o_ip>\n");
        return 0;
    }

    // Asignación limpia del argumento del host
    strncpy(host, argv[1], sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';

    volatile uint8_t *sv = (volatile uint8_t *)mos_sysvars();

    settings.baudRate = 115200;
    settings.dataBits = 8;
    settings.stopBits = 1;
    settings.parity = 0;
    settings.flowcontrol = 0;
    settings.eir = 0;

    if (mos_uopen(&settings) != 0) {
        printf("Error: UART1 bloqueada.\n");
        return 0;
    }

    if (!init_esp(sv)) {
        flush_uart();
        mos_uclose();
        printf("Error: El modulo ESP no responde.\n");
        return 0;
    }

    // Encabezado clásico
    printf("\nHaciendo ping a %s con emulacion de red nativa:\n\n", host);

    // Bucle para realizar los 4 pings reglamentarios
    for (int p = 0; p < TOTAL_PINGS; p++) {
        packets_sent++;
        int success = 0;
        uint32_t elapsed = 0;

        // Bucle de puertos en cascada: intenta uno por uno si el anterior falla
        for (int i = 0; i < TOTAL_PORTS; i++) {
            send_esp("AT+CIPCLOSE\r\n");
            flush_uart();

            snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", host, fallback_ports[i]);

            uint32_t start_tick = *(volatile uint32_t *)(sv + 0x00);

            send_esp(cmd);
            send_esp("\r\n");

            // scan_serial_response ahora es estricta: ignora el OK espurio de comandos fallidos
            if (scan_serial_response(sv, 1500)) {
                uint32_t end_tick = *(volatile uint32_t *)(sv + 0x00);
                elapsed = end_tick - start_tick;
                success = 1;
                break; // El host respondió positivamente, rompemos la cascada
            }
        }

        if (success) {
            packets_received++;
            total_time += elapsed;
            if (elapsed < min_time) min_time = elapsed;
            if (elapsed > max_time) max_time = elapsed;

            printf("Respuesta desde %s: bytes=4 tiempo=%dms\n", host, (int)elapsed);
        } else {
            printf("Tiempo de espera agotado para esta solicitud.\n");
        }

        send_esp("AT+CIPCLOSE\r\n");
        wait_ms(800);
        flush_uart();
    }

    mos_uclose();

    int packet_loss = ((packets_sent - packets_received) * 100) / packets_sent;
    printf("\nEstadisticas de ping para %s:\n", host);
    printf("    Paquetes: enviados = %d, recibidos = %d, perdidos = %d (%d%% de perdida),\n",
           packets_sent, packets_received, packets_sent - packets_received, packet_loss);

    if (packets_received > 0) {
        uint32_t avg_time = total_time / packets_received;
        printf("Tiempos aproximados de ida y vuelta en milisegundos:\n");
        printf("    Minimo = %dms, Maximo = %dms, Media = %dms\n", (int)min_time, (int)max_time, (int)avg_time);
    }

    return 0;
}
