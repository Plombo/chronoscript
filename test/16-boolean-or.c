#include "test/expect.h"

// should not use a 'bool' instruction
int shortcOr(int a, int b)
{
    return (!a || !b);
}

// should use a 'bool' instruction
int evalOr(int a, int b)
{
    return a || b;
}

// should not use a 'bool' instruction
int ifOr(int a, int b)
{
    if (a || b)
        return 7;
    else
        return 3;
}

// should not use a 'bool' instruction for any of these
int loopsOr(int a, int b)
{
    while (!(a || b))
    {
        a = 3;
    }
    // log("first loop done");
    do {
        b = 4;
    } while(!(a || b));
    // log("second loop done");
    for (;!(a || b);) {
        a = 5;
    }
    return a;
}

void main()
{
    // log("Expect: 1 0 7 3 2 3");
    expect(evalOr(3, 4), 1);
    expect(evalOr(0, 0), 0);
    expect(ifOr(1, 2), 7);
    expect(ifOr(0, 0), 3);
    expect(loopsOr(2, 0), 2);
    expect(loopsOr(0, 0), 3);
}
