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

bool GDragDropSource::CreateFileDrop(GDragData *OutputData, GMouse &m, List<char> &Files)
{
	if (!OutputData || !Files.First())
		return false;

	GStringPipe p;
	for (char *f=Files.First(); f; f=Files.Next())
	{
		char s[256];
		sprintf_s(s, sizeof(s), "file:%s", f);
		if (p.GetSize()) p.Push("\n");
		p.Push(s);
	}

	GAutoString s(p.NewStr());
	if (s)
	{
		OutputData->Data[0].SetBinary(strlen(s), s);
		return true;
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

void 
LgiDragDataGet(GtkWidget        *widget,
               GdkDragContext   *context,
               GtkSelectionData *selection_data,
               guint             info,
               guint             time,
               gpointer          data)
{
	GDragDropSource *Src = (GDragDropSource*)data;

	// Iterate over the targets and put their formats into 'dd'
	Gtk::GList *targets = gdk_drag_context_list_targets(context);
	Gtk::GList *node;
	if (targets)
	{
		::GArray<GDragData> dd;
		for (node = g_list_first(targets); node != NULL; node = ((node) ? (((Gtk::GList *)(node))->next) : NULL))
        {
			gchar *format = gdk_atom_name((GdkAtom)node->data);
			dd[dd.Length()].Format = format;
        }
		
		if (Src->GetData(dd))
		{
			if (dd.Length() > 0 &&
				dd[0].Data.Length() > 0)
			{
				::GVariant &v = dd[0].Data[0];
				switch (v.Type)
				{
					case GV_STRING:
					{
						char *string = v.Str();
						if (string)
						{
							gtk_selection_data_set (selection_data,
													gtk_selection_data_get_target(selection_data),
													8,
													(Gtk::guchar*) string,
													strlen(string) + 1);
						}
						else LgiAssert(0);
						break;
					}
					case GV_BINARY:
					{
						if (v.Value.Binary.Data)
						{
							gtk_selection_data_set (selection_data,
													gtk_selection_data_get_target(selection_data),
													8,
													(Gtk::guchar*) v.Value.Binary.Data,
													v.Value.Binary.Length);
						}
						else LgiAssert(0);
						break;
					}
					default:
					{
						LgiAssert(!"Impl this data type?");
						break;
					}
				}
			}
		}
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
	OsView Wnd = SourceWnd->Handle();
	gtk_signal_connect(GTK_OBJECT(Wnd), "drag_data_get", GTK_SIGNAL_FUNC(LgiDragDataGet), this);

	d->Ctx = Gtk::gtk_drag_begin(Wnd,
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

