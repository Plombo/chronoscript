// should trigger an error since it uses a variable that may be undefined
int maybeUndefined(int x)
{
    int y;
    if (x)
        y = 3;
    return y + 4;
}

void main()
{
    maybeUndefined(1);
    maybeUndefined(0);
}

