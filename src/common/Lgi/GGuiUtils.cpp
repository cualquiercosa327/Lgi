#include <stdio.h>
#include "Lgi.h"
#include "GSkinEngine.h"
#include "GToken.h"
#ifdef LINUX
#include "LgiWinManGlue.h"
#endif

//////////////////////////////////////////////////////////////////////////////////
#ifndef VK_CONTEXTKEY
#define VK_CONTEXTKEY 0x5d
#endif

bool GKey::IsContextMenu()
{
	#if WIN32NATIVE
	return vkey == VK_CONTEXTKEY;
	#else
	return false;
	#endif
}

//////////////////////////////////////////////////////////////////////////////////
bool GMouse::IsContextMenu()
{
	if (Right())
		return true;
	
	#ifdef MAC
	if (Left() AND Ctrl())
		return true;
	#endif
	
	return false;
}

bool GMouse::ToScreen()
{
	if (ViewCoords && Target)
	{
		GdcPt2 p(x, y);
		Target->PointToScreen(p);
		x = p.x; y = p.y;
		ViewCoords = false;
		return true;
	}
	else
	{
		printf("%s:%i - Error: Target=%p ViewCoords=%i\n", __FILE__, __LINE__, Target, ViewCoords);
	}
	
	return false;
}

bool GMouse::ToView()
{
	if (!ViewCoords AND Target)
	{
		GdcPt2 p(x, y);
		Target->PointToView(p);
		x = p.x; y = p.y;
		ViewCoords = true;
		return true;
	}
	else
	{
		printf("%s:%i - Error: Target=%p ViewCoords=%i\n",
			__FILE__,
			__LINE__,
			Target,
			ViewCoords);
	}
	
	return false;
}

//////////////////////////////////////////////////////////////////////////////
uint32 _LgiColours[LC_MAXIMUM];

#define ReadColourConfig(def)	_lgi_read_colour_config("Colour."#def, _LgiColours+i++)

/*
int _hex_to_int(char h)
{
	if (h >= '0' AND h <= '9')
	{
		return h - '0';
	}
	else if (h >= 'A' AND h <= 'F')
	{
		return h - 'A' + 10;
	}
	else if (h >= 'a' AND h <= 'f')
	{
		return h - 'a' + 10;
	}

	LgiAssert(0);
	return 0;
}
*/

bool _lgi_read_colour_config(const char *Tag, uint32 *c)
{
	if (!c || !Tag)
		return false;

	GXmlTag *Col = LgiApp->GetConfig(Tag);
	if (!Col)
		return false;

	char *h;
	if (!(h = Col->GetAttr("Hex")))
		return false;

	if (*h == '#') h++;
	int n = htoi(h);

	*c = Rgb24( n>>16, n>>8, n );
	return true;
}

////////////////////////////////////////////////////////////////////////////
#ifdef __GTK_H__
using namespace Gtk;
COLOUR ColTo24(Gtk::GdkColor &c)
{
	return Rgb24(c.red >> 8, c.green >> 8, c.blue >> 8);
}
#endif

