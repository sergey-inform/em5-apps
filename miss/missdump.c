#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/mman.h>  // mmap
#include <sys/stat.h>  // fstat
#include <assert.h>

#include "em5.h"
#include "hptdc.h"

#define PURPOSE "Print a raw miss data in human readable format (for debugging)."
#define CFG_ALLNUM 0	/* all modules */

const char * USAGE = "[-n num] <filename> \n";
const char * ARGS = "\n"
"  <filename>:  raw data file; \n"
"  num:  module number, '31' means all modules (default); \n";
const char * CONTRIB = "\nWritten by Sergey Ryzhikov, \nsergey.ryzhikov@ihep.ru, 03.2017.\n";

/*
 * TODO: 
 * - add "skip" parameter
 * 
 */ 

struct conf {
	char * filename;
	int a;
	unsigned num;
	uint64_t fskip;
	} cfg = {
		.filename = "-",  // stdin by default
		.num = CFG_ALLNUM,  // CFG_ALLNUM -- all moudles
		.fskip = 0,	// skip from the beginning 
	};

int parse_opts(int argc, char ** argv, struct conf *);
void print_opts(struct conf*);
int nhex(unsigned long val);


//~ static EMWORD next_emword(FILE *fptr) {
	//~ 
	//~ if (S_ISCHR(fmode) || S_ISFIFO(fmode) || S_ISSOCK(fmode)){ //S_ISREG(fmode)
	//~ 
	//~ /// try mmap()
	//~ memblock = mmap(NULL, fsize, PROT_READ, MAP_SHARED, fd, 0);
	//~ if (memblock == MAP_FAILED) {
		//~ fprintf(stderr, "mmap failed %d: %s\n", errno, strerror(errno));
		//~ fprintf(stderr, "using read() instead\n");
	//~ }
	//~ else{
		//~ /// tell OS how to handle allocated pages
		//~ madvise((void*)memblock, fsize, MADV_SEQUENTIAL | MADV_DONTNEED);
	//~ }
//~ }

struct missstate {
	enum state {INIT, OK};
};

void pprint_dump( FILE *fptr, unsigned num, uint64_t fskip)
/** Print file dump word by word.
 */
{
	
}

void print_dump( FILE *fptr, unsigned long wcount /* could be 0 if chardev or fifo */)
/**
 * Print em3 dump word by word;
 */
{
	HPTDC_WORD hptdc_word= 0;
	unsigned long wi = 0;  // buf index
	unsigned wcount_nhex = nhex(wcount);  //how many hex digits to print
	
	#define BUFLEN 64
	static char prbuf[BUFLEN];

	unsigned event_nwords = 0; // a number of data words in event
	unsigned event_timemark = 0; 
	unsigned mod_num;  // an index number of the em-3 module
	
	unsigned duplicate_counter = 0;
	EMWORD em_word, em_word_prev = {0};
	
	if (fptr == NULL)
		return;
	
	
	while (1 == fread(&em_word,sizeof(EMWORD),1,fptr))
	{
	//~ for (; wi < wcount; wi++) {
		
		if (em_word.word == em_word_prev.word) {
			duplicate_counter++;
			continue;
		}
		
		if (duplicate_counter) {
			printf("= DUPLICATE euromiss word %X \t%u times\n",
					em_word.word,
					duplicate_counter
					);
			duplicate_counter =0;
		}
		
		em_word_prev = em_word;

		switch (em_word.hdr)
		{
		case 0xBE:  ///begin event
			event_nwords = 0;
			event_timemark = em_word.timemark;
			break;
			
		case 0xDE:  ///PCHN
			
		case 0xFE:  ///finish event
			event_timemark |= em_word.timemark <<16;
			printf("= event end ts: %u \t"
					"words: %u "
					"%s"
					"\n",
					event_timemark,
					event_nwords,
					(event_nwords & 0b1) ? "not even!" : ""
					);
			break;
			
		case 0x1F:  ///em5 event flags? (appears before FE)
			//ignore
			//TODO: ?
			break;
			
		default:  ///data word
			if (em_word.hdr > 0x1F) {
				printf("wrong hdr %X\n", em_word.hdr);  
				break;
			}
			
			event_nwords++;
			
			mod_num = em_word.hdr;
			if (cfg.num != CFG_ALLNUM && mod_num != cfg.num)
				continue;  // skip the data for other modules, but count;
			
			if (event_nwords & 0b1) {
				hptdc_word = em_word.data;
			}
			else {
				hptdc_word |= em_word.data << 16;
				
				if ( hptdc_word & 0b1<<31) { //highest bit is set
					printf("skip word %8X -- missing MISS word?\n", hptdc_word);
					hptdc_word = 0;
					event_nwords++; //sync
					continue;
				}
				
				//~ snprint_hptdc_word(prbuf, BUFLEN-1, hptdc_word);
				printf( "%0*lX %08X >> %02u" //nhex, wi, raw
						" %s \n",
						wcount_nhex, wi, hptdc_word,
						em_word.hdr,
						prbuf
						);
			}
		};
				
	}
	
	if (feof(fptr)) {
		printf(" EOF\n");
	}
	
}

int main( int argc, char ** argv)
{
	FILE *fptr;
	int fd;
	struct stat sb;
	
	const void *memblock = NULL;
	uint64_t fsize;
	//~ mode_t fmode;
	
	if (parse_opts(argc, argv, &cfg))
		exit(EXIT_FAILURE);
	//~ print_opts(&cfg);
	
	fptr = fopen(cfg.filename, "rb");
	if (fptr == NULL) {
		fprintf(stderr, "Err open %d: %s\n", errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	//~ fd = fileno(fptr);
	//~ fstat(fd, &sb);
	//~ fsize = sb.st_size;
	//~ fmode = sb.st_mode;
	//~ fprintf(stderr, "Size: %llu\n", fsize);
	
	/// print the file word by word
	print_dump(fptr, fsize/sizeof(EMWORD));
	
	munmap((void*)memblock, fsize);
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
	
	while ((opt = getopt(argc, argv, "p:D:n:h")) != -1) {
		switch (opt)
		{
			case 'a': 
				cfg->a = atoi(optarg);
				break;
				
			case 'n':
				cfg->num = atoi(optarg);
				if (cfg->num > 0x1F) {
					fprintf(stderr,
							"module num can't be more than 31,"
							"%d given",
							cfg->num
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
		fprintf(stderr, "\nErr: Was waiting for a one non-optional argument!\n");
		fprintf(stderr, "Usage: %s %s", argv[0], USAGE);
		exit(EXIT_FAILURE);
	}
	
	cfg->filename = argv[optind];
		
	return 0;
}

void print_opts(struct conf * cfg)
/** Print program configuration.
 */
{
	fprintf(stderr,
		"file: %s \n"
		"module: %d \n"
		"\n",
		cfg->filename,
		cfg->num
		);
	return;
}

int nhex(unsigned long val)
/** How many hex chars do we need to print val.*/
{	
	
	// TODO: make it better =)
	// int __builtin_clzll
	if      ( val & 0xF0000000) return 8;
	else if ( val & 0x0F000000) return 7;
	else if ( val & 0x00F00000) return 6;
	else if ( val & 0x000F0000) return 5;
	else if ( val & 0x0000F000) return 4;
	else if ( val & 0x00000F00) return 3;
	else if ( val & 0x000000F0) return 2;
	else if ( val & 0x0000000F) return 1;
	return 0;
}
