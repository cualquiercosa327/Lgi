/// \file
/// \author Matthew Allen (fret@memecode.com)
#ifndef _DATE_TIME_CTRLS_
#define _DATE_TIME_CTRLS_

class GTimeDrop;
class GDateDrop;

/// This class is a little button to pull down the list of times...
/// You use it by creating a custom control in LgiRes and using the control
/// name "GTimePopup". Then at runtime bind it to an editbox by getting the
/// GView handle using GView::FindControl then give it the pointer to the
/// edit box using GView::SetNotify.
class GTimePopup :
	public GDropDown,
	public ResObject
{
	friend class GTimeDrop;
	GTimeDrop *Drop;
	GViewI *DateSrc;

public:
	GTimePopup();

	/// This sets the date source control is the notify control is empty.
	void SetDateSrc(GViewI *ds) { DateSrc = ds; }

	void SetDate(char *d);
	void OnMouseClick(GMouse &m);
};

/// This class is a little button to pull down a date selection control...
/// You use it by creating a custom control in LgiRes and using the control
/// name "GDatePopup". Then at runtime bind it to an editbox by getting the
/// GView handle using GView::FindControl then give it the pointer to the
/// edit box using GView::SetNotify.
class GDatePopup :
	public GDropDown,
	public ResObject
{
	friend class GDateDrop;
	GDateDrop *Drop;
	GViewI *DateSrc;

public:
	GDatePopup();

	/// This function sets the date src edit box, the date source is used
	/// to select an appropriate starting point if the Notify control is
	/// empty.
	void SetDateSrc(GViewI *ds) { DateSrc = ds; }

	void SetDate(char *d);
	void OnMouseClick(GMouse &m);
};

#endif