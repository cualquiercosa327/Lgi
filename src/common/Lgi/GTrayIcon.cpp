#include "Lgi.h"

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

////////////////////////////////////////////////////////////////////////////
#ifdef XWIN

OsView LgiGetSystemTray()
{
	/*
	char Tray[64];
	sprintf(Tray, "_NET_SYSTEM_TRAY_S%i", XDefaultScreen(Dsp));
	Window SystemTrayWnd = XGetSelectionOwner(Dsp, XInternAtom(Dsp, Tray, false));
	if (SystemTrayWnd)
	{
	}
	else if (!SystemTrayWnd)
	{
		printf("%s:%i - Couldn't find SystemTrayWnd.\n", __FILE__, __LINE__);
	}
	
	return SystemTrayWnd;
	*/
	
	return 0;
}

void LgiSendTrayMessage(long Message, long Data1 = 0, long Data2 = 0, long Data3 = 0)
{
	/*
	Atom XA_SysTrayOpCode = XInternAtom(Dsp, "_NET_SYSTEM_TRAY_OPCODE", false);
	Window SystemTrayWnd = LgiGetSystemTray(Dsp);
	if (SystemTrayWnd)
	{
		XEvent ev;

		ZeroObj(ev);
		ev.xclient.type = ClientMessage;
		ev.xclient.window = SystemTrayWnd;
		ev.xclient.message_type = XA_SysTrayOpCode;
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = CurrentTime;
		ev.xclient.data.l[1] = Message;
		ev.xclient.data.l[2] = Data1;
		ev.xclient.data.l[3] = Data2;
		ev.xclient.data.l[4] = Data3;

//		printf("%s:%i - XSendEvent\n", __FILE__, __LINE__);
		XSendEvent(Dsp, SystemTrayWnd, false, NoEventMask, &ev);
		XSync(Dsp, False);
	}
	else
	{
		printf("%s:%i - GTrayIcon::SendTrayMessage(...) Don't have the SystemTrayWnd.\n", __FILE__, __LINE__);
	}
	*/
}

#endif

class GTrayIconPrivate
{
public:
	GWindow *Parent;	// parent window
	int Val;			// which icon is currently visible

	#if WIN32NATIVE
	
	int TrayCreateMsg;
	int MyId;

	union
	{
		NOTIFYICONDATAA	a;
		NOTIFYICONDATAW	w;
	}
	TrayIcon;

	List<HICON> Icon;
	
	#else
		
	List<GSurface> Icon;
	GView *TrayWnd;
	
	#ifdef MAC
	bool Visible;
	#endif;
	
	#endif

	GTrayIconPrivate(GWindow *p)
	{
		Parent = p;
		Val = 0;
		
		#if WIN32NATIVE
		
		ZeroObj(TrayIcon);
		MyId = 0;
		TrayCreateMsg = RegisterWindowMessage(TEXT("TaskbarCreated"));
		
		#else

		TrayWnd = 0;

		#ifdef MAC
		Visible = false;
		#endif;
		
		#endif
	}	
	
	~GTrayIconPrivate()
	{
		#if WIN32NATIVE
		for (HICON *i=Icon.First(); i; i=Icon.Next())
		{
			DeleteObject(*i);
		}
		#endif
		Icon.DeleteObjects();
	}

};

////////////////////////////////////////////////////////////////////////////
#ifdef XWIN
class GTrayWnd : public GView
{
	GTrayIcon *TrayIcon;
	int Atom_XEMBED;

public:
	GTrayWnd(GTrayIcon *ti)
	{
		TrayIcon = ti;
		
		/*
		Display *Dsp = Handle()->XDisplay();
		Atom_XEMBED = XInternAtom(Dsp, "_XEMBED", false);
		
		XSizeHints *Sh = XAllocSizeHints();
		if (Sh)
		{
			Sh->flags = USSize | PBaseSize;
			Sh->base_width = Sh->width = 24;
			Sh->base_height = Sh->height = 24;
			XSetWMNormalHints(Dsp, Handle()->handle(), Sh);
			XFree(Sh);
		}		
		
		// Support for freedesktop.org
		uint32 x[2] = { 0, 1 };
		XChangeProperty(Dsp,
						Handle()->handle(),
						XInternAtom(Dsp, "_XEMBED_INFO", false),
						XInternAtom(Dsp, "CARD32", false), // ??
						32,
						PropModeReplace,
						(uchar*) &x,
						2);


		// Support for KDE
		Window Win = Handle()->handle();
		Atom kde_net_wm_system_tray_window_for = XInternAtom(Dsp, "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", false);
	    XChangeProperty(Dsp,
	    				Handle()->handle(),
	    				kde_net_wm_system_tray_window_for,
	    				XA_WINDOW,
	    				32,
			    		PropModeReplace,
			    		(unsigned char *)&Win,
			    		1);

		if (Handle() AND
			Handle()->handle())
		{
			LgiSendTrayMessage(Dsp, SYSTEM_TRAY_REQUEST_DOCK, Handle()->handle());
		}
		else
		{
			printf("%s:%i - No handle()\n", __FILE__, __LINE__);
		}
		*/
	}
	
