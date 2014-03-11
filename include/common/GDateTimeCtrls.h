/// \file
/// \author Matthew Allen (fret@memecode.com)
#ifndef _DATE_TIME_CTRLS_
#define _DATE_TIME_CTRLS_

#include "MonthView.h"

/// This is a popup window to select a time.
class GTimePopup : public GPopup
{
	GView *Owner;
	GList *Times;
	bool Ignore;

public:
	GTimePopup(GView *owner);
	~GTimePopup();

	void OnCreate();
	void OnPaint(GSurface *pDC);
	void SetTime(GDateTime *t);
	int OnNotify(GViewI *c, int f);
};

/// This class is a little button to pull down the list of times...
/// You use it by creating a custom control in LgiRes and using the control
/// name "GTimePopup". Then at runtime bind it to an editbox by getting the
/// GView handle using GView::FindControl then give it the pointer to the
/// edit box using GView::SetNotify.
class GTimeDropDown :
	public GDropDown,
	public ResObject
{
	class GTimePopup *Drop;
	GViewI *DateSrc;

public:
	GTimeDropDown();

	/// This sets the date source control is the notify control is empty.
	void SetDateSrc(GViewI *ds) { DateSrc = ds; }
	void SetDate(char *d);
	void OnMouseClick(GMouse &m);
	bool OnLayout(GViewLayoutInfo &Inf);
};

/// Popup window used to select a date.
class GDatePopup : public GPopup
{
	GView *Owner;
	GRect Caption;
	GRect Date;
	GRect Prev;
	GRect Next;

	int Cx, Cy; // Cell Dimensions
	MonthView Mv;
	
public:
	GDatePopup(GView *owner);
	~GDatePopup();

	GDateTime Get();
	void Set(GDateTime &Ts);
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void Move(int Dx, int Dy);
	bool OnKey(GKey &k);
};

/// This class is a little button to pull down a date selection control...
/// You use it by creating a custom control in LgiRes and using the control
/// name "GDatePopup". Then at runtime bind it to an editbox by getting the
/// GView handle using GView::FindControl then give it the pointer to the
/// edit box using GView::SetNotify.
class GDateDropDown :
	public GDropDown,
	public ResObject
{
	GDatePopup *Drop;
	GViewI *DateSrc;

public:
	GDateDropDown();

	/// This function sets the date src edit box, the date source is used
	/// to select an appropriate starting point if the Notify control is
	/// empty.
	void SetDateSrc(GViewI *ds) { DateSrc = ds; }
	void SetDate(char *d);
	void OnMouseClick(GMouse &m);
	bool OnLayout(GViewLayoutInfo &Inf);
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnChildrenChanged(GViewI *Wnd, bool Attaching);
};

#endif