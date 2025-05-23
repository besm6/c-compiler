#include "scanner.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global variables
static FILE *input_file;
static char yytext[1024]; // Buffer for current lexeme
static int yyleng = 0;    // Length of current lexeme
static int next_char;     // Lookahead character

// Function prototypes
static void consume_char(void);
static void unget_char(void);
static int is_keyword(const char *str);
static void skip_whitespace(void);
static void skip_comment(void);
static int scan_identifier(void);
static int scan_number(void);
static int scan_string(void);
static int scan_char(void);
static int scan_operator(void);
static void scan_line_marker(void);

// Current location in input file
int scanner_lineno;
char scanner_filename[1024];

// Initialize scanner with input file
void init_scanner(FILE *input)
{
    input_file = input;
    yyleng     = 0;
    yytext[0]  = '\0';
    next_char  = input_file ? fgetc(input_file) : EOF;

    if (next_char == '#') {
        consume_char();
        scan_line_marker();
    }
}

// Main lexer function
int yylex(void)
{
again:
    if (next_char == EOF) {
        return TOKEN_EOF; // End of input
    }
    skip_whitespace();
    yyleng    = 0;
    yytext[0] = '\0';
    if (next_char == EOF) {
        return TOKEN_EOF;
    }

    // Check for comments
    if (next_char == '/') {
        consume_char();
        if (next_char == '*') {
            skip_comment();
            goto again; // Recurse after skipping comment
        } else if (next_char == '/') {
            while (next_char != '\n' && next_char != EOF) {
                consume_char();
            }
            goto again; // Recurse after skipping line comment
        } else {
            unget_char();
            next_char = '/';
        }
    }

    // Scan tokens
    int token;
    if (isalpha(next_char) || next_char == '_') {
        token = scan_identifier();
    } else if (isdigit(next_char)) {
        token = scan_number();
    } else if (next_char == '"') {
        token = scan_string();
    } else if (next_char == '\'') {
        token = scan_char();
    } else {
        token = scan_operator();
    }
    return token;
}

// Consume a character and add to yytext
static void consume_char(void)
{
    if (yyleng < sizeof(yytext) - 1) {
        yytext[yyleng++] = next_char;
        yytext[yyleng]   = '\0';
    }
    next_char = fgetc(input_file);
}

// Push back the current character
static void unget_char(void)
{
    if (next_char != EOF) {
        ungetc(next_char, input_file);
        if (yyleng > 0) {
            yyleng--;
            yytext[yyleng] = '\0';
        }
    }
}

struct keyword {
    const char *name;
    int token;
};

// Comparison function for bsearch
static int compare(const void *a, const void *b)
{
    const char *key             = a;
    const struct keyword *entry = b;
    return strcmp(key, entry->name);
}

// Check if yytext is a keyword using bsearch
static int is_keyword(const char *str)
{
    static const struct {
        const char *name;
        int token;
    } keywords[] = {
        { "__func__", TOKEN_FUNC_NAME },
        { "_Alignas", TOKEN_ALIGNAS },
        { "_Alignof", TOKEN_ALIGNOF },
        { "_Atomic", TOKEN_ATOMIC },
        { "_Bool", TOKEN_BOOL },
        { "_Complex", TOKEN_COMPLEX },
        { "_Generic", TOKEN_GENERIC },
        { "_Imaginary", TOKEN_IMAGINARY },
        { "_Noreturn", TOKEN_NORETURN },
        { "_Static_assert", TOKEN_STATIC_ASSERT },
        { "_Thread_local", TOKEN_THREAD_LOCAL },
        { "auto", TOKEN_AUTO },
        { "break", TOKEN_BREAK },
        { "case", TOKEN_CASE },
        { "char", TOKEN_CHAR },
        { "const", TOKEN_CONST },
        { "continue", TOKEN_CONTINUE },
        { "default", TOKEN_DEFAULT },
        { "do", TOKEN_DO },
        { "double", TOKEN_DOUBLE },
        { "else", TOKEN_ELSE },
        { "enum", TOKEN_ENUM },
        { "extern", TOKEN_EXTERN },
        { "float", TOKEN_FLOAT },
        { "for", TOKEN_FOR },
        { "goto", TOKEN_GOTO },
        { "if", TOKEN_IF },
        { "inline", TOKEN_INLINE },
        { "int", TOKEN_INT },
        { "long", TOKEN_LONG },
        { "register", TOKEN_REGISTER },
        { "restrict", TOKEN_RESTRICT },
        { "return", TOKEN_RETURN },
        { "short", TOKEN_SHORT },
        { "signed", TOKEN_SIGNED },
        { "sizeof", TOKEN_SIZEOF },
        { "static", TOKEN_STATIC },
        { "struct", TOKEN_STRUCT },
        { "switch", TOKEN_SWITCH },
        { "typedef", TOKEN_TYPEDEF },
        { "union", TOKEN_UNION },
        { "unsigned", TOKEN_UNSIGNED },
        { "void", TOKEN_VOID },
        { "volatile", TOKEN_VOLATILE },
        { "while", TOKEN_WHILE },
    };

    // Perform binary search
    struct keyword *result = bsearch(str, keywords, sizeof(keywords) / sizeof(keywords[0]),
                                     sizeof(keywords[0]), compare);
    if (!result) {
        return 0;
    }
    return result->token;
}

