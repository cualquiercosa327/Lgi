#include <stdio.h>
#include <math.h>

#include "Lgi.h"
#include "GEdit.h"
#include "GPopup.h"
#include "GToolBar.h"
#include "GPanel.h"
#include "GVariant.h"
#include "GToken.h"

#define DEBUG_WINDOW_PLACEMENT				0

extern bool In_SetWindowPos;

///////////////////////////////////////////////////////////////////////////////////////////////
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
	bool SnapToEdge;
	GWindowZoom Show;
	bool InCreate;
	WINDOWPLACEMENT *Wp;

	GWindowPrivate()
	{
		InCreate = true;
		Show = GZoomNormal;
		SnapToEdge = false;
		Wp = 0;
	}

	~GWindowPrivate()
	{
		DeleteObj(Wp);
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

///////////////////////////////////////////////////////////////////////////////////////////////
GWindow::GWindow() : GView(0)
{
	d = new GWindowPrivate;
	Menu = 0;
	SetStyle(GetStyle() | WS_TILEDWINDOW | WS_CLIPCHILDREN);
	SetStyle(GetStyle() & ~WS_CHILD);
	SetExStyle(GetExStyle() | WS_EX_CONTROLPARENT);
	
	GWin32Class *c = GWin32Class::Create(GetClass());
	if (c)
	{
		c->Register();
	}	
	
	LastFocus = 0;
	VirtualFocusId = -1;

	Visible(false);

	_Default = 0;
	_Window = this;
	_Lock = new GSemaphore("GWindow");
	_QuitOnClose = false;
}

GWindow::~GWindow()
{
	if (Menu)
	{
		Menu->Detach();
		DeleteObj(Menu);
	}

	DeleteObj(_Lock);
	DeleteObj(d);
}

bool GWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void GWindow::SetSnapToEdge(bool b)
{
	d->SnapToEdge = b;
}

void GWindow::Raise()
{
	if (_View)
	{
		DWORD dwFGProcessId;
		DWORD dwFGThreadId = GetWindowThreadProcessId(_View, &dwFGProcessId);
		DWORD dwThisThreadId = GetCurrentThreadId();

		AttachThreadInput(dwThisThreadId, dwFGThreadId, true);

		SetForegroundWindow(_View);
		BringWindowToTop(_View);

		if (In_SetWindowPos)
		{
			assert(0);
			LgiTrace("%s:%i - SetFocus(%p)\n", __FILE__, __LINE__, _View);
		}
		SetFocus(_View);

		AttachThreadInput(dwThisThreadId, dwFGThreadId, false);
	}
}

GWindowZoom GWindow::GetZoom()
{
	if (IsZoomed(Handle()))
	{
		return GZoomMax;
	}
	else if (IsIconic(Handle()))
	{
		return GZoomMin;
	}

	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
	if (_View AND IsWindowVisible(_View))
	{
		switch (i)
		{
			case GZoomMax:
			{
				ShowWindow(Handle(), SW_MAXIMIZE);
				break;
			}
			case GZoomMin:
			{
				ShowWindow(Handle(), SW_MINIMIZE);
				break;
			}
			case GZoomNormal:
			{
				if (!Visible())
				{
					Visible(true);
				}

				if (IsIconic(Handle()) OR IsZoomed(Handle()))
				{
					ShowWindow(Handle(), SW_NORMAL);
				}

				LgiYield();

				RECT r;
				GetWindowRect(Handle(), &r);

				if (r.left != Pos.x1 OR
					r.top != Pos.y1)
				{
					SetWindowPos(Handle(), 0, Pos.x1, Pos.y1, Pos.X(), Pos.Y(), SWP_NOZORDER);
				}
				break;
			}
		}
	}

	d->Show = i;
}

bool GWindow::OnRequestClose(bool OsShuttingDown)
{
	if (GetQuitOnClose())
	{
		LgiCloseApp();
	}

	return true;
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
	// Any window in a popup always gets the key...
	GViewI *p;
	for (p = v->GetParent(); p; p = p->GetParent())
	{
		if (dynamic_cast<GPopup*>(p))
		{
			return v->OnKey(k);
		}
	}

	// Allow any hooks to see the key...
	for (int i=0; i<d->Hooks.Length(); i++)
	{
		if (d->Hooks[i].Flags & GKeyEvents)
		{
			if (d->Hooks[i].Target->OnViewKey(v, k))
			{
				return true;
			}
		}
	}

	// Give the key to the window...
	GViewI *Focus = VirtualFocusId >= 0 ? FindControl(VirtualFocusId) : v;
	if (Focus && Focus->OnKey(k))
	{
		return true;
	}

	// Check default controls
	p = 0;
	if (k.c16 == VK_RETURN)
	{
		p = _Default;
	}
	else if (k.c16 == VK_ESCAPE)
	{
		p = FindControl(IDCANCEL);
	}
	if (p AND p->OnKey(k))
	{
		return true;
	}

	// Menu shortcut?
	if (Menu AND Menu->OnKey(v, k))
	{
		return true;
	}

	return false;
}

void GWindow::OnPaint(GSurface *pDC)
{
	pDC->Colour(LC_MED, 24);
	pDC->Rectangle();
}

bool GWindow::Visible()
{
	return GView::Visible();
}

void GWindow::Visible(bool v)
{
	if (v)
	{
		Pour();
	}
	else
	{
		int asd=0;
	}

	if (v)
	{
		SetStyle(GetStyle() | WS_VISIBLE);

		if (_View)
		{
			GWindowZoom z = d->Show;
			char *Cmd = 0;

			WINDOWPLACEMENT *Wp = new WINDOWPLACEMENT;
			if (Wp)
			{
				ZeroObj(*Wp);
				Wp->length = sizeof(*Wp);
				Wp->flags = 2;
				Wp->ptMaxPosition.x = -1;
				Wp->ptMaxPosition.y = -1;

				if (d->Show == GZoomMax)
				{
					Wp->showCmd = SW_MAXIMIZE;
					Cmd = "SW_MAXIMIZE";
				}
				else if (d->Show == GZoomMin)
				{
					Wp->showCmd = SW_MINIMIZE;
					Cmd = "SW_MINIMIZE";
				}
				else
				{
					Wp->showCmd = SW_NORMAL;
					Cmd = "SW_NORMAL";
				}

				Wp->rcNormalPosition = Pos;
				#if DEBUG_WINDOW_PLACEMENT
				LgiTrace("%s:%i - SetWindowPlacement, pos=%s, show=%i\n", __FILE__, __LINE__, Pos.GetStr(), Wp->showCmd);
				#endif
				SetWindowPlacement(_View, Wp);

				if (d->InCreate)
				{
					DeleteObj(d->Wp);
					d->Wp = Wp;
				}
				else
				{
					DeleteObj(Wp);
				}
			}
		}
	}
	else
	{
		#if DEBUG_WINDOW_PLACEMENT
		LgiTrace("%s:%i - Visible(%i)\n", __FILE__, __LINE__, v);
		#endif
		GView::Visible(v);
	}

	if (v)
	{
		OnZoom(d->Show);
	}
}

void GWindow::Pour()
{
	GRegion Client(GetClient());
	GRegion Update;
	bool HasTools = false;
	GViewI *v;

	{
		GRegion Tools;

		for (v = Children.First(); v; v = Children.Next())
		{
			GView *k = dynamic_cast<GView*>(v);
			if (k AND k->_IsToolBar)
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

	for (v = Children.First(); v; v = Children.Next())
	{
		GView *k = dynamic_cast<GView*>(v);
		if (!(k AND k->_IsToolBar))
		{
			GRect OldPos = v->GetPos();
			Update.Union(&OldPos);

			if (v->Pour(Client))
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

				Client.Subtract(&v->GetPos());
				Update.Subtract(&v->GetPos());
			}
			else
			{
				// make the view not visible
				// v->Visible(FALSE);
			}
		}
	}

	for (int i=0; i<Update.Length(); i++)
	{
		Invalidate(Update[i]);
	}
}

class GTempView : public GView
{
public:
	GTempView(OsView v)
	{
		_View = v;
	}

	~GTempView()
	{
		_View = 0;
	}
};

GMessage::Result GWindow::OnEvent(GMessage *Msg)
{
	int Status = 0;

	switch (MsgCode(Msg))
	{
		case M_SET_WINDOW_PLACEMENT:
		{
			/*	
				Apparently if you use SetWindowPlacement inside the WM_CREATE handler,
				then the restored rect doesn't "stick", it gets stomped on by windows.
				So this code... RESETS it to be what we set earlier. Windows sucks.
			*/
			if (d->Wp)
			{
				if (_View)
				{
					GRect r = d->Wp->rcNormalPosition;

					if (!GView::Visible())
					{
						d->Wp->showCmd = SW_HIDE;
					}

					#if DEBUG_WINDOW_PLACEMENT
					LgiTrace("%s:%i - SetWindowPlacement, pos=%s, show=%i\n", __FILE__, __LINE__, r.GetStr(), d->Wp->showCmd);
					#endif

					SetWindowPlacement(_View, d->Wp);
				}
				DeleteObj(d->Wp);
			}
			break;
		}
		case WM_SYSCOLORCHANGE:
		{
			LgiInitColours();
			break;
		}
		case WM_WINDOWPOSCHANGING:
		{
			bool Icon = IsIconic(Handle());
			bool Zoom = IsZoomed(Handle());

			if (!Icon AND !Zoom)
			{
				WINDOWPOS *Info = (LPWINDOWPOS) Msg->b;
				if (Info)
				{
					if (GetMinimumSize().x AND
						GetMinimumSize().x > Info->cx)
					{
						Info->cx = GetMinimumSize().x;
					}

					if (GetMinimumSize().y AND
						GetMinimumSize().y > Info->cy)
					{
						Info->cy = GetMinimumSize().y;
					}

					RECT Rc;
					if (d->SnapToEdge AND
						SystemParametersInfo(SPI_GETWORKAREA, 0, &Rc, SPIF_SENDCHANGE))
					{
						GRect r = Rc;
						GRect p(Info->x, Info->y, Info->x + Info->cx - 1, Info->y + Info->cy - 1);
						if (r.Valid() AND p.Valid())
						{
							int Snap = 12;

							if (abs(p.x1 - r.x1) <= Snap)
							{
								// Snap left edge
								Info->x = r.x1;
							}
							else if (abs(p.x2 - r.x2) <= Snap)
							{
								// Snap right edge
								Info->x = r.x2 - Info->cx + 1;
							}

							if (abs(p.y1 - r.y1) <= Snap)
							{
								// Snap top edge
								Info->y = r.y1;
							}
							else if (abs(p.y2 - r.y2) <= Snap)
							{
								// Snap bottom edge
								Info->y = r.y2 - Info->cy + 1;
							}
						}
					}
				}
			}
			break;
		}
		case WM_SIZE:
		{
			if (Visible())
			{
				GWindowZoom z = d->Show;
				switch (Msg->a)
				{
					case SIZE_MINIMIZED:
					{
						z = GZoomMin;
						break;
					}
					case SIZE_MAXIMIZED:
					{
						z = GZoomMax;
						break;
					}
					case SIZE_RESTORED:
					{
						z = GZoomNormal;
						break;
					}
				}
				if (z != d->Show)
				{
					OnZoom(d->Show = z);
				}
			}

			Status = GView::OnEvent(Msg);
			break;
		}
		case WM_ACTIVATE:
		{
			// This is a hack to make Windows set the focus of a child
			// control when you Alt-Tab in and out of a top level window
			if (LOWORD(Msg->a) != WA_INACTIVE)
			{
				// gaining focus
				if (LastFocus)
				{
					if (In_SetWindowPos)
					{
						assert(0);
						LgiTrace("%s:%i - %s->SetFocus()\n", __FILE__, __LINE__, GetClass());
					}
					SetFocus(LastFocus);
					LastFocus = 0;
				}
			}
			else
			{
				// losing focus
				LastFocus = 0;
				HWND f = GetFocus();
				for (HWND p = ::GetParent(f); p; p = ::GetParent(p))
				{
					if (p == Handle())
					{
						LastFocus = f;
					}
				}
			}
			break;
		}
		case WM_CREATE:
		{
			#ifdef SKIN_MAGIC
			SetWindowSkin(Handle(), "MainFrame");
			#endif
			
			Pour();
			OnCreate();

			if (!_Default)
			{
				_Default = FindControl(IDOK);
				if (_Default)
				{
					_Default->Invalidate();
				}
			}

			d->InCreate = false;
			if (d->Wp)
			{
				PostEvent(M_SET_WINDOW_PLACEMENT);
			}
			break;
		}
		case WM_WINDOWPOSCHANGED:
		{
			DeleteObj(d->Wp);
			Status = GView::OnEvent(Msg);
			break;
		}
		case WM_QUERYENDSESSION:
	 	case WM_CLOSE:
		{
			bool QuitApp = false;
			if (OnRequestClose(MsgCode(Msg) == WM_QUERYENDSESSION))
			{
				delete this;
			}

			if (MsgCode(Msg) == WM_CLOSE)
			{
				return 0;
			}
			else
			{
				return QuitApp;
			}
			break;
		}
		case WM_DROPFILES:
		{
			HDROP hDrop = (HDROP) Msg->a;
			if (hDrop)
			{
				GArray<char*> FileNames;
				int Count = 0;
				
				if (IsWin9x)
				{
					Count = DragQueryFile(hDrop, -1, NULL, 0);;
				}
				else
				{
					Count = DragQueryFileW(hDrop, -1, NULL, 0);
				}

				for (int i=0; i<Count; i++)
				{
					if (IsWin9x)
					{
						char FileName[256];
						if (DragQueryFile(hDrop, i, FileName, sizeof(FileName)-1) > 0)
						{
							FileNames.Add(LgiFromNativeCp(FileName));
						}
					}
					else
					{
						char16 FileName[256];
						if (DragQueryFileW(hDrop, i, FileName, sizeof(FileName)-1) > 0)
						{
							FileNames.Add(LgiNewUtf16To8(FileName));
						}
					}
				}

				OnReceiveFiles(FileNames);
				FileNames.DeleteArrays();
			}
			break;
		}
		case M_HANDLEMOUSEMOVE:
		{
			// This receives events fired from the GMouseHookPrivate class so that
			// non-LGI windows create mouse hook events as well.
			GTempView v((OsView)MsgB(Msg));
			GMouse m;
			m.x = LOWORD(MsgA(Msg));
			m.y = HIWORD(MsgA(Msg));
			HandleViewMouse(&v, m);
			break;
		}
		case M_COMMAND:
		{
			HWND OurWnd = Handle(); // copy onto the stack, because
									// we might lose the 'this' object in the
									// OnCommand handler which would delete
									// the memory containing the handle.

			Status = OnCommand(Msg->a, 0, (HWND) Msg->b);
			if (!IsWindow(OurWnd))
			{
				// The window was deleted so break out now
				break;
			}
			// otherwise fall thru to the GView handler
		}
		default:
		{
			Status = GView::OnEvent(Msg);
			break;
		}
	}

	return Status;
}

GRect &GWindow::GetPos()
{
	if (_View AND IsZoomed(_View))
	{
		static GRect r;
		RECT rc;
		GetWindowRect(_View, &rc);
		r = rc;
		return r;
	}

	return Pos;
}

void GWindow::OnPosChange()
{
	Pour();
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

bool GWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	if (!Store OR !FieldName)
		return false;

	if (Load)
	{
		GVariant v;
		if (Store->GetValue(FieldName, v) AND v.Str())
		{
			GRect Position(0, 0, -1, -1);
			int State = 0;

			GToken t(v.Str(), ";");
			for (int i=0; i<t.Length(); i++)
			{
				char *Var = t[i];
				char *Value = strchr(Var, '=');
				if (Value)
				{
					*Value++ = 0;

					if (stricmp(Var, "State") == 0)
						State = atoi(Value);
					else if (stricmp(Var, "Pos") == 0)
					{
					    GRect r;
					    r.SetStr(Value);
					    if (r.Valid())
						    Position = r;
					}
				}
				else return false;
			}

			// Apply any shortcut override
			int Show = LgiApp->GetShow();
			if (Show == SW_SHOWMINIMIZED OR
				Show == SW_SHOWMINNOACTIVE OR
				Show == SW_MINIMIZE)
			{
				State = -1;
			}
			else if (Show == SW_SHOWMAXIMIZED OR
					 Show == SW_MAXIMIZE)
			{
				State = 1;
			}

			WINDOWPLACEMENT *Wp = new WINDOWPLACEMENT;
			if (Wp)
			{
				ZeroObj(*Wp);
				Wp->length = sizeof(*Wp);
				if (Visible())
				{
					if (State > 0)
					{
						Wp->showCmd = SW_SHOWMAXIMIZED;
					}
					else if (State < 0)
					{
						Wp->showCmd = SW_MINIMIZE;
					}
					else
					{
						Wp->showCmd = SW_NORMAL;
					}
				}
				else
				{
					Wp->showCmd = SW_HIDE;
					if (State > 0)
					{
						d->Show = GZoomMax;
					}
					else if (State < 0)
					{
						d->Show = GZoomMin;
					}
					else
					{
						d->Show = GZoomNormal;
					}
				}

				if (Position.Valid())
				    Pos = Position;
				Wp->rcNormalPosition = Pos;
				#if DEBUG_WINDOW_PLACEMENT
				LgiTrace("%s:%i - SetWindowPlacement, pos=%s, show=%i\n", __FILE__, __LINE__, Pos.GetStr(), Wp->showCmd);
				#endif
				SetWindowPlacement(Handle(), Wp);

				if (d->InCreate)
				{
					DeleteObj(d->Wp);
					d->Wp = Wp;
				}
				else
				{
					DeleteObj(Wp);
				}
			}
		}
		else return false;
	}
	else
	{
		char s[256];
		int State = 0;
		if (IsZoomed(Handle()))
			State = 1;
		else if (IsIconic(Handle()))
			State = -1;

		WINDOWPLACEMENT Wp;
		ZeroObj(Wp);
		Wp.length = sizeof(Wp);
		GetWindowPlacement(Handle(), &Wp);
		GRect Position = Wp.rcNormalPosition;
		sprintf(s, "State=%i;Pos=%s", State, Position.GetStr());

		GVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}

	return true;
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

