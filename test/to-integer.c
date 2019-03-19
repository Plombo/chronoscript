#include "test/expect.h"

void main()
{
    expect(to_integer(3), 3);
    expect(to_integer(3.5), 3);
    expect(to_integer(-3.5), -3);
    expect(to_integer(2147483647.0), 2147483647);
    expect(to_integer(-2147483648.0), -2147483648);
    expect(to_integer("3"), 3);
    expect(to_integer("4294967295"), -1);
    expect(to_integer("-2147483648"), -2147483648);
}

