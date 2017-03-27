#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>

#include "libem5.h"

#define PURPOSE "EuroMISS tools."
#define DEVICE_FILE "/dev/em5"
#define EXECUTABLE "em5toolbox"

const char * USAGE = "[<command>] [-f devfile] [-r num] [-l] [-i usec] [-h]";
const char * ARGS = ""
"  <command>: you must specify command directly  \n"
"             or make descriptive symlink to this executable. \n"
"             Example: `ln -s /bin/"EXECUTABLE" /bin/pchi` \n"
"  -f:   device file, default is '" DEVICE_FILE "'\n"
"  -l    repeat infinitely \n"
"  -r:   repeat several times \n"
"  -i:   minimum interval between repetitions (microseconds) \n"
"  -h    display this help\n";
const char * CONTRIB = "Written by Sergey Ryzhikov <sergey.ryzhikov@ihep.ru>, 03.2017.";


struct conf {
	char * devfile;
	char * whoami;  // executable name
	unsigned repeat_cnt;  // -1 is infinitely
	unsigned interval;
	bool showhelp;
	} cfg = {  
		/// reasonable defaults
		.devfile = DEVICE_FILE,
		.repeat_cnt = 0,
		.interval = 0,
		.showhelp = 0,
	};
	
struct subcommand  {
	const char * name;
	int (*func)(int argc, char ** argv);
};

struct subcommand * commands[] = {
	{"hehe", test},
	{NULL}
};

int test(int argc, char ** argv) {
	printf("test\n");
}
	
int parse_opts (int argc, char ** argv, unsigned * optind);

int main(int argc, char ** argv) {
	
	struct em5ctl * dev;
	unsigned optind;
	
	if (parse_opts(argc, argv, &optind))
		exit(EXIT_FAILURE);

	printf("%s\n", cfg.whoami);
	
	dev = libem5_init("/dev/zero");
	
	return 0;
}


int parse_opts (int argc, char ** argv, unsigned * retoptind)
/** Parse command line options.
 *  Permutes argv: all nonoptions are moved to the end.
 * 
 *  retoptind - an index of the first nonoption in permuted argv
 * 	            or 0 if no nonoptions.
 *  Returns 0 on success.
 */
{
	
	int opt;
	char * subcommand = NULL;  // command to execute
	opterr = 1; /// 1 is to print error messages
	
	if ((cfg.whoami = strrchr(argv[0], '/')))  // basename
		cfg.whoami++;
	else
		cfg.whoami = argv[0];
	
	/// guess subcommand
	if (strcmp(cfg.whoami, EXECUTABLE)) {  // not default executable name
		subcommand = cfg.whoami;
	}
	else {
		/// subcommand is the first argument
		if (argc < 2) {
			fprintf(stderr, "%s %s\n", cfg.whoami, USAGE);
				exit(EXIT_FAILURE);
		}
		subcommand = argv[1];
	}
	
	//TODO: check subcommand is valid
	
	/// global options
	while ((opt = getopt(argc, argv, "hlf:r:i:")) != -1) {
		switch (opt)
		{
			case 'l': 
				cfg.repeat_cnt = -1;  //infinite
				break;
				
			case 'r': 
				cfg.repeat_cnt = (unsigned)atoi(optarg);
				break;
			
			case 'i':
				cfg.interval = (unsigned)atoi(optarg);
				break;
				
			case 'f':
				cfg.devfile = optarg;
				break;
			
			case 'h': /// Print help
				if (subcommand) {
					/// Show subcommand usage info
					cfg.showhelp = true;
				}
				else {
					/// Show general help banner
					fprintf(stderr,"\n" PURPOSE "\n\n" "usage: \n%s %s \n%s \n%s\n",
							argv[0], USAGE, ARGS, CONTRIB);
					exit(0);
				}
				break;
				
			default: /// '?'
				fprintf(stderr, "%s %s \n", cfg.whoami, USAGE);
				exit(EXIT_FAILURE);
		}
	}
	
	//~ int i;
	//~ for (i=0; i < argc; i++)
		//~ printf("opt %d %s\n", i, argv[i]);
	
	*retoptind = (argc > optind) ? optind : 0;
	return 0;
}

/* TODO:
 * subcommands
 * command handlers
 * call library functions in each handler
 * init device correctly
 * 
 */
