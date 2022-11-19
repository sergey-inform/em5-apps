/*
 * Pedestal values readout for EM-6 ADC module.
 *
 * Clear pedestal values, read new peds multiple times,
 * write back average value for each channel.
 */

// TODO: median instead of average

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
#include <sys/time.h>

#define XILBUS 0x0C000000L   // start addr. of Xilinx)
#define SYSPERIF 0x40000000L // reg.addr. of system periferals (AIC,PIO,...)

volatile char *ioaddr;

#define REG_DATA (volatile long *)(ioaddr + 0)
#define REG_PCHI (volatile long *)(ioaddr + 4)
#define REG_PCHN (volatile long *)(ioaddr + 8)
#define REG_STAT (volatile long *)(ioaddr + 0x10) // EM5 status register
#define REG_XXXX (volatile long *)(ioaddr + 0x18)
#define REG_APW (volatile long *)(ioaddr + 0x20)

#define dbgprintf(format, ...) fprintf(stderr, format, ##__VA_ARGS__)

#define PEDBUF_LEN 1000
uint32_t pedbuf[PEDBUF_LEN];

uint32_t *pedbuf_ptr = pedbuf;
unsigned pedlen = 0;

volatile int end_spill = 0, spill_state = 0, ped_gen = 1;

// command line options
static struct Conf
{
    int threshold;
    int number;
    bool raw_output;
} cfg = {
    .threshold = 0,
    .number = 50,
    .raw_output = false};

int clear_peds(void);
int pxabus_apw(int32_t data);

void pchngen(void)
/** generate pchn event */
{
    *REG_PCHN = 1; // generate pchn request (start PCHN)
    usleep(100);
}

void pedgen(void)
/** generate event */
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
}

#define STAT_ERR 0b01000 // MISS error
#define STAT_AP 0b10000  // MISS FSM busy (ap_i)

int pxabus_apw(int32_t data)
{
    unsigned to = 1000;
    int32_t stat;

    *REG_APW = data; // AP write

    /// wait for not busy?
    do
    {
        stat = *REG_STAT;
        if (!(stat & STAT_AP))
            break;
    } while (--to);

    if (stat & STAT_ERR || !to)
    {
        fprintf(
            stderr,
            "APW MISS Error or timeout, reset required! data: 0x%x stat:0x%x to:%u\n",
            data, stat, to);
        return -EIO;
    }

    //~ fprintf(stderr, "apw data: x%x stat:0x%x to:%u\n", data, stat, to );
    return 0;
}

ssize_t get_data(uint32_t *data, unsigned max)
/** event data readout */
{
    uint32_t stat;
    uint32_t tmp;

    uint32_t *pdata = data;
    uint32_t *pmax = pdata + max;

    while (1)
    {
        stat = *REG_STAT;

        if (stat & 0x8)
        { // miss error
            fprintf(stderr, "MISS Error, reset required! stat: 0x%x\n", stat);
            return -EIO;
        }

        if (!(stat & 1))
        {                    // data in fifo
            tmp = *REG_DATA; // read it
            if ((tmp & 0x1F) < 20)
            { // not a service word
                if (pdata < pmax)
                    *pdata++ = tmp;
            }
            //~fprintf(stderr, "get_data %x %x\n", tmp, stat);
        }
        else
        {
            break; // no more data in fifo
        }
    }

    return (pdata - data);
}

