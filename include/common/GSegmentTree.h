#ifndef _GSEGMENT_TREE_H_
#define _GSEGMENT_TREE_H_

#include "LgiDefs.h"
#include "GMem.h"

class GSegment
{
	friend class GSegmentTree;
	GSegment *Left, *Right, *Parent;	

	bool Insert(GSegment *Seg, GSegment **Conflict);
	GSegment *Find(int64 Off);
	void Index(GSegment **&i);

public:
	int64 Start;
	int64 Length;
	void *UserData;

	GSegment(int64 start, int64 len, void *userData = 0)
	{
		Left = Right = Parent = 0;
		Start = start;
		Length = len;
		UserData = userData;
	}

	~GSegment()
	{
		DeleteObj(Left);
		DeleteObj(Right);
	}

	bool Overlap(GSegment *Seg)
	{
		if (Seg->Start + Seg->Length <= Start ||
			Seg->Start >= Start + Length)
		{
			return false;
		}

		return true;
	}

	bool operator < (GSegment *Seg)
	{
		return Start + Length <= Seg->Start;
	}

	bool operator > (GSegment *Seg)
	{
		return Start >= Seg->Start + Seg->Length;
	}
};

class GSegmentTree
{
	class GSegmentTreePrivate *d;

public:
	GSegmentTree();
	virtual ~GSegmentTree();

	// Status
	int Items();

	// Change
	bool Insert(GSegment *Seg, GSegment **Conflict = 0);
	bool Delete(GSegment *Seg);
	void Empty();

	// Query
	GSegment *ItemAt(int Offset);
	GSegment **CreateIndex();
};

#endif