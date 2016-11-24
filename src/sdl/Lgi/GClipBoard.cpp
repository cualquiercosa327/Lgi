// Clipboard Implementation
#include "Lgi.h"
#include "GVariant.h"
#include "GClipBoard.h"

#define DEBUG_CLIPBOARD		0

class GClipBoardPriv
{
public:
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
	return false;
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
	{
		Empty();
	}
	

	return Status;
}

char *GClipBoard::Text()
{
    char *t = 0;
    
	
	return t;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
    GAutoString u(WideToUtf8(Str));
    return Text(u, AutoEmpty);
}

char16 *GClipBoard::TextW()
{
    GAutoString u(Text());
    return Utf8ToWide(u);
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	return pDC;
}

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, int Len, bool AutoEmpty)
{
	bool Status = false;

	if (Ptr && Len > 0)
	{
	}

	return Status;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8> &Ptr, int *Len)
{
	bool Status = false;

	if (Ptr && Len)
	{
	}

	return Status;
}

