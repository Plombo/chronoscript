/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "Lexer.h"
#include "ScriptUtils.h"

static const char *keywords[] = {
    "auto",
    "break",
    "case",
    "char",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "float",
    "for",
    "goto",
    "if",
    "int",
    "long",
    "register",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while",
};

static const MY_TOKEN_TYPE keyword_tokens[] = {
    TOKEN_AUTO,
    TOKEN_BREAK,
    TOKEN_CASE,
    TOKEN_CHAR,
    TOKEN_CONST,
    TOKEN_CONTINUE,
    TOKEN_DEFAULT,
    TOKEN_DO,
    TOKEN_DOUBLE,
    TOKEN_ELSE,
    TOKEN_ENUM,
    TOKEN_EXTERN,
    TOKEN_FLOAT,
    TOKEN_FOR,
    TOKEN_GOTO,
    TOKEN_IF,
    TOKEN_INT,
    TOKEN_LONG,
    TOKEN_REGISTER,
    TOKEN_RETURN,
    TOKEN_SHORT,
    TOKEN_SIGNED,
    TOKEN_SIZEOF,
    TOKEN_STATIC,
    TOKEN_STRUCT,
    TOKEN_SWITCH,
    TOKEN_TYPEDEF,
    TOKEN_UNION,
    TOKEN_UNSIGNED,
    TOKEN_VOID,
    TOKEN_VOLATILE,
    TOKEN_WHILE,
};

