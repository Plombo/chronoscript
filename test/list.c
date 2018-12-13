#include "test/expect.h"

void main()
{
    // initialize list
    void list = ["a", "b", "c"];
    log(list);
    expect(list[0], "a");
    expect(list[1], "b");
    expect(list[2], "c");

    // set a list element
    list[1] = "d";
    log(list);
    expect(list[0], "a");
    expect(list[1], "d");
    expect(list[2], "c");

    // append an element to the list
    list_append(list, "e");
    log(list);
    expect(list[3], "e");

    // insert an element in the middle of the list
    list_insert(list, 3, "f");
    log(list);
    expect(list[3], "f");
    expect(list[4], "e");

    list_remove(list, 3);
    log(list);
    expect(list[3], "e");
}

