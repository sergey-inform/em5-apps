#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <string.h>
#include <strings.h>

#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h> 
#include <errno.h>

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

off_t fsize(int fd){
    off_t prev, sz;
    prev = lseek(fd, 0L, SEEK_CUR);
    sz = lseek(fd, 0L, SEEK_END); 
    lseek(fd, prev, SEEK_SET);
    return sz;
}

int main(int argc, char *argv[])
{
	int sockfd, portno, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	
	struct stat sb;
	off_t len, count;
	char *p;
	int fd;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <file> <len>\n", argv[0]);
		return 1;
	}
	
	len = atol(argv[2]);

	portno = atoi(argv[4]);
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) { 
		error("Error creating socket");
		exit(1);
	}

	server = gethostbyname(argv[3]);
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}
	
	memset((char *) &serv_addr,0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy((char *)&serv_addr.sin_addr.s_addr,
		(char *)server->h_addr,
		 server->h_length);
	serv_addr.sin_port = htons(portno);
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
		error("ERROR connecting");
	}
	
	

	
	if (len <= 0) {
		perror("len should be interger number of bytes");
		return 1;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		return 1;
	}

	if (!S_ISREG(sb.st_mode) && !S_ISCHR(sb.st_mode) ) {
		fprintf(stderr, "%s is not a regular or a character file\n", argv[1]);
		return 1;
	}

	p = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	
	count = fsize(fd);
	fprintf(stderr, "count: %ld\n", count);
	
	
	n = write( sockfd, p, count);
	//n = send( sockfd, buff, bsize, 0);
	if (n < 0) {
		fprintf(stderr, "ERROR, write, %s\n", strerror(errno) );
		exit(1);
	}
	
	
	
	close(sockfd);

	if (close(fd) == -1) {
		perror("close");
		return 1;
	}

	//~ for (c = 0; c < len; c++)
		//~ putchar(p[c]);

	if (munmap(p, len) == -1) {
		perror("munmap");
		return 1;
	}

	return 0;
}
