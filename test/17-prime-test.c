// Determine if a number is prime
int isPrime(int n)
{
    if (n < 2) return 0; // only positive integers can be prime, and 1 is not prime
    // iterate from 2 through truncate(sqrt(n)) to find an integer factor of n
    // for (int i = truncate(sqrt(n)); i >= 2; i--)
    for (int i = n-1; i >= 2; i--)
    {
        // if n is the product of 2 integers, then it is composite, i.e. not prime
        if (n == i * (n / i))
            return 0;
    }
    // otherwise, n is prime
    return 1;
}

int main()
{
    char result = "\n";
    for (int i = 1; i <= 20; i++)
    {
        if (isPrime(i))
        {
            log(i + ": yes");
            // result += i + ": yes\n";
        }
        else
        {
            log(i + ": no");
            // result += i + ": no\n";
        }
    }
    return result;
}
