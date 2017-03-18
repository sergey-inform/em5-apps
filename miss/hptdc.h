/**
 * CERN HPTDC chip data format.
 */
#ifndef HPTDC_H
#define HPTDC_H

#include <inttypes.h>
#include <stdbool.h>

enum _HPTDC_TYPE_EVENT {
	GRP_HDR,
	GRP_TRL,
	TDC_HDR,
	TDC_TRL,
	LEADING,  //==COMBINED
	TRAILING,
	ERR_FLAG,
	DEBUG,
	};
	
enum _HPTDC_SUBTYPE_DEBUG {
	BUNCH_ID,
	L1_OCCUP,
	FIFO_OCCUP,
};

typedef uint32_t HPTDC_WORD;

typedef struct {
	enum _HPTDC_TYPE_EVENT type :4; 
	unsigned char tdc :4;
	union {
		struct {
			unsigned short eventid :12;
			union {
				unsigned short bunchid;  ///:12  GRP_HDR, TDC_HDR
				unsigned short wordcount;  ///:12  GRP_TRL, TDC_TRL
			};
		};
		struct {
			unsigned char chan :5;
			union {
				unsigned long leading;  ///:19  LEADING
				unsigned long trailing;  ///:19  TRAILING
				struct {  ///COMBINED
					unsigned short pair_width :7;	
					unsigned short pair_leading :12;
				};
			};
		};
		struct {  ///ERR_FLAG
			unsigned errflags :15;
		};
		struct {  ///DEBUG
			enum _HPTDC_SUBTYPE_DEBUG debug_subtype :4;
			union {
				struct {
					unsigned short debug_bunchid :12;
				};
				struct {
					unsigned char gr :2;
					unsigned char l1_occupancy :8;
				};
				struct {
					unsigned char trig_fifo :4;
					bool f :1;
					unsigned char readout_fifo :8;
				};
			};
		};
	};
} HPTDC_EVENT;

HPTDC_EVENT hptdc_unpack(HPTDC_WORD);

#endif /* HPTDC_H */
