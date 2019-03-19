#include "test/expect.h"

void main()
{
    expect(to_decimal(3), 3.0);
    expect(to_decimal(2147483647), 2147483647.0);
    expect(to_decimal(-2147483648), -2147483648.0);
    expect(to_decimal(3.5), 3.5);
    expect(to_decimal("3.0"), 3.0);
    expect(to_decimal("3.1e4"), 3.1e4);
    expect(to_decimal("3.1e+4"), 3.1e+4);
    expect(to_decimal("3.1e-4"), 3.1e-4);
}

