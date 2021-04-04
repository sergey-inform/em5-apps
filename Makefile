INCDIRS = -I./include
CFLAGS = -g -O3 -Wall -W ${INCDIRS}
LDFLAGS = -Wl,-rpath,. # add current dir to library search path

INSTALLDIR = /home/user/nfsroot/root/

SRCS=$(wildcard *.c)
PROGS=$(SRCS:.c=)


all: note clean $(PROGS)

%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS) $(LDLIBS) 

arm: ARCH = arm
arm: CC = /home/user/toolchain/arm-linux-gcc
arm: clean $(PROGS)

clean:
	rm -f *.o $(PROGS)

note:
	@echo '####################################'
	@echo '# run `make arm` to build for em-5 #'
	@echo '####################################'

install: $(PROGS)
	cp $(PROGS) $(INSTALLDIR)
