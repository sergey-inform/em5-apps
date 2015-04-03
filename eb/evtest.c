/* 
 * Parse em-5 raw data files, check the file syntax and optionally print some statistics.
 */
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> //malloc()
#include <assert.h>
#include <inttypes.h> //to be shure we are using 32-bit words

const char *argp_program_version = "evstat 2.0";
const char *argp_program_bug_address = "\"Sergey Ryzhikov\" <sergey-inform@ya.ru>";
static char doc[] = "Parse EM-5 events, check event file syntax and pring some statistics.";
static char args_doc[] = "[FILENAME]";
static struct argp_option options[] = { 
	{ "quiet", 'q', 0, 0, "No output to stderr."},
	{ "events", 'e', 0, 0, "Print a count of events."},
	{ "stats", 's', 0, 0, "Print statistics."},
	{ "dump", 'd', 0, 0, "Dump event times."},
	{ "verbose", 'v', 0, 0, "Be verbose."},
	{ 0 } 
};

struct args {
	bool isStats, isQuiet, isVerbose, isEvents, isDump;
	char *infile;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct args *args = state->input;
	switch (key) {
		case 'q': args->isQuiet = true; break;
		case 's': args->isStats = true; break;
		case 'v': args->isVerbose = true; break;
		case 'e': args->isEvents = true; break;
		case 'd': args->isDump = true; break;
		case ARGP_KEY_ARG: 
			if (state->arg_num >= 1){
				argp_usage(state);
			}
			args->infile = arg;
			break;
		default: return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

enum err_num {
	OK,

	ZEROES, ONES,
	NO_FE_BEFORE_BE, NO_BE_BEFORE_FE,
	NO_1F_BEFORE_FE, WRONG_LEN_1F, FILE_LEN_ODD,

	WARNING, //warnings
	UNKNOWN, ADDR_ORDER, NO_SPILL_INFO,
};


char * err_str[] = {
	[ZEROES] = "A zero word.",
	[ONES] = "A word with all ones.",
	[NO_FE_BEFORE_BE]= "A new 0xBE before 0xFE.",
	[NO_BE_BEFORE_FE]= "0xFE without corresponding 0xBE.",
	[NO_1F_BEFORE_FE] = "No 0x1f before 0xfe",
	[WRONG_LEN_1F] = "Event length counter value != actual event length.",
	[FILE_LEN_ODD] = "File length is not a multiple of 4 bytes",
	[UNKNOWN] = "Unknown event type",
	[ADDR_ORDER] = "Miss addresses are not in ascending order",
	[NO_SPILL_INFO] = "No spill info event",
};

typedef union {
	unsigned int whole;
	struct {
		unsigned short addr;
		unsigned short data;
	};
	unsigned char byte[4];
} emword;


typedef struct {
	bool opened;
	unsigned int evt_count;
	unsigned int ts_first;

/** TODO: spill_stats
	struct stat{
		unsigned int wlen_summ;
		unsigned int max_wlen;
		unsigned int empty_events;
	};
*/
} spill;


typedef struct {
	bool opened;
	unsigned int ts;
	unsigned int wpos;
	unsigned int wlen;
	unsigned int wlen_1f;
} event;


int main(int argc, char *argv[])
{
	struct args args = {0};
	FILE *instream;
	size_t bytes;
	enum err_num err;

	assert(sizeof(emword) == 4); // in case of troubles on 64-bit platforms use -m32 compiler option

	argp_parse(&argp, argc, argv, 0, 0, &args);

	if (args.infile) {
		instream = fopen (args.infile, "r");
		if (instream == NULL) {
			err = errno;
			fprintf(stderr, "Error opening file: %s\n", strerror( err ));
			return err;
		}
		
	} else {
		instream = stdin;
	}

	/// set file buffer type and size
	setvbuf(instream, NULL /*auto allocation on first read*/, _IOFBF, 4*1024);


	emword backlog[8] = {0};
	
	assert(__builtin_popcount (sizeof(backlog)) == 1 ); //check backlog size is a power of 2
	// a nice trick for non-gcc compilers #define nHasOnlyOne1( p ) ( ((p)==0) || ( (p) & ((p)-1) ) )

	unsigned int wpos = 0;

	#define backlog_sz (sizeof(backlog) / sizeof(backlog[0]) )
	#define cur  (wpos % backlog_sz)
	#define prev ((wpos-1) % backlog_sz)
	#define ago(n) ((wpos-n) % backlog_sz)

	spill sp = {0};
	event evt = {0};

	//print headers
	if (args.isVerbose && args.isDump) {
		printf("timestamp\twlen\n");
	}

	while ((!err && (bytes = fread( &backlog[cur], 1, sizeof(emword), instream) )))
	{
		emword wrd = backlog[cur];
//		printf("-%08X\n", wrd);
		
		if (bytes && bytes != 4 /*size of word*/) {
			err = FILE_LEN_ODD;
			printf("leftover is %d bytes.\n",bytes);
		}

		//check is valid
		if (wrd.whole == 0x0U) {
			err = ZEROES;
			break;
		}

		if (wrd.whole == ~0x0U) {
			err = ONES;
			break;
		}

		switch (wrd.byte[0]) {
		case 0xBE:
			if (evt.opened) {
				err = NO_FE_BEFORE_BE;
				continue;
			}
			evt = (event){0}; //reuse event
			evt.opened = true;
			evt.wpos = wpos;


			evt.ts = wrd.data; //save timestamp low
			
			if (!sp.opened){
				//start spill
				sp.opened = true;
			}

			break;

		case 0xFE:
			if (!evt.opened) {
				err = NO_BE_BEFORE_FE;
				continue;
			}
			evt.ts += wrd.data << 16; //save timestamp high

			if(!sp.evt_count) { //a first event
				sp.ts_first = evt.ts;
			}
			sp.evt_count++;
			evt.ts -= sp.ts_first; //make the timestamp relative to a spill

			evt.wlen_1f = backlog[prev].data;
			evt.opened=false;
			
			if (evt.wlen!= evt.wlen_1f) {
				err = WRONG_LEN_1F;
				fprintf(stderr, "len_1f:%d != len:%d\n", evt.wlen_1f, evt.wlen);
				continue;
			}

			if (args.isDump){
				printf("%9d\t%4d\n", evt.ts, evt.wlen);
			}
			break;

		case 0x1F:
		default:
			if(evt.opened) {
				evt.wlen++;	
			}
			break;
		}
		
 		wpos++;
	}
	

	///print error
	if (!err) {
		if(args.isEvents){
			printf("%s%d\n", args.isVerbose ? "events: ": "", sp.evt_count);
		}

		if(args.isVerbose){
			printf("ok\n");
		}
		return 0;
	}
	else {
		fprintf(stderr, "wPos: %d Word: 0x%04x %04x  Err: %s \n",
				wpos,
				backlog[cur].addr,
				backlog[cur].data,
				err_str[err]
				);
		printf("err\n");
		return 1;
	}
}

