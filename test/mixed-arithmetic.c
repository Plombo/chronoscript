#include "test/expect.h"

void main()
{
    // Make sure the result is an integer by testing that division is truncating
    log("Integer arithmetic:");
    expect((10 + 3) / 2, 6);
    expect((10 - 3) / 2, 3);
    expect((10 * 2) / 7, 2);
    expect((10 / 2) / 2, 2);

    log("Floating point arithmetic:");
    expect(10.0 + 2.5, 12.5);
    expect(10.0 - 2.5, 7.5);
    expect(10.0 * 2.5, 25.0);
    expect(10.0 / 2.5, 4.0);

    log("Mixed arithmetic:");
    expect(10 + 2.5, 12.5);
    expect(10 - 2.5, 7.5);
    expect(10 * 2.5, 25.0);
    expect(10 / 2.5, 4.0);
}

