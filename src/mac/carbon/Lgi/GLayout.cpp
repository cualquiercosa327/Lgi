/*
**	FILE:			GLayout.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			18/7/1998
**	DESCRIPTION:	Standard Views
**
**	Copyright (C) 1998-2001, Matthew Allen
**		fret@memecode.com
*/

#include <stdio.h>
#include "Lgi.h"
#include "GScrollBar.h"

#define IsValid() ((VScroll ? VScroll->Valid() : false) || (HScroll ? HScroll->Valid() : false))
static char ScrollViewName[] = "[ScrollView]";
static char ScrollBarName[] = "[ScrollBar]";

OsView CreateScrollView(bool x, bool y)
{
	OsView v = 0;
	
	// Create and attach scroll bar
	int Flags = y ? kHIScrollViewOptionsVertScroll : 0;
	Flags |= x ? kHIScrollViewOptionsHorizScroll : 0;
	OSStatus e = HIScrollViewCreate(Flags, &v);
	if (e) printf("%s:%i - HIScrollViewCreate error %i\n", _FL, e);
	else
	{
		SetControlProperty(v, 'meme', 'type', sizeof(ScrollViewName), ScrollViewName);

		for (HIViewRef c = HIViewGetFirstSubview(v); c; c = HIViewGetNextView(c))
		{
			SetControlProperty(c, 'meme', 'type', strlen(ScrollBarName)+1, ScrollBarName);
		}
	}
	
	return v;
}

bool IsScrollView(OsView v)
{
	char Buf[256] = "";
	if (GetControlProperty(v, 'meme', 'type', sizeof(Buf), 0, Buf))
		return false;

	return !stricmp(Buf, ScrollViewName);
}

bool IsScrollBar(OsView v)
{
	char Buf[256] = "";
	if (GetControlProperty(v, 'meme', 'type', sizeof(Buf), 0, Buf))
		return false;

	return !stricmp(Buf, ScrollBarName);
}

bool CarbonRelease(OsView v)
{
	// Null check
	if (!v)
	{
		LgiAssert(!"Null handle.");
		return false;
	}

	// Check that no other control is a child (other than scroll bars)
	HIViewRef c;
	while (c = HIViewGetFirstSubview(v))
	{
		CarbonRelease(c);
	}
	
	// Check retain count...
	int Retain = Retain = CFGetRetainCount(v);
	if (Retain != 1)
	{
		LgiAssert(!"Retain count is weird.");
		return false;
	}
	
	if (HIViewGetSuperview(v))
	{
		OSStatus e = HIViewRemoveFromSuperview(v);
		if (e) printf("%s:%i - HIViewRemoveFromSuperview failed %i\n", _FL, e);
	}
	
	// Release the object...
	//printf("			DisposeControl %p\n", v);
	//DisposeControl(v);
	
	return true;
}


//////////////////////////////////////////////////////////////////////////////
class GLayoutScrollBar : public GScrollBar
{
	GLayout *Lo;

public:
	GLayoutScrollBar(GLayout *lo, int id, int x, int y, int cx, int cy, char *name) :
		GScrollBar(id, x, y, cx, cy, name)
	{
		Lo = lo;
	}

	void OnConfigure()
	{
		Lo->OnScrollConfigure();
	}
};

