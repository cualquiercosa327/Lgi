//
//  GPixelRops.h
//  Lgi
//
//  Created by Matthew Allen on 1/03/14.
//  Copyright (c) 2014 Memecode. All rights reserved.
//

#ifndef _GPixelRops_h
#define _GPixelRops_h

#ifdef MAC
#define REG
#else
#define REG register
#endif


//////////////////////////////////////////////////////////////////////////
// 24 bit over
//////////////////////////////////////////////////////////////////////////
#define OverNpm24toNpm32(s,d,sa)	\
	{ \
		REG uint8 oma = 0xff - sa; \
		REG uint8 da = sa + DivLut[d->a * oma]; \
		d->r = ((s->r * sa) + (DivLut[d->r * da] * oma)) / da; \
		d->g = ((s->g * sa) + (DivLut[d->g * da] * oma)) / da; \
		d->b = ((s->b * sa) + (DivLut[d->b * da] * oma)) / da; \
		d->a = da; \
	}

//////////////////////////////////////////////////////////////////////////
// 32 bit over
//////////////////////////////////////////////////////////////////////////
#define OverNpm32toNpm24(s,d)	\
	{ \
		REG uint8 sa = (s)->a; \
		REG uint8 oma = 0xff - sa; \
		(d)->r = DivLut[((s)->r * sa) + ((d)->r * oma)]; \
		(d)->g = DivLut[((s)->g * sa) + ((d)->g * oma)]; \
		(d)->b = DivLut[((s)->b * sa) + ((d)->b * oma)]; \
	}
#define OverNpm32toNpm32(s,d)	\
	{ \
		REG uint8 sa = s->a; \
		REG uint8 oma = 0xff - sa; \
		REG uint8 da = sa + DivLut[d->a * oma]; \
		d->r = ((s->r * sa) + (DivLut[d->r * da] * oma)) / da; \
		d->g = ((s->g * sa) + (DivLut[d->g * da] * oma)) / da; \
		d->b = ((s->b * sa) + (DivLut[d->b * da] * oma)) / da; \
		d->a = da; \
	}
#define OverNpm32toNpm48(s,d)	\
	{ \
		REG uint16 sa = G8bitTo16bit((s)->a); \
		REG uint16 oma = 0xffff - sa; \
		(d)->r = ( (G8bitTo16bit((s)->r) * sa) + ((d)->r * oma) ) / 0xffff; \
		(d)->g = ( (G8bitTo16bit((s)->g) * sa) + ((d)->g * oma) ) / 0xffff; \
		(d)->b = ( (G8bitTo16bit((s)->b) * sa) + ((d)->b * oma) ) / 0xffff; \
	}
#define OverNpm32toNpm64(s,d)	\
	{ \
		REG uint16 sa = G8bitTo16bit((s)->a); \
		REG uint16 oma = 0xffff - sa; \
		REG uint16 da = sa + ((uint32)(d)->a * oma) / 0xffff; \
		d->r = ( (G8bitTo16bit(s->r) * sa) + ( ((uint32)(d->r * da) / 0xffff) * oma)) / da; \
		d->g = ( (G8bitTo16bit(s->g) * sa) + ( ((uint32)(d->g * da) / 0xffff) * oma)) / da; \
		d->b = ( (G8bitTo16bit(s->b) * sa) + ( ((uint32)(d->b * da) / 0xffff) * oma)) / da; \
		d->a = da; \
	}

#define OverPm32toPm24(s,d)	\
	{ \
		REG uint8 oma = 0xff - s->a; \
		d->r = s->r + DivLut[d->r * oma]; \
		d->g = s->g + DivLut[d->g * oma]; \
		d->b = s->b + DivLut[d->b * oma]; \
	}
#define OverPm32toPm32(s,d)	\
	{ \
		REG uint8 sa = s->a; \
		REG uint8 oma = 0xff - sa; \
		d->r = s->r + DivLut[d->r * oma]; \
		d->g = s->g + DivLut[d->g * oma]; \
		d->b = s->b + DivLut[d->b * oma]; \
		d->a = sa + DivLut[d->a * oma]; \
	}

//////////////////////////////////////////////////////////////////////////
// 64 bit over
//////////////////////////////////////////////////////////////////////////
#define OverNpm64toNpm24(s,d)	\
	{ \
		REG uint8 sa = (s)->a >> 8; \
		REG uint8 oma = 0xff - sa; \
		(d)->r = DivLut[( ((s)->r >> 8) * sa) + ((d)->r * oma)]; \
		(d)->g = DivLut[( ((s)->g >> 8) * sa) + ((d)->g * oma)]; \
		(d)->b = DivLut[( ((s)->b >> 8) * sa) + ((d)->b * oma)]; \
	}
#define OverNpm64toNpm32(s,d)	\
	{ \
		REG uint8 sa = s->a >> 8; \
		REG uint8 oma = 0xff - sa; \
		REG uint8 da = sa + DivLut[d->a * oma]; \
		d->r = (( (s->r>>8) * sa) + (DivLut[d->r * da] * oma)) / da; \
		d->g = (( (s->g>>8) * sa) + (DivLut[d->g * da] * oma)) / da; \
		d->b = (( (s->b>>8) * sa) + (DivLut[d->b * da] * oma)) / da; \
		d->a = da; \
	}
#define OverNpm64toNpm48(s,d)	\
	{ \
		REG uint16 sa = (s)->a; \
		REG uint16 oma = 0xffff - sa; \
		(d)->r = ( ((s)->r * sa) + ((d)->r * oma) ) / 0xffff; \
		(d)->g = ( ((s)->g * sa) + ((d)->g * oma) ) / 0xffff; \
		(d)->b = ( ((s)->b * sa) + ((d)->b * oma) ) / 0xffff; \
	}
#define OverNpm64toNpm64(s,d)	\
	{ \
		REG uint16 sa = (s)->a; \
		REG uint16 oma = 0xffff - sa; \
		REG uint16 da = sa + ((uint32)(d)->a * oma) / 0xffff; \
		(d)->r = (( (uint32)(s)->r * sa) + ( ((uint32)d->r * da) / 0xffff * oma)) / da; \
		(d)->g = (( (uint32)(s)->g * sa) + ( ((uint32)d->g * da) / 0xffff * oma)) / da; \
		(d)->b = (( (uint32)(s)->b * sa) + ( ((uint32)d->b * da) / 0xffff * oma)) / da; \
		(d)->a = da; \
	}


#endif
