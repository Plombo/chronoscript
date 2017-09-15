/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2014 OpenBOR Team
 */

#ifndef STACK_H
#define STACK_H

#include "List.h"

template <typename T>
class Stack : public List<T>
{
public:
    inline void push(T e)
    {
        this->gotoFirst();
        this->insertBefore(e, NULL);
    }

    inline void pop()
    {
        this->gotoFirst();
        this->remove();
    }

    inline T top()
    {
        this->gotoFirst();
        return this->retrieve();
    }
};

#endif