	int OnEvent(GMessage *m)
	{
		switch (MsgCode(m))
		{
			case M_X11_REPARENT:
			{
				static uint64 LastReparent = 0; // don't ask.
			
				/*
				Window NewParent = (Window)MsgA(m);
				Window Desktop = XDefaultRootWindow(Handle()->XDisplay());
				bool IsDesktop = NewParent == Desktop;
				if (IsDesktop)
				{
					uint64 Now = LgiCurrentTime();

					if (LastReparent + 1000 < Now)
					{
						Display *Dsp = Handle()->XDisplay();
						XSync(Dsp, false);
						LgiSendTrayMessage(Dsp, SYSTEM_TRAY_REQUEST_DOCK, Handle()->handle());
						LastReparent = Now;
					}
				}
				*/
				
				break;
			}
		}
		
		if (MsgCode(m) == Atom_XEMBED)
		{
			Invalidate();
		}
		
		return GView::OnEvent(m);
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();

		GSurface *i = TrayIcon->d->Icon[TrayIcon->d->Val];
		if (i)
		{
			pDC->Blt((pDC->X() - i->X()) / 2,
					 (pDC->Y() - i->Y()) / 2,
					 i);
		}
	}

	void OnMouseClick(GMouse &m)
	{
		if (TrayIcon->d->Parent)
			TrayIcon->d->Parent->OnTrayClick(m);
	}
};
#endif

////////////////////////////////////////////////////////////////////////////
GTrayIcon::GTrayIcon(GWindow *p)
{
	d = new GTrayIconPrivate(p);
}

GTrayIcon::~GTrayIcon()
{
	Visible(false);
	DeleteObj(d);
}

bool GTrayIcon::Load(const TCHAR *Str)
{
	#if WIN32NATIVE
	
	HICON i = ::LoadIcon(LgiProcessInst(), Str);
	if (i)
	{
		d->Icon.Insert(new HICON(i));
		return true;
	}
	
	#elif defined(__GTK_H__) || defined(MAC)
	
	GAutoString File(LgiFindFile(Str));
	if (File)
	{
		GSurface *i = LoadDC(File);
		if (i)
		{
			if (GdcD->GetBits() != i->GetBits())
			{
				GSurface *n = new GMemDC(i->X(), i->Y(), GdcD->GetBits());
				if (n)
				{
					n->Colour(0);
					n->Rectangle();
					n->Blt(0, 0, i);
					DeleteObj(i);
					i = n;
				}
			}

			d->Icon.Insert(i);
		}
		else
		{
			printf("%s:%i - Couldn't load '%s'\n", __FILE__, __LINE__, Str);
		}

		return i != 0;
	}
	else
	{
		printf("%s:%i - Couldn't find '%s'\n", __FILE__, __LINE__, Str);
	}
	
	#else

	// FIXME
	LgiAssert(0);
	
	#endif

	return false;
}

bool GTrayIcon::Visible()
{
	#if WIN32NATIVE
	return d->TrayIcon.a.cbSize != 0;
	#elif defined LINUX	
	return d->TrayWnd;
	#elif defined MAC
	return d->Visible;
	#else
	return false;
	#endif
}

void GTrayIcon::Visible(bool v)
{
	if (Visible() != v)
	{
		if (v)
		{
			#if WIN32NATIVE
			
			static int Id = 1;
			HICON *Cur = d->Icon[d->Val];

			ZeroObj(d->TrayIcon);
			if (IsWin9x)
			{
				d->TrayIcon.a.cbSize = sizeof(d->TrayIcon.a);
				d->TrayIcon.a.hWnd = d->Parent ? d->Parent->Handle() : 0;
				d->TrayIcon.a.uID = d->MyId = Id++;
				d->TrayIcon.a.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
				d->TrayIcon.a.uCallbackMessage = M_TRAY_NOTIFY;
				d->TrayIcon.a.hIcon = Cur ? *Cur : 0;
				
				char *n = LgiToNativeCp(Name());
				strncpy(d->TrayIcon.a.szTip, n?n:"", sizeof(d->TrayIcon.a.szTip));
				DeleteArray(n);

				Shell_NotifyIconA(NIM_ADD, &d->TrayIcon.a);
			}
			else
			{
				d->TrayIcon.w.cbSize = sizeof(d->TrayIcon.w);
				d->TrayIcon.w.hWnd = d->Parent ? d->Parent->Handle() : 0;
				d->TrayIcon.w.uID = d->MyId = Id++;
				d->TrayIcon.w.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
				d->TrayIcon.w.uCallbackMessage = M_TRAY_NOTIFY;
				d->TrayIcon.w.hIcon = Cur ? *Cur : 0;
				StrncpyW(d->TrayIcon.w.szTip, (char16*)(NameW()?NameW():L""), sizeof(d->TrayIcon.w.szTip));

				Shell_NotifyIconW(NIM_ADD, &d->TrayIcon.w);
			}
			
			#elif defined XWIN
			
			if (!d->TrayWnd)
			{
				d->TrayWnd = new GTrayWnd(this);
			}
			
			#elif defined MAC
			
			if (!d->Visible)
			{
				d->Visible = true;
				int Ico = d->Val;
				d->Val = -1;
				Value(Ico);
			}
			
			#endif
		}
		else
		{
			#if WIN32NATIVE
			
			if (IsWin9x)
			{
				Shell_NotifyIconA(NIM_DELETE, &d->TrayIcon.a);
			}
			else
			{
				Shell_NotifyIconW(NIM_DELETE, &d->TrayIcon.w);
			}
			ZeroObj(d->TrayIcon);
			
			#elif defined XWIN
			
			DeleteObj(d->TrayWnd);
			
			#elif defined MAC
			
			RestoreApplicationDockTileImage();
			d->Visible = false;
			
			#endif
		}
	}
}

