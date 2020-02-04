/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

/**
 * This is derived from Lexer.c in scriptlib, but is modified to be used by the
 * script preprocessor.  Although the two script lexers share much of their codebase,
 * there are some significant differences - for example, the preprocessor lexer
 * can detect preprocessor tokens (#include, #define) and doesn't eat whitespace.
 *
 * @author Plombo (original Lexer.c by utunnels)
 * @date 15 October 2010
 */

#include "pp_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"

#ifdef PP_TEST
#undef printf
#endif

//MACROS
/******************************************************************************
*  CONSUMECHARACTER -- This macro inserts code to remove a character from the
*  input stream and add it to the current token buffer.
******************************************************************************/
#define CONSUMECHARACTER \
   plexer->theTokenSource[plexer->theTokenLen++] = *(plexer->pcurChar);\
   plexer->theTokenSource[plexer->theTokenLen] = '\0';\
   plexer->pcurChar++; \
   plexer->theTextPosition.col++; \
   plexer->offset++;

/******************************************************************************
*  MAKETOKEN(x) -- This macro inserts code to create a new CToken object of
*  type x, using the current token position, and source.
******************************************************************************/
#define MAKETOKEN(x) \
   pp_token_Init(theNextToken, x, plexer->theTokenSource, plexer->theTokenPosition, \
   plexer->tokOffset, plexer->fallthrough); \
   if (theNextToken->fallthrough) { plexer->fallthrough = false; }

/******************************************************************************
*  SKIPCHARACTER -- Skip a char without adding it in plexer->theTokenSource
******************************************************************************/
#define SKIPCHARACTER \
   plexer->pcurChar++; \
   plexer->theTextPosition.col++; \
   plexer->offset++;


//Constructor
void pp_token_Init(pp_token *ptoken, PP_TOKEN_TYPE theType, const char *theSource, TEXTPOS theTextPosition, unsigned int charOffset, bool fallthrough)
{
    ptoken->theType = theType;
    ptoken->theTextPosition = theTextPosition;
    ptoken->charOffset = charOffset;
    snprintf(ptoken->theSource, sizeof(ptoken->theSource), "%s", theSource);
    ptoken->fallthrough = (theType == PP_TOKEN_WHITESPACE || theType == PP_TOKEN_NEWLINE) ? false : fallthrough;
}


void pp_lexer_Init(pp_lexer *plexer, const char *theSource, TEXTPOS theStartingPosition)
{
    plexer->ptheSource = theSource;
    plexer->theTextPosition = theStartingPosition;
    plexer->pcurChar = (char *)plexer->ptheSource;
    plexer->offset = 0;
    plexer->tokOffset = 0;
    plexer->fallthrough = false;
}

void pp_lexer_Clear(pp_lexer *plexer)
{
    memset(plexer, 0, sizeof(pp_lexer));
}

