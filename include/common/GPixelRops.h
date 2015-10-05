//
//  GPixelRops.h
//  Lgi
//
//  Created by Matthew Allen on 1/03/14.
//  Copyright (c) 2014 Memecode. All rights reserved.
//

#ifndef _GPixelRops_h
#define _GPixelRops_h

#define NpmOver32to32(s,d)	\
	{ \
		register uint8 sa = s->a; \
		register uint8 oma = 0xff - sa; \
		register uint8 da = sa + DivLut[d->a * oma]; \
		d->r = ((s->r * sa) + (DivLut[d->r * da] * oma)) / da; \
		d->g = ((s->g * sa) + (DivLut[d->g * da] * oma)) / da; \
		d->b = ((s->b * sa) + (DivLut[d->b * da] * oma)) / da; \
		d->a = da; \
	}
#define NpmOver24to32(s,d,sa)	\
	{ \
		register uint8 oma = 0xff - sa; \
		register uint8 da = sa + DivLut[d->a * oma]; \
		d->r = ((s->r * sa) + (DivLut[d->r * da] * oma)) / da; \
		d->g = ((s->g * sa) + (DivLut[d->g * da] * oma)) / da; \
		d->b = ((s->b * sa) + (DivLut[d->b * da] * oma)) / da; \
		d->a = da; \
	}
#define NpmOver32to24(s,d)	\
	{ \
		register uint8 sa = (s)->a; \
		register uint8 oma = 0xff - sa; \
		register uint8 da = sa + ((d)->a * oma); \
		(d)->r = DivLut[((s)->r * sa) + ((d)->r * oma)]; \
		(d)->g = DivLut[((s)->g * sa) + ((d)->g * oma)]; \
		(d)->b = DivLut[((s)->b * sa) + ((d)->b * oma)]; \
	}
#define PmOver32to32(s,d)	\
	{ \
		register uint8 sa = s->a; \
		register uint8 oma = 0xff - sa; \
		register uint8 da = sa + DivLut[d->a * oma]; \
		d->r = s->r + DivLut[d->r * oma]; \
		d->g = s->g + DivLut[d->g * oma]; \
		d->b = s->b + DivLut[d->b * oma]; \
		d->a = da; \
	}

#endif
