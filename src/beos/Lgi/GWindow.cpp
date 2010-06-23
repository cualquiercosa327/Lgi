#include <Path.h>
#include <string.h>

#include "Lgi.h"
#include "GToken.h"
#include "GPopup.h"

///////////////////////////////////////////////////////////////////////////////////////////////
GWnd::GWnd(GView *notify) :
	BWindow(BRect(10, 10, 100, 100),
			"",
			B_TITLED_WINDOW,
			B_ASYNCHRONOUS_CONTROLS | B_WILL_ACCEPT_FIRST_CLICK,
			B_CURRENT_WORKSPACE)
{
	Notify = notify;
}

GWnd::GWnd(	GView *notify,
			BRect frame,
			char *title,
			window_type type,
			uint32 flags,
			uint32 workspaces)
	: BWindow(frame, title, type, flags, workspaces)		
{
	Notify = notify;
}

GWnd::~GWnd()
{
}

bool GWnd::QuitRequested()
{
	return Notify->OnRequestClose(false);
}

void GWnd::MessageReceived(BMessage *Msg)
{
	BWindow::MessageReceived(Msg);
}

void GWnd::FrameMoved(BPoint origin)
{
	if (Notify)
	{
		Notify->Pos.Offset(	-Notify->Pos.x1 + origin.x,
							-Notify->Pos.y1 + origin.y);
	}
}

void GWnd::FrameResized(float width, float height)
{
	if (Notify)
	{
		Notify->Pos.Dimension(width, height);
		Notify->OnPosChange();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	int Flags;
	GView *Target;
};

class GWindowPrivate
{
public:
	GArray<HookInfo> Hooks;

	GWindowPrivate()
	{
	}

	int GetHookIndex(GView *Target, bool Create = false)
	{
		for (int i=0; i<Hooks.Length(); i++)
		{
			if (Hooks[i].Target == Target) return i;
		}
		
		if (Create)
		{
			HookInfo *n = &Hooks[Hooks.Length()];
			if (n)
			{
				n->Target = Target;
				n->Flags = 0;
				return Hooks.Length() - 1;
			}
		}
		
		return -1;
	}
};

GWindow::GWindow() :
	GView(NEW(BViewRedir(this)))
{
	d = NEW(GWindowPrivate);
	_Window = this;
	_Default = 0;
	Menu = 0;

	Handle()->SetResizingMode(B_FOLLOW_ALL_SIDES);
	Handle()->SetViewColor(B_TRANSPARENT_COLOR);
	Handle()->SetFlags(Handle()->Flags() & ~B_NAVIGABLE);

	Wnd = NEW(GWnd(this));
}

GWindow::~GWindow()
{
	DeleteObj(Menu);

	if (Handle()->LockLooper())
	{
		if (Handle()->Window())
		{
			Handle()->RemoveSelf();
		}

		Wnd->Hide();

		((GWnd*)Wnd)->Notify = 0;
		_QuitMe = Wnd;
	}

	DeleteObj(d);
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	return GView::GetClient(ClientSpace);
}

void GWindow::Raise()
{
	if (Wnd AND Wnd->Lock())
	{
		Wnd->Activate();
		Wnd->Unlock();
	}
}

bool GWindow::Attach(GViewI *Parent)
{
	return true;
}

void GWindow::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

void GWindow::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
}

bool GWindow::OnRequestClose(bool Os)
{
	if (GetQuitOnClose())
	{
		LgiCloseApp();
	}

	delete this;

	return true;
}

void GWindow::OnPosChange()
{
	GetPos();
	Pour();
}

void GWindow::OnCreate()
{
}

bool GWindow::Visible()
{
	bool Status = FALSE;
	if (Wnd)
	{
		if (Wnd->Lock())
		{
			Status = NOT Wnd->IsHidden();
			Wnd->Unlock();
		}
	}
	return Status;
}

