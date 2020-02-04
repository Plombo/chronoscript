// replacement for real globals.h

#ifndef FAKE_GLOBALS_H
#define FAKE_GLOBALS_H

#include <stdlib.h>
#include <string.h> // memcpy, memset, etc.
#include <assert.h> // assert
#include "depends.h"

#include <stdio.h> // printf

#define DEBUG_LEVEL 1

#if DEBUG_LEVEL > 0
#define INFO printf
#else
#define INFO(format, __VA_ARGS__)
#endif

#ifndef _WIN32
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#endif
