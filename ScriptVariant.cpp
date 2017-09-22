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
    // String variables should never be changed
    // unless the engine is creating a new one
    if (cvt == VT_STR)
    {
        var->strVal = StrCache_Pop();
    }
    var->vt = cvt;
}

// find an existing constant before copy
void ScriptVariant_ParseStringConstant(ScriptVariant *var, char *str)
{
    int index = StrCache_FindString(str);
    if (index >= 0)
    {
        var->vt = VT_STR;
        var->strVal = index;
        StrCache_Grab(index);
        return;
    }

    ScriptVariant_ChangeType(var, VT_STR);
    StrCache_Copy(var->strVal, str);
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

void ScriptVariant_ToString(ScriptVariant *svar, char *buffer, size_t bufsize)
{
    switch (svar->vt)
    {
    case VT_EMPTY:
        snprintf(buffer, bufsize, "<VT_EMPTY>   Unitialized");
        break;
    case VT_INTEGER:
        snprintf(buffer, bufsize, "%d", svar->lVal);
        break;
    case VT_DECIMAL:
        snprintf(buffer, bufsize, "%lf", svar->dblVal);
        break;
    case VT_PTR:
        snprintf(buffer, bufsize, "%p", svar->ptrVal);
        break;
    case VT_STR:
        snprintf(buffer, bufsize, "%s", StrCache_Get(svar->strVal));
        break;
    case VT_OBJECT:
        ObjectHeap_Get(svar->objVal)->toString(buffer, bufsize);
        break;
    default:
        snprintf(buffer, bufsize, "<Unprintable VARIANT type.>");
        break;
    }
}

// faster if it is not VT_STR
void ScriptVariant_Copy(ScriptVariant *svar, ScriptVariant *rightChild)
{
    // collect the str cache index
    if (svar->vt == VT_STR)
    {
        StrCache_Collect(svar->strVal);
    }
    switch (rightChild->vt)
    {
    case VT_INTEGER:
        svar->lVal = rightChild->lVal;
        break;
    case VT_DECIMAL:
        svar->dblVal = rightChild->dblVal;
        break;
    case VT_PTR:
        svar->ptrVal = rightChild->ptrVal;
        break;
    case VT_STR:
        svar->strVal = rightChild->strVal;
        StrCache_Grab(svar->strVal);
        break;
    default:
        //should not happen unless the variant is not intialized correctly
        //shutdown(1, "invalid variant type");
        svar->ptrVal = NULL;
        break;
    }
    svar->vt = rightChild->vt;
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


ScriptVariant *ScriptVariant_Add(ScriptVariant *svar, ScriptVariant *rightChild)
{
    static ScriptVariant retvar = {{.ptrVal = NULL}, VT_EMPTY};
    double dbl1, dbl2;
    char buf[MAX_STR_VAR_LEN + 1];
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
        ScriptVariant_ChangeType(&retvar, VT_STR);
        char *dst = StrCache_Get(retvar.strVal);
        ScriptVariant_ToString(svar, dst, StrCache_Len(retvar.strVal) + 1);
        ScriptVariant_ToString(rightChild, buf, sizeof(buf));
        strncat(dst, buf, StrCache_Len(retvar.strVal) - strlen(dst));
    }
    else
    {
        ScriptVariant_Clear(&retvar);
    }

    return &retvar;
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





