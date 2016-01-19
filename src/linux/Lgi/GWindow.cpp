#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "GPopup.h"
#include "GPanel.h"

using namespace Gtk;
#include "LgiWidget.h"

#define DEBUG_SETFOCUS			0
#define DEBUG_HANDLEVIEWKEY		0

///////////////////////////////////////////////////////////////////////
class HookInfo
{
public:
	GWindowHookType Flags;
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
	::GString Icon;
	
	// State
	GdkWindowState State;
	bool HadCreateEvent;
	
	// Focus stuff
	OsView FirstFocus;
	GViewI *Focus;
	bool Active;

	GWindowPrivate()
	{
		FirstFocus = NULL;
		Focus = NULL;
		Active = false;
		State = (Gtk::GdkWindowState)0;
		HadCreateEvent = false;
		
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
				n->Flags = GNoEvents;
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
		LgiApp->AppWnd = NULL;

    if (_Root)
        _Root = NULL;

	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

bool GWindow::SetIcon(const char *FileName)
{
	GAutoString a;
	if (Wnd)
	{
		if (!FileExists(FileName))
		{
			if (a.Reset(LgiFindFile(FileName)))
				FileName = a;
		}

		if (!FileExists(FileName))
		{
			LgiTrace("%s:%i - SetIcon failed to find '%s'\n", _FL);
			return false;
		}
		else
		{		
			GError *error = NULL;
			Gtk::GdkPixbuf *pixbuf = Gtk::gdk_pixbuf_new_from_file(FileName, &error);
			if (pixbuf)
			{
				#if 0
				printf("Calling gtk_window_set_icon with '%s' (%ix%i, %ich, %ibytes/line)\n",
					FileName,
					gdk_pixbuf_get_width(pixbuf),
					gdk_pixbuf_get_height(pixbuf),
					gdk_pixbuf_get_n_channels(pixbuf),
					gdk_pixbuf_get_rowstride(pixbuf));
				#endif
									
				Gtk::gtk_window_set_icon(Wnd, pixbuf);
			}
			else LgiTrace("%s:%i - gdk_pixbuf_new_from_file(%s) failed.\n", _FL, FileName);
		}
	}
	
	if (FileName != d->Icon.Get())
		d->Icon = FileName;

	return d->Icon != NULL;
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
	ThreadCheck();

	if (i)
		gtk_widget_show(GTK_WIDGET(Wnd));
	else
		gtk_widget_hide(GTK_WIDGET(Wnd));
}

bool GWindow::Obscured()
{
	return	d->State == GDK_WINDOW_STATE_WITHDRAWN ||
			d->State == GDK_WINDOW_STATE_ICONIFIED;
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
	// DeleteObj(Menu);

	#if 0
	while (Children.Length())
	{
		GViewI *c = Children.First();
		c->Detach();
	}
	#else
	for (unsigned i=0; i<Children.Length(); i++)
	{
		GViewI *c = Children[i];
		GView *v = c->GetGView();
		if (v)
			v->OnGtkDelete();
	}
	#endif
	
	// These will be destroyed by GTK after returning from GWindowCallback
	Wnd = NULL;
	_View = NULL;
}

gboolean GWindow::OnGtkEvent(GtkWidget *widget, GdkEvent *event)
{
	if (!event)
	{
		printf("%s:%i - No event %i\n", _FL);
		return FALSE;
	}

	#if 0
	if (event->type != 28)
		printf("%s::OnGtkEvent(%i) name=%s\n", GetClass(), event->type, Name());
	#endif
	switch (event->type)
	{
		case GDK_DELETE:
		{
			bool Close = OnRequestClose(false);
			if (Close)
				OnGtkDelete();     		
			return !Close;
		}
		case GDK_DESTROY:
		{
			delete this;
			return true;
		}
		case GDK_CONFIGURE:
		{
			GdkEventConfigure *c = (GdkEventConfigure*)event;
			Pos.Set(c->x, c->y, c->x+c->width-1, c->y+c->height-1);
			OnPosChange();
			return FALSE;
			break;
		}
		case GDK_FOCUS_CHANGE:
		{
			d->Active = event->focus_change.in;
			#if 0
			printf("%s/%s::GDK_FOCUS_CHANGE(%i)\n",
				GetClass(),
				Name(),
				event->focus_change.in);
			#endif
			break;
		}
		case GDK_WINDOW_STATE:
		{
			d->State = event->window_state.new_window_state;
			break;
		}
		case GDK_CLIENT_EVENT:
		{
			GMessage m(event);
			OnEvent(&m);
			break;
		}
		default:
		{
			printf("%s:%i - Unknown event %i\n", _FL, event->type);
			break;
		}
	}
	
	return true;
}

gboolean GtkViewDestroy(GtkWidget *widget, GView *This)
{
	delete This;
	return true;
}

bool GWindow::Attach(GViewI *p)
{
	bool Status = false;

	ThreadCheck();
	
	if (!Wnd)
		Wnd = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
	if (Wnd)
	{
		_View = GTK_WIDGET(Wnd);
		GView *i = this;
		
		g_signal_connect(	G_OBJECT(Wnd),
							"destroy",
							G_CALLBACK(GtkViewDestroy),
							i);
		g_signal_connect(	G_OBJECT(Wnd),
							"delete_event",
							G_CALLBACK(GtkViewCallback),
							i);
		g_signal_connect(	G_OBJECT(Wnd),
							"configure-event",
							G_CALLBACK(GtkViewCallback),
							i);
		g_signal_connect(	G_OBJECT(Wnd),
							"button-press-event",
							G_CALLBACK(GtkViewCallback),
							i);
		g_signal_connect(	G_OBJECT(Wnd),
							"focus-in-event",
							G_CALLBACK(GtkViewCallback),
							i);
		g_signal_connect(	G_OBJECT(Wnd),
							"focus-out-event",
							G_CALLBACK(GtkViewCallback),
							i);
		g_signal_connect(	G_OBJECT(Wnd),
							"client-event",
							G_CALLBACK(GtkViewCallback),
							i);
		g_signal_connect(	G_OBJECT(Wnd),
							"window-state-event",
							G_CALLBACK(GtkViewCallback),
							i);

		gtk_window_set_default_size(GTK_WINDOW(Wnd), Pos.X(), Pos.Y());
		gtk_widget_add_events(GTK_WIDGET(Wnd), GDK_ALL_EVENTS_MASK);
		gtk_window_set_title(Wnd, GBase::Name());
		
        if (_Root = lgi_widget_new(this, Pos.X(), Pos.Y(), true))
        {
            gtk_container_add(GTK_CONTAINER(Wnd), _Root);
            gtk_widget_show(_Root);
        }

		// This call sets up the GdkWindow handle
		gtk_widget_realize(GTK_WIDGET(Wnd));
		
		// Do a rough layout of child windows
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
		
		// Call on create
		OnCreate();

		// Add icon
		if (d->Icon)
		{
			SetIcon(d->Icon);
			d->Icon.Empty();
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
	#ifdef __GTK_H__
	if (m.Down())
	{
		bool InPopup = false;
		for (GViewI *p = v; p; p = p->GetParent())
		{
			if (dynamic_cast<GPopup*>(p))
			{
				InPopup = true;
				break;
			}
		}
		if (!InPopup && GPopup::CurrentPopups.Length())
		{
			for (int i=0; i<GPopup::CurrentPopups.Length(); i++)
			{
				GPopup *p = GPopup::CurrentPopups[i];
				if (p->Visible())
					p->Visible(false);
			}
		}
	}
	#endif

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
			GView *Target = d->Hooks[i].Target;
			if (Target->OnViewKey(v, k))
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
	switch (d->State)
	{
		case GDK_WINDOW_STATE_ICONIFIED:
			return GZoomMin;
		case GDK_WINDOW_STATE_MAXIMIZED:
			return GZoomMax;
	}

	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
	if (Wnd)
	{
		ThreadCheck();

		switch (i)
		{
			case GZoomMin:
			{
				gtk_window_iconify(Wnd);
				break;
			}
			case GZoomNormal:
			{
				gtk_window_deiconify(Wnd);
				gtk_window_unmaximize(Wnd);
				break;
			}
			case GZoomMax:
			{
				gtk_window_unmaximize(Wnd);
				break;
			}
		}	
	}
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
	if (Wnd)
	{
		ThreadCheck();
		gtk_window_set_title(Wnd, n);
	}

	return GBase::Name(n);
}

char *GWindow::Name()
{
	return GBase::Name();
}

struct CallbackParams
{
	GRect Menu;
	int Depth;
	
	CallbackParams()
	{
		Menu.ZOff(-1, -1);
		Depth = 0;
	}
};

void ClientCallback(GtkWidget *w, CallbackParams *p)
{
	/*
	printf("%.*sCallback %s\n",
		p->Depth,
		"                                         ", gtk_widget_get_name(w));
	*/
	const char *Name = gtk_widget_get_name(w);
	if (Name && !_stricmp(Name, "GtkMenuBar"))
	{
		GtkRequisition alloc;
		gtk_widget_size_request(w, &alloc);
		p->Menu.ZOff(alloc.width-1, alloc.height-1);
	}
	
	if (!p->Menu.Valid())
	{
		p->Depth++;
		if (GTK_IS_CONTAINER(w))
			gtk_container_forall(GTK_CONTAINER(w), (GtkCallback)ClientCallback, p);
		p->Depth--;
	}
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	static GRect r;
	r = GView::GetClient(ClientSpace);
	if (Wnd)
	{
		CallbackParams p;
		gtk_container_forall(GTK_CONTAINER(Wnd), (GtkCallback)ClientCallback, &p);
		if (p.Menu.Valid())
		{
			// printf("MenuSize=%s\n", p.Menu.GetStr());
			r.y2 -= p.Menu.Y();
		}
	}
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

// printf("SerializeState load %s\n", v.Str());


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
// printf("SerializeState setpos %s\n", Position.GetStr());
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
		ThreadCheck();
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

	//if (d->Sx != X() ||	d->Sy != Y())
	{
		Pour();
		//d->Sx = X();
		//d->Sy = Y();
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

void GWindow::OnFrontSwitch(bool b)
{
}

GViewI *GWindow::GetFocus()
{
	return d->Focus;
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
		char Buf[256] = "";
		if (!stricmp(v->GetClass(), "GMdiChild"))
			sprintf(Buf, "'%s'", v->Name());
		v = p[n];
		
		ch += sprintf_s(s + ch, sizeof(s) - ch, "%s>%s", Buf, v->GetClass());
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
			#if WINNATIVE
			SetForegroundWindow(Handle());
			#endif
			int Result = RClick.Float(this, m.x, m.y);
			#if WINNATIVE
			PostMessage(Handle(), WM_NULL, 0, 0);
			#endif
			OnTrayMenuResult(Result);
		}
	}
}

