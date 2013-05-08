#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "GPopup.h"
#include "GPanel.h"

using namespace Gtk;
#include "LgiWidget.h"

///////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	int Flags;
	GView *Target;
};

class GWindowPrivate
{
public:
	int Sx, Sy;
	bool Dynamic;
	GKey LastKey;
	::GArray<HookInfo> Hooks;
	bool SnapToEdge;
	
	// Focus stuff
	OsView FirstFocus;
	GViewI *Focus;
	bool Active;

	GWindowPrivate()
	{
		FirstFocus = NULL;
		Focus = NULL;
		Active = false;
		
		Sx = Sy = 0;
		Dynamic = true;
		SnapToEdge = false;
		ZeroObj(LastKey);		
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

///////////////////////////////////////////////////////////////////////
#define GWND_CREATE		0x0010000

GWindow::GWindow(GtkWidget *w) : GView(0)
{
	d = new GWindowPrivate;
	_QuitOnClose = false;
	Menu = NULL;
	Wnd = GTK_WINDOW(w);
	_Root = NULL;
	_MenuBar = NULL;
	_VBox = NULL;
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	ClearFlag(WndFlags, GWF_VISIBLE);

    _Lock = new ::GMutex;
}

GWindow::~GWindow()
{
	if (LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

    if (_Root)
    {
        _Root = NULL;
    }

	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

bool GWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void GWindow::SetSnapToEdge(bool s)
{
	d->SnapToEdge = s;
}

bool GWindow::IsActive()
{
	return d->Active;
}

bool GWindow::Visible()
{
	return GView::Visible();
}

void GWindow::Visible(bool i)
{
	if (i)
		gtk_widget_show(GTK_WIDGET(Wnd));
	else
		gtk_widget_hide(GTK_WIDGET(Wnd));
}

bool GWindow::Obscured()
{
	return false;
}

void GWindow::_SetDynamic(bool i)
{
	d->Dynamic = i;
}

void GWindow::_OnViewDelete()
{
	if (d->Dynamic)
	{
		delete this;
	}
}

void GWindow::OnGtkDelete()
{
	// Delete everything we own...
	DeleteObj(Menu);
	while (Children.Length())
	{
		GViewI *c = Children.First();
		c->Detach();
	}
	
	// These will be destroyed by GTK after returning from GWindowCallback
	Wnd = NULL;
	_View = NULL;
}

static
gboolean
GWindowCallback(GtkWidget   *widget,
				GdkEvent    *event,
				GWindow     *This)
{
	if (!event)
	{
		printf("%s:%i - No event %i\n", _FL);
		return FALSE;
	}

	switch (event->type)
	{
		case GDK_DELETE:
		{
			bool Close = This->OnRequestClose(false);
			if (Close)
			{
				This->OnGtkDelete();
				delete This;
			}
			return !Close;
		}
		case GDK_DESTROY:
		{
			delete This;
			return TRUE;
		}
		case GDK_CONFIGURE:
		{
			GdkEventConfigure *c = (GdkEventConfigure*)event;
			This->Pos.Set(c->x, c->y, c->x+c->width-1, c->y+c->height-1);
			This->OnPosChange();
			return FALSE;
			break;
		}
		case GDK_FOCUS_CHANGE:
		{
			char buf[1024];
			int ch = 0;
			::GArray<GViewI*> a;
			for (GViewI *i = This; i; i = i->GetParent())
			{
				a.Add(i);
			}
			for (int n=a.Length()-1; n>=0; n--)
			{
				ch += sprintf_s(buf + ch, sizeof(buf) - ch, "> %s \"%-.8s\" ", a[n]->GetClass(), a[n]->Name());
			}
			LgiTrace("%s : gwnd_focus=%i\n", buf, event->focus_change.in);
			
			This->d->Active = event->focus_change.in;
			break;
		}
		case GDK_CLIENT_EVENT:
		{
			GMessage m(event);
			This->OnEvent(&m);
			break;
		}
		default:
		{
			printf("%s:%i - Unknown event %i\n", _FL, event->type);
			break;
		}
	}
	
	return TRUE;
}

static gboolean GWindowClientEvent(GtkWidget *widget, GdkEventClient *ev, GWindow *Wnd)
{
	GMessage m(ev->data.l[0], ev->data.l[1], ev->data.l[2]);
    Wnd->OnEvent(&m);
	return FALSE;
}

bool GWindow::Attach(GViewI *p)
{
	bool Status = false;
	
	if (!Wnd)
		Wnd = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	if (Wnd)
	{
		_View = GTK_WIDGET(Wnd);
		
		g_signal_connect(	G_OBJECT(Wnd),
							"delete_event",
							G_CALLBACK(GWindowCallback),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"destroy",
							G_CALLBACK(GWindowCallback),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"configure-event",
							G_CALLBACK(GWindowCallback),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"button-press-event",
							G_CALLBACK(GWindowCallback),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"focus-in-event",
							G_CALLBACK(GWindowCallback),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"focus-out-event",
							G_CALLBACK(GWindowCallback),
							this);
		g_signal_connect(	G_OBJECT(Wnd),
							"client-event",
							G_CALLBACK(GWindowClientEvent),
							this);

		gtk_window_set_default_size(GTK_WINDOW(Wnd), Pos.X(), Pos.Y());
		gtk_widget_add_events(GTK_WIDGET(Wnd), GDK_ALL_EVENTS_MASK);
		
        if (_Root = lgi_widget_new(this, Pos.X(), Pos.Y(), true))
        {
            gtk_container_add(GTK_CONTAINER(Wnd), _Root);
            gtk_widget_show(_Root);
            printf("Root window for %p is %p\n", _View, _Root);
        }
		
		OnCreate();
		Pour();
		
		// Setup default button...
		if (!_Default)
		{
			_Default = FindControl(IDOK);
		}
		if (_Default)
		{
			_Default->Invalidate();
		}
		
		Status = true;
	}
	
	return Status;
}

bool GWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LgiCloseApp();
	}

	return GView::OnRequestClose(OsShuttingDown);
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

#define DEBUG_KEYS			0

/*
	// Any window in a popup always gets the key...
	for (GView *p = v; p; p = p->GetParent())
	{
		GPopup *Popup;
		if (Popup = dynamic_cast<GPopup*>(p))
		{
			Status = v->OnKey(k);
			if (!Status)
			{
				if (k.c16 == VK_ESCAPE)
				{
					// Popup window (or child) didn't handle the key, and the user
					// pressed ESC, so do the default thing and close the popup.
					Popup->Cancelled = true;
					Popup->Visible(false);
				}
				else
				{
					#if DEBUG_KEYS
					printf("Popup ignores '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
					#endif
					break;
				}
			}
			
			#if DEBUG_KEYS
			printf("Popup ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
			
			goto AllDone;
		}
	}
*/

#define DEBUG_HANDLEVIEWKEY		0

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

	// Give the key to the window...
	if (v->OnKey(k))
	{
		#if DEBUG_HANDLEVIEWKEY
		if (Debug)
			printf("View ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
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


void GWindow::Raise()
{
	/*
	if (_View->handle())
	{
		#if defined XWIN
		XWindowChanges c;
		c.stack_mode = XAbove;
		XConfigureWindow(	_View->XDisplay(),
							_View->handle(),
							CWStackMode,
							&c);
		#endif							
	}
	*/
}


GWindowZoom GWindow::GetZoom()
{
	/*
	#if defined XWIN
	if (Handle())
	{
		Display *Dsp = Handle()->XDisplay();
		Window Win = Handle()->handle();
		Atom Type;
		int Format;
		ulong Items, Bytes;
		uchar *Prop = 0;

		XGetWindowProperty(	Dsp,
							Win,
							XInternAtom(Dsp, "_NET_WM_STATE", false),
							0, 1024,
							false,
							AnyPropertyType,
							&Type,
							&Format,
							&Items,
							&Bytes,
							&Prop);
		if (Prop)
		{
			Atom s[] =
			{
				XInternAtom(Dsp, "_NET_WM_STATE_MAX_V", false),
				XInternAtom(Dsp, "_NET_WM_STATE_MAX_H", false)
			};

			for (int i=0; i<Items; i++)
			{
				Atom a = ((Atom*)Prop)[i];
				if (a == s[0] OR a == s[1])
				{
					XFree(Prop);
					return GZoomMax;
				}
			}

			XFree(Prop);
		}

		XWindowAttributes a;
		ZeroObj(a);
		XGetWindowAttributes(Dsp, Win, &a);
		
		if (a.map_state == IsUnmapped)
		{
			return GZoomMin;
		}
	}
	#endif
	*/

	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
	/*
	#if defined XWIN
	Display *Dsp = Handle()->XDisplay();
	Window Win = Handle()->handle();

	switch (i)
	{
		case GZoomMax:
		{
			//_NET_WM_STATE_MAX_V
			//_NET_WM_STATE_MAX_H
		    //_NET_WM_STATE_STAYS_ON_TOP
            //_NET_WM_STATE_MODAL

			Atom s[] =
			{
				XInternAtom(Dsp, "_NET_WM_STATE_MAX_V", false),
				XInternAtom(Dsp, "_NET_WM_STATE_MAX_H", false)
			};

		    XChangeProperty(Dsp,
							Win,
							XInternAtom(Dsp, "_NET_WM_STATE", false),
							XA_ATOM,
							32,
							PropModeReplace,
							(unsigned char *) s,
							CountOf(s));
			break;
		}
		case GZoomMin:
		{
			XIconifyWindow(Dsp, Win, DefaultScreen(Dsp) );
			break;
		}
		case GZoomNormal:
		{
		    XChangeProperty(Dsp,
							Win,
							XInternAtom(Dsp, "_NET_WM_STATE", false),
							XA_ATOM,
							32,
							PropModeReplace,
							0,
							0);

			XMapRaised(Dsp, Win);
			break;
		}
	}
	#endif
	*/
}

GViewI *GWindow::GetDefault()
{
	return _Default;
}

void GWindow::SetDefault(GViewI *v)
{
	if (v &&
		v->GetWindow() == this)
	{
		if (_Default != v)
		{
			GViewI *Old = _Default;
			_Default = v;

			if (Old) Old->Invalidate();
			if (_Default) _Default->Invalidate();
		}
	}
	else
	{
		_Default = 0;
	}
}

bool GWindow::Name(const char *n)
{
	return GBase::Name(n);
}

char *GWindow::Name()
{
	return GBase::Name();
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	static GRect r;
	
	r = GView::GetClient(ClientSpace);
	/*
	if (Menu &&
		Menu->Handle())
	{
		GRect p = Menu->GetPos();
		r.y1 = p.y2 + 1;
	}
	*/
	
	return r;
}

bool GWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;

	if (Load)
	{
		::GVariant v;
		if (Store->GetValue(FieldName, v) && v.Str())
		{
			GRect Position(0, 0, -1, -1);
			GWindowZoom State = GZoomNormal;

			GToken t(v.Str(), ";");
			for (int i=0; i<t.Length(); i++)
			{
				char *Var = t[i];
				char *Value = strchr(Var, '=');
				if (Value)
				{
					*Value++ = 0;

					if (stricmp(Var, "State") == 0)
						State = (GWindowZoom) atoi(Value);
					else if (stricmp(Var, "Pos") == 0)
						Position.SetStr(Value);
				}
				else return false;
			}
			
			if (Position.Valid())
			{
				SetPos(Position);
			}
			
			SetZoom(State);
		}
		else return false;
	}
	else
	{
		char s[256];
		GWindowZoom State = GetZoom();
		sprintf(s, "State=%i;Pos=%s", State, GetPos().GetStr());

		::GVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}

	return true;
}

GRect &GWindow::GetPos()
{
	if (Wnd && _View)
	{
		/*
		OsRect r = _View->geometry();

		Pos.x1 = r.left;
		Pos.y1 = r.top;
		Pos.x2 = r.right;
		Pos.y2 = r.bottom;

		#if defined XWIN
		XWindowAttributes a;
		ZeroObj(a);
		XGetWindowAttributes(Handle()->XDisplay(), Handle()->handle(), &a);
		if (!a.override_redirect)
		{
			int Dx, Dy;
			_View->GetDecorationSize(Dx, Dy);
			Pos.Offset(Dx, Dy);
		}
		#endif
		*/
	}

	return Pos;
}

bool GWindow::SetPos(GRect &p, bool Repaint)
{
	Pos = p;
	if (Wnd)
	{
		gtk_window_set_default_size(GTK_WINDOW(Wnd), Pos.X(), Pos.Y());
	}
	return true;
}

void GWindow::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	// Force repour
	d->Sx = d->Sy = -1;
}

void GWindow::OnCreate()
{
}

void GWindow::_Paint(GSurface *pDC, int Ox, int Oy)
{
	GRect r = GetClient();
	GView::_Paint(pDC, r.x1, r.y1);
}

void GWindow::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

void GWindow::OnPosChange()
{
	GView::OnPosChange();

	if (d->Sx != X() ||	d->Sy != Y())
	{
		Pour();
		d->Sx = X();
		d->Sy = Y();
	}
}

#define IsTool(v) \
	( \
		dynamic_cast<GView*>(v) \
		&& \
		dynamic_cast<GView*>(v)->_IsToolBar \
	)

void GWindow::Pour()
{
	GRegion Client(GetClient());
	GViewI *MenuView = 0;

	/*
	if (Menu &&
		Menu->Handle() &&
		Menu->Handle()->View())
	{
		MenuView = Menu->Handle()->View();
		MenuView->Pour(Client);
		
		GRect *r = &MenuView->GetPos();
		Client.Subtract(r);
		// printf("Menu=%p MenuPos=%s\n", MenuView, r->GetStr());
	}
	else if (Menu)
	{
		printf("%s:%i - Menu has no view!\n", __FILE__, __LINE__);
	}
	*/
	
	GRegion Update(Client);
	bool HasTools = false;
	GViewI *v;
	List<GViewI>::I Lst = Children.Start();

	{
		GRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			bool IsMenu = MenuView == v;
			if (!IsMenu && IsTool(v))
			{
				GRect OldPos = v->GetPos();
				Update.Union(&OldPos);
				
				if (HasTools)
				{
					// 2nd and later toolbars
					if (v->Pour(Tools))
					{
						if (!v->Visible())
						{
							v->Visible(true);
						}

						if (OldPos != v->GetPos())
						{
							// position has changed update...
							v->Invalidate();
						}

						Tools.Subtract(&v->GetPos());
						Update.Subtract(&v->GetPos());
					}
				}
				else
				{
					// First toolbar
					if (v->Pour(Client))
					{
						HasTools = true;

						if (!v->Visible())
						{
							v->Visible(true);
						}

						if (OldPos != v->GetPos())
						{
							v->Invalidate();
						}

						GRect Bar(v->GetPos());
						Bar.x2 = GetClient().x2;

						Tools = Bar;
						Tools.Subtract(&v->GetPos());
						Client.Subtract(&Bar);
						Update.Subtract(&Bar);
					}
				}
			}
		}
	}

	Lst = Children.Start();
	for (GViewI *v = *Lst; v; v = *++Lst)
	{
		bool IsMenu = MenuView == v;
		if (!IsMenu && !IsTool(v))
		{
			GRect OldPos = v->GetPos();
			Update.Union(&OldPos);

			if (v->Pour(Client))
			{
				GRect p = v->GetPos();

				if (!v->Visible())
				{
					v->Visible(true);
				}

				v->Invalidate();

				Client.Subtract(&v->GetPos());
				Update.Subtract(&v->GetPos());
			}
			else
			{
				// non-pourable
			}
		}
	}
	
	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}
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

int GWindow::OnDrop(char *Format, ::GVariant *Data, GdcPt2 Pt, int KeyState)
{
	int Status = DROPEFFECT_NONE;

	if (Format && Data)
	{
		if (stricmp(Format, LGI_FileDropFormat) == 0)
		{
			::GArray<char*> Files;
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
					
					char *in = File, *out = File;
					while (*in)
					{
						if (in[0] == '%' &&
							in[1] &&
							in[2])
						{
							char h[3] = { in[1], in[2], 0 };
							*out++ = htoi(h);
							in += 3;
						}
						else
						{
							*out++ = *in++;
						}
					}
					*out++ = 0;
					
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

GMessage::Param GWindow::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_CLOSE:
		{
			if (OnRequestClose(false))
			{
				Quit();
				return 0;
			}
			break;
		}
	}

	return GView::OnEvent(m);
}

bool GWindow::RegisterHook(GView *Target, int EventType, int Priority)
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

GViewI *GWindow::GetFocus()
{
	return d->Focus;
}

void GWindow::SetFocus(GViewI *ctrl, bool ondelete)
{
	if (d->Focus == ctrl)
		return;

	if (d->Focus)
	{
		GView *gv = d->Focus->GetGView();
		if (gv)
		{
			gv->_Focus(false);
		}
		else if (IsActive())
		{
			d->Focus->OnFocus(false);
			d->Focus->Invalidate();
		}
	}
	d->Focus = ctrl;
	if (d->Focus)
	{
		if (d->Focus->Handle())
			gtk_widget_grab_focus(d->Focus->Handle());

		GView *gv = d->Focus->GetGView();
		if (gv)
		{
			gv->_Focus(true);
		}
		else if (IsActive())
		{			
			d->Focus->OnFocus(true);
			d->Focus->Invalidate();
		}
	}
}

void GWindow::SetDragHandlers(bool On)
{
}

void GWindow::OnMap(bool m)
{
}

void GWindow::OnTrayClick(GMouse &m)
{
	if (m.Down() || m.IsContextMenu())
	{
		GSubMenu RClick;
		OnTrayMenu(RClick);
		if (GetMouse(m, true))
		{
			#if WIN32NATIVE
			SetForegroundWindow(Handle());
			#endif
			int Result = RClick.Float(this, m.x, m.y);
			#if WIN32NATIVE
			PostMessage(Handle(), WM_NULL, 0, 0);
			#endif
			OnTrayMenuResult(Result);
		}
	}
}
