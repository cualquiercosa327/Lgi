#ifndef _GRANGE_H_
#define _GRANGE_H_

#include <assert.h>

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

	GRange &Set(ssize_t s, ssize_t l)
	{
		Start = s;
		Len = l;
		return *this;
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

	bool Overlap(ssize_t Val)
	{
		return Val >= Start &&
				Val <= End();
	}

	ssize_t End() const
	{
		return Start + Len;
	}

	bool Valid() const
	{
		return Start >= 0 && Len > 0;
	}

	bool operator ==(const GRange &r) const { return Start == r.Start && Len == r.Len; }
	bool operator !=(const GRange &r) const { return Start != r.Start || Len != r.Len; }
	bool operator >(const GRange &r) const { return Start > r.Start; }
	bool operator >=(const GRange &r) const { return Start >= r.Start; }
	bool operator <(const GRange &r) const { return Start < r.Start; }
	bool operator <=(const GRange &r) const { return Start < r.Start; }

	GRange &operator -=(const GRange &del)
	{
		GRange o = Overlap(del);
		if (o.Valid())
		{
			assert(o.Len <= Len);
			Len -= o.Len;
			if (del.Start < o.Start)
				Start = del.Start;
		}
		// else nothing happens

		return *this;
	}

	GRange &operator =(const GRange &r)
	{
		this->Start = r.Start;
		this->Len = r.Len;
		return *this;
	}
};

#endif