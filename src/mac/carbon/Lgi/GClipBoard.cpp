// MacOSX Clipboard Implementation
#include "Lgi.h"
#include "GClipBoard.h"

#define kClipboardTextType				"public.utf16-plain-text"

class GClipBoardPriv
{
public:
	PasteboardRef Pb;
	
	GClipBoardPriv()
	{
		OSStatus e = PasteboardCreate(kPasteboardClipboard, &Pb);
		if (e) printf("%s:%i - PasteboardCreate failed with %i\n", __FILE__, __LINE__, (int)e);
	}

	~GClipBoardPriv()
	{
		if (Pb)
		{
			CFRelease(Pb);
		}
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

bool GClipBoard::EnumFormats(GArray<FormatType> &Formats)
{
	return false;
}

bool GClipBoard::Empty()
{
	bool Status = false;

	if (d->Pb)
	{
		OSStatus e = PasteboardClear(d->Pb);
		if (e) printf("%s:%i - PasteboardClear failed with %i\n", _FL, (int)e);
		else Status = true;
	}

	Txt.Reset();
	wTxt.Reset();
	DeleteObj(pDC);

	return Status;
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
	{
		Empty();
	}
	
	if (!wTxt.Reset(Utf8ToWide(Str)))
		return false;

	return TextW(wTxt);
}

char *GClipBoard::Text()
{
	char16 *w = TextW();
	Txt.Reset(WideToUtf8(w));
	return Txt;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
		Empty();
	
	if (Str && Owner && d->Pb)
	{
		if (wTxt.Get() != Str)
			wTxt.Reset(NewStrW(Str));

		if (wTxt)
		{
			OSStatus e;

			GAutoPtr<uint16> w((uint16*)LgiNewConvertCp("utf-16", wTxt, LGI_WideCharset));

			CFDataRef Data = CFDataCreate(kCFAllocatorDefault, (UInt8*)w.Get(), StringLen(w.Get()) * sizeof(uint16));
			if (!Data) printf("%s:%i - CFDataCreate failed\n", _FL);
			else
			{
				e = PasteboardClear(d->Pb);
				if (e) printf("%s:%i - PasteboardClear failed with %i\n", _FL, (int)e);

				e = PasteboardPutItemFlavor(d->Pb, (PasteboardItemID)1, CFSTR(kClipboardTextType), Data, 0);
				if (e) printf("%s:%i - PasteboardPutItemFlavor failed with %i\n", _FL, (int)e);
				else
				{
					Status = true;
				}
				
				CFRelease(Data);
			}
		}
	}

	return Status;
}

char16 *GClipBoard::TextW()
{
	Txt.Reset();
	wTxt.Reset();
	if (!d->Pb)
		return NULL;

	ItemCount Items;
	OSStatus e;
	
	e = PasteboardGetItemCount(d->Pb, &Items);
	if (e)
	{
		printf("%s:%i - PasteboardGetItemCount failed with %i\n", _FL, (int)e);
		return NULL;
	}

	for (UInt32 i=1; i<=Items; i++)
	{
		PasteboardItemID Id;
		
		e = PasteboardGetItemIdentifier(d->Pb, i, &Id);
		if (e)
		{
			printf("%s:%i - PasteboardGetItemIdentifier failed with %i\n", _FL, (int)e);
			continue;
		}

		CFArrayRef Flavours;
		e = PasteboardCopyItemFlavors(d->Pb, Id, &Flavours);
		if (e)
		{
			printf("%s:%i - PasteboardCopyItemFlavors failed with %i\n", _FL, (int)e);
			continue;
		}

		int FCount = CFArrayGetCount(Flavours);
		for (CFIndex f=0; f<FCount; f++)
		{
			CFStringRef Type = (CFStringRef)CFArrayGetValueAtIndex(Flavours, f);
			if (UTTypeConformsTo(Type, CFSTR(kClipboardTextType)))
			{
				CFDataRef Data;
				e = PasteboardCopyItemFlavorData(d->Pb, Id, Type, &Data);
				if (e)
				{
					printf("%s:%i - PasteboardCopyItemFlavorData failed with %i\n", _FL, (int)e);
					continue;
				}

				int Size = CFDataGetLength(Data);
				int Ch = Size / sizeof(uint16);
				GAutoPtr<uint16> w(new uint16[Ch+1]);
				if (!w)
					continue;

				memcpy(w, CFDataGetBytePtr(Data), Size);
				w[Ch] = 0;
														
				// Convert '\r' to '\n', which all Lgi applications expect
				bool HasR = false, HasN = false;
				for (uint16 *c = w; *c; c++)
				{
					if (*c == '\n') HasN = true;
					else if (*c == '\r') HasR = true;
				}
				if (HasR && !HasN)
				{
					for (uint16 *c = w; *c; c++)
					{
						if (*c == '\r')
							*c = '\n';
					}
				}
				
				wTxt.Reset((char16*)LgiNewConvertCp(LGI_WideCharset, w, "utf-16", Size));
				break;
			}
		}					
	}
	
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
	GSurface *pDC = NULL;

	LgiAssert(!"Not impl");
	
	return pDC;
}

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	bool Status = false;

	if (Ptr && Len > 0)
	{
		LgiAssert(!"Not impl");
	}

	return Status;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8> &Ptr, ssize_t *Len)
{
	bool Status = false;

	if (Ptr && Len)
	{
		LgiAssert(!"Not impl");
	}

	return Status;
}