void GWindow::Visible(bool v)
{
	if (Wnd)
	{
		if (Wnd->Lock())
		{
			if (v)
			{
				if (Wnd->IsHidden())
				{
					// Add BView here, to make sure "OnCreate" is called at the end
					// of a GWindow constructor not at the beginning, where it's not
					// useful.
					if (NOT Handle()->Parent())
					{
						// Set pos
						BRect r = Wnd->Bounds();
						Handle()->MoveTo(0, 0);
						Handle()->ResizeTo(r.Width(), r.Height());

						// Insert view
						Wnd->AddChild(Handle());
					}
					
					Wnd->Show();
					Invalidate((GRect*)0, true);
				}
			}
			else
			{
				if (NOT Wnd->IsHidden())
				{
					Wnd->Hide();
				}
			}
			Wnd->Unlock();
		}
	}

	if (v)
	{
		Pour();
	}
}

bool GWindow::Name(char *n)
{
	bool Status = GObject::Name(n);
	if (Wnd)
	{
		bool Lock = Wnd->Lock();
		Wnd->SetTitle(n);
		if (Lock) Wnd->Unlock();
	}
	return Status;
}

char *GWindow::Name()
{
	return GObject::Name();
}

GRect &GWindow::GetPos()
{
	static GRect r;
	
	r = Pos;
	
	if (Wnd AND Wnd->Lock())
	{
		BRect frame = Wnd->Frame();
		r.x1 = frame.left;
		r.y1 = frame.top;
		r.x2 = frame.right;
		r.y2 = frame.bottom;
		Pos = frame;

		Wnd->Unlock();
	}
	
	return r;
}

bool GWindow::SetPos(GRect &p, bool Repaint)
{
	if (Wnd)
	{
		if (p.x1 < -100)
		{
			printf("%s:%i - Weird setpos(%s)\n", __FILE__, __LINE__, p.GetStr());
		}
		
		bool Lock = Wnd->Lock();

		Wnd->MoveTo(p.x1, p.y1);
		Wnd->ResizeTo(p.X(), p.Y());
		
		if (NOT Handle()->Parent())
		{
			// Our view is not attached yet, so move it ourself
			BRect r = Wnd->Bounds();
			Handle()->MoveTo(0, 0);
			Handle()->ResizeTo(r.Width(), r.Height());
		}

		if (Lock) Wnd->Unlock();

		Pos = p;

		return true;
	}

	return false;
}

void GWindow::Pour()
{
	bool SafeToLock = false;

	if (Wnd->IsLocked())
	{
		// I'm already locked... this could be bad if it's not me
		thread_id Thread = WindowHandle()->LockingThread();
		if (Thread != -1)
		{
			if (Thread == find_thread(NULL))
			{
				// it's all ok, I'm locking me
				SafeToLock = true;
			}
			else
			{
				// someone else is locking us
				// ok who is locking me?
				thread_info Info;
				if (get_thread_info(Thread, &Info) == B_OK)
				{
					printf("Evil locking thread: %i (%s)\n", Thread, Info.name);
				}
				else
				{
					printf("Couldn't get evil thread info\n");
				}
			}
		}
	}
	else
	{
		SafeToLock = true;
	}
	
	if (NOT SafeToLock)
	{
		printf("%s:%i - Not safe to lock for ::Pour.\n", __FILE__, __LINE__);
		return;
	}
	
	bool Lock = Wnd->Lock();
	Wnd->BeginViewTransaction();
	GRect r(Handle()->Frame());
	r.Offset(-r.x1, -r.y1);

	GRegion Client(r);
	GRegion Update;

	if (Menu)
	{
		GRect Mp = Menu->GetPos();
		Mp.x2 = 10000;
		Client.Subtract(&Mp);
	}

	for (GViewI *w = Children.First(); w; w = Children.Next())
	{
		GRect OldPos = w->GetPos();
		Update.Union(&OldPos);

		if (w->Pour(Client))
		{
			if (NOT w->Visible())
			{
				w->Visible(true);
			}

			Client.Subtract(&w->GetPos());
			Update.Subtract(&w->GetPos());
		}
		else
		{
			// non-pourable
		}
	}

	Wnd->EndViewTransaction();
	Wnd->Sync();

	// Handle()->Invalidate();
	if (Lock) Wnd->Unlock();
}

