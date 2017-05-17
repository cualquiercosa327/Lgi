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
	int Start;
	int Len;

	GRange(int s = 0, int l = 0)
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

		int e = MIN(End(), r.End());
		o.Start = MAX(r.Start, Start);
		o.Len = e - o.Start;
		return o; 
	}

	int End() const
	{
		return Start + Len;
	}
};

#endif