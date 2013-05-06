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
	DynFunc4(int, TIFFReadScanline, t::TIFF*, tif, t::tdata_t, ptr, uint32, a, t::tsample_t, b);
	DynFunc1(int, TIFFScanlineSize, t::TIFF*, t);
	
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

	IoStatus ReadImage(GSurface *pDC, GStream *In);
	IoStatus WriteImage(GStream *Out, GSurface *pDC);

	bool GetVariant(const char *n, GVariant &v, char *a)
	{
		if (!_stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Tiff";
		}
		else if (!_stricmp(n, LGI_FILTER_EXTENSIONS))
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
			return (File) ? stristr(File, ".tiff") != 0 ||
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

	return (t::toff_t) s->GetPos();
}

static int TClose(t::thandle_t Hnd)
{
	GStream *s = (GStream*)Hnd;
	return !s->Close();
}

static t::toff_t TSize(t::thandle_t Hnd)
{
	GStream *s = (GStream*)Hnd;
	t::toff_t size = (t::toff_t) s->GetSize();
	return size;
}

static char PrevError[1024] = "(none)";
static void TError(const char *Func, const char *Fmt, va_list Args)
{
	vsprintf_s(PrevError, Fmt, Args);
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

/// This swaps the R and B components as well as flips the image on the Y
/// axis. Which is the required format to read and write TIFF's. Use it a
/// 2nd time to restore the image to it's original state.
void SwapRBandY(GSurface *pDC)
{
	for (int y1=0, y2 = pDC->Y()-1; y1<=y2; y1++, y2--)
	{
		switch (pDC->GetColourSpace())
		{
			case System24BitColourSpace:
			{
				System24BitPixel *s1 = (System24BitPixel*)(*pDC)[y1];
				System24BitPixel *s2 = (System24BitPixel*)(*pDC)[y2];
				System24BitPixel *e1 = (System24BitPixel*) ((*pDC)[y1] + (pDC->X() * sizeof(System24BitPixel)));

				if (y1 == y2)
				{
					uint8 t;
					while (s1 < e1)
					{
						t = s1->r;
						s1->r = s1->b;
						s1->b = t;
						s1++;
					}
				}
				else
				{
					System24BitPixel t;
					while (s1 < e1)
					{
						t = *s1;

						s1->r = s2->b;
						s1->g = s2->g;
						s1->b = s2->r;

						s2->r = t.b;
						s2->g = t.g;
						s2->b = t.r;

						s1++;
						s2++;
					}
				}
				break;
			}
			case System32BitColourSpace:
			{
				System32BitPixel *s1 = (System32BitPixel*)(*pDC)[y1];
				System32BitPixel *s2 = (System32BitPixel*)(*pDC)[y2];
				System32BitPixel *e1 = s1 + pDC->X();

				if (y1 == y2)
				{
					uint8 t;
					while (s1 < e1)
					{
						t = s1->r;
						s1->r = s1->b;
						s1->b = t;
						s1++;
					}
				}
				else
				{
					System32BitPixel t;
					while (s1 < e1)
					{
						t = *s1;

						s1->r = s2->b;
						s1->g = s2->g;
						s1->b = s2->r;
						s1->a = s2->a;

						s2->r = t.b;
						s2->g = t.g;
						s2->b = t.r;
						s2->a = t.a;

						s1++;
						s2++;
					}
				}
				break;
			}
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
		}
	}
}

void SwapRB(GSurface *pDC)
{
	for (int y1=0; y1<pDC->Y(); y1++)
	{
		switch (pDC->GetColourSpace())
		{
			case System24BitColourSpace:
			{
				System24BitPixel *s1 = (System24BitPixel*)(*pDC)[y1];
				System24BitPixel *e1 = (System24BitPixel*) ((*pDC)[y1] + (pDC->X() * sizeof(System24BitPixel)));

				uint8 t;
				while (s1 < e1)
				{
					t = s1->r;
					s1->r = s1->b;
					s1->b = t;
					s1++;
				}
				break;
			}
			case System32BitColourSpace:
			{
				System32BitPixel *s1 = (System32BitPixel*)(*pDC)[y1];
				System32BitPixel *e1 = s1 + pDC->X();

				uint8 t;
				while (s1 < e1)
				{
					t = s1->r;
					s1->r = s1->b;
					s1->b = t;
					s1++;
				}
				break;
			}
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
		}
	}
}

struct Cmyka
{
	uint8 c, m, y, k, a;
};

template<typename D, typename S>
void TiffProcess24(D *d, S *s, int width)
{
	D *e = d + width;
	while (d < e)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		d++;
		s++;
	}
}

