#ifndef ARRAY_LIST_HPP
#define ARRAY_LIST_HPP

#include <stdlib.h>
#include <string.h>
#include "globals.h"

#define ARRAY_LIST_DEFAULT_CAPACITY 16

// A dynamic array class. Caller is responsible for all bounds checking.
template <typename T>
class ArrayList
{
private:
    T *array;
    size_t numElements;
    size_t capacity;

    inline void resize(size_t newCapacity)
    {
        array = (T*) realloc(array, newCapacity * sizeof(T));
        assert(array != NULL);
        capacity = newCapacity;
    }

public:
    explicit inline ArrayList(size_t initialCapacity) : numElements(0), capacity(initialCapacity)
    {
        if (capacity == 0)
        {
            capacity = ARRAY_LIST_DEFAULT_CAPACITY;
        }

        array = (T*) malloc(capacity * sizeof(T));
    }

    inline ArrayList(size_t initialSize, const T &value) : ArrayList(initialSize)
    {
        numElements = initialSize;

        for (size_t i = 0; i < initialSize; i++)
        {
            array[i] = value;
        }
    }

    inline ArrayList() : ArrayList(ARRAY_LIST_DEFAULT_CAPACITY) {}

    inline ~ArrayList()
    {
        free(array);
    }

    // returns the number of elements in the list
    inline size_t size() const
    {
        return numElements;
    }

    // returns the element at index
    inline T get(size_t index) const
    {
        return array[index];
    }

    // returns a pointer to the element at index
    inline T *getPtr(size_t index)
    {
        return &array[index];
    }

    // sets the element at index to the given value
    inline void set(size_t index, const T &value)
    {
        array[index] = value;
    }

    // adds an element with the given value to the end of the list
    inline void append(const T &newElem)
    {
        if (numElements == capacity)
        {
            resize(capacity * 2);
        }

        array[numElements] = newElem;
        ++numElements;
    }

    // inserts the given value into the list at index i, moving everything after it forward
    // runs in linear (not constant) time
    inline void insert(size_t pos, const T &newElem)
    {
        if (pos == numElements)
        {
            append(newElem);
        }
        else
        {
            if (numElements == capacity)
            {
                resize(capacity * 2);
            }
            memmove(&array[pos + 1], &array[pos], numElements - pos);
            array[pos] = newElem;
            ++numElements;
        }
    }

    // removes the element at index from the list; runs in linear (not constant) time
    inline void remove(size_t pos)
    {
        memmove(&array[pos], &array[pos + 1], numElements - pos);
        --numElements;
    }

    // removes elements from start (inclusive) to end (exclusive)
    inline void removeRange(size_t start, size_t end)
    {
        assert(end >= start);
        memmove(&array[start], &array[end], numElements - (end - start));
        numElements -= (end - start);
    }

    // removes all elements from list (but doesn't change allocated capacity)
    inline void clear()
    {
        removeRange(0, numElements);
    }
};

#endif

