#ifndef LIBEM5_H
#define LIBEM5_H

#ifdef __cplusplus /*  Could be called from C and C++. */
extern "C" {
#endif

struct em5ctl {
	//~ int (*apwr)(ulong);
	//~ int (*aprd)(ulong); /* puts to fifo */
	//~ int (*pchi)(void);
	//~ int (*pchn)(void);
	//~ int (*reset)(void);  /* euromiss reset */
	//~ int (*readout_start)(void);
	//~ int (*readout_stop)(void);
	//~ int (*readout_flush)(void);
	int (*test)(void);
	
	int fd; /* device file */	
};

struct em5ctl * libem5_init(const char * filename);
void libem5_free(void);

#ifdef  __cplusplus
}
#endif

#endif /*LIBEM5_H*/
