// should short-circuit
// shouldn't emit a 'bool' instruction
int shortc(int a, int b)
{
    return (!a && !b);
}

// should use a 'bool' instruction
int evalAnd(int a, int b)
{
    return a && b;
}

// should not use a 'bool' instruction
int ifAnd(int a, int b)
{
    if (a && b)
        return 7;
    else
        return 3;
}

// should not use a 'bool' instruction for any of these
int loopsAnd(int a, int b)
{
    while (a && b)
    {
        a = 0;
    }
    // log("first loop done");
    do {
        b = 0;
    } while(a && b);
    // log("second loop done");
    for (;a && b;) {
        a = 0;
    }
    return a;
}

void main()
{
    log("Expect: 1 0 7 3 2 0");
    log(evalAnd(3, 4)); // expect 1
    log(evalAnd(3, 0)); // expect 0
    log(ifAnd(1, 2)); // expect 7
    log(ifAnd(2, 0)); // expect 3
    log(loopsAnd(2, 0)); // expect 2
    log(loopsAnd(2, 1)); // expect 0
}
