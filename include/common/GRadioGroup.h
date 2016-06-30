/// \file
/// \author Matthew Allen
/// \brief A radio group view

#ifndef _GRADIO_GROUP_H_
#define _GRADIO_GROUP_H_

/// A grouping control. All radio buttons that are children of this control will automatically 
/// have only one option selected. Other controls can be children as well but are ignored in the
/// calculation of the groups value. The value of the group is the index into a list of radio buttons
/// of the radio button that is on.
class LgiClass GRadioGroup :
	#ifdef WINNATIVE
	public GControl,
	#else
	public GView,
	#endif
	public ResObject
{
	class GRadioGroupPrivate *d;
	void OnCreate();

public:
	GRadioGroup(int id, int x, int y, int cx, int cy, const char *name, int Init = 0);
	~GRadioGroup();
	
	const char *GetClass() { return "GRadioGroup"; }

	/// Returns the index of the set radio button
	int64 Value();
	/// Sets the 'ith' radio button to on.
	void Value(int64 i);
	/// Adds a radio button to the group.
	GRadioButton *Append(int x, int y, const char *name);

	// Impl
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPaint(GSurface *pDC);
	void OnAttach();
	GMessage::Result OnEvent(GMessage *m);
	bool OnLayout(GViewLayoutInfo &Inf);

	char *Name() { return GView::Name(); }
	char16 *NameW() { return GView::NameW(); }
	bool Name(const char *n);
	bool NameW(const char16 *n);
	void SetFont(GFont *Fnt, bool OwnIt = false);
};

/// A radio button control. A radio button is used to select between mutually exclusive options. i.e.
/// only one can be valid at any given time. For non-mutually exclusive options see the GCheckBox control.
class LgiClass GRadioButton :
	#ifdef WINNATIVE
	public GControl,
	#else
	public GView,
	#endif
	public ResObject
{
	friend class GRadioGroup;
	class GRadioButtonPrivate *d;

public:
	GRadioButton(int id, int x, int y, int cx, int cy, const char *name);
	~GRadioButton();

	const char *GetClass() { return "GRadioButton"; }

	// Impl
	char *Name() { return GView::Name(); }
	char16 *NameW() { return GView::NameW(); }
	bool Name(const char *n);
	bool NameW(const char16 *n);
	int64 Value();
	void Value(int64 i);
	bool OnLayout(GViewLayoutInfo &Inf);

	// Events
	void OnAttach();
	bool OnKey(GKey &k);
	
	#ifdef WINNATIVE
	int SysOnNotify(int Msg, int Code);
	#else
	void OnMouseClick(GMouse &m);
	GMessage::Result OnEvent(GMessage *m);
	void OnMouseEnter(GMouse &m);
	void OnMouseExit(GMouse &m);
	void OnFocus(bool f);
	void OnPaint(GSurface *pDC);
	void SetFont(GFont *Fnt, bool OwnIt = false);
	#endif
};

#endif