#if 0

#include "Lgi.h"
#include "qpopupmenu.h"
#include "qmenuprivate.h"

class QPopupPrivate
{
public:
	bool Cancelled;

	QPopupPrivate()
	{
		Cancelled = false;
	}
};

///////////////////////////////////////////////////////////////////////////
QPopup::QPopup(int i)
{
	Popup = new QPopupPrivate;

	setText("QPopup");

	XSetWindowAttributes a;
	a.override_redirect = true;
	a.save_under = false; // true;
	XChangeWindowAttributes(QWidget::XDisplay(), handle(), CWOverrideRedirect, &a); // CWSaveUnder | 
}

QPopup::~QPopup()
{
	QWidget::QApp()->Popups.Delete(this);
	DeleteObj(Popup);
}

bool QPopup::GetCancelled()
{
	return Popup->Cancelled;
}

int QPopup::popup(QWidget *parent, int Sx, int Sy)
{
	if (isVisible())
	{
		hide();
	}
	else
	{
		setParentWidget(parent);
		show(true);

		XRaiseWindow(XDisplay(), handle());
	}

	return -1;
}

void QPopup::show(bool Raise)
{
	Popup->Cancelled = false;
	
	/*
	if (NOT QWidget::QApp()->Popups.HasItem(this))
	{
		QWidget::QApp()->Popups.Insert(this);
	}
	*/
	
	QWidget::show(Raise);
	
	/*
	if (QWidget::MouseGrabber)
	{
		MouseGrabber->ungrabMouse();
	}
	
	QWidget::MouseGrabber = this;
	
	XGrabPointer(	QWidget::XDisplay(),
					handle(),
					true,				// owner events
					ButtonPressMask,	// event mask
					GrabModeAsync,		// pointer mode
					GrabModeAsync,		// keyboard mode
					XNone,				// confine to
					XNone,				// cursor
					CurrentTime);
	*/
}

void QPopup::hide()
{
	// QWidget::QApp()->Popups.Delete(this);
	// ungrabMouse();

	QWidget::hide();
	
	if (hasFocus() AND parentWidget())
	{
		// We can't keep the focus when we hide ourselves.
		QMainWindow *Top = GetWindow();
		if (NOT Top OR parentWidget()->IsPopup())
		{
			parentWidget()->setFocus();
		}
		else
		{
			Top->DefaultFocus();
		}
	}
}

void QPopup::mousePressEvent(XlibEvent *e)
{
	QWidget *w = QWidget::Find(e->GetEvent()->xbutton.window);
	if (NOT w OR
		NOT w->IsPopup())
	{
		printf("Popup cancelled.\n");
		Popup->Cancelled = true;
		hide();
	}
	
	/*
	bool IsChild = false;

	if (w != this)
	{
		while (w AND w != this)
		{
			if (w == this)
			{
				IsChild = true;
				break;
			}

			if (w->parentWidget())
			{
				w = w->parentWidget();
			}
			else
			{
				QPopupMenu *p = dynamic_cast<QPopupMenu*>(w);
				if (p)
				{
					IsChild = true;
				}
				w = 0;
			}
		}
	}

	if (IsChild OR
		r.Overlap(e->x(), e->y()))
	{

	}
	else
	{
		// close popup - user clicked outside the window
		Popup->Cancelled = true;
		hide();
	}
	*/
}

#endif