#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h> /*memset*/
#include <stddef.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <sys/stat.h>
#include <sys/mman.h>

#define PURPOSE "Mmap a file and stream it's contents via TCP.\n" \
	"blocking read() is used to wait for appended data (until read() returns EOF).\n"
#define DEVICE_FILE "/dev/em5"
#define MMAP_SZ_FILE "/sys/module/em5_module/parameters/mem"
#define CONNECT_TIMEOUT 1  // sec

const char * USAGE = "<host> <port> [-p] [-f filename] [-m mmap_size] [-x loop_count] [-h] ";
const char * ARGS = ""
"  <host>: server IP address or hostname \n"
"  <port>: server port or service name\n"
"  -f \t device file, default is " DEVICE_FILE "\n"
"  -p \t print progress \n"
"  -m: memory buffer size in megabytes \n"
"      (can be determined by reading `"MMAP_SZ_FILE"`)\n"
"  -x: \t send several times (useful for debugging). \n"
"  -h \t display help\n";
const char * CONTRIB = "Written by Sergey Ryzhikov <sergey.ryzhikov@ihep.ru>, 03.2017.";
const char * VERSION = "v20.3";

struct conf {
	size_t mmap_sz;
	char * hostname;
	char * port;
	char * filename;
	unsigned repeat_cnt;
	bool progress;
	} cfg = {
		.filename = DEVICE_FILE,
		.repeat_cnt = 0,
	};
	
void print_opts();
int parse_opts(int argc, char ** argv);
int _open_socket(); /* returns: sockfd or -1 on error */

#define PERR(format, ...)	fprintf(stderr, "Error: " format "\n",  ##__VA_ARGS__)


off_t _fsize(int fd) {
/** Get data size by seeking to the end of file. */
    off_t prev, sz;
    prev = lseek(fd, 0L, SEEK_CUR);
    sz   = lseek(fd, 0L, SEEK_END); 
    lseek(fd, prev, SEEK_SET);
    return sz;
}


int get_buf_sz(void) {
/** Get buffer size by reading MMAP_SZ_FILE.
 *  Returns: buffer size in MB, 0 on failure.
 */
	char line[32];
	int ret;
	FILE* file = fopen(MMAP_SZ_FILE, "r");
	
	if (!file) {
		PERR("Can't open %s", MMAP_SZ_FILE);
        return 0;
	}
	
	if (fgets(line, sizeof(line), file)) {
		ret = atoi(line);
    } 
    else {
		PERR("Can't read %s", MMAP_SZ_FILE);
		return 0;
	} 
    
    fclose(file);
    return ret;
}


int stream_file_mmap(int fd, int sockfd, void* fptr, size_t fsz) {
/** Write mmapped file contents to the socket.
 *  read() used to detect "no more data".
 **/
	off_t fd_sz;
	int n;
	int tmp;
	size_t count = 0;  // a number of bytes written
	char *p = (char*) fptr;


	do {
		fd_sz = _fsize(fd);  // data size
		if (fd_sz == 0 && count != 0) // no more data
			break;

		assert(fd_sz <= (off_t)fsz);
		
		n = write( sockfd, ((char*)p + count), (fd_sz - count) );
		if (n < 0) { 
			if (errno == EAGAIN) 
				continue; 
				
			perror("Error in write to socket.");
			fprintf(stderr, "errno %d \n", errno);
			return errno;
		}
		count += n;
		lseek(fd, count, SEEK_SET);  // notify the driver
	
		if(cfg.progress && fd_sz) {
			fprintf(stderr, "\r %3.0f%% ", 100.0 * count / fd_sz );  //print progress
		}
		
	} while ( read(fd, &tmp, sizeof(tmp) ) && count < fsz); /* sleeps here */
	
	fprintf(stdout, "%u ", count);
	
	return 0;
}

