#ARCH = arm
INSTALLDIR = /home/user/nfsroot/usr/lib/
#CC = /home/user/toolchain/arm-linux-gcc

MAJOR = 1
MINOR = 0
REVISION = 0
LIB = libem5.so
LIB_STATIC = libem5.a

#AR = /home/user/toolchain/arm-linux-ar
ARFLAGS = rcs
DEBUG = -g
OPTIM = -O3
CFLAGS += $(DEBUG) $(OPTIM) -Wall -std=c99 -pedantic  -fpic
#~ CFLAGS += -Wformat-security -Wno-format-zero-length -Werror -Wwrite-strings -Wformat -fdiagnostics-show-option -Wextra -Wsign-compare -Wcast-align -Wno-unused-parameter
LDFLAGS += -shared
LIBPATH += -L.

LDFLAGS += -Wl,-soname,$(LIB).$(MAJOR).$(MINOR)
LIBS = -lcrypt

all: $(LIB) $(LIB_STATIC)

$(LIB): libem5.o
	$(CC) -o $(LIB).$(MAJOR).$(MINOR).$(REVISION) $^ $(LDFLAGS) $(LIBS)
	ln -sf $(LIB).$(MAJOR).$(MINOR).$(REVISION) $(LIB).$(MAJOR).$(MINOR)
	ln -sf $(LIB).$(MAJOR).$(MINOR) $(LIB)

$(LIB_STATIC): libem5.o
	$(AR) $(ARFLAGS) $@ $^

install: $(LIB)
	cp $(LIB)* $(INSTALLDIR)
	
clean:
	rm -f *.o *.so.* *.a
