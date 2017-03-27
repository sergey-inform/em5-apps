/**
	Library for em5 device.
	Provides MISS interface for em5 apps.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <fcntl.h>		/* open */
#include <unistd.h>		/* getopt, exit */
#include <sys/ioctl.h>		/* ioctl */

#include "em5.h"
#include "libem5.h"

#define MYNAME "libem5"
#define DEFAULT_FILENAME "/dev/em5"

#define PERROR(format, ...) fprintf(stderr, MYNAME ": " format "\n", ##__VA_ARGS__)

//~ static int readout_start(void);
//~ static int readout_stop(void);
//~ static int readout_flush(void);
static int test(void);

static struct em5ctl ctl = { 
	.test	= test,
};

static int exec_ioc( int ionum, void * data)
{
	int ret, err;

	ret = ioctl( ctl.fd, ionum, data );
	err = errno;
	if (ret  == 0) {
		return 0;
	}
	else {
		fprintf(stderr,"ioctl failed and returned errno %s \n",strerror(errno));
		return (err);
	}
	
}

static int test (void)
{
	return exec_ioc( EM5_IOC_TEST, NULL);
}



struct em5ctl * libem5_init( const char * filename)
{
	if (filename == NULL) {
		filename = DEFAULT_FILENAME;
	}
	
	ctl.fd = open( filename, O_NONBLOCK);
	if (ctl.fd < 0) {
		PERROR( "Couldn't open file %s; %s",
			       	filename, strerror(errno) );
		return NULL;
	}

	return &ctl;
}


void libem5_free(void)
{
	if (ctl.fd) {
		close(ctl.fd);
	}
	
	return;
}
