#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <agon/mos.h>

#define MODE_TEXT 0
#define MODE_ASM  1
#define MODE_C    2
#define MODE_BAS  3

#define PAGE_LINES 26     // Max vertical text density optimized for Mode 27
#define MAX_PAGES 200

// Static page offset table calculated at startup
uint32_t page_positions[MAX_PAGES];
int current_page = 0;
int line_number = 1;
int active_lines = 0;
int col_count = 0;

int total_lineas_archivo = 0;
int ancho_texto_rejilla = 4;
int coordenada_x_grafica = 64;

// Sends a 16-bit integer in Little-Endian format to the VDP graphics processor
void vdu_enviar_int16(int valor) {
	putch(valor & 0xFF);
	putch((valor >> 8) & 0xFF);
}

void set_color_texto(int c) {
	putch(17);
	putch(c);
}

void set_color_grafico(uint8_t color_id) {
	putch(18);
	putch(0);
	putch(color_id);
}

void salto_de_linea() {
	putch(10); // Line Feed
	putch(13); // Carriage Return
}

// Resets terminal hardware, forces Mode 27, and activates background transparency
void inicializar_pagina_vdp() {
	printf("\x0C"); // Unified text/graphics CLS in Agon MOS
	putch(22);      // VDU 22: Change video mode
	putch(27);      // Native Mode 27 (640x256, 64 colors, 1280x1024 virtual grid)

	// VDU 23, 0, 14, 1: Activates text background transparency in the VDP
	// This absolutely prevents opaque text cells from overwriting the vector graphics
	putch(23); putch(0); putch(14); putch(1);
}

// Renders the file header consuming exactly two physical text lines
void dibujar_cabecera_texto(const char *filename) {
	set_color_texto(2); // Light Green
	putch('F'); putch('i'); putch('l'); putch('e'); putch(':'); putch(' ');
	for (int i = 0; filename[i] != '\0'; i++) {
		putch(filename[i]);
	}
	salto_de_linea(); // Row 0 completed
	salto_de_linea(); // Row 1 completed (Leaves room to isolate the horizontal vector)
}

// Renders the high-resolution vector frame. Drawn over the text layout.
void dibujar_interfaz_grafica_vdp() {
	set_color_grafico(7); // Light Grey/White for the frame layout

	// 1. Top Horizontal Vector Line: Joins point (0, 960) with point (1279, 960)
	putch(25); putch(4);  // VDU 25,4 -> Move graphics cursor absolute without drawing
	vdu_enviar_int16(0);
	vdu_enviar_int16(960);

	putch(25); putch(5);  // VDU 25,5 -> Draw straight line ABSOLUTE to target coordinates
	vdu_enviar_int16(1279);
	vdu_enviar_int16(960);

	// 2. Left Vertical Vector Line: Joins top intersection (coordenada_x_grafica, 960) with floor (coordenada_x_grafica, 0)
	putch(25); putch(4);
	vdu_enviar_int16(coordenada_x_grafica);
	vdu_enviar_int16(960);

	putch(25); putch(5);
	vdu_enviar_int16(coordenada_x_grafica);
	vdu_enviar_int16(0);
}

// Formats the line numbers utilizing the dynamically allocated text columns
void imprimir_rejilla_lateral() {
	set_color_texto(7);

	int millar  = (line_number / 1000) % 10;
	int centena = (line_number / 100) % 10;
	int decena  = (line_number / 10) % 10;
	int unidad  = line_number % 10;

	// Expand to 4 digits if the file has 1000+ total lines
	if (total_lineas_archivo >= 1000) {
		if (line_number >= 1000) putch('0' + millar);
		else putch(' ');
	}

	if (line_number >= 100) putch('0' + centena); else putch(' ');
	if (line_number >= 10) putch('0' + decena); else putch(' ');
	putch('0' + unidad);

	putch(32); // Space separator. The vertical vector line will align flush right to this cell.
	line_number++;
}

