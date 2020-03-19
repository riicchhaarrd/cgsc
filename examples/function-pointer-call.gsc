a(v)
{
	printf("hello world %\n", v);
	return "test";
}

main()
{
	b = ::a;
	
	printf("b %\n", typeof(b));
	printf("ret = %\n", [[ b ]](123));
}