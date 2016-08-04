// basic switch, no default, break at end
void basicSwitchBreakEnd(int param)
{
    int var = "unknown";
    switch(param)
    {
        case 0: var = "zero"; break;
        case 1:
        case 2:
            var = "one or two";
            break;
        case 3: var = "three"; break;
    }
    return var;
}
