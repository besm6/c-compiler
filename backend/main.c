#include <fcntl.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "codegen.h"
#include "tac.h"
#include "wio.h"
#include "xalloc.h"

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

static FILE *output_file;

//
// Structure to hold parsed arguments
//
typedef struct {
    int verbose;          // -v or --verbose
    int help;             // -h or --help
    int debug;            // -D or --debug
    Besm_Dialect dialect; // --madlen / --unix / --bemsh
    char *input_file;     // Input filename
    char *output_file;    // Output filename (optional)
} Args;

// Long-option values for the dialect flags (outside the ASCII range so they do not
// collide with the short options).
enum {
    OPT_MADLEN = 1000,
    OPT_UNIX,
    OPT_BEMSH,
};

// Default output-file extension for each dialect.
static const char *dialect_ext(Besm_Dialect d)
{
    switch (d) {
    case BESM_UNIX:
        return ".s";
    case BESM_BEMSH:
        return ".bem";
    case BESM_MADLEN:
    default:
        return ".mad";
    }
}

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
    fprintf(stderr, "        --madlen        Emit Madlen assembly for Dubna (default)\n");
    fprintf(stderr, "        --unix          Emit Unix (b6as) assembly\n");
    fprintf(stderr, "        --bemsh         Emit Bemsh autocode for Dubna\n");
    fprintf(stderr, "    -v, --verbose       Enable verbose mode\n");
    fprintf(stderr, "    -D, --debug         Print debug information\n");
    fprintf(stderr, "    -h, --help          Show this help message\n");
}

//
// Initialize Args structure with default values
//
static void init_args(Args *args)
{
    args->verbose     = 0;
    args->help        = 0;
    args->debug       = 0;
    // Madlen is kept as the effective default so the existing libc build and run
    // tests stay green; the intended eventual default is --unix, to be flipped once
    // the Unix emitter and libc.a land (see backend/besm6/TODO.md task U4).
    args->dialect     = BESM_MADLEN;
    args->input_file  = NULL;
    args->output_file = NULL;
}

//
// Generate output filename from input filename
//
static char *generate_output_filename(const char *input_file, Besm_Dialect dialect)
{
    // Find the last '.' in input_file to replace extension
    const char *ext     = strrchr(input_file, '.');
    size_t base_len     = ext ? (size_t)(ext - input_file) : strlen(input_file);
    const char *new_ext = dialect_ext(dialect);
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
        { "verbose", no_argument, 0, 'v' },        //
        { "help", no_argument, 0, 'h' },           //
        { "debug", no_argument, 0, 'D' },          //
        { "madlen", no_argument, 0, OPT_MADLEN },  //
        { "unix", no_argument, 0, OPT_UNIX },      //
        { "bemsh", no_argument, 0, OPT_BEMSH },    //
        {},                                        //
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
        case OPT_MADLEN:
            args->dialect = BESM_MADLEN;
            break;
        case OPT_UNIX:
            args->dialect = BESM_UNIX;
            break;
        case OPT_BEMSH:
            args->dialect = BESM_BEMSH;
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
        // Generate output filename based on input
        args->output_file = generate_output_filename(args->input_file, args->dialect);
        if (!args->output_file) {
            return -1;
        }
    }

    return 0;
}

static void open_output(const Args *args)
{
    output_file = stdout;
    if (args->output_file[0] != '-') {
        output_file = fopen(args->output_file, "w");
    }
}

static void close_output(const Args *args)
{
    (void)args;
    if (output_file != stdout) {
        fclose(output_file);
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
        printf("Debug: Input = %s, Output = %s\n", args->input_file, args->output_file);
        // import_debug     = 1;
        // export_debug     = 1;
        // wio_debug        = 1;
        // xalloc_debug     = 1;
    }
    open_output(args);

    WFILE input;
    wopen(&input, args->input_file, "r");

    // Phase 1: read all toplevels into a linked chain for global-name resolution.
    Tac_TopLevel *head = NULL, **tail_ptr = &head;
    for (;;) {
        Tac_TopLevel *tac = tac_import_toplevel(&input);
        if (!tac)
            break;
        *tail_ptr = tac;
        tail_ptr  = &tac->next;
    }
    wclose(&input);

    // Phase 2: codegen each toplevel with the full program chain as context.
    for (const Tac_TopLevel *tl = head; tl; tl = tl->next) {
        if (args->debug)
            tac_print_toplevel(stdout, tl, 0);
        codegen_program(head, tl, output_file, args->dialect);
    }
    tac_free_toplevel(head);
    close_output(args);

    if (args->debug) {
        xreport_lost_memory();
    }
    xfree_all();
}

//
// Error handling
//
void _Noreturn fatal_error(const char *message, ...)
{
    fprintf(stderr, "Fatal error: ");

    va_list ap;
    va_start(ap, message);
    vfprintf(stderr, message, ap);
    va_end(ap);

    fprintf(stderr, "\n");
    exit(1);
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
