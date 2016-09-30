/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2013 OpenBOR Team
 */

/**
 * This is the parser for the script preprocessor.  Its purpose is to emit the
 * preprocessed source code for use by scriptlib.  It is not related to the
 * parser in scriptlib because it does something entirely different.
 *
 * @author Plombo
 * @date 15 October 2010
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <malloc.h>
#include <errno.h>
#include "List.h"
#include "pp_parser.h"
#include "pp_expr.h"
//#include "borendian.h"

#define PP_TEST 1

#if PP_TEST // using pp_test.c to test the preprocessor functionality; OpenBOR functionality is not available
#undef printf
#define openpackfile(fname, pname)    ((size_t)fopen(fname, "rb"))
#define readpackfile(hnd, buf, len)    fread(buf, 1, len, (FILE*)hnd)
#define seekpackfile(hnd, loc, md)    fseek((FILE*)hnd, loc, md)
#define tellpackfile(hnd)            ftell((FILE*)hnd)
#define closepackfile(hnd)            fclose((FILE*)hnd)
#define printf(msg, args...)        fprintf(stderr, msg, ##args)
#define shutdown(ret, msg, args...) { fprintf(stderr, msg, ##args); exit(ret); }
char *get_full_path(char *filename) { return filename; };
#else // otherwise, we can use OpenBOR functionality like writeToLogFile
#include "openbor.h"
#include "globals.h"
#include "packfile.h"
#define tellpackfile(hnd)            seekpackfile(hnd, 0, SEEK_CUR)
#undef time
#endif

static bool pp_is_builtin_macro(const char *name);

static void freeParams(void *data)
{
    CList<char> *params = (CList<char>*) data;
    while(params->size() > 0)
    {
        free(params->retrieve());
        params->remove();
    }
    params->clear();
    delete params;
}

/**
 * Initializes a preprocessor context.  Assumes that this context either hasn't
 * been initialized yet or has been destroyed since the last time it was initialized.
 */
pp_context::pp_context()
    : macros(CFUHASH_FREE_DATA), func_macros(CFUHASH_FREE_DATA)
{
    char buf[64];
    time_t currentTime = time(NULL);
    char *datetime = ctime(&currentTime);

    func_macros.setFreeFunction(freeParams);

    // initialize the conditional stack
    conditionals.all = 0ll;
    num_conditionals = 0;

    // initialize the builtin macros other than __FILE__ and __LINE__
    strcpy(buf, "\"");
    strncat(buf, datetime + 4, 7);
    strncat(buf, datetime + 20, 4);
    strcat(buf, "\"");
    macros.put("__DATE__", strdup(buf));

    strcpy(buf, "\"");
    strncat(buf, datetime + 11, 8);
    strcat(buf, "\"");
    macros.put("__TIME__", strdup(buf));

#if PP_TEST
    macros.put("__STDC__", strdup("1"));
    macros.put("__STDC_HOSTED__", strdup("1"));
    macros.put("__STDC_VERSION__", strdup("199901L"));
#endif
}

/**
 * Frees the memory associated with a preprocessor context.  This function can
 * safely be called multiple times on the same context with no negative
 * consequences.  However, it does assume that the context has been initialized
 * at least once.
 */
extern "C" void pp_context_destroy(pp_context *self)
{
    // free the import list
    self->imports.clear();
}

/**
 * Initializes a preprocessor (pp_parser) object.
 * @param ctx the shared context used by this parser
 * @param filename the name of the file to parse
 * @param sourceCode the source code to parse, in string form
 */
pp_parser::pp_parser(pp_context *ctx, const char *filename, char *sourceCode, TEXTPOS initialPosition)
{
    pp_lexer_Init(&lexer, sourceCode, initialPosition);
    this->ctx = ctx;
    this->filename = filename;
    this->sourceCode = sourceCode;

    type = PP_ROOT;
    freeFilename = false;
    freeSourceCode = false;
    parent = NULL;
    child = NULL;
    numParams = 0;
    params = NULL;
    macroName = NULL;
    newline = true;
    overread = false;
}

/**
 * Allocates a subparser, used for including files and expanding macros.
 * @param parent the parent parser of this subparser
 * @param filename the name of the file to parse
 * @param sourceCode the source code to parse, in string form
 */
static pp_parser *pp_parser_alloc(pp_parser *parent, const char *filename, char *sourceCode, pp_parser_type type)
{
    TEXTPOS initialPos = {1, 0};
    pp_parser *self = new pp_parser(parent->ctx, filename, sourceCode, initialPos);
    self->type = type;
    self->parent = parent;
    parent->child = self;

    return self;
}

/**
 * Allocates a subparser to expand a macro.  This is a convenience constructor.
 * @param self the object
 * @param parent the parent parser
 * @param numParams number of macros to free from the start of the macro list
 *        when freeing this parser (0 for non-function macros)
 * @return the newly allocated parser
 */
