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
	static GString FmtToStr(FormatType Fmt);
	static FormatType StrToFmt(GString Fmt);

	/// Creates the clipboard access object.
	GClipBoard(GView *o);
	~GClipBoard();

	bool IsOpen() { return Open; }
	GClipBoard &operator =(GClipBoard &c);

	/// Empties the clipboard of it's current content.
	bool Empty();
	bool EnumFormats(GArray<FormatType> &Formats);

	// Text
	bool Text(char *Str, bool AutoEmpty = true);
	char *Text(); // ptr returned is still owned by this object

	bool TextW(char16 *Str, bool AutoEmpty = true);
	char16 *TextW(); // ptr returned is still owned by this object

	// HTML
	bool Html(const char *doc, bool AutoEmpty = true);
	GString Html();

	// Bitmap
	bool Bitmap(GSurface *pDC, bool AutoEmpty = true);
	GSurface *Bitmap();
	#if WINNATIVE
	GSurface *ConvertFromPtr(void *Ptr);
	#endif

	// Files
	GString::Array Files();
	bool Files(GString::Array &Paths, bool AutoEmpty = true);

	// Binary
	bool Binary(FormatType Format, uint8 *Ptr, ssize_t Len, bool AutoEmpty);	// Set
	bool Binary(FormatType Format, GAutoPtr<uint8> &Ptr, ssize_t *Len);	// Get
};

#endif
