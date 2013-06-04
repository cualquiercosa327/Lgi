/*hdr
**      FILE:           Gui.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           23/4/98
**      DESCRIPTION:    Memory subsystem
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>

#include "SimpleGameSound.h"
#include "Roster.h"
#include "Cursor.h"

#include "Lgi.h"
#include "GDragAndDrop.h"
#include "GViewPriv.h"

///////////////////////////////////////////////////////////////////////////////////////////////
// CursorData is a bitmap in an array of uint32's. This is generated from a graphics file:
// ./Code/cursors.png
//
// The pixel values are turned into C code by a program called i.Mage:
//	http://www.memecode.com/image.php
//
// Load the graphic into i.Mage and then go Edit->CopyAsCode
// Then paste the text into the CursorData variable at the bottom of this file.
//
// This saves a lot of time finding and loading an external resouce, and even having to
// bundle extra files with your application. Which have a tendancy to get lost along the
// way etc.
uint32 CursorData[] = {
0x02020202, 0x02020200, 0x02020202, 0x02020202, 0x02020202, 0x00020202, 0x02020202, 0x02020202, 0x00000000, 0x00000000, 0x00000000, 0x02020202, 0x02000000, 0x02000000, 0x02020202, 0x02020202, 0x02020202, 0x02000002, 0x02020202, 0x02020202, 0x02020202, 0x02020002, 0x02000202, 0x02020202, 0x02020202, 0x00000000, 0x00000000, 0x02020200, 0x00000000, 0x00000000, 0x02020200, 0x02020202, 0x02020202, 0x00020202, 0x02020202, 0x02020202, 0x02020202, 0x00000202, 0x02020000, 0x02020202, 0x02020202, 0x00020202, 0x02020202, 0x02020202, 0x02020202, 0x02000002, 0x02020202, 0x02020202, 0x00020202, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x01010100, 0x01010101, 0x00010101, 0x02020202, 0x00010100, 0x02000101, 0x02020202, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 
0x02020202, 0x02020202, 0x02020000, 0x00000202, 0x02020202, 0x02020202, 0x01010002, 0x01010101, 0x02020200, 0x01010100, 0x00010101, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x01000202, 0x02020001, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x00020202, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x01000100, 0x01000100, 0x00010100, 0x02020202, 0x01000002, 0x02020000, 0x02020202, 0x02020202, 0x00020202, 0x01010101, 0x02020200, 0x02020202, 0x00020202, 0x02020001, 0x01000202, 0x02020200, 0x02020202, 0x01000202, 0x01010101, 0x02020200, 0x01010100, 0x02000101, 0x02020202, 0x02020202, 0x02020202, 0x01010002, 0x02020001, 0x02020202, 0x02020202, 0x01000202, 
0x02020001, 0x02020202, 0x02020202, 0x01010002, 0x02020001, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x01000202, 0x02000101, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x00010002, 0x01010001, 0x02000101, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x01000202, 0x01010101, 0x02020001, 0x02020202, 0x01000202, 0x02020001, 0x01000202, 0x02020001, 0x02020202, 0x00020202, 0x01010101, 0x02020200, 0x01010100, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x01000000, 0x02000000, 0x02020202, 0x02020202, 0x01000200, 0x00020001, 0x02020202, 0x02020202, 0x01010100, 0x02000101, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x01000202, 0x02000101, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x01000202, 
0x01010100, 0x02020001, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x01010002, 0x01010101, 0x02000101, 0x02020202, 0x01010002, 0x00000001, 0x01000000, 0x02000101, 0x02020200, 0x01000202, 0x01010101, 0x02020200, 0x01010100, 0x02000101, 0x02020202, 0x02020200, 0x00020202, 0x01000202, 0x00020200, 0x02020202, 0x00020202, 0x01000200, 0x00020001, 0x02020200, 0x00020202, 0x01000000, 0x00000000, 0x02020202, 0x02020202, 0x00010100, 0x02020000, 0x02020202, 0x01010002, 0x00010101, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x00020202, 0x01010101, 0x02020200, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x00000000, 0x00010100, 0x00000000, 0x02020202, 0x01010100, 0x01010101, 0x01010101, 0x00010101, 0x02020000, 0x01010002, 0x01010001, 0x02020200, 
0x00010100, 0x00010101, 0x00020202, 0x02020200, 0x00000202, 0x01000202, 0x00020200, 0x02020200, 0x01000202, 0x01000200, 0x00020001, 0x02020001, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x01010100, 0x00000101, 0x02020200, 0x01010002, 0x00010101, 0x02020202, 0x02020202, 0x00000002, 0x01000000, 0x00000000, 0x02020000, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x01010100, 0x01010101, 0x01010101, 0x00010101, 0x02000100, 0x01010100, 0x01000200, 0x02020200, 0x02000100, 0x01010100, 0x01000200, 0x02020200, 0x00010002, 0x01000000, 0x00000000, 0x02020001, 0x01010002, 0x01000000, 0x00000001, 0x02000101, 0x00000000, 0x01000000, 0x00000000, 0x02000000, 0x02000002, 0x01010100, 0x01010101, 
0x02020001, 0x01010100, 0x01010101, 0x02020200, 0x02020202, 0x01010100, 0x01010101, 0x01010101, 0x02000101, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x01010002, 0x00000001, 0x01000000, 0x02000101, 0x00010100, 0x00010101, 0x00020202, 0x02020200, 0x02020000, 0x01010002, 0x01010001, 0x02020200, 0x01010100, 0x01010101, 0x01010101, 0x02000101, 0x01010100, 0x01010101, 0x01010101, 0x00010101, 0x01010100, 0x01010101, 0x01010101, 0x02000101, 0x00010100, 0x01010100, 0x01010101, 0x02020001, 0x01010100, 0x01010101, 0x02020200, 0x02020202, 0x00000002, 0x01000000, 0x00000000, 0x02020000, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x02020202, 0x00010100, 
0x02020202, 0x02020202, 0x01000202, 0x02020001, 0x01000202, 0x02020001, 0x01010100, 0x02000101, 0x02020202, 0x02020200, 0x02020200, 0x01000202, 0x01010101, 0x02020200, 0x00010002, 0x01000000, 0x00000000, 0x02020001, 0x01010002, 0x01000000, 0x00000001, 0x02000101, 0x01010100, 0x01010101, 0x01010101, 0x02000101, 0x01010100, 0x01010101, 0x01010101, 0x02020001, 0x00000000, 0x00000001, 0x02020200, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x00020202, 0x02020001, 0x01000202, 0x02020200, 0x01010100, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x00020202, 0x01010101, 0x02020200, 0x00000202, 0x01000202, 0x00020200, 0x02020200, 0x01000202, 
0x01000200, 0x00020001, 0x02020001, 0x00000000, 0x01000000, 0x00000000, 0x02000000, 0x01010002, 0x01010101, 0x01010101, 0x02020001, 0x00020202, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x00020202, 0x01010101, 0x02020200, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x00000000, 0x00010100, 0x00000000, 0x02020202, 0x02020202, 0x02020000, 0x00000202, 0x02020202, 0x01010100, 0x02000101, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x01010101, 0x02020200, 0x00020202, 0x01000202, 0x00020200, 0x02020202, 0x00020202, 0x01000200, 0x00020001, 0x02020200, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x01000202, 0x01010101, 0x01010101, 0x02020001, 0x00020202, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 
0x01000202, 0x00010100, 0x02020001, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x01010002, 0x01010101, 0x02000101, 0x02020202, 0x02020202, 0x02020002, 0x02000202, 0x02020202, 0x01010100, 0x00010101, 0x02020202, 0x02020202, 0x02020202, 0x01010002, 0x01010101, 0x02020200, 0x02020202, 0x01000000, 0x02000000, 0x02020202, 0x02020202, 0x01000200, 0x00020001, 0x02020202, 0x00020202, 0x01000000, 0x00000000, 0x02020202, 0x00020202, 0x01010101, 0x01010101, 0x02020001, 0x00020202, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x00010002, 0x00010100, 0x02000100, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x01000202, 0x01010101, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000000, 0x00000000, 0x02020200, 
0x02020202, 0x02020202, 0x00000000, 0x00000000, 0x02020200, 0x02020202, 0x01010002, 0x02020001, 0x02020202, 0x02020202, 0x01000202, 0x02020001, 0x02020202, 0x02020202, 0x01010100, 0x02000101, 0x02020202, 0x02020202, 0x01010100, 0x01010101, 0x02020200, 0x00020202, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x01000100, 0x01010101, 0x00010001, 0x02020202, 0x01000002, 0x02020000, 0x02020202, 0x02020202, 0x00020202, 0x01010101, 0x02020200, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x01000202, 0x02020001, 0x02020202, 0x02020202, 0x01010002, 0x02020001, 0x02020202, 0x02020202, 0x00000002, 
0x00000000, 0x02020202, 0x00020202, 0x02020001, 0x02020202, 0x02020202, 0x02020202, 0x00020202, 0x02020202, 0x02020202, 0x01010100, 0x01010101, 0x00010101, 0x02020202, 0x00010100, 0x02000101, 0x02020202, 0x02020202, 0x02020202, 0x00010100, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00020202, 0x02020202, 0x02020202, 0x02020202, 0x00000202, 0x02020000, 0x02020202, 0x02020202, 0x01000202, 0x02020200, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00020202, 0x02020000, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00000000, 0x00000000, 0x00000000, 0x02020202, 0x02000000, 0x02000000, 0x02020202, 0x02020202, 0x02020202, 
0x02000002, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x00020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 0x02020202, 
};

GInlineBmp Cursors =
{
	192, 16, 8, CursorData
};

struct CursorInfo
{
public:
	GRect Pos;
	GdcPt2 HotSpot;
}
CursorMetrics[] =
{
	// up arrow
	{ GRect(0, 0, 8, 15),			GdcPt2(4, 0) },
	// cross hair
	{ GRect(16, 0, 30, 13),		GdcPt2(23, 7) },
	// hourglass
	{ GRect(32, 0, 43, 15),		GdcPt2(37, 7) },
	// I beam
	{ GRect(48, 0, 54, 15),		GdcPt2(51, 7) },
	// N-S arrow
	{ GRect(64, 0, 75, 15),		GdcPt2(69, 7) },
	// E-W arrow
	{ GRect(80, 0, 95, 11),		GdcPt2(87, 5) },
	// NW-SE arrow
	{ GRect(96, 0, 108, 12),		GdcPt2(102, 6) },
	// NE-SW arrow
	{ GRect(112, 0, 124, 12),		GdcPt2(118, 6) },
	// 4 way arrow
	{ GRect(128, 0, 142, 14),		GdcPt2(135, 7) },
	// Blank
	{ GRect(0, 0, 0, 0),			GdcPt2(0, 0) },
	// Vertical split
	{ GRect(144, 0, 158, 15),		GdcPt2(151, 7) },
	// Horizontal split
	{ GRect(160, 0, 174, 15),		GdcPt2(167, 7) },
	// Hand
	{ GRect(176, 0, 189, 13),		GdcPt2(180, 0) },
};


/*
class GViewPrivate
{
public:
	GViewI			*Notify;
	GViewI			*ParentI;
	GView			*ParentV;
	GView			*Popup;
	GViewFill		*Background;
	int				CtrlId;
	GDragDropTarget	*DropTarget;
	GFont			*Fnt;
	bool			FntOwn;
*/
	
