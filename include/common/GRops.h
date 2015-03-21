#ifndef _GROPS_H_
#define _GROPS_H_

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;
	
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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	if (IsOverlapping())
	{
		register uint8 r, g, b;
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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	if (IsOverlapping())
	{
		register uint8 r, g, b;
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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G5bitTo8Bit(s->r);
		d->g = G5bitTo8Bit(s->g);
		d->b = G5bitTo8Bit(s->b);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To24(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G5bitTo8Bit(s->r);
		d->g = G6bitTo8Bit(s->g);
		d->b = G5bitTo8Bit(s->b);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To24(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	if ((uint8*)d == (uint8*)s)
	{
		register uint8 r, g, b;
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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G5bitTo8Bit(s->r);
		d->g = G5bitTo8Bit(s->g);
		d->b = G5bitTo8Bit(s->b);
		d->a = 255;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To32(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G5bitTo8Bit(s->r);
		d->g = G6bitTo8Bit(s->g);
		d->b = G5bitTo8Bit(s->b);
		d->a = 255;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To32(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	if (IsOverlapping())
	{
		register uint8 r, g, b;
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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	if (IsOverlapping())
	{
		register uint8 r, g, b, a;
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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = (int)G5bitTo8Bit(s->r) << 8;
		d->g = (int)G5bitTo8Bit(s->g) << 8;
		d->b = (int)G5bitTo8Bit(s->b) << 8;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To48(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = (int)G5bitTo8Bit(s->r) << 8;
		d->g = (int)G6bitTo8Bit(s->g) << 8;
		d->b = (int)G5bitTo8Bit(s->b) << 8;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To48(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G8bitTo16Bit(s->r);
		d->g = G8bitTo16Bit(s->g);
		d->b = G8bitTo16Bit(s->b);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop32To48(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G8bitTo16Bit(s->r);
		d->g = G8bitTo16Bit(s->g);
		d->b = G8bitTo16Bit(s->b);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop48To48(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = (int)G5bitTo8Bit(s->r) << 8;
		d->g = (int)G5bitTo8Bit(s->g) << 8;
		d->b = (int)G5bitTo8Bit(s->b) << 8;
		d->a = 0xffff;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop16To64(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = (int)G5bitTo8Bit(s->r) << 8;
		d->g = (int)G6bitTo8Bit(s->g) << 8;
		d->b = (int)G5bitTo8Bit(s->b) << 8;
		d->a = 0xffff;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop24To64(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G8bitTo16Bit(s->r);
		d->g = G8bitTo16Bit(s->g);
		d->b = G8bitTo16Bit(s->b);
		d->a = 0xffff;
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop32To64(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

	OverlapCheck()

	while (Px--)
	{
		d->r = G8bitTo16Bit(s->r);
		d->g = G8bitTo16Bit(s->g);
		d->b = G8bitTo16Bit(s->b);
		d->a = G8bitTo16Bit(s->a);
		s++;
		d++;
	}
}

template<typename DstPx, typename SrcPx>
void GRop48To64(DstPx *dst, SrcPx *src, int px)
{
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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
	register DstPx *d = dst;
	register SrcPx *s = src;
	register int Px = px;

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

#endif