static pp_parser* pp_parser_alloc_macro(pp_parser* parent, char* macroContents, CList<char>* params, pp_parser_type type)
{
    pp_parser *self = pp_parser_alloc(parent, parent->filename, macroContents, type);
    self->params = params;
    self->numParams = params ? params->size() : 0;
    return self;
}

/**
 * Prints an error or warning message to the log file.
 * @param messageType the type of message ("error" or "warning")
 * @param message the actual message to display
 */
static void pp_message(pp_parser *self, const char *messageType, const char *message)
{
    pp_parser *parser = self;
    char buf[1024] = {""}, *linePtr;
    int i;
    size_t bufPos;

    printf("\n\n");

    // print a backtrace of #includes to make it clear exactly where this error/warning is occurring
    while(parser->parent)
    {
        parser = parser->parent;
    }
    while(parser->child && parser->child->type == PP_INCLUDE)
    {
        printf("In file included from %s, line %d:\n", parser->filename, parser->lexer.theTokenPosition.row);
        parser = parser->child;
    }

    // print the error/warning and its location
    printf("Script %s: %s, line %d: %s\n", messageType, parser->filename, parser->lexer.theTokenPosition.row, message);

    // find the start of the line that the error occurred on
    linePtr = parser->lexer.pcurChar - strlen(parser->lexer.theTokenSource);
    while(linePtr-- > parser->lexer.ptheSource)
        if(*linePtr == '\n' || *linePtr == '\r' || *linePtr == '\f')
        {
            break;
        }
    linePtr++;

    // write the line that the error/warning occurred on to the log file
    bufPos = 0;
    while(*linePtr != '\n' && *linePtr != '\r' && *linePtr != '\f' && *linePtr != '\0')
    {
        buf[bufPos++] = *linePtr++;
        if(bufPos >= sizeof(buf) - 1)
        {
            break;
        }
    }
    buf[bufPos] = '\0';
    printf("\n%s\n", buf);

    // print a position marker to show where in the line the error/warning occurred, just for completeness :)
    for(i = 0; i < parser->token.theTextPosition.col; i++)
    {
        printf(" ");
    }
    printf("^\n\n");
}

/**
 * Writes an error message to the log.
 * @return E_FAIL
 */
HRESULT pp_error(pp_parser *self, const char *format, ...)
{
    char buf[1024] = {""};
    va_list arglist;

    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);
    pp_message(self, "error", buf);

    return E_FAIL;
}

/**
 * Writes a warning message to the log.
 */
void pp_warning(pp_parser *self, const char *format, ...)
{
    char buf[1024] = {""};
    va_list arglist;

    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);
    pp_message(self, "warning", buf);
}

/**
 * Gets the next parsable token from the lexer.
 * @param skip_whitespace true to ignore whitespace, false otherwise
 */
HRESULT pp_parser::lexToken(bool skipWhitespace)
{
    bool success = true;

    while (success)
    {
        if (overread)
        {
            memcpy(&token, &last_token, sizeof(pp_token));
            overread = false;
            success = true;
        }
        else
        {
            success = SUCCEEDED(pp_lexer_GetNextToken(&lexer, &token));
        }

        if (!success)
        {
            return E_FAIL;
        }

        if (skipWhitespace && token.theType == PP_TOKEN_WHITESPACE)
        {
            continue;
        }
        else
        {
            break;
        }
    }

    memcpy(&last_token, &token, sizeof(pp_token));
    return S_OK;
}

/**
 * Returns the type of the next non-whitespace token in the lexer's stream
 * without permanently changing the state of the parser or its lexer, or -1 if
 * there is a lexical error.
 */
int pp_parser::peekToken()
{
    pp_lexer tempLexer;
    pp_token tempToken;

    memcpy(&tempLexer, &this->lexer, sizeof(pp_lexer));

    while (true)
    {
        if (FAILED(pp_lexer_GetNextToken(&this->lexer, &tempToken)))
        {
            memcpy(&this->lexer, &tempLexer, sizeof(pp_lexer));
            return -1;
        }
        if (tempToken.theType != PP_TOKEN_WHITESPACE)
        {
            break;
        }
    }

    memcpy(&this->lexer, &tempLexer, sizeof(pp_lexer));
    return tempToken.theType;
}

/**
 * Gets the next parsable token from the lexer, using a token from a parent
 * lexer if necessary.  This is useful for expanding macros.
 * @param skip_whitespace true to ignore whitespace, false otherwise
 */
