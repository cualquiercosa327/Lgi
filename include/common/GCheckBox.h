/// \file
/// \author Matthew Allen (fret@memecode.com)
/// \brief A check box control

#ifndef _GCHECK_BOX_H_
#define _GCHECK_BOX_H_

/// A checkbox to allow the user to select a boolean setting, i.e. a non-mutually exclusive option. For
/// mutually exclusive options see GRadioButton.
class LgiClass GCheckBox :
	public GView,
	public ResObject
{
	class GCheckBoxPrivate *d;

public:
    static int PadXPx, PadYPx;

	/// Constructor
	GCheckBox
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
		/// The text of the label
		const char *name,
		/// The initial state of the control
		int InitState = false
	);
	~GCheckBox();

	// Methods
	const char *GetClass() { return "GCheckBox"; }
	
	/// Returns whether the control is 3 state
	bool ThreeState();
	/// Sets whether the control is 3 state.
	///
	/// In the case that the control is 3 state, the 3 states are:
	/// <ul>
	/// 	<li> Fully off: 0
	/// 	<li> Fully on: 1
	/// 	<li> Partially on/Undefined: 2
	/// </ul>
	void ThreeState(bool t);
	/// Returns the current value, 0 or 1. Or possibly 2 if ThreeState() is set.
	int64 Value();
	/// Sets the current value.
	void Value(int64 b);

	// Impl
	char *Name() { return GView::Name(); }
	char16 *NameW() { return GView::NameW(); }
	bool Name(const char *n);
	bool NameW(const char16 *n);
	void SetFont(GFont *Fnt, bool OwnIt = false);
    bool OnLayout(GViewLayoutInfo &Inf);

	void OnMouseClick(GMouse &m);
	void OnMouseEnter(GMouse &m);
	void OnMouseExit(GMouse &m);
	bool OnKey(GKey &k);
	void OnFocus(bool f);
	void OnPaint(GSurface *pDC);
	void OnAttach();
	GMessage::Result OnEvent(GMessage *Msg);
};

#endif
