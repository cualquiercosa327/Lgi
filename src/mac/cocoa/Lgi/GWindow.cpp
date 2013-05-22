#include <stdio.h>

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GToken.h"
#include "GPopup.h"

extern void NextTabStop(GViewI *v, int dir);
extern void SetDefaultFocus(GViewI *v);

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
	GWindow *Wnd;
	int Sx, Sy;
	bool Dynamic;
	GKey LastKey;
	GArray<HookInfo> Hooks;
	bool SnapToEdge;
	GDialog *ChildDlg;
	bool DeleteWhenDone;
	bool InitVisible;
	uint64 LastMinimize;
	bool CloseRequestDone;

	GMenu *EmptyMenu;

	GWindowPrivate(GWindow *wnd)
	{
		InitVisible = false;
		LastMinimize = 0;
		Wnd = wnd;
		DeleteWhenDone = false;
		ChildDlg = 0;
		Sx = Sy = -1;
		Dynamic = true;
		SnapToEdge = false;
		ZeroObj(LastKey);
		EmptyMenu = 0;
		CloseRequestDone = false;
	}
	
	~GWindowPrivate()
	{
		DeleteObj(EmptyMenu);
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

GWindow::GWindow() :
	GView(0)
{
	d = new GWindowPrivate(this);
	_QuitOnClose = false;
	Wnd = 0;
	Menu = 0;
	VirtualFocusId = -1;
	_Default = 0;
	_Window = this;
	WndFlags |= GWND_CREATE;
	GView::Visible(false);

    _Lock = new GMutex;

	GRect pos(0, 50, 200, 100);
}

GWindow::~GWindow()
{
	SetDragHandlers(false);
	
	if (LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

	_Delete();
	
	if (Wnd)
	{
		Wnd = 0;
	}

	DeleteObj(Menu);
	DeleteObj(d);
	DeleteObj(_Lock);
}

bool &GWindow::CloseRequestDone()
{
	return d->CloseRequestDone;
}

void GWindow::SetDragHandlers(bool On)
{
}

static void _ClearChildHandles(GViewI *v)
{
	GViewIterator *it = v->IterateViews();
	if (it)
	{
		for (GViewI *v = it->First(); v; v = it->Next())
		{
			_ClearChildHandles(v);
			v->Detach();			
		}
	}
	DeleteObj(it);
}

void GWindow::Quit(bool DontDelete)
{
	if (_QuitOnClose)
	{
		LgiCloseApp();
	}
	
	d->DeleteWhenDone = !DontDelete;
	if (Wnd)
	{
		SetDragHandlers(false);
		Wnd = 0;
		_View = 0;
	}
}

void GWindow::SetChildDialog(GDialog *Dlg)
{
	d->ChildDlg = Dlg;
}

bool GWindow::GetSnapToEdge()
{
	return d->SnapToEdge;
}

void GWindow::SetSnapToEdge(bool s)
{
	d->SnapToEdge = s;
}

void GWindow::OnFrontSwitch(bool b)
{
	if (b && d->InitVisible)
	{
	}
}

bool GWindow::Visible()
{
	if (Wnd)
	{
	}
	
	return false;
}

void GWindow::Visible(bool i)
{
	if (Wnd)
	{
		if (i)
		{
			d->InitVisible = true;
			Pour();
			SetDefaultFocus(this);
		}
		else
		{
		}
		
		OnPosChange();
	}
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

bool GWindow::PostEvent(int Event, int a, int b)
{
	bool Status = false;

	if (Wnd)
	{
	}
	
	return Status;
}

#define InRect(r, x, y) \
	( (x >= r.left) && (y >= r.top) && (x <= r.right) && (y <= r.bottom) )

bool GWindow::Attach(GViewI *p)
{
	bool Status = false;
	
	if (Wnd)
	{
		if (GBase::Name())
			Name(GBase::Name());
		
		Status = true;
		
		// Setup default button...
		if (!_Default)
		{
			_Default = FindControl(IDOK);
			if (_Default)
			{
				_Default->Invalidate();
			}
		}

		OnCreate();
		OnAttach();
		OnPosChange();

		// Set the first control as the focus...
		NextTabStop(this, 0);
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
	#ifdef MAC
	if (m.Down())
	{
		GAutoPtr<GViewIterator> it(IterateViews());
		for (GViewI *n = it->Last(); n; n = it->Prev())
		{
			GPopup *p = dynamic_cast<GPopup*>(n);
			if (p)
			{
				GRect pos = p->GetPos();
				if (!pos.Overlap(m.x, m.y))
				{
					printf("Closing Popup, m=%i,%i not over pos=%s\n", m.x, m.y, pos.GetStr());
					p->Visible(false);
				}
			}
			else break;
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

	// Give key to popups
	if (LgiApp AND
		LgiApp->GetMouseHook() AND
		LgiApp->GetMouseHook()->OnViewKey(v, k))
	{
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
				
				#if DEBUG_KEYS
				printf("Hook ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
				#endif
				
				goto AllDone;
			}
		}
	}

	// Give the key to the window...
	if (v->OnKey(k))
	{
		#if DEBUG_KEYS
		printf("View ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
			k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
		#endif
		
		Status = true;
		goto AllDone;
	}
	
	// Window didn't want the key...
	switch (k.c16)
	{
		case VK_RETURN:
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

	if (Ctrl AND Ctrl->Enabled())
	{
		if (Ctrl->OnKey(k))
		{
			Status = true;

			#if DEBUG_KEYS
			printf("Default Button ate '%c'(%i) down=%i alt=%i ctrl=%i sh=%i\n",
				k.c16, k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
			
			goto AllDone;
		}
	}

	if (Menu)
	{
		Status = Menu->OnKey(v, k);
		if (Status)
		{
			#if DEBUG_KEYS
			printf("Menu ate '%c' down=%i alt=%i ctrl=%i sh=%i\n", k.c16, k.Down(), k.Alt(), k.Ctrl(), k.Shift());
			#endif
		}
	}
	
	// Command+W closes the window... if it doesn't get nabbed earlier.
	if (k.Down() AND
		k.System() AND
		tolower(k.c16) == 'w')
	{
		// Close
		if (d->CloseRequestDone || OnRequestClose(false))
		{
			d->CloseRequestDone = true;
			delete this;
			return true;
		}
	}

AllDone:
	d->LastKey = k;

	return Status;
}


void GWindow::Raise()
{
	if (Wnd)
	{
	}
}

GWindowZoom GWindow::GetZoom()
{
	if (Wnd)
	{
	}

	return GZoomNormal;
}

void GWindow::SetZoom(GWindowZoom i)
{
	switch (i)
	{
		case GZoomMin:
		{
			break;
		}
		default:
		case GZoomNormal:
		{
			break;
		}
	}
}

GViewI *GWindow::GetDefault()
{
	return _Default;
}

void GWindow::SetDefault(GViewI *v)
{
	if (v AND
		v->GetWindow() == (GViewI*)this)
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
	bool Status = GBase::Name(n);

	if (Wnd)
	{	
	}

	return Status;
}

char *GWindow::Name()
{
	return GBase::Name();
}

GRect &GWindow::GetClient(bool ClientSpace)
{
	if (Wnd)
	{
		static GRect c;
	}
	
	return GView::GetClient(ClientSpace);
}

bool GWindow::SerializeState(GDom *Store, const char *FieldName, bool Load)
{
	if (!Store || !FieldName)
		return false;

	if (Load)
	{
		GVariant v;
		if (Store->GetValue(FieldName, v) AND v.Str())
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

		GVariant v = s;
		if (!Store->SetValue(FieldName, v))
			return false;
	}

	return true;
}

GRect &GWindow::GetPos()
{
	if (Wnd)
	{
	}

	return Pos;
}

bool GWindow::SetPos(GRect &p, bool Repaint)
{
	int x = GdcD->X();
	int y = GdcD->Y();

	GRect r = p;

	int MenuY = 0;
	if (r.y1 < MenuY)
		r.Offset(0, MenuY - r.y1);
	if (r.y2 > y)
		r.y2 = y - 1;
	if (r.X() > x)
		r.x2 = r.x1 + x - 1;

	Pos = r;
	if (Wnd)
	{
	}

	return true;
}

void GWindow::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	Pour();
}

void GWindow::OnCreate()
{
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
		AND \
		dynamic_cast<GView*>(v)->_IsToolBar \
	)

void GWindow::Pour()
{
	GRect r = GetClient();
	// printf("::Pour r=%s\n", r.GetStr());
	GRegion Client(r);
	
	GRegion Update(Client);
	bool HasTools = false;
	GViewI *v;
	List<GViewI>::I Lst = Children.Start();

	{
		GRegion Tools;
		
		for (v = *Lst; v; v = *++Lst)
		{
			if (IsTool(v))
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
		if (!IsTool(v))
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
		if (!stricmp(f, LGI_FileDropFormat))
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

	if (Format && Data)
	{
		if (!stricmp(Format, LGI_FileDropFormat))
		{
			GArray<char*> Files;
			GArray< GAutoString > Uri;

			if (Data->IsBinary())
			{
				Uri[0].Reset( NewStr((char*)Data->Value.Binary.Data, Data->Value.Binary.Length) );
			}
			else if (Data->Str())
			{
				Uri[0].Reset( NewStr(Data->Str()) );
			}
			else if (Data->Type == GV_LIST)
			{
				for (GVariant *v=Data->Value.Lst->First(); v; v=Data->Value.Lst->Next())
				{
					char *f = v->Str();
					Uri.New().Reset(NewStr(f));
				}
			}

			for (int i=0; i<Uri.Length(); i++)
			{
				char *File = Uri[i].Get();
				if (strnicmp(File, "file:", 5) == 0)
					File += 5;
				if (strnicmp(File, "//localhost", 11) == 0)
					File += 11;
				
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

int GWindow::OnEvent(GMessage *m)
{
	switch (MsgCode(m))
	{
		case M_CLOSE:
		{
			if (d->CloseRequestDone || OnRequestClose(false))
			{
				d->CloseRequestDone = true;
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

int GWindow::OnCommand(int Cmd, int Event, OsView SrcCtrl)
{

	switch (Cmd)
	{	}
	
	return 0;
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

bool GWindow::Obscured()
{
	return false;
}