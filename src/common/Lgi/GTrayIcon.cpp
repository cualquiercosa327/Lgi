#include "Lgi.h"

#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#if defined(__GTK_H__)
namespace Gtk {
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
}
using namespace Gtk;

class GTrayIconPrivate;
void tray_icon_on_click(GtkStatusIcon *status_icon, GTrayIconPrivate *d);
static void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, GTrayIconPrivate *d);
#endif

////////////////////////////////////////////////////////////////////////////
class GTrayIconPrivate
{
public:
	GWindow *Parent;	// parent window
	int Val;			// which icon is currently visible
	bool Visible;

	#if WIN32NATIVE
	
	int TrayCreateMsg;
	int MyId;

	union
	{
		NOTIFYICONDATAA	a;
		NOTIFYICONDATAW	w;
	}
	TrayIcon;

	typedef HICON IconRef;

	#elif defined(__GTK_H__)
	
	GtkStatusIcon *tray_icon;
	typedef GdkPixbuf *IconRef;
	uint64 LastClickTime;
	gint DoubleClickTime;
	
	void OnClick()
	{
		uint64 Now = LgiCurrentTime();
		GdkScreen *s = gtk_status_icon_get_screen(tray_icon);
		GdkDisplay *dsp = gdk_screen_get_display(s);
		gint x, y;
		GdkModifierType mask;
		gdk_display_get_pointer(dsp, &s, &x, &y, &mask);
		
		GMouse m;
		m.x = x;
		m.y = y;
		m.Shift((mask & GDK_SHIFT_MASK) != 0);
		m.Ctrl((mask & GDK_CONTROL_MASK) != 0);
		m.Alt((mask & GDK_MOD1_MASK) != 0);
		m.Left(true);
		m.Down(true);
		m.Double(Now - LastClickTime < DoubleClickTime);
		Parent->OnTrayClick(m);
		LastClickTime = Now;
	}
	
	void OnMenu(guint button, guint activate_time)
	{
		GMouse m;
		m.Left(button == 1);
		m.Middle(button == 2);
		m.Right(button == 3);
		m.Down(true);
		
		LgiTrace("button=%x\n", button);
		m.Trace("OnMenu");
		Parent->OnTrayClick(m);
	}

	#else

	typedef GSurface *IconRef;
	
	#endif

	::GArray<IconRef> Icon;

	GTrayIconPrivate(GWindow *p)
	{
		Parent = p;
		Val = 0;
		Visible = false;
		
		#if WIN32NATIVE
		
		ZeroObj(TrayIcon);
		MyId = 0;
		TrayCreateMsg = RegisterWindowMessage(TEXT("TaskbarCreated"));
		
		#elif defined(__GTK_H__)

		GtkSettings *settings = gtk_settings_get_default();
		DoubleClickTime = 500;
		if (settings)
			g_object_get(G_OBJECT(settings), "gtk-double-click-time", &DoubleClickTime, NULL);

		LastClickTime = 0;
		tray_icon = Gtk::gtk_status_icon_new();
		if (tray_icon)
		{
			g_signal_connect(G_OBJECT(tray_icon),
							"activate", 
							G_CALLBACK(tray_icon_on_click),
							this);
			g_signal_connect(G_OBJECT(tray_icon), 
							 "popup-menu",
							 G_CALLBACK(tray_icon_on_menu),
							 this);
		}
		
		#endif
	}	
	
	~GTrayIconPrivate()
	{
		#if WIN32NATIVE
		for (int n=0; n<Icon.Length(); n++)
		{
			DeleteObject(Icon[n]);
		}
		#elif defined(__GTK_H__)
		for (int n=0; n<Icon.Length(); n++)
		{
			g_object_unref(Icon[n]);
		}
		#else
		Icon.DeleteObjects();
		#endif
	}

};

#if defined(__GTK_H__)
static void tray_icon_on_click(GtkStatusIcon *status_icon, GTrayIconPrivate *d)
{
	d->OnClick();
}

static void tray_icon_on_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, GTrayIconPrivate *d)
{
	d->OnMenu(button, activate_time);
}
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
		d->Icon.Add(i);
		return true;
	}
	
	#elif defined(__GTK_H__)

	if (Str)	
	{
		GAutoString File(LgiFindFile(Str));
		// d->Icon.Add(new GAutoString(NewStr(File ? File : Str)));
		GError *err = NULL;
		Gtk::GdkPixbuf *pb =  Gtk::gdk_pixbuf_new_from_file(File ? File : Str, &err);
		if (!pb)
			return false;
		
		d->Icon.Add(pb);
	}
	
	#else
	
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

			d->Icon.Add(i);
		}
		else LgiTrace("%s:%i - Couldn't load '%s'\n", _FL, Str);

		return i != 0;
	}
	else LgiTrace("%s:%i - Couldn't find '%s'\n", _FL, Str);
	
	#endif

	return false;
}