// Skip whitespace
static void skip_whitespace(void)
{
    while (isspace(next_char)) {
        int c = next_char;
        consume_char();
        if (c == '\n' && next_char == '#') {
            consume_char();
            scan_line_marker();
        }
    }
}

// Skip multi-line comment
static void skip_comment(void)
{
    consume_char(); // Consume '*'
    while (next_char != EOF) {
        if (next_char == '*') {
            consume_char();
            if (next_char == '/') {
                consume_char();
                return;
            }
        } else {
            consume_char();
        }
    }
    fprintf(stderr, "Error: unterminated comment\n");
    exit(1);
}

//
// Process line markers:
//      # line_number "filename" [flag1 [flag2 ...]]
// Fields:
//  - line_number: The line number in the original source file
//                 that the following code corresponds to.
//  - filename: The name of the source file (in quotes) where
//              the code originated.
//  - flags (optional): Numeric flags that provide additional context.
//
// Common flags include:
//  1: Indicates the start of a new file
//     (e.g., after an #include).
//  2: Indicates a return to the previous file
//     (e.g., after finishing an included file).
//  3: Indicates the code is from a system header file
//     (e.g., <stdio.h>).
//  4: Indicates the code should be treated as an implicit
//     extern "C" block (relevant for C++).
//
static void scan_line_marker()
{
    // Skip whitespace after '#'
    while (isspace(next_char)) {
        // Handle newline or spaces
        if (next_char == '\n')
            return; // Empty # line (null directive)
        consume_char();
    }

    // Expect a number (line_number)
    if (!isdigit(next_char)) {
        return; // Not a line marker, just a # (null directive)
    }

    yyleng = 0;
    while (isdigit(next_char)) {
        consume_char();
    }
    yytext[yyleng] = '\0';
    int line_num = atoi(yytext); // Convert to integer

    // Skip whitespace
    while (isspace(next_char) && next_char != '\n') {
        consume_char();
        if (next_char == '\n')
            return; // No filename
    }

    // Expect a quoted filename
    if (next_char == '"') {
        yyleng = 0;
        scan_string();

        // Store in current_location
        scanner_lineno = line_num;
        strncpy(scanner_filename, yytext, sizeof(scanner_filename) - 1);
        scanner_filename[sizeof(scanner_filename) - 1] = '\0';
    }

    // Skip optional flags and rest of the line
    while (next_char != EOF && next_char != '\n') {
        consume_char();
    }
}

// Scan identifier or keyword
static int scan_identifier(void)
{
    if (next_char == 'L' || next_char == 'u' || next_char == 'U') {
        consume_char();
        if (next_char == '"') {
            // String with L/u/U prefix.
            return scan_string();
        }
        if (next_char == '\'') {
            // Character with L/u/U prefix.
            return scan_char();
        }
        if (yytext[0] == 'u' && next_char == '8') {
            consume_char();
            if (next_char == '"') {
                // String with u8 prefix.
                return scan_string();
            }
            if (next_char == '\'') {
                // Character with u8 prefix.
                return scan_char();
            }
        }
    }
    while (isalnum(next_char) || next_char == '_') {
        consume_char();
    }
    int token = is_keyword(yytext);
    if (token) {
        return token;
    }
    return TOKEN_IDENTIFIER;
}

