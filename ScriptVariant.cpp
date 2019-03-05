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
#include "ScriptVariant.hpp"
#include "ObjectHeap.hpp"

void ScriptVariant_Clear(ScriptVariant *var)
{
    memset(var, 0, sizeof(*var));
}

void ScriptVariant_Init(ScriptVariant *var)
{
    memset(var, 0, sizeof(*var));
}

void ScriptVariant_Ref(const ScriptVariant *var)
{
    if (var->vt == VT_STR)
    {
        StrCache_Ref(var->strVal);
    }
    else if (var->vt == VT_OBJECT || var->vt == VT_LIST)
    {
        ObjectHeap_Ref(var->objVal);
    }
}

void ScriptVariant_Unref(const ScriptVariant *var)
{
    if (var->vt == VT_STR)
    {
        StrCache_Unref(var->strVal);
    }
    else if (var->vt == VT_OBJECT || var->vt == VT_LIST)
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
        var->strVal = index;
        StrCache_Ref(index);
    }
    else
    {
        var->strVal = StrCache_PopPersistent(strlen(str));
        var->vt = VT_STR;
        StrCache_Copy(var->strVal, str);
        StrCache_SetHash(var->strVal);
    }
}

CCResult ScriptVariant_IntegerValue(const ScriptVariant *var, int32_t *pVal)
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
        return CC_FAIL;
    }

    return CC_OK;
}

CCResult ScriptVariant_DecimalValue(const ScriptVariant *var, double *pVal)
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
        return CC_FAIL;
    }

    return CC_OK;
}


bool ScriptVariant_IsTrue(const ScriptVariant *svar)
{
    switch (svar->vt)
    {
        case VT_INTEGER:
            return svar->lVal != 0;
        case VT_DECIMAL:
            return svar->dblVal != 0.0;
        case VT_PTR:
            // builtins should return VT_EMPTY instead of VT_PTR with a null pointer, but just in case...
            return svar->ptrVal != NULL;
        case VT_STR:
        case VT_OBJECT:
        case VT_LIST:
            return true;
        default:
            return false;
    }
}

bool ScriptVariant_IsEqual(const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    double dbl1, dbl2;

    if (svar->vt == rightChild->vt)
    {
        switch (svar->vt)
        {
            case VT_INTEGER:
                return (svar->lVal == rightChild->lVal);
            case VT_DECIMAL:
                return (svar->dblVal == rightChild->dblVal);
            case VT_STR:
                return (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) == 0);
            case VT_PTR:
                return (svar->ptrVal == rightChild->ptrVal);
            case VT_OBJECT:
            case VT_LIST:
                return (svar->objVal == rightChild->objVal);
            case VT_EMPTY:
                return true;
            default:
                return false;
        }
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        return (dbl1 == dbl2);
    }

    return false;
}

// returns size of output string (or desired size, if greater than bufsize)
int ScriptVariant_ToString(const ScriptVariant *svar, char *buffer, size_t bufsize)
{
    switch (svar->vt)
    {
    case VT_EMPTY:
        return snprintf(buffer, bufsize, "NULL");
    case VT_INTEGER:
        return snprintf(buffer, bufsize, "%d", svar->lVal);
    case VT_DECIMAL:
        return snprintf(buffer, bufsize, "%lf", svar->dblVal);
    case VT_PTR:
        return snprintf(buffer, bufsize, "%p", svar->ptrVal);
    case VT_STR:
        return snprintf(buffer, bufsize, "%s", StrCache_Get(svar->strVal));
    case VT_OBJECT:
    case VT_LIST:
        return ObjectHeap_Get(svar->objVal)->toString(buffer, bufsize);
    default:
        return snprintf(buffer, bufsize, "<Unprintable VARIANT type.>");
    }
}

// returns the length of svar converted to a string
static int ScriptVariant_LengthAsString(const ScriptVariant *svar)
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

void ScriptVariant_Copy(ScriptVariant *svar, const ScriptVariant *rightChild)
{
    *svar = *rightChild;
}

