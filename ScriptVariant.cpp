/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ScriptVariant.h"
#include "ScriptObject.h"

void ScriptVariant_Clear(ScriptVariant *var)
{
    ScriptVariant_ChangeType(var, VT_EMPTY);
    memset(var, 0, sizeof(*var));
}

void ScriptVariant_Init(ScriptVariant *var)
{
    memset(var, 0, sizeof(*var));
}

void ScriptVariant_ChangeType(ScriptVariant *var, VARTYPE cvt)
{
    var->vt = cvt;
}

// makes persistent version of variant if needed, and returns it
ScriptVariant *ScriptVariant_Ref(ScriptVariant *var)
{
    static ScriptVariant retval;

    if (var->vt == VT_STR)
    {
        retval.strVal = StrCache_Ref(var->strVal);
        retval.vt = VT_STR;
    }
    else if (var->vt == VT_OBJECT)
    {
        retval.objVal = ObjectHeap_Ref(var->objVal);
        retval.vt = VT_OBJECT;
    }
    else
    {
        retval = *var;
    }

    return &retval;
}

void ScriptVariant_Unref(ScriptVariant *var)
{
    if (var->vt == VT_STR)
    {
        StrCache_Unref(var->strVal);
    }
    else if (var->vt == VT_OBJECT)
    {
        ObjectHeap_Unref(var->objVal);
    }
}

// find an existing constant before copy
void ScriptVariant_ParseStringConstant(ScriptVariant *var, char *str)
{
    int index = StrCache_FindString(str);
    if (index >= 0)
    {
        var->vt = VT_STR;
        var->strVal = StrCache_Ref(index);
    }
    else
    {
        var->strVal = StrCache_PopPersistent(strlen(str));
        var->vt = VT_STR;
        StrCache_Copy(var->strVal, str);
    }
}

HRESULT ScriptVariant_IntegerValue(ScriptVariant *var, int32_t *pVal)
{
    if (var->vt == VT_INTEGER)
    {
        *pVal = var->lVal;
    }
    else if (var->vt == VT_DECIMAL)
    {
        *pVal = (int32_t)var->dblVal;
    }
    else
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT ScriptVariant_DecimalValue(ScriptVariant *var, double *pVal)
{
    if (var->vt == VT_INTEGER)
    {
        *pVal = (double)var->lVal;
    }
    else if (var->vt == VT_DECIMAL)
    {
        *pVal = var->dblVal;
    }
    else
    {
        return E_FAIL;
    }

    return S_OK;
}


bool ScriptVariant_IsTrue(ScriptVariant *svar)
{
    switch (svar->vt)
    {
    case VT_STR:
        return StrCache_Get(svar->strVal)[0] != 0;
    case VT_INTEGER:
        return svar->lVal != 0;
    case VT_DECIMAL:
        return svar->dblVal != 0.0;
    case VT_PTR:
        return svar->ptrVal != 0;
    default:
        return false;
    }
}

// returns size of output string (or desired size, if greater than bufsize)
int ScriptVariant_ToString(ScriptVariant *svar, char *buffer, size_t bufsize)
{
    switch (svar->vt)
    {
    case VT_EMPTY:
        return snprintf(buffer, bufsize, "<VT_EMPTY>   Unitialized");
    case VT_INTEGER:
        return snprintf(buffer, bufsize, "%d", svar->lVal);
    case VT_DECIMAL:
        return snprintf(buffer, bufsize, "%lf", svar->dblVal);
    case VT_PTR:
        return snprintf(buffer, bufsize, "%p", svar->ptrVal);
    case VT_STR:
        return snprintf(buffer, bufsize, "%s", StrCache_Get(svar->strVal));
    case VT_OBJECT:
        return ObjectHeap_Get(svar->objVal)->toString(buffer, bufsize);
    default:
        return snprintf(buffer, bufsize, "<Unprintable VARIANT type.>");
    }
}

// returns the length of svar converted to a string
static int ScriptVariant_LengthAsString(ScriptVariant *svar)
{
    if (svar->vt == VT_STR)
    {
        return StrCache_Len(svar->strVal);
    }
    else
    {
        return ScriptVariant_ToString(svar, NULL, 0);
    }
}

void ScriptVariant_Copy(ScriptVariant *svar, ScriptVariant *rightChild)
{
    *svar = *rightChild;
}

//Logical Operations

ScriptVariant *ScriptVariant_Or(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.lVal = 0}, VT_INTEGER};
    retvar.lVal = (ScriptVariant_IsTrue(svar) || ScriptVariant_IsTrue(rightChild));
    return &retvar;
}


