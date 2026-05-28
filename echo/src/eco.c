/*
 * echo.c - Display a line of text (with file output)
 * For Agon Light / Agondev
 * 
 * Usage: echo [options] [text...] [-o file] [-a file]
 * 
 * Options:
 *   -n                Do not output trailing newline
 *   -e                Enable interpretation of backslash escapes
 *   -E                Disable interpretation (default)
 *   -o <file>         Write output to file (overwrite)
 *   -a <file>         Append output to file
 *   -h, --help        Show this help
 * 
 * Examples:
 *   echo Hello World
 *   echo -o hello.txt "Hello World"
 *   echo -a hello.txt "More text"
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <agon/mos.h>

#define VERSION "1.0"
#define BUFFER_SIZE 4096

// Options
int opt_no_newline = 0;
int opt_escape = 0;
char *output_file = NULL;
int output_append = 0;
int opt_help = 0;

/**
 * Create parent directories if needed (same as touch)
 */
int create_parent_dirs(const char *path) {
    char dir_path[256];
    char *last_slash;
    
    strcpy(dir_path, path);
    last_slash = strrchr(dir_path, '/');
    
    if (!last_slash) return 1;
    
    *last_slash = '\0';
    
    // Check if directory exists
    FILINFO finfo;
    uint8_t result = ffs_stat(&finfo, dir_path);
    
    if (result != 0 || !(finfo.fattrib & AM_DIR)) {
        char cmd[300];
        snprintf(cmd, sizeof(cmd), "mkdir %s", dir_path);
        mos_oscli(cmd, NULL, 0);
    }
    
    return 1;
}

/**
 * Convert escape sequences
 */
char *process_escapes(const char *input, char *output, int max_len) {
    char *out = output;
    const char *in = input;
    int i;
    unsigned int val;
    
    while (*in && (out - output) < max_len - 1) {
        if (*in == '\\' && opt_escape) {
            in++;
            switch (*in) {
                case '\\': *out++ = '\\'; break;
                case 'a':  *out++ = '\a'; break;
                case 'b':  *out++ = '\b'; break;
                case 'c':  *out = '\0'; return output;
                case 'e':  *out++ = 0x1B; break;
                case 'f':  *out++ = '\f'; break;
                case 'n':  *out++ = '\n'; break;
                case 'r':  *out++ = '\r'; break;
                case 't':  *out++ = '\t'; break;
                case 'v':  *out++ = '\v'; break;
                case '0':
                    val = 0;
                    for (i = 0; i < 3 && in[i+1] >= '0' && in[i+1] <= '7'; i++) {
                        val = val * 8 + (in[i+1] - '0');
                    }
                    if (i > 0) {
                        *out++ = (char)val;
                        in += i;
                    } else {
                        *out++ = '\\';
                        *out++ = '0';
                    }
                    break;
                case 'x':
                    in++;
                    val = 0;
                    for (i = 0; i < 2 && isxdigit(in[i]); i++) {
                        val = val * 16 + (isdigit(in[i]) ? in[i] - '0' : tolower(in[i]) - 'a' + 10);
                    }
                    if (i > 0) {
                        *out++ = (char)val;
                        in += i - 1;
                    } else {
                        *out++ = '\\';
                        *out++ = 'x';
                        in--;
                    }
                    break;
                default:
                    *out++ = '\\';
                    *out++ = *in;
                    break;
            }
        } else {
            *out++ = *in;
        }
        in++;
    }
    *out = '\0';
    return output;
}

/**
 * Write text to file (creates directories if needed)
 */
int write_to_file(const char *filename, const char *text, size_t len, int append) {
    FILE *fp;
    
    // Create parent directories if needed
    create_parent_dirs(filename);
    
    // Open file
    fp = fopen(filename, append ? "ab" : "wb");
    if (!fp) {
        fprintf(stderr, "echo: cannot open '%s' for writing\n", filename);
        return 0;
    }
    
    // Write text
    fwrite(text, 1, len, fp);
    fclose(fp);
    
    return 1;
}

/**
 * Build the output text from arguments
 */
char *build_text(char **args, int count, size_t *out_len) {
    static char buffer[BUFFER_SIZE];
    char processed[BUFFER_SIZE];
    int i;
    size_t total_len = 0;
    
    buffer[0] = '\0';
    
    for (i = 0; i < count; i++) {
        if (i > 0) {
            buffer[total_len++] = ' ';
        }
        
        if (opt_escape) {
            process_escapes(args[i], processed, BUFFER_SIZE);
            strcpy(&buffer[total_len], processed);
            total_len += strlen(processed);
        } else {
            strcpy(&buffer[total_len], args[i]);
            total_len += strlen(args[i]);
        }
        
        if (total_len >= BUFFER_SIZE - 10) break;
    }
    
    // Add newline if needed
    if (!opt_no_newline) {
        buffer[total_len++] = '\n';
        buffer[total_len] = '\0';
    }
    
    if (out_len) *out_len = total_len;
    return buffer;
}

/**
 * Display help
 */
void show_help(void) {
    printf("echo v%s - Display a line of text\n", VERSION);
    printf("Usage: echo [options] [text...] [-o file] [-a file]\n");
    printf("\nOptions:\n");
    printf("  -n                Do not output trailing newline\n");
    printf("  -e                Enable interpretation of backslash escapes\n");
    printf("  -E                Disable interpretation (default)\n");
    printf("  -o <file>         Write output to file (overwrite)\n");
    printf("  -a <file>         Append output to file\n");
    printf("  -h, --help        Show this help\n");
    printf("\nEscapes (with -e):\n");
    printf("  \\\\   backslash     \\n   new line        \\r   carriage return\n");
    printf("  \\t   tab           \\v   vertical tab    \\a   alert (bell)\n");
    printf("  \\b   backspace     \\f   form feed       \\e   escape\n");
    printf("  \\c   stop output   \\0nnn octal        \\xHH hex\n");
    printf("\nExamples:\n");
    printf("  echo Hello World\n");
    printf("  echo -o hello.txt \"Hello World\"\n");
    printf("  echo -a hello.txt \"More text\"\n");
    printf("  echo -e -o file.txt \"Line1\\nLine2\"\n");
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    char *args[256];
    int arg_count = 0;
    char *text;
    size_t text_len;
    int i;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-n") == 0) {
            opt_no_newline = 1;
        }
        else if (strcmp(argv[i], "-e") == 0) {
            opt_escape = 1;
        }
        else if (strcmp(argv[i], "-E") == 0) {
            opt_escape = 0;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
            output_append = 0;
        }
        else if (strcmp(argv[i], "-a") == 0 && i + 1 < argc) {
            output_file = argv[++i];
            output_append = 1;
        }
        else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            printf("echo: unknown option '%s'\n", argv[i]);
            printf("Try 'echo -h' for help\n");
            return 1;
        }
        else {
            args[arg_count++] = argv[i];
        }
    }
    
    // Build the text to output
    text = build_text(args, arg_count, &text_len);
    
    // Output
    if (output_file) {
        // Write to file (creates directories if needed)
        if (!write_to_file(output_file, text, text_len, output_append)) {
            return 1;
        }
    } else {
        // Write to stdout
        fwrite(text, 1, text_len, stdout);
    }
    
    return 0;
}