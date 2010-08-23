#ifndef _GCLIPBOARD_H
#define _GCLIPBOARD_H

class LgiClass GClipBoard
{
	class GClipBoardPriv *d;
	GView *Owner;
	bool Open;
	char *Txt;
	GSurface *pDC;

public:
	#if defined(WIN32NATIVE)
	typedef int FormatType;
	#else
	#error "Not impl."
	#endif

	GClipBoard(GView *o);
	~GClipBoard();

	bool IsOpen() { return Open; }
	bool Empty();
	bool EnumFormats(GArray<FormatType> &Formats);

	// Text
	bool Text(char *Str, bool AutoEmpty = true);
	char *Text();

	bool TextW(char16 *Str, bool AutoEmpty = true);
	char16 *TextW();

	// Bitmap
	bool Bitmap(GSurface *pDC, bool AutoEmpty = true);
	GSurface *Bitmap();
	#if WIN32NATIVE
	GSurface *ConvertFromPtr(void *Ptr);
	#endif

	// Binary
	bool Binary(FormatType Format, uint8 *Ptr, int Len, bool AutoEmpty);	// Set
	bool Binary(FormatType Format, uint8 **Ptr, int *Len);	// Get
};

#endif