HRESULT pp_parser::lexTokenEssential(bool skipWhitespace)
{
    pp_parser *parser = this;

    while(1)
    {
        if(FAILED(parser->lexToken(skipWhitespace)))
        {
            return E_FAIL;
        }

        if(parser->token.theType == PP_TOKEN_EOF && parser->parent)
        {
            parser->overread = true;
            parser = parser->parent;
            continue;
        }
        else
        {
            break;
        }
    }

    if(parser != this)
    {
        memcpy(&this->token, &parser->token, sizeof(pp_token));
    }

    return S_OK;
}

/**
 * Preprocesses the source file until it reaches a token that should be emitted.
 * @return the next token of the output
 */
pp_token *pp_parser::emitToken()
{
    bool emitme = false;
    bool success = true;
    pp_token token2;
    pp_token *child_token;
    int i;

    while (success && !emitme)
    {
        // get token from subparser ("child") if one exists
        if (child)
        {
            child_token = child->emitToken();

            if (child_token == NULL || child_token->theType == PP_TOKEN_EOF)
            {
                // free the parameters of function macros
                if (child->type == PP_FUNCTION_MACRO)
                {
                    child->params->gotoFirst();
                    for(i=0; i<child->numParams; i++)
                    {
                        free(child->params->retrieve());
                        child->params->remove();
                    }
                    child->params->clear();
                    delete child->params;
                }

                // free the source code and filename if necessary
                if (child->freeFilename)
                {
                    free((void *)child->filename);
                }
                if (child->freeSourceCode)
                {
                    free(child->sourceCode);
                }
                if (child->macroName)
                {
                    free(child->macroName);
                }

                delete child;
                child = NULL;

                if (child_token == NULL)
                {
                    return NULL;
                }
                else
                {
                    continue;
                }
            }
            else
            {
                memcpy(&this->token, child_token, sizeof(pp_token));
                emitme = true;
            }
        }

        if (emitme) // re-process tokens emitted by the subparser
        {
            emitme = false;
        }
        else if (FAILED(lexToken(false))) // lex the next token if no token is obtained from the subparser
        {
            break;
        }

        if (this->token.theType == PP_TOKEN_DIRECTIVE ||
            ctx->conditionals.all == (ctx->conditionals.all & 0x5555555555555555ll))
        {
            // handle token concatenation
            if(type == PP_FUNCTION_MACRO || type == PP_NORMAL_MACRO)
            {
                pp_parser* p;
                pp_lexer tmpLexer;
                pp_token tokenA, tokenB;
                char *param1, *param2;
                char buf1[1024] = {""}, buf2[1024] = {""};
                TEXTPOS start = {0,0};

                if (peekToken() == PP_TOKEN_CONCATENATE)
                {
                    memcpy(&token2, &this->token, sizeof(pp_token));
                    success = SUCCEEDED(lexToken(true)); // lex '##'
                    assert(this->token.theType == PP_TOKEN_CONCATENATE);
                    if (!success)
                    {
                        break;
                    }
                    success = SUCCEEDED(lexToken(true)); // lex second token
                    if (!success)
                    {
                        break;
                    }

                    if (this->token.theType == PP_TOKEN_EOF)
                    {
                        pp_error(this, "'##' at end of macro expansion");
                        return NULL;
                    }

                    // expand the token on the left side of the ##
                    param1 = token2.theSource;
                    p = this;
                    buf1[0] = 0;
                    while (true)
                    {
                        if (p->type == PP_FUNCTION_MACRO && p->params->findByName(param1))
                        {
                            param1 = p->params->retrieve();
                            p = p->parent;
                        }
                        else break;

                        // if the expanded form has >1 token, the concatenation is only
                        // applied to the last token, so only expand it for now
                        pp_lexer_Init(&tmpLexer, param1, start);
                        tokenB.theSource[0] = '\0';
                        do
                        {
                            pp_lexer_GetNextToken(&tmpLexer, &tokenA);
                            if (tokenA.theType == PP_TOKEN_EOF) break;
                            // printf("cat '%s' to buf1\n", tokenB.theSource);
                            strcat(buf1, tokenB.theSource);
                            memcpy(&tokenB, &tokenA, sizeof(pp_token));
                        } while (true);
                        param1 = param1 + strlen(param1) - strlen(tokenB.theSource);
                        assert(strcmp(param1, tokenB.theSource) == 0);
                    }
                    strcat(buf1, param1);

                    param2 = this->token.theSource;
                    p = this;
                    while (true)
                    {
                        if (p->type == PP_FUNCTION_MACRO && p->params->findByName(param2))
                        {
                            param2 = (char*)p->params->retrieve();
                            p = p->parent;
                        }
                        else break;

                        // this is ugly but works as long as buf2 doesn't overflow
                        pp_lexer_Init(&tmpLexer, param2, start);
                        pp_lexer_GetNextToken(&tmpLexer, &tokenA);
                        memmove(buf2+strlen(tmpLexer.pcurChar), buf2, strlen(buf2)+1);
                        memcpy(buf2, tmpLexer.pcurChar, strlen(tmpLexer.pcurChar));
                        param2 = tokenA.theSource; // is this safe?
                    }
                    memmove(buf2+strlen(param2), buf2, strlen(buf2)+1);
                    memcpy(buf2, param2, strlen(param2));

                    // printf("buf1='%s' buf2='%s'\n", buf1, buf2);
                    concatenate(buf1, buf2);
                    emitme = false;
                    continue;
                }
            }

            switch (this->token.theType)
            {
            case PP_TOKEN_DIRECTIVE:
                if (this->type == PP_FUNCTION_MACRO)
                {
                    success = SUCCEEDED(lexToken(true));
                    if (!success)
                    {
                        break;
                    }

                    if (this->token.theType == PP_TOKEN_IDENTIFIER &&
                        params->findByName(this->token.theSource))
                    {
                        if (FAILED(stringify()))
                        {
                            return NULL;
                        }
                        emitme = true;
                        break;
                    }
                    else
                    {
                        overread = true;
                    }
                }
                else if (newline)
                {
                    /* only parse the "#" symbol when it's at the beginning of a
                     * line (ignoring whitespace) */
                    if (FAILED(parseDirective()))
                    {
                        return NULL;
                    }
                    break;
                }

                // if none of the above cases are true, emit the token
                emitme = true;
                break;
            case PP_TOKEN_NEWLINE:
                if (!newline)
                {
                    emitme = true;
                }
                newline = true;
                break;
            case PP_TOKEN_WHITESPACE:
                emitme = true;
                // whitespace doesn't affect the newline property
                break;
            case PP_TOKEN_IDENTIFIER:
                memcpy(&token2, &this->token, sizeof(pp_token)); // FIXME: this should be moved into the function macro else if block
                newline = false;

                if (this->type == PP_FUNCTION_MACRO && params->findByName(this->token.theSource))
                {
                    insertParam(this->token.theSource);
                }
                else if (!strcmp(this->token.theSource, "__FILE__") || !strcmp(this->token.theSource, "__LINE__"))
                {
                    insertBuiltinMacro(token2.theSource);
                }
                else if (peekToken() == PP_TOKEN_LPAREN &&
                         ctx->func_macros.containsKey(token2.theSource))
                {
                    success = SUCCEEDED(lexToken(true));
                    assert(this->token.theType == PP_TOKEN_LPAREN);
                    if (!success)
                    {
                        break;
                    }
                    if (FAILED(insertFunctionMacro(token2.theSource)))
                    {
                        return NULL;
                    }
                }
                else if (ctx->macros.containsKey(token2.theSource))
                {
                    insertMacro(token2.theSource);
                }
                else
                {
                    emitme = true;
                }
                break;
            default: // now includes EOF
                emitme = true;
                newline = false;
            }
        }
    }

    return success ? &this->token : NULL;
}