bool GTrayIcon::Visible()
{
	#if WIN32NATIVE
	return d->TrayIcon.a.cbSize != 0;
	#else
	return d->Visible;
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
			HICON Cur = d->Icon[d->Val];

			ZeroObj(d->TrayIcon);
			if (IsWin9x)
			{
				d->TrayIcon.a.cbSize = sizeof(d->TrayIcon.a);
				d->TrayIcon.a.hWnd = d->Parent ? d->Parent->Handle() : 0;
				d->TrayIcon.a.uID = d->MyId = Id++;
				d->TrayIcon.a.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
				d->TrayIcon.a.uCallbackMessage = M_TRAY_NOTIFY;
				d->TrayIcon.a.hIcon = Cur;
				
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
				d->TrayIcon.w.hIcon = Cur;
				StrncpyW(d->TrayIcon.w.szTip, (char16*)(NameW()?NameW():L""), sizeof(d->TrayIcon.w.szTip));

				Shell_NotifyIconW(NIM_ADD, &d->TrayIcon.w);
			}
			
			#elif defined(__GTK_H__)

			if (d->tray_icon)
			{
				if (d->Val < 0 || d->Val >= d->Icon.Length())
					d->Val = 0;
				if (d->Val < d->Icon.Length())
				{
					GTrayIconPrivate::IconRef Ref = d->Icon[d->Val];
					if (Ref)
					{
						Gtk::gtk_status_icon_set_from_pixbuf(d->tray_icon, Ref);
						Gtk::gtk_status_icon_set_tooltip(d->tray_icon, GBase::Name());
						Gtk::gtk_status_icon_set_visible(d->tray_icon, true);
					}
				}
				else LgiTrace("%s:%i - No icon to show in tray.\n", _FL);
			}
			else LgiTrace("%s:%i - No tray icon to hide.\n", _FL);
			
			#elif defined MAC

			if (!d->Visible)
			{
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

			#elif defined(__GTK_H__)

			if (d->tray_icon)
				gtk_status_icon_set_visible(d->tray_icon, false);
			else
				LgiTrace("%s:%i - No tray icon to hide.\n", _FL);
			
			#elif defined MAC
			
			#ifdef COCOA
			#else
			RestoreApplicationDockTileImage();
			#endif

			#endif
		}
	}

	d->Visible = v;
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
			HICON Cur = d->Icon[d->Val];

			ZeroObj(d->TrayIcon);

			if (IsWin9x)
			{
				d->TrayIcon.a.cbSize = sizeof(d->TrayIcon.a);
				d->TrayIcon.a.hWnd = d->Parent ? d->Parent->Handle() : 0;
				d->TrayIcon.a.uID = d->MyId;
				d->TrayIcon.a.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
				d->TrayIcon.a.uCallbackMessage = M_TRAY_NOTIFY;
				d->TrayIcon.a.hIcon = Cur;
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
				d->TrayIcon.w.hIcon = Cur;
				StrncpyW(d->TrayIcon.w.szTip, (char16*)(NameW()?NameW():L""), sizeof(d->TrayIcon.w.szTip));
				Shell_NotifyIconW(NIM_MODIFY, &d->TrayIcon.w);
			}
		}
		
		#elif defined __GTK_H__

		if (d->tray_icon)
		{
			if (d->Val < 0 || d->Val >= d->Icon.Length())
				d->Val = 0;
			if (d->Val < d->Icon.Length())
			{
				GTrayIconPrivate::IconRef Ref = d->Icon[d->Val];
				if (Ref)
					Gtk::gtk_status_icon_set_from_pixbuf(d->tray_icon, Ref);
			}
			else LgiTrace("%s:%i - No icon to show in tray.\n", _FL);
		}
		else LgiTrace("%s:%i - No tray icon to hide.\n", _FL);
		
		
		#elif defined MAC
		
		#ifdef COCOA
		#else
		GSurface *t = d->Icon[d->Val];
		if (t)
		{
			CGContextRef c = BeginCGContextForApplicationDockTile();
			if (c)
			{
				// CGContextTranslateCTM(c, 0, 128); 
				// CGContextScaleCTM(c, 1.0, -1.0); 

				GScreenDC Dc((GView*)0, c);
				GMemDC m;
				if (m.Create(t->X()*4, t->Y()*4, 32))
				{
					double Sx = (double) t->X() / m.X();
					double Sy = (double) t->Y() / m.Y();
					for (int y=0; y<m.Y(); y++)
					{
						int Y = (int) (y * Sy);
						System32BitPixel *s = (System32BitPixel*)(*t)[Y];
						System32BitPixel *d = (System32BitPixel*)(m)[m.Y()-y-1];
						
						for (int x=0; x<m.X(); x++)
						{
							d[x] = s[(int)(x * Sx)];
						}
					}
					
					Dc.Blt(Dc.X()-m.X()-1, 0, &m);
				}
				else
				{
					LgiAssert(!"Failed to create memory bitmap");
				}

				CGContextFlush(c);
				EndCGContextForApplicationDockTile(c);
			}
			else
			{
				RestoreApplicationDockTileImage();
			}
		}
		else
		{
			RestoreApplicationDockTileImage();
		}
		#endif
		
		#endif
	}
}

GMessage::Result GTrayIcon::OnEvent(GMessage *Message)
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

