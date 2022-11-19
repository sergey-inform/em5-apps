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

#define PURPOSE "Mask all EM-6 ADC channels but specified.\n" \
                "Useful for fast calibration. "

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

#define NMOD 15
#define NCHAN 48

#define CHANBUF_LEN (NCHAN * NMOD)
uint32_t chanbuf[CHANBUF_LEN];
uint32_t *chanbuf_ptr = chanbuf;

#define CHANBUF(mod, chan) (chanbuf[mod * NCHAN + chan])

unsigned maskcount = 0;

int clear_peds(void);
int pxabus_apw(int32_t data);

#define STAT_ERR 0b01000 // MISS error
#define STAT_AP 0b10000  // MISS FSM busy (ap_i)

#define EM_CHAN(x) (((x)&0xFE0) >> 5)
#define EM_MOD(x) ((x)&0x1F)
#define EM_DATA(x) ((x) >> 16)

#define EM_WORD(mod, chan, data) (((data) << 16) + (((chan)&0x7ff) << 5) + ((mod)&0x1F))

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
        // fprintf(
        //     stderr,
        //     "APW MISS Error or timeout, reset required! data: 0x%x stat:0x%x to:%u\n",
        //     data, stat, to);
        return -EIO;
    }

    //~ fprintf(stderr, "apw data: x%x stat:0x%x to:%u\n", data, stat, to );
    return 0;
}

void mask_channels()
{
    uint32_t word;
    int i;
    for (i = 0; i < CHANBUF_LEN; i++)
    {
        if (EM_DATA(chanbuf[i]))
        {
            pxabus_apw(chanbuf[i]);
        }
    }
}

void fill_chanbuf()
{
    int c, m;
    unsigned mask = 0x1FFF;

    for (m = 0; m < NMOD; m++)
    {
        for (c = 0; c < NCHAN; c++)
        {
            CHANBUF(m, c) = EM_WORD(m, c, mask);
        }
    }
}

int read_input()
{
    int res;
    unsigned chan; // MISS channel
    unsigned mod;  // MISS module

    while (!feof(stdin))
    {
        res = scanf("%u %u", &mod, &chan);
        if (res == 2)
        {
            if (mod > NMOD | chan > NCHAN)
            {
                return -1;
            }
            CHANBUF(mod, chan) = EM_WORD(mod, chan, 0x0); // unmask
            maskcount += 1;
        }
        else if (errno != 0)
        {
            perror("scanf");
            return errno;
        }
        else
        {
            if (maskcount == 0)
            {
                fprintf(stderr, "No valid input in stdin. \n");
                return 1;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    int memfd;

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

    fill_chanbuf();

    if (!read_input())
    {
        mask_channels();
    }
    else
    {
        return 1;
    }

    return 0;
}
