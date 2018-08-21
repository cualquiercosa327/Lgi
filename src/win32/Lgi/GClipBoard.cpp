
#include "Lgi.h"
#include "GClipBoard.h"
#include "GPalette.h"
#include "GCom.h"

class GClipBoardPriv
{
public:
	GAutoString Utf8;
	GAutoWString Wide;
};

class LFileEnum : public GUnknownImpl<IEnumFORMATETC>
{
	int Idx;
	GClipBoard::FormatType Type;

public:
	LFileEnum(GClipBoard::FormatType type)
	{
		Idx = 0;
		Type = type;
		AddInterface(IID_IEnumFORMATETC, (IEnumFORMATETC*)this);
	}

	~LFileEnum()
	{
		int asd=0;
	}

    HRESULT STDMETHODCALLTYPE Next(ULONG celt, FORMATETC *fmt, ULONG *pceltFetched)
	{
		if (!fmt || Idx)
			return S_FALSE;

		fmt->cfFormat = Type;
		fmt->dwAspect = DVASPECT_CONTENT;
		fmt->lindex = -1;
		fmt->ptd = NULL;
		fmt->tymed = TYMED_FILE;

		if (pceltFetched) *pceltFetched = 1;
		Idx += celt;
		return S_OK;
	}
        
    HRESULT STDMETHODCALLTYPE Skip(ULONG celt)
	{
		return S_OK;
	}
        
    HRESULT STDMETHODCALLTYPE Reset()
	{
		return S_OK;
	}

    HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC **ppenum)
	{
		return E_NOTIMPL;
	}
};

struct LFileName : public GUnknownImpl<IUnknown>
{
	char16 *w;

	LFileName(STGMEDIUM *Med, const char *u)
	{
		Med->tymed = TYMED_FILE;
		Med->lpszFileName = w = Utf8ToWide(u);
		Med->pUnkForRelease = this;
	}

	~LFileName()
	{
		DeleteArray(w);
	}
};

class LFileData : public GUnknownImpl<IDataObject>
{
	int Cur;
	GClipBoard::FormatType Type, PrefDrop, ShellIdList;

public:
	GString::Array Files;

	LFileData(GString::Array &files) : Files(files)
	{
		Type = GClipBoard::StrToFmt(CFSTR_FILENAMEA);
		PrefDrop = GClipBoard::StrToFmt(CFSTR_PREFERREDDROPEFFECT);
		ShellIdList = GClipBoard::StrToFmt(CFSTR_SHELLIDLIST);
		Cur = 0;
		AddInterface(IID_IDataObject, (IDataObject*)this);
	}

	~LFileData()
	{
		int asd=0;
	}

	HRESULT STDMETHODCALLTYPE GetData(FORMATETC *Fmt, STGMEDIUM *Med)
	{
		GString sFmt = GClipBoard::FmtToStr(Fmt->cfFormat);
		
		if (!Med)
			return E_INVALIDARG;
		if (Fmt->cfFormat == PrefDrop)
		{
			Med->tymed = TYMED_HGLOBAL;
			Med->hGlobal = GlobalAlloc(GHND, sizeof(DWORD));
			DWORD* data = (DWORD*)GlobalLock(Med->hGlobal);
			*data = DROPEFFECT_COPY;
			GlobalUnlock(Med->hGlobal);
			Med->pUnkForRelease = NULL;
		}
		else if (Fmt->cfFormat == Type)
		{
			if (!Med)
				return E_INVALIDARG;
			new LFileName(Med, Files[Cur++]);
		}
		else if (Fmt->cfFormat == ShellIdList)
		{
			return E_NOTIMPL;
		}
		else
		{
			LgiTrace("%s:%i - GetData(%s) not supported.\n", _FL, sFmt.Get());
			return DV_E_FORMATETC;
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium)
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *Fmt)
	{
		if (Fmt->cfFormat == Type ||
			Fmt->cfFormat == ShellIdList)
			return S_OK;
		
		GString sFmt = GClipBoard::FmtToStr(Fmt->cfFormat);
		LgiTrace("%s:%i - QueryGetData(%s) not supported.\n", _FL, sFmt.Get());
		return DV_E_FORMATETC;
	}

	HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *Fmt, FORMATETC *pformatetcOut)
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE SetData(FORMATETC *Fmt, STGMEDIUM *pmedium, BOOL fRelease)
	{
		GString sFmt = GClipBoard::FmtToStr(Fmt->cfFormat);
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dir, IEnumFORMATETC **enumFmt)
	{
		if (dir == DATADIR_GET)
		{
			*enumFmt = new LFileEnum(Type);
			return S_OK;
		}
		else if (dir == DATADIR_SET)
		{
		}

		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection)
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection)
	{
		return E_NOTIMPL;
	}

	HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **ppenumAdvise)
	{
		return E_NOTIMPL;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////
GString GClipBoard::FmtToStr(FormatType Fmt)
{
	TCHAR n[256] = {0};
	GetClipboardFormatName(Fmt, n, CountOf(n));
	return n;
}

GClipBoard::FormatType GClipBoard::StrToFmt(GString Fmt)
{
	return RegisterClipboardFormatA(Fmt);
}

///////////////////////////////////////////////////////////////////////////////////////////////
GClipBoard::GClipBoard(GView *o)
{
    d = new GClipBoardPriv;
	Open = false;
	Owner = o;
	if (Owner)
		Open = OpenClipboard(Owner->Handle()) != 0;
}

GClipBoard::~GClipBoard()
{
	if (Open)
	{
		CloseClipboard();
		Open = FALSE;
	}
	DeleteObj(d);
}

GClipBoard &GClipBoard::operator =(GClipBoard &c)
{
    LgiAssert(0);
    return *this;
}

bool GClipBoard::EnumFormats(GArray<FormatType> &Formats)
{
	UINT Idx = 0;
	UINT Fmt;
	while (Fmt = EnumClipboardFormats(Idx))
	{
		Formats.Add(Fmt);
		Idx++;
	}
	return Formats.Length() > 0;
}

bool GClipBoard::Empty()
{
	d->Utf8.Reset();
	d->Wide.Reset();

	return EmptyClipboard() != 0;
}

// Text
bool GClipBoard::Text(char *Str, bool AutoEmpty)
{
	bool Status = false;

	if (Str)
	{
		GAutoString Native(LgiToNativeCp(Str));
		if (Native)
		{
			Status = Binary(CF_TEXT, (uchar*)Native.Get(), strlen(Native)+1, AutoEmpty);
		}
		else LgiTrace("%s:%i - Conversion to native cs failed.\n", _FL);
	}
	else LgiTrace("%s:%i - No text.\n", _FL);

	return Status;
}

char *GClipBoard::Text()
{
	ssize_t Len = 0;
	GAutoPtr<uint8> Str;
	if (Binary(CF_TEXT, Str, &Len))
	{
		d->Utf8.Reset(LgiFromNativeCp((char*)Str.Get()));
		return d->Utf8;
	}

	return NULL;
}

bool GClipBoard::TextW(char16 *Str, bool AutoEmpty)
{
	if (Str)
	{
		auto Len = StrlenW(Str);
		return Binary(CF_UNICODETEXT, (uchar*) Str, (Len+1) * sizeof(ushort), AutoEmpty);
	}

	return false;
}

char16 *GClipBoard::TextW()
{
	ssize_t Len = 0;
	GAutoPtr<uint8> Str;
	if (Binary(CF_UNICODETEXT, Str, &Len))
	{
		d->Wide.Reset(NewStrW((char16*) Str.Get(), Len / 2));
		return d->Wide;
	}

	return NULL;
}

// Bitmap
bool GClipBoard::Bitmap(GSurface *pDC, bool AutoEmpty)
{
	bool Status = FALSE;

	Empty();
	if (pDC)
	{
		int Bytes = BMPWIDTH(pDC->X() * pDC->GetBits());
		int Colours = (pDC->GetBits()>8) ? ((pDC->GetBits() != 24) ? 3 : 0) : 1 << pDC->GetBits();
		int HeaderSize = sizeof(BITMAPINFOHEADER);
		int PaletteSize = Colours * sizeof(RGBQUAD);
		int MapSize = Bytes * pDC->Y();
		int TotalSize = HeaderSize + PaletteSize + MapSize;

		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, TotalSize);
		if (hMem)
		{
			BITMAPINFO *Info = (BITMAPINFO*) GlobalLock(hMem);
			if (Info)
			{
				// Header
				Info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				Info->bmiHeader.biWidth = pDC->X();
				Info->bmiHeader.biHeight = pDC->Y();
				Info->bmiHeader.biPlanes = 1;
				Info->bmiHeader.biBitCount = pDC->GetBits();
				
				if (pDC->GetBits() == 16 ||
					pDC->GetBits() == 32)
				{
					Info->bmiHeader.biCompression = BI_BITFIELDS;
				}
				else
				{
					Info->bmiHeader.biCompression = BI_RGB;
				}

				Info->bmiHeader.biSizeImage = MapSize;
				Info->bmiHeader.biXPelsPerMeter = 0;
				Info->bmiHeader.biYPelsPerMeter = 0;
				Info->bmiHeader.biClrUsed = 0;
				Info->bmiHeader.biClrImportant = 0;

				if (pDC->GetBits() <= 8)
				{
					// Palette
					GPalette *Pal = pDC->Palette();
					RGBQUAD *Rgb = Info->bmiColors;
					if (Pal)
					{
						GdcRGB *p = (*Pal)[0];
						if (p)
						{
							for (int i=0; i<Colours; i++, p++, Rgb++)
							{
								Rgb->rgbRed = p->r;
								Rgb->rgbGreen = p->g;
								Rgb->rgbBlue = p->b;
								Rgb->rgbReserved = p->a;
							}
						}
					}
					else
					{
						memset(Rgb, 0, Colours * sizeof(RGBQUAD));
					}
				}
				else
				{
					int *Primaries = (int*) Info->bmiColors;
					
					switch (pDC->GetBits())
					{
						case 16:
						{
							Primaries[0] = Rgb16(255, 0, 0);
							Primaries[1] = Rgb16(0, 255, 0);
							Primaries[2] = Rgb16(0, 0, 255);
							break;
						}
						case 32:
						{
							Primaries[0] = Rgba32(255, 0, 0, 0);
							Primaries[1] = Rgba32(0, 255, 0, 0);
							Primaries[2] = Rgba32(0, 0, 255, 0);
							break;
						}
					}
				}

				// Bits
				uchar *Dest = ((uchar*)Info) + HeaderSize + PaletteSize;
				for (int y=pDC->Y()-1; y>=0; y--)
				{
					uchar *s = (*pDC)[y];
					if (s)
					{
						memcpy(Dest, s, Bytes);
					}
					Dest += Bytes;
				}

				#if 0
				GFile f;
				if (f.Open("c:\\tmp\\out.bmp", O_WRITE))
				{
					f.SetSize(0);
					f.Write(Info, TotalSize);
					f.Close();
				}
				#endif

				Status = SetClipboardData(CF_DIB, hMem) != 0;
			}
			else
			{
				GlobalFree(hMem);
			}
		}
	}
	return Status;
}

