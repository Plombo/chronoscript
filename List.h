#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "ralloc.h"

#pragma pack(push, 4)

/* hash table flags */
#define HASH_TABLE_FROZEN 1      /* do not rehash when the size thresholds are reached */
#define HASH_TABLE_FROZEN_UNTIL_GROWS (1 << 1) /* do not shrink the hash until it has grown */

class HashNode {
public:
    char *name;
    HashNode *next_in_bucket;
};

class HashTable {
public:
    HashNode **buckets;
    unsigned int num_buckets;
    unsigned int entries; /* Total number of entries in the table. */
    uint32_t flags;
    float high;
    float low;
    unsigned int resized_count;

public:
    HashTable(size_t size, uint32_t flags);
    inline HashTable() : HashTable(8, HASH_TABLE_FROZEN_UNTIL_GROWS) {}
    inline ~HashTable() { free(buckets); }

    HashNode *get(const char *name);
    void put(HashNode *entry);
    void remove(HashNode *entry);
    void clear(); // TODO remove; List will take care of it

private:
    void rehash();
};

template <typename T>
class Node : public HashNode
{
public:
    Node<T> *prev;
    Node<T> *next;
    T value;

    inline Node(T data, const char *name)
    {
        this->name = name ? strdup(name) : NULL;
        this->next_in_bucket = NULL;
        this->value = data;
    }

    inline ~Node()
    {
        free(this->name);
    }
};

template <typename T> class ListIterator;

// doubly linked list class, with a hash table to access elements by name
// TODO make a non-templated BaseList class to avoid inlining large functions
template <typename T>
class List
{
public: // these should really be private
    Node<T> *current;
    Node<T> *first;
    Node<T> *last;

private:
    HashTable hashTable;
    unsigned int theSize;

    inline Node<T> *createNode(T value, const char *name)
    {
        Node<T> *node = new Node<T>(value, name);
        if (name) hashTable.put(node);
        node->prev = node->next = NULL;
        return node;
    }

public:
    inline List()
      : current(NULL),
        first(NULL),
        last(NULL),
        hashTable(32, HASH_TABLE_FROZEN_UNTIL_GROWS),
        theSize(0)
    {}

    inline ~List() { clear(); }

    // removes all nodes from the list
    inline void clear()
    {
        hashTable.clear();
        Node<T> *cur = first;
        while (cur)
        {
            Node<T> *next = cur->next;
            delete cur;
            cur = next;
        }
        first = last = current = NULL;
        theSize = 0;
    }

    // adds a new item to the list before the given node
    // returns the newly created node
    inline Node<T> *insertBeforeNode(Node<T> *otherNode, T value, const char *name = NULL)
    {
        Node<T> *node = createNode(value, name);
        Node<T> *prev = otherNode->prev;
        node->prev = prev;
        if (prev)
            prev->next = node;
        node->next = otherNode;
        otherNode->prev = node;
        theSize++;
        if (first == otherNode)
            first = node;
        return node;
    }

    // adds a new item to the list after the given node
    inline Node<T> *insertAfterNode(Node<T> *otherNode, T value, const char *name = NULL)
    {
        Node<T> *node = createNode(value, name);
        Node<T> *next = otherNode->next;
        node->next = next;
        if (next)
            next->prev = node;
        otherNode->next = node;
        node->prev = otherNode;
        theSize++;
        if (last == otherNode)
            last = node;
        return node;
    }

    // adds a new item to the list before the current item
    inline void insertBefore(T value, const char *name = NULL)
    {
        if (current)
        {
            current = insertBeforeNode(current, value, name);
        }
        else // list is empty
        {
            Node<T> *node = createNode(value, name);
            current = first = last = node;
            theSize++;
        }
    }

    // adds a new item to the list after the current item
    inline void insertAfter(T value, const char *name = NULL)
    {
        if (current)
        {
            current = insertAfterNode(current, value, name);
        }
        else // list is empty
        {
            Node<T> *node = createNode(value, name);
            current = first = last = node;
            theSize++;
        }
    }

    // removes a node from the list
    // it would be nice if we didn't have to inline this
    inline void removeNode(Node<T> *node)
    {
        if (node->prev)
        {
            node->prev->next = node->next;
        }
        else
        {
            assert(node == first);
            first = node->next;
        }

        if (node->next)
        {
            node->next->prev = node->prev;
        }
        else
        {
            assert(node == last);
            last = node->prev;
        }

        if (current == node)
        {
            current = node->prev ? node->prev : node->next;
        }

        if (node->name)
        {
            hashTable.remove(node);
        }

        delete node;
        theSize--;
    }

