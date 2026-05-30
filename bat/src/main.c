#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Formal declaration to inform the compiler that getch() is available natively
extern int getch(void);
extern void putch(char c); // Envía bytes puros al VDP sin filtros ni interpretaciones de texto de stdio

#define LINE_BUFFER 512
#define MAX_WIDTH 70
#define PAGE_LINES 30 // Congela la salida exactamente en la línea 30 para evitar scroll involuntario

// --- CONSTANTES VDU NATIVAS DE AGON LIGHT ---
#define VDU_TEXT_COL 17
#define COL_GRAY 8
#define COL_RED 9
#define COL_GREEN 10
#define COL_CYAN 11
#define COL_YELLOW 14
#define COL_WHITE 15

// Macro nativa ultra-estable usando putch() para evitar conflictos de diagonales con el byte 10 (\n)
#define SET_COLOR(c) { putch(VDU_TEXT_COL); putch((c)); }

// Modos de detección de archivo
#define MODE_C 0
#define MODE_ASM 1
#define MODE_BASIC 2

// --- LISTAS DE PALABRAS CLAVE ---
const char *keywords_c[] = {
	"void", "int", "char", "return", "if", "while", "for", "else", "const", "struct",
	"#include", "#define", "FILE", "printf", "fopen", "fclose", "fgets", "putchar", NULL
};

// CORREGIDO: Añadidas directivas comunes con punto para compatibilidad total con listados eZ80
const char *keywords_asm[] = {
	"ld", "xor", "inc", "dec", "jp", "call", "org", "equ", "ret", "nop",
	"add", "sub", "cp", "push", "pop", "db", "dw", "ds", "macro", "endm",
	".assume", ".org", ".db", ".dw", ".ds", "moscall", NULL
};

const char *keywords_basic[] = {
	"PRINT", "INPUT", "LET", "IF", "THEN", "ELSE", "GOTO", "GOSUB", "RETURN",
	"FOR", "NEXT", "END", "PROC", "DEF", "LOCAL", "CLS", "MODE", "VDU", NULL
};

// Operadores de control comunes
const char *operators[] = {"==", "!=", "&&", "||", ">=", "<=", NULL};

// NUEVA FUNCIÓN AUXILIAR: Busca una palabra dentro de una línea ignorando mayúsculas y minúsculas
const char *strcasestr_custom(const char *haystack, const char *needle) {
	if (!haystack || !needle) return NULL;
	size_t needle_len = strlen(needle);
	if (needle_len == 0) return haystack;

	for (const char *h = haystack; *h != '\0'; h++) {
		if (strncasecmp(h, needle, needle_len) == 0) {
			return h;
		}
	}
	return NULL;
}