ScriptVariant *ScriptVariant_And(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.lVal = 0}, VT_INTEGER};
    retvar.lVal = (ScriptVariant_IsTrue(svar) && ScriptVariant_IsTrue(rightChild));
    return &retvar;
}

ScriptVariant *ScriptVariant_Bit_Or(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    int32_t l1, l2;
    if (ScriptVariant_IntegerValue(svar, &l1) == S_OK &&
            ScriptVariant_IntegerValue(rightChild, &l2) == S_OK)
    {
        retvar.vt = VT_INTEGER;
        retvar.lVal = l1 | l2;
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}

ScriptVariant *ScriptVariant_Xor(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    int32_t l1, l2;
    if (ScriptVariant_IntegerValue(svar, &l1) == S_OK &&
            ScriptVariant_IntegerValue(rightChild, &l2) == S_OK)
    {
        retvar.vt = VT_INTEGER;
        retvar.lVal = l1 ^ l2;
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}

ScriptVariant *ScriptVariant_Bit_And(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    int32_t l1, l2;
    if (ScriptVariant_IntegerValue(svar, &l1) == S_OK &&
            ScriptVariant_IntegerValue(rightChild, &l2) == S_OK)
    {
        retvar.vt = VT_INTEGER;
        retvar.lVal = l1 & l2;
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}

ScriptVariant *ScriptVariant_Eq(ScriptVariant *svar, ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    static ScriptVariant retvar = {{.lVal = 0}, VT_INTEGER};

    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar.lVal = (svar->lVal == rightChild->lVal);
    }
    else if (svar->vt == VT_STR && rightChild->vt == VT_STR)
    {
        retvar.lVal = !(strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)));
    }
    else if (svar->vt == VT_PTR && rightChild->vt == VT_PTR)
    {
        retvar.lVal = (svar->ptrVal == rightChild->ptrVal);
    }
    else if (svar->vt == VT_EMPTY && rightChild->vt == VT_EMPTY)
    {
        retvar.lVal = 1;
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        retvar.lVal = (dbl1 == dbl2);
    }
    else
    {
        retvar.lVal = !(memcmp(svar, rightChild, sizeof(ScriptVariant)));
    }

    return &retvar;
}


ScriptVariant *ScriptVariant_Ne(ScriptVariant *svar, ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    static ScriptVariant retvar = {{.lVal = 0}, VT_INTEGER};

    if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        retvar.lVal = (dbl1 != dbl2);
    }
    else if (svar->vt == VT_STR && rightChild->vt == VT_STR)
    {
        retvar.lVal = strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal));
    }
    else if (svar->vt == VT_PTR && rightChild->vt == VT_PTR)
    {
        retvar.lVal = (svar->ptrVal != rightChild->ptrVal);
    }
    else if (svar->vt == VT_EMPTY && rightChild->vt == VT_EMPTY)
    {
        retvar.lVal = 0;
    }
    else
    {
        retvar.lVal = (memcmp(svar, rightChild, sizeof(ScriptVariant)) != 0);
    }

    return &retvar;
}


ScriptVariant *ScriptVariant_Lt(ScriptVariant *svar, ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    static ScriptVariant retvar = {{.lVal = 0}, VT_INTEGER};

    if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        retvar.lVal = (dbl1 < dbl2);
    }
    else if (svar->vt == VT_STR && rightChild->vt == VT_STR)
    {
        retvar.lVal = (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) < 0);
    }
    else if (svar->vt == VT_PTR && rightChild->vt == VT_PTR)
    {
        retvar.lVal = (svar->ptrVal < rightChild->ptrVal);
    }
    else if (svar->vt == VT_EMPTY || rightChild->vt == VT_EMPTY)
    {
        retvar.lVal = 0;
    }
    else
    {
        retvar.lVal = (memcmp(svar, rightChild, sizeof(ScriptVariant)) < 0);
    }

    return &retvar;
}



ScriptVariant *ScriptVariant_Gt(ScriptVariant *svar, ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    static ScriptVariant retvar = {{.lVal = 0}, VT_INTEGER};

    if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        retvar.lVal = (dbl1 > dbl2);
    }
    else if (svar->vt == VT_STR && rightChild->vt == VT_STR)
    {
        retvar.lVal = (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) > 0);
    }
    else if (svar->vt == VT_PTR && rightChild->vt == VT_PTR)
    {
        retvar.lVal = (svar->ptrVal > rightChild->ptrVal);
    }
    else if (svar->vt == VT_EMPTY || rightChild->vt == VT_EMPTY)
    {
        retvar.lVal = 0;
    }
    else
    {
        retvar.lVal = (memcmp(svar, rightChild, sizeof(ScriptVariant)) > 0);
    }

    return &retvar;
}



