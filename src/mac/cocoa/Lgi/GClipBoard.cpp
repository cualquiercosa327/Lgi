// MacOSX Clipboard Implementation
#include "Lgi.h"
#include "GClipBoard.h"

#define kClipboardTextType				"public.utf16-plain-text"

class GClipBoardPriv
{
public:
	
	GClipBoardPriv()
	{
	}

	~GClipBoardPriv()
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
GClipBoard::GClipBoard(GView *o)
{
	d = new GClipBoardPriv;
	Owner = o;
	Open = false;
	pDC = 0;
}

GClipBoard::~GClipBoard()
{
	DeleteObj(d);
}

bool GClipBoard::Empty()
{
	bool Status = false;

	Txt.Reset();
	wTxt.Reset();
	DeleteObj(pDC);

	return Status;
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	if (AutoEmpty)
	{
		Empty();
	}
	
	if (!wTxt.Reset(LgiNewUtf8To16(Str)))
		return false;

	return TextW(wTxt);
}

char *GClipBoard::Text()
{
	char16 *w = TextW();
	Txt.Reset(LgiNewUtf16To8(w));
	return Txt;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
		Empty();
	
	return Status;
}

char16 *GClipBoard::TextW()
{
	Txt.Reset();
	wTxt.Reset();
	return wTxt;
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	if (pDC && Owner)
	{
		LgiAssert(!"Not impl");
	}
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	GSurface *pDC = false;

	LgiAssert(!"Not impl");
	
	return pDC;
}

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, int Len, bool AutoEmpty)
{
	bool Status = false;

	if (Ptr && Len > 0)
	{
		LgiAssert(!"Not impl");
	}

	return Status;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8> &Ptr, int *Len)
{
	bool Status = false;

	if (Ptr && Len)
	{
		LgiAssert(!"Not impl");
	}

	return Status;
}

