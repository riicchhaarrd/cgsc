#include <stdio.h>

int fibR(int n)
{
	if(n<2)
	return n;
	return (fibR(n-2)+fibR(n-1));
}

int main()
{
int N = 34;
printf("fib = %d\n", fibR(N));
return 0;
}
