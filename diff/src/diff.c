/*
 * diff.c - Compare files line by line (enhanced output)
 * For Agon Light / Agondev
 * 
 * Usage: diff [options] file1 file2 [-o output]
 * 
 * Options:
 *   -i              Ignore case differences
 *   -w              Ignore all whitespace
 *   -b              Ignore changes in the amount of whitespace
 *   -B              Ignore changes whose lines are all blank
 *   -q              Report only whether files differ
 *   -u              Output unified context format
 *   -o <file>       Write output to file instead of stdout
 *   -h, --help      Show this help
 * 
 * Examples:
 *   diff file1.txt file2.txt
 *   diff -u file1.txt file2.txt
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <agon/mos.h>

#define VERSION "1.0"
#define MAX_LINE_LEN 4096
#define MAX_LINES 10000

// Options
int opt_ignore_case = 0;
int opt_ignore_all_space = 0;
int opt_ignore_space_change = 0;
int opt_ignore_blank_lines = 0;
int opt_quick = 0;
int opt_unified = 0;
char *output_file = NULL;

// File lines storage
char **lines1 = NULL;
char **lines2 = NULL;
int lines1_count = 0;
int lines2_count = 0;

// Statistics
int added_lines = 0;
int removed_lines = 0;
int changed_lines = 0;

// Output file pointer
FILE *out = stdout;

/**
 * Display help
 */
void show_help(void) {
    printf("diff v%s - Compare files line by line\n", VERSION);
    printf("Usage: diff [options] file1 file2 [-o output]\n");
    printf("\nOptions:\n");
    printf("  -i              Ignore case differences\n");
    printf("  -w              Ignore all whitespace\n");
    printf("  -b              Ignore changes in the amount of whitespace\n");
    printf("  -B              Ignore changes whose lines are all blank\n");
    printf("  -q              Report only whether files differ\n");
    printf("  -u              Output unified context format\n");
    printf("  -o <file>       Write output to file instead of stdout\n");
    printf("  -h, --help      Show this help\n");
    printf("\nExamples:\n");
    printf("  diff file1.txt file2.txt\n");
    printf("  diff -u file1.txt file2.txt\n");
    printf("  diff -q file1.txt file2.txt\n");
}

/**
 * Remove trailing newline
 */
void chomp(char *str) {
    int len = strlen(str);
    if (len > 0 && str[len-1] == '\n') {
        str[len-1] = '\0';
    }
}

/**
 * Normalize a line according to options
 */
void normalize_line(char *dest, const char *src) {
    char temp[MAX_LINE_LEN];
    char *p = temp;
    const char *s = src;
    int last_was_space = 0;
    
    while (*s && (p - temp) < MAX_LINE_LEN - 1) {
        *p++ = *s++;
    }
    *p = '\0';
    
    if (opt_ignore_case) {
        for (p = temp; *p; p++) {
            *p = tolower(*p);
        }
    }
    
    if (opt_ignore_all_space) {
        p = dest;
        for (s = temp; *s; s++) {
            if (!isspace(*s)) {
                *p++ = *s;
            }
        }
        *p = '\0';
    } else if (opt_ignore_space_change) {
        p = dest;
        last_was_space = 0;
        for (s = temp; *s; s++) {
            if (isspace(*s)) {
                if (!last_was_space) {
                    *p++ = ' ';
                    last_was_space = 1;
                }
            } else {
                *p++ = *s;
                last_was_space = 0;
            }
        }
        if (p > dest && *(p-1) == ' ') p--;
        *p = '\0';
    } else {
        strcpy(dest, temp);
    }
}

/**
 * Check if a line is blank
 */
int is_blank_line(const char *line) {
    while (*line) {
        if (!isspace(*line)) return 0;
        line++;
    }
    return 1;
}

/**
 * Compare two normalized lines
 */
