#include <net.h>

#pragma comment(lib, "msvcrt.dll")
#pragma comment(lib, "ws2_32.dll")
#pragma comment(lib, "user32.dll")
#pragma comment(lib, "kernel32.dll")

main() {
	
	server_port=80;
	sa = new sockaddr_in;
	memset(sa,0,sizeof(sa));
	$printf("sizeof sa = %d\n", sizeof(sa));
	
	wsadata = new WSADATA;
	
	ws = WSAStartup(2,wsadata);
	
	sa.sin_family=AF_INET;
	sa.sin_addr=0;
	sa.sin_port=htons(server_port);
	
	sock = socket(AF_INET, SOCK_STREAM, 0);
	
	printf("sock = %\n", sock);
	printf("err = %\n",WSAGetLastError());
	
	bind(sock,sa,sa.size);
	listen(sock, 5);
	
	cl = new sockaddr_in;
	clsz = (int)cl.size;
	
	filename = "index.html";
	hdr="HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n";		
	
	rb = buffer(1024);
	$printf("rb size = %d\n",sizeof(rb));
	
	while(1==1)
	{
		client_fd = accept(sock, cl, &clsz);
		if(client_fd == -1)
		{
			printf("error!\n");
			return;
		}
		recv(client_fd,rb,sizeof(rb),0);
		$printf("got %s\n",rb);
		
		response=hdr + read_text_file(filename);
		
		//response="HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<!DOCTYPE html><html><head><title>Bye-bye baby bye-bye</title><style>body { background-color: #111 }h1 { font-size:4cm; text-align: center; color: black; text-shadow: 0 0 2mm red}</style></head><body><h1>Goodbye, world!</h1></body></html>\r\n";
		responselen=strlen(response);
		
		send(client_fd,response,responselen,0);
		closesocket(client_fd);
	}
	
	closesocket(sock);
}