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
bool ScriptVariant_IsEqual(const ScriptVariant *svar, const ScriptVariant *rightChild);
int ScriptVariant_ToString(const ScriptVariant *svar, char *buffer, size_t bufsize);

ScriptVariant *ScriptVariant_Ref(ScriptVariant *var);
void ScriptVariant_Unref(ScriptVariant *var);

HRESULT ScriptVariant_Or(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_And(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Bit_Or(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Xor(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Bit_And(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Eq(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Ne(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Lt(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Gt(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Ge(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Le(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Add(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_AddFolding(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild); // used for constant folding when compiling a script
HRESULT ScriptVariant_Sub(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Shl(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Shr(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Mul(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Div(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);
HRESULT ScriptVariant_Mod(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild);

// note that these are changed from OpenBOR - they now return new value instead of modifying in place
HRESULT ScriptVariant_Neg(ScriptVariant *dst, const ScriptVariant *svar);
HRESULT ScriptVariant_Boolean_Not(ScriptVariant *dst, const ScriptVariant *svar);
HRESULT ScriptVariant_Bit_Not(ScriptVariant *dst, const ScriptVariant *svar);
HRESULT ScriptVariant_ToBoolean(ScriptVariant *dst, const ScriptVariant *svar);

#ifdef __cplusplus
};
#endif

#endif
