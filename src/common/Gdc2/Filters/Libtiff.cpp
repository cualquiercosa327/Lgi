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
	return s->Read(Ptr, Size);
}

static t::tsize_t TWrite(t::thandle_t Hnd, t::tdata_t Ptr, t::tsize_t Size)
{
	GStream *s = (GStream*)Hnd;
	return s->Write(Ptr, Size);
}

static t::toff_t TSeek(t::thandle_t Hnd, t::toff_t Where, int Whence)
{
	GStream *s = (GStream*)Hnd;

	switch (Whence)
	{
		case SEEK_SET:
			s->SetPos(Where);
			return 0;
		case SEEK_CUR:
			s->SetPos(s->GetPos() + Where);
			return 0;
		case SEEK_END:
			s->SetPos(s->GetSize() + Where);
			return 0;
	}

	return -1;
}

static int TClose(t::thandle_t Hnd)
{
	GStream *s = (GStream*)Hnd;
	return !s->Close();
}

static t::toff_t TSize(t::thandle_t Hnd)
{
	GStream *s = (GStream*)Hnd;
	return s->GetSize();
}

GdcLibTiff::GdcLibTiff()
{
	Lib.Reset(new LibTiff);
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

	t::TIFF *tif = Lib->TIFFClientOpen("", "m", In, TRead, TWrite, TSeek, TClose, TSize, 0, 0);
	if (tif)
	{
		Lib->TIFFClose(tif);
	}
	return false;
}

bool GdcLibTiff::WriteImage(GStream *Out, GSurface *pDC)
{
	return false;
}
