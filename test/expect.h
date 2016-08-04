#ifndef EXPECT_H
#define EXPECT_H

#import "test/expect.c"
#define expect(expr, result) actualExpect(#expr, (expr), result)

#endif
