/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#include "ParserSet.h"
#include "FirstFollow.h"
#include <string.h>

ParserSet::ParserSet()
{
    MY_TOKEN_TYPE *ptr = NULL;
    int i = 0;

    ptr = first_sets;
    firstSet[i] = ptr;
    do
    {
        ptr++;
        if (*ptr == END_OF_TOKENS)
        {
            i++;
            firstSet[i] = ++ptr;
        }
    } while (i < NUMPRODUCTIONS);
    i = 0;
    ptr = follow_sets;
    followSet[i] = ptr;
    do
    {
        ptr++;
        if (*ptr == END_OF_TOKENS)
        {
            i++;
            followSet[i] = ++ptr;
        }
    }
    while (i < NUMPRODUCTIONS);
}


bool ParserSet::first(PRODUCTION theProduction, MY_TOKEN_TYPE theToken)
{
    MY_TOKEN_TYPE *ptr = NULL;
    ptr = firstSet[theProduction];
    while (*ptr != END_OF_TOKENS)
    {
        if (*ptr == theToken)
        {
            return true;
        }
        ptr++;
    }
    return false;
}

bool ParserSet::follow(PRODUCTION theProduction, MY_TOKEN_TYPE theToken)
{
    MY_TOKEN_TYPE *ptr = NULL;
    ptr = followSet[theProduction];
    while (*ptr != END_OF_TOKENS)
    {
        if (*ptr == theToken)
        {
            return true;
        }
        ptr++;
    }
    return false;
}