// Safe indent tracking: injects raw space bytes to shift wrapped text without breaking the state machine
void imprimir_sangrado_desbordo() {
	int espacios = ancho_texto_rejilla + 2;
	for(int i = 0; i < espacios; i++) {
		putch(32);
	}
}

// Passive character dispatcher: tracks margins and triggers clean text wraps
void emit_char(char c) {
	int max_columnas = 80 - (ancho_texto_rejilla + 2);
	if (col_count >= max_columnas) {
		salto_de_linea();
		active_lines++; // Informs the layout engine that a physical text line has been consumed
		imprimir_sangrado_desbordo();
		col_count = 0;
	}
	putch(c);
	col_count++;
}
int main(int argc, char *argv[]) {
	FILE *fp;
	char ch;
	int file_mode = MODE_TEXT;
	int in_comment = 0;
	int in_string = 0;
	char word[256]; // Dynamic word parsing buffer
	int word_idx = 0;
	int line_start_asm = 0;
	int useful_asm = 0;
	int temp_lines = 2; // Match initial layout row offset (Rows 0 and 1 occupied)

	if (argc < 2) {
		printf("Usage: bat <filename>\n\r");
		return 1;
	}

	// Automatic extension matching engine
	if (strstr(argv[1], ".c") != NULL || strstr(argv[1], ".h") != NULL) {
		file_mode = MODE_C;
	} else if (strstr(argv[1], ".asm") != NULL || strstr(argv[1], ".s") != NULL || strstr(argv[1], ".lst") != NULL) {
		file_mode = MODE_ASM;
	} else if (strstr(argv[1], ".bas") != NULL) {
		file_mode = MODE_BAS;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		printf("Error: Could not open file.\n\r");
		return 1;
	}

	// Clear static page tracking layout array
	memset(page_positions, 0, sizeof(page_positions));
	page_positions[0] = 0; // Page 0 always points to byte 0 of the stream
	int scan_page = 0;

	// --- HIGH SPEED PRE-SCAN HARDWARE COUNTER AND STATIC PAGE INDEXER ---
	total_lineas_archivo = 0;
	int scan_col_count = 0;
	int max_cols = 80 - 6; // Safe horizontal wrap margin estimation

	while ((ch = fgetc(fp)) != EOF) {
		if (ch == '\n') {
			total_lineas_archivo++;
			temp_lines++;
			scan_col_count = 0;

			if (temp_lines >= PAGE_LINES) {
				scan_page++;
				if (scan_page < MAX_PAGES) {
					page_positions[scan_page] = ftell(fp); // Log absolute page boundary offset
				}
				temp_lines = 2;
			}
		} else if (ch != '\r') {
			scan_col_count++;
			if (scan_col_count >= max_cols) {
				temp_lines++; // Account for forced physical line wraps on long lines
				scan_col_count = 0;
				if (temp_lines >= PAGE_LINES) {
					scan_page++;
					if (scan_page < MAX_PAGES) {
						page_positions[scan_page] = ftell(fp);
					}
					temp_lines = 2;
				}
			}
		}
	}

	// Rewind file to byte 0 using AgonDev standard native pointer seek
	fseek(fp, 0, SEEK_SET);

	// Dynamic grid scaling based on file magnitude
	if (total_lineas_archivo >= 1000) {
		ancho_texto_rejilla = 5;   // 4 digits + 1 space delimiter
		coordenada_x_grafica = 80; // Shift vertical vector line to text column 5
	} else {
		ancho_texto_rejilla = 4;   // 3 digits + 1 space delimiter
		coordenada_x_grafica = 64; // Keep vertical vector line at text column 4
	}

	// Initialize execution environment runtime states
	current_page = 0;
	line_number = 1;
	active_lines = 0;
	col_count = 0;

	memset(word, 0, sizeof(word));

	// Initialize layout for the first page
	inicializar_pagina_vdp();
	dibujar_cabecera_texto(argv[1]);
	active_lines = 2;
	imprimir_rejilla_lateral();
	putch(32); putch(32); // Inject padding airway channel next to the grid

	// Main character parsing loop
	while ((ch = fgetc(fp)) != EOF) {
		int char_procesado = 0;
		// Interactive pagination control synchronized on-the-fly with active_lines
		if (active_lines >= PAGE_LINES) {
			dibujar_interfaz_grafica_vdp();

			int key = getch();

			// 'q' or 'Q' -> Clear screen and exit to MOS prompt
			if (key == 'q' || key == 'Q' || key == 113 || key == 81) {
				printf("\x0C"); // Native hardware CLS upon keyboard exit
				set_color_texto(7);
				fclose(fp);
				salto_de_linea();
				return 0;
			}
			// 'b' or 'B' -> Backward one full page via static offset lookup table
			else if (key == 'b' || key == 'B' || key == 98 || key == 66) {
				if (current_page > 0) {
					current_page--;
				}
				fseek(fp, page_positions[current_page], SEEK_SET);

				// Recalculate base line_number by multiplying real useful lines per page
				line_number = (current_page * (PAGE_LINES - 2)) + 1;

				active_lines = 0;
				col_count = 0;
				word_idx = 0;
				memset(word, 0, sizeof(word));
				in_comment = 0; in_string = 0; line_start_asm = 0; useful_asm = 0;

				inicializar_pagina_vdp();
				dibujar_cabecera_texto(argv[1]);
				active_lines = 2;
				imprimir_rejilla_lateral();
				putch(32); putch(32);
				continue; // Force immediate re-render of the previous page stream
			}
			// Space or any other keyboard input -> Forward one page
			else {
				current_page++;
			}

			active_lines = 0;
			col_count = 0;
			word_idx = 0;
			memset(word, 0, sizeof(word));
			in_comment = 0; in_string = 0; line_start_asm = 0; useful_asm = 0;

			inicializar_pagina_vdp();
			dibujar_cabecera_texto(argv[1]);
			active_lines = 2;
			imprimir_rejilla_lateral();
			putch(32); putch(32);

			if (ch == '\n') {
				salto_de_linea();
				col_count = 0;
				imprimir_rejilla_lateral();
				putch(32); putch(32);
				continue;
			}
		}

		if (ch == '\n') {
			if (word_idx > 0) {
				word[word_idx] = '\0';
				for (int i = 0; word[i] != '\0'; i++) emit_char(word[i]);
				word_idx = 0;
				memset(word, 0, sizeof(word));
			}

			salto_de_linea();
			col_count = 0;
			in_comment = 0;
			in_string = 0;
			line_start_asm = 0;
			useful_asm = 0;
			active_lines++;

			if (active_lines < PAGE_LINES) {
				imprimir_rejilla_lateral();
				putch(32); putch(32);
			}
			continue;
		}

		if (ch == '\r') continue;

		// --- LANGUAGE HIGHLIGHTING ENGINE AND TOKEN DISPATCHER ---
		if (file_mode == MODE_ASM) {
			line_start_asm++;
			if (line_start_asm <= 30) {
				set_color_texto(1); // Dark Grey for assembler compilation metadata
				emit_char(ch);
				continue;
			} else {
				if (line_start_asm == 31) set_color_texto(7);
				if (ch != ' ' && ch != '\t' && useful_asm == 0) useful_asm = 1;
			}
		}

		if (in_comment) {
			emit_char(ch);
			continue;
		}

		if (in_string) {
			emit_char(ch);
			if (ch == '"') in_string = 0;
			continue;
		}

		if (file_mode == MODE_C && ch == '/' && !in_string) {
			char next = fgetc(fp);
			if (next == '/') {
				if (word_idx > 0) {
					word[word_idx] = '\0';
					for (int i = 0; word[i] != '\0'; i++) emit_char(word[i]);
					word_idx = 0;
					memset(word, 0, sizeof(word));
				}
				set_color_texto(1); // Grey for comments
				emit_char('/'); emit_char('/');
				in_comment = 1;
				continue;
			} else {
				ungetc(next, fp);
			}
		}

		if (file_mode == MODE_ASM && (ch == ';' || ch == '@') && !in_string) {
			if (word_idx > 0) {
				word[word_idx] = '\0';
				for (int i = 0; word[i] != '\0'; i++) emit_char(word[i]);
				word_idx = 0;
				memset(word, 0, sizeof(word));
			}
			set_color_texto(1);
			emit_char(ch);
			in_comment = 1;
			continue;
		}

		if (ch == '"' && !in_comment) {
			if (word_idx > 0) {
				word[word_idx] = '\0';
				for (int i = 0; word[i] != '\0'; i++) emit_char(word[i]);
				word_idx = 0;
				memset(word, 0, sizeof(word));
			}
			set_color_texto(3); // Cyan for literal strings
			emit_char(ch);
			in_string = 1;
			continue;
		}

		// Lexical token string builder extraction
		if (isalnum((unsigned char)ch) || ch == '_' || (file_mode == MODE_C && ch == '#')) {
			if (word_idx < 255) {
				word[word_idx++] = ch;
			}
		} else {
			if (word_idx > 0) {
				word[word_idx] = '\0';

				// C Language Keyword Evaluator
				if (file_mode == MODE_C) {
					if (strcmp(word, "int") == 0 || strcmp(word, "char") == 0 || strcmp(word, "void") == 0 ||
						strcmp(word, "return") == 0 || strcmp(word, "if") == 0 || strcmp(word, "else") == 0 ||
						strcmp(word, "while") == 0 || strcmp(word, "for") == 0 || strcmp(word, "static") == 0 ||
						strcmp(word, "uint8_t") == 0 || strcmp(word, "uint16_t") == 0 || strcmp(word, "uint32_t") == 0 ||
						strcmp(word, "volatile") == 0) {
						set_color_texto(6); // Magenta for C keywords
						} else if (word[0] == '#') {
							set_color_texto(10); // Light Green for preprocessor directves
						} else {
							set_color_texto(7);
						}
						// eZ80 Assembler Mnemónicos Evaluator
				} else if (file_mode == MODE_ASM) {
					if (strcasecmp(word, "ld") == 0 || strcasecmp(word, "add") == 0 || strcasecmp(word, "sub") == 0 ||
						strcasecmp(word, "jp") == 0 || strcasecmp(word, "jr") == 0 || strcasecmp(word, "call") == 0 ||
						strcasecmp(word, "ret") == 0 || strcasecmp(word, "inc") == 0 || strcasecmp(word, "dec") == 0 ||
						strcasecmp(word, "cp") == 0 || strcasecmp(word, "push") == 0 || strcasecmp(word, "pop") == 0) {
						set_color_texto(4); // Blue for assembler opcodes
						} else {
							set_color_texto(7);
						}
						// BBC BASIC Token Statement Evaluator
				} else if (file_mode == MODE_BAS) {
					if (strcmp(word, "PRINT") == 0 || strcmp(word, "INPUT") == 0 || strcmp(word, "FOR") == 0 ||
						strcmp(word, "NEXT") == 0 || strcmp(word, "IF") == 0 || strcmp(word, "THEN") == 0 ||
						strcmp(word, "GOTO") == 0 || strcmp(word, "GOSUB") == 0 || strcmp(word, "RETURN") == 0) {
						set_color_texto(5); // Red/Orange for BASIC tokens
						} else {
							set_color_texto(7);
						}
				}

				// Render the finalized parsed token string
				for (int i = 0; word[i] != '\0'; i++) {
					emit_char(word[i]);
				}
				set_color_texto(7);
				word_idx = 0;
				memset(word, 0, sizeof(word)); // Atomic cleanup of memory slots
			}

			if (!char_procesado) {
				emit_char(ch);
			}
		}
	}

	// Finalize page frame lines before waiting for final exit key
	dibujar_interfaz_grafica_vdp();

	set_color_texto(3);
	printf("\r[EOF: Press 'q' to exit]");
	while (1) {
		int key = getch();
		if (key == 'q' || key == 'Q' || key == 113 || key == 81) {
			printf("\x0C"); // Hardware CLS upon reaching end of stream
			break;
		}
	}

	set_color_texto(7);
	salto_de_linea();
	fclose(fp);
	return 0;
}