int lines_equal(const char *a, const char *b) {
    char norm_a[MAX_LINE_LEN];
    char norm_b[MAX_LINE_LEN];
    
    normalize_line(norm_a, a);
    normalize_line(norm_b, b);
    
    return strcmp(norm_a, norm_b) == 0;
}

/**
 * Print to output
 */
void diff_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);
}

/**
 * Print separator line
 */
void print_separator(void) {
    diff_printf("----------------------------------------\n");
}

/**
 * Load file into array of lines
 */
int load_file(const char *filename, char ***lines, int *count) {
    FILE *fp;
    char buffer[MAX_LINE_LEN];
    char **line_array = NULL;
    int capacity = 0;
    int c = 0;
    
    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "diff: cannot open '%s' for reading\n", filename);
        return 0;
    }
    
    while (fgets(buffer, MAX_LINE_LEN, fp)) {
        chomp(buffer);
        
        if (opt_ignore_blank_lines && is_blank_line(buffer)) {
            continue;
        }
        
        if (c >= capacity) {
            int new_capacity = capacity == 0 ? 100 : capacity * 2;
            char **new_array = realloc(line_array, new_capacity * sizeof(char *));
            if (!new_array) {
                fprintf(stderr, "diff: out of memory\n");
                fclose(fp);
                return 0;
            }
            line_array = new_array;
            capacity = new_capacity;
        }
        
        line_array[c] = malloc(strlen(buffer) + 1);
        if (!line_array[c]) {
            fprintf(stderr, "diff: out of memory\n");
            fclose(fp);
            return 0;
        }
        strcpy(line_array[c], buffer);
        c++;
    }
    
    fclose(fp);
    *lines = line_array;
    *count = c;
    return 1;
}

/**
 * Free loaded lines
 */
void free_lines(char **lines, int count) {
    int i;
    for (i = 0; i < count; i++) {
        free(lines[i]);
    }
    free(lines);
}

/**
 * Quick diff
 */
int quick_diff(void) {
    if (lines1_count != lines2_count) {
        return 1;
    }
    
    for (int i = 0; i < lines1_count; i++) {
        if (!lines_equal(lines1[i], lines2[i])) {
            return 1;
        }
    }
    return 0;
}

/**
 * Find differences with enhanced output
 */
void find_differences(void) {
    int i = 0, j = 0;
    int line_num1 = 1, line_num2 = 1;
    
    diff_printf("\n");
    print_separator();
    diff_printf("DIFFERENCES BETWEEN FILES\n");
    print_separator();
    diff_printf("\n");
    
    while (i < lines1_count && j < lines2_count) {
        if (lines_equal(lines1[i], lines2[j])) {
            i++;
            j++;
            line_num1++;
            line_num2++;
        } else {
            // Found difference
            diff_printf("[Line %d] FILE 1: %s\n", line_num1, lines1[i]);
            diff_printf("[Line %d] FILE 2: %s\n", line_num2, lines2[j]);
            diff_printf("\n");
            changed_lines++;
            i++;
            j++;
            line_num1++;
            line_num2++;
        }
    }
    
    // Remaining lines in file1
    while (i < lines1_count) {
        diff_printf("[Line %d] FILE 1 ONLY: %s\n", line_num1, lines1[i]);
        removed_lines++;
        i++;
        line_num1++;
    }
    
    // Remaining lines in file2
    while (j < lines2_count) {
        diff_printf("[Line %d] FILE 2 ONLY: %s\n", line_num2, lines2[j]);
        added_lines++;
        j++;
        line_num2++;
    }
    
    // Summary
    if (changed_lines > 0 || added_lines > 0 || removed_lines > 0) {
        diff_printf("\n");
        print_separator();
        diff_printf("SUMMARY:\n");
        if (changed_lines > 0) diff_printf("  Changed lines: %d\n", changed_lines);
        if (added_lines > 0)   diff_printf("  Added lines:   %d\n", added_lines);
        if (removed_lines > 0) diff_printf("  Removed lines: %d\n", removed_lines);
        print_separator();
    }
}

