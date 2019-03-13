#include "test/expect.h"

// this was handled incredibly wrong, but works now
void trip()
{
    void object1 = {"a": 3};
    void i = object1;

    i = object1.a = 4;
    log("i = " + i + "\n");
    log("object1 = " + object1 + "\n");
    expect(i, 4);
    expect(object1.a, 4);
}

// the reference "object1.a" in this used to evaluate to the whole object instead of the property
void trip2()
{
    void object1 = {"a": 3};
    void i = object1;

    i = object1.a = 4;
    log("i = " + i + "\n");
    log("object1 = " + object1 + "\n");
    log(object1.a); // note how this one doesn't work! it should log "4" but it logs the whole object1 instead
    expect(i, 4);
    expect(object1.a, 4);
}

void trip3()
{
    void object = {"key": "key2",
                   "key2": 5};
    int val = object[object["key"]]; // should be 5, but currently the entire object is assigned instead
    expect(val, 5);
}

void main()
{
    void object = {};
    void object2 = {"key": 77};
    void object3 = {"key1": "value1",
                    "key2": 7.5,
                    "key"+3: { "nestedkey": 81 }};
    log("object:  " + object);
    log("object2: " + object2);
    log("object3: " + object3);
    log("object3.key2: " + object3.key2);
    log("object3.key3: " + object3.key3);
    log("object3[\"key3\"]: " + object3["key3"]);
    log("object3.key3.nestedkey: " + object3.key3.nestedkey);
    log("object3.key3[\"nestedkey\"]: " + object3.key3["nested" + "key"]);
    object3.key3.nestedkey -= 40;
    object3.key3.newkey = 13;
    expect(object3.key3.nestedkey, 41);
    expect(object3.key3.newkey, 13);
    expect(object3.has_key("key2"), 1);
    expect(object3.has_key("key4"), 0);
    
    trip();
    trip2();
    trip3();
    return object3;
}


