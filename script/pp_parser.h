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
#include "List.hpp"
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
 * Stack of conditional directives.  The preprocessor can handle up to 32 nested
 * conditionals.  The stack is implemented as a 64-bit integer.
 */
typedef union
{
    uint64_t all;
    struct
    {
        uint64_t top:2;
        uint64_t others:62;
    };
} conditional_stack;

#ifdef __cplusplus
struct pp_context
{
public:
    List<char*> macros;                // list of currently defined non-function macros
    List<List<char*>*> func_macros;    // list of currently defined function-style macros
    List<void*> imports;               // list of files for the interpreter to "import"
    conditional_stack conditionals;    // the conditional stack
    int num_conditionals;              // current size of the conditional stack
public:
    pp_context();
    inline ~pp_context() { clear(); }

    void clear();
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
    List<char*>* params;
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
    CCResult lexToken(bool skipWhitespace);
    bool isDefined(const char *name);
    pp_token *emitToken();
private:
    int peekToken();
    CCResult lexTokenEssential(bool skipWhitespace);
    CCResult readLine(char *buf, size_t bufsize);
    CCResult stringify();
    void concatenate(const char *token1, const char *token2);
    CCResult parseDirective();
    CCResult include(char *filename);
    CCResult define(char *name);
    CCResult conditional(const char *directive);
    CCResult evalConditional(const char *directive, int *result);
    void insertParam(char* name);
    void insertMacro(char *name);
    CCResult insertFunctionMacro(char *name);
    void insertBuiltinMacro(const char *name);
};
#else
typedef struct pp_parser pp_parser;
#endif

#ifdef __cplusplus
extern "C" {
#endif

CCResult pp_error(pp_parser *self, const char *format, ...);
void pp_warning(pp_parser *self, const char *format, ...);

#ifdef __cplusplus
} // extern "C"
#endif

#endif

