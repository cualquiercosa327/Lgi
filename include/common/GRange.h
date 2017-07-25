#ifndef _GRANGE_H_
#define _GRANGE_H_

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? a : b)
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? a : b)
#endif

struct GRange
{
	ssize_t Start;
	ssize_t Len;

	GRange(ssize_t s = 0, ssize_t l = 0)
	{
		Start = s;
		Len = l;
	}

	GRange Overlap(const GRange &r)
	{
		GRange o;
		if (r.Start >= End())
			return o;
		if (r.End() <= Start)
			return o;

		ssize_t e = MIN(End(), r.End());
		o.Start = MAX(r.Start, Start);
		o.Len = e - o.Start;
		return o; 
	}

	ssize_t End() const
	{
		return Start + Len;
	}
};

#endif