GSurface *GClipBoard::ConvertFromPtr(void *Ptr)
{
	GSurface *pDC = NULL;
	if (Ptr)
	{
		BITMAPINFO *Info = (BITMAPINFO*) Ptr;
		if (Info)
		{
			pDC = new GMemDC;
			if (pDC &&
				(Info->bmiHeader.biCompression == BI_RGB ||
				Info->bmiHeader.biCompression == BI_BITFIELDS))
			{
				if (pDC->Create(Info->bmiHeader.biWidth,
						Info->bmiHeader.biHeight,
						GBitsToColourSpace(max(Info->bmiHeader.biPlanes * Info->bmiHeader.biBitCount, 8))))
				{
					int Colours = 0;
					char *Source = (char*) Info->bmiColors;

					// do palette
					if (pDC->GetBits() <= 8)
					{
						if (Info->bmiHeader.biClrUsed > 0)
						{
							Colours = Info->bmiHeader.biClrUsed;
						}
						else
						{
							Colours = 1 << pDC->GetBits();
						}

						GPalette *Pal = new GPalette(NULL, Colours);
						if (Pal)
						{
							GdcRGB *d = (*Pal)[0];
							RGBQUAD *s = (RGBQUAD*) Source;
							if (d)
							{
								for (int i=0; i<Colours; i++, d++, s++)
								{
									d->r = s->rgbRed;
									d->g = s->rgbGreen;
									d->b = s->rgbBlue;
									d->a = s->rgbReserved;
								}
							}
							Source = (char*) s;
							pDC->Palette(Pal);
						}
					}

					if (Info->bmiHeader.biCompression == BI_BITFIELDS)
					{
						Source += sizeof(DWORD) * 3;
					}

					// do pixels
					int Bytes = BMPWIDTH(pDC->X() * pDC->GetBits());
					for (int y=pDC->Y()-1; y>=0; y--)
					{
						uchar *d = (*pDC)[y];
						if (d)
						{
							memcpy(d, Source, Bytes);
						}
						Source += Bytes;
					}
				}
			}
		}
	}
	return pDC;
}

