/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2016 OpenBOR Team
 */

#ifndef IMPORTCACHE_HPP
#define IMPORTCACHE_HPP

#include "List.hpp"
#include "Interpreter.hpp"

void ImportCache_Init();
Interpreter *ImportCache_ImportFile(const char *path);
void ImportCache_Clear();
ExecFunction *ImportList_GetFunctionPointer(List<Interpreter*> *list, const char *name);

#endif

