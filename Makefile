ARCH = arm
CC = /home/user/toolchain/arm-linux-gcc

INCDIRS = -I./include
CFLAGS = -g -O -Wall -W ${INCDIRS}
#LDLIBS = libem5_raw.so.1
LDFLAGS = -Wl,-rpath,. # add current dir to library search path

INSTALLDIR = /home/user/nfsroot/root/

SRCS=$(wildcard *.c)
PROGS=$(SRCS:.c=)

all: $(PROGS)

%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS) $(LDLIBS) 

clean:
	rm -f *.o $(PROGS)

install: $(PROGS)
	cp $(PROGS) $(INSTALLDIR)
