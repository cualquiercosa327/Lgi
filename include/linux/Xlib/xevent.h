
#ifndef __XEvent_h
#define __XEvent_h

#include "LgiLinux.h"

#define DOUBLE_CLICK_TIME	400

class XlibEvent : public XObject
{
protected:
	XEvent *Event;

	// Keyboard state
	uint8 Scancode;
	LgiKey Sym;
	int Mod;
	char16 Unicode;

public:
	XlibEvent(XEvent *e);

	XEvent *GetEvent() { return Event; }

	// General
	int type();

	// Mouse
	int x();
	int y();
	int button();
	bool down();
	bool doubleclick();

	int ScreenX();
	int ScreenY();
	
	// Keyboard
	char16 unicode(XIC Ic);
	LgiKey getsym() { return Sym; }
	bool ischar();
	int state();

	// Wheel
	int delta();

	// Focus
	bool focus();

	// Expose
	GRect &exposed();
};

#endif
