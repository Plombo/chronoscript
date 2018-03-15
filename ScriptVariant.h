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
void ScriptVariant_Copy(ScriptVariant *svar, const ScriptVariant *rightChild);
void ScriptVariant_ParseStringConstant(ScriptVariant *var, char *str);
HRESULT ScriptVariant_IntegerValue(const ScriptVariant *var, int32_t *pVal);
HRESULT ScriptVariant_DecimalValue(const ScriptVariant *var, double *pVal);
bool ScriptVariant_IsTrue(const ScriptVariant *svar);
int ScriptVariant_ToString(const ScriptVariant *svar, char *buffer, size_t bufsize);

ScriptVariant *ScriptVariant_Ref(ScriptVariant *var);
void ScriptVariant_Unref(ScriptVariant *var);

ScriptVariant *ScriptVariant_Or(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_And(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Bit_Or(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Xor(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Bit_And(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Eq(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Ne(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Lt(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Gt(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Ge(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Le(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Add(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_AddFolding(const ScriptVariant *svar, const ScriptVariant *rightChild); // used for constant folding when compiling a script
ScriptVariant *ScriptVariant_Sub(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Shl(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Shr(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Mul(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Div(const ScriptVariant *svar, const ScriptVariant *rightChild);
ScriptVariant *ScriptVariant_Mod(const ScriptVariant *svar, const ScriptVariant *rightChild);

// note that these are changed from OpenBOR - they now return new value instead of modifying in place
ScriptVariant *ScriptVariant_Neg(const ScriptVariant *svar);
ScriptVariant *ScriptVariant_Boolean_Not(const ScriptVariant *svar);
ScriptVariant *ScriptVariant_Bit_Not(const ScriptVariant *svar);
ScriptVariant *ScriptVariant_ToBoolean(const ScriptVariant *svar);

#ifdef __cplusplus
};
#endif

#endif
