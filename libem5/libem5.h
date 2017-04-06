#ifndef LIBEM5_H
#define LIBEM5_H

#ifdef __cplusplus /*  Could be called from C and C++. */
extern "C" {
#endif

struct em5dev {
	//~ int (*apwr)(ulong);
	//~ int (*aprd)(ulong); /* puts to fifo */
	int (*pchi)(void);
	//~ int (*pchn)(void);
	//~ int (*reset)(void);  /* euromiss reset */
	//~ int (*tst_set)(int); /* 0 - off, 1 - on */
	//~ int (*tst_pulse)(ulong);  /* pulse length usec */
	//~ int (*readout_start)(void);
	//~ int (*readout_stop)(void);
	//~ int (*readout_flush)(void);
	//~ int (*begin_spill)(void);
	//~ int (*end_spill)(void);
	int (*test)(void);
	
	int fd; /* device file */	
};

struct em5dev * libem5_init(const char * filename);
void libem5_free(void);

#ifdef  __cplusplus
}
#endif

#endif /*LIBEM5_H*/
