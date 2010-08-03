// BeOS Clipboard Implementation
#include "Lgi.h"

#define kClipboardTextType				"public.utf16-plain-text"

class GClipBoardPriv
{
public:
	PasteboardRef Pb;
	
	GClipBoardPriv()
	{
		OSStatus e = PasteboardCreate(kPasteboardClipboard, &Pb);
		if (e) printf("%s:%i - PasteboardCreate failed with %i\n", __FILE__, __LINE__, e);
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
	Txt = 0;
	pDC = 0;
}

GClipBoard::~GClipBoard()
{
	DeleteObj(d);
}

bool GClipBoard::Empty()
{
	bool Status = false;

	if (d->Pb)
	{
		OSStatus e = PasteboardClear(d->Pb);
		if (e) printf("%s:%i - PastbaordClear failed with %i\n", __FILE__, __LINE__, e);
		else Status = true;
		
		DeleteArray(Txt);
		DeleteObj(pDC);
	}

	return Status;
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
	{
		Empty();
	}
	
	if (Str AND Owner AND d->Pb)
	{
		Str = NewStr(Str);
		if (Str)
		{
			char16 *w = LgiNewUtf8To16(Str);
			if (w)
			{
				OSStatus e;

				CFDataRef Data = CFDataCreate(kCFAllocatorDefault, (UInt8*)w, StrlenW(w) * sizeof(*w));
				if (!Data) printf("%s:%i - CFDataCreate failed\n", _FL);
				else
				{
					e = PasteboardClear(d->Pb);
					if (e) printf("%s:%i - PasteboardClear failed with %i\n", _FL, e);

					e = PasteboardPutItemFlavor(d->Pb, (PasteboardItemID)1, CFSTR(kClipboardTextType), Data, 0);
					if (e) printf("%s:%i - PasteboardPutItemFlavor failed with %i\n", _FL, e);
					else
					{
						Status = true;
					}
					
					CFRelease(Data);
				}
			}
		}
	}

	return Status;
}

char *GClipBoard::Text()
{
	char *Status = 0;
	
	DeleteArray(Txt);
	if (d->Pb)
	{
		ItemCount Items;
		OSStatus e;
		
		e = PasteboardGetItemCount(d->Pb, &Items);
		if (e) printf("%s:%i - PasteboardGetItemCount failed with %i\n", __FILE__, __LINE__, e);
		else
		{
			for (UInt32 i=1; i<=Items; i++)
			{
				PasteboardItemID Id;
				
				e = PasteboardGetItemIdentifier(d->Pb, i, &Id);
				if (e) printf("%s:%i - PasteboardGetItemIdentifier failed with %i\n", __FILE__, __LINE__, e);
				else
				{
					CFArrayRef Flavours;
					e = PasteboardCopyItemFlavors(d->Pb, Id, &Flavours);
					if (e) printf("%s:%i - PasteboardCopyItemFlavors failed with %i\n", __FILE__, __LINE__, e);
					else
					{
						int FCount = CFArrayGetCount(Flavours);
						for (CFIndex f=0; f<FCount; f++)
						{
							CFStringRef Type = (CFStringRef)CFArrayGetValueAtIndex(Flavours, f);
							if (UTTypeConformsTo(Type, CFSTR(kClipboardTextType)))
							{
								CFDataRef Data;
								e = PasteboardCopyItemFlavorData(d->Pb, Id, Type, &Data);
								if (e) printf("%s:%i - PasteboardCopyItemFlavorData failed with %i\n", __FILE__, __LINE__, e);
								else
								{
									int Size = CFDataGetLength(Data);
									int Ch = Size / sizeof(char16);
									char16 *w = new char16[Ch+1];
									if (w)
									{
										memcpy(w, CFDataGetBytePtr(Data), Size);
										w[Ch] = 0;
										
										Txt = Status = LgiNewUtf16To8(w);
										DeleteArray(w);
									}
								}
							}
						}
					}					
				}
			}
		}	
	}
	
	return Status;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
	bool Status = false;


	return Status;
}

char16 *GClipBoard::TextW()
{
	char16 *Status = 0;


	return Status;
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	if (pDC AND Owner)
	{
	}
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	GSurface *pDC = false;
	
	return pDC;
}

bool GClipBoard::Binary(int Format, uchar *Ptr, int Len, bool AutoEmpty)
{
	bool Status = false;

	if (Ptr AND Len > 0)
	{
	}

	return Status;
}

bool GClipBoard::Binary(int Format, uchar **Ptr, int *Len)
{
	bool Status = false;

	if (Ptr AND Len)
	{
	}

	return Status;
}

