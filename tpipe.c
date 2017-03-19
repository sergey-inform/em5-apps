#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/mman.h>

#define PURPOSE "Send stdin via tcp."
#define CONNECT_TIMEOUT 1

const char * USAGE = "<host> <port> [-h] \n";
const char * ARGS = "\n"
"  <host>: listening server IP \n"
"  <port>: server port \n"
"  -n \t repeat transmission <count> times (for debug purpose). \n";
#define ARGN 2

const char * CONTRIB = "\nWritten by Sergey Ryzhikov, IHEP@Protvino, 03.2015.\n";
#define BUFSIZE 4096

struct conf {
	char * hostname;
	unsigned int port;
	} cfg;
	
void print_opts();
int parse_opts(int argc, char ** argv);
int _open_socket(); /* returns: sockfd */

int main ( int argc, char ** argv)
{
	int sockfd;
	char buf[BUFSIZE];
	int ret;
	
	if (parse_opts(argc, argv))
		exit(-1);
		
	//~ print_opts();
		
	sockfd = _open_socket();
	if (sockfd < 0) {
		exit(EXIT_FAILURE);
	}
	
	int bytes; 
    while(( bytes = fread(buf, sizeof(char), BUFSIZE, stdin) )){
        ret = write(sockfd, buf, bytes);
        if (ret != bytes) {
			perror("socket write error");
			exit(-1);
		}
    }
	
	/// rollup 
	close(sockfd);
	return 0;
}

int _open_socket()
{
	int res;
	int sockfd;
	struct sockaddr_in address;
	struct timeval tv;
	fd_set fdset;
	long arg;
	
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) { 
		perror("Error creating socket");
		exit(0); 
	} 
	
	/// set to non-blocking mode
	if( (arg = fcntl(sockfd, F_GETFL, NULL)) < 0) { 
		perror("Error fcntl(..., F_GETFL)"); 
		exit(0); 
	}
	arg |= O_NONBLOCK; 
	if( fcntl(sockfd, F_SETFL, arg) < 0) { 
		 perror("Error fcntl(..., F_SETFL)"); 
		 exit(0); 
	}
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(cfg.hostname); /* assign the address */
	address.sin_port = htons(cfg.port); 
	
	
	
	/// Trying to connect with timeout 
	res = connect(sockfd, (struct sockaddr *)&address, sizeof(address)); 
	if (res < 0) {
		if (errno == EINPROGRESS) { ///something goes wrong
			//~ fprintf(stderr, "EINPROGRESS in connect() - selecting\n");
			do {
				tv.tv_sec = CONNECT_TIMEOUT; /* socket timeout */
				tv.tv_usec = 0;
				FD_ZERO(&fdset); 
				FD_SET(sockfd, &fdset);
				
				res = select(sockfd+1, NULL, &fdset, NULL, &tv); 
				if (res < 0 && errno != EINTR) {
					perror("Error connecting");
					exit(0);
				} 
				else if (res > 0) {
					int so_error;
					socklen_t len = sizeof so_error;
					if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) { 
						perror("Error in getsockopt()");
						exit(0);
					}
					
					if (so_error) {
						perror("Error in delayed connection()");
						exit(0); 
					}
					break;
				}
				else {
					perror("Timeout in select");
					exit(0);
				}
			} while (1);
		}
		else {
			perror("Connection error!");
			exit(0);
		}
	}
	/// set to non-blocking mode again... 
	if( (arg = fcntl(sockfd, F_GETFL, NULL)) < 0) { 
		 perror("Error2 fcntl(..., F_GETFL)"); 
		 exit(0); 
	} 
	arg &= (~O_NONBLOCK); 
	if( fcntl(sockfd, F_SETFL, arg) < 0) { 
		 perror("Error2 fcntl(..., F_SETFL)"); 
		 exit(0); 
	}
		
	return sockfd;
}

void print_opts()
{
	fprintf(stderr, "conf:\n"
		"hostname: %s \n"
		"port: %u \n"
		,
		cfg.hostname,
		cfg.port
		);
	return;
}

int parse_opts (int argc, char ** argv)
{
	int opt;
	opterr = 1; /// 1 is to print error messages
	
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt)
		{
			case 'h': ///help
				fprintf(stderr, PURPOSE "\n""usage: \n%s %s %s %s",
						argv[0], USAGE, ARGS, CONTRIB);
				exit(0);
				
			default: /// '?'
				fprintf(stderr, "%s %s", argv[0], USAGE);
				exit(EXIT_FAILURE);
		}
	}
	
	if (argc - optind != ARGN) {
		fprintf(stderr, "\nWas waiting for a three non-optional arguments!\n\n");
		fprintf(stderr, "Usage: %s %s", argv[0], USAGE);
		exit(EXIT_FAILURE);
	}
	
	cfg.hostname = argv[optind];
	cfg.port = atoi(argv[++optind]);
		
	return 0;
}