GViewPrivate::GViewPrivate()
{
	CtrlId = -1;
	DropTarget = NULL;
	IsThemed = false;
	ParentI = NULL;
	Parent = NULL;
	Notify = NULL;
	MinimumSize.x = MinimumSize.y = 0;
	Font = NULL;
	FontOwn = false;
}

GViewPrivate::~GViewPrivate()
{
	if (FontOwn)
		DeleteObj(Font);
}

GMessage CreateMsg(int m, int a, int b)
{
	GMessage Msg(m, a, b);
	return Msg;
}

int MsgA(GMessage *m)
{
	int32 i=0;
	if (m) m->FindInt32("a", &i);
	return i;
}

int MsgB(GMessage *m)
{
	int32 i=0;
	if (m) m->FindInt32("b", &i);
	return i;
}


#if 0
////////////////////////////////////////////////////////////////////////////////////////////////////
int LastX = -DOUBLE_CLICK_THRESHOLD, LastY = -DOUBLE_CLICK_THRESHOLD;

////////////////////////////////////////////////////////////////////////////////////////////////////
struct GView::OsMouseInfo
{
	// View needing mouse info
	GView *Wnd;

	// Data
	int32 Clicks;
	int Sx, Sy;
	uint32 Buttons;
	BPoint Point;

	// Reason for thread
	bool Capture;
	bool MouseClick;
};

class GuiLocker
{
	BHandler *Handler;
	bool Lock;

public:
	GuiLocker(BHandler *handler)
	{
		Lock = 0;
		Handler = handler;
		if (Handler)
		{
			Lock = Handler->LockLooperWithTimeout(20000) == B_OK;
		}
	}
	
	~GuiLocker()
	{
		if (Handler && Lock)
		{
			Handler->UnlockLooper();
			Lock = 0;
		}
	}
	
