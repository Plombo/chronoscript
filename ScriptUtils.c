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


