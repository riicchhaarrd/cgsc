#include <net.h>

#pragma comment(lib, "msvcrt.dll")
#pragma comment(lib, "ws2_32.dll")
#pragma comment(lib, "user32.dll")
#pragma comment(lib, "kernel32.dll")

send_thread(sa)
{
	net_send_string(sa, "how");
	wait 1;
	net_send_string(sa, "are");
	wait 1;
	net_send_string(sa, "you");
	wait 1;
	net_send_string(sa, "doing?");
}

main()
{
	sa = new sockaddr_in;
	memset(sa,0,sizeof(sa));
	
	wsadata = new WSADATA;
	ws = WSAStartup(2,wsadata);
	
	sa.sin_family=AF_INET;
	addr = (int)0;
	inet_pton(AF_INET,"127.0.0.1",&addr);
	sa.sin_addr = addr;
	sa.sin_port=htons(20710);
	
	sock = socket(AF_INET, SOCK_DGRAM, 17);
	level.sock=sock;
	send_thread(sa);
	/*
	sb = buffer(1024);
	$sprintf(sb, "%s", "hello world!\n");
	len = sendto(sock, sb, strlen(sb) + 1, 0, sa, sizeof(sa));
	*/
	/*
	rb = buffer(1024);
	
	rlen = recvfrom(sock, rb, sizeof(rb), 0, 0, 0);
	$printf("received %d bytes -> %s\n", rlen, rb);
	*/
	closesocket(sock);
}