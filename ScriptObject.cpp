/*
 * The hash table implementation in this file is heavily inspired by
 * the implementation of tables in Lua. It's close enough that the Lua
 * copyright probably applies to it:
 *
 * Copyright (C) 1994-2016 Lua.org, PUC-Rio.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include "globals.h"
#include "List.hpp"
#include "ScriptVariant.hpp"
#include "StrCache.hpp"
#include "ScriptObject.hpp"
#include "ObjectHeap.hpp"

// this has to be defined somewhere...since there's no ScriptContainer.cpp, let's put it here
ScriptContainer::~ScriptContainer()
{
}

ScriptObject::ScriptObject()
{
    persistent = false;
    currentlyPrinting = false;
    log2_hashTableSize = 0;
    hashTable = new ObjectHashNode[1];
    lastFreeNode = 1;
}

// destructor: unrefs all keys and values in hash table
ScriptObject::~ScriptObject()
{
    // unref hash table keys and (if persistent) values
    for (size_t i = 0; i < (1u << log2_hashTableSize); i++)
    {
        if (hashTable[i].key != -1)
        {
            StrCache_Unref(hashTable[i].key);
            if (persistent)
            {
                ScriptVariant_Unref(&hashTable[i].value);
            }
        }
    }

    delete[] hashTable;
}

inline bool keysEqual(int key1, int key2, const StrCacheEntry *entry1)
{
    // Most keys are string constants, so (key1 == key2) will catch most true cases.
    // Almost all non-equal field names will have different hashes, and comparing hashes is faster than strcmp.
    // If neither of those has told us whether the keys are equal, finally call strcmp to find out for sure.
    if (key1 == key2)
    {
        return true;
    }
    else
    {
        const StrCacheEntry *entry2 = StrCache_GetEntry(key2);
        return (entry1->hash == entry2->hash &&
                0 == strcmp(entry1->str, entry2->str));
    }
}

// returns NULL if key is not in the object
ObjectHashNode *ScriptObject::getNodeForKey(int key)
{
    const StrCacheEntry *entry = StrCache_GetEntry(key);
    unsigned int position = entry->hash % (1u << log2_hashTableSize);
    while (true)
    {
        ObjectHashNode *node = &hashTable[position];
        if (node->key != -1 && keysEqual(key, node->key, entry))
        {
            return node;
        }
        else
        {
            if (node->next == -1)
            {
                return NULL;
            }
            position = node->next;
        }
    }
}

// From Lua: computes ceil(log2(x))
static unsigned int ceillog2(unsigned int x)
{
    static const unsigned char log_2[256] = {  /* log_2[i] = ceil(log2(i - 1)) */
        0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
    };
    size_t l = 0;
    x--;
    while (x >= 256) { l += 8; x >>= 8; }
    return l + log_2[x];
}

void ScriptObject::resizeHashTable(unsigned int minNewSize)
{
    ObjectHashNode *oldHashTable = hashTable;
    size_t oldSize = (1u << log2_hashTableSize);

    log2_hashTableSize = ceillog2(minNewSize);
    size_t newSize = (1u << log2_hashTableSize);
    hashTable = new ObjectHashNode[newSize];
    lastFreeNode = newSize;

    for (ssize_t i = oldSize - 1; i >= 0; i--)
    {
        if (oldHashTable[i].key != -1)
        {
            set(oldHashTable[i].key, &oldHashTable[i].value);
        }
    }
    delete[] oldHashTable;
}

ssize_t ScriptObject::getFreePosition()
{
    while (lastFreeNode > 0)
    {
        --lastFreeNode;
        if (hashTable[lastFreeNode].key == -1)
        {
            return lastFreeNode;
        }
    }

    // there is no free space in the table; it will need to be resized
    return -1;
}

size_t ScriptObject::mainPositionForKey(int key)
{
    return StrCache_GetEntry(key)->hash % (1u << log2_hashTableSize);
}