    // removes the current item from the list
    inline void remove()
    {
        if (current)
        {
            removeNode(current);
        }
    }

    inline void gotoNext()
    {
        if (current != last)
        {
            current = current->next;
        }
    }

    inline void gotoPrevious()
    {
        if (current != first)
        {
            current = current->prev;
        }
    }

    inline void gotoLast() { current = last; }
    inline void gotoFirst() { current = first; }

    // get the data value of the current list item
    inline T retrieve() { return current->value; }

    // set the data value of current list item to a new value
    inline void update(T newValue) { current->value = newValue; }

    // this is slow, don't use it
    // sets this->current to value if found
    inline bool includes(T e);

    // if found, sets this->current to item with given name
    inline bool findByName(const char *name)
    {
        if (name)
        {
            Node<T> *foundNode = static_cast<Node<T>*>(hashTable.get(name));
            if (foundNode)
            {
                this->current = foundNode;
                return true;
            }
        }
        return false;
    }

    // returns name of current list item (can be NULL)
    inline const char *getName() { return current->name; }

    // returns number of items in list
    inline int size() { return theSize; }

    inline bool isEmpty() { return (theSize == 0); }

    inline Node<T> *currentNode() { return current; }
    inline void setCurrent(Node<T> *node) { current = node; }

    // this is slow, don't use it
    inline int getIndex()
    {
        int i = 0;
        Node<T> *node = first;
        while (node != current)
        {
            ++i;
            node = node->next;
            assert(node);
        }
        return i;
    }

    // this is slow, don't use it
    inline bool gotoIndex(unsigned int index)
    {
        if (index >= theSize) return false;

        Node<T> *node = first;
        for (unsigned int i = 0; i < index; i++)
        {
            node = node->next;
        }
        current = node;
        return true;
    }

#if 0
    inline void copyFrom(const List<T> *src) {}
#endif

    inline ListIterator<T> iterator() { return ListIterator<T>(this); }
};

template <typename T>
class ListIterator {
private:
    List<T> *list;
    Node<T> *current;
    bool removed;
public:
    inline ListIterator(List<T> *list) : list(list), current(list->first), removed(false) {}
    inline Node<T> *node() { return current; }
    inline const char *name() { return current->name; }
    inline T value() { return current->value; }
    inline T* valuePtr() { return &current->value; }
    inline void update(T newValue) { current->value = newValue; }
    inline bool isFinished() { return (current == NULL); }
    //inline ListIterator<T> next() { return ListIterator<T>(current->next); }
    inline bool hasNext() { return current && current->next; }
    inline void gotoNext()
    {
        // if we just removed a node, we're already on the next node
        if (current && !removed)
            current = current->next;
        removed = false;
    }
    inline void remove()
    {
        assert(current);
        Node<T> *next = current->next,
                *savedCurrent = list->currentNode();
        list->setCurrent(current);
        list->remove();
        if (current != savedCurrent)
            list->setCurrent(savedCurrent);
        current = next;
        removed = true;
    }
};

template <typename T>
bool List<T>::includes(T e)
{
    for (ListIterator<T> iter = iterator(); !iter.isFinished(); iter.gotoNext())
    {
        if (iter.value() == e) return true;
    }
    return false;
}

#if 1 // TODO: replace all CList<T> uses with List<T*>, and remove CList
template <typename T>
class CList : public List<T*>
{
    DECLARE_RALLOC_CXX_OPERATORS(CList);
};

#define foreach_list(list, type, var) \
    for(ListIterator<type*> var = list.iterator(); !var.isFinished(); var.gotoNext())

#define foreach_plist(list, type, var) \
    for(ListIterator<type*> var = list->iterator(); !var.isFinished(); var.gotoNext())
#else
#define foreach_list(list, type, var) \
    for(ListIterator<type> var = list.iterator(); !var.isFinished(); var.gotoNext())

#define foreach_plist(list, type, var) \
    for(ListIterator<type> var = list->iterator(); !var.isFinished(); var.gotoNext())
#endif

#pragma pack(pop)

#endif // !defined(LIST_H)

