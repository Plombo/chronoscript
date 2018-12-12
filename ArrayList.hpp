#ifndef ARRAY_LIST_HPP
#define ARRAY_LIST_HPP

#include <stdlib.h>
#include <vector>

/*
    A mutable list class, implemented as a wrapper around std::vector because
    while std::vector has a bad interface, it's going to have better
    performance than an implementation from scratch.
*/
template <typename T>
class ArrayList
{
private:
    std::vector<T> storage;

public:
    inline ArrayList() {}

    // initializes the list with n elements, each with the given value
    inline ArrayList(size_t n, const T &value) : storage(n, value) {}

    // returns the number of elements in the list
    inline size_t size() const
    {
        return storage.size();
    }

    // returns the element at index
    inline T get(size_t index) const
    {
        return storage.at(index);
    }

    // returns a pointer to the element at index
    inline T *getPtr(size_t index)
    {
        return &storage.at(index);
    }

    // sets the element at index to the given value
    inline void set(size_t index, const T &value)
    {
        storage.at(index) = value;
    }

    // adds an element with the given value to the end of the list
    inline void append(const T &value)
    {
        storage.push_back(value);
    }

    // removes the element at index from the list; runs in linear (not constant) time
    inline void remove(size_t index)
    {
        storage.erase(storage.begin() + index);
    }

    // inserts the given value into the list at index i, moving everything after it forward
    // runs in linear (not constant) time
    inline void insert(size_t index, const T &value)
    {
        storage.insert(storage.begin() + index, value);
    }
};

#endif

