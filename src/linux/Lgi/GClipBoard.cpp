// Clipboard Implementation
#include "Lgi.h"

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


	return Status;
}

bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
	{
		Empty();
	}
	
	if (Str && Owner)
	{
		GVariant v;
		LgiApp->SetClipBoardContent(Owner->Handle(), v = Str);
		Status = true;
	}

	return Status;
}

char *GClipBoard::Text()
{
	char *Status = 0;

	if (Owner)
	{
		GArray<char*> Types;
		Types.Add("UTF8_STRING");
		Types.Add("UTF-8");
		GVariant v;
		if (LgiApp->GetClipBoardContent(Owner->Handle(), v, Types))
		{
			if (Status = v.Str())
			{
				v.Value.String = 0;
			}
		}
	}
	
	return Status;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
	bool Status = false;

	if (AutoEmpty)
	{
		Empty();
	}
	
	if (Str && Owner)
	{
		GVariant v;
		LgiApp->SetClipBoardContent(Owner->Handle(), v = Str);
		Status = true;
	}

	return Status;
}

char16 *GClipBoard::TextW()
{
	char16 *Status = 0;

	if (Owner)
	{
		GArray<char*> Types;
		Types.Add("UTF8_STRING");
		Types.Add("UTF-8");
		GVariant v;
		if (LgiApp->GetClipBoardContent(Owner->Handle(), v, Types))
		{
			if (Status = v.WStr())
			{
				v.Value.WString = 0;
			}
		}
	}
	
	return Status;
}

bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = false;
	if (pDC AND Owner)
	{
		#if defined XWIN
		/*
		XSetSelectionOwner(XDisplay(), XApp()->GetClipboard(), Owner->Handle()->handle(), CurrentTime);
		XApp()->SetClipImage(Owner->Handle(), pDC);
		Status = true;
		*/
		#endif
	}
	return Status;
}

GSurface *GClipBoard::Bitmap()
{
	GSurface *pDC = false;

	#if defined XWIN
	GotEvent = false;
	
	if (Owner)
	{
		/*
		Atom Clipboard = XApp()->GetClipboard();
		Atom APixmap = XInternAtom(XDisplay(), "PIXMAP", false);

		// Try and get a list of target types
		uchar *Data = 0;
		ulong Len = 0;
		if (XApplication::GetApp()->GetSelection(Owner->Handle()->handle(), Clipboard, APixmap, Data, Len))
		{
			Pixmap p = ((Pixmap*)Data)[0];
			
			Window Root;
			int x, y, e;
			uint Width, Height, Border = 0, Depth = 0;
			if ((e = XGetGeometry(XDisplay(), p, &Root, &x, &y, &Width, &Height, &Border, &Depth)) == 1)
			{			
				XImage *Temp = XGetImage(XDisplay(), p, 0, 0, Width, Height, -1, ZPixmap);
				if (Temp)
				{
					pDC = NEW(GMemDC);
					if (pDC)
					{
						if (pDC->Create(Width, Height, GdcD->GetBits()))
						{
							memcpy( (*pDC)[0], Temp->data, Temp->bytes_per_line * Temp->height);
						}
						else
						{
							DeleteObj(pDC);
						}		
					}
					
					XDestroyImage(Temp);
				}
				else
				{
					printf("%s,%i - No image returned from XGetImage.\n", __FILE__, __LINE__);
				}
			}
			
			XFree(Data);
		}
		else
		{
			printf("%s,%i - Couldm't GetSelection.\n", __FILE__, __LINE__);
		}
		*/
	}
	#endif
	
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

