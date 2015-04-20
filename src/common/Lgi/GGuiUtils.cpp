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
	#if WINNATIVE
	return !IsChar && vkey == VK_CONTEXTKEY;
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
	if (Left() && Ctrl())
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
		printf("%s:%i - Error: Target=%p ViewCoords=%i\n", _FL, Target, ViewCoords);
	}
	
	return false;
}

bool GMouse::ToView()
{
	if (!ViewCoords && Target)
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
COLOUR ColTo24(Gtk::GdkColor &c)
{
	return Rgb24(c.red >> 8, c.green >> 8, c.blue >> 8);
}
#endif

#if defined(WINDOWS)
static uint32 ConvertWinColour(uint32 c)
{
	return Rgb24(GetRValue(c), GetGValue(c), GetBValue(c));
}
#elif defined(BEOS)
static uint32 ConvertHaikuColour(color_which c)
{
	rgb_color rgb = ui_color(c);
	return Rgb24(rgb.red, rgb.green, rgb.blue);
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
	_LgiColours[i++] = Rgb24(0xbf, 0x67, 0x93); // LC_FOCUS_SEL_BACK
	_LgiColours[i++] = Rgb24(0xff, 0xff, 0xff); // LC_FOCUS_SEL_FORE
	_LgiColours[i++] = Rgb24(0x70, 0x3a, 0xec); // LC_ACTIVE_TITLE
	_LgiColours[i++] = Rgb24(0xff, 0xff, 0xff); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[i++] = Rgb24(0x80, 0x80, 0x80); // LC_INACTIVE_TITLE
	_LgiColours[i++] = Rgb24(0x40, 0x40, 0x40); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[i++] = Rgb24(0xbc, 0xa9, 0xd4); // LC_MENU_BACKGROUND
	_LgiColours[i++] = Rgb24(0x35, 0x1f, 0x4f); // LC_MENU_TEXT
	_LgiColours[i++] = Rgb24(0xbc, 0xa9, 0xd4); // LC_NON_FOCUS_SEL_BACK
	_LgiColours[i++] = Rgb24(0x35, 0x1f, 0x4f); // LC_NON_FOCUS_SEL_FORE
	LgiAssert(i == LC_MAXIMUM);
	
	#elif defined BEOS
	
	GColour Black(0, 0, 0);
	GColour White(255, 255, 255);
	GColour Med(ConvertHaikuColour(B_PANEL_BACKGROUND_COLOR), 24);
	GColour SelFore(ConvertHaikuColour(B_MENU_SELECTED_ITEM_TEXT_COLOR), 24);
	GColour SelBack(ConvertHaikuColour(B_CONTROL_HIGHLIGHT_COLOR), 24);
	
	// printf("Sel - Fore=%s, Back=%s\n", SelFore.GetStr(), SelBack.GetStr());
	
	_LgiColours[i++] = Med.Mix(Black, 0.4f).c24(); // LC_SHADOW
	_LgiColours[i++] = Med.Mix(Black, 0.25f).c24(); // LC_LOW
	_LgiColours[i++] = Med.c24(); // LC_MED
	_LgiColours[i++] = Med.Mix(White, 0.25f).c24(); // LC_HIGH, 
	_LgiColours[i++] = Med.Mix(White, 0.4f).c24(); // LC_LIGHT
	_LgiColours[i++] = ConvertHaikuColour(B_PANEL_BACKGROUND_COLOR); // LC_DIALOG
	_LgiColours[i++] = ConvertHaikuColour(B_DOCUMENT_BACKGROUND_COLOR); // LC_WORKSPACE
	_LgiColours[i++] = ConvertHaikuColour(B_CONTROL_TEXT_COLOR); // LC_TEXT
	_LgiColours[i++] = SelBack.c24(); // LC_FOCUS_SEL_BACK
	_LgiColours[i++] = SelFore.c24(); // LC_FOCUS_SEL_FORE
	_LgiColours[i++] = ConvertHaikuColour(B_WINDOW_TAB_COLOR); // LC_ACTIVE_TITLE
	_LgiColours[i++] = ConvertHaikuColour(B_WINDOW_TEXT_COLOR); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[i++] = ConvertHaikuColour(B_WINDOW_INACTIVE_TAB_COLOR); // LC_INACTIVE_TITLE
	_LgiColours[i++] = ConvertHaikuColour(B_WINDOW_INACTIVE_TEXT_COLOR); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[i++] = ConvertHaikuColour(B_MENU_BACKGROUND_COLOR); // LC_MENU_BACKGROUND
	_LgiColours[i++] = ConvertHaikuColour(B_MENU_ITEM_TEXT_COLOR); // LC_MENU_TEXT
	_LgiColours[i++] = SelBack.Mix(White, 0.4f).c24(); // LC_NON_FOCUS_SEL_BACK
	_LgiColours[i++] = SelFore.Mix(White, 0.4f).c24(); // LC_NON_FOCUS_SEL_FORE
	LgiAssert(i == LC_MAXIMUM);	
	
	#elif defined __GTK_H__

	Gtk::GtkSettings *set = Gtk::gtk_settings_get_default();
	if (!set)
	{
		printf("%s:%i - gtk_settings_get_for_screen failed.\n", _FL);
		return;
	}
	
	char PropName[] = "gtk-color-scheme";
	Gtk::gchararray Value = 0;
	Gtk::g_object_get(set, PropName, &Value, 0);	
	GToken Lines(Value, "\n");
	GHashTbl<char*, int> Colours(0, false, NULL, -1);
	for (int i=0; i<Lines.Length(); i++)
	{
		char *var = Lines[i];
		char *col = strchr(var, ':');
		if (col)
		{
			*col++ = 0;
			
			char *val = col;
			if (*val == ' ') val++;
			if (*val == '#') val++;
			uint64 c = htoi64(val);
			// printf("%s -> %llX\n", val, c);
			COLOUR c24 = ((c >> 8) & 0xff) |
						((c >> 16) & 0xff00) |
						((c >> 24) & 0xff0000);

			// printf("ParseSysColour %s = %x\n", var, c24);
			Colours.Add(var, c24);
		}
	}
	#define LookupColour(name, default) ((Colours.Find(name) >= 0) ? Colours.Find(name) : default)

	COLOUR Med = LookupColour("bg_color", Rgb24(0xe8, 0xe8, 0xe8));
	COLOUR White = Rgb24(255, 255, 255);
	COLOUR Black = Rgb24(0, 0, 0);
	COLOUR Sel = Rgb24(0x33, 0x99, 0xff);
	_LgiColours[i++] = GdcMixColour(Med, Black, 0.25); // LC_SHADOW
	_LgiColours[i++] = GdcMixColour(Med, Black, 0.5); // LC_LOW
	_LgiColours[i++] = Med; // LC_MED
	_LgiColours[i++] = GdcMixColour(Med, White, 0.5); // LC_HIGH
	_LgiColours[i++] = GdcMixColour(Med, White, 0.25); // LC_LIGHT
	_LgiColours[i++] = Med; // LC_DIALOG
	_LgiColours[i++] = LookupColour("base_color", White); // LC_WORKSPACE
	_LgiColours[i++] = LookupColour("text_color", Black); // LC_TEXT
	_LgiColours[i++] = LookupColour("selected_bg_color", Sel); // LC_FOCUS_SEL_BACK
	_LgiColours[i++] = LookupColour("selected_fg_color", White); // LC_FOCUS_SEL_FORE
	_LgiColours[i++] = LookupColour("selected_bg_color", Sel); // LC_ACTIVE_TITLE
	_LgiColours[i++] = LookupColour("selected_fg_color", White); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[i++] = Rgb24(0xc0, 0xc0, 0xc0); // LC_INACTIVE_TITLE
	_LgiColours[i++] = Rgb24(0x80, 0x80, 0x80); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[i++] = LookupColour("bg_color", White); // LC_MENU_BACKGROUND
	_LgiColours[i++] = LookupColour("text_color", Black); // LC_MENU_TEXT
	_LgiColours[i++] = GdcMixColour(LookupColour("selected_bg_color", Sel), _LgiColours[11]); // LC_NON_FOCUS_SEL_BACK
	_LgiColours[i++] = LookupColour("selected_fg_color", White); // LC_NON_FOCUS_SEL_FORE
	LgiAssert(i <= LC_MAXIMUM);

	#elif defined(WINDOWS)

	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_3DDKSHADOW)); // LC_SHADOW
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_3DSHADOW)); // LC_LOW
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_3DFACE)); // LC_MED
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_3DLIGHT)); // LC_HIGH, 
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_3DHIGHLIGHT)); // LC_LIGHT
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_3DFACE)); // LC_DIALOG
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_WINDOW)); // LC_WORKSPACE
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_WINDOWTEXT)); // LC_TEXT
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_HIGHLIGHT)); // LC_FOCUS_SEL_BACK
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_HIGHLIGHTTEXT)); // LC_FOCUS_SEL_FORE
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_ACTIVECAPTION)); // LC_ACTIVE_TITLE
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_CAPTIONTEXT)); // LC_ACTIVE_TITLE_TEXT
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_INACTIVECAPTION)); // LC_INACTIVE_TITLE
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_INACTIVECAPTIONTEXT)); // LC_INACTIVE_TITLE_TEXT
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_MENU)); // LC_MENU_BACKGROUND
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_MENUTEXT)); // LC_MENU_TEXT
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_BTNFACE)); // LC_NON_FOCUS_SEL_BACK
	_LgiColours[i++] = ConvertWinColour(GetSysColor(COLOR_BTNTEXT)); // LC_NON_FOCUS_SEL_FORE
	LgiAssert(i <= LC_MAXIMUM);

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
		if (WmGetColour && WmGetColour(i, &c)) \
			_LgiColours[i++] = Rgb24(c.r, c.g, c.b); \
		else \
			_LgiColours[i++] = def;

	#else // MAC

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
	SetCol(Rgb24(0x4a, 0x59, 0xa5)); // LC_FOCUS_SEL_BACK
	SetCol(Rgb24(0xff, 0xff, 0xff)); // LC_FOCUS_SEL_FORE
	SetCol(Rgb24(0, 0, 0x80)); // LC_ACTIVE_TITLE
	SetCol(Rgb24(0xff, 0xff, 0xff)); // LC_ACTIVE_TITLE_TEXT
	SetCol(Rgb24(0x80, 0x80, 0x80)); // LC_INACTIVE_TITLE
	SetCol(Rgb24(0x40, 0x40, 0x40)); // LC_INACTIVE_TITLE_TEXT
	SetCol(Rgb24(222, 222, 222)); // LC_MENU_BACKGROUND
	SetCol(Rgb24(0, 0, 0)); // LC_MENU_TEXT
	SetCol(Rgb24(222, 222, 222)); // LC_NON_FOCUS_SEL_BACK
	SetCol(Rgb24(0, 0, 0)); // LC_NON_FOCUS_SEL_FORE
	#endif

	// Tweak
	if (LgiGetOs() == LGI_OS_WIN32
		||
		LgiGetOs() == LGI_OS_WIN64)
	{
		// Win32 doesn't seem to get this right, so we just tweak it here
		#define MixComp(a)		( (a(_LgiColours[7])+a(_LgiColours[9])) / 2 )
		_LgiColours[8] = Rgb24(MixComp(R24), MixComp(G24), MixComp(B24));
	}
	
	_LgiColours[23] = Rgb24(0xff, 0xe0, 0x00);

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
	ReadColourConfig(LC_FOCUS_SEL_BACK);
	ReadColourConfig(LC_FOCUS_SEL_FORE);
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
	if (GApp::SkinEngine &&
		TestFlag(GApp::SkinEngine->GetFeatures(), GSKIN_COLOUR))
	{
		return GApp::SkinEngine->GetColour(i);
	}
	
	return i<LC_MAXIMUM ? _LgiColours[i] : 0;
}

