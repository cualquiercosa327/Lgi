
#ifndef __XApplication_h
#define __XApplication_h

#include "xwidget.h"
#include "LgiOsDefs.h"
#include "GContainers.h"
#include "GSemaphore.h"

#define XA_WmDeleteWindow	XAtom[0]
#define XA_WmProtocols		XAtom[1]
#define XA_Clipboard		XAtom[2]
#define XA_XdndEnter		XAtom[3]
#define XA_XdndPosition		XAtom[4]
#define XA_XdndStatus		XAtom[5]
#define XA_XdndLeave		XAtom[6]
#define XA_XdndDrop			XAtom[7]
#define XA_XdndFinished		XAtom[8]
#define XA_XdndSelection	XAtom[9]
#define XA_NetWmIcon		XAtom[10]
#define XA_Cardinal			XAtom[11]
#define XA_Max				20		// leave room for growth

class XApplication : public XEventSink, public GSemaphore
{
	friend class XObject;
	friend class XPopup;
	friend class XWidgetPrivate;
	friend class XlibEvent;
	friend class XWidget;
	friend class XWindow;

	static XApplication *_App;
	static XWidget *WantsFocus;
	class XApplicationPrivate *d;

	Display *Dsp;
	XList<XWidget> Delete;

	// Atoms
	Atom XAtom[XA_Max];
	
	// Clipboard data..
	char *ClipText;
	Pixmap ClipImage;
	XEventSink *SelectionSink;
	
	// Dnd
	List<char> DndTypes, DndAccepted;
	class GDragDropTarget *DndTarget;
	
	// Methods	
	void OnDeleteWidget(XWidget *w);
	void OnEvent(XEvent *Event);
	XWidget *GetKeyTarget();
	XIM GetInputMethod();

public:
	XApplication(int args, char **arg);
	virtual ~XApplication();

	// Api
	bool IsOk();
	XWidget *desktop();
	int exec();
	void onEvent(XEvent &Event);
	void processEvents();
	void exit(int code);
	void enter_loop();
	void exit_loop();
	void OnTabKey(XWidget *w, int Dir);
	uint64 GetLastButtonPressTime(int Button, int x, int y);
	OsThreadId GetGuiThread();

	// Focus
	XWidget *GetFocus();
	XWidget *GetFocusWindow();
	void SetFocus(XWidget *X);

	// Clipboard
	Atom GetClipboard() { return XA_Clipboard; }
	void EmptyClip();
	void SetClipText(XWidget *w, char *s);
	void SetClipImage(XWidget *w, class GSurface *pDC);
	void SetSelectionSink(XEventSink *s);
	bool GetSelection(Window w, Atom Selection, Atom Format, uchar *&Data, ulong &Len);
	
	// Static members
	static void postEvent(XWidget *o, XlibEvent *e);
	static int wheelScrollLines();
	static XApplication *GetApp() { return _App; }
};

#endif
