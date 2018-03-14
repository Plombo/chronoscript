/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef SCRIPTVARIANT_H
#define SCRIPTVARIANT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "depends.h"
#include "StrCache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum VariantType
{
    VT_EMPTY    = 0,    //not initialized
    VT_INTEGER  = 1,    //int/long
    VT_DECIMAL  = 2,    //double
    VT_PTR      = 5,    //void*
    VT_STR      = 6,    //char*
    VT_OBJECT   = 7,    //ScriptObject*
} VARTYPE;

#pragma pack(push, 4) // make this structure 12 bytes and not 16 on 64-bit architectures
typedef struct ScriptVariant
{
    union//value
    {
        int32_t       lVal;
        void         *ptrVal;
        double        dblVal;
        int           strVal;
        int           objVal;
    };
    VARTYPE vt;//variatn type
} ScriptVariant;
#pragma pack(pop)

void ScriptVariant_Clear(ScriptVariant *var);

void ScriptVariant_Init(ScriptVariant *var);
void ScriptVariant_Copy(ScriptVariant *svar, ScriptVariant *rightChild);
void ScriptVariant_ParseStringConstant(ScriptVariant *var, char *str);
HRESULT ScriptVariant_IntegerValue(ScriptVariant *var, int32_t *pVal);
HRESULT ScriptVariant_DecimalValue(ScriptVariant *var, double *pVal);
bool ScriptVariant_IsTrue(ScriptVariant *svar);
int ScriptVariant_ToString(ScriptVariant *svar, char *buffer, size_t bufsize);

ScriptVariant *ScriptVariant_Ref(ScriptVariant *var);
void ScriptVariant_Unref(ScriptVariant *var);

ScriptVariant *ScriptVariant_Or(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_And(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Bit_Or(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Xor(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Bit_And(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Eq(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Ne(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Lt(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Gt(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Ge(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Le(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Add(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_AddFolding(ScriptVariant *svar, ScriptVariant *rightChild); // used for constant folding when compiling a script
ScriptVariant *ScriptVariant_Sub(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Shl(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Shr(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Mul(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Div(ScriptVariant *svar, ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Mod(ScriptVariant *svar, ScriptVariant *rightChild);

// note that these are changed from OpenBOR - they now return new value instead of modifying in place
ScriptVariant *ScriptVariant_Neg(ScriptVariant *svar);
ScriptVariant *ScriptVariant_Boolean_Not(ScriptVariant *svar);
ScriptVariant *ScriptVariant_Bit_Not(ScriptVariant *svar);
ScriptVariant *ScriptVariant_ToBoolean(ScriptVariant *svar);

#ifdef __cplusplus
};
#endif

#endif
