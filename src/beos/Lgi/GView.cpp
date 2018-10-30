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
struct LockerInfo
{
	int Thread;
	const char *File;
	int Line;
	
	LockerInfo()
	{
		Thread = 0;
		File = NULL;
		Line = 0;
	}
};

static LockerInfo Lockers[16];
static LockerInfo *GetLockerInfo(int Thread, bool GiveUnused)
{
	LockerInfo *Unused = NULL;
	for (int i=0; i<16; i++)
	{
		/*
		if (!GiveUnused)
			printf("[%i]=%s,%i - %i\n", i, Lockers[i].File, Lockers[i].Line, Lockers[i].Thread);
		*/
			
		if (Lockers[i].Thread == Thread)
			return Lockers + i;
		
		if (!Unused && Lockers[i].Thread == 0)
			Unused = Lockers + i;
	}
	return GiveUnused ? Unused : NULL;
}

GLocker::GLocker(BHandler *h, const char *file, int line)
{
	hnd = h;
	Locked = false;
	File = file;
	Line = line;
}

GLocker::~GLocker()
{
	Unlock();
}

bool GLocker::Lock()
{
	LgiAssert(!Locked);
	LgiAssert(hnd != NULL);

	while (!Locked)
	{
		status_t result = hnd->LockLooperWithTimeout(1000 * 1000);
		if (result == B_OK)
		{
			Locked = true;
			break;
		}
		else if (result == B_TIMED_OUT)
		{
			// Warn about failure to lock...
			thread_id Thread = hnd->Looper()->LockingThread();
			const char *Lck = GetLocker(Thread);
			printf("%s:%i - Warning: can't lock. Myself=%i Locker=%i,%s\n", _FL, LgiGetCurrentThread(), Thread, Lck);
		}
		else if (result == B_BAD_VALUE)
		{
			break;
		}
		else
		{
			// Warn about error
			printf("%s:%i - Error from LockLooperWithTimeout = 0x%x\n", _FL, result);
		}
	}

	if (Locked)
	{
		LockerInfo *i = GetLockerInfo(LgiGetCurrentThread(), true);
		i->Thread = LgiGetCurrentThread();
		i->File = File;
		i->Line = Line;

		// printf("Lock %i = %s:%i count=%i\n", i->Thread, i->File, i->Line, hnd->Looper()->CountLocks());
	}
	
	return Locked;
}

status_t GLocker::LockWithTimeout(int64 time)
{
	LgiAssert(!Locked);
	LgiAssert(hnd != NULL);
	
	status_t result = hnd->LockLooperWithTimeout(time);
	if (result == B_OK)
	{
		LockerInfo *i = GetLockerInfo(LgiGetCurrentThread(), true);
		i->Thread = LgiGetCurrentThread();
		i->File = File;
		i->Line = Line;
		Locked = true;
		
		// printf("LockWithTimeout %i = %s:%i count=%i\n", i->Thread, i->File, i->Line, hnd->Looper()->CountLocks());
	}
	
	return result;
}

void GLocker::Unlock()
{
	if (Locked)
	{
		hnd->UnlockLooper();
		Locked = false;

		LockerInfo *i = GetLockerInfo(LgiGetCurrentThread(), false);
		if (i)
		{
			// printf("Unlock %i = %s:%i count=%i\n", i->Thread, i->File, i->Line, hnd->Looper()->CountLocks());
			i->Thread = 0;
			i->File = NULL;
			i->Line = 0;
		}
	}
}

const char *GLocker::GetLocker(int Thread)
{
	LockerInfo *i = GetLockerInfo(Thread ? Thread : LgiGetCurrentThread(), false);
	if (!i)
		return NULL;
	
	static char buf[256];
	sprintf_s(buf, sizeof(buf), "%s:%i", i->File, i->Line);
	return buf;
}

///////////////////////////////////////////////////////////////////////////////////////////////
GKey::GKey(int Vkey, int flags)
{
	c16 = vkey = Vkey;
	Flags = flags;
	Data = 0;
	IsChar = false;
}


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
	WantsFocus = false;
	WantsPulse = -1;
	SinkHnd = -1;
}

GViewPrivate::~GViewPrivate()
{
}

GMessage CreateMsg(int m, int a, int b)
{
	GMessage Msg(m, a, b);
	return Msg;
}

GMessage::Param MsgA(GMessage *m)
{
	GMessage::Param i = 0;
	if (m)
		m->FindInt32("a", (int32*)&i);
	return i;
}

