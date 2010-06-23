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
	public GView,
	public ResObject
{
	class GRadioGroupPrivate *d;
	void OnCreate();

public:
	GRadioGroup(int id, int x, int y, int cx, int cy, char *name, int Init = 0);
	~GRadioGroup();
	
	char *GetClass() { return "GRadioGroup"; }

	/// Returns the index of the set radio button
	int64 Value();
	/// Sets the 'ith' radio button to on.
	void Value(int64 i);
	/// Adds a radio button to the group.
	GRadioButton *Append(int x, int y, char *name);

	// Impl
	int OnNotify(GViewI *Ctrl, int Flags);
	void OnPaint(GSurface *pDC);
	void OnAttach();
	int OnEvent(GMessage *m);

	char *Name() { return GView::Name(); }
	char16 *NameW() { return GView::NameW(); }
	bool Name(char *n);
	bool NameW(char16 *n);
	void SetFont(GFont *Fnt, bool OwnIt = false);
};

/// A radio button control. A radio button is used to select between mutually exclusive options. i.e.
/// only one can be valid at any given time. For non-mutually exclusive options see the GCheckBox control.
class LgiClass GRadioButton :
	public GView,
	public ResObject
{
	friend class GRadioGroup;
	class GRadioButtonPrivate *d;

public:
	GRadioButton(int id, int x, int y, int cx, int cy, char *name);
	~GRadioButton();

	char *GetClass() { return "GRadioButton"; }

	// Impl
	char *Name() { return GView::Name(); }
	char16 *NameW() { return GView::NameW(); }
	bool Name(char *n);
	bool NameW(char16 *n);
	int64 Value();
	void Value(int64 i);
	void SetFont(GFont *Fnt, bool OwnIt = false);

	// Events
	void OnMouseClick(GMouse &m);
	void OnMouseEnter(GMouse &m);
	void OnMouseExit(GMouse &m);
	bool OnKey(GKey &k);
	void OnFocus(bool f);
	void OnPaint(GSurface *pDC);
	void OnAttach();
	int OnEvent(GMessage *m);
};

#endif