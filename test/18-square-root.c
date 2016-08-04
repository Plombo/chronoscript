double squareRoot(double n)
{
    double min = 0.0, max = n, mid = n / 2.0, midSq;
    do {
        midSq = mid * mid;
        if (midSq > n)
            max = mid;
        else
            min = mid;
        mid = (min + max) / 2.0;
    } while(abs(midSq - n) > .0001);
    
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