int64 GTrayIcon::Value()
{
	return d->Val;
}

void GTrayIcon::Value(int64 v)
{
	if (d->Val != v)
	{
		d->Val = v;
		
		#if WIN32NATIVE
		
		if (Visible())
		{
			HICON *Cur = d->Icon[d->Val];

			ZeroObj(d->TrayIcon);

			if (IsWin9x)
			{
				d->TrayIcon.a.cbSize = sizeof(d->TrayIcon.a);
				d->TrayIcon.a.hWnd = d->Parent ? d->Parent->Handle() : 0;
				d->TrayIcon.a.uID = d->MyId;
				d->TrayIcon.a.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
				d->TrayIcon.a.uCallbackMessage = M_TRAY_NOTIFY;
				d->TrayIcon.a.hIcon = Cur ? *Cur : 0;
				char *n = LgiToNativeCp(Name());
				strncpy(d->TrayIcon.a.szTip, n?n:"", sizeof(d->TrayIcon.a.szTip));
				Shell_NotifyIconA(NIM_MODIFY, &d->TrayIcon.a);
			}
			else
			{
				d->TrayIcon.w.cbSize = sizeof(d->TrayIcon.w);
				d->TrayIcon.w.hWnd = d->Parent ? d->Parent->Handle() : 0;
				d->TrayIcon.w.uID = d->MyId;
				d->TrayIcon.w.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
				d->TrayIcon.w.uCallbackMessage = M_TRAY_NOTIFY;
				d->TrayIcon.w.hIcon = Cur ? *Cur : 0;
				StrncpyW(d->TrayIcon.w.szTip, (char16*)(NameW()?NameW():L""), sizeof(d->TrayIcon.w.szTip));
				Shell_NotifyIconW(NIM_MODIFY, &d->TrayIcon.w);
			}
		}
		
		#elif defined LINUX
		
		if (d->TrayWnd)
		{
			d->TrayWnd->Invalidate();
		}
		
		#elif defined MAC
		
		GSurface *t = d->Icon[d->Val];
		if (t)
		{
			CGContextRef c = BeginCGContextForApplicationDockTile();
			if (c)
			{
				CGContextTranslateCTM(c, 0, 128); 
				CGContextScaleCTM(c, 1.0, -1.0); 

				GScreenDC Dc((GView*)0, c);
				
				GMemDC m;
				if (m.Create(t->X()*4, t->Y()*4, 32))
				{
					double Sx = 0.25;
					double Sy = 0.25;
					for (int y=0; y<m.Y(); y++)
					{
						int Y = (int) (y * Sy);
						Pixel32 *s = (Pixel32*)(*t)[Y];
						Pixel32 *d = (Pixel32*)(m)[y];
						
						for (int x=0; x<m.X(); x++)
						{
							d[x] = s[(int)(x * Sx)];
						}
					}
					
					Dc.Blt(Dc.X()-m.X()-1, Dc.Y()-m.Y()-1, &m);
				}
			}
			
			CGContextFlush(c);
			EndCGContextForApplicationDockTile(c);
		}
		else
		{
			RestoreApplicationDockTileImage();
		}
		
		#endif
	}
}

int GTrayIcon::OnEvent(GMessage *Message)
{
	#if WIN32NATIVE
	
	if (MsgCode(Message) == M_TRAY_NOTIFY AND
		MsgA(Message) == d->MyId)
	{
		// got a notification from the icon
		GMouse m;
		ZeroObj(m);
		switch (MsgB(Message))
		{
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDBLCLK:
			{
				m.Down(true);
				m.Double(true);
				break;
			}
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
			{
				m.Down(true);
				break;
			}
		}

		switch (MsgB(Message))
		{
			case WM_LBUTTONDBLCLK:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			{
				m.Left(true);
				break;
			}
			case WM_RBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			{
				m.Right(true);
				break;
			}
			case WM_MBUTTONDBLCLK:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			{
				m.Middle(true);
				break;
			}
		}

		switch (MsgB(Message))
		{
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDBLCLK:
			{
				m.Double(true);
				break;
			}
		}

		if (d->Parent)
		{
			d->Parent->OnTrayClick(m);
		}
	}
	else if (MsgCode(Message) == d->TrayCreateMsg)
	{
		// explorer crashed... reinit the icon
		ZeroObj(d->TrayIcon);
		Visible(true);
	}
	#endif

	return 0;
}

