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

// #define DND_DEBUG_TRACE

static int NextDndType = 600;
static GHashTbl<const char*, int> DndTypes(0, false, NULL, -1);

typedef Gtk::GList GnuList;
using namespace Gtk;

GdkDragAction DropEffectToAction(int DropEffect)
{
	GdkDragAction action = GDK_ACTION_DEFAULT;
	switch (DropEffect)
	{
		case DROPEFFECT_COPY:
			action = GDK_ACTION_COPY;
			break;
		case DROPEFFECT_MOVE:
			action = GDK_ACTION_MOVE;
			break;
		case DROPEFFECT_LINK:
			action = GDK_ACTION_LINK;
			break;
	}
	return action;
}

int GtkGetDndType(const char *Format)
{
	int Type = DndTypes.Find(Format);
	if (Type < 0)
		DndTypes.Add(Format, Type = NextDndType++);
	return Type;
}

const char *GtkGetDndFormat(int Type)
{
	return DndTypes.FindKey(Type);
}

class GDndSourcePriv
{
public:
	GAutoString CurrentFormat;
	Gtk::GdkDragContext *Ctx;
	
	GDndSourcePriv()
	{
		Ctx = NULL;
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

bool GDragDropSource::GetData(::GArray<GDragData> &DragData)
{
	if (DragData.Length() == 0)
		return false;

	// Call the deprecated version of 'GetData'
	::GVariant *v = &DragData[0].Data[0];
	char *fmt = DragData[0].Format;
	return GetData(v, fmt);
}

bool GDragDropSource::SetIcon(GSurface *Img, GRect *SubRgn)
{
	return false;
}

bool GDragDropSource::CreateFileDrop(::GVariant *OutputData, GMouse &m, List<char> &Files)
{
	if (OutputData && Files.First())
	{
		GStringPipe p;
		for (char *f=Files.First(); f; f=Files.Next())
		{
			char s[256];
			sprintf_s(s, sizeof(s), "file:%s", f);
			if (p.GetSize()) p.Push("\n");
			p.Push(s);
		}

		char *s = p.NewStr();
		if (s)
		{
			OutputData->SetBinary(strlen(s), s);
			DeleteArray(s);
			return true;
		}
	}

	return false;
}

static Gtk::GdkDragAction EffectToDragAction(int Effect)
{
	switch (Effect)
	{
		default:
		case DROPEFFECT_NONE:
			return Gtk::GDK_ACTION_DEFAULT;
		case DROPEFFECT_COPY:
			return Gtk::GDK_ACTION_COPY;
		case DROPEFFECT_MOVE:
			return Gtk::GDK_ACTION_MOVE;
		case DROPEFFECT_LINK:
			return Gtk::GDK_ACTION_LINK;
	}
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
	
	::GArray<GtkTargetEntry> e;
	for (char *f = Formats.First(); f; f = Formats.Next())
	{
		Gtk::GtkTargetEntry &entry = e.New();
		entry.target = f;
		entry.flags = 0;
		entry.info = GtkGetDndType(f);
	}
	
	Gtk::GtkTargetList *Targets = Gtk::gtk_target_list_new(&e[0], e.Length());
	
	Gtk::GdkDragAction Action = EffectToDragAction(Effect);
	
	int Button = 1;

	d->Ctx = Gtk::gtk_drag_begin(SourceWnd->Handle(),
								Targets,
								Action,
								Button,
								NULL); // Gdk event if available...

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
			GtkWidget *w = to->Handle();
			
			#if 0
			printf("Installing DND handles on %s, Status=%i\n", to->GetClass(), Status);
			
   			g_signal_connect(w, "drag-motion",			G_CALLBACK(GtkOnDragMotion),		this);
			g_signal_connect(w, "drag-drop",			G_CALLBACK(GtkOnDragDrop),			this);
			g_signal_connect(w, "drag-data-received",	G_CALLBACK(GtkOnDragDataReceived), 	this);
			g_signal_connect(w, "drag-leave",			G_CALLBACK(GtkOnDragLeave),		 	this);
			#endif

   			OnDragInit(Status);
		}
		else
		{
			LgiTrace("%s:%i - No view handle\n", _FL);
		}
	}
}

int GDragDropTarget::OnDrop(::GArray<GDragData> &DropData,
							GdcPt2 Pt,
							int KeyState)
{
	if (DropData.Length() == 0 ||
		DropData[0].Data.Length() == 0)
		return DROPEFFECT_NONE;
	
	char *Fmt = DropData[0].Format;
	::GVariant *Var = &DropData[0].Data[0];
	return OnDrop(Fmt, Var, Pt, KeyState);
}

