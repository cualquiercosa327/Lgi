#include "Lgi.h"

void LgiShowFileProperties(OsView Parent, const char *Filename)
{
	SHELLEXECUTEINFO info;
	ZeroObj(info);
	info.cbSize = sizeof(info);
	info.hwnd = Parent;
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