void LgiInitColours()
{
	int i = 0;

	// Basic colours
	_LgiColours[i++] = Rgb24(0, 0, 0); // LC_BLACK
	_LgiColours[i++] = Rgb24(0x40, 0x40, 0x40); // LC_DKGREY
	_LgiColours[i++] = Rgb24(0x80, 0x80, 0x80); // LC_MIDGREY
	_LgiColours[i++] = Rgb24(0xc0, 0xc0, 0xc0); // LC_LTGREY
	_LgiColours[i++] = Rgb24(0xff, 0xff, 0xff); // LC_WHITE

	// Variable colours
	#if defined _XP_CTRLS
	_LgiColours[i++] = Rgb24(0x42, 0x27, 0x63); // LC_SHADOW
	_LgiColours[i++] = Rgb24(0x7a, 0x54, 0xa9); // LC_LOW
	_LgiColours[i++] = Rgb24(0xbc, 0xa9, 0xd4); // LC_MED
	_LgiColours[i++] = Rgb24(0xdd, 0xd4, 0xe9); // LC_HIGH
	_LgiColours[i++] = Rgb24(0xff, 0xff, 0xff); // LC_LIGHT
	_LgiColours[i++] = Rgb24(0xbc, 0xa9, 0xd4); // LC_DIALOG
	_LgiColours[i++] = Rgb24(0xeb, 0xe6, 0xf2); // LC_WORKSPACE
	_LgiColours[i++] = Rgb24(0x35, 0x1f, 0x4f); // LC_TEXT
	_LgiColours[i++] = Rgb24(0xbf, 0x67, 0x93); // LC_SELECTION
	_LgiColours[i++] = Rgb24(0xff, 0xff, 0xff); // LC_SEL_TEXT
	_LgiColours[i++] = Rgb24(0x70, 0x3a, 0xec); // LC_ACTIVE_TITLE
	_LgiColours[i++] = Rgb24(0xff, 0xff, 0xff); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[i++] = Rgb24(0x80, 0x80, 0x80); // LC_INACTIVE_TITLE
	_LgiColours[i++] = Rgb24(0x40, 0x40, 0x40); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[i++] = Rgb24(0xbc, 0xa9, 0xd4); // LC_MENU_BACKGROUND
	_LgiColours[i++] = Rgb24(0x35, 0x1f, 0x4f); // LC_MENU_TEXT
	#elif defined SKIN_MAGIC
	_LgiColours[i++] = GetSysColor(COLOR_3DDKSHADOW); // LC_SHADOW
	_LgiColours[i++] = GetSysColor(COLOR_3DSHADOW); // LC_LOW
	_LgiColours[i++] = Rgb24(0xf0, 0xf1, 0xf8); // LC_MED
	_LgiColours[i++] = GetSysColor(COLOR_3DLIGHT); // LC_HIGH, 
	_LgiColours[i++] = GetSysColor(COLOR_3DHIGHLIGHT); // LC_LIGHT
	_LgiColours[i++] = GetSysColor(COLOR_3DFACE); // LC_DIALOG
	_LgiColours[i++] = Rgb24(0xc7, 0xc9, 0xd7); // LC_WORKSPACE
	_LgiColours[i++] = GetSysColor(COLOR_WINDOWTEXT); // LC_TEXT
	_LgiColours[i++] = GetSysColor(COLOR_HIGHLIGHT); // LC_SELECTION
	_LgiColours[i++] = GetSysColor(COLOR_HIGHLIGHTTEXT); // LC_SEL_TEXT
	_LgiColours[i++] = GetSysColor(COLOR_ACTIVECAPTION); // LC_ACTIVE_TITLE
	_LgiColours[i++] = GetSysColor(COLOR_CAPTIONTEXT); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[i++] = GetSysColor(COLOR_INACTIVECAPTION); // LC_INACTIVE_TITLE
	_LgiColours[i++] = GetSysColor(COLOR_INACTIVECAPTIONTEXT); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[i++] = GetSysColor(COLOR_MENU); // LC_MENU_BACKGROUND
	_LgiColours[i++] = GetSysColor(COLOR_MENUTEXT); // LC_MENU_TEXT
	#elif defined(_WINDOWS)
	_LgiColours[i++] = GetSysColor(COLOR_3DDKSHADOW); // LC_SHADOW
	_LgiColours[i++] = GetSysColor(COLOR_3DSHADOW); // LC_LOW
	_LgiColours[i++] = GetSysColor(COLOR_3DFACE); // LC_MED
	_LgiColours[i++] = GetSysColor(COLOR_3DLIGHT); // LC_HIGH, 
	_LgiColours[i++] = GetSysColor(COLOR_3DHIGHLIGHT); // LC_LIGHT
	_LgiColours[i++] = GetSysColor(COLOR_3DFACE); // LC_DIALOG
	_LgiColours[i++] = GetSysColor(COLOR_WINDOW); // LC_WORKSPACE
	_LgiColours[i++] = GetSysColor(COLOR_WINDOWTEXT); // LC_TEXT
	_LgiColours[i++] = GetSysColor(COLOR_HIGHLIGHT); // LC_SELECTION
	_LgiColours[i++] = GetSysColor(COLOR_HIGHLIGHTTEXT); // LC_SEL_TEXT
	_LgiColours[i++] = GetSysColor(COLOR_ACTIVECAPTION); // LC_ACTIVE_TITLE
	_LgiColours[i++] = GetSysColor(COLOR_CAPTIONTEXT); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[i++] = GetSysColor(COLOR_INACTIVECAPTION); // LC_INACTIVE_TITLE
	_LgiColours[i++] = GetSysColor(COLOR_INACTIVECAPTIONTEXT); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[i++] = GetSysColor(COLOR_MENU); // LC_MENU_BACKGROUND
	_LgiColours[i++] = GetSysColor(COLOR_MENUTEXT); // LC_MENU_TEXT
	#elif defined __GTK_H__
	Gtk::GtkSettings *set = Gtk::gtk_settings_get_default();
	if (!set)
	{
		printf("%s:%i - gtk_settings_get_for_screen failed.\n", __FILE__, __LINE__);
		return;
	}
	
	char PropName[] = "gtk-color-scheme";
	Gtk::gchararray Value = 0;
	Gtk::g_object_get(G_OBJECT(set), PropName, &Value, 0);	
	GToken Lines(Value, "\n");
	GHashTbl<char*, COLOUR> Colours;
	for (int i=0; i<Lines.Length(); i++)
	{
		char *var = Lines[i];
		char *col = strchr(var, ':');
		if (col)
		{
			*col = 0;

			char *val = col + 1;
			if (*val == ' ') val++;
			if (*val == '#') val++;
			uint64 c = htoi64(val);
			COLOUR c24 = ((c >> 8) & 0xff) |
						((c >> 16) & 0xff00) |
						((c >> 24) & 0xff0000);
			Colours.Add(var, c24);
		}
	}

	_LgiColours[i++] = GdcMixColour(Colours.Find("bg_color"), Rgb24(0, 0, 0), 0.25); // LC_SHADOW
	_LgiColours[i++] = GdcMixColour(Colours.Find("bg_color"), Rgb24(0, 0, 0), 0.5); // LC_LOW
	_LgiColours[i++] = Colours.Find("bg_color"); // LC_MED
	_LgiColours[i++] = GdcMixColour(Colours.Find("bg_color"), Rgb24(255, 255, 255), 0.5); // LC_HIGH
	_LgiColours[i++] = GdcMixColour(Colours.Find("bg_color"), Rgb24(255, 255, 255), 0.25); // LC_LIGHT
	_LgiColours[i++] = Colours.Find("bg_color"); // LC_DIALOG
	_LgiColours[i++] = Colours.Find("base_color"); // LC_WORKSPACE
	_LgiColours[i++] = Colours.Find("text_color"); // LC_TEXT
	_LgiColours[i++] = Colours.Find("selected_bg_color"); // LC_SELECTION
	_LgiColours[i++] = Colours.Find("selected_fg_color"); // LC_SEL_TEXT
	_LgiColours[i++] = Rgb24(0x70, 0x3a, 0xec); // LC_ACTIVE_TITLE
	_LgiColours[i++] = Rgb24(0xff, 0xff, 0xff); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[i++] = Rgb24(0x80, 0x80, 0x80); // LC_INACTIVE_TITLE
	_LgiColours[i++] = Rgb24(0x40, 0x40, 0x40); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[i++] = Colours.Find("bg_color"); // LC_MENU_BACKGROUND
	_LgiColours[i++] = Colours.Find("text_color"); // LC_MENU_TEXT

	#else // defaults for non-windows, plain greys

	#ifdef LINUX
	WmColour c;
	Proc_LgiWmGetColour WmGetColour = 0;
	GLibrary *WmLib = LgiApp->GetWindowManagerLib();
	if (WmLib)
	{
		WmGetColour = (Proc_LgiWmGetColour) WmLib->GetAddress("LgiWmGetColour");
	}

	#define SetCol(def) \
		if (WmGetColour AND WmGetColour(i, &c)) \
			_LgiColours[i++] = Rgb24(c.r, c.g, c.b); \
		else \
			_LgiColours[i++] = def;

	#else

	#define SetCol(def) \
		_LgiColours[i++] = def;

	#endif

	SetCol(Rgb24(64, 64, 64)); // LC_SHADOW
	SetCol(Rgb24(128, 128, 128)); // LC_LOW

	//#ifdef BEOS
	SetCol(Rgb24(216, 216, 216)); // LC_MED
	//#else
	//SetCol(Rgb24(230, 230, 230)); // LC_MED
	//#endif

	SetCol(Rgb24(230, 230, 230)); // LC_HIGH
	SetCol(Rgb24(255, 255, 255)); // LC_LIGHT
	SetCol(Rgb24(216, 216, 216)); // LC_DIALOG
	SetCol(Rgb24(0xff, 0xff, 0xff)); // LC_WORKSPACE
	SetCol(Rgb24(0, 0, 0)); // LC_TEXT
	SetCol(Rgb24(0x4a, 0x59, 0xa5)); // LC_SELECTION
	SetCol(Rgb24(0xff, 0xff, 0xff)); // LC_SEL_TEXT
	SetCol(Rgb24(0, 0, 0x80)); // LC_ACTIVE_TITLE
	SetCol(Rgb24(0xff, 0xff, 0xff)); // LC_ACTIVE_TITLE_TEXT
	SetCol(Rgb24(0x80, 0x80, 0x80)); // LC_INACTIVE_TITLE
	SetCol(Rgb24(0x40, 0x40, 0x40)); // LC_INACTIVE_TITLE_TEXT
	SetCol(Rgb24(222, 222, 222)); // LC_MENU_BACKGROUND
	SetCol(Rgb24(0, 0, 0)); // LC_MENU_TEXT
	#endif

	// Tweak
	if (LgiGetOs() == LGI_OS_WINNT ||
		LgiGetOs() == LGI_OS_WIN9X)
	{
		// Win32 doesn't seem to get this right, so we just tweak it here
		#define MixComp(a)		( (a(_LgiColours[7])+a(_LgiColours[9])) / 2 )
		_LgiColours[8] = Rgb24(MixComp(R24), MixComp(G24), MixComp(B24));
	}

	// Read any settings out of config
	i = 5;
	ReadColourConfig(LC_SHADOW);
	ReadColourConfig(LC_LOW);
	ReadColourConfig(LC_MED);
	ReadColourConfig(LC_HIGH);
	ReadColourConfig(LC_LIGHT);
	ReadColourConfig(LC_DIALOG);
	ReadColourConfig(LC_WORKSPACE);
	ReadColourConfig(LC_TEXT);
	ReadColourConfig(LC_SELECTION);
	ReadColourConfig(LC_SEL_TEXT);
	ReadColourConfig(LC_ACTIVE_TITLE);
	ReadColourConfig(LC_ACTIVE_TITLE_TEXT);
	ReadColourConfig(LC_INACTIVE_TITLE);
	ReadColourConfig(LC_INACTIVE_TITLE_TEXT);
}

