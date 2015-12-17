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

#ifdef __cplusplus
extern "C" {
#endif

typedef List Stack;

void Stack_Init(Stack *stack);
void Stack_Push( Stack *stack, void *e);
void Stack_Pop(Stack *stack );
void *Stack_Top(const Stack *stack);
int Stack_IsEmpty(const Stack *stack);

#ifdef __cplusplus
};

template <typename T>
class CStack : CList<T>
{
public:
    inline void push(T *e) { Stack_Push(&this->list, e); }
    inline void pop() { Stack_Pop(&this->list); }
    inline T *top() { return static_cast<T*>(Stack_Top(&this->list)); }
};

#endif

#endif

