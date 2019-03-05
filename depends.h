/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef DEPENDS_H
#define DEPENDS_H

#include <stdint.h>
#include <stdbool.h>

typedef bool CCResult;

#define CC_OK   ((CCResult) true)
#define CC_FAIL ((CCResult) false)

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define MAX_STR_LEN    127
#define MAX_STR_VAR_LEN    63

#endif
