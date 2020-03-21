a()
{
	printf("hello world\n");
}

main()
{
	b = ::a;
	
	printf("b %\n", typeof(b));
	[[ b ]]();
}