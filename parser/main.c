#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "xalloc.h"

//
// Enum for output format
//
typedef enum {
    FORMAT_AST, // Default: binary AST
    FORMAT_YAML,
    FORMAT_DOT
} OutputFormat;

//
// Structure to hold parsed arguments
//
typedef struct {
    int verbose;         // -v or --verbose
    int help;            // -h or --help
    int debug;           // -D or --debug
    OutputFormat format; // Output format (--ast, --yaml, --dot)
    char *input_file;    // Input filename
    char *output_file;   // Output filename (optional)
} Args;

//
// Function to print usage information
//
void print_usage(const char *prog_name)
{
    const char *p = strrchr(prog_name, '/');
    if (p) {
        prog_name = p + 1;
    }
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "    %s [options] input-filename [output-filename]\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    --ast            Emit AST in binary format (default)\n");
    fprintf(stderr, "    --yaml           Emit YAML format\n");
    fprintf(stderr, "    --dot            Emit Graphviz DOT script\n");
    fprintf(stderr, "    -v, --verbose    Enable verbose mode\n");
    fprintf(stderr, "    -D, --debug      Print debug information\n");
    fprintf(stderr, "    -h, --help       Show this help message\n");
}

//
// Initialize Args structure with default values
//
void init_args(Args *args)
{
    args->verbose     = 0;
    args->help        = 0;
    args->debug       = 0;
    args->format      = FORMAT_AST; // Default format
    args->input_file  = NULL;
    args->output_file = NULL;
}

//
// Generate output filename from input filename based on format
//
char *generate_output_filename(const char *input_file, OutputFormat format)
{
    // Find the last '.' in input_file to replace extension
    const char *ext     = strrchr(input_file, '.');
    size_t base_len     = ext ? (size_t)(ext - input_file) : strlen(input_file);
    const char *new_ext = (format == FORMAT_DOT)    ? ".dot"
                          : (format == FORMAT_YAML) ? ".yaml"
                                                    : ".ast";
    size_t new_ext_len  = strlen(new_ext);

    // Allocate memory for new filename
    char *output_file = malloc(base_len + new_ext_len + 1);
    if (!output_file) {
        fprintf(stderr, "Error: Memory allocation failed for output filename\n");
        return NULL;
    }

    // Copy base name and append new extension
    strncpy(output_file, input_file, base_len);
    strcpy(output_file + base_len, new_ext);
    return output_file;
}

//
// Parse command-line arguments using getopt_long
//
int parse_args(int argc, char *argv[], Args *args)
{
    static struct option long_options[] = { { "verbose", no_argument, 0, 'v' },
                                            { "help", no_argument, 0, 'h' },
                                            { "debug", no_argument, 0, 'D' },
                                            { "ast", no_argument, 0, 0 },
                                            { "yaml", no_argument, 0, 0 },
                                            { "dot", no_argument, 0, 0 },
                                            { 0, 0, 0, 0 } };

    int opt;
    int option_index = 0;

    if (argc < 2) {
        // Show usage.
        args->help = 1;
        return 0;
    }
    while ((opt = getopt_long(argc, argv, "vhD", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'v':
            args->verbose = 1;
            break;
        case 'h':
            args->help = 1;
            return 0;
        case 'D':
            args->debug = 1;
            break;
        case 0: // Long options without short equivalents
            if (strcmp(long_options[option_index].name, "yaml") == 0) {
                args->format = FORMAT_YAML;
            } else if (strcmp(long_options[option_index].name, "dot") == 0) {
                args->format = FORMAT_DOT;
            } else {
                args->format = FORMAT_AST;
            }
            break;
        case '?': // Unknown option
            return -1;
        }
    }

    // Check for input filename (required)
    if (optind < argc) {
        args->input_file = argv[optind++];
    } else {
        fprintf(stderr, "Error: Input filename is required\n");
        return -1;
    }

    // Check for output filename (optional)
    if (optind < argc) {
        args->output_file = argv[optind];
    } else {
        // Generate output filename based on input and format
        args->output_file = generate_output_filename(args->input_file, args->format);
        if (!args->output_file) {
            return -1;
        }
    }

    return 0;
}

//
// Main processing function
//
void process_file(Args *args)
{
    if (args->verbose) {
        printf("Processing %s in verbose mode\n", args->input_file);
    }
    if (args->debug) {
        printf("Debug: Format = %d, Input = %s, Output = %s\n", args->format, args->input_file,
               args->output_file);
    }
    parser_debug = args->debug;
    FILE *input_file = fopen(args->input_file, "r");
    Program *program = parse(input_file);
    fclose(input_file);

    FILE *output_file = stdout;
    if (args->output_file[0] != '-') {
        output_file = fopen(args->output_file, "w");
    }

    switch (args->format) {
    default:
    case FORMAT_AST:
        if (args->verbose) {
            printf("Emitting AST in binary format to %s\n", args->output_file);
        }
        if (args->debug) {
            print_program(output_file, program);
        }
        export_ast(fileno(output_file), program);
        break;
    case FORMAT_YAML:
        if (args->verbose) {
            printf("Emitting YAML format to %s\n", args->output_file);
        }
        export_yaml(output_file, program);
        break;
    case FORMAT_DOT:
        if (args->verbose) {
            printf("Emitting Graphviz DOT script to %s\n", args->output_file);
        }
        export_dot(output_file, program);
        break;
    }

    if (output_file != stdout) {
        fclose(output_file);
    }
    free_program(program);
    if (args->debug) {
        xreport_lost_memory();
    }
    xfree_all();
}

int main(int argc, char *argv[])
{
    Args args;
    init_args(&args);

    if (parse_args(argc, argv, &args) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    if (args.help) {
        print_usage(argv[0]);
        return 0;
    }

    // Pass args to backend for processing
    process_file(&args);

    return 0;
}