/// \brief Returns a 24bit representation of a system colour.
///
/// You can use the macros R24(), G24() and B24() to extract the colour
/// components
COLOUR LgiColour
(
	/// The system colour to return.
	/// \sa The define LC_BLACK and those that follow in LgiDefs.h
	int i
)
{
	if (GApp::SkinEngine AND
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_COLOUR))
	{
		return GApp::SkinEngine->GetColour(i);
	}
	
	return i<LC_MAXIMUM ? _LgiColours[i] : 0;
}

#if WIN32NATIVE

#if !defined(DM_POSITION)
#define DM_POSITION         0x00000020L
#endif

typedef WINUSERAPI BOOL (WINAPI *pEnumDisplayDevicesA)(PVOID, DWORD, PDISPLAY_DEVICEA, DWORD);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplayDevicesW)(PVOID, DWORD, PDISPLAY_DEVICEW, DWORD);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplaySettingsA)(LPCSTR lpszDeviceName, DWORD iModeNum, LPDEVMODEA lpDevMode);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplaySettingsW)(LPCWSTR lpszDeviceName, DWORD iModeNum, LPDEVMODEW lpDevMode);

union DISPLAY_DEVICEU
{
	DISPLAY_DEVICEA a;
	DISPLAY_DEVICEW w;
};

union DEVMODEU
{
	DEVMODEA a[2];
	DEVMODEW w[2];
};

