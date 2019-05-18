#include <test.h>
#pragma comment(lib, "lib/lib.dll")

typedef struct
{
	char test[1024];
	int a,b,c,d;
} msg_t;

main()
{
	
	q = (msg_t)get_message();
	
	std();
	
	$sprintf(q->test,"%s test", "WEW");
	
	
	printf("test = %\n", (string)q->test);
	
	printf("a = %\n", q->a);
	printf("b = %\n", q->b);
	printf("c = %\n", q->c);
	printf("d = %\n", q->d);
}
