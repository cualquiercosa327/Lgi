/// \file
#ifndef _GCLIPBOARD_H
#define _GCLIPBOARD_H

/// Clipboard API
class LgiClass GClipBoard
{
	class GClipBoardPriv *d;
	GView *Owner;
	bool Open;
	GSurface *pDC;

	GAutoString Txt;
	GAutoWString wTxt;

public:
	/// On windows, this equates to a CF_TEXT, CF_BITMAP, CF_DIB type #define
	typedef uint32 FormatType;

	/// Creates the clipboard access object.
	GClipBoard(GView *o);
	~GClipBoard();

	bool IsOpen() { return Open; }
	GClipBoard &operator =(GClipBoard &c);

	/// Empties the clipboard of it's current content.
	bool Empty();
	bool EnumFormats(GArray<FormatType> &Formats);

	// Text
	bool Text(char *Str, bool AutoEmpty = true); // ptr returned is still owned by this object
	char *Text();

	bool TextW(char16 *Str, bool AutoEmpty = true); // ptr returned is still owned by this object
	char16 *TextW();

	// Bitmap
	bool Bitmap(GSurface *pDC, bool AutoEmpty = true);
	GSurface *Bitmap();
	#if WINNATIVE
	GSurface *ConvertFromPtr(void *Ptr);
	#endif

	// Binary
	bool Binary(FormatType Format, uint8 *Ptr, int Len, bool AutoEmpty);	// Set
	bool Binary(FormatType Format, GAutoPtr<uint8> &Ptr, int *Len);	// Get
};

#endif
