#include "test/expect.h"

void main()
{
    // Octal escapes
    // Just like in C, 3 digits max.
    expect("\157\143\164\141\154\040\163\164\162\151\156\147", "octal string");
    expect("\1600", "p0");

    // Hex escapes
    // Unlike C, 2 digits max. It works the way you want, not the C way which is useless.
    expect("\x68\x65\x78\x20\x73\x74\x72\x69\x6e\x67", "hex string");
    expect("\x68\x65\x78\x20\x73\x74\x72\x69\x6E\x67", "hex string");
    expect("\x61ab", "aab");
    return 0;
}