/******************************************************************************
*  getNextToken -- Thie method searches the input stream and returns the next
*  token found within that stream, using the principle of maximal munch.  It
*  embodies the start state of the FSA.
*
*  Parameters: theNextToken -- address of the next CToken found in the stream
*  Returns: CC_OK
*           CC_FAIL
******************************************************************************/
CCResult pp_lexer_GetNextToken (pp_lexer *plexer, pp_token *theNextToken)
{
    theNextToken->fallthrough = false;
    for(;;)
    {
        plexer->theTokenSource[0] = plexer->theTokenLen = 0;
        plexer->theTokenPosition = plexer->theTextPosition;
        plexer->tokOffset = plexer->offset;

        //Whenever we get a new token, we need to watch out for the end-of-input.
        //Otherwise, we could walk right off the end of the stream.
        if ( !strncmp( plexer->pcurChar, "\0", 1))    //A null character marks the end of the stream
        {
            MAKETOKEN( PP_TOKEN_EOF );
            return CC_OK;
        }

        //Windows line break (\r\n)
        else if ( !strncmp( plexer->pcurChar, "\r\n", 2))
        {
            //interpret as a newline
            plexer->theTokenSource[plexer->theTokenLen++] = '\n';
            plexer->theTokenSource[plexer->theTokenLen] = '\0';
            plexer->theTextPosition.col = 0;
            plexer->theTextPosition.row++;
            plexer->pcurChar += 2;
            plexer->offset += 2;
            MAKETOKEN( PP_TOKEN_NEWLINE );
            return CC_OK;
        }

        //newline (\n), carriage return (\r), or form feed (\f)
        else if ( !strncmp( plexer->pcurChar, "\n", 1) || !strncmp( plexer->pcurChar, "\r", 1) ||
                  !strncmp( plexer->pcurChar, "\f", 1))
        {
            //interpret as a newline
            plexer->theTokenSource[plexer->theTokenLen++] = '\n';
            plexer->theTokenSource[plexer->theTokenLen] = '\0';
            plexer->theTextPosition.col = 0;
            plexer->theTextPosition.row++;
            plexer->pcurChar++;
            plexer->offset++;
            MAKETOKEN( PP_TOKEN_NEWLINE );
            return CC_OK;
        }

        // one or more whitespace characters
        else if (!strncmp(plexer->pcurChar, "\\\n", 2) || !strncmp(plexer->pcurChar, "\\\r", 2) ||
                 !strncmp(plexer->pcurChar, "\\\f", 2) || *plexer->pcurChar == '\t' ||
                 *plexer->pcurChar == ' ' || *plexer->pcurChar == '\xa0')
        {
            return pp_lexer_GetTokenWhitespace(plexer, theNextToken);
        }

        //an Identifier starts with an alphabetical character or underscore
        else if ( *plexer->pcurChar == '_' || (*plexer->pcurChar >= 'a' && *plexer->pcurChar <= 'z') ||
                  (*plexer->pcurChar >= 'A' && *plexer->pcurChar <= 'Z'))
        {
            return pp_lexer_GetTokenIdentifier(plexer, theNextToken );
        }

        //a Number starts with a numerical character
        else if ((*plexer->pcurChar >= '0' && *plexer->pcurChar <= '9') )
        {
            return pp_lexer_GetTokenNumber(plexer, theNextToken );
        }
        //a decimal point followed by one or more digits is also a number
        else if (*plexer->pcurChar == '.' &&
                 (plexer->pcurChar[1] >= '0' && plexer->pcurChar[1] <= '9'))
        {
            return pp_lexer_GetTokenNumber(plexer, theNextToken );
        }
        //string
        else if (!strncmp( plexer->pcurChar, "\"", 1))
        {
            return pp_lexer_GetTokenStringLiteral(plexer, theNextToken );
        }
        //character
        else if (!strncmp( plexer->pcurChar, "'", 1))
        {
            CONSUMECHARACTER;
            //escape characters
            if (!strncmp( plexer->pcurChar, "\\", 1))
            {
                CONSUMECHARACTER;
                CONSUMECHARACTER;
            }
            //must not be an empty character
            else if(strncmp( plexer->pcurChar, "'", 1))
            {
                CONSUMECHARACTER;
            }
            else
            {
                CONSUMECHARACTER;
                CONSUMECHARACTER;
                MAKETOKEN( PP_TOKEN_ERROR );
                return CC_OK;
            }
            if (!strncmp( plexer->pcurChar, "'", 1))
            {
                CONSUMECHARACTER;

                MAKETOKEN( PP_TOKEN_CHAR_LITERAL );
                return CC_OK;
            }
            else
            {
                CONSUMECHARACTER;
                CONSUMECHARACTER;
                MAKETOKEN( PP_TOKEN_ERROR );
                return CC_OK;
            }
        }

        //Before checking for comments
        else if ( !strncmp( plexer->pcurChar, "/", 1))
        {
            CONSUMECHARACTER;
            if ( !strncmp( plexer->pcurChar, "/", 1))
            {
                pp_lexer_SkipComment(plexer, COMMENT_SLASH);
            }
            else if ( !strncmp( plexer->pcurChar, "*", 1))
            {
                pp_lexer_SkipComment(plexer, COMMENT_STAR);
            }

            //Now complete the symbol scan for regular symbols.
            else if ( !strncmp( plexer->pcurChar, "=", 1))
            {
                CONSUMECHARACTER;
                MAKETOKEN( PP_TOKEN_DIV_ASSIGN );
                return CC_OK;
            }
            else
            {
                MAKETOKEN( PP_TOKEN_DIV );
                return CC_OK;
            }
        }

        //Concatenation operator (inside of #defines)
        else if ( !strncmp( plexer->pcurChar, "##", 2))
        {
            CONSUMECHARACTER;
            CONSUMECHARACTER;
            MAKETOKEN( PP_TOKEN_CONCATENATE );
            return CC_OK;
        }

        //Preprocessor directive
        else if ( !strncmp( plexer->pcurChar, "#", 1))
        {
            CONSUMECHARACTER;
            MAKETOKEN( PP_TOKEN_DIRECTIVE );
            return CC_OK;
        }

        //a Symbol starts with one of these characters
        else if (( !strncmp( plexer->pcurChar, ">", 1)) || ( !strncmp( plexer->pcurChar, "<", 1))
                 || ( !strncmp( plexer->pcurChar, "+", 1)) || ( !strncmp( plexer->pcurChar, "-", 1))
                 || ( !strncmp( plexer->pcurChar, "*", 1)) || ( !strncmp( plexer->pcurChar, "/", 1))
                 || ( !strncmp( plexer->pcurChar, "%", 1)) || ( !strncmp( plexer->pcurChar, "&", 1))
                 || ( !strncmp( plexer->pcurChar, "^", 1)) || ( !strncmp( plexer->pcurChar, "|", 1))
                 || ( !strncmp( plexer->pcurChar, "=", 1)) || ( !strncmp( plexer->pcurChar, "!", 1))
                 || ( !strncmp( plexer->pcurChar, ";", 1)) || ( !strncmp( plexer->pcurChar, "{", 1))
                 || ( !strncmp( plexer->pcurChar, "}", 1)) || ( !strncmp( plexer->pcurChar, ",", 1))
                 || ( !strncmp( plexer->pcurChar, ":", 1)) || ( !strncmp( plexer->pcurChar, "(", 1))
                 || ( !strncmp( plexer->pcurChar, ")", 1)) || ( !strncmp( plexer->pcurChar, "[", 1))
                 || ( !strncmp( plexer->pcurChar, "]", 1)) || ( !strncmp( plexer->pcurChar, ".", 1))
                 || ( !strncmp( plexer->pcurChar, "~", 1)) || ( !strncmp( plexer->pcurChar, "?", 1)))
        {
            return pp_lexer_GetTokenSymbol(plexer, theNextToken );
        }

        //If we get here, we've hit a character we don't recognize
        else
        {
            //Consume the character
            CONSUMECHARACTER;

            /* Create an "error" token, but continue normally since unrecognized
             * characters are none of the preprocessor's business.  Scriptlib can
             * deal with them if necessary. */
            MAKETOKEN( PP_TOKEN_ERROR );
            //HandleCompileError( *theNextToken, UNRECOGNIZED_CHARACTER );
            return CC_OK;
        }
    }
}