template<typename D, typename S>
void TiffProcess24To32(D *d, S *s, int width)
{
	D *e = d + width;
	while (d < e)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		d->a = 255;
		d++;
		s++;
	}
}

template<typename D, typename S>
void TiffProcess32(D *d, S *s, int width)
{
	D *e = d + width;
	while (d < e)
	{
		d->r = s->r;
		d->g = s->g;
		d->b = s->b;
		d->a = s->a;
		d++;
		s++;
	}
}

GFilter::IoStatus GdcLibTiff::ReadImage(GSurface *pDC, GStream *In)
{
	GVariant v;
	if (!pDC || !In)
	{
		Props->SetValue(LGI_FILTER_ERROR, v = "Parameter error.");
		return IoError;
	}

	if (!IsOk())
	{
		Props->SetValue(LGI_FILTER_ERROR, v = "Can't find libtiff library.");
		return IoComponentMissing;
	}

	IoStatus Status = IoError;
	t::TIFF *tif = Lib->TIFFClientOpen("", "rm", In, TRead, TWrite, TSeek, TClose, TSize, 0, 0);
	if (tif)
	{
		int Photometric = 0;
		int Inkset = 0;
		int Samples = 0;
		int BitsPerSample = 0;
		int Planar = 0;
		Lib->TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &Planar);
		Lib->TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &BitsPerSample);
		Lib->TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &Samples);
		Lib->TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &Photometric);
		Lib->TIFFGetField(tif, TIFFTAG_INKSET, &Inkset);

		t::TIFFRGBAImage img;
		if (Lib->TIFFRGBAImageBegin(&img, tif, 0, PrevError))
		{
			int Bits = img.bitspersample * img.samplesperpixel;
			
			if (img.samplesperpixel == 5)
			{
				int rowlen = img.width * img.samplesperpixel * img.bitspersample / 8;
				GArray<uint8> a;
				if (a.Length(rowlen) &&
					pDC->Create(img.width, img.height, 32))
				{
				    if (Meter)
				        Meter->SetLimits(0, img.height);
				    
					for (unsigned y=0; y<img.height; y++)
					{
						int r = Lib->TIFFReadScanline(tif, &a[0], y, 0);
						if (r != 1)
							break;

						System32BitPixel *d = (System32BitPixel*) (*pDC)[y];
						System32BitPixel *e = d + img.width;
						Cmyka *s = (Cmyka*) &a[0];
						while (d < e)
						{
							double C = (double) s->c / 255;
							double M = (double) s->m / 255;
							double Y = (double) s->y / 255;
							double K = (double) s->k / 255;

							d->r = (int) ((1.0 - min(1, C * (1.0 - K) + K)) * 255);
							d->g = (int) ((1.0 - min(1, M * (1.0 - K) + K)) * 255);
							d->b = (int) ((1.0 - min(1, Y * (1.0 - K) + K)) * 255);
							d->a = s->a;

							d++;
							s++;
						}

				        if (Meter)
				            Meter->Value(y);
					}

					Status = IoSuccess;
				}
			}
			else
			{
	            if (pDC->Create(img.width, img.height, Bits))
	            {
	                if (Meter)
	                    Meter->SetLimits(0, img.height);

					if (Bits == 24)
					{
						GArray<GRgb24> Buf;
						Buf.Length(img.width);
						GRgb24 *b = &Buf[0];
						LgiAssert(Lib->TIFFScanlineSize(tif) == Buf.Length() * sizeof(Buf[0]));

						for (unsigned y=0; y<img.height; y++)
						{
							uint8 *d = (*pDC)[y];
							Lib->TIFFReadScanline(tif, (t::tdata_t)b, y, 0);
				            
							switch (pDC->GetColourSpace())
							{
								#define TiffCase(name, bits) \
									case Cs##name: TiffProcess##bits((G##name*)d, b, pDC->X()); break
								
								TiffCase(Rgb24, 24);
								TiffCase(Bgr24, 24);
								TiffCase(Xrgb32, 24);
								TiffCase(Rgbx32, 24);
								TiffCase(Xbgr32, 24);
								TiffCase(Bgrx32, 24);

								TiffCase(Rgba32, 24To32);
								TiffCase(Bgra32, 24To32);
								TiffCase(Argb32, 24To32);
								TiffCase(Abgr32, 24To32);
							}

							if (Meter && (y % 32) == 0)
								Meter->Value(y);
						}
					}
					else if (Bits == 32)
					{
						GArray<GAbgr32> Buf;
						Buf.Length(img.width);
						GAbgr32 *b = &Buf[0];
						LgiAssert(Lib->TIFFScanlineSize(tif) == Buf.Length() * sizeof(Buf[0]));

						for (unsigned y=0; y<img.height; y++)
						{
							uint8 *d = (*pDC)[y];
							Lib->TIFFReadScanline(tif, (t::tdata_t)b, y, 0);
				            
							switch (pDC->GetColourSpace())
							{
								#define TiffCase(name, bits) \
									case Cs##name: TiffProcess##bits((G##name*)d, b, pDC->X()); break
								
								TiffCase(Rgb24, 24);
								TiffCase(Bgr24, 24);
								TiffCase(Xrgb32, 24);
								TiffCase(Rgbx32, 24);
								TiffCase(Xbgr32, 24);
								TiffCase(Bgrx32, 24);

								TiffCase(Rgba32, 32);
								TiffCase(Bgra32, 32);
								TiffCase(Argb32, 32);
								TiffCase(Abgr32, 32);
							}

							if (Meter && (y % 32) == 0)
								Meter->Value(y);
						}
					}
			            
		            Status = IoSuccess;
		        }
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

GFilter::IoStatus GdcLibTiff::WriteImage(GStream *Out, GSurface *pDC)
{
	GVariant v;
	if (!pDC || !Out)
	{
		Props->SetValue(LGI_FILTER_ERROR, v = "Parameter error.");
		return IoError;
	}

	if (!IsOk())
	{
		Props->SetValue(LGI_FILTER_ERROR, v = "Can't find libtiff library.");
		return IoUnsupportedFormat;
	}

	IoStatus Status = IoError;
	t::TIFF *tif = Lib->TIFFClientOpen("", "wm", Out, TRead, TWrite, TSeek, TClose, TSize, 0, 0);
	if (tif)
	{
	    int bits = pDC->GetBits();
		
		Lib->TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, pDC->X());
		Lib->TIFFSetField(tif, TIFFTAG_IMAGELENGTH, pDC->Y());
		Lib->TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, bits / 8);
		Lib->TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
		Lib->TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		Lib->TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		Lib->TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

		Status = IoSuccess;
		SwapRB(pDC);

        if (Meter)
            Meter->SetLimits(0, pDC->Y());

		for (int y=0; y<pDC->Y(); y++)
		{
			uint8 *Scan = (*pDC)[y];
			if (Lib->TIFFWriteScanline(tif, Scan, y, 0) != 1)
			{
				Status = IoError;
				break;
			}

            if (Meter && (y % 32) == 0)
                Meter->Value(y);
		}

		Lib->TIFFClose(tif);
		SwapRB(pDC);
	}
	else
	{
		Props->SetValue(LGI_FILTER_ERROR, v = PrevError);
	}

	return Status;
}