int main ( int argc, char ** argv)
{
	int sockfd;
	struct stat sb;
	int fd;
	int ret;
	void * fptr;
	unsigned long fsz;
	unsigned repeat_cnt;

	
	if (parse_opts(argc, argv))
		exit(EXIT_FAILURE);
	
	if (cfg.mmap_sz == 0) {
		cfg.mmap_sz = get_buf_sz(); /* autodetect */
		if (cfg.mmap_sz == 0)
			exit(EXIT_FAILURE);
	}
	//print_opts();

	sockfd = _open_socket();
	if (sockfd < 0) {
		exit(EXIT_FAILURE);
	}
	
	fd = open(cfg.filename, O_RDONLY);
	if (fd == -1) {
		PERR("open() '%s': %s", cfg.filename, strerror(errno));
		return -1;
	}
	
	/// check the file
	if (fstat(fd, &sb) == -1) {
		PERR("fstat() %s: %s", cfg.filename, strerror(errno));
		return 1;
	}
	if (!S_ISREG(sb.st_mode) && !S_ISCHR(sb.st_mode) ) {
		PERR("%s is not a regular or a character file\n", cfg.filename);
		return 1;
	}
	
	/// Try to mmap a whole file
	fsz = cfg.mmap_sz;
	fptr = mmap(NULL, fsz, PROT_READ, MAP_SHARED, fd, 0);
	if (fptr == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}
	
	repeat_cnt = cfg.repeat_cnt;
	do { 
		ret = stream_file_mmap(fd, sockfd, fptr, fsz);
		if (ret) {
			printf("%s\n", strerror(ret));
			break;
		}

	} while (repeat_cnt--);
	
	/// rollup 
	close(sockfd);
	
	if (munmap(fptr, fsz) == -1) {
		perror("munmap");
		return 1;
	}
	
	if (close(fd) == -1) {
		perror("file close");
		return 1;
	}

	return 0;
}

int _open_socket()
{
	int err;
	int sfd;
	struct addrinfo hints;
    struct addrinfo *result, *rp;
	
	struct timeval tv;
	fd_set fdset;
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Stream socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;          /* Any protocol */

	err = getaddrinfo(cfg.hostname, cfg.port, &hints, &result);
	if (err) {
		if (err == EAI_SYSTEM)
			fprintf(stderr, "getaddrinfo: %s\n", strerror(errno));
		else
			fprintf(stderr, "%s: %s\n", cfg.hostname, gai_strerror(err));
		return -1;
	}

	/* getaddrinfo() returns a list of address structures.
	  Try each address until we successfully connect(2).
	  If socket(2) (or connect(2)) fails, we (close the socket
	  and) try the next address. */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		
		if (sfd == -1)
			continue;

		fcntl(sfd, F_SETFL, O_NONBLOCK); /* connect will not block,
		                                    so using select() later */
		connect(sfd, rp->ai_addr, rp->ai_addrlen);
		
		FD_ZERO(&fdset); 
		FD_SET(sfd, &fdset);
		tv.tv_sec = CONNECT_TIMEOUT;  /* socket timeout */
		tv.tv_usec = 0;
		
		if (select(sfd+1, NULL, &fdset, NULL, &tv) == 1) {
			int so_error;
			socklen_t len = sizeof so_error;
			getsockopt(sfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

			if (so_error == 0) {
				break; /* Success */
			}
		}
		else {
			if (errno == EINTR) {
				fprintf(stderr,"%s: Connection timeout\n", rp->ai_canonname);
			}
		}
			
	   close(sfd);
	}

	if (rp == NULL) {  /* No address succeeded */
	   fprintf(stderr, "Could not connect\n");
	   return(-1);
	}

	freeaddrinfo(result);
	return sfd;
}

void print_opts()
{
	fprintf(stderr, "conf:\n"
		"buf size: %zu \n"
		"hostname: %s \n"
		"port: %s \n"
		"device file: %s \n"
		"count: %d \n"
		"progress: %s \n"
		,cfg.mmap_sz
		,cfg.hostname
		,cfg.port
		,cfg.filename
		,cfg.repeat_cnt
		,cfg.progress ? "yes" : "no"
		);
	return;
}

int parse_opts (int argc, char ** argv)
{
	int opt;
	opterr = 1; /// 1 is to print error messages
	
	while ((opt = getopt(argc, argv, "hpf:m:x:")) != -1) {
		switch (opt)
		{
			case 'x': 
				cfg.repeat_cnt = atoi(optarg);
				break;
				
			case 'm': 
				cfg.mmap_sz = 1024 * 1024 * atoi(optarg);
				break;
				
			case 'f':
				cfg.filename = optarg;
				break;

			case 'p':
				cfg.progress = true;
				break;
			
			case 'h': ///help
				fprintf(stderr, PURPOSE "\n""usage: \n%s\n%s\n%s\n%s\n%s\n",
						argv[0], USAGE, ARGS, CONTRIB, VERSION);
				exit(0);
				
			default: /// '?'
				fprintf(stderr, "%s %s", argv[0], USAGE);
				exit(EXIT_FAILURE);
		}
	}
	
	if (argc - optind != 2) {
		fprintf(stderr, "\nWas waiting for a two non-optional arguments!\n\n");
		fprintf(stderr, "Usage: %s %s", argv[0], USAGE);
		exit(EXIT_FAILURE);
	}
	
	cfg.hostname = argv[optind];
	cfg.port = argv[++optind];
		
	return 0;
}