GMessage::Param MsgB(GMessage *m)
{
	GMessage::Param i = 0;
	if (m)
		m->FindInt32("b", (int32*)&i);
	return i;
}

bool GView::Detach()
{
	bool Status = false;

	if (_Window)
	{
		GWindow *Wnd = dynamic_cast<GWindow*>(_Window);
		if (Wnd)
			Wnd->SetFocus(this, GWindow::ViewDelete);
		_Window = NULL;
	}
	if (d->GetParent())
	{
		BView *Par = d->GetParent()->_View;
		if (Par)
		{
			GLocker Locker(Par, _FL);
			Locker.Lock();
			Par->RemoveChild(_View);
		}
		
		d->GetParent()->DelView(this);
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
	if (!_View)
		return false;

	GLocker Locker(_View, _FL);
	if (Locker.Lock())
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
		
		return true;
	};

	return false;
}

bool GView::SetPos(GRect &p, bool Repaint)
{
	Pos = p;

	if (_View)
	{
		GLocker Locker(_View, _FL);

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
		
		if (!Locker.IsLocked())
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
		GLocker Locker(_View, _FL);
		
		BPoint b(p.x, p.y);
		b = _View->ConvertToScreen(b);
		p.x = b.x;
		p.y = b.y;
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
		GLocker Locker(_View, _FL);
		Locker.Lock();
		BPoint b(p.x, p.y);
		b = _View->ConvertFromScreen(b);
		p.x = b.x;
		p.y = b.y;
	}
	else
	{
		printf("%s:%i - No view.\n", __FILE__, __LINE__);
	}
}

