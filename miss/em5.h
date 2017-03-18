#ifndef EM5_H
#define EM5_H

#include <inttypes.h>

typedef union {
	uint32_t word; 
	struct {
		uint8_t hdr;
		uint8_t :8 ;  //zeroes
		union {
			uint16_t data;
			uint16_t timemark;
		};
	};
} EMWORD;	/* EuroMISS word: addr+data */


#endif /* EM5_H*/
