/** \file
	\author Matthew Allen
	\brief A button control
 */

#ifndef _GBUTTON_H_
#define _GBUTTON_H_

#ifdef MAC
#define GBUTTON_MIN_X	28
#else
#define GBUTTON_MIN_X	16
#endif

/// \brief A clickable button
///
/// When the user clicks a GButton the OnNotify() event of the GetNotify() or 
/// GetParent() view will be called with this control as the parameter. Allowing
/// action to be taken in response to the click. This event by default bubbles up
/// to the top level window unless some other view intercepts it on the way up
/// the chain of parent views.
class LgiClass GButton :
	public GView,
	public ResObject
{
	class GButtonPrivate *d;

public:
	/// Construct the control
	GButton
	(
		/// The control's ID
		int id,
		/// x coord
		int x,
		/// y coord
		int y,
		/// width
		int cx,
		/// height
		int cy,
		/// Initial text
		const char *name
	);
	~GButton();
	
	const char *GetClass() { return "GButton"; }
	
	/// True if the button is the default action on the dialog
	bool Default();
	/// Sets the button to be the default action on the dialog
	void Default(bool b);
	/// True if the button is down.
	int64 Value();
	/// Sets the button to down.
	void Value(int64 i);

	// Events
	int OnEvent(GMessage *Msg);
	void OnMouseClick(GMouse &m);
	void OnMouseEnter(GMouse &m);
	void OnMouseExit(GMouse &m);
	bool OnKey(GKey &k);
	void OnFocus(bool f);
	void OnPaint(GSurface *pDC);
	void OnAttach();

	// Impl
	char *Name() { return GView::Name(); }
	char16 *NameW() { return GView::NameW(); }
	bool Name(const char *n);
	bool NameW(const char16 *n);
	void SetFont(GFont *Fnt, bool OwnIt = false);
};

#endif