CCResult pp_lexer_GetTokenWhitespace(pp_lexer *plexer, pp_token *theNextToken)
{
    while (1)
    {
        if (plexer->theTokenLen + 5 >= MAX_TOKEN_LENGTH)
        {
            break;
        }

        //Backslash-escaped Windows line break (\r\n)
        else if (!strncmp(plexer->pcurChar, "\\\r\n", 3))
        {
            //interpret as a newline, but not as a PP_TOKEN_NEWLINE
            plexer->theTokenSource[plexer->theTokenLen++] = '\n';
            plexer->theTokenSource[plexer->theTokenLen] = '\0';
            plexer->theTextPosition.col = 0;
            plexer->theTextPosition.row++;
            plexer->pcurChar += 3;
            plexer->offset += 3;
        }

        //Backslash-escaped newline (\n), carriage return (\r), or form feed (\f)
        else if (!strncmp(plexer->pcurChar, "\\\n", 2) || !strncmp(plexer->pcurChar, "\\\r", 2) ||
                 !strncmp(plexer->pcurChar, "\\\f", 2))
        {
            //interpret as a newline, but not as a PP_TOKEN_NEWLINE
            plexer->theTokenSource[plexer->theTokenLen++] = '\n';
            plexer->theTokenSource[plexer->theTokenLen] = '\0';
            plexer->theTextPosition.col = 0;
            plexer->theTextPosition.row++;
            plexer->pcurChar += 2;
            plexer->offset += 2;
        }

        //tab
        else if (*plexer->pcurChar == '\t')
        {
            //increment the offset counter by TABSIZE
            int numSpaces = TABSIZE - (plexer->theTextPosition.col % TABSIZE);
            snprintf(plexer->theTokenSource, sizeof(plexer->theTokenSource), "%s", "    ");
            plexer->theTokenLen += numSpaces;
            plexer->theTokenSource[plexer->theTokenLen] = '\0';
            plexer->theTextPosition.col += numSpaces;
            plexer->pcurChar++;
            plexer->offset++;
        }

        //space
        else if (*plexer->pcurChar == ' ')
        {
            //increment the offset counter
            CONSUMECHARACTER;
        }

        //non-breaking space (A0 in Windows-1252 and ISO-8859-* encodings)
        else if (*plexer->pcurChar == '\xa0')
        {
            //increment the offset counter and replace with a normal space
            plexer->theTokenSource[plexer->theTokenLen++] = ' ';
            plexer->theTokenSource[plexer->theTokenLen] = '\0';
            plexer->pcurChar++;
            plexer->offset++;
            plexer->theTextPosition.col++;
        }

        else
        {
            break;
        }
    }

    MAKETOKEN(PP_TOKEN_WHITESPACE);
    return CC_OK;
}