	bool Locked()
	{
		return Lock;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
extern uint32 _lgi_mouse_keys();

class GMouseDoEnterExit : public GThread, public GMutex
{
	GView *Over;

public:
	GMouseDoEnterExit() : GThread("GMouseDoEnterExit")
	{
		Over = 0;
		
		#if 0 // FIXME
		Run();
		#endif
	}
	
	int Main()
	{
		// Run forever
		while (true)
		{
			if (Lock(_FL))
			{
				uint32 But;
				BPoint Pos;
				
				if (Over)
				{
					if (Over->Handle()->LockLooperWithTimeout(50 * 1000) == B_OK)
					{					
						Over->Handle()->GetMouse(&Pos, &But, false);
						Over->Handle()->UnlockLooper();
					}
					
					if (Pos.x < 0 ||
						Pos.y < 0 ||
						Pos.x >= Over->X() ||
						Pos.y >= Over->Y())
					{
						SendEvent(LGI_MOUSE_EXIT);
						Over = 0;
					}
				}
				
				Unlock();
			}
			
			// Go easy on the CPU
			LgiSleep(100);
		}
	}
	
	void SendEvent(uint32 Event)
	{
		if (Over)
		{
			uint32 But;
			BPoint Pos;
				
			if (Over->Handle()->LockLooperWithTimeout(300 * 1000) == B_OK)
			{					
				Over->Handle()->GetMouse(&Pos, &But, false);

				BMessage *Msg = new BMessage(Event);
				if (Msg)
				{
					Msg->AddPoint("pos", Pos);
					Msg->AddInt32("flags", But | _lgi_mouse_keys());
					
					BMessenger m(Over->Handle());
					m.SendMessage(Msg);
				}

				Over->Handle()->UnlockLooper();
			}
		}
	}
	
	void SetMouseOver(GView *over)
	{
		GMutex::Auto Lck(this, _FL);
		if (Over != over)
		{
			SendEvent(LGI_MOUSE_EXIT);
			Over = over;
			SendEvent(LGI_MOUSE_ENTER);
		}
	}
	
	void OnViewDelete(GView *View)
	{
		if (View == Over)
		{
			if (Lock(_FL))
			{
				Over = 0;
				Unlock();
			}
		}
	}

} EnterExitThread;

////////////////////////////////////////////////////////////////////////////////////////////////////
void _lgi_yield()
{
	/*
	BLooper *Looper = BLooper::LooperForThread(find_thread(0));
	if (Looper)
	{
		BWindow *Wnd = dynamic_cast<BWindow*>(Looper);
		if (Wnd)
		{
			if (Wnd->Lock())
			{
				Wnd->UpdateIfNeeded();
				Wnd->Unlock();
			}
		}
	}
	*/
}

uint32 _lgi_mouse_keys()
{
	int Keys = modifiers();
	
	return	(TestFlag(Keys, B_SHIFT_KEY) ? LGI_VMOUSE_SHIFT : 0) |
			(TestFlag(Keys, B_CONTROL_KEY) ? LGI_VMOUSE_ALT : 0) |
			(TestFlag(Keys, B_COMMAND_KEY) ? LGI_VMOUSE_SHIFT : 0);
}

void _lgi_assert(bool b, char *test, char *file, int line)
{
	if (!b)
	{
		/*
		LgiMsg(	0,
				"Assert failed, file: %s, line: %i\n"
				"%s\n",
				"Debug",
				MB_OK,
				file, line, test);
		*/
		exit(-1);					
	}
}

BMessage NewMsg(int m, int a, int b)
{
	BMessage Msg(m);
	Msg.AddInt32("a", a);
	Msg.AddInt32("b", b);
	return Msg;
}

////////////////////////////////////////////////////////////////////////////
bool LgiIsKeyDown(int Key)
{
	key_info KeyInfo;
	if (get_key_info(&KeyInfo) == B_OK)
	{
		switch (Key)
		{
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
class GViewIter : public GViewIterator
{
	List<GViewI>::I i;

public:
	GViewIter(GView *v) : i(v->Children.Start())
	{
	}

	GViewI *First()
	{
		return i.First();
	}

	GViewI *Last()
	{
		return i.Last();
	}

	GViewI *Next()
	{
		return i.Next();
	}

	GViewI *Prev()
	{
		return i.Prev();
	}

	int Length()
	{
		return i.Length();
	}

	int IndexOf(GViewI *view)
	{
		return i.IndexOf(view);
	}

	GViewI *operator [](int Idx)
	{
		return i[Idx];
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/*
GView::GView(BView *view)
{
	d = new GViewPrivate;
	_View = view ? view : new DefaultOsView(this);
	_MouseInfo = 0;
	_CaptureThread = 0;
	_PulseThread = 0;
	_PulseRate = -1;
	_Window = 0;
	_BorderSize = 0;
	_QuitMe = 0;
	Script = 0;
	WndFlags = GWF_VISIBLE | GWF_ENABLED;

	Pos.ZOff(0, 0);
}

GView::~GView()
{
	EnterExitThread.OnViewDelete(this);
	
	if (_CaptureThread)
	{
		printf("%s:%i - Killing capture thread\n", __FILE__, __LINE__);
		kill_thread(_CaptureThread);
	}
	if (_PulseThread)
	{
		printf("%s:%i - Killing pulse thread\n", __FILE__, __LINE__);
		kill_thread(_PulseThread);
	}
	if (_Over == this)
	{
		_Over = 0;
	}

	if (d->ParentI)
	{
		d->ParentI->DelView(this);
	}
	GViewI *c;
	while (c = Children.First())
	{
		LgiAssert(c->GetParent() != 0);
		DeleteObj(c);
	}
	d->ParentI = 0;
	d->ParentV = 0;
	_Window = 0;

	if (_View)
	{
		BView *ParentView = _View->Parent();
		if (ParentView)
		{
			bool Lock = ParentView->LockLooper();
			if (!ParentView->RemoveChild(_View))
			{
				printf("%s:%i - RemoveChild failed.\n", __FILE__, __LINE__);
			}
			if (Lock) ParentView->UnlockLooper();
		}
		else
		{
			printf("%s:%i - _View not attached?.\n", __FILE__, __LINE__);
		}

		DeleteObj(_View);
	}

	DeleteObj(_MouseInfo);
	DeleteObj(d);
	
	if (_QuitMe)
	{
		// Window thread ends here...
		printf("%s:%i - Window quiting here.\n", __FILE__, __LINE__);
		_QuitMe->Quit();
		// Code will never reach here.
	}
}

GViewIterator *GView::IterateViews()
{
	return new GViewIter(this);
}

bool GView::AddView(GViewI *v, int Where)
{
	LgiAssert(!Children.HasItem(v));
	return Children.Insert(v, Where);
}

bool GView::DelView(GViewI *v)
{
	return Children.Delete(v);
}

bool GView::HasView(GViewI *v)
{
	return Children.HasItem(v);
}

GWindow *GView::GetWindow()
{
	return dynamic_cast<GWindow*>(_Window);
}

OsWindow GView::WindowHandle()
{
	return (_View) ? _View->Window() : 0;
}
*/

#define ADJ_LEFT			1
#define ADJ_RIGHT			2
#define ADJ_UP				3
#define ADJ_DOWN			4

extern int IsAdjacent(GRect &a, GRect &b);
int IsAdjacent(GRect &a, GRect &b)
{
	if (	(a.x1 == b.x2 + 1) &&
		!(a.y1 > b.y2 || a.y2 < b.y1))
	{
		return ADJ_LEFT;
	}

	if (	(a.x2 == b.x1 - 1) &&
		!(a.y1 > b.y2 || a.y2 < b.y1))
	{
		return ADJ_RIGHT;
	}

	if (	(a.y1 == b.y2 + 1) &&
		!(a.x1 > b.x2 || a.x2 < b.x1))
	{
		return ADJ_UP;
	}

	if (	(a.y2 == b.y1 - 1) &&
		!(a.x1 > b.x2 || a.x2 < b.x1))
	{
		return ADJ_DOWN;
	}

	return 0;
}

extern GRect JoinAdjacent(GRect &a, GRect &b, int Adj);
GRect JoinAdjacent(GRect &a, GRect &b, int Adj)
{
	GRect t;

	switch (Adj)
	{
		case ADJ_LEFT:
		case ADJ_RIGHT:
		{
			t.y1 = max(a.y1, b.y1);
			t.y2 = min(a.y2, b.y2);
			t.x1 = min(a.x1, b.x1);
			t.x2 = max(a.x2, b.x2);
			break;
		}
		case ADJ_UP:
		case ADJ_DOWN:
		{
			t.y1 = min(a.y1, b.y1);
			t.y2 = max(a.y2, b.y2);
			t.x1 = max(a.x1, b.x1);
			t.x2 = min(a.x2, b.x2);
			break;
		}
	}

	return t;
}


GMouse &_lgi_adjust_click_for_window(GMouse &Info, GViewI *Wnd)
{
	static GMouse Temp;
	 
	Temp = Info;
	if (Wnd)
	{
		GdcPt2 Offset(0, 0);
		if (Wnd->WindowVirtualOffset(&Offset))
		{
			Temp.x -= Offset.x;
			Temp.y -= Offset.y;
		}
	}
	
	return Temp;
}

/*
bool GView::Lock(const char *file, int line, int Timeout)
{
	if (Handle()->Window())
	{
		if (Timeout >= 0)
		{
			return Handle()->LockLooperWithTimeout(Timeout);
		}
		else
		{
			return Handle()->LockLooper();
		}
	}
	
	return false;
}

void GView::Unlock()
{
	if (Handle()->Window())
	{
		Handle()->UnlockLooper();
	}
}
*/

GdcPt2 GView::GetMinimumSize()
{
	static GdcPt2 p;
	p.x = 0;
	p.y = 0;

	/*
	if (_View && Lock())
	{
		GWindow *w = GetWindow();
		if (w && w->Wnd)
		{
			float MinX, MaxX, MinY, MaxY;
			w->Wnd->GetSizeLimits(&MinX, &MaxX, &MinY, &MaxY);
			p.x = MinX;
			p.y = MinY;
		}
		Unlock();
	}
	*/	

	return p;
}

void GView::SetMinimumSize(GdcPt2 Size)
{
	/*
	if (_View && Lock())
	{
		GWindow *w = GetWindow();
		if (w && w->Wnd)
		{
			float MinX, MaxX, MinY, MaxY;
			w->Wnd->GetSizeLimits(&MinX, &MaxX, &MinY, &MaxY);
			MinX = Size.x;
			MinY = Size.y;
			w->Wnd->SetSizeLimits(MinX, MaxX, MinY, MaxY);
		}
		Unlock();
	}
	*/
}

GRect *GView::FindLargest(GRegion &r)
{
	int Pixels = 0;
	GRect *Best = 0;
	static GRect Final;

	// do initial run through the list to find largest single region
	for (GRect *i = r.First(); i; i = r.Next())
	{
		int Pix = i->X() * i->Y();
		if (Pix > Pixels)
		{
			Pixels = Pix;
			Best = i;
		}
	}

	if (Best)
	{
		Final = *Best;
		Pixels = Final.X() * Final.Y();

		int LastPixels = Pixels;
		GRect LastRgn = Final;
		int ThisPixels = Pixels;
		GRect ThisRgn = Final;

		GRegion TempList;
		for (GRect *i = r.First(); i; i = r.Next())
		{
			TempList.Union(i);
		}
		TempList.Subtract(Best);

		do
		{
			LastPixels = ThisPixels;
			LastRgn = ThisRgn;

			// search for adjoining rectangles that maybe we can add
			for (GRect *i = TempList.First(); i; i = TempList.Next())
			{
				int Adj = IsAdjacent(ThisRgn, *i);
				if (Adj)
				{
					GRect t = JoinAdjacent(ThisRgn, *i, Adj);
					int Pix = t.X() * t.Y();
					if (Pix > ThisPixels)
					{
						ThisPixels = Pix;
						ThisRgn = t;

						TempList.Subtract(i);
					}
				}
			}
		} while (LastPixels < ThisPixels);

		Final = ThisRgn;
	}

	return &Final;
}

GRect *GView::FindSmallestFit(GRegion &r, int Sx, int Sy)
{
	int X = 1000000;
	int Y = 1000000;
	GRect *Best = 0;

	for (GRect *i = r.First(); i; i = r.Next())
	{
		if ((i->X() >= Sx && i->Y() >= Sy) &&
			(i->X() < X || i->Y() < Y))
		{
			X = i->X();
			Y = i->Y();
			Best = i;
		}
	}

	return Best;
}

GRect *GView::FindLargestEdge(GRegion &r, int Edge)
{
	GRect *Best = 0;

	for (GRect *i = r.First(); i; i = r.Next())
	{
		if (!Best)
		{
			Best = i;
		}

		if ((Edge & GV_EDGE_TOP) && (i->y1 < Best->y1) ||
			(Edge & GV_EDGE_RIGHT) && (i->x2 > Best->x2) ||
			(Edge & GV_EDGE_BOTTOM) && (i->y2 > Best->y2) ||
			(Edge & GV_EDGE_LEFT) && (i->x1 < Best->x1))
		{
			Best = i;
		}

		if (((Edge & GV_EDGE_TOP) && (i->y1 == Best->y1) ||
			(Edge & GV_EDGE_BOTTOM) && (i->y2 == Best->y2)) &&
			(i->X() > Best->X()))
		{
			Best = i;
		}

		if (((Edge & GV_EDGE_RIGHT) && (i->x2 == Best->x2) ||
			(Edge & GV_EDGE_LEFT) && (i->x1 == Best->x1)) &&
			(i->Y() > Best->Y()))
		{
			Best = i;
		}
	}

	return Best;
}

int64 GView::GetCtrlValue(int Id)
{
	GViewI *w = FindControl(Id);
	return (w) ? w->Value() : -1;
}

void GView::SetCtrlValue(int Id, int64 i)
{
	GViewI *w = FindControl(Id);
	if (w)
	{
		w->Value(i);
	}
}

char *GView::GetCtrlName(int Id)
{
	GViewI *w = FindControl(Id);
	return (w) ? w->Name() : 0;
}

void GView::SetCtrlName(int Id, const char *s)
{
	GViewI *w = FindControl(Id);
	if (w)
	{
		w->Name(s);
	}
}

bool GView::GetCtrlEnabled(int Id)
{
	GViewI *w = FindControl(Id);
	return (w) ? w->Enabled() : 0;
}

void GView::SetCtrlEnabled(int Id, bool Enabled)
{
	GViewI *w = FindControl(Id);
	if (w)
	{
		w->Enabled(Enabled);
	}
}

void GView::SetId(int i)
{
	d->CtrlId = i;
}

bool GView::GetCtrlVisible(int Id)
{
	GViewI *w = FindControl(Id);
	return (w) ? w->Visible() : 0;
}

void GView::SetCtrlVisible(int Id, bool v)
{
	GViewI *w = FindControl(Id);
	if (w)
	{
		w->Visible(v);
	}
}

bool GView::Sunken()
{
	return TestFlag(WndFlags, GWF_SUNKEN);
}

void GView::Sunken(bool i)
{
	if (i)
	{
		SetFlag(WndFlags, GWF_SUNKEN);
		if (!_BorderSize)
		{
			_BorderSize = 2;
		}
	}
	else ClearFlag(WndFlags, GWF_SUNKEN);

	Invalidate();
}

bool GView::Flat()
{
	return TestFlag(WndFlags, GWF_FLAT);
}

void GView::Flat(bool i)
{
	if (i) SetFlag(WndFlags, GWF_FLAT);
	else ClearFlag(WndFlags, GWF_FLAT);

	Invalidate();
}

bool GView::Raised()
{
	return TestFlag(WndFlags, GWF_RAISED);
}

void GView::Raised(bool i)
{
	if (i)
	{
		SetFlag(WndFlags, GWF_RAISED);
		if (!_BorderSize)
		{
			_BorderSize = 2;
		}
	}
	else ClearFlag(WndFlags, GWF_RAISED);

	Invalidate();
}

bool GView::AttachChildren()
{
	bool Status = true;

	for (GViewI *c=Children.First(); c; c=Children.Next())
	{
		Status |= c->Attach(this);
	}

	return Status;
}

GFont *GView::GetFont()
{
	return d->Fnt ? d->Fnt : SysFont;
}

void GView::SetFont(GFont *Fnt, bool OwnIt)
{
	if (d->FntOwn)
	{
		DeleteObj(d->Fnt);
	}
	d->Fnt = Fnt;
	d->FntOwn = OwnIt;
}

bool GView::IsOver(GMouse &m)
{
	return	(m.x >= 0) &&
			(m.y >= 0) &&
			(m.x < Pos.X()) &&
			(m.y < Pos.Y());
}

GViewI *GView::WindowFromPoint(int x, int y, bool debug)
{
	List<GViewI>::I n = Children.Start();
	for (GViewI *c = n.First(); c; c = n.Next())
	{
		GRect r = c->GetPos();
		if (r.Overlap(x, y))
		{
			return c->WindowFromPoint(x - r.x1, y - r.y1);
		}
	}

	if (x >= 0 && y >= 0 && x < Pos.X() && y < Pos.Y())
	{
		return this;
	}

	return NULL;
}

void GView::OnPosChange()
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnPosChange();
	}
}

bool GView::Invalidate(GRegion *r, bool Repaint, bool NonClient)
{
	if (r)
	{
		for (int i=0; i<r->Length(); i++)
		{
			bool Last = i == r->Length()-1;
			Invalidate((*r)[i], Last ? Repaint : false, NonClient);
		}
		
		return true;
	}
	
	return false;
}

long _lgi_pulse_thread(void *ptr)
{
	GView *Wnd = (GView*) ptr;

	if (Wnd)
	{
		while (Wnd->_PulseRate > 0)
		{
			LgiSleep(Wnd->_PulseRate);
			
			if (Wnd->Handle()->LockLooperWithTimeout(1000) == B_OK)
			{
				BMessenger m(Wnd->Handle());
				m.SendMessage(M_PULSE);
				Wnd->Handle()->UnlockLooper();
			}
		}
		
		Wnd->_PulseRate = -1;
		Wnd->_PulseThread = 0;
	}
	
	return 0;
}

bool GView::InThread()
{
	return find_thread(0) == WindowHandle()->Thread();
}

void GView::SendNotify(int Data)
{
	GViewI *n = GetNotify() ? GetNotify() : GetParent();
	if (n)
	{
		n->OnNotify(this, Data);
	}
}

bool GView::PostEvent(int Cmd, GMessage::Param a, GMessage::Param b)
{
	bool Status = false;

	if (Lock(_FL))
	{
		BMessage *Msg = new BMessage(Cmd);
		if (Msg)
		{
			Msg->AddInt32("a", a);
			Msg->AddInt32("b", b);
			BMessenger m(Handle());
			Status = m.SendMessage(Msg) == B_OK;
			DeleteObj(Msg);
		}
		Unlock();
	}
	
	return Status;
}

int GView::OnNotify(GViewI *Ctrl, int Flags)
{
	if (Ctrl && d->GetParent())
	{
		// default behaviour is just to pass the 
		// notification up to the parent
		return d->GetParent()->OnNotify(Ctrl, Flags);
	}

	return 0;
}

bool GView::Name(const char *n)
{
	bool Status = GBase::Name(n);
	
	/*
	if (_View)
	{
		bool Lock = _View->LockLooper();
		_View->SetName(n);
		if (Lock) _View->UnlockLooper();
	}
	*/
	
	return true;
}

char *GView::Name()
{
	return GBase::Name();
}

bool GView::NameW(const char16 *n)
{
	bool Status = GBase::NameW(n);

	/*
	if (_View)
	{
		bool Lock = _View->LockLooper();
		_View->SetName(GBase::Name());
		if (Lock) _View->UnlockLooper();
	}
	*/
	
	return true;
}

char16 *GView::NameW()
{
	return GBase::NameW();
}

bool GView::Enabled()
{
	return TestFlag(WndFlags, GWF_ENABLED);
}

void GView::Enabled(bool e)
{
	if (e) SetFlag(WndFlags, GWF_ENABLED);
	else ClearFlag(WndFlags, GWF_ENABLED);

	/*	
	BControl *Ctrl = dynamic_cast<BControl*>(_View);
	if (Ctrl)
	{
		bool Lock = Ctrl->LockLooper();
		Ctrl->SetEnabled(e);
		if (Lock) Ctrl->UnlockLooper();
	}
	*/

	Invalidate();
}

bool GView::Visible()
{
	return TestFlag(WndFlags, GWF_VISIBLE);
}

void GView::Visible(bool v)
{
	bool Change = TestFlag(WndFlags, GWF_VISIBLE) ^ v;

	if (v) SetFlag(WndFlags, GWF_VISIBLE);
	else ClearFlag(WndFlags, GWF_VISIBLE);

	if (Change && _View)
	{
		bool Lock = _View->LockLooper();
		if (v)
			_View->Show();
		else
			_View->Hide();
		if (Lock) _View->UnlockLooper();
	}
}

bool GView::Focus()
{
	bool f = false;

	/*
	BWindow *Wnd = WindowHandle();
	if (Wnd && _View && _View->LockLooper())
	{
		BView *Cur = Wnd->CurrentFocus();
		f = (_View == Cur);
		if (f) SetFlag(WndFlags, GWF_FOCUS);
		else ClearFlag(WndFlags, GWF_FOCUS);

		_View->UnlockLooper();
	}
	*/

	return TestFlag(WndFlags, GWF_FOCUS);
}

void GView::Focus(bool f)
{
	if (f) SetFlag(WndFlags, GWF_FOCUS);
	else ClearFlag(WndFlags, GWF_FOCUS);

	/*
	if (_View && _View->LockLooper())
	{
		_View->MakeFocus(f);
		_View->UnlockLooper();
	}
	*/
}

extern bool _setup_mouse_thread(GView::OsMouseInfo *&i, OsThread &t);

void GView::Sys_MouseDown(BPoint Point)
{
/*
	if (!_CaptureThread)
	{
		if (_setup_mouse_thread(_MouseInfo, _CaptureThread))
		{
			_MouseInfo->Wnd = this;
			_MouseInfo->Clicks = 1;
			_MouseInfo->Sx = Pos.X();
			_MouseInfo->Sy = Pos.Y();
			_MouseInfo->MouseClick = true;
		
			BMessage *Msg = (WindowHandle())?WindowHandle()->CurrentMessage():0;
			if (Msg)
			{
				Msg->FindInt32("clicks", &_MouseInfo->Clicks);
			}
	
			// Do down click
			GMouse m;
			Handle()->GetMouse(&_MouseInfo->Point, &_MouseInfo->Buttons, FALSE);
	
			m.x = _MouseInfo->Point.x - _BorderSize;
			m.y = _MouseInfo->Point.y - _BorderSize;
			m.Flags = _MouseInfo->Buttons | _lgi_mouse_keys();
			m.Down(true);
			m.Double(	(_MouseInfo->Clicks > 1) &&
						(abs(m.x-LastX) < DOUBLE_CLICK_THRESHOLD) &&
						(abs(m.y-LastY) < DOUBLE_CLICK_THRESHOLD));
			LastX = _MouseInfo->Point.x;
			LastY = _MouseInfo->Point.y;

			OnMouseClick(m);
	
			// Spawn thread to watch the mouse until the user
			// releases the button
			resume_thread(_CaptureThread);
		}
	}
*/
}

void GView::Sys_MouseMoved(BPoint Point, uint32 transit, const BMessage *message)
{
/*
	if (_View)
	{
		GMouse m;
		ulong Buttons;
	
		_View->GetMouse(&Point, &Buttons, FALSE);
	
		m.x = Point.x - _BorderSize;
		m.y = Point.y - _BorderSize;
		m.Flags = Buttons | _lgi_mouse_keys();
		m.Down(Buttons != 0);
		
		OnMouseMove(m);
	}
*/
}

long _lgi_mouse_thread(GView::OsMouseInfo *Info)
{
/*
	if (Info && Info->Wnd)
	{
		bool Over = true;
		uint32 LastButton = Info->Buttons;

		// watch the mouse while the button is down
		while (Info->Buttons)
		{
			if (Info->Wnd->Handle()->LockLooper())
			{
				BPoint p;
				LastButton = Info->Buttons;
				Info->Wnd->Handle()->GetMouse(&p, &Info->Buttons, false);
				if (p.x != Info->Point.x || p.y != Info->Point.y)
				{
					// Position has changed...
					Info->Point = p;

					// Do captured mouse move...
					Over =	(Info->Point.x >= 0) &&
							(Info->Point.y >= 0) &&
							(Info->Point.x < Info->Sx) &&
							(Info->Point.y < Info->Sy);

					if (Info->Capture)
					{
						// Mouse is captured, send events mouse move events
						// While the capturing is on the BViewRedir doesn't
						// send events because we do it here.
						BMessage *Msg = new BMessage(LGI_MOUSE_MOVE);
						if (Msg)
						{
							Msg->AddPoint("pos", Info->Point);
							Msg->AddInt32("flags", Info->Buttons | _lgi_mouse_keys());
							
							BMessenger m(Info->Wnd->Handle());
							m.SendMessage(Msg);
						}
					}
				}
				
				Info->Wnd->Handle()->UnlockLooper();
			}
			
			LgiSleep(50);
		}
		
		// button has been released
		if (Info->Wnd->Handle()->LockLooper())
		{
			if (Info->MouseClick)
			{
				// Do down click
				Info->Wnd->Handle()->GetMouse(&Info->Point, &Info->Buttons, false);
		
				BMessage *Msg = new BMessage(LGI_MOUSE_CLICK);
				if (Msg)
				{
					Msg->AddPoint("pos", Info->Point);
					Msg->AddInt32("flags", LastButton | _lgi_mouse_keys());
					
					BMessenger m(Info->Wnd->Handle());
					m.SendMessage(Msg);
				}
			}
			// else is just to Capture the mouse
	
			OsView v = Info->Wnd->Handle();
			Info->Wnd->_CaptureThread = 0;
			Info->Wnd->_MouseInfo = 0;
			v->UnlockLooper();
		}
	}
*/

	return 0;
}

bool _setup_mouse_thread(GView::OsMouseInfo *&i, OsThread &t)
{
	/*
	if (!i)
	{
		i = new GView::OsMouseInfo;
		if (i)
		{
			i->Buttons = 0;
			i->Wnd = 0;
			i->Clicks = 0;
			i->MouseClick = false;
			i->Capture = false;
		}
	}

	if (!t)
	{
		t = spawn_thread(_lgi_mouse_thread, "_lgi_mouse_thread", B_NORMAL_PRIORITY, i);
	}
	
	bool Status = i && t > 0;

	return Status;
	*/
	
	return false;
}

bool GView::IsCapturing()
{
	bool Status = false;

	if (_View && _View->LockLooper())
	{
		Status =	_CaptureThread > 0 &&
					_MouseInfo &&
					_MouseInfo->Capture;

		_View->UnlockLooper();
	}

	return Status;
}

bool GView::Capture(bool c)
{
	bool Status = false;

/*
	if (_View &&
		_View->LockLooper())
	{
		bool HadThread = _CaptureThread > 0;
		
		if (c)
		{
			if (_setup_mouse_thread(_MouseInfo, _CaptureThread))
			{
				// setup new Info for capture
				_MouseInfo->Wnd = this;
				_MouseInfo->Clicks = 0;
				_MouseInfo->Sx = Pos.X();
				_MouseInfo->Sy = Pos.Y();

				BPoint p;
				Handle()->GetMouse(&p, &_MouseInfo->Buttons, false);

				Status = true;
			}
		}
		else
		{
			Status = true;
		}

		if (_MouseInfo)
		{
			// Has thread already
			_MouseInfo->Capture = c;

			if (!c &&
				!_MouseInfo->MouseClick)
			{
				kill_thread(_CaptureThread);
				DeleteObj(_MouseInfo);
				_CaptureThread = 0;
			}
			
		}

		if (!HadThread && _CaptureThread)
		{
			resume_thread(_CaptureThread);
		}

		_View->UnlockLooper();
	}
*/

	return Status;
}

void GView::Sys_FrameMoved(BPoint p)
{
	OnPosChange();
}

bool GView::WindowVirtualOffset(GdcPt2 *Offset)
{
	bool Status = false;

	if (Offset)
	{
		Offset->x = 0;
		Offset->y = 0;
		GViewI *Wnd = this;
		while (Wnd)
		{
			GRect r = Wnd->GetPos();
			GViewI *Parent = Wnd->GetParent();
			if (!Wnd->Handle())
			{
				Offset->x += r.x1;
				Offset->y += r.y1;
				Wnd = Parent;
				Status = true;
			}
			else
			{
				Wnd = 0;
			}
		}
	}

	return Status;
}

void GView::OnMouseClick(GMouse &m)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnMouseClick(m);
	}
}

void GView::OnMouseEnter(GMouse &m)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnMouseEnter(m);
	}
}

void GView::OnMouseExit(GMouse &m)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnMouseExit(m);
	}
}

