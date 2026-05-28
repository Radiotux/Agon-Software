/*
 * uniq.c - Report or omit repeated lines
 * For Agon Light / Agondev
 * 
 * Usage: uniq [options] input [output]
 * 
 * Options:
 *   -c              Prefix lines by the number of occurrences
 *   -d              Only print duplicate lines, one for each group
 *   -u              Only print unique lines
 *   -i              Ignore case when comparing
 *   -h, --help      Show this help
 * 
 * Note: Input file must be sorted for correct results.
 * Use 'sort' first: sort input.txt > sorted.txt, then uniq sorted.txt
 * 
 * Examples:
 *   uniq file.txt
 *   uniq -c file.txt
 *   uniq -d file.txt
 *   uniq -i file.txt
 *   uniq sorted.txt output.txt
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <agon/mos.h>

#define VERSION "1.0"
#define MAX_LINE_LEN 4096

// Options
int opt_count = 0;
int opt_duplicates = 0;
int opt_unique = 0;
int opt_ignore_case = 0;
int opt_help = 0;

/**
 * Display help
 */
void show_help(void) {
    printf("uniq v%s - Report or omit repeated lines\n", VERSION);
    printf("Usage: uniq [options] input [output]\n");
    printf("\nOptions:\n");
    printf("  -c              Prefix lines by the number of occurrences\n");
    printf("  -d              Only print duplicate lines, one for each group\n");
    printf("  -u              Only print unique lines\n");
    printf("  -i              Ignore case when comparing\n");
    printf("  -h, --help      Show this help\n");
    printf("\nNote: Input file must be sorted for correct results.\n");
    printf("Use 'sort' first: sort input.txt > sorted.txt, then uniq sorted.txt\n");
    printf("\nExamples:\n");
    printf("  uniq file.txt\n");
    printf("  uniq -c file.txt\n");
    printf("  uniq -d file.txt\n");
    printf("  uniq -i file.txt\n");
    printf("  uniq sorted.txt output.txt\n");
}

/**
 * Compare two strings (case-sensitive or case-insensitive)
 */
int compare_lines(const char *a, const char *b) {
    if (opt_ignore_case) {
        while (*a && *b) {
            char ca = tolower(*a);
            char cb = tolower(*b);
            if (ca != cb) return ca - cb;
            a++;
            b++;
        }
        return tolower(*a) - tolower(*b);
    }
    return strcmp(a, b);
}

/**
 * Process the input file
 */
int process_file(const char *input_file, const char *output_file) {
    FILE *in;
    FILE *out = stdout;
    char current_line[MAX_LINE_LEN];
    char next_line[MAX_LINE_LEN];
    int count = 1;
    int is_duplicate;
    
    // Open input file (required)
    in = fopen(input_file, "rb");
    if (!in) {
        fprintf(stderr, "uniq: cannot open '%s' for reading\n", input_file);
        return 0;
    }
    
    // Open output file if specified
    if (output_file) {
        out = fopen(output_file, "wb");
        if (!out) {
            fprintf(stderr, "uniq: cannot open '%s' for writing\n", output_file);
            fclose(in);
            return 0;
        }
    }
    
    // Read first line
    if (fgets(current_line, MAX_LINE_LEN, in) == NULL) {
        // Empty file
        fclose(in);
        if (out != stdout) fclose(out);
        return 1;
    }
    
    // Remove trailing newline
    current_line[strcspn(current_line, "\n")] = '\0';
    
    // Read and process lines
    while (fgets(next_line, MAX_LINE_LEN, in)) {
        // Remove trailing newline
        next_line[strcspn(next_line, "\n")] = '\0';
        
        is_duplicate = (compare_lines(current_line, next_line) == 0);
        
        if (is_duplicate) {
            count++;
        } else {
            // Output current group
            if (!opt_duplicates || count > 1) {
                if (!opt_unique || count == 1) {
                    if (opt_count) {
                        fprintf(out, "%7d %s\n", count, current_line);
                    } else {
                        fprintf(out, "%s\n", current_line);
                    }
                }
            }
            
            // Move to next group
            strcpy(current_line, next_line);
            count = 1;
        }
    }
    
    // Output last group
    if (!opt_duplicates || count > 1) {
        if (!opt_unique || count == 1) {
            if (opt_count) {
                fprintf(out, "%7d %s\n", count, current_line);
            } else {
                fprintf(out, "%s\n", current_line);
            }
        }
    }
    
    // Close files
    fclose(in);
    if (out != stdout) fclose(out);
    
    return 1;
}

/**
 * Main program
 */
int main(int argc, char **argv) {
    char *input_file = NULL;
    char *output_file = NULL;
    int i;
    
    // Parse arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_help();
            return 0;
        }
        else if (strcmp(argv[i], "-c") == 0) {
            opt_count = 1;
        }
        else if (strcmp(argv[i], "-d") == 0) {
            opt_duplicates = 1;
        }
        else if (strcmp(argv[i], "-u") == 0) {
            opt_unique = 1;
        }
        else if (strcmp(argv[i], "-i") == 0) {
            opt_ignore_case = 1;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "uniq: unknown option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'uniq -h' for help\n");
            return 1;
        }
        else if (!input_file) {
            input_file = argv[i];
        }
        else if (!output_file) {
            output_file = argv[i];
        }
        else {
            fprintf(stderr, "uniq: extra operand '%s'\n", argv[i]);
            return 1;
        }
    }
    
    // Need input file
    if (!input_file) {
        fprintf(stderr, "uniq: missing file operand\n");
        fprintf(stderr, "Try 'uniq -h' for help\n");
        return 1;
    }
    
    // -d and -u are mutually exclusive
    if (opt_duplicates && opt_unique) {
        fprintf(stderr, "uniq: cannot specify both -d and -u\n");
        return 1;
    }
    
    // Process the file
    if (!process_file(input_file, output_file)) {
        return 1;
    }
    
    return 0;
}