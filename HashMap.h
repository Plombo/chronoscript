#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdio.h>
#include "cfuhash.h"

template <typename T>
class HashMap
{
private:
    cfuhash_table_t *hashTable;
public:
    inline HashMap(uint32_t flags = 0) { hashTable = cfuhash_new_with_flags(flags | CFUHASH_FROZEN_UNTIL_GROWS); }
    inline ~HashMap() { cfuhash_destroy(hashTable); }
    inline void setFreeFunction(cfuhash_free_fn_t func) { cfuhash_set_free_function(hashTable, func); }
    inline bool containsKey(const char *key) { return cfuhash_exists(hashTable, key); }
    inline T get(const char *key) { return (T)cfuhash_get(hashTable, key); }
    inline T put(const char *key, T value) { return (T)cfuhash_put(hashTable, key, value); }
    inline T remove(const char *key) { return (T)cfuhash_delete(hashTable, key); }
    inline void prettyPrint() { cfuhash_pretty_print(hashTable, stdout); }
};

#endif

