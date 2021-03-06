/*
    This test tries to make ChronoScript free an object that is still in use by adding a persistent reference,
    removing the persistent reference, and then accessing the object through its original temporary reference.

    This is a special test where the main function needs to be run twice, so runscript has to be modified.
*/

#include "test/expect.h"

int hasRun = 0;
void globalObject;
char globalString;

void main()
{
    if (hasRun == 0)
    {
        globalObject = {"data": 3};
        globalString = "a " + stringPart();
        hasRun = 1;
    }
    else
    {
        log(globalObject.data);
        void obj = globalObject;
        globalObject = 0;
        expect(obj.data, 3);

        log(globalString);
        void str = globalString;
        globalString = 0;
        expect(str, "a string");
    }
}

char stringPart()
{
    return "string";
}

