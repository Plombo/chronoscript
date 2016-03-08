#import "test/19-recursive-import.c"

int bar()
{
    return 5 + foo();
}
