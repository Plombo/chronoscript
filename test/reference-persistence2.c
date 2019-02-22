/*
    This test tries to make ChronoScript free an object that is still in use by adding a persistent reference,
    removing the persistent reference, and then accessing the object through its original temporary reference.
*/

#include "test/expect.h"

void globalObject;
char globalString;

void main()
{
    void obj = {"data": 3};
    globalObject = obj;
    globalObject = 0;
    expect(obj.data, 3);

    // Constant strings are persistent, so use concatenation to create a temporary string at runtime. Concatenate
    // with a function result so that the script compiler can't do constant folding on it.
    char str = "a " + stringPart();
    globalString = str;
    globalString = 0;
    expect(str, "a string");
}

char stringPart()
{
    return "string";
}