GSurface *GClipBoard::Bitmap()
{
	HGLOBAL hMem = GetClipboardData(CF_DIB);
	void *Ptr = GlobalLock(hMem);
	GSurface *pDC = 0;
	if (Ptr)
	{
		#if 0
		SIZE_T TotalSize = GlobalSize(hMem);
		GFile f;
		if (f.Open("c:\\tmp\\in.bmp", O_WRITE))
		{
			f.SetSize(0);
			f.Write(Ptr, TotalSize);
			f.Close();
		}
		#endif

		pDC = ConvertFromPtr(Ptr);
		GlobalUnlock(hMem);
	}
	return pDC;
}

GString::Array GClipBoard::Files()
{
	GString::Array f;

	LgiAssert(!"Impl me");

	return f;
}

bool GClipBoard::Files(GString::Array &Paths, bool AutoEmpty)
{
	if (Open)
	{
		CloseClipboard();
		Open = FALSE;
	}
	// CFSTR_FILEDESCRIPTOR
	HRESULT r = OleSetClipboard(new LFileData(Paths));
	return SUCCEEDED(r);
}

bool GClipBoard::Binary(FormatType Format, uchar *Ptr, ssize_t Len, bool AutoEmpty)
{
	bool Status = FALSE;

	if (AutoEmpty)
	{
		Empty();
	}

	if (Ptr && Len > 0)
	{
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, Len);
		if (hMem)
		{
			char *Data = (char*) GlobalLock(hMem);
			if (Data)
			{
				memcpy(Data, Ptr, Len);
				GlobalUnlock(hMem);
				Status = SetClipboardData(Format, hMem) != 0;
				if (!Status)
					LgiTrace("%s:%i - SetClipboardData failed.\n", _FL);
			}
			else
			{
				GlobalFree(hMem);
			}
		}
		else LgiTrace("%s:%i - Alloc failed.\n", _FL);
	}
	else LgiTrace("%s:%i - No data to set.\n", _FL);

	return Status;
}

bool GClipBoard::Binary(FormatType Format, GAutoPtr<uint8> &Ptr, ssize_t *Length)
{
	bool Status = false;

	HGLOBAL hMem = GetClipboardData(Format);
	if (hMem)
	{
		auto Len = GlobalSize(hMem);
		if (Length)
			*Length = Len;
			
		uchar *Data = (uchar*) GlobalLock(hMem);
		if (Data)
		{
			if (Ptr.Reset(new uchar[Len+sizeof(char16)]))
			{
				memcpy(Ptr, Data, Len);
				
				// String termination
				memset(Ptr + Len, 0, sizeof(char16));
				
				Status = true;
			}
		}

		GlobalUnlock(hMem);
	}

	return Status;
}

