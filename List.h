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
    void clear();

private:
    void rehash();
};

class BaseNode : public HashNode
{
public:
    BaseNode *prev;
    BaseNode *next;
};

template <typename T>
class Node : public BaseNode
{
public:
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

    inline Node<T> *getPrevious() { return static_cast<Node<T>*>(prev); }
    inline Node<T> *getNext() { return static_cast<Node<T>*>(next); }
};

class BaseList
{
public: // these should be protected
    BaseNode *current;
    BaseNode *first;
    BaseNode *last;
protected:
    unsigned int theSize;

protected:
    inline BaseList() : current(NULL), first(NULL), last(NULL), theSize(0) {}

    void insertNodeBeforeNode(BaseNode *newNode, BaseNode *otherNode);
    void insertNodeAfterNode(BaseNode *newNode, BaseNode *otherNode);
    void insertNodeBeforeCurrent(BaseNode *newNode);
    void insertNodeAfterCurrent(BaseNode *newNode);
    void baseRemoveNode(BaseNode *node);
};

template <typename T> class ListIterator;

// doubly linked list class, with a hash table to access elements by name
// TODO make a non-templated BaseList class to avoid inlining large functions
template <typename T>
class List : public BaseList
{
    DECLARE_RALLOC_CXX_OPERATORS(List);

private:
    HashTable hashTable;

    inline Node<T> *createNode(T value, const char *name)
    {
        Node<T> *node = new Node<T>(value, name);
        if (name) hashTable.put(node);
        node->prev = node->next = NULL;
        return node;
    }

public:
    inline List()
      : BaseList(),
        hashTable(32, HASH_TABLE_FROZEN_UNTIL_GROWS)
    {}

    inline ~List() { clear(); }

    // removes all nodes from the list
    inline void clear()
    {
        hashTable.clear();
        Node<T> *cur = static_cast<Node<T>*>(first);
        while (cur)
        {
            Node<T> *next = cur->getNext();
            delete cur;
            cur = next;
        }
        first = last = current = NULL;
        theSize = 0;
    }

    // adds a new item to the list before the given node
    // returns the newly created node
    inline void insertBeforeNode(BaseNode *otherNode, T value, const char *name = NULL)
    {
        Node<T> *newNode = createNode(value, name);
        insertNodeBeforeNode(newNode, otherNode);
    }

    // adds a new item to the list after the given node
    inline void insertAfterNode(BaseNode *otherNode, T value, const char *name = NULL)
    {
        Node<T> *newNode = createNode(value, name);
        insertNodeAfterNode(newNode, otherNode);
    }

    // adds a new item to the list before the current item
    inline void insertBefore(T value, const char *name = NULL)
    {
        Node<T> *newNode = createNode(value, name);
        insertNodeBeforeCurrent(newNode);
    }

    // adds a new item to the list after the current item
    inline void insertAfter(T value, const char *name = NULL)
    {
        Node<T> *newNode = createNode(value, name);
        insertNodeAfterCurrent(newNode);
    }

    // removes a node from the list
    // it would be nice if we didn't have to inline this
    inline void removeNode(Node<T> *node)
    {
        baseRemoveNode(node);

        if (node->name)
        {
            hashTable.remove(node);
        }

        delete node;
    }

    // removes the current item from the list
    inline void remove()
    {
        if (current)
        {
            removeNode(static_cast<Node<T>*>(current));
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
    inline T retrieve() const { return static_cast<Node<T>*>(current)->value; }

    // set the data value of current list item to a new value
    inline void update(T newValue) { static_cast<Node<T>*>(current)->value = newValue; }

    // searches for value in list (slow)
    // if found, sets this->current to found node
    // returns true if value is in list, false if not
    inline bool includes(T value)
    {
        for (BaseNode *node = first; node; node = node->next)
        {
            if (static_cast<Node<T>*>(node)->value == value)
            {
                current = node;
                return true;
            }
        }
        return false;
    }

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
    inline const char *getName() const { return current->name; }

    // returns number of items in list
    inline int size() const { return theSize; }

    inline bool isEmpty() const { return (theSize == 0); }

    inline Node<T> *firstNode() const { return static_cast<Node<T>*>(first); }
    inline Node<T> *lastNode() const { return static_cast<Node<T>*>(last); }
    inline Node<T> *currentNode() const { return static_cast<Node<T>*>(current); }
    inline void setCurrent(Node<T> *node) { current = node; }

    // this is slow, don't use it
    inline int getIndex()
    {
        int i = 0;
        BaseNode *node = first;
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

        BaseNode *node = first;
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
    inline ListIterator(List<T> *list) : list(list), current(list->firstNode()), removed(false) {}
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
            current = current->getNext();
        removed = false;
    }
    inline void remove()
    {
        assert(current);
        Node<T> *next = current->getNext(),
                *savedCurrent = list->currentNode();
        list->setCurrent(current);
        list->remove();
        if (current != savedCurrent)
            list->setCurrent(savedCurrent);
        current = next;
        removed = true;
    }
};

#define foreach_list(list, type, var) \
    for(ListIterator<type> var = list.iterator(); !var.isFinished(); var.gotoNext())

#define foreach_plist(list, type, var) \
    for(ListIterator<type> var = list->iterator(); !var.isFinished(); var.gotoNext())

#pragma pack(pop)

#endif // !defined(LIST_H)