bool ScriptObject::set(int key, const ScriptVariant *value)
{
    ObjectHashNode *node = getNodeForKey(key);
    if (node != NULL) // Key is already in the object, so just update the value.
    {
        node->value = *value;
        return true;
    }

    // Otherwise, we have to insert a new key.
    size_t mainIndex = mainPositionForKey(key) % (1u << log2_hashTableSize);
    ObjectHashNode *mainPosition = &hashTable[mainIndex];
    if (mainPosition->key == -1) // No collision, so just put the key+value in the main position.
    {
        mainPosition->key = key;
        mainPosition->next = -1;
        mainPosition->value = *value;
    }
    else // The main position is already taken.
    {
        ssize_t freePosition = getFreePosition();
        if (freePosition == -1)
        {
            // The hash table is full; it needs to be resized so the element can be inserted.
            resizeHashTable(1 + (1u << log2_hashTableSize));
            return set(key, value);
        }

        ObjectHashNode *freeNode = &hashTable[freePosition];
        ObjectHashNode *otherPosition = &hashTable[mainPositionForKey(mainPosition->key)];
        if (otherPosition == mainPosition)
        {
            // The colliding node is in its main position. Put the new key+value in the free position.
            freeNode->key = key;
            freeNode->value = *value;
            freeNode->next = mainPosition->next;
            mainPosition->next = freePosition;
        }
        else
        {
            // The colliding node is out of its main position. Move it to the free position.
            *freeNode = *mainPosition;
            while (otherPosition->next != (int)mainIndex)
            {
                otherPosition = &hashTable[otherPosition->next];
            }
            otherPosition->next = freePosition;

            // Now the main position is free, so we can put the new key+value there.
            mainPosition->key = key;
            mainPosition->value = *value;
            mainPosition->next = -1;
        }
    }

    return true;
}

bool ScriptObject::set(const ScriptVariant *key, const ScriptVariant *value)
{
    if (key->vt != VT_STR)
    {
        // TODO: include the invalid key in the error message
        printf("error: object key must be a string\n");
        return false;
    }

    set(key->strVal, value);
    StrCache_Ref(key->strVal);
    return true;
}

bool ScriptObject::get(ScriptVariant *dst, const ScriptVariant *key)
{
    if (key->vt == VT_STR)
    {
        ObjectHashNode *node = getNodeForKey(key->strVal);
        if (node)
        {
            *dst = node->value;
            return true;
        }
        else
        {
            printf("error: object has no member named %s\n", StrCache_Get(key->strVal));
            return false;
        }
    }
    else
    {
        // TODO: include the invalid key in the error message
        printf("error: object key must be a string\n");
        return false;
    }
}

void ScriptObject::makePersistent()
{
    if (persistent) return;
    persistent = true; // set it up here to avoid infinite recursion in case of cycles
    for (size_t i = 0; i < (1u << log2_hashTableSize); i++)
    {
        if (hashTable[i].key != -1)
        {
            ScriptVariant_Ref(&hashTable[i].value);
        }
    }
}

void ScriptObject::print()
{
    char buf[256];
    bool first = true;

    if (currentlyPrinting)
    {
        printf("{(object cycle)}");
        return;
    }
    currentlyPrinting = true;

    printf("{");
    for (size_t i = 0; i < (1u << log2_hashTableSize); i++)
    {
        if (hashTable[i].key == -1)
        {
            continue;
        }

        if (!first) printf(", ");
        first = false;
        printf("\"%s\": ", StrCache_Get(hashTable[i].key));
        ScriptVariant *var = &hashTable[i].value;
        if (var->vt == VT_OBJECT) ObjectHeap_GetObject(var->objVal)->print();
        else if (var->vt == VT_LIST) ObjectHeap_GetList(var->objVal)->print();
        else
        {
            ScriptVariant_ToString(var, buf, sizeof(buf));
            printf("%s", buf);
        }
    }
    printf("}");
    currentlyPrinting = false;
}

int ScriptObject::toString(char *dst, int dstsize)
{
#define SNPRINTF(...) { int n = snprintf(dst, dstsize, __VA_ARGS__); dst += n; length += n; dstsize -= n; if (dstsize < 0) dstsize = 0; }
    char buf[256];
    bool first = true;
    int length = 0;

    if (currentlyPrinting)
    {
        SNPRINTF("{(object cycle)}");
        return length;
    }
    currentlyPrinting = true;

    SNPRINTF("{");
    for (size_t i = 0; i < (1u << log2_hashTableSize); i++)
    {
        if (hashTable[i].key == -1)
        {
            continue;
        }

        if (!first) SNPRINTF(", ");
        first = false;
        SNPRINTF("\"%s\": ", StrCache_Get(hashTable[i].key));
        ScriptVariant_ToString(&hashTable[i].value, buf, sizeof(buf));
        SNPRINTF("%s", buf);
    }
    SNPRINTF("}");
    currentlyPrinting = false;
    return length;
#undef SNPRINTF
}