// Scan number (integer or floating-point)
static int scan_number(void)
{
    int is_float = 0;

    // Handle hexadecimal prefix
    if (next_char == '0') {
        consume_char(); // '0'
        if (tolower(next_char) != 'x') {
            goto decimal;
        }
        consume_char(); // 'x' or 'X'
        while (isxdigit(next_char)) {
            consume_char();
        }
        if (next_char == '.') {
            is_float = 1;
            consume_char();
            while (isxdigit(next_char)) {
                consume_char();
            }
        }
        if (tolower(next_char) == 'p') {
            is_float = 1;
            consume_char();
            if (next_char == '+' || next_char == '-') {
                consume_char();
            }
            while (isdigit(next_char)) {
                consume_char();
            }
        }
    } else {
        // Decimal or octal
    decimal:
        while (isdigit(next_char)) {
            consume_char();
        }
        if (next_char == '.') {
            is_float = 1;
            consume_char();
            while (isdigit(next_char)) {
                consume_char();
            }
        }
        if (tolower(next_char) == 'e') {
            is_float = 1;
            consume_char();
            if (next_char == '+' || next_char == '-') {
                consume_char();
            }
            while (isdigit(next_char)) {
                consume_char();
            }
        }
    }

    // Handle suffixes
    while (tolower(next_char) == 'u' || tolower(next_char) == 'l' || tolower(next_char) == 'f') {
        consume_char();
    }

    return is_float ? TOKEN_F_CONSTANT : TOKEN_I_CONSTANT;
}

// Scan string literal
static int scan_string(void)
{
    if (next_char == 'L' || next_char == 'u' || next_char == 'U') {
        consume_char(); // Consume prefix
    }
    if (next_char != '"') {
        fprintf(stderr, "Error: expected string literal\n");
        exit(1);
    }
    consume_char(); // Consume opening quote
    while (next_char != '"' && next_char != '\n' && next_char != EOF) {
        if (next_char == '\\') {
            consume_char();
            if (next_char != EOF) {
                consume_char();
            }
        } else {
            consume_char();
        }
    }
    if (next_char == '"') {
        consume_char(); // Consume closing quote
    } else {
        fprintf(stderr, "Error: unterminated string\n");
        exit(1);
    }
    return TOKEN_STRING_LITERAL;
}

// Scan character literal
static int scan_char(void)
{
    if (next_char == 'L' || next_char == 'u' || next_char == 'U') {
        consume_char(); // Consume optional prefix
    }
    if (next_char != '\'') {
        fprintf(stderr, "Error: expected character literal\n");
        exit(1);
    }
    consume_char(); // Consume opening quote
    int has_content = 0;
    while (next_char != '\'' && next_char != '\n' && next_char != EOF) {
        has_content = 1;
        if (next_char == '\\') {
            consume_char();
            // Handle escape sequences
            if (next_char == '\'' || next_char == '"' || next_char == '?' || next_char == '\\' ||
                next_char == 'a' || next_char == 'b' || next_char == 'f' || next_char == 'n' ||
                next_char == 'r' || next_char == 't' || next_char == 'v') {
                consume_char();
            } else if (next_char >= '0' && next_char <= '7') {
                consume_char();
                if (next_char >= '0' && next_char <= '7') {
                    consume_char();
                    if (next_char >= '0' && next_char <= '7') {
                        consume_char();
                    }
                }
            } else if (next_char == 'x') {
                consume_char();
                while (isxdigit(next_char)) {
                    consume_char();
                }
            } else {
                consume_char(); // Consume invalid escape
            }
        } else {
            consume_char();
        }
    }
    if (next_char == '\'') {
        consume_char(); // Consume closing quote
        if (!has_content) {
            fprintf(stderr, "Error: empty character literal\n");
            exit(1);
        }
    } else {
        fprintf(stderr, "Error: unterminated character literal\n");
        exit(1);
    }
    return TOKEN_I_CONSTANT;
}