void GView::OnMouseMove(GMouse &m)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnMouseMove(m);
	}
}

bool GView::OnMouseWheel(double Lines)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnMouseWheel(Lines);
	}
	return false;
}

bool GView::OnKey(GKey &k)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnKey(k);
	}
	return false;
}

void GView::OnCreate()
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnCreate();
	}
}

void GView::OnDestroy()
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnDestroy();
	}
}

void GView::OnFocus(bool f)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnFocus(f);
	}
}

void GView::OnPulse()
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnPulse();
	}
}

bool GView::OnRequestClose(bool OsShuttingDown)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnRequestClose(OsShuttingDown);
	}

	return true;
}

int GView::OnHitTest(int x, int y)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnHitTest(x, y);
	}

	return -1;
}

void GView::OnChildrenChanged(GViewI *Wnd, bool Attaching)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnChildrenChanged(Wnd, Attaching);
	}
}

void GView::OnNcPaint(GSurface *pDC, GRect &r)
{
	int Border = Sunken() || Raised() ? _BorderSize : 0;
	if (Border == 2)
	{
		LgiWideBorder(pDC, r, Sunken() ? SUNKEN : RAISED);
	}
	else if (Border == 1)
	{
		LgiThinBorder(pDC, r, Sunken() ? SUNKEN : RAISED);
	}
	// r.Offset(Border, Border);
}

