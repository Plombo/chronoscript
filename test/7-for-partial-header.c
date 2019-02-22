// Test for-loops with various parts of the loop header missing.
#include "test/expect.h"

// for(;;) is the same as while(1)
void headerEmpty()
{
    int i = 0;
    for (;;)
    {
        ++i;
        if (i == 10) break;
    }
    expect(i, 10);
}

void header1()
{
    int i;
    for (i = 0;;)
    {
        ++i;
        if (i == 10) break;
    }
    expect(i, 10);
}

void header12()
{
    int i;
    for (i = 0; i != 10;)
    {
        ++i;
    }
    expect(i, 10);
}

void header13()
{
    int i;
    for (i = 0;; ++i)
    {
        if (i == 10) break;
    }
    expect(i, 10);
}

void header123()
{
    int i;
    for (i = 0; i != 10; ++i)
    {
    }
    expect(i, 10);
}

void header2()
{
    int i = 0;
    for (;i != 10;)
    {
        ++i;
    }
    expect(i, 10);
}

void header23()
{
    int i = 0;
    for (;i != 10; ++i)
    {
    }
    expect(i, 10);
}

void header3()
{
    int i = 0;
    for (;;)
    {
        ++i;
        if (i == 10) break;
    }
    expect(i, 10);
}

void main()
{
    headerEmpty();
    header1();
    header12();
    header13();
    header123();
    header2();
    header23();
    header3();
}