#endif

bool LgiGetDisplays(::GArray<GDisplayInfo*> &Displays, GRect *AllDisplays)
{
	#if WIN32NATIVE
	if (AllDisplays)
		AllDisplays->ZOff(-1, -1);

	bool Win9x = IsWin9x;
	GLibrary User32("User32");
	DISPLAY_DEVICEU disp;
	ZeroObj(disp);
	if (Win9x)
		disp.a.cb = sizeof(disp.a);
	else
		disp.w.cb = sizeof(disp.w);
	pEnumDisplayDevicesA EnumDisplayDevicesA = (pEnumDisplayDevicesA) User32.GetAddress("EnumDisplayDevicesA");
	pEnumDisplayDevicesW EnumDisplayDevicesW = (pEnumDisplayDevicesW) User32.GetAddress("EnumDisplayDevicesW");
	pEnumDisplaySettingsA EnumDisplaySettingsA = (pEnumDisplaySettingsA) User32.GetAddress("EnumDisplaySettingsA");
	pEnumDisplaySettingsW EnumDisplaySettingsW = (pEnumDisplaySettingsW) User32.GetAddress("EnumDisplaySettingsW");

	for (int i=0;
		Win9x ? EnumDisplayDevicesA(0, i, &disp.a, 0) : EnumDisplayDevicesW(0, i, &disp.w, 0);
		i++)
	{
		DEVMODEU mode;
		ZeroObj(mode);
		if (Win9x)
		{
			mode.a[0].dmSize = sizeof(mode.a[0]);
			mode.a[0].dmDriverExtra = sizeof(mode.a[0]);
		}
		else
		{
			mode.w[0].dmSize = sizeof(mode.w[0]);
			mode.w[0].dmDriverExtra = sizeof(mode.w[0]);
		}

		if
		(
			Win9x
			?
			EnumDisplaySettingsA((char*)disp.a.DeviceName, ENUM_CURRENT_SETTINGS, mode.a)
			:
			EnumDisplaySettingsW(disp.w.DeviceName, ENUM_CURRENT_SETTINGS, mode.w)
		)
		{
			GDisplayInfo *Dsp = new GDisplayInfo;
			if (Dsp)
			{
				if (Win9x)
					Dsp->r.ZOff(mode.a->dmPelsWidth-1, mode.a->dmPelsHeight-1);
				else
					Dsp->r.ZOff(mode.w->dmPelsWidth-1, mode.w->dmPelsHeight-1);

				if ((IsWin9x ? mode.a->dmFields : mode.w->dmFields) & DM_POSITION)
				{
					if (Win9x)
						Dsp->r.Offset(mode.a->dmPosition.x, mode.a->dmPosition.y);
					else
						Dsp->r.Offset(mode.w->dmPosition.x, mode.w->dmPosition.y);
				}
				if (AllDisplays)
				{
					if (AllDisplays->Valid())
						AllDisplays->Union(&Dsp->r);
					else
						*AllDisplays = Dsp->r;
				}

				if (Win9x)
					disp.a.cb = sizeof(disp.a);
				else
					disp.w.cb = sizeof(disp.w);

				if (Win9x)
				{
					Dsp->BitDepth = mode.a->dmBitsPerPel;
					Dsp->Refresh = mode.a->dmDisplayFrequency;
					Dsp->Name = LgiFromNativeCp((char*)disp.a.DeviceString);
					Dsp->Device = LgiFromNativeCp((char*)disp.a.DeviceName);

					DISPLAY_DEVICEA temp = disp.a;
					if (EnumDisplayDevicesA(temp.DeviceName, 0, &disp.a, 0))
					{
						Dsp->Monitor = LgiFromNativeCp((char*)disp.a.DeviceString);
					}
				}
				else
				{
					Dsp->BitDepth = mode.w->dmBitsPerPel;
					Dsp->Refresh = mode.w->dmDisplayFrequency;
					Dsp->Name = LgiNewUtf16To8(disp.w.DeviceString);
					Dsp->Device = LgiNewUtf16To8(disp.w.DeviceName);

					DISPLAY_DEVICEW temp = disp.w;
					if (EnumDisplayDevicesW(temp.DeviceName, 0, &disp.w, 0))
					{
						Dsp->Monitor = LgiNewUtf16To8(disp.w.DeviceString);
					}
				}

				Displays.Add(Dsp);
			}
		}

		if (Win9x)
			disp.a.cb = sizeof(disp.a);
		else
			disp.w.cb = sizeof(disp.w);

	}

	#endif

	return Displays.Length() > 0;
}

