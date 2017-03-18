#include "hptdc.h"

HPTDC_EVENT hptdc_unpack(HPTDC_WORD w)
{
	unsigned data = w & 0x00FFffFF; 
	HPTDC_EVENT e;
	
	e.type = (w & 0xF0000000) >> 28;
	e.tdc  = (w & 0x0F000000) >> 24;
	
	switch(e.type)
	{
	case GRP_HDR:
	case TDC_HDR:
	case GRP_TRL:
	case TDC_TRL:
		e.eventid = (data & 0xFFF000) >>12;
		e.bunchid = data & 0xFFF;  //same as wordcount
		break;
	
	case LEADING:  //==COMBINED
	case TRAILING:
		e.chan = (data & 0xF80000) >>19;
		e.leading = data & 0x7FFFF; //same as trailings
		break;
		
	case ERR_FLAG:
		e.errflags = data & 0x7FFF;
		break;
		
	case DEBUG:
		e.debug_subtype = (data & 0xF00000) >> 20;
		switch(e.debug_subtype)
		{
			case BUNCH_ID:
				e.debug_bunchid = data & 0xFFF;
				break;
			case L1_OCCUP:
				e.gr = (data & 0x300) >> 8;
				e.l1_occupancy = (data & 0xFF);
			case FIFO_OCCUP:
				e.trig_fifo = (data & 0x1E00) >> 8;
				e.f = (bool)(data & 0x100);
				e.readout_fifo = (data & 0xFF);
		}
		break;
	}
	return e;
}

void snprint_hptdc_word(char * buf, int len, HPTDC_WORD w)
/**
 * Print given HPTDC_WORD to the buffer in human-readable format.
 */
{
	HPTDC_EVENT e = hptdc_unpack(w);
	
	switch(e.type)
	{
	
#define pr(format,...) snprintf(buf, len, "%u " format, e.tdc, __VA_ARGS__)	
	
	case GRP_HDR:	pr("-- HEAD_MASTER evt: %4u \tbunch: %u", e.eventid, e.bunchid); break;
	case TDC_HDR:	pr("-- HEAD_SLAVE evt: %4u \tbunch: %u", e.eventid, e.bunchid); break;
	case GRP_TRL:	pr("-- END_MASTER evt: %4u \tcnt: %u", e.eventid, e.wordcount); break;
	case TDC_TRL:	pr("-- END_SLAVE evt: %4u \tcnt: %u", e.eventid, e.wordcount); break;
	case LEADING:	pr("%02u LEAD  ts: %lu", e.chan, e.leading); break;
	case TRAILING:	pr("%02u TRAIL ts: %lu", e.chan, e.trailing); break;
	  //==COMBINED
	case ERR_FLAG:  pr("ERROR flags: %05X", e.errflags); break;
	case DEBUG:
		switch(e.debug_subtype)
		{
			case BUNCH_ID: 
					pr("DEBUG bunch: %u", e.debug_bunchid); break;
			case L1_OCCUP:
					pr("DEBUG grp: %u l1_occup: %u", e.gr, e.l1_occupancy); break;
			case FIFO_OCCUP:
					pr("DEBUG trig_fifo: %u readout_fifo: %u %s", e.trig_fifo, e.readout_fifo, (e.f)?"FULL":""); break;
		}
		break;
	default:
			pr("UNKNOWN_TYPE 0x%X, missing emword?", e.type );
	}
}