// this->token contains the first token of the macro/message if this->overread is true
HRESULT pp_parser::readLine(char *buf, size_t bufsize)
{
    int total_length = 1;

    if(FAILED(lexToken(true)))
    {
        return E_FAIL;
    }
    buf[0] = '\0';

    while(1)
    {
        if (token.theType == PP_TOKEN_EOF)
        {
            overread = true;
            break;
        }
        else if (token.theType == PP_TOKEN_NEWLINE)
        {
            break;
        }

        if((total_length + strlen(token.theSource)) > bufsize)
        {
            // Prevent buffer overflow
            pp_error(this, "length of macro or message contents is too long; must be <= %i characters", bufsize);
            return E_FAIL;
        }

        strcat(buf, token.theSource);
        total_length += strlen(token.theSource);
        if(FAILED(lexToken(false)))
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

/**
 * Implements the C "stringify" operator.
 */
HRESULT pp_parser::stringify()
{
    const char *source = (const char*) params->retrieve();
    bool in_string = false;
    pp_token_Init(&token, PP_TOKEN_STRING_LITERAL, "\"", token.theTextPosition, 0);

    while(*source)
    {
        if(*source == '"')
        {
            strcat(token.theSource, "\\\"");
            in_string = !in_string;
        }
        else if(*source == '\\' && in_string)
        {
            strcat(token.theSource, "\\\\");
            // handle escaped quotation mark
            if (source[1] == '"')
            {
                strcat(token.theSource, "\\\"");
                source++;
            }
        }
        else
        {
            strncat(token.theSource, source, 1);
        }

        if(strlen(token.theSource) + 2 > MAX_TOKEN_LENGTH)
        {
            return pp_error(this, "sequence is too long to stringify");
        }

        source++;
    }

    strncat(token.theSource, "\"", 1);
    return S_OK;
}

/**
 * Concatenates two tokens together, implementing the "##" operator.
 * TODO: concatenation still needs work
 * @param token1 the contents of the first token
 * @param token2 the contents of the second token
 */
void pp_parser::concatenate(const char *token1, const char *token2)
{
    char *output = (char*) malloc(strlen(token1) + strlen(token2) + 1);
    pp_parser *outputParser;

    sprintf(output, "%s%s", token1, token2);
    outputParser = pp_parser_alloc(this, filename, output, PP_CONCATENATE);
    outputParser->freeSourceCode = true;
}

/**
 * Parses a C preprocessor directive.  When this function is called, the token
 * '#' has just been detected by the compiler.
 */
HRESULT pp_parser::parseDirective()
{
    char directiveName[MAX_TOKEN_LENGTH + 1] = {""};

    if(FAILED(lexToken(true)))
    {
        return E_FAIL;
    }

    strcpy(directiveName, token.theSource);

    // most directives shouldn't be parsed if we're in the middle of a conditional false
    if(ctx->conditionals.all > (ctx->conditionals.all & 0x5555555555555555ll))
    {
        if(!(!strcmp(directiveName, "if") ||
             !strcmp(directiveName, "ifdef") ||
             !strcmp(directiveName, "ifndef") ||
             !strcmp(directiveName, "elif") ||
             !strcmp(directiveName, "else") ||
             !strcmp(directiveName, "endif")))
        {
            return S_OK;
        }
    }

    if (!strcmp(directiveName, "include") || !strcmp(directiveName, "import"))
    {
        char filename[128] = {""};
        bool isImport = !strcmp(directiveName, "import");

        if(FAILED(lexToken(true)))
        {
            return E_FAIL;
        }

#if PP_TEST
        if(token.theType == PP_TOKEN_LT)
        {
            char *end;
            if(FAILED(readLine(filename, sizeof(filename))))
            {
                return pp_error(this, "unable to read rest of line?");
            }
            end = strrchr(filename, '>');
            if(end == NULL)
            {
                return pp_error(this, "invalid #include syntax");
            }
            *end = '\0';
        }
        else
#endif
            if(token.theType == PP_TOKEN_STRING_LITERAL)
            {
                strcpy(filename, this->token.theSource + 1); // trim first " mark
                filename[strlen(filename) - 1] = '\0'; // trim last " mark
            }
            else
            {
                return pp_error(this, "#include not followed by a path string");
            }

        if (isImport)
        {
            ctx->imports.insertAfter(NULL, filename);
            return S_OK;
        }
        else
        {
            return include(filename);
        }
    }
    else if (!strcmp(directiveName, "define"))
    {
        char name[MAX_TOKEN_LENGTH];

        if(FAILED(lexToken(true)))
        {
            return E_FAIL;
        }
        if(token.theType != PP_TOKEN_IDENTIFIER)
        {
            // Macro must have at least a name
            return pp_error(this, "no macro name given in #define directive");
        }

        // Parse macro name and contents
        strcpy(name, token.theSource);
        return define(name);
    }
    else if (!strcmp(directiveName, "undef"))
    {
        if(FAILED(lexToken(true)))
        {
            return E_FAIL;
        }
        if(pp_is_builtin_macro(token.theSource))
        {
            return pp_error(this, "'%s' is a builtin macro and cannot be undefined", token.theSource);
        }
        if(ctx->macros.containsKey(token.theSource))
        {
            ctx->macros.remove(token.theSource);
        }
        if(ctx->func_macros.containsKey(token.theSource))
        {
            ctx->func_macros.remove(token.theSource);
        }

        return S_OK;
    }
    else if (!strcmp(directiveName, "if") ||
             !strcmp(directiveName, "ifdef") ||
             !strcmp(directiveName, "ifndef") ||
             !strcmp(directiveName, "elif") ||
             !strcmp(directiveName, "else") ||
             !strcmp(directiveName, "endif"))
    {
        return conditional(directiveName);
    }
    else if (!strcmp(directiveName, "warning") || !strcmp(directiveName, "error"))
    {
        char text[256] = {""};

        // this->token is about to be clobbered, so save whether this is a warning or error
        if(FAILED(readLine(text, sizeof(text))))
        {
            return E_FAIL;
        }

        if (!strcmp(directiveName, "warning"))
        {
            pp_warning(this, "#warning %s", text);
        }
        else
        {
            return pp_error(this, "#error %s", text);
        }
    }
    else if (token.theType == PP_TOKEN_NEWLINE)
    {
        // null directive - do nothing
        return S_OK;
    }
    else
    {
        return pp_error(this, "unknown directive '#%s'", directiveName);
    }

    return S_OK;
}

/**
 * Includes a source file specified with the #include directive.
 * @param filename the path to include
 */
HRESULT pp_parser::include(char *filename)
{
    char *buffer;
    int length;
    int bytesRead;
    size_t handle;
    pp_parser *includeParser;

#if PP_TEST
    filename = get_full_path(filename);
#endif

    // Open the file
    handle = openpackfile(filename, packfile);
#if PP_TEST
    if(!handle)
#else
    if(handle < 0)
#endif
    {
        return pp_error(this, "unable to open file '%s'", filename);
    }

    // Determine the file's size
    seekpackfile(handle, 0, SEEK_END);
    length = tellpackfile(handle);
    seekpackfile(handle, 0, SEEK_SET);

    // Allocate a buffer for the file's contents
    buffer = (char*) malloc(length + 1);
    memset(buffer, 0, length + 1);

    // Read the file into the buffer
    bytesRead = readpackfile(handle, buffer, length);
    closepackfile(handle);

    if(bytesRead != length)
    {
        free(buffer);
        return pp_error(this, "I/O error: %s", strerror(errno));
    }

    // Allocate a subparser for the included file
    includeParser = pp_parser_alloc(this, NAME(filename), buffer, PP_INCLUDE);
    includeParser->freeFilename = true;
    includeParser->freeSourceCode = true;

    return S_OK;
}

/**
 * Defines a macro specified with the #define directive.
 * The length of the contents is limited to MACRO_CONTENTS_SIZE (512) characters.
 * @param name the macro name
 */
HRESULT pp_parser::define(char *name)
{
    char *contents = NULL;
    bool is_function = false; // true if this is a function-style #define; false otherwise
    CList<char> *funcParams = new(NULL) CList<char>();

    // emit an error if the macro is already defined
    if(!strcmp(name, "defined"))
    {
        pp_error(this, "'defined' can't be defined as a macro");
        goto error;
    }
    if(pp_is_builtin_macro(name))
    {
        pp_error(this, "'%s' is a builtin macro and cannot be redefined", name);
        goto error;
    }

    // emit a warning if the macro is already defined
    // note: this won't mess with function macro parameters since #define can't be used from inside a macro
    if(ctx->macros.containsKey(name))
    {
        pp_warning(this, "'%s' redefined (previously \"%s\")", name, ctx->macros.get(name));
    }
    if (ctx->func_macros.containsKey(name))
    {
        CList<char> *params2 = ctx->func_macros.get(name);
        params2->gotoLast();
        pp_warning(this, "'%s' redefined (previously \"%s\")", name, params2->retrieve());
    }

    // do NOT skip whitespace here - the '(' must come immediately after the name!
    if(FAILED(lexToken(false)))
    {
        goto error;
    }

    if(token.theType == PP_TOKEN_LPAREN) // function-style #define
    {
        is_function = true;
        if(FAILED(lexToken(true)))
        {
            goto error;
        }
        while(token.theType != PP_TOKEN_RPAREN)
        {
            switch(token.theType)
            {
                // All of the below types are technically valid macro names
            case PP_TOKEN_IDENTIFIER:
                funcParams->insertAfter(NULL, token.theSource);
                if(FAILED(lexToken(true)))
                {
                    goto error;
                }
                if(token.theType == PP_TOKEN_COMMA || token.theType == PP_TOKEN_RPAREN)
                {
                    break;
                }
            default:
                return pp_error(this, "unexpected token '%s' in #define parameter list", token.theSource);
            }

            if(token.theType != PP_TOKEN_RPAREN)
            {
                lexToken(true);
            }
        }
        overread = false;
    }
    else
    {
        overread = true;
    }

    // Read macro contents
    contents = (char*) malloc(MACRO_CONTENTS_SIZE);
    contents[0] = '\0';
    if(FAILED(readLine(contents, MACRO_CONTENTS_SIZE)))
    {
        goto error;
    }

    // Add macro to the correct list, either macros or func_macros
    if(is_function)
    {
        funcParams->insertAfter(contents, NULL);
        ctx->func_macros.put(name, funcParams);
    }
    else
    {
        delete funcParams;
        ctx->macros.put(name, contents);
    }

    return S_OK;

error:
    funcParams->gotoFirst();
    funcParams->clear();
    delete funcParams;
    if(contents)
    {
        free(contents);
    }
    return E_FAIL;
}

/**
 * Handles conditional directives.
 * @param directive the type of conditional directive
 */
HRESULT pp_parser::conditional(const char *directive)
{
    int result;
    if (!strcmp(directive, "if") || !strcmp(directive, "ifdef") || !strcmp(directive, "ifndef"))
    {
        if(ctx->num_conditionals++ > 32)
        {
            return pp_error(this, "too many levels of nested conditional directives");
        }
        ctx->conditionals.all <<= 2; // push a new conditional state onto the stack
        if(ctx->conditionals.others > (ctx->conditionals.others & 0x5555555555555555ll))
        {
            ctx->conditionals.top = cs_false;
            return S_OK;
        }
        if(FAILED(evalConditional(directive, &result)))
        {
            return E_FAIL;
        }
        ctx->conditionals.top = result ? cs_true : cs_false;
    }
    else if (!strcmp(directive, "elif"))
    {
        if(ctx->conditionals.top == cs_none)
        {
            return pp_error(this, "stray #elif");
        }
        if(ctx->conditionals.top == cs_done || ctx->conditionals.top == cs_true)
        {
            ctx->conditionals.top = cs_done;
        }
        else if(ctx->conditionals.others == (ctx->conditionals.others & 0x5555555555555555ll))
        {
            // unconditionally enable parsing long enough to parse the condition
            ctx->conditionals.top = cs_true;
            if(FAILED(evalConditional(directive, &result)))
            {
                return E_FAIL;
            }
            ctx->conditionals.top = result ? cs_true : cs_false;
        }
    }
    else if (!strcmp(directive, "else"))
    {
        if(ctx->conditionals.top == cs_none)
        {
            return pp_error(this, "stray #else");
        }
        ctx->conditionals.top = (ctx->conditionals.top == cs_false) ? cs_true : cs_done;
    }
    else if (!strcmp(directive, "endif"))
    {
        if(ctx->conditionals.top == cs_none || ctx->num_conditionals-- < 0)
        {
            return pp_error(this, "stray #endif");
        }
        ctx->conditionals.all >>= 2; // pop a conditional state from the stack
    }
    else
    {
        return pp_error(this, "unknown conditional directive type '%s'", directive);
    }

    return S_OK;
}

HRESULT pp_parser::evalConditional(const char *directive, int *result) // FIXME: shouldn't result be bool?
{
    if (!strcmp(directive, "ifdef") || !strcmp(directive, "ifndef"))
    {
        if (FAILED(lexToken(true)))
        {
            return E_FAIL;
        }
        *result = ctx->macros.containsKey(token.theSource);
        if (!strcmp(directive, "ifndef"))
        {
            *result = !(*result);
        }
        return S_OK;
    }
    if (!strcmp(directive, "if") || !strcmp(directive, "elif"))
    {
        pp_parser *subparser;
        char buf[1024];

        if(FAILED(readLine(buf, sizeof(buf))))
        {
            return E_FAIL;
        }
        subparser = pp_parser_alloc(this, filename, buf, PP_CONDITIONAL);
        //subparser->parent = NULL;
        subparser->lexer.theTextPosition.row = lexer.theTextPosition.row;

        *result = pp_expr_eval_expression(subparser);
        free(subparser);
        child = NULL;
        if(*result < 0)
        {
            return E_FAIL;
        }
        return S_OK;
    }
    else
    {
        return pp_error(this, "unknown conditional directive '%s' (this is a bug; please report it)", directive);
    }
}

/**
 * Expands a parameter of a function macro.
 * Pre: the macro is defined
 */
void pp_parser::insertParam(char* name)
{
    params->findByName(name);
    pp_parser_alloc_macro(this, params->retrieve(), NULL, PP_NORMAL_MACRO);
}

/**
 * Expands a regular (i.e. non-function) macro.
 * Pre: the macro is defined
 */
void pp_parser::insertMacro(char *name)
{
    pp_parser_alloc_macro(this, ctx->macros.get(name), NULL, PP_NORMAL_MACRO);
}

/**
 * Expands a macro.
 * Pre: the macro is defined in func_macros
 * Pre: the last token retrieved was a PP_TOKEN_LPAREN
 */
HRESULT pp_parser::insertFunctionMacro(char *name)
{
    int numParams, paramCount = 0, paramMacros = 0, parenLevel = 0, type;
    CList<char> *paramDefs; // note that params is different from self->params
    CList<char> *funcParams;
    char paramBuffer[1024] = "", *tail;

    // find macro and get number of parameters
    funcParams = ctx->func_macros.get(name);
    numParams = funcParams->size() - 1;

    paramDefs = new(NULL) CList<char>();

    // read the parameter list and temporarily define a "simple" macro for each parameter
    funcParams->gotoFirst();
    do
    {
        if(FAILED(lexTokenEssential(false)))
        {
            return E_FAIL;
        }
        type = token.theType;
        bool write = false;

        switch(type)
        {
        case PP_TOKEN_NEWLINE:
            if(paramBuffer[0] != '\0')
            {
                return pp_error(this, "unexpected newline in parameter list for function '%s'", name);
            }
        case PP_TOKEN_EOF:
            return pp_error(this, "unexpected end of file in parameter list for function '%s'", name);
        case PP_TOKEN_LPAREN:
            parenLevel++;
            write = true;
            break;
        case PP_TOKEN_RPAREN:
            parenLevel--;
            // fallthrough
        case PP_TOKEN_COMMA:
            if(type == PP_TOKEN_RPAREN || parenLevel > 0)
            {
                write = true;
                if(parenLevel >= 0)
                {
                    break;
                }
            }

            // remove trailing whitespace
            tail = strchr(paramBuffer, '\0');
            while(--tail >= paramBuffer)
            {
                if(*tail == ' ' || *tail == '\t' || *tail == '\n' || *tail == '\r')
                {
                    *tail = '\0';
                }
                else
                {
                    break;
                }
            }

            // handle the case of a function with no parameters
            if(type == PP_TOKEN_RPAREN && paramCount == 0 && paramBuffer[0] == '\0') break;

            paramCount++;
            if(paramCount > numParams)
            {
                return pp_error(this, "too many parameters to function '%s'", name);
            }

            // no need to create a macro if passed variable name is the same as the parameter name
            if(strncmp(paramBuffer, funcParams->getName(), sizeof(paramBuffer)) != 0)
            {
                // add the new macro to the beginning of the macro list
                paramMacros++;
                paramDefs->insertAfter(strdup(paramBuffer), funcParams->getName());
            }

            funcParams->gotoNext();
            paramBuffer[0] = '\0';
            break;
        case PP_TOKEN_IDENTIFIER:
            if (this->type == PP_FUNCTION_MACRO && this->params->findByName(token.theSource))
            {
                if((strlen(paramBuffer) + strlen(this->params->retrieve()) + 1) > sizeof(paramBuffer))
                {
                    return pp_error(this,
                                    "parameter %d of function '%s' exceeds max length of %d characters",
                                    name, sizeof(paramBuffer)-1);
                }
                strcat(paramBuffer, this->params->retrieve());
                write = false;
            }
            else write = true;
            break;
        case PP_TOKEN_WHITESPACE:
            if(paramBuffer[0] == '\0')
                write = false;
            else
                write = true;
            break;
        default:
            write = true;
        }
        if(write)
        {
            if((strlen(paramBuffer) + strlen(token.theSource) + 1) > sizeof(paramBuffer))
                return pp_error(this, "parameter %d of function '%s' exceeds max length of %d characters",
                                name, sizeof(paramBuffer) - 1);

            strcat(paramBuffer, token.theSource);
        }
    }
    while(parenLevel >= 0 || type != PP_TOKEN_RPAREN);

    if(paramCount < numParams)
        return pp_error(this, "not enough parameters to function '%s' (%i given, %i expected)",
                        name, paramCount, numParams);

    // do the actual parsing
    funcParams->gotoLast();
    pp_parser_alloc_macro(this, funcParams->retrieve(), paramDefs, PP_FUNCTION_MACRO);
    this->child->macroName = strdup(name);

    return S_OK;
}

/**
 * Returns true if the given name is the name of one of the built-in C99 macros.
 */
static bool pp_is_builtin_macro(const char *name)
{
    if(!strcmp(name, "__FILE__") || !strcmp(name, "__LINE__") ||
       !strcmp(name, "__DATE__") || !strcmp(name, "__TIME__"))
    {
        return true;
    }
#if PP_TEST
    else if(!strcmp(name, "__STDC__") || !strcmp(name, "__STDC_HOSTED__") ||
            !strcmp(name, "__STDC_VERSION__"))
    {
        return true;
    }
#endif
    else
    {
        return false;
    }
}

/**
 * Expands a __FILE__ or __LINE__ macro.  __FILE__ and __LINE__ are the two C99
 * builtin macros whose value does not stay constant throughout preprocessing.
 * (ISO/IEC 9899:1999 section 6.10.8, Predefined macros)
 */
void pp_parser::insertBuiltinMacro(const char *name)
{
    char value[256];
    if(!strcmp(name, "__FILE__"))
    {
        value[0] = '"';
        strncpy(value + 1, filename, 253);
        strcat(value, "\"");
    }
    else if(!strcmp(name, "__LINE__"))
    {
        pp_parser *fileroot = this;
        while(fileroot->type != PP_ROOT && fileroot->type != PP_INCLUDE)
        {
            fileroot = fileroot->parent;
        }
        sprintf(value, "%i", fileroot->token.theTextPosition.row);
    }
    else
    {
        assert(!"name parameter not __FILE__ or __LINE__");
    }

    pp_parser_alloc_macro(this, strdup(value), NULL, PP_NORMAL_MACRO);
    child->freeSourceCode = true;
}

/**
 * Returns true if the given name is the name of a currently defined macro,
 * false otherwise.
 */
bool pp_parser::isDefined(const char *name)
{
    return (ctx->macros.containsKey(name) ||
            ctx->func_macros.containsKey(name) ||
            pp_is_builtin_macro(name));
}