bool GView::Invalidate(GRect *r, bool Repaint, bool NonClient)
{
	if (_View)
	{
		BWindow *Wnd = _View->Window();
		if (Wnd)
		{
			GLocker Locker(_View, _FL);
			if (Locker.LockWithTimeout(50 * 1000) == B_OK)
			{
				if (r)
				{
					if (NonClient)
					{
						BRect Rc = *r;
						_View->Invalidate(Rc);
					}
					else
					{
						GRect c = GetClient(false);
						GRect a = *r;
						a.Offset(c.x1, c.y1);
						a.Bound(&c);
						BRect Rc = a;
						_View->Invalidate(Rc);
					}
				}
				else
				{
					_View->Invalidate(_View->Bounds());
				}

				if (Repaint)
				{
					Wnd->UpdateIfNeeded();
				}
			}
			else
			{
				printf("%s:%i - Can't lock looper for invalidate\n", _FL);
			}
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

static void SetBeosCursor(LgiCursor c)
{
}

#define DEBUG_MOUSE_CLICK	1
#define DEBUG_MOUSE_MOVE	1

bool GView::_Mouse(GMouse &m, bool Move)
{
	GWindow *Wnd = GetWindow();

	#if DEBUG_MOUSE_CLICK
	if (!Move)
	{
		GString s;
		s.Printf("%s.Click Capture=%s", GetClass(), _Capturing?_Capturing->GetClass():"(none)");
		m.Trace(s);
	}
	#endif
	if (_Capturing)
	{
		if (_Capturing != m.Target)
		{
			m.ToScreen();
			m.Target = _Capturing;
			m.ToView();
		}
		
		if (Move)
		{
			GViewI *c = _Capturing;
			SetBeosCursor(c->GetCursor(m.x, m.y));
			c->OnMouseMove(m);
		}
		else
		{
			if (!Wnd || Wnd->HandleViewMouse(dynamic_cast<GView*>(m.Target), m))
			{
				m.Target->OnMouseClick(m);
				#if DEBUG_MOUSE_CLICK
				LgiTrace("\tCap HandleViewMouse=0\n");
				#endif
			}
			else
			{
				#if DEBUG_MOUSE_CLICK
				LgiTrace("\tCap HandleViewMouse=1\n");
				#endif
			}
		}
	}
	else
	{
		if (_Over != this)
		{
			if (_Over)
			{
				_Over->OnMouseExit(m);
			}

			_Over = this;
			#if DEBUG_MOUSE_CLICK
			if (!Move) LgiTrace("\tCap Over changed to %s\n", _Over?_Over->GetClass():0);
			#endif

			if (_Over)
			{
				_Over->OnMouseEnter(m);
			}
		}
			
		GView *Target = dynamic_cast<GView*>(_Over ? _Over : this);
		// GLayout *Lo = dynamic_cast<GLayout*>(Target);
		GRect Client = Target->GView::GetClient(false);

		if (!Client.Valid() || Client.Overlap(m.x, m.y))
		{
			if (Move)
			{
				SetBeosCursor(Target->GetCursor(m.x, m.y));
				Target->OnMouseMove(m);
			}
			else
			{
				if (!Wnd || Wnd->HandleViewMouse(Target, m))
				{
					Target->OnMouseClick(m);
					#if DEBUG_MOUSE_CLICK
					LgiTrace("\tClick HandleViewMouse=0 %s\n", Target->GetClass());
					#endif
				}
				else
				{
					#if DEBUG_MOUSE_CLICK
					LgiTrace("\tClick HandleViewMouse=1 %s\n", Target->GetClass());
					#endif
				}
			}
		}
		else
		{
			#if DEBUG_MOUSE_CLICK
			if (!Move)
				LgiTrace("\tClick no client %s, m=%i,%i cli=%s\n", Target->GetClass(), m.x, m.y, Client.GetStr());
			#endif
			return false;
		}
	}
	
	return true;
}

long _lgi_pulse_thread(void *ptr)
{
	GView *Wnd = (GView*) ptr;
	if (Wnd)
	{
		while (Wnd->_PulseRate > 0)
		{
			LgiSleep(Wnd->_PulseRate);
			
			BMessenger m(Wnd->Handle());
			BMessage msg(M_PULSE);
			m.SendMessage(&msg);
		}

		Wnd->_PulseThread = 0;
	}
	else LgiTrace("%s:%i - No view?\n", _FL);
} 

void GView::SetPulse(int Length)
{
	if (!IsAttached())
	{
		d->WantsPulse = Length;
		if (Length > 0)
			printf("%s Not attached, wants pulse %i\n", GetClass(), Length);
		return;
	}

	GLocker Locker(_View, _FL);
	if (Locker.Lock())
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
			if (_PulseThread < B_OK)
			{
				printf("%s:%i - spawn_thread failed with %i\n", _FL, _PulseThread);
			}
			else
			{
				status_t s = resume_thread(_PulseThread);
				// printf("%s starting pulse thread: %i resuming: %i\n", GetClass(), _PulseThread, s);
			}
		}
	}
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
			int CtrlId = MsgA(Msg);
			GViewI *Ctrl = FindControl(CtrlId);
			if (Ctrl)
			{
				int32 Cid = -1;
				Msg->FindInt32("cid", &Cid);
				GMessage::Param Data = MsgB(Msg);

				OnNotify(Ctrl, Data);
			}
			else LgiTrace("%s:%i - Couldn't find ctrl '%i'\n", _FL, CtrlId);
			break;
		}
		case M_COMMAND:
		{
			int32 a=MsgA(Msg);
			return OnCommand(a&0xFFFF, a>>16, (OsView) MsgB(Msg));
		}
		default:
		{
			OsWindow h = WindowHandle();
			if (h)
			{
				BMessage *Msg = WindowHandle()->CurrentMessage();
				if (Msg && Msg->WasDropped())
				{
					Msg->PrintToStream();
				}
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
	SetParent(Wnd);
	if (!d->GetParent())
	{
		printf("%s:%i - No parent to attach to (%s)\n", _FL, GetClass());
		return false;
	}
	if (!d->GetParent()->_View)
	{
		printf("%s:%i - Can't attach to parent that isn't attached (%s)\n", _FL, GetClass());
		return false;
	}

	_Window = d->GetParent()->_Window;
	if (!_View)
		_View = new BViewRedir(this);

	BView *ParentSysView = _View->Parent();
	if (ParentSysView != d->GetParent()->_View)
	{
		if (ParentSysView)
		{
			// detach from the previous BView
			GLocker Locker(ParentSysView, _FL);
			if (Locker.Lock())
			{
				ParentSysView->RemoveChild(_View);
				Locker.Unlock();
			}
			else
			{
				printf("%s:%i - Can't remove from parent view (%s).\n", _FL, GetClass());
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

		GLocker Locker(Par, _FL);
		Locker.Lock();
		Par->AddChild(_View);
	}

	if (d->WantsFocus)
	{
		d->WantsFocus = false;
		_View->MakeFocus();
		// printf("Processing wants focus for %i\n", GetId());
	}
	if (d->WantsPulse)
	{
		SetPulse(d->WantsPulse);
		d->WantsPulse = -1;
	}

	if (!d->GetParent()->Children.HasItem(this))
	{
		// we aren't already a member of the parents children
		// list so add ourselves in.
		d->GetParent()->Children.Insert(this);
		d->GetParent()->OnChildrenChanged(this, true);
	}

	return true;
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
	if (_PulseThread)
	{
		kill_thread(_PulseThread);
		_PulseThread = 0;
	}
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

void GView::_Key(const char *bytes, int32 numBytes, bool down)
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
		
		k.vkey = k.c16;
		k.Flags = 0;
		k.Data = 0;
		k.Down(down);

		int mods = modifiers();
		if (mods & B_SHIFT_KEY)
			k.Shift(true);
		if (mods & B_CONTROL_KEY)
			k.Ctrl(true);
		if (mods & B_COMMAND_KEY)
			k.Alt(true);
		if (mods & B_OPTION_KEY)
			k.System(true);

		// k.Trace("sys down");		

		GWindow *w = GetWindow();
		if (w)
			w->HandleViewKey(this, k);
		else
			OnKey(k);
	}
}

void GView::_Focus(bool f)
{
	if (f)
		SetFlag(WndFlags, GWF_FOCUS);
	else
		ClearFlag(WndFlags, GWF_FOCUS);

	OnFocus(f);
	Invalidate();

	// printf("_Focus(%i) view=%p id=%i attached=%i\n", f, _View, GetId(), IsAttached());
	if (f)
	{
		if (IsAttached())
		{
			GLocker Locker(_View, _FL);
			Locker.Lock();
			_View->MakeFocus();
		}
		else
		{
			d->WantsFocus = true;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
BViewRedir::BViewRedir(GView *wnd, uint32 Resize) :
	BView(	BRect(0, 0, 100, 100),
			"BViewRedir",
			Resize,
			B_WILL_DRAW | B_NAVIGABLE | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE
			// This doesn't work: B_WILL_ACCEPT_FIRST_CLICK
			)
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
	Wnd->OnDestroy();
}

void BViewRedir::Draw(BRect UpdateRect)
{
	GScreenDC DC(this);
	Wnd->_Paint(&DC);
}

void BViewRedir::FrameResized(float width, float height)
{
	Wnd->OnPosChange();
}

void BViewRedir::Pulse()
{
	Wnd->OnPulse();
}

void BViewRedir::MessageReceived(BMessage *message)
{
	Wnd->OnEvent((GMessage*) message); // Haha, this is so bad... oh well.
}

void BViewRedir::KeyDown(const char *bytes, int32 numBytes)
{
	Wnd->_Key(bytes, numBytes, true);
}

void BViewRedir::KeyUp(const char *bytes, int32 numBytes)
{
	Wnd->_Key(bytes, numBytes, false);
}

void BViewRedir::MouseDown(BPoint point)
{
	BPoint u;
	GetMouse(&u, &WndBtn);

	GMouse m;
	m.x = point.x;
	m.y = point.y;
	if (WndBtn == B_PRIMARY_MOUSE_BUTTON) m.Left(true);
	else if (WndBtn == B_SECONDARY_MOUSE_BUTTON) m.Right(true);
	else if (WndBtn == B_TERTIARY_MOUSE_BUTTON) m.Middle(true);
	m.Down(true);
	m.Target = Wnd;
	
	int32 Clicks = 0;	
	BWindow *w = Window();
	if (w)
	{
		if (w->CurrentMessage()->FindInt32("clicks", &Clicks) == B_OK)
		{
			if (Clicks > 1)
				m.Double(true);
		}
		else
		{
			w->CurrentMessage()->PrintToStream();
		}
		
		w->Activate();
	}
		
	Wnd->_Mouse(m, false);
	BView::MouseDown(point);
}

void BViewRedir::MouseUp(BPoint point)
{
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
	m.Target = Wnd;
	
	Wnd->_Mouse(m, false);
	WndBtn = Btns;
	BView::MouseUp(point);
}

void BViewRedir::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	int32 Btns = 0;
	if (message)
		message->FindInt32("buttons", &Btns);
	else
		Window()->CurrentMessage()->FindInt32("buttons", &Btns);

	GMouse m;
	m.x = point.x;
	m.y = point.y;
	if (Btns == B_PRIMARY_MOUSE_BUTTON) m.Left(true);
	else if (Btns == B_SECONDARY_MOUSE_BUTTON) m.Right(true);
	else if (Btns == B_TERTIARY_MOUSE_BUTTON) m.Middle(true);
	m.Down(false);
	m.Target = Wnd;
	
	Wnd->_Mouse(m, true);
	BView::MouseMoved(point, transit, message);
}

bool BViewRedir::QuitRequested()
{
	GWindow *App = dynamic_cast<GWindow*>(Wnd);
	return (App) ? App->QuitRequested() : true;
}

