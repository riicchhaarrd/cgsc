#!/usr/bin/env gsc
fibR(n)
{
	if(n < 2) {
		return n;
	}
	return (fibR(n - 2) + fibR(n - 1));
}

main()
{
	N = 34;
	printf("fib: %\n",fibR(N));
}