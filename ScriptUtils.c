#include <stdio.h>
#include <ctype.h>

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

