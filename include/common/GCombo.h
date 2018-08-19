/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A combo box control

#ifndef __GCOMBO2_H
#define __GCOMBO2_H

/// Combo Box widget.
class LgiClass GCombo :
	#ifdef WINNATIVE
	public GControl,
	#else
	public GView,
	#endif
	public ResObject	
{
private:
	class GComboPrivate *d;

public:
	static GRect Pad;

	/// Constructor
	GCombo
	(
		/// The control ID
		int id,
		/// The left edge x coordinate
		int x,
		/// The top edge y coordinate
		int y,
		/// The width
		int cx,
		/// The height
		int cy,
		/// The initial text
		const char *name = NULL
	);
	~GCombo();

	// Properties
	const char *GetClass() { return "GCombo"; }
	/// Returns whether to sort the list
	bool Sort();
	/// Sets whether to sort the list
	void Sort(bool s);
	/// Returns whether to classify the list into submenus
	int Sub();
	/// Makes the list of entries sort into submenus. Good for large lists.
	void Sub
	(
		/// The type of classification to use. One of #GV_INT32, #GV_DOUBLE or #GV_STRING.
		int Type
	);
	/// Sets the selected item
	void Value(int64 i);
	/// Returns the select item
	int64 Value();
	/// Sets the selected item by name
	bool Name(const char *n);
	/// Returns the selected item's name
	char *Name();
	/// Gets the menu used
	GSubMenu *GetMenu();
	/// Sets the menu used
	void SetMenu(GSubMenu *m);

	/// This state describes what happens to the currently selected
	/// entry in the list when the menu is openned.
	enum SelectedState
	{
		/// Disable the current item
		SelectedDisable,
		/// Hide the current item
		SelectedHide,
		/// Show (enabled) the current item
		SelectedShow
	};

	/// Gets the selected item's state when the menu is shown.
	SelectedState GetSelectedState();
	/// Sets the selected item's state when the menu is shown.
	void SetSelectedState(SelectedState s);

	// List
	
	/// Deletes the currently select item in the list
	bool Delete();
	/// Deletes the item at index 'i'
	bool Delete(size_t i);
	/// Deletes the item matching 'p'
	bool Delete(char *p);
	/// Clears all items in the list
	void Empty();
	/// Inserts a new item
	bool Insert
	(
		/// The new item
		const char *p,
		/// The location to insert or -1 for the end of the list
		int Index = -1
	);
	/// Gets the items in the list
	size_t Length();
	/// Returns the item at index 'i'
	char *operator [](int i);
	/// Returns the index of a given string
	int IndexOf(const char *str);

	// Overridable events
	virtual void DoMenu();

	// Events/Window/Implementation
	GMessage::Result OnEvent(GMessage *Msg);
	void OnAttach();
	bool OnKey(GKey &k);
	
	#if WINNATIVE
	bool SetPos(GRect &p, bool Repaint = false);
	int SysOnNotify(int Msg, int Code);
	#else
	void OnFocus(bool f);
	void OnPaint(GSurface *pDC);
	void OnMouseClick(GMouse &m);
	void OnPosChange();
	void SetFont(GFont *Fnt, bool OwnIt = false);
	#endif
};

#endif

