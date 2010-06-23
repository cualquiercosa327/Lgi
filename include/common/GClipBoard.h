#ifndef _GCLIPBOARD_H
#define _GCLIPBOARD_H

class LgiClass GClipBoard
{
	class GClipBoardPriv *d;
	GView *Owner;
	bool Open;
	char *Txt;
	GSurface *pDC;

#ifdef XWIN
	bool GotEvent;
	void OnEvent(GMessage *Event);
#endif
	
public:
	GClipBoard(GView *o);
	~GClipBoard();

	bool IsOpen() { return true; }
	bool Empty();

	// Text
	bool Text(char *Str, bool AutoEmpty = true);
	char *Text();

	bool TextW(char16 *Str, bool AutoEmpty = true);
	char16 *TextW();

	// Bitmap
	bool Bitmap(GSurface *pDC, bool AutoEmpty = true);
	GSurface *Bitmap();
	#ifdef WIN32
	GSurface *ConvertFromPtr(void *Ptr);
	#endif

	// Binary
	bool Binary(int Format, uint8 *Ptr, int Len, bool AutoEmpty);	// Set
	bool Binary(int Format, uint8 **Ptr, int *Len);	// Get
};

#endif
