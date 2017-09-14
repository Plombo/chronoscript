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

#ifdef __cplusplus
struct pp_context
{
public:
    List<char*> macros;             // list of currently defined non-function macros
    List<CList<char>*> func_macros; // list of currently defined function-style macros
    CList<void> imports;               // list of files for the interpreter to "import"
    conditional_stack conditionals;    // the conditional stack
    int num_conditionals;              // current size of the conditional stack
public:
    pp_context();
};
#else
typedef struct pp_context pp_context;
#endif

typedef enum
{
    PP_ROOT,
    PP_INCLUDE,
    PP_NORMAL_MACRO,
    PP_FUNCTION_MACRO,
    PP_CONCATENATE,
    PP_CONDITIONAL
} pp_parser_type;

#ifdef __cplusplus
struct pp_parser
{
public:
    pp_parser_type type;
    pp_context *ctx;
    pp_lexer lexer;
    const char *filename;
    char *sourceCode;
    CList<char>* params;
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

    pp_parser(pp_context *ctx, const char *filename, char *sourceCode, TEXTPOS initialPosition);
    HRESULT lexToken(bool skipWhitespace);
    bool isDefined(const char *name);
    pp_token *emitToken();
private:
    int peekToken();
    HRESULT lexTokenEssential(bool skipWhitespace);
    HRESULT readLine(char *buf, size_t bufsize);
    HRESULT stringify();
    void concatenate(const char *token1, const char *token2);
    HRESULT parseDirective();
    HRESULT include(char *filename);
    HRESULT define(char *name);
    HRESULT conditional(const char *directive);
    HRESULT evalConditional(const char *directive, int *result);
    void insertParam(char* name);
    void insertMacro(char *name);
    HRESULT insertFunctionMacro(char *name);
    void insertBuiltinMacro(const char *name);
};
#else
typedef struct pp_parser pp_parser;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void pp_context_destroy(pp_context *self);

HRESULT pp_error(pp_parser *self, const char *format, ...);
void pp_warning(pp_parser *self, const char *format, ...);

#ifdef __cplusplus
}; // extern "C"
#endif

#endif