// returns true if the character c is a valid hex digit
static inline bool is_hex_digit(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// precondition: the character c must be a valid hex digit
static inline int hex_digit_value(int c)
{
    if (c >= 'a')
        return c - 'a' + 10;
    else if (c >= 'A')
        return c - 'A' + 10;
    else
        return c - '0';
}

//Constructor
void Token_Init(Token *ptoken, MY_TOKEN_TYPE theType, const char *theSource, TEXTPOS theTextPosition, u32 charOffset)
{
    ptoken->theType = theType;
    ptoken->theTextPosition = theTextPosition;
    ptoken->charOffset = charOffset;
    strcpy(ptoken->theSource, theSource );
}

//Construct from a pp_token
HRESULT Token_InitFromPreprocessor(Token *ptoken, pp_token *ppToken)
{
    ptoken->theTextPosition = ppToken->theTextPosition;
    ptoken->charOffset = ppToken->charOffset;
    strncpy(ptoken->theSource, ppToken->theSource, MAX_TOKEN_LENGTH);

    switch (ppToken->theType)
    {
    case PP_TOKEN_IDENTIFIER:
    {
        int tokenType = searchList(keywords, ppToken->theSource, sizeof(keywords) / sizeof(keywords[0]));
        // searchList is case-insensitive, we want case-sensitive
        if (tokenType >= 0 && !strcmp(ppToken->theSource, keywords[tokenType]))
            ptoken->theType = keyword_tokens[tokenType];
        else
            ptoken->theType = TOKEN_IDENTIFIER;
        break;
    }
    case PP_TOKEN_HEXCONSTANT:
        ptoken->theType = TOKEN_HEXCONSTANT;
        break;
    case PP_TOKEN_INTCONSTANT:
        ptoken->theType = TOKEN_INTCONSTANT;
        break;
    case PP_TOKEN_FLOATCONSTANT:
        ptoken->theType = TOKEN_FLOATCONSTANT;
        break;
    case PP_TOKEN_STRING_LITERAL:
    {
        char *src, *dest;

        // handle escape sequences and convert to correct format
        ptoken->theType = TOKEN_STRING_LITERAL;
        src = ppToken->theSource + 1; // skip first quote mark
        dest = ptoken->theSource;

        while (*src && *src != '"')
        {
            if (*src == '\\')
            {
                switch (*(++src))
                {
                case 's':
                    *dest++ = ' ';
                    src++;
                    break;
                case 'r':
                    *dest++ = '\r';
                    src++;
                    break;
                case 'n':
                    *dest++ = '\n';
                    src++;
                    break;
                case 't':
                    *dest++ = '\t';
                    src++;
                    break;
                case '\"':
                    *dest++ = '\"';
                    src++;
                    break;
                case '\'':
                    *dest++ = '\'';
                    src++;
                    break;
                case '\\':
                    *dest++ = '\\';
                    src++;
                    break;
                // Octal escape
                // Just like in C, 3 digits max.
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                {
                    int value = 0, i;
                    for (i = 0; i < 3; i++)
                    {
                        value *= 8;
                        value += *src - '0';
                        src++;
                        if (*src < '0' || *src > '9') break;
                    }
                    if (value > 255)
                    {
                        printf("Error: Invalid octal escape: %03o > %i > 255\n", value, value);
                        ptoken->theType = TOKEN_ERROR;
                        return E_FAIL;
                    }
                    *dest++ = value;
                    break;
                }
                // Hex escape
                // Unlike C, 2 digits max. It works the way you want, not the C way which is useless.
                case 'x':
                {
                    src++;
                    if (!is_hex_digit(*src))
                    {
                        printf("Error: Invalid hex escape\n");
                        ptoken->theType = TOKEN_ERROR;
                        return E_FAIL;
                    }
                    int value = hex_digit_value(*src);
                    src++;
                    if (is_hex_digit(*src))
                    {
                        value *= 16;
                        value += hex_digit_value(*src);
                        src++;
                    }
                    *dest++ = value;
                    break;
                }
                default: // invalid escape sequence
                    // TODO: emit a warning here
                    *dest++ = '\\';
                    *dest++ = *src;
                }
            }
            else
            {
                *dest++ = *src++;
            }
        }
        *dest = '\0';
        break;
    }
    case PP_TOKEN_PTR_OP:
        ptoken->theType = TOKEN_PTR_OP;
        break;
    case PP_TOKEN_INC_OP:
        ptoken->theType = TOKEN_INC_OP;
        break;
    case PP_TOKEN_DEC_OP:
        ptoken->theType = TOKEN_DEC_OP;
        break;
    case PP_TOKEN_LEFT_OP:
        ptoken->theType = TOKEN_LEFT_OP;
        break;
    case PP_TOKEN_RIGHT_OP:
        ptoken->theType = TOKEN_RIGHT_OP;
        break;
    case PP_TOKEN_CONDITIONAL:
        ptoken->theType = TOKEN_CONDITIONAL;
        break;
    case PP_TOKEN_LE_OP:
        ptoken->theType = TOKEN_LE_OP;
        break;
    case PP_TOKEN_GE_OP:
        ptoken->theType = TOKEN_GE_OP;
        break;
    case PP_TOKEN_EQ_OP:
        ptoken->theType = TOKEN_EQ_OP;
        break;
    case PP_TOKEN_NE_OP:
        ptoken->theType = TOKEN_NE_OP;
        break;
    case PP_TOKEN_AND_OP:
        ptoken->theType = TOKEN_AND_OP;
        break;
    case PP_TOKEN_OR_OP:
        ptoken->theType = TOKEN_OR_OP;
        break;
    case PP_TOKEN_MUL_ASSIGN:
        ptoken->theType = TOKEN_MUL_ASSIGN;
        break;
    case PP_TOKEN_DIV_ASSIGN:
        ptoken->theType = TOKEN_DIV_ASSIGN;
        break;
    case PP_TOKEN_MOD_ASSIGN:
        ptoken->theType = TOKEN_MOD_ASSIGN;
        break;
    case PP_TOKEN_ADD_ASSIGN:
        ptoken->theType = TOKEN_ADD_ASSIGN;
        break;
    case PP_TOKEN_SUB_ASSIGN:
        ptoken->theType = TOKEN_SUB_ASSIGN;
        break;
    case PP_TOKEN_LEFT_ASSIGN:
        ptoken->theType = TOKEN_LEFT_ASSIGN;
        break;
    case PP_TOKEN_RIGHT_ASSIGN:
        ptoken->theType = TOKEN_RIGHT_ASSIGN;
        break;
    case PP_TOKEN_AND_ASSIGN:
        ptoken->theType = TOKEN_AND_ASSIGN;
        break;
    case PP_TOKEN_XOR_ASSIGN:
        ptoken->theType = TOKEN_XOR_ASSIGN;
        break;
    case PP_TOKEN_OR_ASSIGN:
        ptoken->theType = TOKEN_OR_ASSIGN;
        break;
    case PP_TOKEN_ELLIPSIS:
        ptoken->theType = TOKEN_ELLIPSIS;
        break;
    case PP_TOKEN_SEMICOLON:
        ptoken->theType = TOKEN_SEMICOLON;
        break;
    case PP_TOKEN_LCURLY:
        ptoken->theType = TOKEN_LCURLY;
        break;
    case PP_TOKEN_RCURLY:
        ptoken->theType = TOKEN_RCURLY;
        break;
    case PP_TOKEN_COMMA:
        ptoken->theType = TOKEN_COMMA;
        break;
    case PP_TOKEN_COLON:
        ptoken->theType = TOKEN_COLON;
        break;
    case PP_TOKEN_ASSIGN:
        ptoken->theType = TOKEN_ASSIGN;
        break;
    case PP_TOKEN_LPAREN:
        ptoken->theType = TOKEN_LPAREN;
        break;
    case PP_TOKEN_RPAREN:
        ptoken->theType = TOKEN_RPAREN;
        break;
    case PP_TOKEN_LBRACKET:
        ptoken->theType = TOKEN_LBRACKET;
        break;
    case PP_TOKEN_RBRACKET:
        ptoken->theType = TOKEN_RBRACKET;
        break;
    case PP_TOKEN_FIELD:
        ptoken->theType = TOKEN_FIELD;
        break;
    case PP_TOKEN_BITWISE_AND:
        ptoken->theType = TOKEN_BITWISE_AND;
        break;
    case PP_TOKEN_BOOLEAN_NOT:
        ptoken->theType = TOKEN_BOOLEAN_NOT;
        break;
    case PP_TOKEN_BITWISE_NOT:
        ptoken->theType = TOKEN_BITWISE_NOT;
        break;
    case PP_TOKEN_ADD:
        ptoken->theType = TOKEN_ADD;
        break;
    case PP_TOKEN_SUB:
        ptoken->theType = TOKEN_SUB;
        break;
    case PP_TOKEN_MUL:
        ptoken->theType = TOKEN_MUL;
        break;
    case PP_TOKEN_DIV:
        ptoken->theType = TOKEN_DIV;
        break;
    case PP_TOKEN_MOD:
        ptoken->theType = TOKEN_MOD;
        break;
    case PP_TOKEN_LT:
        ptoken->theType = TOKEN_LT;
        break;
    case PP_TOKEN_GT:
        ptoken->theType = TOKEN_GT;
        break;
    case PP_TOKEN_XOR:
        ptoken->theType = TOKEN_XOR;
        break;
    case PP_TOKEN_BITWISE_OR:
        ptoken->theType = TOKEN_BITWISE_OR;
        break;
    case PP_TOKEN_EOF:
        ptoken->theType = TOKEN_EOF;
        break;
    case PP_TOKEN_ERROR:
    case PP_TOKEN_DIRECTIVE:
    case PP_TOKEN_CONCATENATE:
    case PP_TOKEN_WHITESPACE:
    case PP_TOKEN_NEWLINE:
    case PP_EPSILON:
    case PP_END_OF_TOKENS:
        ptoken->theType = TOKEN_ERROR;
        return E_FAIL;
    default:
        printf("Script error: unknown token type %d\n", ppToken->theType);
        ptoken->theType = TOKEN_ERROR;
        return E_FAIL;
    }

    return S_OK;
}

void Lexer_Init(Lexer *plexer, pp_context *pcontext, const char *thePath, char *theSource, TEXTPOS theStartingPosition)
{
    plexer->thePath = thePath;
    plexer->ptheSource = theSource;
    plexer->theTokenPosition = theStartingPosition;
    pp_parser_init(&plexer->preprocessor, pcontext, thePath, theSource, theStartingPosition);
}

void Lexer_Clear(Lexer *plexer)
{
    memset(plexer, 0, sizeof(Lexer));
}

/******************************************************************************
*  getNextToken -- Thie method searches the input stream and returns the next
*  token found within that stream, using the principle of maximal munch.  It
*  embodies the start state of the FSA.
*
*  Parameters: theNextToken -- address of the next CToken found in the stream
*  Returns: S_OK
*           E_FAIL
******************************************************************************/
HRESULT Lexer_GetNextToken(Lexer *plexer, Token *theNextToken)
{
    pp_token *ppToken;

    // get the next non-whitespace, non-newline token from the preprocessor
    do
    {
        ppToken = pp_parser_emit_token(&plexer->preprocessor);
        if (ppToken == NULL)
        {
            return E_FAIL;
        }
    }
    while (ppToken->theType == PP_TOKEN_WHITESPACE || ppToken->theType == PP_TOKEN_NEWLINE);

    plexer->theTokenPosition = plexer->preprocessor.lexer.theTokenPosition;
    plexer->pcurChar = plexer->preprocessor.lexer.pcurChar;

    return Token_InitFromPreprocessor(theNextToken, ppToken);
}

