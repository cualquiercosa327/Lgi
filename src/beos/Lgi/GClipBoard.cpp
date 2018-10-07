// BeOS Clipboard Implementation
#include "Lgi.h"
#include "GClipBoard.h"
#include "Clipboard.h"

///////////////////////////////////////////////////////////////////////////////////////////////
GClipBoard::GClipBoard(GView *o)
{
	Owner = o;
	Open = be_clipboard != 0;
}

GClipBoard::~GClipBoard()
{
}

bool GClipBoard::Empty()
{
	bool Status = false;
	
	if (be_clipboard &&
		be_clipboard->Lock())
	{
		be_clipboard->Clear();
		be_clipboard->Unlock();
		Status = true;
	}
	
	return Status;
}

char16 *GClipBoard::TextW()
{
	char16 *Status = 0;
	char *c8 = Text();
	if (c8)
	{
		Status = LgiNewUtf8To16(c8);
		DeleteArray(c8);
	}
	return Status;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
	bool Status = false;
	char *c8 = LgiNewUtf16To8(Str);
	if (c8)
	{
		Status = Text(c8, AutoEmpty);
		DeleteArray(c8);
	}
	return Status;
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	bool Status = false;
	
	if (Str &&
		be_clipboard &&
		be_clipboard->Lock())
	{
		if (AutoEmpty)
		{
			be_clipboard->Clear();
		}
		
		BMessage *Clip = be_clipboard->Data();
		if (Clip)
		{
			Clip->AddData("text/plain", B_MIME_TYPE, Str, strlen(Str));
			be_clipboard->Commit();
			Status = true;
		}
		
		be_clipboard->Unlock();
	}
	
	return Status;
}

char *GClipBoard::Text()
{
	char *Status = 0;
	
	if (be_clipboard &&
		be_clipboard->Lock())
	{
		BMessage *Clip = be_clipboard->Data();
		if (Clip)
		{
			char *Str = 0;
			ssize_t Len = 0;
			if (Clip->FindData("text/plain", B_MIME_TYPE, (const void **)&Str, &Len) == B_OK)
			{
				Status = NewStr(Str, Len);
			}
			
			be_clipboard->Commit();
		}
		
		be_clipboard->Unlock();
	}
	
	return Status;
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = FALSE;
	if (pDC)
	{
	}
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	return 0;
}

bool GClipBoard::Binary(FormatType Format, uint8 *Ptr, int Len, bool AutoEmpty)
{
	bool Status = false;

	return Status;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8> &Ptr, int *Len)
{
	bool Status = false;

	return Status;
}
