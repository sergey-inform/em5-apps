#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/mman.h>

#define PURPOSE "Send all EM5 readout buffer contents via TCP to the server."
#define DEVICE_FILE "/dev/em5"
#define CONNECT_TIMEOUT 1  //sec

const char * USAGE = "<buf size> <host> <port> [-D device] [-n count] [-h] \n";
const char * ARGS = "\n"
"  <buf size>: kernel buffer size in megabytes \n"
"      (get it with `cat /sys/module/em5_module/parameters/mem`)"
"  <host>: server IP address \n"
"  <port>: server port \n"
"  -D \t default is " DEVICE_FILE "\n"
"  -n \t repeat transmission <count> times (for debugging). \n"
"  -h \t display help";
const char * CONTRIB = "\nWritten by Sergey Ryzhikov <sergey.ryzhikov@ihep.ru>, 11.2014.\n";

struct conf {
	unsigned long buf_sz;
	char * hostname;
	unsigned int port;
	char * device;
	unsigned int count;
	} cfg = {
		.device = DEVICE_FILE,
		.count = 1,
	};
	
void print_opts();
int parse_opts(int argc, char ** argv);
int _open_socket(); /* returns: sockfd or -1 on error */

#define PRERR(format, ...)	fprintf(stderr, "Error: " format "\n",  ##__VA_ARGS__)

off_t fsize(int fd){
/** Get data size by seeking to it's end */
    off_t prev, sz;
    prev	= lseek(fd, 0L, SEEK_CUR);
    sz  	= lseek(fd, 0L, SEEK_END); 
    lseek(fd, prev, SEEK_SET);
    return sz;
}

//~ unsigned long get_buf_sz(void) {
	//~ 
	//~ 
//~ }

int main ( int argc, char ** argv)
{
	int sockfd;
	struct stat sb;
	int fd;
	off_t flen, fcount, n;
	void * fptr;
	unsigned int ui;
	
	if (parse_opts(argc, argv))
		exit(-1);
	//~ print_opts/();
		
	sockfd = _open_socket();
	if (sockfd < 0) {
		exit(EXIT_FAILURE);
	}
	
	fd = open(cfg.device, O_RDONLY);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	
	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		return 1;
	}
	
	if (!S_ISREG(sb.st_mode) && !S_ISCHR(sb.st_mode) ) {
		fprintf(stderr, "%s is not a regular or a character file\n", cfg.device);
		return 1;
	}
	
	flen = cfg.buf_sz;
	
	fptr = mmap(NULL, flen, PROT_READ, MAP_SHARED, fd, 0);
	if (fptr == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	
	fcount = fsize(fd);
	
	for (ui = cfg.count; ui != 0; ui--) {
		n = write( sockfd, fptr, fcount);
		//n = send( sockfd, buff, bsize, 0);
		if (n < 0) {
			perror("ERROR in write. ");
			exit(1);
		}
		fprintf(stderr,".");
	}
	
	/// rollup 
	close(sockfd);
	
	if (close(fd) == -1) {
		perror("file close");
		return 1;
	}
	
	if (munmap(fptr, flen) == -1) {
		perror("munmap");
		return 1;
	}
	
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
		return -1;
	} 
	
	/// set to non-blocking mode
	if( (arg = fcntl(sockfd, F_GETFL, NULL)) < 0) { 
		perror("Error fcntl(..., F_GETFL)"); 
		return -1;
	}
	arg |= O_NONBLOCK; 
	if( fcntl(sockfd, F_SETFL, arg) < 0) { 
		 perror("Error fcntl(..., F_SETFL)"); 
		 return -1; 
	}
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = inet_addr(cfg.hostname); /* assign the address */
	address.sin_port = htons(cfg.port); 
	
	/// Trying to connect with timeout 
	res = connect(sockfd, (struct sockaddr *)&address, sizeof(address)); 
	if (res < 0) {
		if (errno == EINPROGRESS) {  /// something went wrong
			do {
				tv.tv_sec = CONNECT_TIMEOUT;  /* socket timeout */
				tv.tv_usec = 0;
				FD_ZERO(&fdset); 
				FD_SET(sockfd, &fdset);
				
				res = select(sockfd+1, NULL, &fdset, NULL, &tv); 
				if (res < 0 && errno != EINTR) {
					PRERR("connection failed");
					return -2;
				} 
				else if (res > 0) {
					int so_error;
					socklen_t len = sizeof so_error;
					if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0) { 
						PRERR("getsockopt() failed");
						return -2;
					}
					
					if (so_error) {
						PRERR("connection delayed");
						return -2; 
					}
					break;
				}
				else {  // res == 0
					PRERR("timeout in select()");
					return -2;
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
		"buf size: %lu \n"
		"hostname: %s \n"
		"port: %u \n"
		"device: %s \n"
		"count: %d \n",
		cfg.buf_sz,
		cfg.hostname,
		cfg.port,
		cfg.device,
		cfg.count
		);
	return;
}

int parse_opts (int argc, char ** argv)
{
	int opt;
	opterr = 1; /// 1 is to print error messages
	
	while ((opt = getopt(argc, argv, "p:D:n:h")) != -1) {
		switch (opt)
		{
			case 'p': 
				cfg.port = atoi(optarg);
				break;
				
			case 'n': 
				cfg.count = atoi(optarg);
				break;
				
			case 'D':
				cfg.device = optarg;
			
			case 'h': ///help
				fprintf(stderr, PURPOSE "\n""usage: \n%s %s %s %s",
						argv[0], USAGE, ARGS, CONTRIB);
				exit(0);
				
			default: /// '?'
				fprintf(stderr, "%s %s", argv[0], USAGE);
				exit(EXIT_FAILURE);
		}
	}
	
	if (argc - optind != 3) {
		fprintf(stderr, "\nWas waiting for a three non-optional arguments!\n\n");
		fprintf(stderr, "Usage: %s %s", argv[0], USAGE);
		exit(EXIT_FAILURE);
	}
	
	cfg.buf_sz = 1024 * 1024 * atoi(argv[optind]);
	cfg.hostname = argv[++optind];
	cfg.port = atoi(argv[++optind]);
		
	return 0;
}
