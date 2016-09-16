/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2013 OpenBOR Team
 */

/**
 * This is the parser for the script preprocessor.  Its purpose is to emit the
 * preprocessed source code for use by scriptlib.  It is not derived from the
 * parser in scriptlib because it does something entirely different.
 *
 * @author Plombo
 * @date 15 October 2010
 */

#ifndef PP_PARSER_H
#define PP_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pp_lexer.h"
#include "List.h"
#include "depends.h" // #include "types.h"

#define MACRO_CONTENTS_SIZE		512

/*
 * The current "state" of a single conditional (#ifdef/#else/#endif) sequence:
 * true, false, already completed (i.e. not eligible for #elif), or non-existent.
 */
enum conditional_state
{
    cs_none = 0,
    cs_true = 1,
    cs_false = 2,
    cs_done = 3
};

/**
 * Stack of conditional directives.  The preprocessor can handle up to 16 nested
 * conditionals.  The stack is implemented as a 32-bit integer.
 */
typedef union
{
    u64 all;
    struct
    {
        u64 top: 2;
        u64 others: 62;
    };
} conditional_stack;

typedef struct pp_context
{
    List macros;                       // list of currently defined non-function macros
    List func_macros;                  // list of currently defined function-style macros
    List imports;                      // list of files for the interpreter to "import"
    conditional_stack conditionals;    // the conditional stack
    int num_conditionals;              // current size of the conditional stack
} pp_context;

typedef enum
{
    PP_ROOT,
    PP_INCLUDE,
    PP_NORMAL_MACRO,
    PP_FUNCTION_MACRO,
    PP_CONCATENATE,
    PP_CONDITIONAL
} pp_parser_type;

typedef struct pp_parser
{
    pp_parser_type type;
    pp_context *ctx;
    pp_lexer lexer;
    const char *filename;
    char *sourceCode;
    List* params;
    int numParams;                     // parameter macros defined for a function macro parser
    char *macroName;
    bool freeFilename;
    bool freeSourceCode;
    pp_token token;
    pp_token last_token;
    struct pp_parser *parent;
    struct pp_parser *child;
    bool newline;
    bool overread;
} pp_parser;

void pp_context_init(pp_context *self);
void pp_context_destroy(pp_context *self);

void pp_parser_init(pp_parser *self, pp_context *ctx, const char *filename, char *sourceCode, TEXTPOS initialPosition);
pp_token *pp_parser_emit_token(pp_parser *self);
HRESULT pp_parser_lex_token(pp_parser *self, bool skip_whitespace);
bool pp_parser_is_defined(pp_parser *self, const char *name);

HRESULT pp_error(pp_parser *self, const char *format, ...);
void pp_warning(pp_parser *self, const char *format, ...);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

