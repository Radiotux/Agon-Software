#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Formal declaration to inform the compiler that getch() is available natively
extern int getch(void);

#define LINE_BUFFER 512
#define MAX_WIDTH 70
#define PAGE_LINES 30 // Freezes output exactly at line 30 to prevent scrolling

// --- NATIVE AGON LIGHT VDU CONSTANTS ---
#define VDU_TEXT_COL 17
#define COL_GRAY     8
#define COL_RED      9
#define COL_GREEN    10
#define COL_CYAN     11
#define COL_YELLOW   14
#define COL_WHITE    15

#define SET_COLOR(c) printf("%c%c", VDU_TEXT_COL, (c))

// File detection modes
#define MODE_C      0
#define MODE_ASM    1
#define MODE_BASIC  2

// --- EXPANDED KEYWORD LISTS (No trailing spaces to maximize matching flexibility) ---
const char *keywords_c[] = {
	"void", "int", "char", "return", "if", "while", "for", "else", "const", "struct",
	"#include", "#define", "FILE", "printf", "fopen", "fclose", "fgets", "putchar", NULL
};

const char *keywords_asm[] = {
	"ld", "xor", "inc", "dec", "jp", "call", "org", "equ", "ret", "nop",
	"add", "sub", "cp", "push", "pop", "db", "dw", "ds", "macro", "endm", NULL
};

const char *keywords_basic[] = {
	"PRINT", "INPUT", "LET", "IF", "THEN", "ELSE", "GOTO", "GOSUB", "RETURN",
	"FOR", "NEXT", "END", "PROC", "DEF", "LOCAL", "CLS", "MODE", "VDU", NULL
};

// Common control operators
const char *operators[] = {"==", "!=", "&&", "||", ">=", "<=", NULL};

void process_and_print_line(char *line, int file_mode, int line_num) {
	int length = strlen(line);

	// Strict line ending cleanup (\n and \r) from the buffer
	while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
		line[length - 1] = '\0';
		length--;
	}

	char *useful_text = line;

	// Dynamic line omission logic exclusive to TPASM assembly listings
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

	// 1. PRINT GUTTER / LINE NUMBER (Gray)
	SET_COLOR(COL_GRAY);
	printf("%4d | ", line_num);
	SET_COLOR(COL_WHITE);

	// 2. DYNAMIC SYNTAX HIGHLIGHTING ENGINE
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
					if (strstr(useful_text, keywords_c[i]) != NULL) { line_color = COL_YELLOW; break; }
				}
			} else if (file_mode == MODE_ASM) {
				for (int i = 0; keywords_asm[i] != NULL; i++) {
					if (strstr(useful_text, keywords_asm[i]) != NULL) { line_color = COL_YELLOW; break; }
				}
			} else if (file_mode == MODE_BASIC) {
				for (int i = 0; keywords_basic[i] != NULL; i++) {
					if (strstr(useful_text, keywords_basic[i]) != NULL) { line_color = COL_YELLOW; break; }
				}
			}

			SET_COLOR(line_color);

			int in_string = 0;
			for (int i = 0; useful_text[i] != '\0'; i++) {
				if (useful_text[i] == '"') {
					if (!in_string) {
						SET_COLOR(COL_GREEN); // Open string literal in GREEN
						in_string = 1;
					} else {
						putchar(useful_text[i]);
						SET_COLOR(line_color); // Restore line base color
						in_string = 0;
						continue;
					}
				}

				if (!in_string) {
					int is_operator = 0;
					for (int o = 0; operators[o] != NULL; o++) {
						int op_len = strlen(operators[o]);
						if (strncmp(&useful_text[i], operators[o], op_len) == 0) {
							SET_COLOR(COL_CYAN); // Logical operator in CYAN
							printf("%s", operators[o]);
							SET_COLOR(line_color);
							i += (op_len - 1);
							is_operator = 1;
							break;
						}
					}
					if (is_operator) continue;
				}

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

	// --- DETECTION BY FILE EXTENSION (BBC BASIC) ---
	int name_len = strlen(argv[1]);
	if (name_len > 4) {
		char *ext = argv[1] + name_len - 4;
		if (strcasecmp(ext, ".bas") == 0 || strcasecmp(ext, ".bbc") == 0) {
			file_mode = MODE_BASIC;
		}
	}

	// --- DETECTION VIA FIRST LINE CONTENT (TPASM Assembly Listings) ---
	if (fgets(line, sizeof(line), file_handle) != NULL) {
		if (strncmp(line, "tpasm", 5) == 0) {
			file_mode = MODE_ASM;
		}

		process_and_print_line(line, file_mode, line_num);
		line_num++;
		screen_line_count++;
	}

	// MAIN PROCESSING LOOP
	while (fgets(line, sizeof(line), file_handle) != NULL) {
		process_and_print_line(line, file_mode, line_num);
		line_num++;
		screen_line_count++;

		// Handle page freezing upon reaching the 30-line display boundary
		if (screen_line_count >= PAGE_LINES) {

			fflush(stdout);

			// Silent hardware pause using the native getch() routine
			int input_key = getch();

			// Intercept Q (113) or q (81) to exit immediately
			if (input_key == 'q' || input_key == 'Q' || input_key == 113 || input_key == 81) {
				SET_COLOR(COL_CYAN);
				printf("+---[ Viewer closed by user ]----------------------\n\r");
				SET_COLOR(COL_WHITE);
				fclose(file_handle);
				return 0;
			}

			screen_line_count = 0;
		}
	}

	SET_COLOR(COL_CYAN);
	printf("+---[ End of file ]---------------------------------\n\r");
	SET_COLOR(COL_WHITE);

	fclose(file_handle);
	return 0;
}
