#include "test/expect.h"

void main()
{
    expect(to_string("str"), "str");
    expect(to_string(3), "3");
    expect(to_string([]), "[]");
    expect(to_string({}), "{}");
}

