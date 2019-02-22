#include "test/expect.h"

void main()
{
    globals().foo = 5;
    expect(globals().foo, 5);
    globals().foo = "foo_value1";
    expect(globals().foo, "foo_value1");
    globals().foo = "foo_value2";
    expect(globals().foo, "foo_value2");
}

