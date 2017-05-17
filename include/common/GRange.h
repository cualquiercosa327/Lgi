#ifndef _GRANGE_H_
#define _GRANGE_H_

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

		int e = min(End(), r.End());
		o.Start = max(r.Start, Start);
		o.Len = e - o.Start;
		return o; 
	}

	int End() const
	{
		return Start + Len;
	}
};

#endif