int globalA;
void globalB, globalC;

// basic switch with default, no break at end
void useGlobals()
{
    if (globalA)
    {
        void tmp = globalB;
        globalB = globalC;
        globalC = tmp;
    }
    globalA = !globalA;
}

void main()
{
    globalA = 3;
    globalB = 2.5;
    globalC = "hello";
    useGlobals();
    return globalB + globalC;
}
