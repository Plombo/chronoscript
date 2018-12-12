#include "List.hpp"

// adds a new item to the list before the given node
// returns the newly created node
void BaseList::insertNodeBeforeNode(BaseNode *newNode, BaseNode *otherNode)
{
    BaseNode *prev = otherNode->prev;
    newNode->prev = prev;
    if (prev)
        prev->next = newNode;
    newNode->next = otherNode;
    otherNode->prev = newNode;
    theSize++;
    if (first == otherNode)
        first = newNode;
}

// adds a new item to the list after the given node
void BaseList::insertNodeAfterNode(BaseNode *newNode, BaseNode *otherNode)
{
    BaseNode *next = otherNode->next;
    newNode->next = next;
    if (next)
        next->prev = newNode;
    otherNode->next = newNode;
    newNode->prev = otherNode;
    theSize++;
    if (last == otherNode)
        last = newNode;
}

void BaseList::insertNodeBeforeCurrent(BaseNode *newNode)
{
    if (current)
    {
        insertNodeBeforeNode(newNode, current);
        current = newNode;
    }
    else // list is empty
    {
        current = first = last = newNode;
        theSize++;
    }
}

void BaseList::insertNodeAfterCurrent(BaseNode *newNode)
{
    if (current)
    {
        insertNodeAfterNode(newNode, current);
        current = newNode;
    }
    else // list is empty
    {
        current = first = last = newNode;
        theSize++;
    }
}

// removes a node from the list
// it would be nice if we didn't have to inline this
void BaseList::baseRemoveNode(BaseNode *node)
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

    theSize--;
}

