#include <net.h>

#pragma comment(lib, "msvcrt.dll")
#pragma comment(lib, "ws2_32.dll")
#pragma comment(lib, "user32.dll")
#pragma comment(lib, "kernel32.dll")

main()
{
	sa = new sockaddr_in;
	memset(sa,0,sizeof(sa));
	
	wsadata = new WSADATA;
	ws = WSAStartup(2,wsadata);
	
	sa.sin_family=AF_INET;
	sa.sin_addr = 0; //htonl(INADDR_ANY)
	sa.sin_port=htons(20710);
	
	sock = socket(AF_INET, SOCK_DGRAM, 17);
	level.sock=sock;
	bind(sock,sa,sizeof(sa));
	
	rb = buffer(1024);
	cl = new sockaddr_in;
	clsize=(int)sizeof(cl);
	while(1==1)
	{
		len = recvfrom(sock, rb, sizeof(rb), 0, cl, &clsize);
		$printf("got %d bytes -> %s\n", len, rb);
		net_send_string(cl, "hey fam");	
	}
	closesocket(sock);
}