/**
 * Unified format output
 */
void find_differences_unified(void) {
    int i = 0, j = 0;
    int line_num1 = 1, line_num2 = 1;
    int in_block = 0;
    
    while (i < lines1_count && j < lines2_count) {
        if (lines_equal(lines1[i], lines2[j])) {
            if (in_block) {
                in_block = 0;
                diff_printf("\n");
            }
            i++;
            j++;
            line_num1++;
            line_num2++;
        } else {
            if (!in_block) {
                diff_printf("@@ -%d,%d +%d,%d @@\n", line_num1, 1, line_num2, 1);
                in_block = 1;
            }
            diff_printf("-%s\n", lines1[i]);
            diff_printf("+%s\n", lines2[j]);
            changed_lines++;
            i++;
            j++;
            line_num1++;
            line_num2++;
        }
    }
    
    while (i < lines1_count) {
        if (!in_block) {
            diff_printf("@@ -%d,%d +%d,%d @@\n", line_num1, lines1_count - i, line_num2, 0);
            in_block = 1;
        }
        diff_printf("-%s\n", lines1[i]);
        removed_lines++;
        i++;
    }
    
    while (j < lines2_count) {
        if (!in_block) {
            diff_printf("@@ -%d,%d +%d,%d @@\n", line_num1, 0, line_num2, lines2_count - j);
            in_block = 1;
        }
        diff_printf("+%s\n", lines2[j]);
        added_lines++;
        j++;
    }
    
    if (in_block) {
        diff_printf("\n");
    }
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    char *file1 = NULL;
    char *file2 = NULL;
    int differ;
    int i;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-i") == 0) {
            opt_ignore_case = 1;
        }
        else if (strcmp(argv[i], "-w") == 0) {
            opt_ignore_all_space = 1;
        }
        else if (strcmp(argv[i], "-b") == 0) {
            opt_ignore_space_change = 1;
        }
        else if (strcmp(argv[i], "-B") == 0) {
            opt_ignore_blank_lines = 1;
        }
        else if (strcmp(argv[i], "-q") == 0) {
            opt_quick = 1;
        }
        else if (strcmp(argv[i], "-u") == 0) {
            opt_unified = 1;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "diff: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'diff -h' for help\n");
            return 0;
        }
        else if (!file1) {
            file1 = argv[i];
        }
        else if (!file2) {
            file2 = argv[i];
        }
        else {
            fprintf(stderr, "diff: extra operand '%s'\n", argv[i]);
            return 0;
        }
    }
    
    // Need two files
    if (!file1 || !file2) {
        fprintf(stderr, "diff: missing file operand\n");
        fprintf(stderr, "Try 'diff -h' for help\n");
        return 0;
    }
    
    // Open output file if specified
    if (output_file) {
        out = fopen(output_file, "wb");
        if (!out) {
            fprintf(stderr, "diff: cannot open '%s' for writing\n", output_file);
            return 0;
        }
    }
    
    // Load files
    if (!load_file(file1, &lines1, &lines1_count)) {
        goto cleanup;
    }
    if (!load_file(file2, &lines2, &lines2_count)) {
        goto cleanup;
    }
    
    // Check if files differ
    differ = quick_diff();
    
    if (opt_quick) {
        if (differ) {
            diff_printf("Files %s and %s differ\n", file1, file2);
        } else {
            diff_printf("Files %s and %s are identical\n", file1, file2);
        }
    } else if (differ) {
        if (opt_unified) {
            find_differences_unified();
        } else {
            find_differences();
        }
    } else {
        diff_printf("\nFiles are identical.\n");
    }
    
cleanup:
    // Clean up
    if (lines1) free_lines(lines1, lines1_count);
    if (lines2) free_lines(lines2, lines2_count);
    
    // Close output file if used
    if (out != stdout) {
        fclose(out);
    }
    
    return 0;
}