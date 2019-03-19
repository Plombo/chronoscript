#include "test/expect.h"

void main()
{
    char str = "abcdefghijklmnopqrstuvwxyz";

    expect(str.substring(0), str);
    expect(str.substring(24), "yz");
    expect(str.substring(9, 14), "jklmn");
    expect(str.substring(0, 5), "abcde");
    expect(str.substring(0, 0), "");
    expect(str.substring(5, 6), "f");
}

