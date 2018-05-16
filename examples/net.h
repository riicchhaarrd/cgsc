typedef struct
{
	unsigned short sin_family;
	unsigned short sin_port;
	
	unsigned int sin_addr; //long on x64
    char sin_zero[8];
} sockaddr_in;

#define AF_INET 2
#define SOCK_DGRAM 2
#define SOCK_STREAM 1

typedef struct {
        short                    wVersion;
        short                    wHighVersion;
        char                    szDescription[257];
        char                    szSystemStatus[129];
        unsigned short          iMaxSockets;
        unsigned short          iMaxUdpDg;
        char*              lpVendorInfo;
} WSADATA;

net_send_string(cl, s) {
	len = sendto(level.sock, s, strlen(s) + 1, 0, cl, sizeof(cl));
	return len;
}