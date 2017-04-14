#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>   // usleep

#include <getopt.h> //getopt


#define XILBUS   0x0C000000L	// start addr. of Xilinx)
#define SYSPERIF 0x40000000L	// reg.addr. of system periferals (AIC,PIO,...)

char *ioaddr, *sysaddr;

#define DEVICE_FILE "/dev/em5"

#define dbgprintf(format, ...)   fprintf(stderr, format, ##__VA_ARGS__)

#define PEDBUF_LEN 1000
uint32_t pedbuf[PEDBUF_LEN];

uint32_t *pedbuf_ptr = pedbuf;
unsigned  pedlen = 0;

volatile int end_spill = 0, spill_state = 0, ped_gen = 1;

static struct Conf { // command line options 
	int threshold;
	bool raw_output; 
} cfg = {0};


int clear_peds(void);
int pxabus_apw(int32_t data);

void pchngen(void)
/** generate pchn event */
{
	*(long *) (ioaddr + 8) = 1;	// generate pchn request (start PCHN)
	usleep(100);
}

void pedgen(void)
/** generate event */
{
	pxabus_apw((1 << 16) + (2044 << 5) + 20);	// set T1 output to 1
	pxabus_apw((2044 << 5) + 20);	// set T1 output to 0
	usleep(10);			// wait for ADC end of conversion
	*(long *) (ioaddr + 4) = 1;	// generate software trigger (start PCHI) 
	usleep(100);
}


#define EM5_APW   0x20
#define EM5_STAT  0x10			// EM5 status register

#define STAT_ERR  0b01000		// MISS error
#define STAT_AP   0b10000		// MISS FSM busy (ap_i)

int pxabus_apw(int32_t data)
{
	unsigned to = 1000;
	int32_t stat;

	*(long *) (ioaddr + EM5_APW) = data;	// AP write
	
	/// wait for not busy?
	do {
		stat = *(volatile long *) (ioaddr + EM5_STAT);
		if (!(stat & STAT_AP))
			break;
	} while (--to);
	
	if (stat & STAT_ERR || !to) {
		fprintf(stderr, "APW MISS Error or timeout, OS required, data: 0x%x stat:0x%x to:%u\n",
			   data, stat, to);
		return -EIO;
	}
	
	//~ fprintf(stderr, "apw data: x%x stat:0x%x to:%u\n", data, stat, to );
	return 0;
}

ssize_t get_data(uint32_t * data, unsigned max)
/** event data readout */
{
	uint32_t stat;
	uint32_t tmp;
	
	uint32_t * pdata = data;
	uint32_t * pmax = pdata + max;
	
	while (1)  
	{
		stat = *(volatile long *) (ioaddr + EM5_STAT);
		
		if (stat & 0x8) { // miss error
			fprintf(stderr,"MISS Error, OS required stat 0x%x\n", stat);
			return -EIO;	
		}
		
		if (!(stat & 1)) { // data in fifo
			tmp = *(volatile unsigned long *) ioaddr;	//read it
			if ( (tmp & 0x1F) < 20 ) { // not a service word
				if (pdata < pmax)
					*pdata++ = tmp;
			}
			
			//~ fprintf(stderr, "get_data %x %x\n", tmp, stat);
		}
		else {
			break;	// no more data in fifo
		}
	}
	
	return (pdata - data);
}

void out_peds()
{
	int i;
	uint32_t data; 
	int mod, chan, val;
	
	if ( cfg.raw_output == false || isatty(fileno(stdout))) {
		// output is a terminal
		for (i=0; i<pedlen; i++) {
			
			data = pedbuf[i];
			
			mod = data & 0x1f;
			chan = (data >> 5) & 0x7ff;
			val = data >> 16;
			
			printf("%2d %2d %4d\n", mod, chan, val);
		}
	}
	else {
		fwrite(pedbuf, sizeof(* pedbuf), pedlen, stdout);
	}
	
}


int clear_peds(void)
/** clear pedestal memory of ADCs */
{
	int i, j;
	int32_t data = 0;
	int modcount;
	int m;
	
	// get module numbers
	pchngen();
	modcount = get_data(pedbuf, PEDBUF_LEN);
	
	for (i = 0; i < modcount; i++) {
		data = pedbuf[i];
		m = data & 0xff;
		//~ printf("i %d mod %d\n",i, m);
		
		if (m > 18)  // not more than 18 modules in crate
			break;
		
		for (j = 0; j < 48; j++) {	//channels
			data = (j << 5) + m;
			if (pxabus_apw(data) < 0)
				return -1;
		}
	}
		
	return 0;
}


static void read_peds()
{
	int i = 0;
	uint32_t data; 

	dbgprintf("Cleaned up the FIFO...");
	if (get_data(pedbuf, PEDBUF_LEN) < 0)
		exit(-1); // miss error
	dbgprintf("ok\n");
	
	dbgprintf("Clear pedestal memory...");
	if (clear_peds())
		exit(-1);
	dbgprintf("ok\n");
	
	
	dbgprintf("Ped event generation...");
	pedgen();
	pedlen = get_data(pedbuf, PEDBUF_LEN);
	out_peds();
	dbgprintf("ok\n");
	
	dbgprintf("Write peds...");
	/// Write pedestal values back to ADC's pedestal registers
	for (i = 0; i < (int)pedlen; i++) {
		data = pedbuf[i];
		pxabus_apw( data + (cfg.threshold << 16) ); /// write pedestals + thresholds
	}
	dbgprintf("ok\n");
		
	
}



void parse_options(int argc, char **argv) {
	int opt;
	opterr = 1;		/// 1 is to print error messages

	while ((opt = getopt(argc, argv, "hxt:")) != -1) {
		switch (opt) 
		{
		case 't':
			cfg.threshold = atoi(optarg);
            fprintf(stderr, "Pedestal threshold: %d\n", cfg.threshold);
            break;
           
        case 'x':
			cfg.raw_output = true;
			break;
			
		case 'h':
			fprintf(stderr, "USAGE: %s [-x] [-t threshold]\n ",
					argv[0]);
			exit(0);
		
		case '?':
		default:
			fprintf(stderr,"?? getopt returned character code 0%o ??\n", opt);
			exit(EXIT_FAILURE);
		}
	}
}

int main(int argc, char **argv)
{
	int memfd;
	parse_options(argc, argv);

	memfd = open("/dev/mem", O_RDWR | O_SYNC);
	if (memfd < 0) {
		perror("open /dev/mem");
		exit(1);
	}

	sysaddr = mmap(0, 0x10000000, PROT_READ | PROT_WRITE, MAP_SHARED, memfd,
			 SYSPERIF);
	
	if (sysaddr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	//~ jtaggpio_init(sysaddr);
	//~ xilbus_init(sysaddr);

	ioaddr = mmap(0, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, memfd,
			 XILBUS);
	if (ioaddr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	read_peds();
	
	return 0;
}