int GView::OnCommand(int Cmd, int Event, OsView Wnd)
{
	if (Script && Script->OnScriptEvent(this))
	{
		Script->OnCommand(Cmd, Event, Wnd);
	}

	return 0;
}

GDragDropTarget *&GView::DropTargetPtr()
{
	return d->DropTarget;
}

bool GView::DropTarget()
{
	return false;
}

bool GView::DropTarget(bool b)
{
	return false;
}

void GView::SetParent(GViewI *p)
{
	d->ParentI = p;
	d->ParentV = p ? p->GetGView() : 0;
}

GViewI *GView::GetParent()
{
	return d->ParentI;
}

void GView::SetNotify(GViewI *p)
{
	d->Notify = p;
}

GViewI *GView::GetNotify()
{
	return d->Notify;
}

GViewFill *GView::GetBackgroundFill()
{
	return d->Background;
}

bool GView::SetBackgroundFill(GViewFill *Fill)
{
	if (d->Background != Fill)
	{
		DeleteObj(d->Background);
		d->Background = Fill;
		Invalidate();
	}

	return true;
}

int GView::GetId()
{
	return d->CtrlId;
}

#define MIN_WINDOW_X		4
#define MIN_WINDOW_Y		24

void GView::MoveOnScreen()
{
	GRect Screen(0, 0, GdcD->X()-1, GdcD->Y()-1);
	GRect p = GetPos();

	if (p.x2 >= Screen.X())
		p.Offset(Screen.X() - p.x2, 0);
	if (p.y2 >= Screen.Y())
		p.Offset(0, Screen.Y() - p.y2);
	if (p.x1 < MIN_WINDOW_X)
		p.Offset(MIN_WINDOW_X-p.x1, 0);
	if (p.y1 < MIN_WINDOW_Y)
		p.Offset(0, MIN_WINDOW_Y-p.y1);
		
	SetPos(p);
}