void process_and_print_line(char *line, int file_mode, int line_num) {
	int length = strlen(line);

	// Limpieza estricta de finales de línea (\n y \r) del buffer
	while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
		line[length - 1] = '\0';
		length--;
	}

	char *useful_text = line;

	// Lógica de omisión dinámica exclusiva para listados de TPASM
	if (file_mode == MODE_ASM && length > 30) {
		int has_real_content = 0;
		for (int i = 0; line[i] != '\0'; i++) {
			if (line[i] != ' ' && line[i] != '\t' && line[i] != '0') {
				has_real_content = 1;
				break;
			}
		}
		if (!has_real_content) useful_text = "";
	}

	if (strlen(useful_text) > MAX_WIDTH) {
		useful_text[MAX_WIDTH] = '\0';
	}

	// 1. IMPRIMIR NÚMERO DE LÍNEA (Gris)
	SET_COLOR(COL_GRAY);
	printf("%4d | ", line_num);
	SET_COLOR(COL_WHITE);

	// 2. MOTOR DE RESALTADO DINÁMICO DE SINTAXIS
	if (strlen(useful_text) > 0) {
		char *comment = NULL;
		int comment_prefix_len = 0;

		if (file_mode == MODE_C) {
			comment = strstr(useful_text, "//");
			comment_prefix_len = 2;
		} else if (file_mode == MODE_ASM) {
			comment = strstr(useful_text, ";");
			comment_prefix_len = 1;
		} else if (file_mode == MODE_BASIC) {
			comment = strstr(useful_text, "REM");
			comment_prefix_len = 3;
		}

		if (comment != NULL) {
			*comment = '\0';
			int in_string = 0;
			for (int i = 0; useful_text[i] != '\0'; i++) {
				if (useful_text[i] == '"') {
					if (!in_string) {
						SET_COLOR(COL_GREEN);
						in_string = 1;
					} else {
						putchar(useful_text[i]);
						SET_COLOR(COL_WHITE);
						in_string = 0;
						continue;
					}
				}

				// Buscar operadores incluso dentro de las cadenas en líneas con comentarios
				int is_operator = 0;
				for (int o = 0; operators[o] != NULL; o++) {
					int op_len = strlen(operators[o]);
					if (strncmp(&useful_text[i], operators[o], op_len) == 0) {
						SET_COLOR(COL_CYAN); // Operador en Cian/Morado
						printf("%s", operators[o]);
						SET_COLOR(in_string ? COL_GREEN : COL_WHITE); // Retorna a verde si sigue dentro de la cadena
						i += (op_len - 1);
						is_operator = 1;
						break;
					}
				}
				if (is_operator) continue;

				putchar(useful_text[i]);
			}
			if (in_string) SET_COLOR(COL_WHITE);
			SET_COLOR(COL_RED);
			if (file_mode == MODE_C) printf("//%s", comment + comment_prefix_len);
				else if (file_mode == MODE_ASM) printf(";%s", comment + comment_prefix_len);
				else if (file_mode == MODE_BASIC) printf("REM%s", comment + comment_prefix_len);
				SET_COLOR(COL_WHITE);
		}
		else {
			int line_color = COL_WHITE;
			if (file_mode == MODE_C) {
				for (int i = 0; keywords_c[i] != NULL; i++) {
					if (strcasestr_custom(useful_text, keywords_c[i]) != NULL) { line_color = COL_YELLOW; break; }
				}
			} else if (file_mode == MODE_ASM) {
				// CORREGIDO: Ahora busca ignorando si el código asm está en mayúsculas o minúsculas
				for (int i = 0; keywords_asm[i] != NULL; i++) {
					if (strcasestr_custom(useful_text, keywords_asm[i]) != NULL) { line_color = COL_YELLOW; break; }
				}
			} else if (file_mode == MODE_BASIC) {
				for (int i = 0; keywords_basic[i] != NULL; i++) {
					if (strcasestr_custom(useful_text, keywords_basic[i]) != NULL) { line_color = COL_YELLOW; break; }
				}
			}

			SET_COLOR(line_color);
			int in_string = 0;
			for (int i = 0; useful_text[i] != '\0'; i++) {
				if (useful_text[i] == '"') {
					if (!in_string) {
						SET_COLOR(COL_GREEN); // Abrir cadena en verde
						in_string = 1;
					} else {
						putchar(useful_text[i]);
						SET_COLOR(line_color); // Restaurar al color base de la línea
						in_string = 0;
						continue;
					}
				}

				// Buscar operadores siempre, dándoles prioridad cian incluso dentro de comillas
				int is_operator = 0;
				for (int o = 0; operators[o] != NULL; o++) {
					int op_len = strlen(operators[o]);
					if (strncmp(&useful_text[i], operators[o], op_len) == 0) {
						SET_COLOR(COL_CYAN); // Operador en Cian/Morado
						printf("%s", operators[o]);
						SET_COLOR(in_string ? COL_GREEN : line_color); // Retorna a verde si sigue dentro de la cadena
						i += (op_len - 1);
						is_operator = 1;
						break;
					}
				}
				if (is_operator) continue;

				putchar(useful_text[i]);
			}
			SET_COLOR(COL_WHITE);
		}
	}
	printf("\n\r");
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Usage: %s <filename>\n\r", argv[0]);
		return 1;
	}

	FILE *file_handle = fopen(argv[1], "r");
	if (!file_handle) {
		printf("Error: Could not open file '%s'\n\r", argv[1]);
		return 1;
	}

	SET_COLOR(COL_CYAN);
	printf("+---[ File: %s ]-----------------------------------\n\r", argv[1]);
	SET_COLOR(COL_WHITE);

	char line[LINE_BUFFER];
	int line_num = 1;
	int screen_line_count = 1;
	int file_mode = MODE_C;

	// --- DETECCIÓN POR EXTENSIÓN DE ARCHIVO (BBC BASIC / ASSEMBLER) ---
	int name_len = strlen(argv[1]);
	if (name_len > 4) {
		char *ext = argv[1] + name_len - 4;
		if (strcasecmp(ext, ".bas") == 0 || strcasecmp(ext, ".bbc") == 0) {
			file_mode = MODE_BASIC;
		} else if (strcasecmp(ext, ".asm") == 0 || strcasecmp(ext, ".s") == 0) {
			file_mode = MODE_ASM; // Fuerza modo ASM si detecta la extensión de forma explícita
		}
	}

	// --- DETECCIÓN POR CONTENIDO (Listados TPASM) ---
	if (fgets(line, sizeof(line), file_handle) != NULL) {
		if (strncmp(line, "tpasm", 5) == 0) {
			file_mode = MODE_ASM;
		}
		process_and_print_line(line, file_mode, line_num);
		line_num++;
		screen_line_count++;
	}

	// BUCLE PRINCIPAL DE PROCESAMIENTO
	while (fgets(line, sizeof(line), file_handle) != NULL) {
		process_and_print_line(line, file_mode, line_num);
		line_num++;
		screen_line_count++;

		// Control de congelamiento de página al llegar a la línea 30
		if (screen_line_count >= PAGE_LINES) {
			fflush(stdout);

			// Usamos getchar() estándar para asegurar compatibilidad total del teclado en el emulador
			int input_key = getchar();

			if (input_key == 'q' || input_key == 'Q' || input_key == 113 || input_key == 81) {
				SET_COLOR(COL_CYAN);
				printf("+---[ Viewer closed by user ]----------------------\n\r");
				SET_COLOR(COL_WHITE);
				fclose(file_handle);
				return 0;

			}screen_line_count = 0;

		}}SET_COLOR(COL_CYAN);
		printf("+---[ End of file ]---------------------------------\n\r");
		SET_COLOR(COL_WHITE);
		fclose(file_handle);
		return 0;}
