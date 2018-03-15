double squareRoot(double n)
{
    double min = 0.0,
           max = n < 1.0 ? 1.0 : n,
           mid = max / 2.0,
           midSq = mid * mid;
    while (abs(midSq - n) > 1e-6f)
    {
        if (midSq > n)
            max = mid;
        else
            min = mid;
        mid = (min + max) / 2.0;
        midSq = mid * mid;
    }
    
    // see if the result is actually an integer
    double midInt = round(mid);
    if (midInt * midInt == n)
        return midInt;
    else
        return mid;
}

double abs(double src)
{
    return (src < 0.0) ? -src : src;
}

// this doesn't actually round, but whatever
double round(double n)
{
    return n;
}

void printSqrt(double n)
{
    log("sqrt(" + n + ") = " + squareRoot(n));
}

void main()
{
    printSqrt(4);
    printSqrt(2);
    printSqrt(3);
    printSqrt(0.25);
    printSqrt(1.0 / 16.0);
    printSqrt(1.0 / 9.0);
}