#if WINNATIVE

#if !defined(DM_POSITION)
#define DM_POSITION         0x00000020L
#endif

typedef WINUSERAPI BOOL (WINAPI *pEnumDisplayDevicesA)(PVOID, DWORD, PDISPLAY_DEVICEA, DWORD);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplayDevicesW)(PVOID, DWORD, PDISPLAY_DEVICEW, DWORD);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplaySettingsA)(LPCSTR lpszDeviceName, DWORD iModeNum, LPDEVMODEA lpDevMode);
typedef WINUSERAPI BOOL (WINAPI *pEnumDisplaySettingsW)(LPCWSTR lpszDeviceName, DWORD iModeNum, LPDEVMODEW lpDevMode);

#endif

bool LgiGetDisplays(::GArray<GDisplayInfo*> &Displays, GRect *AllDisplays)
{
	#if WINNATIVE
	if (AllDisplays)
		AllDisplays->ZOff(-1, -1);

	GLibrary User32("User32");
	DISPLAY_DEVICEW disp;
	ZeroObj(disp);
	disp.cb = sizeof(disp);
	pEnumDisplayDevicesW EnumDisplayDevicesW = (pEnumDisplayDevicesW) User32.GetAddress("EnumDisplayDevicesW");
	pEnumDisplaySettingsW EnumDisplaySettingsW = (pEnumDisplaySettingsW) User32.GetAddress("EnumDisplaySettingsW");

	for (int i=0;
		EnumDisplayDevicesW(0, i, &disp, 0);
		i++)
	{
		DEVMODEW mode;
		ZeroObj(mode);
		mode.dmSize = sizeof(mode);
		mode.dmDriverExtra = sizeof(mode);

		if (EnumDisplaySettingsW(disp.DeviceName, ENUM_CURRENT_SETTINGS, &mode))
		{
			GDisplayInfo *Dsp = new GDisplayInfo;
			if (Dsp)
			{
				Dsp->r.ZOff(mode.dmPelsWidth-1, mode.dmPelsHeight-1);

				if (mode.dmFields & DM_POSITION)
				{
					Dsp->r.Offset(mode.dmPosition.x, mode.dmPosition.y);
				}
				if (AllDisplays)
				{
					if (AllDisplays->Valid())
						AllDisplays->Union(&Dsp->r);
					else
						*AllDisplays = Dsp->r;
				}

				disp.cb = sizeof(disp);

				Dsp->BitDepth = mode.dmBitsPerPel;
				Dsp->Refresh = mode.dmDisplayFrequency;
				Dsp->Name = LgiNewUtf16To8(disp.DeviceString);
				Dsp->Device = LgiNewUtf16To8(disp.DeviceName);

				DISPLAY_DEVICEW temp = disp;
				if (EnumDisplayDevicesW(temp.DeviceName, 0, &disp, 0))
				{
					Dsp->Monitor = LgiNewUtf16To8(disp.DeviceString);
				}

				Displays.Add(Dsp);
			}
		}

		disp.cb = sizeof(disp);
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
				#if WINNATIVE
				int Style = GetWindowLong(v->Handle(), GWL_STYLE);
				if (TestFlag(Style, WS_VISIBLE) &&
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



