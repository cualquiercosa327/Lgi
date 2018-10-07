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

	OsView SignalWnd;
	
	GDndSourcePriv()
	{
		Ctx = NULL;
		SignalWnd = NULL;
	}
	
	~GDndSourcePriv()
	{
		
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
struct SignalInfo
{
	OsView Wnd;
	gulong Sig;
};

static ::GArray<SignalInfo> ExistingSignals;

void RemoveExistingSignals(OsView w)
{
	for (unsigned i=0; i<ExistingSignals.Length(); i++)
	{
		SignalInfo &Si = ExistingSignals[i];
		if (Si.Wnd == w)
		{
			if (Si.Sig > 0)
			{
				gtk_signal_disconnect(GTK_OBJECT(w), Si.Sig);
				Si.Sig = 0;
			}
			
			ExistingSignals.DeleteAt(i--);			
		}
	}
}

GDragDropSource::GDragDropSource()
{
	d = new GDndSourcePriv;
	OnRegister(true);
}

GDragDropSource::~GDragDropSource()
{
	RemoveExistingSignals(d->SignalWnd);
	DeleteObj(d);
}

bool GDragDropSource::SetIcon(GSurface *Img, GRect *SubRgn)
{
	return false;
}

bool GDragDropSource::CreateFileDrop(GDragData *OutputData, GMouse &m, ::GString::Array &Files)
{
	if (!OutputData || !Files.First())
		return false;

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
		
		// LgiTrace("%s:%i - dnd fmt %s %i\n", _FL, f, entry.info);
	}
	
	Gtk::GtkTargetList *Targets = Gtk::gtk_target_list_new(&e[0], e.Length());
	
	Gtk::GdkDragAction Action = EffectToDragAction(Effect);
	
	int Button = 1;
	d->SignalWnd = SourceWnd->Handle();
	RemoveExistingSignals(d->SignalWnd);
	SignalInfo &Si = ExistingSignals.New();
	Si.Wnd = d->SignalWnd;
	Si.Sig = gtk_signal_connect(GTK_OBJECT(d->SignalWnd), "drag-data-get", G_CALLBACK(LgiDragDataGet), this);

	d->Ctx = Gtk::gtk_drag_begin(d->SignalWnd,
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
   			OnDragInit(Status);
		}
		else
		{
			LgiTrace("%s:%i - No view handle\n", _FL);
		}
	}
}