CCResult ScriptVariant_Bit_Or(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar->lVal = svar->lVal | rightChild->lVal;
        retvar->vt = VT_INTEGER;
        return CC_OK;
    }
    else
    {
        printf("Invalid operands for bitwise 'or' operation (requires 2 integers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }
}

CCResult ScriptVariant_Xor(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar->lVal = svar->lVal ^ rightChild->lVal;
        retvar->vt = VT_INTEGER;
        return CC_OK;
    }
    else
    {
        printf("Invalid operands for bitwise 'xor' operation (requires 2 integers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }
}

CCResult ScriptVariant_Bit_And(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar->lVal = svar->lVal & rightChild->lVal;
        retvar->vt = VT_INTEGER;
        return CC_OK;
    }
    else
    {
        printf("Invalid operands for bitwise 'xor' operation (requires 2 integers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }
}

CCResult ScriptVariant_Eq(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    retvar->lVal = ScriptVariant_IsEqual(svar, rightChild);
    retvar->vt = VT_INTEGER;
    return CC_OK;
}


CCResult ScriptVariant_Ne(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    retvar->lVal = !ScriptVariant_IsEqual(svar, rightChild);
    retvar->vt = VT_INTEGER;
    return CC_OK;
}


CCResult ScriptVariant_Lt(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    double dbl1, dbl2;

    if (svar->vt == rightChild->vt)
    {
        switch (svar->vt)
        {
            case VT_INTEGER:
                retvar->lVal = (svar->lVal < rightChild->lVal);
                break;
            case VT_DECIMAL:
                retvar->lVal = (svar->dblVal < rightChild->dblVal);
                break;
            case VT_STR:
                retvar->lVal = (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) < 0);
                break;
            case VT_PTR:
            case VT_OBJECT:
            case VT_LIST:
            case VT_EMPTY:
            default:
                retvar->lVal = 0;
                break;
        }
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        retvar->lVal = (dbl1 < dbl2);
    }
    else
    {
        retvar->lVal = 0;
    }

    retvar->vt = VT_INTEGER;
    return CC_OK;
}



CCResult ScriptVariant_Gt(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    double dbl1, dbl2;

    if (svar->vt == rightChild->vt)
    {
        switch (svar->vt)
        {
            case VT_INTEGER:
                retvar->lVal = (svar->lVal > rightChild->lVal);
                break;
            case VT_DECIMAL:
                retvar->lVal = (svar->dblVal > rightChild->dblVal);
                break;
            case VT_STR:
                retvar->lVal = (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) > 0);
                break;
            case VT_PTR:
            case VT_OBJECT:
            case VT_LIST:
            case VT_EMPTY:
            default:
                retvar->lVal = 0;
                break;
        }
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        retvar->lVal = (dbl1 > dbl2);
    }
    else
    {
        retvar->lVal = 0;
    }

    retvar->vt = VT_INTEGER;
    return CC_OK;
}



CCResult ScriptVariant_Ge(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    double dbl1, dbl2;

    if (svar->vt == rightChild->vt)
    {
        switch (svar->vt)
        {
            case VT_INTEGER:
                retvar->lVal = (svar->lVal >= rightChild->lVal);
                break;
            case VT_DECIMAL:
                retvar->lVal = (svar->dblVal >= rightChild->dblVal);
                break;
            case VT_STR:
                retvar->lVal = (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) >= 0);
                break;
            case VT_PTR:
            case VT_OBJECT:
            case VT_LIST:
            case VT_EMPTY:
            default:
                retvar->lVal = 0;
                break;
        }
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        retvar->lVal = (dbl1 >= dbl2);
    }
    else
    {
        retvar->lVal = 0;
    }

    retvar->vt = VT_INTEGER;
    return CC_OK;
}


CCResult ScriptVariant_Le(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    double dbl1, dbl2;

    if (svar->vt == rightChild->vt)
    {
        switch (svar->vt)
        {
            case VT_INTEGER:
                retvar->lVal = (svar->lVal <= rightChild->lVal);
                break;
            case VT_DECIMAL:
                retvar->lVal = (svar->dblVal <= rightChild->dblVal);
                break;
            case VT_STR:
                retvar->lVal = (strcmp(StrCache_Get(svar->strVal), StrCache_Get(rightChild->strVal)) <= 0);
                break;
            case VT_PTR:
            case VT_OBJECT:
            case VT_LIST:
            case VT_EMPTY:
            default:
                retvar->lVal = 0;
                break;
        }
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        retvar->lVal = (dbl1 <= dbl2);
    }
    else
    {
        retvar->lVal = 0;
    }

    retvar->vt = VT_INTEGER;
    return CC_OK;
}


