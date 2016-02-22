#include <Path.h>
#include <string.h>

#include "Lgi.h"
#include "GToken.h"
#include "GPopup.h"

#define DEBUG_SETFOCUS			0
#define DEBUG_HANDLEVIEWKEY		0

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

void GWnd::FrameMoved(BPoint origin)
{
	if (Notify)
	{
		Notify->Pos.Offset(	-Notify->Pos.x1 + origin.x,
							-Notify->Pos.y1 + origin.y);
	}
	else printf("%s:%i - Error: no notify.\n", _FL);
	
	BWindow::FrameMoved(origin);
}

void GWnd::FrameResized(float width, float height)
{
	if (Notify)
	{
		Notify->Pos.Dimension(width, height);
		Notify->OnPosChange();
	}
	else printf("%s:%i - Error: no notify.\n", _FL);

	BWindow::FrameResized(width, height);
}

void GWnd::KeyDown(const char *bytes, int32 numBytes)
{
	if (Notify)
		Notify->_Key(bytes, numBytes, true);
}

void GWnd::KeyUp(const char *bytes, int32 numBytes)
{
	if (Notify)
		Notify->_Key(bytes, numBytes, false);
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
	GViewI *Focus;
	GArray<HookInfo> Hooks;
	GKey LastKey;

	GWindowPrivate()
	{
		Focus = NULL;
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
	GView(new BViewRedir(this))
{
	d = new GWindowPrivate;
	_Window = this;
	_Default = 0;
	Menu = 0;

	Handle()->SetResizingMode(B_FOLLOW_ALL_SIDES);
	Handle()->SetViewColor(B_TRANSPARENT_COLOR);
	Handle()->SetFlags(Handle()->Flags() & ~B_NAVIGABLE);

	Wnd = new GWnd(this);
}

GWindow::~GWindow()
{
	DeleteObj(Menu);

	if (Wnd && Handle()->LockLooper())
	{
		if (Handle()->Window())
		{
			Handle()->RemoveSelf();
		}

		Wnd->Hide();

		((GWnd*)Wnd)->Notify = 0;
		Wnd->Quit();
		Wnd = NULL;
	}

	DeleteObj(d);
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	return GView::GetClient(ClientSpace);
}

void GWindow::Raise()
{
	if (Wnd && Wnd->Lock())
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
			Status = !Wnd->IsHidden();
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
					if (!Handle()->Parent())
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
				if (!Wnd->IsHidden())
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

bool GWindow::Name(const char *n)
{
	bool Status = GBase::Name(n);
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
	return GBase::Name();
}

GRect &GWindow::GetPos()
{
	static GRect r;
	
	r = Pos;
	
	if (Wnd && Wnd->Lock())
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
		
		if (!Handle()->Parent())
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
	
	if (!SafeToLock)
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
			if (!w->Visible())
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

GMessage::Result GWindow::OnEvent(GMessage *Msg)
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

bool GWindow::SetIcon(char const *IcoFile)
{
	return false;
}

int GWindow::OnDrop(char *Format, GVariant *Data, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;

	if (Format && Data)
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
				for (int n=0; n<Uri.Length(); n++)
				{
					char *File = Uri[n];
					if (strnicmp(File, "file:", 5) == 0)
						File += 5;
					
					char *i = File, *o = File;
					while (*i)
					{
						if (i[0] == '%' &&
							i[1] &&
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
			if (!d->Hooks[i].Target->OnViewMouse(v, m))
			{
				return false;
			}
		}
	}
	
	return true;
}

bool GWindow::HandleViewKey(GView *v, GKey &k)
{
	bool Status = false;
	GViewI *Ctrl = 0;
	
	#if DEBUG_HANDLEVIEWKEY
	bool Debug = 1; // k.vkey == VK_RETURN;
	char SafePrint = k.c16 < ' ' ? ' ' : k.c16;
	
	if (Debug)
	{
		printf("%s::HandleViewKey=%i ischar=%i %s%s%s%s\n",
			v->GetClass(),
			k.c16,
			k.IsChar,
			(char*)(k.Down()?" Down":" Up"),
			(char*)(k.Shift()?" Shift":""),
			(char*)(k.Alt()?" Alt":""),
			(char*)(k.Ctrl()?" Ctrl":""));
	}
	#endif

	// Any window in a popup always gets the key...
	GViewI *p;
	for (p = v->GetParent(); p; p = p->GetParent())
	{
		if (dynamic_cast<GPopup*>(p))
		{
			#if DEBUG_HANDLEVIEWKEY
			if (Debug)
				printf("Sending key to popup\n");
			#endif
			
			return v->OnKey(k);
		}
	}

	// Give key to popups
	if (LgiApp &&
		LgiApp->GetMouseHook() &&
		LgiApp->GetMouseHook()->OnViewKey(v, k))
	{
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			printf("MouseHook got key\n");
		#endif
		goto AllDone;
	}

	// Allow any hooks to see the key...
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GKeyEvents)
		{
			if (d->Hooks[i].Target->OnViewKey(v, k))
			{
				Status = true;
				
				#if DEBUG_HANDLEVIEWKEY
				if (Debug)
					printf("Hook ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n", SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
				#endif
				
				goto AllDone;
			}
		}
	}
	
	// Does the key reference a menu shortcut?
	// Some of the shortcuts in Haiku are not supported by the system. So we
	// have to hook the key here and match it against the unsupported shortcuts.
	if (Menu)
	{
		if (Menu->OnKey(v, k))
		{
			Status = true;
			goto AllDone;
		}
	}

	// Give the key to the window...
	if (v->OnKey(k))
	{
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			printf("%s ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
				v->GetClass(),
				SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
		#endif
		
		Status = true;
		goto AllDone;
	}
	#if DEBUG_HANDLEVIEWKEY
	else if (Debug)
		printf("%s didn't eat '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
			v->GetClass(), SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
	#endif
	
	// Window didn't want the key...
	switch (k.vkey)
	{
		case VK_RETURN:
		#ifdef VK_KP_ENTER
		case VK_KP_ENTER:
		#endif
		{
			Ctrl = _Default;
			break;
		}
		case VK_ESCAPE:
		{
			Ctrl = FindControl(IDCANCEL);
			break;
		}
	}

	// printf("Ctrl=%p\n", Ctrl);
	if (Ctrl)
	{
		if (Ctrl->Enabled())
		{
			if (Ctrl->OnKey(k))
			{
				Status = true;

				#if DEBUG_HANDLEVIEWKEY
				if (Debug)
					printf("Default Button ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
						SafePrint, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
				#endif
				
				goto AllDone;
			}
			// else printf("OnKey()=false\n");
		}
		// else printf("Ctrl=disabled\n");
	}
	#if DEBUG_HANDLEVIEWKEY
	else if (Debug)
		printf("No default ctrl to handle key.\n");
	#endif
	
	if (Menu)
	{
		Status = Menu->OnKey(v, k);
		if (Status)
		{
			#if DEBUG_HANDLEVIEWKEY
			if (Debug)
				printf("Menu ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
		}
	}
	
	// Tab through controls
	if (k.vkey == VK_TAB && k.Down() && !k.IsChar)
	{
		GViewI *Wnd = GetNextTabStop(v, k.Shift());
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			printf("Tab moving focus shift=%i Wnd=%p\n", k.Shift(), Wnd);
		#endif
		if (Wnd)
			Wnd->Focus(true);
	}

AllDone:
	d->LastKey = k;

	return Status;
}


bool GWindow::RegisterHook(GView *Target, GWindowHookType EventType, int Priority)
{
	bool Status = false;
	
	if (Target && EventType)
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

bool GWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	return false;
}

void GWindow::OnTrayClick(GMouse &m)
{
}

bool GWindow::IsActive()
{
	return Wnd ? Wnd->IsActive() : false;
}

GViewI *GWindow::GetFocus()
{
	return d->Focus;
}

void GWindow::OnFrontSwitch(bool b)
{
}

#if DEBUG_SETFOCUS
static GAutoString DescribeView(GViewI *v)
{
	if (!v)
		return GAutoString(NewStr("NULL"));

	char s[512];
	int ch = 0;
	::GArray<GViewI*> p;
	for (GViewI *i = v; i; i = i->GetParent())
	{
		p.Add(i);
	}
	for (int n=min(3, p.Length()-1); n>=0; n--)
	{
		v = p[n];
		ch += sprintf_s(s + ch, sizeof(s) - ch, ">%s", v->GetClass());
	}
	return GAutoString(NewStr(s));
}
#endif

void GWindow::SetFocus(GViewI *ctrl, FocusType type)
{
	#if DEBUG_SETFOCUS
	const char *TypeName = NULL;
	switch (type)
	{
		case GainFocus: TypeName = "Gain"; break;
		case LoseFocus: TypeName = "Lose"; break;
		case ViewDelete: TypeName = "Delete"; break;
	}
	#endif

	switch (type)
	{
		case GainFocus:
		{
			if (d->Focus == ctrl)
			{
				#if DEBUG_SETFOCUS
				GAutoString _ctrl = DescribeView(ctrl);
				LgiTrace("SetFocus(%s, %s) already has focus.\n", _ctrl.Get(), TypeName);
				#endif
				return;
			}

			if (d->Focus)
			{
				GView *gv = d->Focus->GetGView();
				if (gv)
				{
					#if DEBUG_SETFOCUS
					GAutoString _foc = DescribeView(d->Focus);
					LgiTrace(".....defocus GView: %s\n", _foc.Get());
					#endif
					gv->_Focus(false);
				}
				else if (IsActive())
				{
					#if DEBUG_SETFOCUS
					GAutoString _foc = DescribeView(d->Focus);
					LgiTrace(".....defocus view: %s (active=%i)\n", _foc.Get(), IsActive());
					#endif
					d->Focus->OnFocus(false);
					d->Focus->Invalidate();
				}
			}

			d->Focus = ctrl;

			if (d->Focus)
			{
				GView *gv = d->Focus->GetGView();
				if (gv)
				{
					#if DEBUG_SETFOCUS
					GAutoString _set = DescribeView(d->Focus);
					LgiTrace("GWindow::SetFocus(%s, %s) focusing GView %p\n",
						_set.Get(),
						TypeName,
						d->Focus->Handle());
					#endif

					gv->_Focus(true);
				}
				else if (IsActive())
				{			
					#if DEBUG_SETFOCUS
					GAutoString _set = DescribeView(d->Focus);
					LgiTrace("GWindow::SetFocus(%s, %s) focusing nonGView %p (active=%i)\n",
						_set.Get(),
						TypeName,
						d->Focus->Handle(),
						IsActive());
					#endif

					d->Focus->OnFocus(true);
					d->Focus->Invalidate();
				}
			}
			break;
		}
		case LoseFocus:
		{
			#if DEBUG_SETFOCUS
			GAutoString _Ctrl = DescribeView(d->Focus);
			GAutoString _Focus = DescribeView(d->Focus);
			LgiTrace("GWindow::SetFocus(%s, %s) d->Focus=%s\n",
				_Ctrl.Get(),
				TypeName,
				_Focus.Get());
			#endif
			if (ctrl == d->Focus)
			{
				d->Focus = NULL;
			}
			break;
		}
		case ViewDelete:
		{
			if (ctrl == d->Focus)
			{
				#if DEBUG_SETFOCUS
				GAutoString _Ctrl = DescribeView(d->Focus);
				LgiTrace("GWindow::SetFocus(%s, %s) on delete\n",
					_Ctrl.Get(),
					TypeName);
				#endif
				d->Focus = NULL;
			}
			break;
		}
	}
}