// Scan operators and punctuation
static int scan_operator(void)
{
    int c = next_char;
    consume_char();
    int c2 = next_char;

    // Multi-character operators
    if (c == '.' && c2 == '.') {
        consume_char();
        if (next_char != '.') {
            fprintf(stderr, "Error: bad ellipsis\n");
            exit(1);
        }
        consume_char();
        return TOKEN_ELLIPSIS;
    }
    if (c == '>' && c2 == '>') {
        consume_char();
        if (next_char == '=') {
            consume_char();
            return TOKEN_RIGHT_ASSIGN;
        }
        return TOKEN_RIGHT_OP;
    }
    if (c == '<' && c2 == '<') {
        consume_char();
        if (next_char == '=') {
            consume_char();
            return TOKEN_LEFT_ASSIGN;
        }
        return TOKEN_LEFT_OP;
    }
    if (c == '+' && c2 == '=') {
        consume_char();
        return TOKEN_ADD_ASSIGN;
    }
    if (c == '-' && c2 == '=') {
        consume_char();
        return TOKEN_SUB_ASSIGN;
    }
    if (c == '*' && c2 == '=') {
        consume_char();
        return TOKEN_MUL_ASSIGN;
    }
    if (c == '/' && c2 == '=') {
        consume_char();
        return TOKEN_DIV_ASSIGN;
    }
    if (c == '%' && c2 == '=') {
        consume_char();
        return TOKEN_MOD_ASSIGN;
    }
    if (c == '&' && c2 == '=') {
        consume_char();
        return TOKEN_AND_ASSIGN;
    }
    if (c == '^' && c2 == '=') {
        consume_char();
        return TOKEN_XOR_ASSIGN;
    }
    if (c == '|' && c2 == '=') {
        consume_char();
        return TOKEN_OR_ASSIGN;
    }
    if (c == '+' && c2 == '+') {
        consume_char();
        return TOKEN_INC_OP;
    }
    if (c == '-' && c2 == '-') {
        consume_char();
        return TOKEN_DEC_OP;
    }
    if (c == '-' && c2 == '>') {
        consume_char();
        return TOKEN_PTR_OP;
    }
    if (c == '&' && c2 == '&') {
        consume_char();
        return TOKEN_AND_OP;
    }
    if (c == '|' && c2 == '|') {
        consume_char();
        return TOKEN_OR_OP;
    }
    if (c == '<' && c2 == '=') {
        consume_char();
        return TOKEN_LE_OP;
    }
    if (c == '>' && c2 == '=') {
        consume_char();
        return TOKEN_GE_OP;
    }
    if (c == '=' && c2 == '=') {
        consume_char();
        return TOKEN_EQ_OP;
    }
    if (c == '!' && c2 == '=') {
        consume_char();
        return TOKEN_NE_OP;
    }

    // Single-character tokens
    switch (c) {
    case ';':
        return TOKEN_SEMICOLON;
    case '{':
        return TOKEN_LBRACE;
    case '}':
        return TOKEN_RBRACE;
    case ',':
        return TOKEN_COMMA;
    case ':':
        return TOKEN_COLON;
    case '=':
        return TOKEN_ASSIGN;
    case '(':
        return TOKEN_LPAREN;
    case ')':
        return TOKEN_RPAREN;
    case '[':
        return TOKEN_LBRACKET;
    case ']':
        return TOKEN_RBRACKET;
    case '.':
        return TOKEN_DOT;
    case '&':
        return TOKEN_AMPERSAND;
    case '!':
        return TOKEN_NOT;
    case '~':
        return TOKEN_TILDE;
    case '-':
        return TOKEN_MINUS;
    case '+':
        return TOKEN_PLUS;
    case '*':
        return TOKEN_STAR;
    case '/':
        return TOKEN_SLASH;
    case '%':
        return TOKEN_PERCENT;
    case '<':
        return TOKEN_LT;
    case '>':
        return TOKEN_GT;
    case '^':
        return TOKEN_CARET;
    case '|':
        return TOKEN_PIPE;
    case '?':
        return TOKEN_QUESTION;
    default:
        // Unknown character.
        return TOKEN_UNKNOWN;
    }
}

// Get current lexeme
char *get_yytext(void)
{
    return yytext;
}