ScriptVariant *ScriptVariant_Ge(ScriptVariant *svar, ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    static ScriptVariant retvar = {{.lVal = 0}, VT_INTEGER};

    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar.lVal = (svar->lVal >= rightChild->lVal);
    }
    else if (svar->vt == VT_STR && rightChild->vt == VT_STR)
    {
        retvar.lVal = (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) >= 0);
    }
    else if (svar->vt == VT_PTR && rightChild->vt == VT_PTR)
    {
        retvar.lVal = (svar->ptrVal >= rightChild->ptrVal);
    }
    else if (svar->vt == VT_EMPTY || rightChild->vt == VT_EMPTY)
    {
        retvar.lVal = 0;
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        retvar.lVal = (dbl1 >= dbl2);
    }
    else
    {
        retvar.lVal = (memcmp(svar, rightChild, sizeof(ScriptVariant)) >= 0);
    }

    return &retvar;
}


ScriptVariant *ScriptVariant_Le(ScriptVariant *svar, ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    static ScriptVariant retvar = {{.lVal = 0}, VT_INTEGER};

    if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        retvar.lVal = (dbl1 <= dbl2);
    }
    else if (svar->vt == VT_STR && rightChild->vt == VT_STR)
    {
        retvar.lVal = (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) <= 0);
    }
    else if (svar->vt == VT_PTR && rightChild->vt == VT_PTR)
    {
        retvar.lVal = (svar->ptrVal <= rightChild->ptrVal);
    }
    else if (svar->vt == VT_EMPTY || rightChild->vt == VT_EMPTY)
    {
        retvar.lVal = 0;
    }
    else
    {
        retvar.lVal = (memcmp(svar, rightChild, sizeof(ScriptVariant)) <= 0);
    }

    return &retvar;
}


