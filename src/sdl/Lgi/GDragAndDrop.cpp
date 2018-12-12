/*
**	FILE:			GDragAndDrop.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			30/11/98
**	DESCRIPTION:	Drag and drop support
**
**	Copyright (C) 1998-2014, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"

static int NextDndType = 600;
static LHashTbl<ConstStrKey<char,false>, int> DndTypes(0, -1);

class GDndSourcePriv
{
public:
	GAutoString CurrentFormat;
	
	GDndSourcePriv()
	{
	}
	
	~GDndSourcePriv()
	{
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
GDragDropSource::GDragDropSource()
{
	d = new GDndSourcePriv;
	OnRegister(true);
}

GDragDropSource::~GDragDropSource()
{
	DeleteObj(d);
}

bool GDragDropSource::SetIcon(GSurface *Img, GRect *SubRgn)
{
	return false;
}

bool GDragDropSource::CreateFileDrop(GDragData *OutputData, GMouse &m, GString::Array &Files)
{
	if (OutputData && Files.First())
	{
		GStringPipe p;
		for (auto f : Files)
		{
			char s[256];
			sprintf_s(s, sizeof(s), "file:%s", f.Get());
			if (p.GetSize()) p.Push("\n");
			p.Push(s);
		}

		GAutoString s(p.NewStr());
		if (s)
		{
			OutputData->Format = LGI_FileDropFormat;
			OutputData->Data.First().SetBinary(strlen(s), s);
			return true;
		}
	}

	return false;
}

int GDragDropSource::Drag(GView *SourceWnd, int Effect)
{
	LgiAssert(SourceWnd);
	if (!SourceWnd)
		return -1;

	if (!SourceWnd || !SourceWnd->Handle())
	{
		LgiTrace("%s:%i - Error: No source window or handle.\n", _FL);
		return -1;
	}

	List<char> Formats;
	if (!GetFormats(Formats))
	{
		LgiTrace("%s:%i - Error: Failed to get source formats.\n", _FL);
		return -1;
	}
	
	int Button = 1;

    return -1;
}

////////////////////////////////////////////////////////////////////////////////////////////
GDragDropTarget::GDragDropTarget()
{
	To = 0;
}

GDragDropTarget::~GDragDropTarget()
{
	Formats.DeleteArrays();
}

void GDragDropTarget::SetWindow(GView *to)
{
	bool Status = false;
	To = to;
	if (To)
	{
		To->DropTarget(this);
		Status = To->DropTarget(true);
		if (To->Handle())
		{
   			OnDragInit(Status);
		}
		else
		{
			LgiTrace("%s:%i - No view handle\n", _FL);
		}
	}
}

