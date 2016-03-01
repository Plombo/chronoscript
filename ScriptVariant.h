/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef SCRIPTVARIANT_H
#define SCRIPTVARIANT_H

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
} VARTYPE;

typedef struct ScriptVariant
{
    union//value
    {
        s32           lVal;
        VOID         *ptrVal;
        DOUBLE        dblVal;
        int           strVal;
    };
    VARTYPE vt;//variatn type
} ScriptVariant;

void ScriptVariant_Clear(ScriptVariant *var);

void ScriptVariant_Init(ScriptVariant *var);
void ScriptVariant_Copy(ScriptVariant *svar, ScriptVariant *rightChild ); // faster in some situations
void ScriptVariant_ChangeType(ScriptVariant *var, VARTYPE cvt);
void ScriptVariant_ParseStringConstant(ScriptVariant *var, char *str);
inline HRESULT ScriptVariant_IntegerValue(ScriptVariant *var, s32 *pVal);
inline HRESULT ScriptVariant_DecimalValue(ScriptVariant *var, DOUBLE *pVal);
BOOL ScriptVariant_IsTrue(ScriptVariant *svar);
void ScriptVariant_ToString(ScriptVariant *svar, LPSTR buffer );

// light version, for compiled call, faster than above, but not safe in some situations
// This function are used by compiled scripts
inline ScriptVariant *ScriptVariant_Assign(ScriptVariant *svar, ScriptVariant *rightChild );
inline ScriptVariant *ScriptVariant_MulAssign(ScriptVariant *svar, ScriptVariant *rightChild );
inline ScriptVariant *ScriptVariant_DivAssign(ScriptVariant *svar, ScriptVariant *rightChild );
inline ScriptVariant *ScriptVariant_AddAssign(ScriptVariant *svar, ScriptVariant *rightChild );
inline ScriptVariant *ScriptVariant_SubAssign(ScriptVariant *svar, ScriptVariant *rightChild );
inline ScriptVariant *ScriptVariant_ModAssign(ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Or( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_And( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Bit_Or( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Xor( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Bit_And( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Eq( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Ne( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Lt( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Gt( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Ge( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Le( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Add( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Sub( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Shl( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Shr( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Mul( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Div( ScriptVariant *svar, ScriptVariant *rightChild );
ScriptVariant *ScriptVariant_Mod( ScriptVariant *svar, ScriptVariant *rightChild );
inline void ScriptVariant_Inc_Op(ScriptVariant *svar );
ScriptVariant *ScriptVariant_Inc_Op2(ScriptVariant *svar );
inline void ScriptVariant_Dec_Op(ScriptVariant *svar );
ScriptVariant *ScriptVariant_Dec_Op2(ScriptVariant *svar );
//inline ScriptVariant *ScriptVariant_Pos( ScriptVariant *svar);

// note that these are changed from OpenBOR - they now return new value instead of modifying in place
ScriptVariant *ScriptVariant_Neg( ScriptVariant *svar);
ScriptVariant *ScriptVariant_Boolean_Not(ScriptVariant *svar );
ScriptVariant *ScriptVariant_Bit_Not(ScriptVariant *svar );

#ifdef __cplusplus
};
#endif

#endif
