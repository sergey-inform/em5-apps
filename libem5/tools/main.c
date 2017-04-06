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
#define DEVICE_FILENAME "/dev/em5"
#define EXECNAME "em5toolbox"

const char * USAGE = "[<command>] [-f filename] [-r num] [-l] [-i usec] [-h]";
const char * ARGS = ""
"  <command>: you must specify command directly  \n"
"             or make descriptive symlink to this EXECNAME. \n"
"             Example: `ln -s /bin/"EXECNAME" /bin/pchi` \n"
"  -f:   device file, default is '" DEVICE_FILENAME "'\n"
"  -l    repeat infinitely \n"
"  -r:   repeat several times \n"
"  -i:   minimum repeat_interval between repetitions (microseconds) \n"
"  -h    display this help\n";
const char * CONTRIB = "Written by Sergey Ryzhikov <sergey.ryzhikov@ihep.ru>, 03.2017.";


#define len(a)  (sizeof(a)/sizeof(a[0]))

struct conf {
	char * filename;  // device file
	char * cmd;  // if not EXECNAME
	unsigned repeat_cnt;  // -1 means infinitely
	unsigned repeat_interval;
	bool showhelp;
	int (*handler)(int argc, char ** argv);
	} cfg = {  
		/// reasonable defaults
		.filename = DEVICE_FILENAME,
		.repeat_cnt = 0,
		.repeat_interval = 0,
		.showhelp = false,
	};

int pchi_main(int argc, char ** argv) ;

int test(int argc, char ** argv) {
	printf("test %d %s\n", argc, argv[0]);
	return 0;
}

struct command  {
	const char * str;
	int (*func)(int argc, char ** argv);
};

static struct command commands[] = {
	{"pchi", pchi_main},
	{"hehe", test},
	{"bebe", test}
};


	
int parse_opts (int argc, char ** argv, unsigned * optind);

int main(int argc, char ** argv) {
	
	struct em5dev * dev;
	unsigned optind;
	
	if (parse_opts(argc, argv, &optind))
		exit(EXIT_FAILURE);

	printf("%s\n", cfg.cmd);
	
	dev = libem5_init("/dev/zero");
	cfg.handler(argc-optind,(char**)argv[optind]);
	
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
	int i;
	int opt;
	char * cmd = NULL;  // command to execute
	opterr = 1; /// 1 is to print error messages
	
	if ((cfg.cmd = strrchr(argv[0], '/')))  // basename
		cfg.cmd++;
	else
		cfg.cmd = argv[0];
	
	/// guess cmd
	if (strcmp(cfg.cmd, EXECNAME)) {  // not default: hard- or symlink
		cmd = cfg.cmd;
	}
	else {
		/// cmd is the first argument
		if (argc < 2) {  // no first argument given
			fprintf(stderr, "%s %s\n", cfg.cmd, USAGE);
			exit(EXIT_FAILURE);
		}
		cmd = argv[1];
	}
	
	for(i = len(commands)-1; i>=0; i--) {
		fprintf(stderr, "i=%d %s\n", i,commands[i].str);
		if (!strcasecmp(commands[i].str, cmd)) { //equal
			cfg.handler = commands[i].func;
			break;
		}
	}
	
	if (i == -1) {
		fprintf(stderr,"Err: unknown command '%s'\n", cmd);
		return -EINVAL;
	}
	
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
				cfg.repeat_interval = (unsigned)atoi(optarg);
				break;
				
			case 'f':
				cfg.filename = optarg;
				break;
			
			case 'h': /// Print help
				if (cmd) {
					/// Show cmd usage info
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
				fprintf(stderr, "%s %s \n", cfg.cmd, USAGE);
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
 * cmds
 * command handlers
 * call library functions in each handler
 * init device correctly
 * 
 */
