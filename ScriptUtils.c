/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2016 OpenBOR Team
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "globals.h"

void printEscapedString(const char *string)
{
    printf("\"");
    while (*string)
    {
        unsigned char c = *string++;
        switch (c)
        {
            case '\t': printf("\\t"); break;
            case '\r': printf("\\r"); break;
            case '\n': printf("\\n"); break;
            case '\f': printf("\\f"); break;
            case '\v': printf("\\v"); break;
            case '\\': printf("\\\\"); break;
            case '"':  printf("\\\""); break;
            default:
            {
                if (isprint(c))
                {
                    printf("%c", c);
                }
                else
                {
                    printf("\\x%02x", c);
                }
            }
        }
    }
    printf("\"");
}

// Creates an "escaped" version of a string with quotes added around it and characters like '\n' escaped. Always
// writes the terminating null character and never writes past the end of the destination buffer. Returns the number
// of characters that would be written if dst were large enough, so if the return value is >= dstSize, it means the
// output was truncated.
// This implementation that calls snprintf() for every character isn't the most efficient possible solution, but it
// gets the job done.
int escapeString(char *dst, int dstSize, const char *src, int srcLength)
{
#define SNPRINTF(...) { int n = snprintf(dst, dstSize, __VA_ARGS__); dst += n; length += n; dstSize -= n; if (dstSize < 0) dstSize = 0; }
    int length = 0;
    int i;

    SNPRINTF("\"");
    for (i = 0; i < srcLength; i++)
    {
        unsigned char c = src[i];
        switch (c)
        {
            case '\t': SNPRINTF("\\t"); break;
            case '\r': SNPRINTF("\\r"); break;
            case '\n': SNPRINTF("\\n"); break;
            case '\f': SNPRINTF("\\f"); break;
            case '\v': SNPRINTF("\\v"); break;
            case '\\': SNPRINTF("\\\\"); break;
            case '"':  SNPRINTF("\\\""); break;
            default:
            {
                if (isprint(c))
                {
                    SNPRINTF("%c", c);
                }
                else
                {
                    SNPRINTF("\\x%02x", c);
                }
            }
        }
    }

    SNPRINTF("\"");
    return length;
#undef SNPRINTF
}

// from source/utils.c in OpenBOR
// Optimized search in an arranged string table, return the index
int searchList(const char *list[], const char *value, int length)
{
    int low = 0, high = length - 1, mid = 0;

    while (low <= high)
    {
        mid = (low + high) / 2;
        if (stricmp(list[mid], value) < 0)
        {
            low = mid + 1;
        }
        else if (stricmp(list[mid], value) == 0)
        {
            return mid;
        }
        else if (stricmp(list[mid], value) > 0)
        {
            high = mid - 1;
        }

    }

    return -1;
}


