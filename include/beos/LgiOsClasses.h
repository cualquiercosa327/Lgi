// BEOS

#ifndef __OS_CLASS_H
#define __OS_CLASS_H

typedef BApplication		OsApplication;
typedef BView				*OsPainter;
typedef BMenuItem			*OsMenuItem;

class GView;

class LgiClass BViewRedir : public BView
{
	void AttachedToWindow();
	void DetachedFromWindow();
	void Draw(BRect UpdateRect);
	void FrameResized(float width, float height);
	void Pulse();
	void MessageReceived(BMessage *message);
	void KeyDown(const char *bytes, int32 numBytes);
	void KeyUp(const char *bytes, int32 numBytes);
	void MouseDown(BPoint point);
	void MouseUp(BPoint point);
	void MouseMoved(BPoint point, uint32 transit, const BMessage *message);
	bool QuitRequested();

	GView *Wnd;
	uint32 WndBtn;
	
public:
	BViewRedir(GView *Wnd, uint32 Resize = B_FOLLOW_LEFT | B_FOLLOW_TOP);
	~BViewRedir();
	
	GView *WindowHandle() { return Wnd; }
};

typedef BViewRedir DefaultOsView;

class LgiClass GWnd : public BWindow
{
	friend class GWindow;
	
	GView *Notify;

public:
	GWnd(GView *notify);
	GWnd(GView *notify, BRect frame, char *title, window_type type, uint32 flags, uint32 workspaces = B_CURRENT_WORKSPACE);
	~GWnd();
	
	bool QuitRequested();
	void MessageReceived(BMessage *Msg);
	void FrameMoved(BPoint origin);
	void FrameResized(float width, float height);
};

class LgiClass GLocker
{
	BHandler *hnd;
	bool Locked;
	const char *File;
	int Line;

public:
	GLocker(BHandler *h, const char *file, int line);
	~GLocker();
	
	bool IsLocked() { return Locked; }
	bool Lock();
	status_t LockWithTimeout(int64 time);
	void Unlock();
	static const char *GetLocker(int Thread = 0);
};

#endif
