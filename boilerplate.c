#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>  // mmap
#include <sys/stat.h>  // fstat
#include <assert.h>

#define PURPOSE "Change me to a meaningful text."

const char * USAGE = "-f [-t test] <filename> [-h]\n";
const char * ARGS = "\n"
	"  <filename>:  a file as an argument; \n"
	"  -t:  parameter; \n"
	"  -f   flag; \n";
const char * CONTRIB = "\nWritten by , \n @ihep.ru, 01.1970.\n";

struct conf {
	char * filename;
	int t;
	char f;
	} cfg = {
		.filename = "-",  // stdin by default
		.t = 123,  // default value for t
		.f = false,  // flag
	};

int parse_opts(int argc, char ** argv, struct conf *);
void print_opts(struct conf*);


void print_file_sz(FILE * fptr) {
/** Demo function.
 *  Print file size and unix permissions to stdout.
 */

	int fd;  // file descriptor
	struct stat sb;  // file stats
	off_t fsize;
	mode_t fmode;
	
	fd = fileno(fptr);
	fstat(fd, &sb);
	fsize = sb.st_size;
	fmode = sb.st_mode;
	
	printf("file size: %jd bytes, mode: (%3o)\n",
			(intmax_t)fsize,
			fmode & 0777
			);
}


int main( int argc, char ** argv)
{
	FILE *fptr;
	
	if (parse_opts(argc, argv, &cfg))
		exit(EXIT_FAILURE);
		
	print_opts(&cfg);
	
	if(!strcmp(cfg.filename,"-")) {
		fptr = stdin;
		if (isatty(0)) {
			//stdin is a terminal
		}
	}
	else {
		fptr = fopen(cfg.filename, "rb");
		if (fptr == NULL) {
			fprintf(stderr, "Err open %d: %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		
	}
	
	print_file_sz(fptr);
	
	fclose(fptr);
	return 0;
}


int parse_opts (int argc, char ** argv, struct conf * cfg)
/** Parse command line arguments.
 * Return 0 on success.
 */
{
	int opt;
	opterr = 1; /// 1 is to print error messages
	
	
	while ((opt = getopt(argc, argv, "hft:")) != -1) {
		switch (opt)
		{
		case 'f': 
			cfg->f = true;
			break;
			
		case 't':
			cfg->t = atoi(optarg);
			if (cfg->t < 0) {
				fprintf(stderr,
						"Err: t parameter can't be less then 0, "
						"but '%d' given. \n",
						cfg->t
						);
				exit(EXIT_FAILURE);
			}
			break;
			
		case 'h':  ///help
			fprintf(stderr, PURPOSE "\n""usage: \n%s %s %s %s",
					argv[0], USAGE, ARGS, CONTRIB);
			exit(0);
			
		default:  /// '?'
			fprintf(stderr, "%s %s", argv[0], USAGE);
			exit(EXIT_FAILURE);
		}
	}
	
	if (argc - optind != 1) {
		fprintf(stderr, "\nErr: expecting one non-optional argument, "
			"%d given!\n",
			argc-optind
			);
		fprintf(stderr, "Usage: %s %s", argv[0], USAGE);
		exit(EXIT_FAILURE);
	}
	
	cfg->filename = argv[optind];
		
	return 0;
}

void print_opts(struct conf * cfg)
/** Print program configuration options. */
{
	fprintf(stderr,
		"config: \n"
		"file: %s \n"
		"f: %s \n"
		"t: %d \n"
		"\n",
		cfg->filename,
		cfg->f ? "yes" : "no",
		cfg->t
		);
	return;
}

