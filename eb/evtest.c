/* 
 * Parse em-5 events, check event file syntax and pring some statistics.
 */
#include <argp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> //malloc()
#include <assert.h>
#include <inttypes.h> //to be shure we are using 32-bit words

const char *argp_program_version = "evstat 1.0";
const char *argp_program_bug_address = "<sergey-inform@ya.ru>";
static char doc[] = "Parse EM-5 events, check event file syntax and pring some statistics.";
static char args_doc[] = "[FILENAME]";
static struct argp_option options[] = { 
	{ "quiet", 'q', 0, 0, "Print only ok/err."},
	{ "stats", 's', 0, 0, "Print some termpral statistics."},
	{ "verbose", 'v', 0, 0, "Dump all events."},
	{ 0 } 
};

struct arguments {
	bool isStats, isQuiet, isVerbose;
	char *infile;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;
	switch (key) {
		case 'q': arguments->isQuiet = true; break;
		case 's': arguments->isStats = true; break;
		case 'v': arguments->isVerbose = true; break;
		case ARGP_KEY_ARG: 
			if (state->arg_num >= 1){
				argp_usage(state);
			}
			arguments->infile = arg;
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
	struct arguments arguments = {0};
	FILE *instream;
	size_t bytes;
	enum err_num err;

	assert(sizeof(emword) == 4); // in case of troubles on 64-bit platforms use -m32 compiler option

	argp_parse(&argp, argc, argv, 0, 0, &arguments);

	if (arguments.infile) {
		instream = fopen (arguments.infile, "r");
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

	while ((!err && (bytes = fread( &backlog[cur], 1, sizeof(emword), instream) )))
	{
		emword wrd = backlog[cur];
//		printf("-%08X\n", wrd);
		
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
				fprintf(stderr, "len:%d len_1f:%d\n", evt.wlen, evt.wlen_1f);
				continue;
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
	
	if (bytes && bytes != 4/*size of word*/) {
		err = FILE_LEN_ODD;
		printf("bytes: %d\n",bytes);
	}

	///print error
	if (!err) {
		//if verbose
			printf("ok\n");
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

