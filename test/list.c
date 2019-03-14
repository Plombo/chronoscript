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
    list.append("e");
    log(list);
    expect(list[3], "e");

    // insert an element in the middle of the list
    list.insert(3, "f");
    log(list);
    expect(list[3], "f");
    expect(list[4], "e");

    list.remove(3);
    log(list);
    expect(list[3], "e");

    list.remove(0);
    log(list);
    list.remove(0);
    log(list);
    list.remove(0);
    list.remove(0);
    expect(list + "", "[]");
}