bool AttachHnd(GViewI *p, OsView Child)
{
	OSStatus e;
	#if defined(kHIViewFeatureIsOpaque)
	HIViewChangeFeatures(Child, kHIViewFeatureIsOpaque, 0);
	#else
	HIViewChangeFeatures(Child, kHIViewIsOpaque, 0);
	#endif
	OsView Par = 0;
	if (dynamic_cast<GWindow*>(p))
	{
		HIViewRef ViewRef = HIViewGetRoot(p->WindowHandle());
		if (ViewRef)
		{
			e = HIViewFindByID(ViewRef, kHIViewWindowContentID, &Par);
			if (e) printf("%s:%i - HIViewFindByID error %i\n", _FL, e);
		}
	}
	else Par = p->Handle();
	if (Par)
	{
		
		e = HIViewAddSubview(Par, Child);
		if (e) printf("%s:%i - HIViewAddSubview error %i\n", _FL, e);
		return e == 0;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////
GLayout::GLayout()
{
	_PourLargest = false;
	VScroll = 0;
	HScroll = 0;
	RealWnd = 0;
	Line.height = Line.width = 20;
}

GLayout::~GLayout()
{
	if (RealWnd)
		Detach();
	DeleteObj(HScroll);
	DeleteObj(VScroll);
}

void GLayout::OnScrollConfigure()
{
	EventRef theEvent;
	CreateEvent(NULL, kEventClassScrollable,
				kEventScrollableInfoChanged, 
				GetCurrentEventTime(),
				kEventAttributeUserEvent, 
				&theEvent);
	SendEventToEventTarget(theEvent, GetControlEventTarget(_View));
	ReleaseEvent(theEvent);
}

bool GLayout::_OnGetInfo(HISize &size, HISize &line, HIRect &bounds, HIPoint &origin)
{
	if (VScroll)
	{
		int64 Min, Max;
		VScroll->Limits(Min, Max);
		int Page = VScroll->Page();
		
		if (Max > Min)
		{
			float Shown = (float) Page / (Max - Min);
			size.height = Y() / Shown;
			// printf("%s Shown=%g Size=%g MinMax=%i Page=%i\n", GetClass(), Shown, size.height, (int)(Max-Min), Page);
		}
		else size.height = Y();
	}
	else
	{
		size.height = Y();
	}
	if (HScroll)
	{
		int64 Min, Max;
		HScroll->Limits(Min, Max);
		int Page = HScroll->Page();

		if (Max > Min)
		{
			float Shown = (float) Page / (Max - Min);
			size.width = X() / Shown;
		}
		else size.width = X();
	}
	else
	{
		size.width = X();
	}

	line = Line;
	origin.x = 0;
	origin.y = 0;
	
	return true;
}

void GLayout::_OnScroll(HIPoint &origin)
{
	bool Change = false;
	if (VScroll)
	{
		int64 n = origin.y;
		if (VScroll->Value() != n)
		{
			VScroll->Value(n);
			Change = true;
		}
	}
	if (HScroll) 
	{
		int64 n = origin.x;
		if (HScroll->Value() != n)
		{
			HScroll->Value(n);
			Change = true;
		}
	}
	
	if (RealWnd && Change)
		HIViewSetNeedsDisplay(RealWnd, true);
}

bool GLayout::GetPourLargest()
{
	return _PourLargest;
}

void GLayout::SetPourLargest(bool i)
{
	_PourLargest = i;
}

bool GLayout::Pour(GRegion &r)
{
	if (_PourLargest)
	{
		GRect *Best = FindLargest(r);
		if (Best)
		{
			SetPos(*Best);
			return true;
		}
	}

	return false;
}

void GLayout::GetScrollPos(int &x, int &y)
{
	if (HScroll)
	{
		x = HScroll->Value();
	}
	else
	{
		x = 0;
	}

	if (VScroll)
	{
		y = VScroll->Value();
	}
	else
	{
		y = 0;
	}
}

void GLayout::SetScrollPos(int x, int y)
{
	if (HScroll)
	{
		HScroll->Value(x);
	}

	if (VScroll)
	{
		VScroll->Value(y);
	}
}

bool GLayout::Attach(GViewI *p)
{
	bool v = IsValid();
	bool Status = false;

	// printf("%s:%i v=%p p=%p, _View=%p\n", _FL, v, p, _View);
	if (v && p)
	{
		// Attach scroll wnd first...
		if (_View)
		{
			LgiAssert(!IsScrollView(_View));
			RealWnd = _View;
		}
		
		_View = CreateScrollView(HScroll && HScroll->Valid(), VScroll && VScroll->Valid());
		// printf("	%s:%i - Attaching, create new ScrollView=%p\n", _FL, _View);
			
		if (_View && AttachHnd(p, _View))
		{
			// Then attach this view as a child
			OsView ScrollWnd = _View;
			if (_CreateCustomView())
			{
				RealWnd = _View;
				_View = ScrollWnd;
				if (AttachHnd(this, RealWnd))
				{
					SetControlVisibility(_View, Visible(), true);
					SetControlVisibility(RealWnd, Visible(), true);
					
					SetParent(p);
					if (!p->HasView(this))
						p->AddView(this);
					
					OnCreate();
					OnAttach();
					Status = true;
				}
				else printf("%s:%i - AttachHnd failed.\n", _FL);
			}
			else printf("%s:%i - CreateCustomView failed\n", _FL);
			
			SetPos(Pos);
		}
		else printf("%s:%i - AttachHnd failed.\n", _FL);
	}
	else
	{
		// Do normal attach because we not showing the scroll bars yet...
		Status = GView::Attach(p);
	}
	return Status;
}

bool GLayout::Detach()
{
	if (RealWnd)
	{
		/*
		printf("%s:%i - Releasing _View=%p {\n", _FL, _View);
		DumpHnd(HIViewGetSuperview(_View), 1);
		
		OSStatus e = HIViewRemoveFromSuperview(_View);
		if (e) printf("%s:%i - HIViewRemoveFromSuperview failed with %i\n", _FL, e);
		e = HIViewRemoveFromSuperview(RealWnd);
		if (e) printf("%s:%i - HIViewRemoveFromSuperview failed with %i\n", _FL, e);
		
		LgiAssert(IsScrollView(_View));
		CarbonRelease(_View);
		printf("}\n");
		*/
		
		OSStatus e = HIViewRemoveFromSuperview(RealWnd);
		if (e) printf("%s:%i - HIViewRemoveFromSuperview failed with %i\n", _FL, e);
		e = HIViewRemoveFromSuperview(_View);
		if (e) printf("%s:%i - error %i\n", _FL, e);
		// CFRelease(_View);
		_View = RealWnd;
		RealWnd = 0;
	}
	
	return GView::Detach();
}

void GLayout::AttachScrollBars()
{
}

bool GLayout::SetScrollBars(bool x, bool y)
{
	static bool Processing = false;

	if (!Processing &&
		(((HScroll!=0) ^ x ) || ((VScroll!=0) ^ y )) )
	{
		Processing = true;
		
		int NeedsScroll = (x?1:0) + (y?1:0);
		int HasScroll = (VScroll?1:0) + (HScroll?1:0);
		
		if (IsAttached())
		{
			LgiAssert(GetParent());
			OSStatus e;

			// Need to change the arrangement of windows
			if (NeedsScroll)
			{
				// Setup Parent->Scroll->Real by inserting the Scroll between real and parent wnds
				if (RealWnd)
				{
					// ie. changing which bars are on the scroll...
					LgiAssert(!IsScrollView(RealWnd));
					LgiAssert(IsScrollView(_View));
					
					e = HIViewRemoveFromSuperview(RealWnd);
					if (e) printf("%s:%i - error %i\n", _FL, e);
					e = HIViewRemoveFromSuperview(_View);
					if (e) printf("%s:%i - error %i\n", _FL, e);
					
					CFRelease(_View);
					_View = 0;
				}
				else
				{
					e = HIViewRemoveFromSuperview(_View);
					if (e) printf("%s:%i - error %i\n", _FL, e);

					// Setting up initial scroll bar..
					LgiAssert(!IsScrollView(_View));
					RealWnd = _View;
				}

				// Create and attach scroll bar
				if (_View = CreateScrollView(x, y))
				{
					if (!AttachHnd(GetParent(), _View))
						printf("%s:%i - AttachHnd failed.\n", _FL);
					else
					{
						if (!AttachHnd(this, RealWnd))
							printf("%s:%i - AttachHnd failed.\n", _FL);
						else
						{
							SetControlVisibility(_View, Visible(), true);
							SetControlVisibility(RealWnd, Visible(), true);

							SetPos(Pos);
							MoveControl(RealWnd, 0, 0);
							SizeControl(RealWnd, Pos.X(), Pos.Y());
							
							printf("ScrollView setup Real=%p, Scroll=%p\n", RealWnd, _View);
						}
					}
				}
			}
			else
			{
				#if 1
				// Remove the ScrollBar (_View) from between the real and parent
				OSStatus e = HIViewRemoveFromSuperview(RealWnd);
				if (e) printf("%s:%i - error %i\n", _FL, e);
				
				e = HIViewRemoveFromSuperview(_View);
				if (e) printf("%s:%i - error %i\n", _FL, e);
				// CFRelease(_View);
				
				// Now connect up the real and parent wnds
				_View = RealWnd;
				RealWnd = 0;
				if (AttachHnd(GetParent(), _View))
				{
					SetPos(Pos);
				}
				else printf("%s:%i - AttachHnd failed.\n", _FL);
				#else
				printf("Removing scroll %s - Real=%p, Scroll=%p, attaching...\n", GetClass(), RealWnd, _View);
				if (AttachHnd(GetParent(), RealWnd))
				{
					// printf("Attached... removing scroll %p\n", _View);
					OSStatus e = HIViewRemoveFromSuperview(_View);
					if (e) printf("%s:%i - error %i\n", _FL, e);

					_View = RealWnd;
					RealWnd = 0;

					// printf("SetPos %s\n", Pos.GetStr());
					SetPos(Pos);
				}
				else printf("%s:%i - AttachHnd failed.\n", _FL);
				#endif
			}
		}
		
		if (x)
		{
			if (!HScroll)
			{
				HScroll = new GLayoutScrollBar(this, IDC_HSCROLL, 0, 0, 100, 10, "GLayout.HScroll");
				if (HScroll)
				{
					HScroll->SetVertical(false);
					HScroll->Visible(false);
				}
			}
		}
		else
		{
			DeleteObj(HScroll);
		}
		if (y)
		{
			if (!VScroll)
			{
				VScroll = new GLayoutScrollBar(this, IDC_VSCROLL, 0, 0, 10, 100, "GLayout.VScroll");
				if (VScroll)
				{
					VScroll->Visible(false);
				}
			}
		}
		else if (VScroll)
		{
			DeleteObj(VScroll);
		}

		Invalidate();
		Processing = false;
	}

	return true;
}

GRect &GLayout::GetClient(bool ClientSpace)
{
	static GRect r;
	r = GView::GetClient(ClientSpace);

	if (_View)
	{
		for (HIViewRef c = HIViewGetFirstSubview(_View); c; c = HIViewGetNextView(c))
		{
			char Buf[256] = "";
			if (!GetControlProperty(c, 'meme', 'type', sizeof(Buf), 0, Buf) &&
				!stricmp(Buf, ScrollBarName))
			{
				HIRect rc;
				HIViewGetFrame(c, &rc);
				if (rc.size.height > rc.size.width)
				{
					// Vertical
					r.x2 = rc.origin.x - 1;
				}
				else
				{
					// Horizontal
					r.y2 = rc.origin.y - 1;
				}
			}
		}
	}

	// printf("%s::GetClient %s\n", GetClass(), r.GetStr());
	return r;
}

int GLayout::OnEvent(GMessage *Msg)
{
	if (VScroll) VScroll->OnEvent(Msg);
	if (HScroll) HScroll->OnEvent(Msg);

	int Status = GView::OnEvent(Msg);

	if (MsgCode(Msg) == M_CHANGE AND
		Status == -1 AND
		GetParent())
	{
		Status = GetParent()->OnEvent(Msg);
	}

	return Status;
}

bool GLayout::Invalidate(GRect *r, bool Repaint, bool NonClient)
{
	if (!RealWnd)
		return GView::Invalidate(r, Repaint, NonClient);

	HIViewSetNeedsDisplay(RealWnd, true);
	return true;
}

bool GLayout::Focus()
{
	return GView::Focus();
}

void GLayout::Focus(bool f)
{
	if (!RealWnd)
		GView::Focus(f);
	else if (f)
	{
		GViewI *Wnd = GetWindow();
		if (Wnd)
		{
			OSErr e = SetKeyboardFocus(Wnd ? Wnd->WindowHandle() : 0, RealWnd, 1);
			if (e) printf("%s:%i - SetKeyboardFocus failed.\n", _FL);
		}
		if (f) SetFlag(WndFlags, GWF_FOCUS);
		else ClearFlag(WndFlags, GWF_FOCUS);
	}
}

bool GLayout::SetPos(GRect &p, bool Repaint)
{
	bool Status = GView::SetPos(p, Repaint);
	if (RealWnd)
	{
		// This makes the child control at least big enough to cover the client area
		// of the scroll view.
		SizeControl(RealWnd, p.X(), p.Y());
	}
	return Status;
}
