/*
**	FILE:			GDragAndDrop.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			30/11/98
**	DESCRIPTION:	Drag and drop support
**
**	Copyright (C) 1998-2003, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"

class GDndSourcePriv
{
public:
	GAutoString CurrentFormat;
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

bool GDragDropSource::GetData(GArray<GDragData> &DragData)
{
	if (DragData.Length() == 0)
		return false;

	// Call the deprecated version of 'GetData'
	GVariant *v = &DragData[0].Data[0];
	char *fmt = DragData[0].Format;
	if (!v || !fmt)
		return false;

	return GetData(v, fmt);
}

bool GDragDropSource::CreateFileDrop(GDragData *OutputData, GMouse &m, List<char> &Files)
{
	if (OutputData && Files.First())
	{
	}

	return false;
}

int GDragDropSource::Drag(GView *SourceWnd, int Effect)
{
	LgiAssert(SourceWnd);
	if (!SourceWnd)
		return -1;

	if (SourceWnd)
	{
		BMessage Msg(M_DRAG_DROP);
		Msg.AddPointer("GDragDropSource", this);

		GMouse m;
		SourceWnd->GetMouse(m);
		BRect r(m.x, m.y, m.x+14, m.y+14);

		SourceWnd->Handle()->DragMessage(&Msg, r);

		return DROPEFFECT_COPY;
	}

	return DROPEFFECT_NONE;
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
		// To->DropTargetPtr() = this;
		Status = To->DropTarget(true);
		if (To->Handle())
		{
			OnDragInit(Status);
		}
		else
		{
			printf("%s:%i - Error\n", _FL);
		}
	}
}

int GDragDropTarget::OnDrop(GArray<GDragData> &DropData,
							GdcPt2 Pt,
							int KeyState)
{
	if (DropData.Length() == 0 ||
		DropData[0].Data.Length() == 0)
		return DROPEFFECT_NONE;
	
	char *Fmt = DropData[0].Format;
	GVariant *Var = &DropData[0].Data[0];
	return OnDrop(Fmt, Var, Pt, KeyState);
}
