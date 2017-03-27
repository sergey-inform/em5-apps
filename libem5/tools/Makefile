INCDIRS = -I./include
CFLAGS = -g -O -Wall -W ${INCDIRS}
LDFLAGS = -Wl,-rpath,. # add current dir to library search path
LDLIBS = libem5.so

INSTALLDIR = /home/user/nfsroot/root/

SRCS=$(wildcard *.c)
OBJS = $(SRCS:.c=.o)
TARGET=em5toolbox

all: note clean $(TARGET)
#~ all: clean arm

$(TARGET): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) $(LDFLAGS) $(LDLIBS) 

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

arm: ARCH = arm
arm: CC=/home/user/toolchain/arm-linux-gcc
arm: clean $(TARGET)

note:
	@echo '###############################################'
	@echo '# run `make arm` to build executables for EM5 #'
	@echo '###############################################'

clean:
	rm -f *.o $(TARGET)

install: $(TARGET)
	cp $(TARGET) $(INSTALLDIR)
	
