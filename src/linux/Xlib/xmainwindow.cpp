
#include <stdio.h>
#include "LgiOsDefs.h"
#include "xmainwindow.h"

///////////////////////////////////////////////////////
// Motif delc
#define MWM_DECOR_ALL						(1L << 0)
#define MWM_HINTS_INPUT_MODE				(1L << 2)
#define MWM_INPUT_FULL_APPLICATION_MODAL	3L

class MotifWmHints
{
public:
	ulong Flags, Functions, Decorations;
	long InputMode;
	ulong Status;
	
	MotifWmHints()
	{
		Flags = Functions = Status = 0;
		Decorations = MWM_DECOR_ALL;
		InputMode = 0;
	}
};

///////////////////////////////////////////////////////
class XMainWindowPrivate
{
public:
	// XMenuBar *Menu;
	bool IgnoreInput;
	bool Modal;
	Window LastFocus;

	XMainWindowPrivate()
	{
		LastFocus = 0;
		// Menu = 0;
		IgnoreInput = false;
		Modal = false;
	}
};

XMainWindow::XMainWindow() : XWidget((char*)0, true)
{
	d = NEW(XMainWindowPrivate);
}

XMainWindow::~XMainWindow()
{
	DeleteObj(d);
}

XWidget *XMainWindow::GetLastFocus()
{
	return XWidget::Find(d->LastFocus);
}

void XMainWindow::SetLastFocus(XWidget *w)
{
	if (w AND w->IsPopup())
	{
		return;
	}
	
	d->LastFocus = w ? w->handle() : 0;
}

bool XMainWindow::GetIgnoreInput()
{
	return d->IgnoreInput;
}

void XMainWindow::SetIgnoreInput(bool i)
{
	d->IgnoreInput = i;
}

bool XMainWindow::Modal()
{
	return d->Modal;
}

void XMainWindow::Modal(XMainWindow *Owner)
{
	d->Modal = Owner != 0;
	
	Atom XaMotifWmHints = XInternAtom(XDisplay(), "_MOTIF_WM_HINTS", false);
	Atom XaWmStateModal = XInternAtom(XDisplay(), "_NET_WM_STATE_MODAL", false);
	Atom XaWmState = XInternAtom(XDisplay(), "_NET_WM_STATE", false);

	MotifWmHints Hints;
	if (Owner)
	{
		Hints.InputMode = MWM_INPUT_FULL_APPLICATION_MODAL;
		Hints.Flags |= MWM_HINTS_INPUT_MODE;
	}

	XChangeProperty(XDisplay(),
					handle(),
					XaMotifWmHints,
					XaMotifWmHints,
					32,
					PropModeReplace,
					(unsigned char *) &Hints,
					5);

	if (Owner)
	{
		int e = 0;
		
		if (Owner->isVisible())
		{
			e = XSetTransientForHint(XDisplay(), handle(), Owner->handle());
		}
		
		XChangeProperty(XDisplay(),
						handle(),
						XaWmState,
						XA_ATOM,
						32,
						PropModeReplace,
						(unsigned char *) &XaWmStateModal,
						1);
		
		if (e == 1)
		{
			Owner->SetIgnoreInput(true);
		}
	}
}

/*
XMenuBar *XMainWindow::menuBar()
{
	if (NOT d->Menu)
	{
		d->Menu = NEW(XMenuBar);
		if (d->Menu)
		{
			OsPoint p(0, 0);
			d->Menu->reparent(this, p, true);
		}
	}

	return d->Menu;
}
*/

extern void _AddStop(XWidget *p, XList<XWidget> &Stops);

void XMainWindow::DefaultFocus()
{
	XWidget *f = GetLastFocus();
	
	if (NOT f)
	{
		XList<XWidget> Stops;
		_AddStop(this, Stops);
		f = Stops.First();
	}
	
	if (f)
	{
		f->setFocus();
	}
}

