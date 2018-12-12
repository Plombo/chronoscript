#include "test/expect.h"

void main()
{
    void list = ["a", "b", "c"];
    log(list);
    expect(list[0], "a");
    expect(list[1], "b");
    expect(list[2], "c");
    list[1] = "d";
    log(list);
    expect(list[0], "a");
    expect(list[1], "d");
    expect(list[2], "c");
}