ScriptVariant *ScriptVariant_Shl(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    int32_t l1, l2;
    if (ScriptVariant_IntegerValue(svar, &l1) == S_OK &&
            ScriptVariant_IntegerValue(rightChild, &l2) == S_OK)
    {
        retvar.vt = VT_INTEGER;
        retvar.lVal = ((uint32_t)l1) << ((uint32_t)l2);
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}


ScriptVariant *ScriptVariant_Shr(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    int32_t l1, l2;
    if (ScriptVariant_IntegerValue(svar, &l1) == S_OK &&
            ScriptVariant_IntegerValue(rightChild, &l2) == S_OK)
    {
        retvar.vt = VT_INTEGER;
        retvar.lVal = ((uint32_t)l1) >> ((uint32_t)l2);
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}


inline ScriptVariant *ScriptVariant_AddGeneric(ScriptVariant *svar, ScriptVariant *rightChild, int (*stringPopFunc)(int))
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    double dbl1, dbl2;
    if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        if (svar->vt == VT_DECIMAL || rightChild->vt == VT_DECIMAL)
        {
            ScriptVariant_ChangeType(&retvar, VT_DECIMAL);
            retvar.dblVal = dbl1 + dbl2;
        }
        else
        {
            ScriptVariant_ChangeType(&retvar, VT_INTEGER);
            retvar.lVal = (int32_t)(dbl1 + dbl2);
        }
    }
    else if (svar->vt == VT_STR || rightChild->vt == VT_STR)
    {
        int length = ScriptVariant_LengthAsString(svar) + ScriptVariant_LengthAsString(rightChild);
        int strVal = stringPopFunc(length);
        char *dst = StrCache_Get(strVal);
        int offset = ScriptVariant_ToString(svar, dst, length + 1);
        ScriptVariant_ToString(rightChild, dst + offset, length - offset + 1);

        retvar.strVal = strVal;
        retvar.vt = VT_STR;
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}


// used when running a script
ScriptVariant *ScriptVariant_Add(ScriptVariant *svar, ScriptVariant *rightChild)
{
    return ScriptVariant_AddGeneric(svar, rightChild, StrCache_Pop);
}


// used for constant folding when compiling a script
ScriptVariant *ScriptVariant_AddFolding(ScriptVariant *svar, ScriptVariant *rightChild)
{
    return ScriptVariant_AddGeneric(svar, rightChild, StrCache_PopPersistent);
}


ScriptVariant *ScriptVariant_Sub(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    double dbl1, dbl2;
    if (svar->vt == rightChild->vt)
    {
        if (svar->vt == VT_INTEGER)
        {
            retvar.vt = VT_INTEGER;
            retvar.lVal = svar->lVal - rightChild->lVal;
        }
        else if (svar->vt == VT_DECIMAL)
        {
            retvar.vt = VT_DECIMAL;
            retvar.dblVal = svar->dblVal - rightChild->dblVal;
        }
        else
        {
            ScriptVariant_Clear(&retvar);
        }
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        if (svar->vt == VT_DECIMAL || rightChild->vt == VT_DECIMAL)
        {
            retvar.vt = VT_DECIMAL;
            retvar.dblVal = dbl1 - dbl2;
        }
        else
        {
            retvar.vt = VT_INTEGER;
            retvar.lVal = (int32_t)(dbl1 - dbl2);
        }
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}


ScriptVariant *ScriptVariant_Mul(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    double dbl1, dbl2;
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar.vt = VT_INTEGER;
        retvar.lVal = svar->lVal * rightChild->lVal;
    }
    else if (svar->vt == VT_DECIMAL && rightChild->vt == VT_DECIMAL)
    {
        retvar.vt = VT_DECIMAL;
        retvar.dblVal = svar->dblVal * rightChild->dblVal;
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        if (svar->vt == VT_DECIMAL || rightChild->vt == VT_DECIMAL)
        {
            retvar.vt = VT_DECIMAL;
            retvar.dblVal = dbl1 * dbl2;
        }
        else
        {
            retvar.vt = VT_INTEGER;
            retvar.lVal = (int32_t)(dbl1 * dbl2);
        }
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}


ScriptVariant *ScriptVariant_Div(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    double dbl1, dbl2;
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        if (rightChild->lVal == 0)
        {
            printf("Divide by 0 error!\n");
            ScriptVariant_Clear(&retvar);
        }
        else
        {
            retvar.vt = VT_INTEGER;
            retvar.lVal = svar->lVal / rightChild->lVal;
        }
    }
    else if (svar->vt == VT_DECIMAL && rightChild->vt == VT_DECIMAL)
    {
        retvar.vt = VT_DECIMAL;
        retvar.dblVal = svar->dblVal / rightChild->dblVal;
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == S_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == S_OK)
    {
        if (dbl2 == 0)
        {
            ScriptVariant_Init(&retvar);
        }
        else if (svar->vt == VT_DECIMAL || rightChild->vt == VT_DECIMAL)
        {
            retvar.vt = VT_DECIMAL;
            retvar.dblVal = dbl1 / dbl2;
        }
        else
        {
            retvar.vt = VT_INTEGER;
            retvar.lVal = (int32_t)(dbl1 / dbl2);
        }
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}


ScriptVariant *ScriptVariant_Mod(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    int32_t l1, l2;
    if (ScriptVariant_IntegerValue(svar, &l1) == S_OK &&
            ScriptVariant_IntegerValue(rightChild, &l2) == S_OK)
    {
        retvar.vt = VT_INTEGER;
        retvar.lVal = l1 % l2;
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
}

ScriptVariant *ScriptVariant_Neg(ScriptVariant *svar)
{
    static ScriptVariant retvar;
    retvar.vt = svar->vt;
    switch (svar->vt)
    {
    case VT_DECIMAL:
        retvar.dblVal = -(svar->dblVal);
        break;
    case VT_INTEGER:
        retvar.lVal = -(svar->lVal);
        break;
    default:
        retvar.vt = VT_EMPTY;
        retvar.ptrVal = NULL;
        break;
    }
    return &retvar;
}


ScriptVariant *ScriptVariant_Boolean_Not(ScriptVariant *svar)
{
    
    static ScriptVariant retvar = {{.lVal=0}, VT_INTEGER};
    retvar.lVal = !ScriptVariant_IsTrue(svar);
    return &retvar;

}

ScriptVariant *ScriptVariant_Bit_Not(ScriptVariant *svar)
{
    static ScriptVariant retvar = {{.lVal=0}, VT_INTEGER};
    int32_t l1;
    if (ScriptVariant_IntegerValue(svar, &l1) == S_OK)
    {
        retvar.lVal = ~l1;
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }
    return &retvar;
}

ScriptVariant *ScriptVariant_ToBoolean(ScriptVariant *svar)
{
    static ScriptVariant retvar = {{.lVal=0}, VT_INTEGER};
    retvar.lVal = ScriptVariant_IsTrue(svar);
    return &retvar;
}





