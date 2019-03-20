/* ChronoScript used to fail to compile this function with an erroneous
   "possible use of undefined value" error. This was caused by a naive
   implementation of dead code elimination that did not detect a cycle in
   which two otherwise dead phi instructions referenced each other. As a
   bonus, the fix to this bug also allows the script compiler to completely
   eliminate the unnecessary "y" variable from the function.
 */

void main()
{
    int x = 0, y = 0, a, b, c;
	
    for (a = 1; a < 10; a++)
    {
	    for (b = 2; b < 10; b++)
	    {
	        for (c = 3; c < 10; c++)
	        {
	            x += 1;
	            y += 2;
	        }
	    }
	}

    log(x);
}

