#include "LgiOsDefs.h"
#include "xapplication.h"

Display *XObject::XDisplay()
{
	return XApplication::_App->Dsp;
}

XApplication *XObject::XApp()
{
	return XApplication::_App;
}

OsPoint &XCursor::pos()
{
	OsPoint p;

	Window Root, Child;
	int Rx, Ry, Cx, Cy;
	uint Mask;
	XQueryPointer(	XDisplay(),
					RootWindow(XDisplay(), 0),
					&Root,
					&Child,
					&Rx, &Ry,
					&Cx, &Cy,
					&Mask);

	p.set(Rx, Ry);

	return p;
}

char *XMessage(int i)
{
	switch (i)
	{
		case Expose: return "Expose";
		case ConfigureNotify: return "ConfigureNotify";
		case MapNotify: return "MapNotify";
		case UnmapNotify: return "UnmapNotify";
		case ButtonPress: return "ButtonPress";
		case ButtonRelease: return "ButtonRelease";
		case MotionNotify: return "MotionNotify";
		case EnterNotify: return "EnterNotify";
		case LeaveNotify: return "LeaveNotify";
		case FocusIn: return "FocusIn";
		case FocusOut: return "FocusOut";
		case ClientMessage: return "ClientMessage";
		case KeyPress: return "KeyPress";
		case KeyRelease: return "KeyRelease";
		case SelectionRequest: return "SelectionRequest";
		case SelectionNotify: return "SelectionNotify";
		case SelectionClear: return "SelectionClear";
	}

	static char n[16];
	sprintf(n, "Unknown(%i)", i);
	return n;
}

char *XErr(int i)
{
	switch (i)
	{
		case  BadRequest:
			return "BadRequest";
		case  BadValue:
			return "BadValue";
		case  BadWindow:
			return "BadWindow";
		case  BadPixmap:
			return "BadPixmap";
		case  BadAtom:
			return "BadAtom";
		case  BadCursor:
			return "BadCursor";
		case  BadFont:
			return "BadFont";
		case  BadMatch:
			return "BadMatch";
		case  BadDrawable:
			return "BadDrawable";
		case  BadAccess:
			return "BadAccess";
		case  BadAlloc:
			return "BadAlloc";
		case  BadColor:
			return "BadColor";
		case  BadGC:
			return "BadGC";
		case  BadIDChoice:
			return "BadIDChoice";
		case  BadName:
			return "BadName";
		case  BadLength:
			return "BadLength";
		case  BadImplementation:
			return "BadImplementation";
	}
	
	static char n[16];
	sprintf(n, "Unknown(%i)", i);
	return n;
}