void out_peds()
{
    unsigned i;
    uint32_t data;
    int mod, chan, val;

    if (cfg.raw_output == false ||
        isatty(fileno(stdout)))
    { // output is a terminal
        for (i = 0; i < pedlen; i++)
        {

            data = pedbuf[i];

            mod = data & 0x1f;
            chan = (data >> 5) & 0x7ff;
            val = data >> 16;

            printf("%2d %2d %4d\n", mod, chan, val);
        }
    }
    else
    {
        fwrite(pedbuf, sizeof(*pedbuf), pedlen, stdout);
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

    for (i = 0; i < modcount; i++)
    {
        data = pedbuf[i];
        m = data & 0x1f;
        // printf("i %d mod %d\n",i, m);

        if (m > 18) // not more than 18 modules in crate
            break;

        for (j = 0; j < 48; j++)
        { // channels
            data = (j << 5) + m;
            if (pxabus_apw(data) < 0)
                return -1;
        }
    }

    return 0;
}

/**
 * Returns the current time in microseconds.
 */
long getMicrotime()
{
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

static void read_peds(int n)
{
    int i, j;
    uint32_t data;
    uint16_t *pedmem;
    long timestamps[n];
    unsigned mod, chan, val, val_avg;
    unsigned cnt;

#define N_MOD 15
#define CHAN_PER_MOD 48

    pedmem = calloc(sizeof(uint16_t),
                    N_MOD * CHAN_PER_MOD * n /* channels * modules * runs */);
    if (pedmem == NULL)
    {
        perror("malloc error");
        exit(1);
    }

    dbgprintf("Cleaning up the FIFO... ");
    if (get_data(pedbuf, PEDBUF_LEN) < 0)
        exit(-1); // miss error
    dbgprintf("ok\n");

    dbgprintf("Clear pedestal memory... ");
    if (clear_peds())
        exit(-1);
    dbgprintf("ok\n");

    dbgprintf("Ped event generation (%d times)... ", n);
    for (i = 0; i < n; i++)
    {
        pedgen();
        timestamps[i] = getMicrotime();
        usleep(100);
        pedlen = get_data(pedbuf, PEDBUF_LEN);

        for (j = 0; j < (int)pedlen; j++)
        {
            data = pedbuf[j];
            mod = data & 0x1f;
            chan = (data >> 5) & 0x7ff;
            val = data >> 16;

            if (mod < N_MOD && chan < CHAN_PER_MOD)
            {
                pedmem[(mod * CHAN_PER_MOD + chan) * n + i] = val;
            }
        }
    }
    dbgprintf("ok\n");

    dbgprintf("Write average peds...");
    /// Write pedestal values back to ADC's pedestal registers
    for (j = 0; j < (int)pedlen; j++)
    {
        data = pedbuf[j];
        mod = data & 0x1f;
        chan = (data >> 5) & 0x7ff;

        // calculate average
        val = 0;
        cnt = 0;
        for (i = 0; i < n; i++)
        {
            if (pedmem[(mod * CHAN_PER_MOD + chan) * n + i])
            {
                val += pedmem[(mod * CHAN_PER_MOD + chan) * n + i];
                cnt += 1;
            }
        }
        if (cnt > 0)
        {
            val_avg = val / cnt;
        }
        else
        {
            val_avg = 0;
        }
        data = (data & 0xffff) + (val_avg << 16);
        pxabus_apw(data + (cfg.threshold << 16)); /// write pedestals + thresholds
        pedbuf[j] = data;
    }
    dbgprintf("ok\n");

    out_peds();

    if (cfg.raw_output == false)
    {
        // out pedmem
        printf("# raw ");
        for (j = 0; j < (int)pedlen; j++)
        {
            data = pedbuf[j];
            mod = data & 0x1f;
            chan = (data >> 5) & 0x7ff;
            printf("\n%02d %02d : ", mod, chan);
            for (i = 0; i < n; i++)
            {
                val = pedmem[(mod * CHAN_PER_MOD + chan) * n + i];
                printf("%d ", val);
            }
        }

        printf("\n# timestamps \n");
        printf("%lu ", timestamps[0]);
        for (i = 1; i < n; i++)
        {
            printf("+%lu ", timestamps[i] - timestamps[0]);
        };
    }
}

void parse_options(int argc, char **argv)
{
    int opt;
    opterr = 1; /// 1 is to print error messages

    while ((opt = getopt(argc, argv, "hxn:t:")) != -1)
    {
        switch (opt)
        {
        case 't':
            cfg.threshold = atoi(optarg);
            fprintf(stderr, "Pedestal threshold: %d\n", cfg.threshold);
            break;

        case 'x':
            cfg.raw_output = true;
            break;

        case 'n':
            cfg.number = atoi(optarg);
            fprintf(stderr, "Nuber of iterations: %d\n", cfg.number);
            break;

        case 'h':
            fprintf(stderr, "USAGE: %s [-x] [-t threshold] [-n number] \n ", argv[0]);
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

    // Map Euromiss controller memory.
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

    read_peds(cfg.number);

    return 0;
}
