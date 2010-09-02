/*hdr
**      FILE:           Tiff.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           9/10/98
**      DESCRIPTION:    Tiff filter
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#ifdef WIN32
#include <conio.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "Lgi.h"
#include "Lzw.h"
#include "GVariant.h"
#include "GLibraryUtils.h"

namespace t {
#include "tiffio.h"
}

// Libtiff support
class LibTiff : public GLibrary
{
public:
	LibTiff() : GLibrary("libtiff") {}

	DynFunc2(t::TIFF*, TIFFOpen, const char*, a, const char*, b);
	DynFunc10(t::TIFF*, TIFFClientOpen,
			const char*, a,
			const char*, b,
			t::thandle_t, c,
			t::TIFFReadWriteProc, d,
			t::TIFFReadWriteProc, e,
			t::TIFFSeekProc, f,
			t::TIFFCloseProc, g,
			t::TIFFSizeProc, h,
			t::TIFFMapFileProc, i,
			t::TIFFUnmapFileProc, j);
	DynFunc1(int, TIFFClose, t::TIFF*, t);
	DynFunc1(t::TIFFErrorHandler, TIFFSetErrorHandler, t::TIFFErrorHandler, e);
	DynFunc4(int, TIFFRGBAImageBegin, t::TIFFRGBAImage*, a, t::TIFF*, b, int, c, char*, d);
	DynFunc1(int, TIFFRGBAImageEnd, t::TIFFRGBAImage*, a);
	DynFunc4(int, TIFFRGBAImageGet, t::TIFFRGBAImage*, img, uint32*, ptr, uint32, x, uint32, y);
	DynFunc4(int, TIFFWriteScanline, t::TIFF*, tif, t::tdata_t, ptr, uint32, a, t::tsample_t, b);
	
	int TIFFGetField(t::TIFF *tif, t::ttag_t tag, ...)
	{
		va_list Args;
		va_start(Args, tag);
		typedef int (*pTIFFVGetField)(t::TIFF*, t::ttag_t, va_list);
		pTIFFVGetField TIFFVGetField = (pTIFFVGetField) GetAddress("TIFFVGetField");
		if (TIFFVGetField)
			return TIFFVGetField(tif, tag, Args);
		return 0;
	}

	int TIFFSetField(t::TIFF *tif, t::ttag_t tag, ...)
	{
		va_list Args;
		va_start(Args, tag);
		typedef int (*pTIFFVSetField)(t::TIFF*, t::ttag_t, va_list);
		pTIFFVSetField TIFFVSetField = (pTIFFVSetField) GetAddress("TIFFVSetField");
		if (TIFFVSetField)
			return TIFFVSetField(tif, tag, Args);
		return 0;
	}

};

class GdcLibTiff : public GFilter
{
	GAutoPtr<LibTiff> Lib;

public:
	GdcLibTiff();
	~GdcLibTiff();

	int GetCapabilites() { return FILTER_CAP_READ|FILTER_CAP_WRITE; }
	Format GetFormat() { return FmtTiff; }
	bool IsOk() { return Lib ? Lib->IsLoaded() : false; }

	bool ReadImage(GSurface *pDC, GStream *In);
	bool WriteImage(GStream *Out, GSurface *pDC);

	bool GetVariant(char *n, GVariant &v, char *a)
	{
		if (!stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Tiff";
		}
		else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "TIF,TIFF";
		}
		else return false;

		return true;
	}
};

// Object Factory
class GdcTiffFactory : public GFilterFactory
{
	bool CheckFile(char *File, int Access, uchar *Hint)
	{
		if (Access == FILTER_CAP_READ ||
			Access == FILTER_CAP_WRITE)
		{
			return (File) ? stristr(File, ".tiff") != 0 OR
							stristr(File, ".tif") != 0 : false;
		}

		return false;
	}

	GFilter *NewObject()
	{
		return new GdcLibTiff;
	}

} TiffFactory;

static t::tsize_t TRead(t::thandle_t Hnd, t::tdata_t Ptr, t::tsize_t Size)
{
	GStream *s = (GStream*)Hnd;
	int r = s->Read(Ptr, Size);
	return r;
}

static t::tsize_t TWrite(t::thandle_t Hnd, t::tdata_t Ptr, t::tsize_t Size)
{
	GStream *s = (GStream*)Hnd;
	int w = s->Write(Ptr, Size);
	return w; 
}

static t::toff_t TSeek(t::thandle_t Hnd, t::toff_t Where, int Whence)
{
	GStream *s = (GStream*)Hnd;

	switch (Whence)
	{
		case SEEK_SET:
			s->SetPos(Where);
			break;
		case SEEK_CUR:
			s->SetPos(s->GetPos() + Where);
			break;
		case SEEK_END:
			s->SetPos(s->GetSize() + Where);
			break;
	}

	return s->GetPos();
}

static int TClose(t::thandle_t Hnd)
{
	GStream *s = (GStream*)Hnd;
	return !s->Close();
}

static t::toff_t TSize(t::thandle_t Hnd)
{
	GStream *s = (GStream*)Hnd;
	t::toff_t size =  s->GetSize();
	return size;
}

static char PrevError[1024] = "(none)";
static void TError(const char *Func, const char *Fmt, va_list Args)
{
	vsprintf(PrevError, Fmt, Args);
}

GdcLibTiff::GdcLibTiff()
{
	Lib.Reset(new LibTiff);
	if (IsOk())
	{
		Lib->TIFFSetErrorHandler(TError);
	}
}

GdcLibTiff::~GdcLibTiff()
{
}

bool GdcLibTiff::ReadImage(GSurface *pDC, GStream *In)
{
	GVariant v;
	if (!pDC || !In)
	{
		Props->SetValue(LGI_FILTER_ERROR, v = "Parameter error.");
		return false;
	}

	if (!IsOk())
	{
		Props->SetValue(LGI_FILTER_ERROR, v = "Can't find libtiff library.");
		return false;
	}

	bool Status = false;
	t::TIFF *tif = Lib->TIFFClientOpen("", "rm", In, TRead, TWrite, TSeek, TClose, TSize, 0, 0);
	if (tif)
	{
		// int Width, Height;
		// Lib->TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &Width);
        // Lib->TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &Height);

		t::TIFFRGBAImage img;
		if (Lib->TIFFRGBAImageBegin(&img, tif, 0, PrevError))
		{
			int Bits = img.bitspersample * img.samplesperpixel;
			if (pDC->Create(img.width, img.height, 32))
			{
				uint8 *d = (*pDC)[0];
				Lib->TIFFRGBAImageGet(&img, (uint32*)d, pDC->X(), pDC->Y());
				Status = true;
			}

			Lib->TIFFRGBAImageEnd(&img);
		}
		else
		{
			Props->SetValue(LGI_FILTER_ERROR, v = PrevError);
		}

		Lib->TIFFClose(tif);
	}
	else
	{
		Props->SetValue(LGI_FILTER_ERROR, v = PrevError);
	}

	return Status;
}

bool GdcLibTiff::WriteImage(GStream *Out, GSurface *pDC)
{
	GVariant v;
	if (!pDC || !Out)
	{
		Props->SetValue(LGI_FILTER_ERROR, v = "Parameter error.");
		return false;
	}

	if (!IsOk())
	{
		Props->SetValue(LGI_FILTER_ERROR, v = "Can't find libtiff library.");
		return false;
	}

	bool Status = false;
	t::TIFF *tif = Lib->TIFFClientOpen("", "wm", Out, TRead, TWrite, TSeek, TClose, TSize, 0, 0);
	if (tif)
	{
		Lib->TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, pDC->X());
		Lib->TIFFSetField(tif, TIFFTAG_IMAGELENGTH, pDC->Y());
		Lib->TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, pDC->GetBits() / 8);
		Lib->TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
		Lib->TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		Lib->TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		Lib->TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

		Status = true;
		for (int y=0; y<pDC->Y(); y++)
		{
			uint8 *Scan = (*pDC)[y];
			if (Lib->TIFFWriteScanline(tif, Scan, y, 0) != 1)
			{
				Status = false;
				break;
			}
		}

		Lib->TIFFClose(tif);
	}
	else
	{
		Props->SetValue(LGI_FILTER_ERROR, v = PrevError);
	}

	return Status;
}
