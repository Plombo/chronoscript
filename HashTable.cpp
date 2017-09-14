/* Based on cfuhash.c in libcfu, which carries the following license:
   
   Copyright (c) 2005 Don Owens
   All rights reserved.

   This code is released under the BSD license:

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

     * Redistributions in binary form must reproduce the above
       copyright notice, this list of conditions and the following
       disclaimer in the documentation and/or other materials provided
       with the distribution.

     * Neither the name of the author nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
   STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
   OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "List.h"

/* Perl's hash function */
static uint32_t hash_func(const char *key)
{
    uint32_t hv = 0; /* could put a seed here instead of zero */
    const unsigned char *s = (const unsigned char *)key;
    unsigned char c = *s;
    while (c)
    {
        hv += c;
        hv += (hv << 10);
        hv ^= (hv >> 6);
        c = *++s;
    }
    hv += (hv << 3);
    hv ^= (hv >> 11);
    hv += (hv << 15);

    return hv;
}

/* makes sure the real size of the buckets array is a power of 2 */
static size_t hash_size(size_t s)
{
    size_t i = 1;
    while (i < s) i <<= 1;
    return i;
}

/* returns the index into the buckets array */
inline uint32_t hash_value(const char *key, size_t num_buckets)
{
    uint32_t hv = 0;

    if (key) {
        hv = hash_func(key);
    }

    /* The idea is the following: if, e.g., num_buckets is 32
       (000001), num_buckets - 1 will be 31 (111110). The & will make
       sure we only get the first 5 bits which will guarantee the
       index is less than 32.
    */
    return hv & (num_buckets - 1);
}

HashTable::HashTable(size_t size, uint32_t flags)
  : num_buckets(hash_size(size)),
    entries(0),
    flags(flags),
    high(0.75),
    low(0.25),
    resized_count(0)
{
    this->buckets = (HashNode **)calloc(num_buckets, sizeof(HashNode *));
}

#if 0
extern int cfuhash_set_thresholds(HashTable *ht, float low, float high)
{
    float h = high < 0 ? ht->high : high;
    float l = low < 0 ? ht->low : low;

    if (h < l) return -1;

    ht->high = h;
    ht->low = l;

    return 0;
}
#endif

// Returns the data, or NULL if not found.
HashNode *HashTable::get(const char *name)
{
    uint32_t hv = 0;
    HashNode *entry = NULL;

    hv = hash_value(name, this->num_buckets);

    assert(hv < this->num_buckets);

    for (entry = this->buckets[hv]; entry; entry = entry->next_in_bucket)
    {
        if (!strcmp(name, entry->name)) return entry;
    }

    return NULL;
}

/*
 Add the entry to the hash.
*/
void HashTable::put(HashNode *entry)
{
    uint32_t hv = hash_value(entry->name, this->num_buckets);
    assert(hv < this->num_buckets);

    entry->next_in_bucket = this->buckets[hv];
    this->buckets[hv] = entry;
    this->entries++;

    if (!(this->flags & HASH_TABLE_FROZEN))
    {
        if ((float)this->entries/(float)this->num_buckets > this->high) rehash();
    }
}

void HashTable::clear()
{
    memset(buckets, 0, num_buckets * sizeof(buckets[0]));
    entries = 0;

    if (!(this->flags & HASH_TABLE_FROZEN) &&
        !((this->flags & HASH_TABLE_FROZEN_UNTIL_GROWS) && !this->resized_count))
    {
        if ((float)this->entries/(float)this->num_buckets < this->low) rehash();
    }

}

// removes an entry from the hash table
void HashTable::remove(HashNode *entry)
{
    uint32_t hv = hash_value(entry->name, this->num_buckets);

    if (this->buckets[hv] == entry)
    {
        this->buckets[hv] = entry->next_in_bucket;
        this->entries--;
    }
    else
    {
        HashNode *prev = this->buckets[hv], *cur = prev->next_in_bucket;
        for (; cur; prev = cur, cur = cur->next_in_bucket)
        {
            if (cur == entry)
            {
                prev->next_in_bucket = cur->next_in_bucket;
                this->entries--;
                return;
            }
        }
        assert(!"entry not found in hash table?");
    }
}

void HashTable::rehash()
{
    size_t new_size, i;
    HashNode **new_buckets = NULL;

    new_size = hash_size(this->entries * 2 / (this->high + this->low));
    if (new_size == this->num_buckets)
    {
        return;
    }
    new_buckets = (HashNode **)calloc(new_size, sizeof(HashNode *));

    for (i = 0; i < this->num_buckets; i++)
    {
        HashNode *entry = this->buckets[i];
        while (entry)
        {
            HashNode *nhe = entry->next_in_bucket;
            uint32_t hv = hash_value(entry->name, new_size);
            entry->next_in_bucket = new_buckets[hv];
            new_buckets[hv] = entry;
            entry = nhe;
        }
    }

    this->num_buckets = new_size;
    free(this->buckets);
    this->buckets = new_buckets;
    this->resized_count++;
}