void GetChildrenList(GViewI *w, List<GViewI> &l)
{
	if (w)
	{
		GViewIterator *it = w->IterateViews();
		if (it)
		{
			for (GViewI *v = it->First(); v; v = it->Next())
			{
				#if WIN32NATIVE
				int Style = GetWindowLong(v->Handle(), GWL_STYLE);
				if (TestFlag(Style, WS_VISIBLE) AND
					!TestFlag(Style, WS_DISABLED))
				{
					if (TestFlag(Style, WS_TABSTOP))
					{
						l.Insert(v);
					}

					GetChildrenList(v, l);
				}
				#else
				if (v->Visible() && v->Enabled())
				{
					if (v->GetTabStop())
					{
						l.Insert(v);
					}

					GetChildrenList(v, l);
				}
				#endif
			}

			DeleteObj(it);
		}
	}
}


GViewI *GetNextTabStop(GViewI *v, bool Back)
{
	if (v)
	{
		GWindow *Wnd = v->GetWindow();
		if (Wnd)
		{
			List<GViewI> All;
			GetChildrenList(Wnd, All);

			int MyIndex = All.IndexOf(v);
			if (MyIndex >= 0)
			{
				int Inc = Back ? -1 : 1;
				int NewIndex = (MyIndex + All.Length() + Inc) % All.Length();
				return All.ItemAt(NewIndex);
			}
			else
			{
				return All.First();
			}
		}
	}

	return 0;
}



