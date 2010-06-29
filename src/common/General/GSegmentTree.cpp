#include "LgiInc.h"
#include "LgiOsDefs.h"
#include "GSegmentTree.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
bool GSegment::Insert(GSegment *Seg, GSegment **Conflict)
{
	if (Overlap(Seg))
	{
		*Conflict = this;
		return false;
	}

	if (*Seg < this)
	{
		if (Left)
		{
			return Left->Insert(Seg, Conflict);
		}
		else
		{
			Left = Seg;
			Seg->Parent = this;
		}
	}
	else if (*Seg > this)
	{
		if (Right)
		{
			return Right->Insert(Seg, Conflict);
		}
		else
		{
			Right = Seg;
			Seg->Parent = this;
		}
	}

	return true;
}

GSegment *GSegment::Find(int64 Off)
{
	if (Off >= Start AND Off < Start + Length)
	{
		return this;
	}

	if (Left)
	{
		GSegment *s = Left->Find(Off);
		if (s) return s;
	}

	if (Right)
	{
		return Right->Find(Off);
	}

	return 0;
}

void GSegment::Index(GSegment **&i)
{
	if (Left)
	{
		Left->Index(i);
	}

	*i++ = this;

	if (Right)
	{
		Right->Index(i);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
class GSegmentTreePrivate
{
public:
	int Items;
	GSegment *Root;

	GSegmentTreePrivate()
	{
		Items = 0;
		Root = 0;
	}

	~GSegmentTreePrivate()
	{
		DeleteObj(Root);
	}
};

GSegmentTree::GSegmentTree()
{
	d = new GSegmentTreePrivate;
}

GSegmentTree::~GSegmentTree()
{
	DeleteObj(d);
}

int GSegmentTree::Items()
{
	return d->Items;
}

void GSegmentTree::Empty()
{
	DeleteObj(d->Root);
	d->Items = 0;
}

bool GSegmentTree::Insert(GSegment *Seg, GSegment **Conflict)
{
	bool Status = false;

	if (Seg)
	{
		if (d->Root)
		{
			GSegment *c = 0;			
			Status = d->Root->Insert(Seg, &c);
			if (!Status AND Conflict)
			{
				*Conflict = c;
			}
		}
		else
		{
			d->Root = Seg;
			Status = true;
		}

		if (Status)
		{
			d->Items++;
		}
	}

	return Status;
}

bool GSegmentTree::Delete(GSegment *Seg)
{
	return false;
}

GSegment *GSegmentTree::ItemAt(int Offset)
{
	return d->Root ? d->Root->Find(Offset) : 0;
}

GSegment **GSegmentTree::CreateIndex()
{
	GSegment **Index = 0;

	if (d->Items > 0 AND d->Root)
	{
		Index = new GSegment*[d->Items];
		if (Index)
		{
			GSegment **i = Index;
			d->Root->Index(i);
			LgiAssert(i == Index + d->Items);
		}
	}

	return Index;
}
