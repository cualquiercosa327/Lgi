#ifndef _GROPS_H_
#define _GROPS_H_

#include "GPixelRops.h"

#define IsOverlapping()	 ((uint8*)dst == (uint8*)src)

#define OverlapCheck()							\
	if (IsOverlapping())						\
	{											\
		LgiAssert(!"regions can't overlap.");	\
		return;									\
	}

//////////////////////////////////////////////////////////////////
// To 15 bits
template<typename DstPx, typename SrcPx>
void GRop15To15(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To15(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r;
		d->g = s->g >> 1;
		d->b = s->b;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To15(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 3;
		d->g = s->g >> 3;
		d->b = s->b >> 3;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop32To15(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 3;
		d->g = s->g >> 3;
		d->b = s->b >> 3;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop48To15(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 11;
		d->g = s->g >> 11;
		d->b = s->b >> 11;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop64To15(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 11;
		d->g = s->g >> 11;
		d->b = s->b >> 11;
		s++;
		d++;
	}
}

//////////////////////////////////////////////////////////////////
// To 16 bits
template<typename DstPx, typename SrcPx>
void GRop15To16(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	if (IsOverlapping())
	{
		REG uint8 r, g, b;
		while (Px--)
		{
			r = s->r;
			g = s->g;
			b = s->b;
			d->r = r;
			d->g = (g << 1) | (g >> 4);
			d->b = b;
			s++;
			d++;
		}
	}
	else
	{
		while (Px--)
		{
			d->r = s->r;
			d->g = (uint16)(s->g << 1) | (s->g >> 4);
			d->b = s->b;
			s++;
			d++;
		}
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To16(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	if (IsOverlapping())
	{
		REG uint8 r, g, b;
		while (Px--)
		{
			r = s->r;
			g = s->g;
			b = s->b;
			d->r = r;
			d->g = g;
			d->b = b;
			s++;
			d++;
		}
	}
	else
	{
		while (Px--)
		{
			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
			s++;
			d++;
		}
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To16(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 3;
		d->g = s->g >> 2;
		d->b = s->b >> 3;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop32To16(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 3;
		d->g = s->g >> 2;
		d->b = s->b >> 3;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop48To16(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 11;
		d->g = s->g >> 10;
		d->b = s->b >> 11;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop64To16(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 11;
		d->g = s->g >> 10;
		d->b = s->b >> 11;
		s++;
		d++;
	}
}

//////////////////////////////////////////////////////////////////
// To 24 bits
template<typename DstPx, typename SrcPx>
void GRop15To24(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G5bitTo8bit(s->r);
		d->g = G5bitTo8bit(s->g);
		d->b = G5bitTo8bit(s->b);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To24(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G5bitTo8bit(s->r);
		d->g = G6bitTo8bit(s->g);
		d->b = G5bitTo8bit(s->b);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To24(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	if ((uint8*)d == (uint8*)s)
	{
		REG uint8 r, g, b;
		d += px - 1;
		s += px - 1;
		while (Px--)
		{
			r = s->r;
			g = s->g;
			b = s->b;
			d->r = r;
			d->g = g;
			d->b = b;
			s--;
			d--;
		}
	}
	else
	{
		while (Px--)
		{
			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
			s++;
			d++;
		}
	}
}

template<typename DstPx, typename SrcPx>
void GRop32To24(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop48To24(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 8;
		d->g = s->g >> 8;
		d->b = s->b >> 8;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop64To24(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 8;
		d->g = s->g >> 8;
		d->b = s->b >> 8;
		s++;
		d++;
	}
}


//////////////////////////////////////////////////////////////////
// To 32 bits
template<typename DstPx, typename SrcPx>
void GRop15To32(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G5bitTo8bit(s->r);
		d->g = G5bitTo8bit(s->g);
		d->b = G5bitTo8bit(s->b);
		d->a = 255;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To32(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G5bitTo8bit(s->r);
		d->g = G6bitTo8bit(s->g);
		d->b = G5bitTo8bit(s->b);
		d->a = 255;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To32(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	if (IsOverlapping())
	{
		REG uint8 r, g, b;
		d += px - 1;
		s += px - 1;
		
		while (Px--)
		{
			r = s->r;
			g = s->g;
			b = s->b;
			d->r = r;
			d->g = g;
			d->b = b;
			d->a = 255;
			s--;
			d--;
		}
	}
	else
	{
		while (Px--)
		{
			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
			d->a = 255;
			s++;
			d++;
		}
	}
}

template<typename DstPx, typename SrcPx>
void GRop32To32(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	if (IsOverlapping())
	{
		REG uint8 r, g, b, a;
		while (Px--)
		{
			r = s->r;
			g = s->g;
			b = s->b;
			a = s->a;
			d->r = r;
			d->g = g;
			d->b = b;
			d->a = a;
			s++;
			d++;
		}
	}
	else
	{
		while (Px--)
		{
			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
			d->a = s->a;
			s++;
			d++;
		}
	}
}

template<typename DstPx, typename SrcPx>
void GRop48To32(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 8;
		d->g = s->g >> 8;
		d->b = s->b >> 8;
		d->a = 255;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop64To32(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r >> 8;
		d->g = s->g >> 8;
		d->b = s->b >> 8;
		d->a = s->a >> 8;
		s++;
		d++;
	}
}

//////////////////////////////////////////////////////////////////
// To 48 bits
template<typename DstPx, typename SrcPx>
void GRop15To48(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = (int)G5bitTo8bit(s->r) << 8;
		d->g = (int)G5bitTo8bit(s->g) << 8;
		d->b = (int)G5bitTo8bit(s->b) << 8;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To48(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = (int)G5bitTo8bit(s->r) << 8;
		d->g = (int)G6bitTo8bit(s->g) << 8;
		d->b = (int)G5bitTo8bit(s->b) << 8;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To48(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G8bitTo16bit(s->r);
		d->g = G8bitTo16bit(s->g);
		d->b = G8bitTo16bit(s->b);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop32To48(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G8bitTo16bit(s->r);
		d->g = G8bitTo16bit(s->g);
		d->b = G8bitTo16bit(s->b);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop48To48(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop64To48(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		s++;
		d++;
	}
}

//////////////////////////////////////////////////////////////////
// To 64 bits
template<typename DstPx, typename SrcPx>
void GRop15To64(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = (int)G5bitTo8bit(s->r) << 8;
		d->g = (int)G5bitTo8bit(s->g) << 8;
		d->b = (int)G5bitTo8bit(s->b) << 8;
		d->a = 0xffff;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To64(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = (int)G5bitTo8bit(s->r) << 8;
		d->g = (int)G6bitTo8bit(s->g) << 8;
		d->b = (int)G5bitTo8bit(s->b) << 8;
		d->a = 0xffff;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To64(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G8bitTo16bit(s->r);
		d->g = G8bitTo16bit(s->g);
		d->b = G8bitTo16bit(s->b);
		d->a = 0xffff;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop32To64(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G8bitTo16bit(s->r);
		d->g = G8bitTo16bit(s->g);
		d->b = G8bitTo16bit(s->b);
		d->a = G8bitTo16bit(s->a);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop48To64(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		d->a = 0xffff;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop64To64(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		d->a = s->a;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite15To24(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		if (s->a)
		{
			d->r = G5bitTo8bit(s->r);
			d->g = G5bitTo8bit(s->g);
			d->b = G5bitTo8bit(s->b);
		}
		
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite15To32(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		if (s->a)
		{
			d->r = G5bitTo8bit(s->r);
			d->g = G5bitTo8bit(s->g);
			d->b = G5bitTo8bit(s->b);
			d->a = 0xff;
		}
		
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite15To48(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		if (s->a)
		{
			d->r = G5bitTo8bit(s->r)<<8;
			d->g = G5bitTo8bit(s->g)<<8;
			d->b = G5bitTo8bit(s->b)<<8;
		}
		
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite15To15(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		if (s->a)
		{
			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
		}
		
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite15To16(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		if (s->a)
		{
			d->r = s->r;
			d->g = s->g << 1;
			d->b = s->b;
		}
		
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite15To64(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		if (s->a)
		{
			d->r = G5bitTo8bit(s->r) << 8;
			d->g = G5bitTo8bit(s->g) << 8;
			d->b = G5bitTo8bit(s->b) << 8;
		}
		
		s++;
		d++;
	}
}

template<typename OutPx, typename InPx>
void GComposite32To15(OutPx *d, InPx *s, int Len)
{
	InPx *end = s + Len;
	uint8 *DivLut = Div255Lut;
	REG uint8 sa;

	while (s < end)
	{
		sa = s->a;
		if (sa == 255)
		{
			// Copy pixel
			d->r = G8bitTo5bit(s->r);
			d->g = G8bitTo5bit(s->g);
			d->b = G8bitTo5bit(s->b);
		}
		else if (sa)
		{
			// Composite pixel
			//		Dc'  = (Sca + Dc.Da.(1 - Sa)) / Da'
			//		Da'  = Sa + Da.(1 - Sa)
			REG uint8 o = 0xff - sa;
			REG uint8 val;
			
			#define NonPreMul(c)	\
				val = DivLut[(s->c * sa) + (G5bitTo8bit(d->c) * o)]; \
				d->c = G8bitTo5bit(val)			
			NonPreMul(r);
			NonPreMul(g);
			NonPreMul(b);
			#undef NonPreMul
		}

		s++;
		d++;
	}
}

template<typename OutPx, typename InPx>
void GComposite32To16(OutPx *d, InPx *s, int Len)
{
	InPx *end = s + Len;
	uint8 *DivLut = Div255Lut;
	REG uint8 sa;

	while (s < end)
	{
		sa = s->a;
		if (sa == 255)
		{
			// Copy pixel
			d->r = G8bitTo5bit(s->r);
			d->g = G8bitTo6bit(s->g);
			d->b = G8bitTo5bit(s->b);
		}
		else if (sa)
		{
			// Composite pixel
			//		Dc'  = (Sca + Dc.Da.(1 - Sa)) / Da'
			//		Da'  = Sa + Da.(1 - Sa)
			REG uint8 o = 0xff - sa;
			REG uint8 val;
			
			#define NonPreMul(c, up, down)	\
				val = DivLut[(s->c * sa) + (G##up(d->c) * o)]; \
				d->c = G##down(val)			
			NonPreMul(r, 5bitTo8bit, 8bitTo5bit);
			NonPreMul(g, 6bitTo8bit, 8bitTo6bit);
			NonPreMul(b, 5bitTo8bit, 8bitTo5bit);
			#undef NonPreMul
		}

		s++;
		d++;
	}
}

template<typename OutPx, typename InPx>
void GComposite32To24(OutPx *d, InPx *s, int Len)
{
	InPx *end = s + Len;
	uint8 *DivLut = Div255Lut;
	REG uint8 sa;

	while (s < end)
	{
		sa = s->a;
		if (sa == 255)
		{
			// Copy pixel
			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
		}
		else if (sa)
		{
			// Composite pixel
			//		Dc'  = (Sca + Dc.Da.(1 - Sa)) / Da'
			//		Da'  = Sa + Da.(1 - Sa)
			REG uint8 o = 0xff - sa;
			
			#define NonPreMul(c)	\
				d->c = DivLut[(s->c * sa) + (d->c * o)]
			NonPreMul(r);
			NonPreMul(g);
			NonPreMul(b);
			#undef NonPreMul
		}

		s++;
		d++;
	}
}

template<typename OutPx, typename InPx>
void GComposite32To32(OutPx *d, InPx *s, int Len)
{
	InPx *end = s + Len;
	uint8 *DivLut = Div255Lut;
	REG uint8 sa;

	while (s < end)
	{
		sa = s->a;
		if (sa == 255)
		{
			// Copy pixel
			d->r = s->r;
			d->g = s->g;
			d->b = s->b;
			d->a = sa;
		}
		else if (sa)
		{
			// Composite pixel
			//		Dc'  = (Sca + Dc.Da.(1 - Sa)) / Da'
			//		Da'  = Sa + Da.(1 - Sa)
			OverNpm32toNpm32(s, d);
		}

		s++;
		d++;
	}
}

template<typename OutPx, typename InPx>
void GComposite32To48(OutPx *d, InPx *s, int Len)
{
	InPx *end = s + Len;
	REG uint8 sa;

	while (s < end)
	{
		sa = s->a;
		if (sa == 255)
		{
			// Copy pixel
			d->r = G8bitTo16bit(s->r);
			d->g = G8bitTo16bit(s->g);
			d->b = G8bitTo16bit(s->b);
		}
		else if (sa)
		{
			// Composite pixel
			//		Da'  = Sa + Da.(1 - Sa)
			//		Dc'  = (Sc.Sa + Dc.Da.(1 - Sa)) / Da'
			OverNpm32toNpm48(s, d);
		}

		s++;
		d++;
	}
}

template<typename OutPx, typename InPx>
void GComposite32To64(OutPx *d, InPx *s, int Len)
{
	InPx *end = s + Len;
	REG uint8 sa;

	while (s < end)
	{
		sa = s->a;
		if (sa == 255)
		{
			// Copy pixel
			d->r = G8bitTo16bit(s->r);
			d->g = G8bitTo16bit(s->g);
			d->b = G8bitTo16bit(s->b);
			d->a = G8bitTo16bit(sa);
		}
		else if (sa)
		{
			// Composite pixel
			//		Dc'  = (Sca + Dc.Da.(1 - Sa)) / Da'
			//		Da'  = Sa + Da.(1 - Sa)
			OverNpm32toNpm64(s, d);
		}

		s++;
		d++;
	}
}

template<typename OutPx, typename InPx>
void GComposite32to24(OutPx *d, InPx *s, int Len)
{
	REG OutPx *dst = d;
	REG InPx *src = s;
	REG InPx *end = src + Len;
	uint8 *DivLut = Div255Lut;
	REG uint8 sa;

	while (src < end)
	{
		sa = src->a;
		if (sa == 255)
		{
			// Copy pixel
			dst->r = src->r;
			dst->g = src->g;
			dst->b = src->b;
		}
		else if (sa)
		{
			// Composite pixel
			//		Dc'  = (Sc.Sa + Dc.Da.(1 - Sa)) / Da'
			//		Da'  = Sa + Da.(1 - Sa)
			REG uint8 o = 0xff - sa;
			
			#define NonPreMul24(c)	\
				dst->c = DivLut[(src->c * sa) + (dst->c * o)]
			
			NonPreMul24(r);
			NonPreMul24(g);
			NonPreMul24(b);
		}

		src++;
		dst++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite64To15(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	REG uint8 *DivLut = Div255Lut;
	
	OverlapCheck()
	
	while (Px--)
	{
		REG GRgb24 dst =
		{
			G5bitTo8bit(d->r),
			G5bitTo8bit(d->g),
			G5bitTo8bit(d->b)
		};
		OverNpm64toNpm24(s, &dst);
		d->r = dst.r >> 3;
		d->g = dst.g >> 3;
		d->b = dst.b >> 3;
		
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite64To16(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	REG uint8 *DivLut = Div255Lut;
	
	OverlapCheck()
	
	while (Px--)
	{
		REG GRgb24 dst =
		{
			G5bitTo8bit(d->r),
			G6bitTo8bit(d->g),
			G5bitTo8bit(d->b)
		};
		OverNpm64toNpm24(s, &dst);
		d->r = dst.r >> 3;
		d->g = dst.g >> 2;
		d->b = dst.b >> 3;
		
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite64To24(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	REG uint8 *DivLut = Div255Lut;
	
	OverlapCheck()
	
	while (Px--)
	{
		OverNpm64toNpm24(s, d);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite64To32(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	REG uint8 *DivLut = Div255Lut;
	
	OverlapCheck()
	
	while (Px--)
	{
		OverNpm64toNpm32(s, d);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite64To48(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		OverNpm64toNpm48(s, d);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GComposite64To64(DstPx *dst, SrcPx *src, int px)
{
	REG DstPx *d = dst;
	REG SrcPx *s = src;
	REG int Px = px;
	
	OverlapCheck()
	
	while (Px--)
	{
		OverNpm64toNpm64(s, d);
		s++;
		d++;
	}
}
#endif