#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "translator.h"
#include "xalloc.h"
#include "wio.h"
#include "symtab.h"

static int input_fd;
static FILE *output_file;

//
// Enum for output format
//
typedef enum {
    FORMAT_TAC, // Default: binary TAC
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
    OutputFormat format; // Output format (--tac, --yaml, --dot)
    char *input_file;    // Input filename
    char *output_file;   // Output filename (optional)
} Args;

//
// Function to print usage information
//
static void print_usage(const char *prog_name)
{
    const char *p = strrchr(prog_name, '/');
    if (p) {
        prog_name = p + 1;
    }
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "    %s [options] input-filename [output-filename]\n", prog_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    --tac            Emit TAC in binary format (default)\n");
    fprintf(stderr, "    --yaml           Emit YAML format\n");
    fprintf(stderr, "    --dot            Emit Graphviz DOT script\n");
    fprintf(stderr, "    -v, --verbose    Enable verbose mode\n");
    fprintf(stderr, "    -D, --debug      Print debug information\n");
    fprintf(stderr, "    -h, --help       Show this help message\n");
}

//
// Initialize Args structure with default values
//
static void init_args(Args *args)
{
    args->verbose     = 0;
    args->help        = 0;
    args->debug       = 0;
    args->format      = FORMAT_TAC; // Default format
    args->input_file  = NULL;
    args->output_file = NULL;
}

//
// Generate output filename from input filename based on format
//
static char *generate_output_filename(const char *input_file, OutputFormat format)
{
    // Find the last '.' in input_file to replace extension
    const char *ext     = strrchr(input_file, '.');
    size_t base_len     = ext ? (size_t)(ext - input_file) : strlen(input_file);
    const char *new_ext = (format == FORMAT_DOT)    ? ".dot"
                          : (format == FORMAT_YAML) ? ".yaml"
                                                    : ".tac";
    size_t new_ext_len  = strlen(new_ext);

    // Allocate memory for new filename
    char *filename = malloc(base_len + new_ext_len + 1);
    if (!filename) {
        fprintf(stderr, "Error: Memory allocation failed for output filename\n");
        return NULL;
    }

    // Copy base name and append new extension
    strncpy(filename, input_file, base_len);
    strcpy(filename + base_len, new_ext);
    return filename;
}

//
// Parse command-line arguments using getopt_long
//
static int parse_args(int argc, char *argv[], Args *args)
{
    static struct option long_options[] = {
        { "verbose", no_argument, 0, 'v' }, //
        { "help", no_argument, 0, 'h' },    //
        { "debug", no_argument, 0, 'D' },   //
        { "tac", no_argument, 0, 't' },     //
        { "yaml", no_argument, 0, 'y' },    //
        { "dot", no_argument, 0, 'd' },     //
        { },                                //
    };

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
        // Long options without short equivalents
        case 'y':
            args->format = FORMAT_YAML;
            break;
        case 'd':
            args->format = FORMAT_DOT;
            break;
        case 't':
            args->format = FORMAT_TAC;
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

static void open_input_output(const Args *args)
{
    if (args->verbose) {
        switch (args->format) {
        case FORMAT_TAC:
            printf("Emitting TAC in binary format to %s\n", args->output_file);
            break;
        case FORMAT_YAML:
            printf("Emitting YAML format to %s\n", args->output_file);
            break;
        case FORMAT_DOT:
            printf("Emitting Graphviz DOT script to %s\n", args->output_file);
            break;
        }
    }
    input_fd = open(args->input_file, O_RDONLY);
    if (input_fd < 0) {
        perror(args->input_file);
        exit(1);
    }
    output_file = stdout;
    if (args->output_file[0] != '-') {
        output_file = fopen(args->output_file, "w");
    }
}

static void close_output(const Args *args)
{
    close(input_fd);
    if (output_file != stdout) {
        fclose(output_file);
    }
}

static void emit_tac_toplevel(const Args *args, const Tac_TopLevel *tac)
{
    switch (args->format) {
    default:
    case FORMAT_TAC:
        //TODO: export_tac(output_fd, tac);
        break;
    case FORMAT_YAML:
        //TODO: export_yaml(output_file, tac);
        break;
    case FORMAT_DOT:
        //TODO: export_dot(output_file, tac);
        break;
    }
}

//
// Main processing function
//
void process_file(const Args *args)
{
    if (args->verbose) {
        printf("Processing %s in verbose mode\n", args->input_file);
    }
    if (args->debug) {
        printf("Debug: Format = %d, Input = %s, Output = %s\n", args->format, args->input_file,
               args->output_file);
        translator_debug = 1;
        import_debug = 1;
        export_debug = 1;
        wio_debug = 1;
    }
    open_input_output(args);

    WFILE input;
    ast_import_open(&input, input_fd);
    for (;;) {
        ExternalDecl *ast = import_external_decl(&input);
        if (!ast)
            break;

        print_external_decl(stdout, ast, 0);

        // Resolve identifiers.
        resolve(ast);

        // Typecheck definitions and uses of functions and variables.
        typecheck_global_decl(ast);

        // Annotate loops and break/continue statements.
        label_loops(ast);

        // Convert the AST to TAC.
        Tac_TopLevel *tac = translate(ast);
        free_external_decl(ast);
        if (tac) {
            if (args->debug) {
                print_tac_toplevel(stdout, tac, 0);
            }
            emit_tac_toplevel(args, tac);
            free_tac_toplevel(tac);
        }
    }
    wclose(&input);
    close_output(args);

    symtab_destroy();
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
