//
//  GPixelRops.h
//  Lgi
//
//  Created by Matthew Allen on 1/03/14.
//  Copyright (c) 2014 Memecode. All rights reserved.
//

#ifndef _GPixelRops_h
#define _GPixelRops_h

template<typename OutPx, typename InPx>
void Copy15To24(OutPx *out, InPx *in, int Len)
{
	InPx *end = in + Len;
	while (in < end)
	{
		out->r = G5bitTo8Bit(in->r);
		out->g = G5bitTo8Bit(in->g);
		out->b = G5bitTo8Bit(in->b);
		out++;
		in++;
	}
}

template<typename OutPx, typename InPx>
void Copy16To24(OutPx *out, InPx *in, int Len)
{
	InPx *end = in + Len;
	while (in < end)
	{
		out->r = G5bitTo8Bit(in->r);
		out->g = G6bitTo8Bit(in->g);
		out->b = G5bitTo8Bit(in->b);
		out++;
		in++;
	}
}

template<typename OutPx, typename InPx>
void Copy24(OutPx *out, InPx *in, int Len)
{
	InPx *end = in + Len;
	while (in < end)
	{
		out->r = in->r;
		out->g = in->g;
		out->b = in->b;
		out++;
		in++;
	}
}

template<typename OutPx, typename InPx>
void Copy32(OutPx *out, InPx *in, int Len)
{
	InPx *end = in + Len;
	while (in < end)
	{
		out->r = in->r;
		out->g = in->g;
		out->b = in->b;
		out->a = in->a;
		out++;
		in++;
	}
}

template<typename OutPx, typename InPx>
void Copy24to32(OutPx *out, InPx *in, int Len)
{
	InPx *end = in + Len;
	while (in < end)
	{
		out->r = in->r;
		out->g = in->g;
		out->b = in->b;
		out->a = -1; // resolves to both 0xff and 0xffff
		out++;
		in++;
	}
}

template<typename OutPx, typename InPx>
void CompositeNonPreMul32(OutPx *d, InPx *s, int Len)
{
	InPx *end = s + Len;
	uint8 *DivLut = Div255Lut;

	while (s < end)
	{
		if (s->a == 255)
		{
			// Copy pixel
			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
			d->a = s->a;
		}
		else if (d->a && s->a)
		{
			// Composite pixel
			//		Dc'  = (Sca + Dc.Da.(1 - Sa)) / Da'
			//		Da'  = Sa + Da.(1 - Sa)
			register int o = 0xff - s->a;
			
			#define NonPreMul32(c)	\
				d->c = (DivLut[s->c * s->a] + (DivLut[d->c * d->a] * o)) / d->a
			
			NonPreMul32(r);
			NonPreMul32(g);
			NonPreMul32(b);
			d->a = s->a + DivLut[d->a * o];
		}

		s++;
		d++;
	}
}

#define CsMapping(OutCs, InCs) ( (((uint64)OutCs) << 32) | (InCs) )

LgiFunc bool GUniversalBlt(	GColourSpace OutCs,
							uint8 *OutPtr,
							int OutLine,
							
							GColourSpace InCs,
							uint8 *InPtr,
							int InLine,
							
							int x,
							int y);
					
					
#endif
