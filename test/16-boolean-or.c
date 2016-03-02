// should not use a 'bool' instruction
int shortcOr(int a, int b)
{
    return (!a || !b);
}

// should use a 'bool' instruction
int evalOr(int a, int b)
{
    return a || b;
}

// should not use a 'bool' instruction
int ifOr(int a, int b)
{
    if (a || b)
        return 7;
    else
        return 3;
}

// should not use a 'bool' instruction for any of these
int loopsOr(int a, int b)
{
    while (!(a || b))
    {
        a = 3;
    }
    // log("first loop done");
    do {
        b = 4;
    } while(!(a || b));
    // log("second loop done");
    for (;!(a || b);) {
        a = 5;
    }
    return a;
}

void main()
{
    log("Expect: 1 0 7 3 2 3");
    log(evalOr(3, 4)); // expect 1
    log(evalOr(0, 0)); // expect 0
    log(ifOr(1, 2)); // expect 7
    log(ifOr(0, 0)); // expect 3
    log(loopsOr(2, 0)); // expect 2
    log(loopsOr(0, 0)); // expect 3
}