void GView::MoveToCenter()
{
	GRect Screen(0, 0, GdcD->X()-1, GdcD->Y()-1);
	GRect p = GetPos();

	p.Offset(-p.x1, -p.y1);
	p.Offset((Screen.X() - p.X()) / 2, (Screen.Y() - p.Y()) / 2);
	
	SetPos(p);
}

void GView::MoveToMouse()
{
	GMouse m;
	if (GetMouse(m, true))
	{
		GRect p = GetPos();
		
		p.Offset(-p.x1, -p.y1);
		p.Offset(m.x-(p.X()/2), m.y-(p.Y()/2));
		
		SetPos(p);
	}
	else
	{
		// Next best thing
		MoveToCenter();
	}
}

GdcPt2 &GView::GetWindowBorderSize()
{
	static GdcPt2 s;
	ZeroObj(s);

	if (Handle())
	{
		/*
		RECT Wnd, Client;
		GetWindowRect(Handle(), &Wnd);
		GetClientRect(Handle(), &Client);
		s.x = (Wnd.right-Wnd.left) - (Client.right-Client.left);
		s.y = (Wnd.bottom-Wnd.top) - (Client.bottom-Client.top);
		*/
	}

	return s;
}

bool GView::GetTabStop()
{
	return false;
}

void GView::SetTabStop(bool b)
{
}

LgiCursor GView::GetCursor(int x, int y)
{
	/*
	if (_View && _View->LockLooper())
	{
		if (i)
		{
			struct _cursor
			{
				uint8 Size;
				uint8 Depth;
				uint8 HotX, HotY;
				uint16 Colour[16];
				uint16 Mask[16];
			};
			
			_cursor Data;
			ZeroObj(Data);
			Data.Size = 16;
			Data.Depth = 1;
			Data.HotX = CursorMetrics[i-1].HotSpot.x - CursorMetrics[i-1].Pos.x1;
			Data.HotY = CursorMetrics[i-1].HotSpot.y - CursorMetrics[i-1].Pos.y1;
			
			GSurface *pDC = Cursors.Create();
			if (pDC)
			{
				for (int y=0; y<16; y++)
				{
					uchar *s = (*pDC)[y] + ((i - 1) * 16);
					uint16 *c = Data.Colour + y;
					uint16 *m = Data.Mask + y;
					for (int x=0; x<16; x++)
					{
						int Mask = x < 8 ? 0x100 << x : 0x1 << (x-8);
						switch (s[x])
						{
							case 0: // black
							{
								*c |= Mask;
								*m |= Mask;
								break;
							}
							case 1: // white
							{
								*m |= Mask;
								break;
							}
							default: // transparent
							{
								break;
							}
						}
					}
				}
	
				BCursor c((void*) &Data);
				_View->SetViewCursor(&c);
				
				DeleteObj(pDC);
			}
		}
		else
		{
			_View->SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
		}

		_View->UnlockLooper();
	}
	*/
	
	return false;
}

///////////////////////////////////////////////////////////////////
bool LgiIsMounted(char *Name)
{
	return false;
}

bool LgiMountVolume(char *Name)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
static List<GViewFactory> *Factories;

GViewFactory::GViewFactory()
{
	if (!Factories) Factories = new List<GViewFactory>;
	if (Factories) Factories->Insert(this);
}

GViewFactory::~GViewFactory()
{
	if (Factories)
	{
		Factories->Delete(this);
		if (Factories->Length() == 0)
		{
			DeleteObj(Factories);
		}
	}
}

GView *GViewFactory::Create(const char *Class, GRect *Pos, const char *Text)
{
	if (Factories && ValidStr(Class))
	{
		for (GViewFactory *f=Factories->First(); f; f=Factories->Next())
		{
			GView *v = f->NewView(Class, Pos, Text);
			if (v)
			{
				return v;
			}
		}
	}

	return 0;
}


////////////////////////////////////////////////////////////////
#endif


bool GView::Detach()
{
	bool Status = false;
	if (d->GetParent())
	{
		BView *Par = d->GetParent()->_View;
		if (Par)
		{
			bool Lock = Par->LockLooper();
			Par->RemoveChild(_View);
			if (Lock) Par->UnlockLooper();
		}
		
		d->GetParent()->Children.Delete(this);
		d->GetParent()->OnChildrenChanged(this, false);

		d->ParentI = NULL;
		d->Parent = NULL;
		Status = true;
	}
	return Status;
}

bool GView::IsAttached()
{
	BWindow *BWin = 0;
	BView *BPar = 0;
	
	if (_View)
	{
		BPar = _View->Parent();
		BWin = _View->Window();
	}

	return	_View && BPar;
}

void GView::Quit(bool DontDelete)
{
	if (dynamic_cast<GWindow*>(this))
	{
		BWindow *Wnd = WindowHandle();
		if (Wnd)
		{
			BMessenger m(0, Wnd);
			m.SendMessage(new BMessage(B_QUIT_REQUESTED));
		}
	}
	else if (_View)
	{
		delete this;
	}
}

LgiCursor GView::GetCursor(int x, int y)
{
	return LCUR_Normal;
}

bool GView::GetMouse(GMouse &m, bool ScreenCoords)
{
	/*
	if (Handle()->LockLooper())
	{
		// get mouse state
		BPoint Cursor;
		uint32 Buttons;
		
		Handle()->GetMouse(&Cursor, &Buttons);
		if (ScreenCoords)
		{
			Handle()->ConvertToScreen(&Cursor);
		}
		
		// position
		m.x = Cursor.x;
		m.y = Cursor.y;

		// buttons
		m.Flags =	(TestFlag(Buttons, B_PRIMARY_MOUSE_BUTTON) ? MK_LEFT : 0) |
					(TestFlag(Buttons, B_TERTIARY_MOUSE_BUTTON) ? MK_MIDDLE : 0) |
					(TestFlag(Buttons, B_SECONDARY_MOUSE_BUTTON) ? MK_RIGHT : 0);

		// key states
		key_info KeyInfo;
		if (get_key_info(&KeyInfo) == B_OK)
		{
			m.Flags |=	(TestFlag(KeyInfo.modifiers, B_CONTROL_KEY) ? MK_CTRL : 0) |
						(TestFlag(KeyInfo.modifiers, B_MENU_KEY) ? MK_ALT : 0) |
						(TestFlag(KeyInfo.modifiers, B_SHIFT_KEY) ? MK_SHIFT : 0);
		}
		
		Handle()->UnlockLooper();
		
		return true;
	};
	*/

	return false;
}

bool GView::SetPos(GRect &p, bool Repaint)
{
	Pos = p;

	if (_View)
	{
		bool Lock = _View->LockLooper();

		int Ox = 0, Oy = 0;
		if (d->GetParent())
		{
			if (d->GetParent()->Sunken() || d->GetParent()->Raised())
			{
				Ox = d->GetParent()->_BorderSize;
				Oy = d->GetParent()->_BorderSize;
			}
		}

		_View->MoveTo(p.x1 + Ox, p.y1 + Oy);
		_View->ResizeTo(p.X()-1, p.Y()-1);		
		if (Repaint)
		{
			Invalidate();
		}
		
		if (Lock )
		{
			_View->UnlockLooper();
		}
		else
		{
			OnPosChange();
		}
	}
	
	return TRUE;
}

GViewI *GView::FindControl(OsView hCtrl)
{
	if (Handle() == hCtrl)
	{
		return this;
	}

	List<GViewI>::I Lst = Children.Start();
	for (GViewI *c = Lst.First(); c; c = Lst.Next())
	{
		GViewI *Ctrl = c->FindControl(hCtrl);
		if (Ctrl)
		{
			return Ctrl;
		}
	}
	return 0;
}

void GView::PointToScreen(GdcPt2 &p)
{
	if (_View)
	{
		bool Lock = _View->LockLooper();
		
		BPoint b(p.x, p.y);
		b = _View->ConvertToScreen(b);
		p.x = b.x;
		p.y = b.y;
		
		if (Lock) _View->UnlockLooper();		
	}
	else
	{
		printf("%s:%i - No view.\n", __FILE__, __LINE__);
	}
}

