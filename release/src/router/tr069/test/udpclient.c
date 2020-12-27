/*=======================================================================
  
       Copyright(c) 2009, Works Systems, Inc. All rights reserved.
  
       This software is supplied under the terms of a license agreement 
       with Works Systems, Inc, and may not be copied nor disclosed except 
       in accordance with the terms of that agreement.
  
  =======================================================================*/
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

struct echo_plus_st
{
	uint32_t test_gen_sn;
	uint32_t test_resp_sn;
	uint32_t recv_ts;
	uint32_t reply_ts;
	uint32_t fail_count;
	char data[0];
};

int main(int argc, char *argv[])
{
	struct sockaddr_in si_other;
	int s, slen=sizeof(si_other);
	struct echo_plus_st *eps;
	char buf[1024];
	int res;

	if(argc != 4) {
		fprintf(stderr, "%s ip port content\n", argv[0]);
		exit(1);
	}


	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) {
		fprintf(stderr, "Create socket failed: %s\n", strerror(errno));
		exit(-1);
	}

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(atoi(argv[2]));
	if (inet_aton(argv[1], &si_other.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed: %s\n", strerror(errno));
		exit(1);
	}
	eps = calloc(sizeof(*eps) + strlen(argv[3]), 1);
	strcat(eps->data, argv[3]);
	sendto(s, eps, sizeof(*eps) + strlen(argv[3]), 0, (struct sockaddr *)&si_other, slen);
	free(eps);
	res = recvfrom(s, buf, sizeof(buf) - 1, 0, NULL, NULL);
	buf[res] = 0;
	eps = (struct echo_plus_st *)buf;
	printf("|%s|\n",eps->data);
	
	sleep(2);
	close(s);
	return 0;
}