CCResult ScriptVariant_Shl(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar->lVal = ((uint32_t)svar->lVal) << ((uint32_t)rightChild->lVal);
        retvar->vt = VT_INTEGER;
        return CC_OK;
    }
    else
    {
        printf("Invalid operands for << operation (requires 2 integers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }
}


CCResult ScriptVariant_Shr(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar->lVal = ((uint32_t)svar->lVal) >> ((uint32_t)rightChild->lVal);
        retvar->vt = VT_INTEGER;
        return CC_OK;
    }
    else
    {
        printf("Invalid operands for << operation (requires 2 integers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }
}


inline CCResult ScriptVariant_AddGeneric(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild,
                                        int (*stringPopFunc)(int))
{
    double dbl1, dbl2;
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar->lVal = svar->lVal + rightChild->lVal;
        retvar->vt = VT_INTEGER;
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        retvar->dblVal = dbl1 + dbl2;
        retvar->vt = VT_DECIMAL;
    }
    else if (svar->vt == VT_STR || rightChild->vt == VT_STR)
    {
        int length = ScriptVariant_LengthAsString(svar) + ScriptVariant_LengthAsString(rightChild);
        int strVal = stringPopFunc(length);
        char *dst = StrCache_Get(strVal);
        int offset = ScriptVariant_ToString(svar, dst, length + 1);
        ScriptVariant_ToString(rightChild, dst + offset, length - offset + 1);
        StrCache_SetHash(strVal);

        retvar->strVal = strVal;
        retvar->vt = VT_STR;
    }
    else
    {
        printf("Invalid operands for addition (must be number + number or string + string)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }

    return CC_OK;
}


// used when running a script
CCResult ScriptVariant_Add(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    return ScriptVariant_AddGeneric(retvar, svar, rightChild, StrCache_Pop);
}


// used for constant folding when compiling a script
CCResult ScriptVariant_AddFolding(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    return ScriptVariant_AddGeneric(retvar, svar, rightChild, StrCache_PopPersistent);
}


CCResult ScriptVariant_Sub(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    if (svar->vt == rightChild->vt)
    {
        if (svar->vt == VT_INTEGER)
        {
            retvar->lVal = svar->lVal - rightChild->lVal;
            retvar->vt = VT_INTEGER;
        }
        else if (svar->vt == VT_DECIMAL)
        {
            retvar->dblVal = svar->dblVal - rightChild->dblVal;
            retvar->vt = VT_DECIMAL;
        }
        else
        {
            printf("Invalid operands for subtraction (must be 2 numbers)\n");
            ScriptVariant_Clear(retvar);
            return CC_FAIL;
        }
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        if (svar->vt == VT_DECIMAL || rightChild->vt == VT_DECIMAL)
        {
            retvar->dblVal = dbl1 - dbl2;
            retvar->vt = VT_DECIMAL;
        }
        else
        {
            retvar->lVal = (int32_t)(dbl1 - dbl2);
            retvar->vt = VT_INTEGER;
        }
    }
    else
    {
        printf("Invalid operands for subtraction (must be 2 numbers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }

    return CC_OK;
}


CCResult ScriptVariant_Mul(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        retvar->lVal = svar->lVal * rightChild->lVal;
        retvar->vt = VT_INTEGER;
    }
    else if (svar->vt == VT_DECIMAL && rightChild->vt == VT_DECIMAL)
    {
        retvar->dblVal = svar->dblVal * rightChild->dblVal;
        retvar->vt = VT_DECIMAL;
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        if (svar->vt == VT_DECIMAL || rightChild->vt == VT_DECIMAL)
        {
            retvar->dblVal = dbl1 * dbl2;
            retvar->vt = VT_DECIMAL;
        }
        else
        {
            retvar->lVal = (int32_t)(dbl1 * dbl2);
            retvar->vt = VT_INTEGER;
        }
    }
    else
    {
        printf("Invalid operands for multiplication (must be 2 numbers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }

    return CC_OK;
}


CCResult ScriptVariant_Div(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    double dbl1, dbl2;
    if (svar->vt == VT_INTEGER && rightChild->vt == VT_INTEGER)
    {
        if (rightChild->lVal == 0)
        {
            printf("Attempt to divide by 0!\n");
            ScriptVariant_Clear(retvar);
            return CC_FAIL;
        }
        else
        {
            retvar->lVal = svar->lVal / rightChild->lVal;
            retvar->vt = VT_INTEGER;
        }
    }
    else if (svar->vt == VT_DECIMAL && rightChild->vt == VT_DECIMAL)
    {
        retvar->dblVal = svar->dblVal / rightChild->dblVal;
        retvar->vt = VT_DECIMAL;
    }
    else if (ScriptVariant_DecimalValue(svar, &dbl1) == CC_OK &&
            ScriptVariant_DecimalValue(rightChild, &dbl2) == CC_OK)
    {
        if (dbl2 == 0)
        {
            printf("Attempt to divide by 0!\n");
            ScriptVariant_Clear(retvar);
            return CC_FAIL;
        }
        else
        {
            retvar->dblVal = dbl1 / dbl2;
            retvar->vt = VT_DECIMAL;
        }
    }
    else
    {
        printf("Invalid operands for division (must be 2 numbers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }

    return CC_OK;
}


CCResult ScriptVariant_Mod(ScriptVariant *retvar, const ScriptVariant *svar, const ScriptVariant *rightChild)
{
    int32_t l1, l2;
    if (ScriptVariant_IntegerValue(svar, &l1) == CC_OK &&
            ScriptVariant_IntegerValue(rightChild, &l2) == CC_OK)
    {
        retvar->lVal = l1 % l2;
        retvar->vt = VT_INTEGER;
    }
    else
    {
        printf("Invalid operands for '%%' (requires 2 numbers)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }

    return CC_OK;
}

CCResult ScriptVariant_Neg(ScriptVariant *retvar, const ScriptVariant *svar)
{
    switch (svar->vt)
    {
    case VT_DECIMAL:
        retvar->dblVal = -(svar->dblVal);
        retvar->vt = VT_DECIMAL;
        return CC_OK;
    case VT_INTEGER:
        retvar->lVal = -(svar->lVal);
        retvar->vt = VT_INTEGER;
        return CC_OK;
    default:
        printf("Invalid operand for negation operator (requires a number)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }
}


CCResult ScriptVariant_Boolean_Not(ScriptVariant *retvar, const ScriptVariant *svar)
{
    retvar->lVal = !ScriptVariant_IsTrue(svar);
    retvar->vt = VT_INTEGER;
    return CC_OK;
}

CCResult ScriptVariant_Bit_Not(ScriptVariant *retvar, const ScriptVariant *svar)
{
    if (svar->vt == VT_INTEGER)
    {
        retvar->lVal = svar->lVal;
        retvar->vt = VT_INTEGER;
        return CC_OK;
    }
    else
    {
        printf("Invalid operand for '~' operator (requires an integer)\n");
        ScriptVariant_Clear(retvar);
        return CC_FAIL;
    }
}

CCResult ScriptVariant_ToBoolean(ScriptVariant *retvar, const ScriptVariant *svar)
{
    retvar->lVal = ScriptVariant_IsTrue(svar);
    retvar->vt = VT_INTEGER;
    return CC_OK;
}

CCResult ScriptVariant_ContainerGet(ScriptVariant *dst, const ScriptVariant *container, const ScriptVariant *key)
{
    if (container->vt == VT_OBJECT)
    {
        if (ObjectHeap_GetObject(container->objVal)->get(dst, key))
        {
            return CC_OK;
        }
        else
        {
            return CC_FAIL;
        }
    }
    else if (container->vt == VT_LIST)
    {
        if (key->vt != VT_INTEGER)
        {
            printf("error: list index must be an integer\n");
            return CC_FAIL;
        }
        if (key->lVal < 0)
        {
            printf("error: list index cannot be negative\n");
            return CC_FAIL;
        }
        ScriptList *list = ObjectHeap_GetList(container->objVal);
        if (!list->get(dst, key->lVal))
        {
            printf("error: list index %i is out of bounds\n", key->lVal);
            return CC_FAIL;
        }
        return CC_OK;
    }
    else
    {
        // TODO: include the invalid container in the error message
        printf("error: cannot fetch a member from a non-container\n");
        return CC_FAIL;
    }
}

CCResult ScriptVariant_ContainerSet(const ScriptVariant *container, const ScriptVariant *key, const ScriptVariant *value)
{
    if (container->vt == VT_OBJECT)
    {
        if (ObjectHeap_SetObjectMember(container->objVal, key, value))
        {
            return CC_OK;
        }
        else
        {
            return CC_FAIL;
        }
    }
    else if (container->vt == VT_LIST)
    {
        if (key->vt != VT_INTEGER)
        {
            printf("error: list index must be an integer\n");
            return CC_FAIL;
        }
        if (key->lVal < 0)
        {
            printf("error: list index cannot be negative\n");
            return CC_FAIL;
        }
        ObjectHeap_SetListMember(container->objVal, (size_t)key->lVal, value);
        return CC_OK;
    }
    else
    {
        // TODO: include the invalid container in the error message
        printf("error: cannot set a member of a non-container\n");
        return CC_FAIL;
    }
}