void GView::PointToView(GdcPt2 &p)
{
	if (_View)
	{
		bool Lock = _View->LockLooper();

		BPoint b(p.x, p.y);
		b = _View->ConvertFromScreen(b);
		p.x = b.x;
		p.y = b.y;

		if (Lock) _View->UnlockLooper();		
	}
	else
	{
		printf("%s:%i - No view.\n", __FILE__, __LINE__);
	}
}

bool GView::Invalidate(GRect *r, bool Repaint, bool NonClient)
{
	return false;
	
	if (_View)
	{
		if (_View->Window() &&
			_View->LockLooperWithTimeout(50 * 1000) == B_OK)
		{
			BWindow *Wnd = _View->Window();
			thread_id MyThread = find_thread(0);
			thread_id WndThread = Wnd ? Wnd->Thread() : 0;
			if (MyThread == WndThread)
			{
				if (r)
				{
					if (NonClient)
					{
						BRect Rc = *r;
						_View->Draw(Rc);
					}
					else
					{
						GRect c = GetClient(false);
						GRect a = *r;
						a.Offset(c.x1, c.y1);
						a.Bound(&c);
						BRect Rc = a;
						_View->Draw(Rc);
					}
				}
				else
				{
					// _View->Invalidate(_View->Bounds());
					// _View->Draw(_View->Bounds());
				}

				if (Repaint)
				{
					Wnd->UpdateIfNeeded();
				}
			}
			else
			{
				/*
				// out of thread
				if (r)
				{
					BRect Rc = *r;
					BRegion *Rgn = new BRegion(Rc);
					_View->ConstrainClippingRegion(Rgn);
					_View->Draw(Rc);
					_View->ConstrainClippingRegion(NULL);
				}
				else
				{
					BRect Rc = _View->Frame();
					_View->Draw(Rc);
				}
				*/
			}
			
			_View->UnlockLooper();
		}
		else
		{
			// printf("******** Invalidate couldn't lock ********\n");
		}
	}
	else
	{
		GRect Up;
		GView *p = this;

		if (r)
		{
			Up = *r;
		}
		else
		{
			Up.Set(0, 0, Pos.X()-1, Pos.Y()-1);
		}

		while (p && !p->Handle())
		{
			Up.Offset(p->Pos.x1, p->Pos.y1);
			p = p->d->GetParent();
		}

		if (p && p->Handle())
		{
			return p->Invalidate(&Up, Repaint);
		}
	}
	
	return false;
}

bool GView::_Mouse(GMouse &m, bool Move)
{
	if (GetWindow())
	{
		if (!GetWindow()->OnViewMouse(this, m))
		{
			return;
		}
	}

	if (_Capturing)
	{
		if (Move)
		{
			_Capturing->OnMouseMove(_lgi_adjust_click_for_window(m, _Capturing));
		}
		else
		{
			_Capturing->OnMouseClick(_lgi_adjust_click_for_window(m, _Capturing));
		}
	}
	else
	{
		if (Move)
		{
			bool Change = false;
			GViewI *o = WindowFromPoint(m.x, m.y);

			if (o && !o->Handle())
			{
				printf("Over virtual\n");
			}

			if (_Over != o)
			{
				if (_Over)
				{
					_Over->OnMouseExit(_lgi_adjust_click_for_window(m, _Over));
				}

				_Over = o;

				if (_Over)
				{
					_Over->OnMouseEnter(_lgi_adjust_click_for_window(m, _Over));
				}
			}
		}
			
		GView *Target = dynamic_cast<GView*>(_Over ? _Over : this);
		GRect Client = Target->GView::GetClient();
		if (Target->Raised() || Target->Sunken())
		{
			Client.Offset(Target->_BorderSize, Target->_BorderSize);
		}
		m = _lgi_adjust_click_for_window(m, Target);
		if (!Client.Valid() || Client.Overlap(m.x, m.y))
		{
			if (Move)
			{
				Target->OnMouseMove(m);
			}
			else
			{
				Target->OnMouseClick(m);
			}
		}
	}
	
	return false;
}

void GView::SetPulse(int Length)
{
	/*
	if (_View->LockLooper())
	{
		if (_PulseThread)
		{
			kill_thread(_PulseThread);
			_PulseThread = 0;
		}
	
		_PulseRate = Length;
		
		if (Length > 0)
		{
			_PulseThread = spawn_thread(_lgi_pulse_thread, "_lgi_pulse_thread", B_LOW_PRIORITY, this);
			if (_PulseThread > 0)
			{
				resume_thread(_PulseThread);
			}
		}
		
		_View->UnlockLooper();
	}
	*/
}

GMessage::Result GView::OnEvent(GMessage *Msg)
{
	switch (MsgCode(Msg))
	{
		// These are internal messages sent to a View to get it to
		// handle it's mouse click in thread.
		case LGI_MOUSE_CLICK:
		case LGI_MOUSE_MOVE:
		case LGI_MOUSE_ENTER:
		case LGI_MOUSE_EXIT:
		{
			BPoint p;
			GMouse m;
			if (Msg->FindPoint("pos", &p) == B_OK &&
				Msg->FindInt32("flags", (int32*)&m.Flags) == B_OK)
			{
				m.x = p.x - _BorderSize;
				m.y = p.y - _BorderSize;
				
				switch (MsgCode(Msg))
				{
					case LGI_MOUSE_CLICK:
						OnMouseClick(m); break;
					case LGI_MOUSE_MOVE:
						OnMouseMove(m); break;
					case LGI_MOUSE_ENTER:
						OnMouseEnter(m); break;
					case LGI_MOUSE_EXIT:
						OnMouseExit(m); break;
				}
			}
			break;
		}
		case B_SIMPLE_DATA:
		{
			if (GetParent())
			{
				GetParent()->OnEvent(Msg);
			}
			break;
		}
		case B_MOUSE_WHEEL_CHANGED:
		{
			float Wheel = 0.0;
			if (Msg->FindFloat("be:wheel_delta_y", &Wheel) == B_OK)
			{
				OnMouseWheel(Wheel * 3.0);
			}
			break;
		}
		case M_PULSE:
		{
			OnPulse();
			break;
		}
		case M_DRAG_DROP:
		{
			GDragDropSource *Source = 0;
			if (Msg->FindPointer("GDragDropSource", (void**) &Source) == B_OK)
			{
				GMouse m;
				GetMouse(m);
			
				GView *TargetView = _lgi_search_children(this, m.x, m.y);
				GDragDropTarget *Target = dynamic_cast<GDragDropTarget*>(TargetView);
				if (Target)
				{
					GdcPt2 MousePt(m.x, m.y);
					List<char> Formats;
					if (Target->WillAccept(Formats, MousePt, 0) != DROPEFFECT_NONE)
					{
						GVariant Data;
						if (Source->GetData(&Data, Formats.First()))
						{
							Target->OnDrop(Formats.First(), &Data, MousePt, 0);
						}
					}
				}
			}
			break;
		}
		case M_CHANGE:
		{
			GViewI *Ctrl = dynamic_cast<GViewI*>((GViewI*) MsgA(Msg));
			if (Ctrl)
			{
				OnNotify(Ctrl, MsgB(Msg));
			}
			break;
		}
		case M_COMMAND:
		{
			int32 a=MsgA(Msg);
			return OnCommand(a&0xFFFF, a>>16, (OsView) MsgB(Msg));
		}
		default:
		{
			BMessage *Msg = WindowHandle()->CurrentMessage();
			if (Msg && Msg->WasDropped())
			{
				Msg->PrintToStream();
			}
			break;
		}
	}

	return 0;
}

GRect &GView::GetClient(bool ClientSpace)
{
	static GRect Client;

	Client.ZOff(Pos.X()-1, Pos.Y()-1);
	if (Sunken() || Raised())
	{
		if (ClientSpace)
		{
			Client.x2 -= _BorderSize << 1;
			Client.y2 -= _BorderSize << 1;
		}
		else
		{
			Client.Size(_BorderSize, _BorderSize);
		}
	}
	
	return Client;
}

bool GView::Attach(GViewI *Wnd)
{
	bool Status = false;
	
	SetParent(Wnd);
	if (d->GetParent())
	{
		_Window = d->GetParent()->_Window;
		if (!_View)
			_View = new BViewRedir(this);
	
		if (!d->GetParent()->_View)
		{
			printf("%s:%i - can't attach to parent that isn't attached...\n", __FILE__, __LINE__);
			DeleteObj(_View);
			return false;
		}
		else
		{
			BView *ParentSysView = _View->Parent();
			if (ParentSysView != d->GetParent()->_View)
			{
				if (ParentSysView)
				{
					// detach from the previous BView
					if (ParentSysView->LockLooper())
					{
						ParentSysView->RemoveChild(_View);
						ParentSysView->UnlockLooper();
					}
					else
					{
						printf("%s:%i - Can't remove from parent view.\n", __FILE__, __LINE__);
						return false;
					}
				}
	
				int Ox = 0, Oy = 0;
				if (d->GetParent())
				{
					if (d->GetParent()->Sunken() || d->GetParent()->Raised())
					{
						Ox = d->GetParent()->_BorderSize;
						Oy = d->GetParent()->_BorderSize;
					}
				}
				
				_View->MoveTo(Pos.x1 + Ox, Pos.y1 + Oy);
				_View->ResizeTo(Pos.X()-1, Pos.Y()-1);
				if (!GView::Visible() &&
					!_View->IsHidden())
				{
					_View->Hide();
				}
				if (!Enabled())
				{
					Enabled(false);
				}
				
				// attach to the new BView
				BView *Par = d->GetParent()->_View;
				bool Lock = Par->LockLooper();
				Par->AddChild(_View);
				if (Lock) Par->UnlockLooper();
			}
		}

		if (!d->GetParent()->Children.HasItem(this))
		{
			// we aren't already a member of the parents children
			// list so add ourselves in.
			d->GetParent()->Children.Insert(this);
			d->GetParent()->OnChildrenChanged(this, true);
		}

		Status = true;
	}
	return Status;
}

