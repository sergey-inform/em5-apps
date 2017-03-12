#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#define PURPOSE "Print EM3 raw data in human readable format."

const char * USAGE = "<filename> \n";
const char * ARGS = "\n"
"  <filename>: raw data file \n";
const char * CONTRIB = "\nWritten by Sergey Ryzhikov, sergey.ryzhikov@ihep.ru, 03.2017.\n";

struct conf {
	char * filename;
	int a;
	} cfg = {
		.filename = "-",  // stdin by default
	};


int parse_opts(int argc, char ** argv, struct conf *);
void print_opts(struct conf*);

int main( int argc, char ** argv)
{
	if (parse_opts(argc, argv, &cfg))
		exit(-1);
		
	print_opts(&cfg);

	return 0;
}


int parse_opts (int argc, char ** argv, struct conf * cfg)
/** Parse command line arguments.
 * Return 0 on success.
 */
{
	int opt;
	opterr = 1; /// 1 is to print error messages
	
	while ((opt = getopt(argc, argv, "p:D:n:h")) != -1) {
		switch (opt)
		{
			case 'a': 
				cfg->a = atoi(optarg);
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
	
	if (argc - optind > 1) {
		fprintf(stderr, "\nErr: Was waiting for a one non-optional argument!\n");
		fprintf(stderr, "Usage: %s %s", argv[0], USAGE);
		exit(EXIT_FAILURE);
	}
	
	if (argc - optind == 1) 
		cfg->filename = argv[optind];
		
	return 0;
}

void print_opts(struct conf * cfg)
/** Print program configuration.
 */
{
	fprintf(stderr,
		"file: %s\n",
		cfg->filename
		);
	return;
}
