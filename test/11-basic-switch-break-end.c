#include "test/expect.h"

// basic switch, no default, break at end
void basicSwitchBreakEnd(int param)
{
    int var = "unknown";
    switch(param)
    {
        case 0: var = "zero"; break;
        case 1:
        case 2:
            var = "one or two";
            break;
        case 3: var = "three"; break;
    }
    return var;
}

void main()
{
    expect(basicSwitchBreakEnd(0), "zero");
    expect(basicSwitchBreakEnd(1), "one or two");
    expect(basicSwitchBreakEnd(2), "one or two");
    expect(basicSwitchBreakEnd(3), "three");
    expect(basicSwitchBreakEnd(4), "unknown");
    expect(basicSwitchBreakEnd(""), "unknown");
}