GView *_lgi_search_children(GView *v, int &x, int &y)
{
	GRect r = v->GetPos();
	if (x >= r.x1 &&
		y >= r.y1 &&
		x < r.x2 &&
		y < r.y2)
	{
		List<GViewI>::I It = v->Children.Start();
		for (GViewI *i=It.First(); i; i=It.Next())
		{
			GRect p = i->GetPos();
			int Cx = x-p.x1;
			int Cy = y-p.y1;
			GView *Child = _lgi_search_children(v, Cx, Cy);
			if (Child)
			{
				x = Cx;
				y = Cy;
				return Child;
			}
		}
		
		return v;
	}

	return 0;
}

void GView::_Delete()
{
	if (_Over == this) _Over = 0;
	if (_Capturing == this) _Capturing = 0;

	if (LgiApp && LgiApp->AppWnd == this)
	{
		LgiApp->AppWnd = 0;
	}

	SetPulse();
	Pos.ZOff(-1, -1);

	GViewI *c;
	while ((c = Children.First()))
	{
		if (c->GetParent() != (GViewI*)this)
		{
			printf("Error: ~GView, child not attached correctly: %p(%s) Parent: %p(%s)\n",
				c, c->Name(),
				c->GetParent(), c->GetParent() ? c->GetParent()->Name() : "");
			Children.Delete(c);
		}

		DeleteObj(c);
	}

	GWindow *w = GetWindow();
	if (w)
		w->SetFocus(this, GWindow::ViewDelete);

	Detach();	
	DeleteObj(_View);
}

GView *&GView::PopupChild()
{
	return d->Popup;
}

static int LastFunctionKey = 0;

void GView::Sys_KeyDown(const char *bytes, int32 numBytes)
{
	key_info Info;
	get_key_info(&Info);

	for (int i=0; i<numBytes; i++)
	{
		GKey k;
		
		if (bytes[i] == B_FUNCTION_KEY)
		{
			k.c16 = 0;
			BMessage *msg = WindowHandle()->CurrentMessage(); 
			if (msg)
			{ 
				int32 key; 
				msg->FindInt32("key", &key); 
				switch (key)
				{ 
					case B_F1_KEY: 
						k.c16 = VK_F1;
						break;
					case B_F2_KEY: 
						k.c16 = VK_F2;
						break;
					case B_F3_KEY: 
						k.c16 = VK_F3;
						break;
					case B_F4_KEY: 
						k.c16 = VK_F4;
						break;
					case B_F5_KEY: 
						k.c16 = VK_F5;
						break;
					case B_F6_KEY: 
						k.c16 = VK_F6;
						break;
					case B_F7_KEY: 
						k.c16 = VK_F7;
						break;
					case B_F8_KEY: 
						k.c16 = VK_F8;
						break;
					case B_F9_KEY: 
						k.c16 = VK_F9;
						break;
					case B_F10_KEY: 
						k.c16 = VK_F10;
						break;
					case B_F11_KEY: 
						k.c16 = VK_F11;
						break;
					case B_F12_KEY: 
						k.c16 = VK_F12;
						break;
					default:
						k.c16 = key;
						break;
				}
			}

			k.IsChar = false;
			LastFunctionKey = k.c16;
		}
		else
		{
			k.c16 = bytes[i];
			k.IsChar = ((k.c16 < 127) && (k.c16 >= ' ')) ||
						(k.c16 == 9) ||
						(k.c16 == 8) ||
						(k.c16 == 10);
		}
		
		k.Flags = modifiers();
		k.Data = 0;
		k.Down(true);
		
		OnKey(k);
	}
}

void GView::Sys_KeyUp(const char *bytes, int32 numBytes)
{
	key_info Info;
	get_key_info(&Info);

	for (int i=0; i<numBytes; i++)
	{
		GKey k;
		
		k.c16 = bytes[i];
		k.Flags = modifiers();
		k.Data = 0;
		k.Down(false);
		k.IsChar = 0;
		
		OnKey(k);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BViewRedir::BViewRedir(GView *wnd, uint32 Resize) :
	BView(	BRect(0, 0, 100, 100),
			"BViewRedir",
			Resize,
			B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE)
{
	Wnd = wnd;
	WndBtn = 0;
	SetViewColor(B_TRANSPARENT_COLOR);
}

BViewRedir::~BViewRedir()
{
}

void BViewRedir::AttachedToWindow()
{
	BView::AttachedToWindow();
	Wnd->OnCreate();
}

void BViewRedir::DetachedFromWindow()
{
	BView::DetachedFromWindow();
	Wnd->OnDestroy();
}

void BViewRedir::Draw(BRect UpdateRect)
{
	GScreenDC DC(this);
	Wnd->_Paint(&DC);
}

void BViewRedir::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	Wnd->OnPosChange();
}

void BViewRedir::Pulse()
{
	BView::Pulse();
	Wnd->OnPulse();
}

void BViewRedir::MessageReceived(BMessage *message)
{
	BView::MessageReceived(message);
	Wnd->OnEvent((GMessage*) message); // Haha, this is so bad... oh well.
}

void BViewRedir::MakeFocus(bool f = true)
{
	BView::MakeFocus(f);
	Wnd->OnFocus(f);
}

void BViewRedir::KeyDown(const char *bytes, int32 numBytes)
{
	BView::KeyDown(bytes, numBytes);
	Wnd->Sys_KeyDown(bytes, numBytes);
}

void BViewRedir::KeyUp(const char *bytes, int32 numBytes)
{
	BView::KeyUp(bytes, numBytes);
	Wnd->Sys_KeyUp(bytes, numBytes);
}

void BViewRedir::MouseDown(BPoint point)
{
	BView::MouseDown(point);

	BPoint u;
	GetMouse(&u, &WndBtn);

	GMouse m;
	m.x = point.x;
	m.y = point.y;
	if (WndBtn == B_PRIMARY_MOUSE_BUTTON) m.Left(true);
	else if (WndBtn == B_SECONDARY_MOUSE_BUTTON) m.Right(true);
	else if (WndBtn == B_TERTIARY_MOUSE_BUTTON) m.Middle(true);
	m.Down(true);
	
	int32 Clicks = 0;
	if (Window()->CurrentMessage()->FindInt32("clicks", &Clicks) == B_OK)
	{
		if (Clicks > 1)
		{
			m.Double(true);
		}
	}
	else
	{
		Window()->CurrentMessage()->PrintToStream();
	}
	
	m.Trace("MouseDown");
	Wnd->_Mouse(m, false);
}

void BViewRedir::MouseUp(BPoint point)
{
	BView::MouseUp(point);

	BPoint u;
	uint32 Btns;
	GetMouse(&u, &Btns);
	int b = Btns ^ WndBtn; // what bits have changed since the down click?

	GMouse m;
	m.Target = Wnd;
	m.x = point.x;
	m.y = point.y;
	if (b == B_PRIMARY_MOUSE_BUTTON) m.Left(true);
	else if (b == B_SECONDARY_MOUSE_BUTTON) m.Right(true);
	else if (b == B_TERTIARY_MOUSE_BUTTON) m.Middle(true);
	m.Down(false);
	
	Wnd->_Mouse(m, false);
	WndBtn = Btns;
}

void BViewRedir::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	BView::MouseMoved(point, transit, message);

	int32 Btns = 0;
	if (message)
		message->FindInt32("buttons", &Btns);
	else
		Window()->CurrentMessage()->FindInt32("buttons", &Btns);

	//EnterExitThread.SetMouseOver(Wnd);

	GMouse m;
	m.x = point.x;
	m.y = point.y;
	if (Btns == B_PRIMARY_MOUSE_BUTTON) m.Left(true);
	else if (Btns == B_SECONDARY_MOUSE_BUTTON) m.Right(true);
	else if (Btns == B_TERTIARY_MOUSE_BUTTON) m.Middle(true);
	m.Down(false);
	
	
	Wnd->_Mouse(m, true);
}

bool BViewRedir::QuitRequested()
{
	GWindow *App = dynamic_cast<GWindow*>(Wnd);
	return (App) ? App->QuitRequested() : true;
}

