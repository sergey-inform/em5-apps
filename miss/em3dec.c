/** 
 * A primitive tool to decode raw EM3 TDC data stream.
 * 
 * Reads raw input stream.
 * Prints output values in text format.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>

#include "em3.h"
#include "emiss.h"

#define PURPOSE "Decode raw em3 data and print the values."

const char * USAGE = "[<filename>] [-v] [-h]\n";
const char * ARGS = "\n"
	"  <filename>:  input file, stdin by default; \n"
	"  -v   be verbose; \n";
const char * CONTRIB = "\nWritten by Sergey Ryzhikov, \n sergey.ryzhikov@ihep.ru, 04.2017.\n";

struct conf {
	char * filename;
	bool v;  // be verbose
	} cfg = {
		.filename = "-",  // stdin by default
		.v = false,  
	};

int parse_opts(int argc, char ** argv, struct conf *);
void print_opts(struct conf*);


off_t em3dec(FILE * fin, FILE *fout)
{
	HPTDC_WORD hptdc_word = 0;
	HPTDC_EVENT hptdc_evt;
	EMWORD em_word;
	off_t wordcount = 0;
	unsigned event_nwords = 0;
	unsigned mod_num;
	
	if (fin == NULL || fout == NULL)
		return 0;
		
	while (1 == fread(&em_word, sizeof(EMWORD), 1, fin))
	{
		wordcount++;
		
		//~ fprintf(fout, "%08X wrong tdc word\n", hptdc_word);
		switch (em_word.hdr)
		{
		case 0xBE:  ///begin event
			event_nwords = 0;  // reset counter
			break;
			
		case 0xDE:  ///PCHN
		case 0xFE:  ///finish event
		case 0x1F:  ///em5 event flags? (appears before FE)
			//ignore
			break;
			
		default:  ///data word
			if (em_word.hdr > 0x1F)  // wrong header
				break;
			
			event_nwords++;
			mod_num = em_word.hdr;
			
			if (event_nwords & 1) {  // odd
				hptdc_word = em_word.data;
			}
			else {  // even
				hptdc_word |= em_word.data << 16;
				//~ fprintf(fout, "%0*lX %08X >> %02u" //nhex, wi, raw
				
				if (hptdc_unpack(hptdc_word, &hptdc_evt)) {
					fprintf(fout, "%08X wrong tdc word\n", hptdc_word);
				}
				else {
					if (hptdc_evt.type != LEADING)
						continue;
						
					fprintf(fout, "%02d %d %02d %lu \n",
						mod_num,
						hptdc_evt.tdc,
						hptdc_evt.chan,
						hptdc_evt.leading
						);
							//~ " %s \n",
							//~ hptdc_word,
							//~ em_word.hdr,
							//~ prbuf
							//~ );
				}
			}
		};
		
	}
	
	return wordcount;
}

int main( int argc, char ** argv)
{
	FILE *fptr;
	off_t cnt;
	
	if (parse_opts(argc, argv, &cfg))
		exit(EXIT_FAILURE);
		
	//~ print_opts(&cfg);
	
	if(!strcmp(cfg.filename,"-")) {
		fptr = stdin;
		if (isatty(0)) {
			//stdin is a terminal
			fprintf(stderr, "Err: trying to read raw data stream from a terminal.\n");
			exit(EXIT_FAILURE);
		}
	}
	else {
		fptr = fopen(cfg.filename, "rbm");
		if (fptr == NULL) {
			fprintf(stderr, "Err open %d: %s\n", errno, strerror(errno));
			exit(EXIT_FAILURE);
		}
		
	}
	
	cnt = em3dec(fptr, stdout);
	
	if(cfg.v)
		fprintf(stderr, "%jd words decoded\n", (intmax_t) cnt);
	
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
	
	
	while ((opt = getopt(argc, argv, "vh")) != -1) {
		switch (opt)
		{
		case 'v': 
			cfg->v = true;
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
		fprintf(stderr, "\nErr: expecting only one non-optional argument, "
			"%d given!\n",
			argc-optind
			);
		fprintf(stderr, "Usage: %s %s", argv[0], USAGE);
		exit(EXIT_FAILURE);
	}
	else if (argc-optind == 1) {
		cfg->filename = argv[optind];
	}
		
	return 0;
}

void print_opts(struct conf * cfg)
/** Print program configuration options. */
{
	fprintf(stderr,
		"config: \n"
		"file: %s \n"
		"verbose: %s \n"
		"\n",
		cfg->filename,
		cfg->v ? "yes" : "no"
		);
	return;
}

