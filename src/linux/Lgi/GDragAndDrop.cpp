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
	DragDropData = 0;
	DragDropLength = 0;
	To = 0;
}

GDragDropTarget::~GDragDropTarget()
{
	DragDropLength = 0;
	DeleteArray(DragDropData);
	Formats.DeleteArrays();
}

gboolean
GtkOnDragDrop(	GtkWidget *widget,
				GdkDragContext *context,
				gint x, gint y,
				guint time,
				gpointer userdata)
{
	GDragDropTarget *Target = (GDragDropTarget*)userdata;
	GdkAtom TargetType = NULL;
	for (Gtk::GList *Types = gdk_drag_context_list_targets(context); Types; Types = Types->next)
	{
		TargetType = (GdkAtom)Types->data;
		break;
	}
	
	if (TargetType)
	{
		gtk_drag_get_data
		(
			widget,
			context,
			TargetType,
			time
		);
		
		return true;
	}

	return false;
}

gboolean
GtkOnDragMotion(GtkWidget *widget,
				GdkDragContext *context,
				gint x, gint y,
				guint t,
				gpointer userdata)
{
	GDragDropTarget *Target = (GDragDropTarget*)userdata;
	List<char> Formats;
	for (Gtk::GList *Types = gdk_drag_context_list_targets(context); Types; Types = Types->next)
	{
		gchar *Type = gdk_atom_name((GdkAtom)Types->data);
		if (Type)
			Formats.Insert(NewStr(Type));
	}
	
	GdcPt2 p(x, y);
	int Result = Target->WillAccept(Formats, p, 0);

	Formats.DeleteArrays();

	return Result != DROPEFFECT_NONE;
}

void
GtkOnDragDataReceived(	GtkWidget *w,
						GdkDragContext *context,
						int x, int y,
                        GtkSelectionData *data,
                        guint info,
                        guint time,
                        gpointer userdata)
{
	GDragDropTarget *Target = (GDragDropTarget*)userdata;
	gchar *Type = gdk_atom_name(gtk_selection_data_get_data_type(data));
	GdcPt2 p(x, y);
	gint Len = gtk_selection_data_get_length(data);
	const guchar *Ptr = gtk_selection_data_get_data(data);
	if (Ptr && Len > 0)
	{
		::GVariant Data;
		Data.SetBinary(Len, (void*)Ptr);		
		int Status = Target->OnDrop(Type, &Data, p, 0);
		int asd=0;
	}
}

void
GtkOnDragLeave(	GtkWidget *widget,
				GdkDragContext *context,
				guint time,
				gpointer userdata)
{
	GDragDropTarget *ddt = (GDragDropTarget*)userdata;
	printf("GtkOnDragLeave ddt=%p\n", ddt);
}

void GDragDropTarget::SetWindow(GView *to)
{
	bool Status = false;
	To = to;
	if (To)
	{
		To->DropTargetPtr() = this;
		Status = To->DropTarget(true);
		if (To->Handle())
		{
			GtkWidget *w = to->Handle();
			
			printf("Installing DND handles on %s, Status=%i\n", to->GetClass(), Status);
			
			g_signal_connect(w, "drag-drop",			G_CALLBACK(GtkOnDragDrop),			this);
   			g_signal_connect(w, "drag-motion",			G_CALLBACK(GtkOnDragMotion),		this);
			g_signal_connect(w, "drag-data-received",	G_CALLBACK(GtkOnDragDataReceived), 	this);
			g_signal_connect(w, "drag-leave",			G_CALLBACK(GtkOnDragLeave),		 	this);

   			OnDragInit(Status);
		}
		else
		{
			printf("%s:%i - Error\n", __FILE__, __LINE__);
		}
	}
}