/******************************************************************************
*  Identifier -- This method extracts an identifier from the stream, once it's
*  recognized as an identifier.  After it is extracted, this method determines
*  if the identifier is a keyword.
*  Parameters: theNextToken -- address of the next CToken found in the stream
*  Returns: CC_OK
*           CC_FAIL
******************************************************************************/
CCResult pp_lexer_GetTokenIdentifier(pp_lexer *plexer, pp_token *theNextToken)
{
    int len = 0;

    //copy the source that makes up this token
    //an identifier is a string of letters, digits and/or underscores
    do
    {
        CONSUMECHARACTER;
        if(++len > MAX_TOKEN_LENGTH)
        {
            printf("Warning: unable to lex token longer than %d characters\n", MAX_TOKEN_LENGTH);
            MAKETOKEN( PP_TOKEN_IDENTIFIER );
            return CC_FAIL;
        }
    }
    while ((*plexer->pcurChar >= '0' && *plexer->pcurChar <= '9')  ||
            (*plexer->pcurChar >= 'a' && *plexer->pcurChar <= 'z')  ||
            (*plexer->pcurChar >= 'A' && *plexer->pcurChar <= 'Z') ||
            ( !strncmp( plexer->pcurChar, "_", 1)));

    MAKETOKEN(PP_TOKEN_IDENTIFIER);

    return CC_OK;
}

