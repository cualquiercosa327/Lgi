
#ifndef __XWidget_h
#define __XWidget_h

#include "xevent.h"
#include "GArray.h"

class XWidget;

typedef void (*XWidgetDestroyNotify)(XWidget*);

class XWidget : public XObject
{
	friend class XApplication;
	friend class XPainter;
	friend class ImagePainter;
	friend class XWidgetPrivate;
	friend class XPopup;
	friend class GApp;
	friend class GMemDC;
	friend class XlibEvent;
	friend class XWidgetList;

	static XWidget *MouseGrabber;
	static XWidget *OwnsClipboard;

	void _paint(XExposeEvent *e, XlibEvent *q);
	void _resize(XConfigureEvent *e, XlibEvent *q);
	void _map(bool m);
	bool _wait_mapped(int timeout = -1);
	XlibEvent *_event;
	virtual void *_GetWndPtr() { return 0; }

protected:
	XIC GetInputContext();
	class XWidgetPrivate *d;
	void setParentWidget(XWidget *p);
	int DebugLog(char *file, int line, char *msg, ...);

public:
	enum MouseButton
	{
		LeftButton = 0x0001,
		RightButton = 0x0002,
		MidButton = 0x0004
	};

	enum KeyButton
	{
		AltButton = 0x0008,
		ShiftButton = 0x0010,
		ControlButton = 0x0020
	};
	
	static XWidgetDestroyNotify DestroyNotifyHandlers[4];
	
	XWidget(char *name = 0, bool top = false);
	XWidget(Window Existing, bool AddToList = false);
	~XWidget();

	Window handle();
    void detachHandle();
	class XMainWindow *GetWindow();
	
	// Internal
	void _dump(int i=0);
	bool _isdebug();
	void _SetClipRgn(GRegion *c); // int Rects, XRectangle *Rect);
	GRegion *_GetClipRgn(); // int &Rects, XRectangle *&Rect);
	virtual void _SetDeleteMe();
	virtual void _SetWndPtr(void *p) {}
	static XWidget *GetMouseGrabber() { return MouseGrabber; }

	// window Api
	void setIcon(char *Path);
	bool IsMouseOver(XlibEvent *e);
	bool IsPopup();
	void GetDecorationSize(int &x, int &y);

	// Widget Api
	virtual int height();
	virtual int width();
	virtual void setGeometry(int x, int y, int w, int h);
	virtual GRect &geometry();
	virtual void setBackgroundMode(ViewBackground b);
	virtual void repaint();
	virtual void repaint(int x, int y, int w, int h);
	virtual void update();
	virtual void update(int x, int y, int w, int h);
	virtual bool isEnabled();
	virtual void setEnabled(bool i);
	virtual bool isVisible();
	virtual void show(bool Raise = false);
	virtual void hide();
	virtual bool hasFocus();
	virtual void setFocus();
	virtual void clearFocus();
	virtual OsPoint &mapFromGlobal(OsPoint &p);
	virtual OsPoint &mapToGlobal(OsPoint &p);
	virtual XWidget *parentWidget();
	virtual XList<XWidget> *childrenWidgets();
	virtual bool reparent(XWidget *parent, OsPoint &p, bool show);
	virtual void grabMouse();
	virtual void ungrabMouse();
	bool grabbedMouse() { return MouseGrabber == this; }

	virtual char *getText();
	virtual void setText(char *text);
	virtual int value();
	virtual void setValue(int i);
	virtual int wantKeys();
	virtual void wantKeys(int keys);
	virtual bool isTabStop();
	virtual void isTabStop(bool i);

	// Static Api
	static XWidget *Find(Window w);
	static XWidget *FirstWidget();

	// Events
	virtual void resizeEvent(XlibEvent *e);
	virtual void paintEvent(XlibEvent *e) {}
	virtual void customEvent(XlibEvent *e) {}
	virtual void mousePressEvent(XlibEvent *e) {}
	virtual void mouseDoubleClickEvent(XlibEvent *e) {}
	virtual void mouseReleaseEvent(XlibEvent *e) {}
	virtual void mouseMoveEvent(XlibEvent *e) {}
	virtual void leaveEvent(XlibEvent *e) {}
	virtual void enterEvent(XlibEvent *e) {}
	virtual void wheelEvent(XlibEvent *e) {}
	virtual void focusInEvent(XlibEvent *e) {}
	virtual void focusOutEvent(XlibEvent *e) {}
	virtual bool keyPressEvent(XlibEvent *e) {}
	virtual bool keyReleaseEvent(XlibEvent *e) {}
	virtual void closeEvent(XlibEvent *e) {}
	virtual void notifyEvent(int i = 0) {}
	virtual void propertyEvent(XlibEvent *e) {}

	// popup
	bool GetPopup();
	void SetPopup(bool b);
};

// typedef XWidget						*OsView;

#define MapWinUsage(var) \
	if (Deleted.HasItem(var)) \
	{ \
		printf("%s:%i - using win = %p\n", __FILE__, __LINE__, var); \
	}

extern GArray<Window> Deleted;

#endif
