/// \file
/// \author Matthew Allen
/// \brief A text label

#ifndef _GTEXT_LABEL_H_
#define _GTEXT_LABEL_H_

/// A static text label widget
class LgiClass GText :
	public GView,
	public ResObject,
	public GDom
{
	class GTextPrivate *d;

public:
	/// Constructor
	GText
	(
		/// The control's ID
		int id,
		/// Left edge position
		int x,
		/// Top edge position
		int y,
		/// The width
		int cx,
		/// The height
		int cy,
		/// Utf-8 text for the label
		const char *name
	);
	~GText();
	
	const char *GetClass() { return "GTextLabel"; }

	/// Set the text
	bool Name(const char *n);
	/// Set the text with a wide string
	bool NameW(const char16 *n);
	/// Sets the font used to render the text
	void SetFont(GFont *Fnt, bool OwnIt = false);

	/// Returns the numeric value of the text (atoi)
	int64 Value();
	/// Sets the text to a number
	void Value(int64 i);
	/// Gets the text
	char *Name() { return GView::Name(); }
	/// Gets the text as a wide string
	char16 *NameW() { return GView::NameW(); }
	/// Word wrap
	bool GetWrap();
	/// Sets the use of word wrap
	void SetWrap(bool b);
	
	// Events
	GMessage::Param OnEvent(GMessage *Msg);
	void OnPaint(GSurface *pDC);
	void OnPosChange();
	bool OnLayout(GViewLayoutInfo &Inf);
	bool SetVariant(const char *Name, GVariant &Value, char *Array = NULL);
};

#endif
