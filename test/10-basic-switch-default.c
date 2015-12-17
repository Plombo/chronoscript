// basic switch with default, no break at end
void basicSwitchDefault(int param)
{
    int var;
    switch(param)
    {
        case 0: var = "zero"; break;
        case 1:
        case 2:
            var = "one or two";
            break;
        case 3: var = "three"; break;
        default: var = "unknown";
    }
    return var;
}
