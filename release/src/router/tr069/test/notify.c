#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

//this is only for test
//username: username
//password: password
//BASIC
//

int main(int argc, char *argv[])
{
	int ipv6_flag = 0;
	if(argc != 3) {
		printf("Usage: %s serverip port\n", argv[0]);
		return -1;
	} else if(fork() == 0) { //Child process
		int fd;
		char *host, *path, *port;
		
		struct addrinfo hints, *res;
		int rc;
		
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		printf("host: %s, cport: %s\n", argv[1], argv[2]);
		if((rc = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0) {
                	printf("Get server address information failed: %s!\n", gai_strerror(rc));
                	return -1;
		}
		do {
			fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
			if(connect(fd, res->ai_addr, res->ai_addrlen) == 0){
		    		break;
			} else {
		    		printf("Connect to server(%s) failed: %s\n", argv[1], strerror(errno));
		    		close(fd);
		    		return -1;
			}
	    	} while((res = res->ai_next) != NULL);

		//connect ok
		{
			int len;
			int res = 0;
			char buffer[512];
			char *from;

			len = strlen(argv[2]);

			len = snprintf(buffer, sizeof(buffer),
				"GET / HTTP/1.1\r\n"
				"User-Agent: Jakarta Commons-HttpClient/3.0-rc1\r\n"
				"Authorization: Basic dXNlcm5hbWU6cGFzc3dvcmQ=\r\n"
				"Host: %s:%s\r\n"
				"Content-Length: 0\r\n"
				"\r\n", argv[1], argv[2]);

			from = buffer;
			do {
				len -= res;
				from += res;
				res = send(fd, from, len, 0);
			} while(res > 0);

			if(res == 0) {
				printf("send OK\n");
				close(fd);
			}
		}
	}
	return 0;
}
