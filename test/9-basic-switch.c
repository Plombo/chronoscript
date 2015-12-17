// basic switch, no default, no break at end
void basicSwitch(int param)
{
    int var = "unknown";
    switch(param)
    {
        case 0: var = "zero"; break;
        case 1:
        case 2:
            var = "one or two";
            break;
        case 3: var = "three";
    }
    return var;
}
