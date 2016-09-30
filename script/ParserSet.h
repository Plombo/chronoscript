/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef PARSERSET_H
#define PARSERSET_H

#include "Lexer.h"
#include "Productions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ParserSet
{
    MY_TOKEN_TYPE *firstSet[NUMPRODUCTIONS];
    MY_TOKEN_TYPE *followSet[NUMPRODUCTIONS];
public:
    ParserSet();
    bool first(PRODUCTION theProduction, MY_TOKEN_TYPE theToken);
    bool follow(PRODUCTION theProduction, MY_TOKEN_TYPE theToken);
} ParserSet;


#ifdef __cplusplus
}; // extern "C"
#endif

#endif
