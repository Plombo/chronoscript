/* This file makes 7 objects persistent: a standalone, then 2 cycles of 3.
   The first will be freed when its refcount drops to 0, but the first cycle
   will need garbage collection to be freed. */

void makeCycle()
{
    void obj1 = {};
    void obj2 = [obj1];
    void obj3 = {"ref3": obj2};
    obj1.ref1 = obj3;
    return obj1;
}

void main()
{
    globals().obj = {"foo": 3};
    globals().obj = makeCycle();
    globals().obj = makeCycle();
}

