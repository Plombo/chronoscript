void actualExpect(char expression, int value, int expected)
{
    if (value == expected)
        log("PASS: " + expression + " = " + expected);
    else
        log("FAIL: " + expression + " = " + value + " != " + expected);
}