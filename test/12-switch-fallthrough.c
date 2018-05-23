#include "test/expect.h"

// basic switch with default, no break at end
void switchFallthrough(int param)
{
    int var = "";
    switch(param)
    {
        case 0: var = "zero"; break;
        case 1:
            var = "one or ";
            // FALLTHROUGH
        case 2:
            var += "two";
            break;
        case 3: var = "three"; break;
        default: var = "unknown";
    }
    return var;
}

void main()
{
    expect(switchFallthrough(0), "zero");
    expect(switchFallthrough(1), "one or two");
    expect(switchFallthrough(2), "two");
    expect(switchFallthrough(3), "three");
    expect(switchFallthrough(4), "unknown");
    expect(switchFallthrough(""), "unknown");
}

