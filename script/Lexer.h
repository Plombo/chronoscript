/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef LEXER_H
#define LEXER_H

#include "depends.h"
#include "pp_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

//enumerate the possible token types.  Some tokens here, such as LBRACKET, are
//never used.
typedef enum MY_TOKEN_TYPE
{
    TOKEN_IDENTIFIER, TOKEN_HEXCONSTANT, TOKEN_INTCONSTANT, TOKEN_FLOATCONSTANT,
    TOKEN_CHAR_LITERAL, TOKEN_STRING_LITERAL, TOKEN_SIZEOF, TOKEN_PTR_OP, TOKEN_INC_OP,
    TOKEN_DEC_OP, TOKEN_LEFT_OP, TOKEN_RIGHT_OP, TOKEN_CONDITIONAL,
    TOKEN_LE_OP, TOKEN_GE_OP, TOKEN_EQ_OP, TOKEN_NE_OP, TOKEN_AND_OP,
    TOKEN_OR_OP, TOKEN_MUL_ASSIGN, TOKEN_DIV_ASSIGN, TOKEN_MOD_ASSIGN,
    TOKEN_ADD_ASSIGN, TOKEN_SUB_ASSIGN, TOKEN_LEFT_ASSIGN, TOKEN_RIGHT_ASSIGN,
    TOKEN_AND_ASSIGN, TOKEN_XOR_ASSIGN, TOKEN_OR_ASSIGN,
    TOKEN_TYPEDEF, TOKEN_EXTERN, TOKEN_STATIC, TOKEN_AUTO, TOKEN_REGISTER,
    TOKEN_CHAR, TOKEN_SHORT, TOKEN_INT, TOKEN_LONG, TOKEN_SIGNED,
    TOKEN_UNSIGNED, TOKEN_FLOAT, TOKEN_DOUBLE, TOKEN_CONST, TOKEN_VOLATILE,
    TOKEN_VOID, TOKEN_STRUCT, TOKEN_UNION, TOKEN_ENUM, TOKEN_NULL,
    TOKEN_CASE, TOKEN_DEFAULT, TOKEN_IF, TOKEN_ELSE, TOKEN_SWITCH,
    TOKEN_WHILE, TOKEN_DO, TOKEN_FOR, TOKEN_GOTO, TOKEN_CONTINUE,
    TOKEN_BREAK, TOKEN_RETURN, TOKEN_SEMICOLON, TOKEN_LCURLY, TOKEN_RCURLY,
    TOKEN_COMMA, TOKEN_COLON, TOKEN_ASSIGN, TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_LBRACKET, TOKEN_RBRACKET, TOKEN_FIELD, TOKEN_BITWISE_AND,
    TOKEN_BOOLEAN_NOT, TOKEN_BITWISE_NOT, TOKEN_ADD, TOKEN_SUB, TOKEN_MUL,
    TOKEN_DIV, TOKEN_MOD, TOKEN_LT, TOKEN_GT, TOKEN_XOR, TOKEN_BITWISE_OR,
    TOKEN_ELLIPSIS, TOKEN_ERROR, TOKEN_EOF, EPSILON, END_OF_TOKENS
} MY_TOKEN_TYPE;

/******************************************************************************
*  CToken -- This class encapsulates the tokens that CLexer creates.  It serves
*  to encapsulate the information for OOD purposes.
******************************************************************************/
typedef struct Token
{
    MY_TOKEN_TYPE theType;
    char theSource[MAX_TOKEN_LENGTH + 1];
    TEXTPOS theTextPosition;
    unsigned int charOffset;
    bool fallthrough;
} Token;

/******************************************************************************
*  CLexer -- This class is created with a string of unicode characters and a
*  starting position, which it uses to create a series of CTokens based on the
*  script.  Its purpose is to break down the characters into "words" for the
*  parser.
******************************************************************************/
#ifdef __cplusplus
typedef struct Lexer
{
    pp_parser preprocessor; // FIXME make private
private:
    const char *thePath;
    const char *ptheSource;
    char *pcurChar;
    TEXTPOS theTokenPosition;
public:
    Lexer(pp_context *context, const char *thePath, char *theSource, TEXTPOS theStartingPosition);
    CCResult getNextToken(Token *theNextToken);
} Lexer;
#else
typedef struct Lexer Lexer;
#endif


//Constructor
void Token_Init(Token *ptoken, MY_TOKEN_TYPE theType, const char *theSource, TEXTPOS theTextPosition, unsigned int charOffset);

#ifdef __cplusplus
} // extern "C"
#endif

#endif



