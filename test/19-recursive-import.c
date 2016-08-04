// tests recursive importing -- 2 files should be able to import each other without issue

#include "test/expect.h"
#import "test/19b-recursive-import.c"

int foo() // used by 19b-recursive-import.c
{
    return 3;
}

void main()
{
    // the bar function is imported from 19b-recursive-import.c
    expect(bar(), 8);
}
