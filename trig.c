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
#include <unistd.h> // usleep
#include <getopt.h> //getopt

#define PURPOSE "Send pulse to EM-5 output and generate PCHI."

#define XILBUS 0x0C000000L   // start addr. of Xilinx)
#define SYSPERIF 0x40000000L // reg.addr. of system periferals (AIC,PIO,...)

volatile char *ioaddr;

#define REG_DATA (volatile long *)(ioaddr + 0)
#define REG_PCHI (volatile long *)(ioaddr + 4)
#define REG_PCHN (volatile long *)(ioaddr + 8)
#define REG_STAT (volatile long *)(ioaddr + 0x10) // EM5 status register
#define REG_XXXX (volatile long *)(ioaddr + 0x18)
#define REG_APW (volatile long *)(ioaddr + 0x20)

const char * USAGE = "[-n count] [-h] \n";
const char * ARGS = "\n"
"  -n \t repeat N times. \n"
"  -h \t display help";
const char * CONTRIB = "\nWritten by Sergey Ryzhikov <sergey.ryzhikov@ihep.ru>, 03.2022.\n";

unsigned count = 1;

void event(int n)
/**
 * Generate event
 */
{
  int i;
  for (i = 0; i < n; i++)
  {
    //~ // gen pulse using test EM02 module
    //~ pxabus_apw((1 << 16) + (2044 << 5) + 20);	// set T1 output to 1
    //~ pxabus_apw((2044 << 5) + 20);	// set T1 output to 0

    // Gen pulse from test output of the EM5
    volatile long cmd_reg_save;
    cmd_reg_save = *REG_XXXX;

    *REG_XXXX = cmd_reg_save | 0x80; // generate test output pulse
    *REG_XXXX = cmd_reg_save;        // turn off test output pulse
    usleep(10);                      // wait for ADC end of conversion

    *REG_PCHI = 1; // generate software trigger (start PCHI)
    usleep(100);
  }
}

void parse_options(int argc, char **argv)
{
  int opt;
  opterr = 1; /// 1 is to print error messages

  while ((opt = getopt(argc, argv, "hn:")) != -1)
  {
    switch (opt)
    {
    case 'n': 
				count = atoi(optarg);
				break;

    case 'h': ///help
      fprintf(stderr, PURPOSE "\n""usage: \n%s %s %s %s",
          argv[0], USAGE, ARGS, CONTRIB);
      exit(0);

    case '?':
    default:
      fprintf(stderr, "?? getopt returned character code 0%o ??\n", opt);
      exit(EXIT_FAILURE);
    }
  }
}

int main(int argc, char **argv)
{
  int memfd;
  parse_options(argc, argv);

  memfd = open("/dev/mem", O_RDWR | O_SYNC);
  if (memfd < 0)
  {
    perror("open /dev/mem");
    exit(1);
  }

  ioaddr = mmap(0, 0x10000, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, XILBUS);

  if (ioaddr == MAP_FAILED)
  {
    perror("mmap");
    exit(1);
  }

  event(count);

  return 0;
}
