#include <tchar.h>
#include "Lgi.h"

void LgiShowFileProperties(OsView Parent, const char *Filename)
{
	SHELLEXECUTEINFO info;
	ZeroObj(info);
	info.cbSize = sizeof(info);
	#ifdef __GTK_H__
	Gtk::GdkWindow *wnd = Parent ? Gtk::gtk_widget_get_window(Parent) : NULL;
	info.hwnd = wnd ? gdk_win32_window_get_impl_hwnd(wnd) : NULL;
	#else
	info.hwnd = Parent;
	#endif
	info.lpVerb = _T("properties");
	#ifdef _UNICODE
		GVariant v(Filename);
		info.lpFile = v.WStr();
	#else
		info.lpFile = Filename;
	#endif
	info.nShow = SW_SHOW;
	info.fMask = SEE_MASK_INVOKEIDLIST;
	ShellExecuteEx(&info);
}

bool LgiBrowseToFile(const char *Filename)
{
	char Args[MAX_PATH];
	sprintf_s(Args, sizeof(Args), "/e,/select,\"%s\"", Filename);
	return LgiExecute("explorer", Args);
}