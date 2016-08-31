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

#if _POSIX_C_SOURCE
#define stricmp strcasecmp
#endif

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
            case '\\': printf("\\"); break;
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

// from source/utils.c in OpenBOR
// Optimized search in an arranged string table, return the index
int searchList(const char *list[], const char *value, int length)
{
    int i;
    int a = 0;
    int b = length / 2;
    int c = length - 1;
    int v = value[0];

    // We must convert uppercase values to lowercase,
    // since this is how every command is written in
    // our source.  Refer to an ASCII Chart
    if (v >= 0x41 && v <= 0x5A)
    {
        v += 0x20;
    }

    // Index value equals middle value,
    // Lets search starting from center.
    if (v == list[b][0])
    {
        if (stricmp(list[b], value) == 0)
        {
            return b;
        }

        // Search Down the List.
        if (v == list[b - 1][0])
        {
            for (i = b - 1 ; i >= 0; i--)
            {
                if (stricmp(list[i], value) == 0)
                {
                    return i;
                }
                if (v != list[i - 1][0])
                {
                    break;
                }
            }
        }

        // Search Up the List.
        if (v == list[b + 1][0])
        {
            for (i = b + 1; i < length; i++)
            {
                if (stricmp(list[i], value) == 0)
                {
                    return i;
                }
                if (v != list[i + 1][0])
                {
                    break;
                }
            }
        }

        // No match, return failure.
        goto searchListFailed;
    }

    // Define the starting point.
    if (v >= list[b + 1][0])
    {
        a = b + 1;
    }
    else if (v <= list[b - 1][0])
    {
        c = b - 1;
    }
    else
    {
        goto searchListFailed;
    }

    // Search Up from starting point.
    for (i = a; i <= c; i++)
    {
        if (v == list[i][0])
        {
            if (stricmp(list[i], value) == 0)
            {
                return i;
            }
            if (v != list[i + 1][0])
            {
                break;
            }
        }
    }

searchListFailed:

    // The search failed!
    // On five reasons for failure!
    // 1. Is the list in alphabetical order?
    // 2. Is the first letter lowercase in list?
    // 3. Does the value exist in the list?
    // 4. Is it a typo?
    // 5. Is it a text file error?
    return -1;
}


