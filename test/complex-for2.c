int counter()
{
    return 10;
}

void forLoop()
{
    int counter = counter();
    log(counter);
    for (int i = 0; i < counter; i++)
    {
        log("iteration ", i);
    }
}

void main()
{
    forLoop();
}