/******************************************************************************
*  Number -- This method extracts a numerical constant from the stream.  It
*  only extracts the digits that make up the number.  No conversion from string
*  to numeral is performed here.
*  Parameters: theNextToken -- address of the next CToken found in the stream
*  Returns: CC_OK
*           CC_FAIL
******************************************************************************/
CCResult pp_lexer_GetTokenNumber(pp_lexer *plexer, pp_token *theNextToken)
{
    //copy the source that makes up this token
    //a constant is one of these:

    //0[xX][a-fA-F0-9]+{u|U|l|L}
    //0{D}+{u|U|l|L}
    if (( !strncmp( plexer->pcurChar, "0X", 2)) || ( !strncmp( plexer->pcurChar, "0x", 2)))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        while ((*plexer->pcurChar >= '0' && *plexer->pcurChar <= '9') ||
                (*plexer->pcurChar >= 'a' && *plexer->pcurChar <= 'f') ||
                (*plexer->pcurChar >= 'A' && *plexer->pcurChar <= 'F'))
        {
            CONSUMECHARACTER;
        }

        if (( !strncmp( plexer->pcurChar, "u", 1)) || ( !strncmp( plexer->pcurChar, "U", 1)) ||
                ( !strncmp( plexer->pcurChar, "l", 1)) || ( !strncmp( plexer->pcurChar, "L", 1)))
        {
            CONSUMECHARACTER;
        }

        MAKETOKEN( PP_TOKEN_HEXCONSTANT );
    }
    else
    {
        while (*plexer->pcurChar >= '0' && *plexer->pcurChar <= '9')
        {
            CONSUMECHARACTER;
        }

        if (( !strncmp( plexer->pcurChar, "E", 1)) || ( !strncmp( plexer->pcurChar, "e", 1)))
        {
            CONSUMECHARACTER;

            if (*plexer->pcurChar == '-' || *plexer->pcurChar == '+')
            {
                CONSUMECHARACTER;
            }

            while (*plexer->pcurChar >= '0' && *plexer->pcurChar <= '9')
            {
                CONSUMECHARACTER;
            }

            if (( !strncmp( plexer->pcurChar, "f", 1)) || ( !strncmp( plexer->pcurChar, "F", 1)) ||
                    ( !strncmp( plexer->pcurChar, "l", 1)) || ( !strncmp( plexer->pcurChar, "L", 1)))
            {
                CONSUMECHARACTER;
            }

            MAKETOKEN( PP_TOKEN_FLOATCONSTANT );
        }
        else if ( !strncmp( plexer->pcurChar, ".", 1))
        {
            CONSUMECHARACTER;
            while (*plexer->pcurChar >= '0' && *plexer->pcurChar <= '9')
            {
                CONSUMECHARACTER;
            }

            if (( !strncmp( plexer->pcurChar, "E", 1)) || ( !strncmp( plexer->pcurChar, "e", 1)))
            {
                CONSUMECHARACTER;

                if (*plexer->pcurChar == '-' || *plexer->pcurChar == '+')
                {
                    CONSUMECHARACTER;
                }

                while (*plexer->pcurChar >= '0' && *plexer->pcurChar <= '9')
                {
                    CONSUMECHARACTER;
                }

                if (( !strncmp( plexer->pcurChar, "f", 1)) ||
                        ( !strncmp( plexer->pcurChar, "F", 1)) ||
                        ( !strncmp( plexer->pcurChar, "l", 1)) ||
                        ( !strncmp( plexer->pcurChar, "L", 1)))
                {
                    CONSUMECHARACTER;
                }
            }
            MAKETOKEN( PP_TOKEN_FLOATCONSTANT );

        }
        else
        {
            if (( !strncmp( plexer->pcurChar, "u", 1)) || ( !strncmp( plexer->pcurChar, "U", 1)) ||
                    ( !strncmp( plexer->pcurChar, "l", 1)) || ( !strncmp( plexer->pcurChar, "L", 1)))
            {
                CONSUMECHARACTER;
            }
            MAKETOKEN( PP_TOKEN_INTCONSTANT );
        }
    }
    return CC_OK;
}