int GWindow::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		case B_SIMPLE_DATA:
		{
			GArray<char*> Files;
			int32 Count = 0;
			type_code Type = 0;
			
			if (Msg->GetInfo("refs", &Type, &Count) == B_OK)
			{
				for (int i=0; i<Count; i++)
				{
					entry_ref Ref;
					if (Msg->FindRef("refs", i, &Ref) == B_OK)
					{
						BPath Path("");
						BEntry Entry(&Ref, true);
						Entry.GetPath(&Path);
						char *p = (char*) Path.Path();
						if (p)
						{
							Files.Add(NewStr(p));
						}
					}
				}
			}
			
			if (Files.Length() > 0)
			{
				OnReceiveFiles(Files);
			}
			
			Files.DeleteArrays();
			break;
		}
		case M_COMMAND:
		{
			int32 Cmd = 0;
			int32 Event = 0;
			Msg->FindInt32("Cmd", &Cmd);
			Msg->FindInt32("Event", &Event);
			OnCommand(Cmd, Event, 0);
			break;
		}
	}

	return GView::OnEvent(Msg);
}

int GWindow::WillAccept(List<char> &Formats, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;
	
	for (char *f=Formats.First(); f; )
	{
		if (stricmp(f, LGI_FileDropFormat) == 0)
		{
			f = Formats.Next();
			Status = DROPEFFECT_COPY;
		}
		else
		{
			Formats.Delete(f);
			DeleteArray(f);
			f = Formats.Current();
		}
	}
	
	return Status;
}

int GWindow::OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;

	if (Format AND Data)
	{
		if (stricmp(Format, LGI_FileDropFormat) == 0)
		{
			GArray<char*> Files;
			if (Data->IsBinary())
			{
				GToken Uri(	(char*)Data->Value.Binary.Data,
							"\r\n,",
							true,
							Data->Value.Binary.Length);
				for (int i=0; i<Uri.Length(); i++)
				{
					char *File = Uri[i];
					if (strnicmp(File, "file:", 5) == 0)
						File += 5;
					
					char *i = File, *o = File;
					while (*i)
					{
						if (i[0] == '%' AND
							i[1] AND
							i[2])
						{
							char h[3] = { i[1], i[2], 0 };
							*o++ = htoi(h);
							i += 3;
						}
						else
						{
							*o++ = *i++;
						}
					}
					*o++ = 0;
					
					if (FileExists(File))
					{
						Files.Add(NewStr(File));
					}
				}
			}
			
			if (Files.Length())
			{
				Status = DROPEFFECT_COPY;
				OnReceiveFiles(Files);
				Files.DeleteArrays();
			}
		}
	}
	
	return Status;
}

bool GWindow::HandleViewMouse(GView *v, GMouse &m)
{
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GMouseEvents)
		{
			if (NOT d->Hooks[i].Target->OnViewMouse(v, m))
			{
				return false;
			}
		}
	}
	
	return true;
}

bool GWindow::HandleViewKey(GView *v, GKey &k)
{
	// printf("GWindow::OnViewKey(%p) key=%i down=%i sh=%i alt=%i ctrl=%i\n", v, k.c, k.Down(), k.Shift(), k.Alt(), k.Ctrl());
	for (GViewI *p = v; p; p = p->GetParent())
	{
		if (dynamic_cast<GPopup*>(p))
		{
			return true;
		}
	}

	if (Menu)
	{
		return Menu->OnKey(v, k);
	}

	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GKeyEvents)
		{
			if (NOT d->Hooks[i].Target->OnViewKey(v, k))
			{
				return false;
			}
		}
	}

	return true;
}

bool GWindow::RegisterHook(GView *Target, int EventType, int Priority)
{
	bool Status = false;
	
	if (Target AND EventType)
	{
		int i = d->GetHookIndex(Target, true);
		if (i >= 0)
		{
			d->Hooks[i].Flags = EventType;
			Status = true;
		}
	}
	
	return Status;
}

bool GWindow::UnregisterHook(GView *Target)
{
	int i = d->GetHookIndex(Target);
	if (i >= 0)
	{
		d->Hooks.DeleteAt(i);
		return true;
	}
	return false;
}


