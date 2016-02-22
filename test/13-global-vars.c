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