/******************************************************************************
*  StringLiteral -- This method extracts a string literal from the character
*  stream.
*  Parameters: theNextToken -- address of the next CToken found in the stream
*  Returns: CC_OK
*           CC_FAIL
******************************************************************************/
CCResult pp_lexer_GetTokenStringLiteral(pp_lexer *plexer, pp_token *theNextToken)
{
    //copy the source that makes up this token
    //an identifier is a string of letters, digits and/or underscores
    //consume that first quote mark
    int esc = 0;
    CONSUMECHARACTER;
    while ( strncmp( plexer->pcurChar, "\"", 1))
    {
        if(!strncmp( plexer->pcurChar, "\\", 1))
        {
            esc = 1;
        }
        CONSUMECHARACTER;
        if(esc)
        {
            CONSUMECHARACTER;
            esc = 0;
        }
    }

    //consume that last quote mark
    CONSUMECHARACTER;

    MAKETOKEN( PP_TOKEN_STRING_LITERAL );
    return CC_OK;
}
/******************************************************************************
*  Symbol -- This method extracts a symbol from the character stream.  For the
*  purposes of lexing, comments are considered symbols.
*  Parameters: theNextToken -- address of the next CToken found in the stream
*  Returns: CC_OK
*           CC_FAIL
******************************************************************************/
CCResult pp_lexer_GetTokenSymbol(pp_lexer *plexer, pp_token *theNextToken)
{
    //">>="
    if ( !strncmp( plexer->pcurChar, ">>=", 3))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_RIGHT_ASSIGN );
    }

    //">>"
    else if ( !strncmp( plexer->pcurChar, ">>", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_RIGHT_OP );
    }

    //">="
    else if ( !strncmp( plexer->pcurChar, ">=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_GE_OP );
    }

    //">"
    else if ( !strncmp( plexer->pcurChar, ">", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_GT );
    }

    //"<<="
    else if ( !strncmp( plexer->pcurChar, "<<=", 3))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_LEFT_ASSIGN );
    }

    //"<<"
    else if ( !strncmp( plexer->pcurChar, "<<", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_LEFT_OP );
    }

    //"<="
    else if ( !strncmp( plexer->pcurChar, "<=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_LE_OP );
    }

    //"<"
    else if ( !strncmp( plexer->pcurChar, "<", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_LT );
    }

    //"++"
    else if ( !strncmp( plexer->pcurChar, "++", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_INC_OP );
    }

    //"+="
    else if ( !strncmp( plexer->pcurChar, "+=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_ADD_ASSIGN );
    }

    //"+"
    else if ( !strncmp( plexer->pcurChar, "+", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_ADD );
    }

    //"--"
    else if ( !strncmp( plexer->pcurChar, "--", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_DEC_OP );
    }

    //"-="
    else if ( !strncmp( plexer->pcurChar, "-=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_SUB_ASSIGN );
    }

    //"-"
    else if ( !strncmp( plexer->pcurChar, "-", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_SUB );
    }

    //"*="
    else if ( !strncmp( plexer->pcurChar, "*=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_MUL_ASSIGN );
    }

    //"*"
    else if ( !strncmp( plexer->pcurChar, "*", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_MUL );
    }

    //"%="
    else if ( !strncmp( plexer->pcurChar, "%=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_MOD_ASSIGN );
    }

    //"%"
    else if ( !strncmp( plexer->pcurChar, "%", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_MOD );
    }

    //"&&"
    else if ( !strncmp( plexer->pcurChar, "&&", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_AND_OP );
    }

    //"&="
    else if ( !strncmp( plexer->pcurChar, "&=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_AND_ASSIGN );
    }

    //"&"
    else if ( !strncmp( plexer->pcurChar, "&", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_BITWISE_AND );
    }

    //"^="
    else if ( !strncmp( plexer->pcurChar, "^=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_XOR_ASSIGN );
    }

    //"^"
    else if ( !strncmp( plexer->pcurChar, "^", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_XOR );
    }

    //"||"
    else if ( !strncmp( plexer->pcurChar, "||", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_OR_OP );
    }

    //"|="
    else if ( !strncmp( plexer->pcurChar, "|=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_OR_ASSIGN );
    }

    //"|"
    else if ( !strncmp( plexer->pcurChar, "|", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_BITWISE_OR );
    }

    //"=="
    else if ( !strncmp( plexer->pcurChar, "==", 2))

    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_EQ_OP );
    }

    //"="
    else if ( !strncmp( plexer->pcurChar, "=", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_ASSIGN );
    }

    //"!="
    else if ( !strncmp( plexer->pcurChar, "!=", 2))
    {
        CONSUMECHARACTER;
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_NE_OP );
    }

    //"!"
    else if ( !strncmp( plexer->pcurChar, "!", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_BOOLEAN_NOT );
    }

    //";"
    else if ( !strncmp( plexer->pcurChar, ";", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_SEMICOLON);
    }

    //"{"
    else if ( !strncmp( plexer->pcurChar, "{", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_LCURLY);
    }

    //"}"
    else if ( !strncmp( plexer->pcurChar, "}", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_RCURLY);
    }

    //","
    else if ( !strncmp( plexer->pcurChar, ",", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_COMMA);
    }

    //":"
    else if ( !strncmp( plexer->pcurChar, ":", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_COLON);
    }

    //"("
    else if ( !strncmp( plexer->pcurChar, "(", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_LPAREN);
    }

    //")"
    else if ( !strncmp( plexer->pcurChar, ")", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_RPAREN);
    }

    //"["
    else if ( !strncmp( plexer->pcurChar, "[", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_LBRACKET);
    }

    //"]"
    else if ( !strncmp( plexer->pcurChar, "]", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_RBRACKET);
    }

    //"."
    else if ( !strncmp( plexer->pcurChar, ".", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_FIELD);
    }

    //"~"
    else if ( !strncmp( plexer->pcurChar, "~", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_BITWISE_NOT);
    }

    //"?"
    else if ( !strncmp( plexer->pcurChar, "?", 1))
    {
        CONSUMECHARACTER;
        MAKETOKEN( PP_TOKEN_CONDITIONAL);
    }

    return CC_OK;
}

/******************************************************************************
*  Comment -- This method extracts a symbol from the character stream.
*  Parameters: theNextToken -- address of the next CToken found in the stream
*  Returns: CC_OK
*           CC_FAIL
******************************************************************************/
CCResult pp_lexer_SkipComment(pp_lexer *plexer, COMMENT_TYPE theType)
{
    bool fall = false;
    if (theType == COMMENT_SLASH)
    {
        do
        {
            SKIPCHARACTER;

            //keep going if we hit a backslash-escaped line break
            if (!strncmp( plexer->pcurChar, "\\\r\n", 3))
            {
                SKIPCHARACTER;
                SKIPCHARACTER;
                plexer->theTextPosition.col = 0;
                plexer->theTextPosition.row++;
                continue;
            }
            if (!strncmp( plexer->pcurChar, "\\\n", 2) ||
                    !strncmp( plexer->pcurChar, "\\\r", 2) ||
                    !strncmp( plexer->pcurChar, "\\\f", 2))
            {
                SKIPCHARACTER;
                plexer->theTextPosition.col = 0;
                plexer->theTextPosition.row++;
                continue;
            }

            // check for "fall through" comment
            if (!fall && !strnicmp( plexer->pcurChar, "fall", 4))
            {
                fall = true;
            }
            else if (fall)
            {
                if (!strnicmp( plexer->pcurChar, "through", 7) ||
                    !strnicmp( plexer->pcurChar, "thru", 4))
                {
                    plexer->fallthrough = true;
                }
            }

            //break out if we hit a new line
            if (!strncmp( plexer->pcurChar, "\r\n", 2) ||
                    !strncmp( plexer->pcurChar, "\n", 1) ||
                    !strncmp( plexer->pcurChar, "\r", 1) ||
                    !strncmp( plexer->pcurChar, "\f", 1))
            {
                break;
            }
        }
        while (strncmp( plexer->pcurChar, "\0", 1));
    }
    else if (theType == COMMENT_STAR)
    {
        //consume the '*' that gets this comment started
        SKIPCHARACTER;

        //loop through the characters till we hit '*/'
        while (strncmp( plexer->pcurChar, "\0", 1))
        {
            if (0 == strncmp( plexer->pcurChar, "*/", 2))
            {
                SKIPCHARACTER;
                SKIPCHARACTER;
                break;
            }
            else if (!strncmp( plexer->pcurChar, "\r\n", 2))
            {
                SKIPCHARACTER;
                plexer->theTextPosition.col = 0;
                plexer->theTextPosition.row++;
            }
            else if (!strncmp( plexer->pcurChar, "\n", 1))
            {
                plexer->theTextPosition.col = 0;
                plexer->theTextPosition.row++;
            }
            else if (!strncmp( plexer->pcurChar, "\r", 1))
            {
                plexer->theTextPosition.col = 0;
                plexer->theTextPosition.row++;
            }
            else if (!strncmp( plexer->pcurChar, "\f", 1))
            {
                plexer->theTextPosition.col = 0;
                plexer->theTextPosition.row++;
            }

            // check for "fall through" comment
            if (!fall && !strnicmp( plexer->pcurChar, "fall", 4))
            {
                fall = true;
            }
            else if (fall)
            {
                if (!strnicmp( plexer->pcurChar, "through", 7) ||
                    !strnicmp( plexer->pcurChar, "thru", 4))
                {
                    plexer->fallthrough = true;
                }
            }

            SKIPCHARACTER;
        };
    }

    return CC_OK;
}

