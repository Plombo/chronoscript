#include "test/expect.h"

// basic switch, no default, no break at end
void basicSwitch(int param)
{
    int var = "unknown";
    switch(param)
    {
        case 0: var = "zero"; break;
        case 1:
        case 2:
            var = "one or two";
            break;
        case 3: var = "three";
    }
    return var;
}

void main()
{
    expect(basicSwitch(0), "zero");
    expect(basicSwitch(1), "one or two");
    expect(basicSwitch(2), "one or two");
    expect(basicSwitch(3), "three");
    expect(basicSwitch(4), "unknown");
    expect(basicSwitch(""), "unknown");
}

