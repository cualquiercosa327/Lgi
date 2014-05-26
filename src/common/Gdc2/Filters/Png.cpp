/*hdr
**      FILE:           Png.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Png file filter
**
**      Copyright (C) 2002, Matthew Allen
**              fret@memecode.com
*/

//
// 'png.h' comes from libpng, which you can get from:
// http://www.libpng.org/pub/png/libpng.html
//
// You will also need zlib, which pnglib requires. zlib
// is available here:
// http://www.gzip.org/zlib 
//
// If you don't want to build with PNG support then set
// the define HAS_LIBPNG_ZLIB to '0' in Lgi.h
//

#ifndef __CYGWIN__ 
#include "math.h"
#include "png.h"
#endif

#include "Lgi.h"
#include "GPalette.h"

#ifdef __CYGWIN__ 
#include "png.h"
#endif

#if HAS_LIBPNG_ZLIB

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "GLibraryUtils.h"
#ifdef FILTER_UI
#include "GTransparentDlg.h"
#endif
#include "GVariant.h"

// Pixel formats
typedef uint8 Png8;
typedef GRgb24 Png24;
typedef GRgba32 Png32;

struct Png48
{
	uint16 r, g, b;
};

struct Png64
{
	uint16 r, g, b, a;
};

const char sLibrary[] =
	#if defined(MAC) || defined(LINUX)
		"libpng"
	#else
		#if defined(__CYGWIN__)
			"cygpng12"
		#else
			"libpng9"
			#if defined(WIN64)
			"x64"
			#else
			"x32"
			#endif
		#endif
	#endif
	;

// Library interface
class LibPng : public GLibrary
{
public:
	LibPng() : GLibrary(sLibrary)
	{
		if (!IsLoaded())
		{
			#if defined(MAC) || defined(LINUX)
			if (!Load("/opt/local/lib/libpng.dylib"))
			#endif
			{
				static bool First = true;
				if (First)
				{
					LgiTrace("%s:%i - Failed to load libpng.\n", _FL);
					First = false;
				}
			}
		}
		else
		{
			#if 0
			char File[256];
			GetModuleFileName(Handle(), File, sizeof(File));
			LgiTrace("%s:%i - PNG: %s\n", __FILE__, __LINE__, File);
			#endif
		}
	}

	DynFunc4(	png_structp,
				png_create_read_struct,
				png_const_charp, user_png_ver,
				png_voidp, error_ptr,
				png_error_ptr, error_fn,
				png_error_ptr, warn_fn);

	DynFunc4(	png_structp,
				png_create_write_struct,
				png_const_charp, user_png_ver,
				png_voidp, error_ptr,
				png_error_ptr, error_fn,
				png_error_ptr, warn_fn);

	DynFunc1(	png_infop,
				png_create_info_struct,
				png_structp, png_ptr);

	DynFunc2(	int,
				png_destroy_info_struct,
				png_structp, png_ptr,
				png_infopp, info_ptr);

	DynFunc3(	int,
				png_destroy_read_struct,
				png_structpp, png_ptr_ptr,
				png_infopp, info_ptr_ptr,
				png_infopp, end_info_ptr_ptr);
	
	DynFunc2(	int,
				png_destroy_write_struct,
				png_structpp, png_ptr_ptr,
				png_infopp, info_ptr_ptr);

	DynFunc3(	int,
				png_set_read_fn,
				png_structp, png_ptr,
				png_voidp, io_ptr,
				png_rw_ptr, read_data_fn);

	DynFunc4(	int,
				png_set_write_fn,
				png_structp, png_ptr,
				png_voidp, io_ptr,
				png_rw_ptr, write_data_fn,
				png_flush_ptr, output_flush_fn);

	DynFunc4(	int,
				png_read_png,
				png_structp, png_ptr,
				png_infop, info_ptr,
				int, transforms,
				png_voidp, params);

	DynFunc4(	int,
				png_write_png,
				png_structp, png_ptr,
                png_infop, info_ptr,
                int, transforms,
                png_voidp, params);

	DynFunc2(	png_bytepp,
				png_get_rows,
				png_structp, png_ptr,
				png_infop, info_ptr);

	DynFunc3(	int,
				png_set_rows,
				png_structp, png_ptr,
				png_infop, info_ptr,
				png_bytepp, row_pointers);

	DynFunc6(	png_uint_32,
				png_get_iCCP,
				png_structp, png_ptr,
				png_infop, info_ptr,
				png_charpp, name,
				int*, compression_type,
				png_charpp, profile,
				png_uint_32*, proflen);

	DynFunc6(	int,
				png_set_iCCP,
				png_structp, png_ptr,
				png_infop, info_ptr,
				png_charp, name,
				int, compression_type,
				png_charp, profile,
				png_uint_32, proflen);

    DynFunc5(   png_uint_32,
                png_get_tRNS,
                png_structp, png_ptr,
                png_infop, info_ptr,
                png_bytep*, trans_alpha,
                int*, num_trans,
                png_color_16p*, trans_color);

    DynFunc3(   png_uint_32,
                png_get_valid,
                png_structp, png_ptr,
                png_infop, info_ptr,
                png_uint_32, flag);


    DynFunc4(   png_uint_32,
                png_get_PLTE,
                png_structp, png_ptr,
                png_infop, info_ptr,
                png_colorp*, palette,
                int*, num_palette);

    DynFunc2(   png_uint_32,
                png_get_image_width,
                png_structp, png_ptr,
                png_infop, info_ptr);

    DynFunc2(   png_uint_32,
                png_get_image_height,
                png_structp, png_ptr,
                png_infop, info_ptr);

    DynFunc2(   png_byte,
                png_get_channels,
                png_structp, png_ptr,
                png_infop, info_ptr);

	DynFunc2(   png_byte,
				png_get_color_type,
				png_const_structp, png_ptr,
				png_const_infop, info_ptr);

    DynFunc2(   png_byte,
                png_get_bit_depth,
                png_structp, png_ptr,
                png_infop, info_ptr);

    DynFunc1(   png_voidp,
                png_get_error_ptr,
                png_structp, png_ptr);

    DynFunc1(   png_voidp,
                png_get_io_ptr,
                png_structp, png_ptr);

    DynFunc9(   int,
                png_set_IHDR,
                png_structp, png_ptr,
                png_infop, info_ptr,
                png_uint_32, width, png_uint_32, height,
                int, bit_depth, int, color_type,
                int, interlace_method, int, compression_method,
                int, filter_method);

    DynFunc4(   int,
                png_set_PLTE,
                png_structp, png_ptr, png_infop, info_ptr,
                png_colorp, palette, int, num_palette);

    DynFunc5(   int,
                png_set_tRNS,
                png_structp, png_ptr,
				png_infop, info_ptr, png_bytep, trans_alpha, int, num_trans,
				png_color_16p, trans_color);
				
	/*
	DynFunc2(   png_byte,
				png_get_interlace_type,
				png_const_structp, png_ptr,
				png_const_infop, info_ptr);
	*/
};

static GAutoPtr<LibPng> CurrentLibPng;

class GdcPng : public GFilter
{
	static char PngSig[];
	friend void PNGAPI LibPngError(png_structp Png, png_const_charp Msg);
	friend void PNGAPI LibPngWarning(png_structp Png, png_const_charp Msg);

	LibPng *Lib;
	int Pos;
	uchar *PrevScanLine;
	GSurface *pDC;
	GBytePipe DataPipe;

	GView *Parent;
	jmp_buf Here;

public:
	GdcPng(LibPng *lib);
	~GdcPng();

    const char *GetComponentName() { return "libpng"; }
	Format GetFormat() { return FmtPng; }
	void SetMeter(int i) { if (Meter) Meter->Value(i); }
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	IoStatus ReadImage(GSurface *pDC, GStream *In);
	IoStatus WriteImage(GStream *Out, GSurface *pDC);

	bool GetVariant(const char *n, GVariant &v, char *a)
	{
		if (!_stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Png"; // Portable Network Graphic
		}
		else if (!_stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "PNG";
		}
		else return false;

		return true;
	}
};

// Object Factory
class GdcPngFactory : public GFilterFactory
{
	bool CheckFile(char *File, int Access, uchar *Hint)
	{
		if (Hint)
		{
			if (Hint[1] == 'P' &&
				Hint[2] == 'N' &&
				Hint[3] == 'G')
				return true;
		}

		return (File) ? stristr(File, ".png") != 0 : false;
	}

	GFilter *NewObject()
	{
		if (!CurrentLibPng)
			CurrentLibPng.Reset(new LibPng);
		return new GdcPng(CurrentLibPng);
	}
	
} PngFactory;

// Class impl
char GdcPng::PngSig[] = { (char)137, 'P', 'N', 'G', '\r', '\n', (char)26, '\n', 0 };

GdcPng::GdcPng(LibPng *lib)
{
	Lib = lib;
	Parent = 0;
	Pos = 0;
	PrevScanLine = 0;
}

GdcPng::~GdcPng()
{
	DeleteArray(PrevScanLine);
}

void PNGAPI LibPngError(png_structp Png, png_const_charp Msg)
{
	GdcPng *This = (GdcPng*)CurrentLibPng->png_get_error_ptr(Png);
	if (This)
	{
		printf("Libpng Error Message='%s'\n", Msg);

		if (This->Props)
		{
			GVariant v;
			This->Props->SetValue(LGI_FILTER_ERROR, v = (char*)Msg);
		}

		longjmp(This->Here, -1);
	}
}

void PNGAPI LibPngWarning(png_structp Png, png_const_charp Msg)
{
	// GdcPng *This = (GdcPng*)Png->error_ptr;
	// LgiMsg(This->Parent, (char*)Msg, "LibPng Warning", MB_OK);
}

void PNGAPI LibPngRead(png_structp Png, png_bytep Ptr, png_size_t Size)
{
	GStream *s = (GStream*)CurrentLibPng->png_get_io_ptr(Png);
	if (s)
	{
		s->Read(Ptr, Size);
	}
	else
	{
		LgiTrace("%s:%i - No this ptr? (%p)\n", __FILE__, __LINE__, Ptr);
		LgiAssert(0);
	}
}

struct PngWriteInfo
{
	GStream *s;
	Progress *m;
};

void PNGAPI LibPngWrite(png_structp Png, png_bytep Ptr, png_size_t Size)
{
	PngWriteInfo *i = (PngWriteInfo*)CurrentLibPng->png_get_io_ptr(Png);
	if (i)
	{
		i->s->Write(Ptr, Size);
		/*
		if (i->m)
			i->m->Value(Png->flush_rows);
		*/
	}
	else
	{
		LgiTrace("%s:%i - No this ptr?\n", __FILE__, __LINE__);
		LgiAssert(0);
	}
}

template<typename Out, typename In>
void Read32_16(Out *o, In *i, int Len)
{
	In *e = i + Len;

	while (i < e)
	{
		o->r = i->r >> 3;
		o->g = i->g >> 2;
		o->b = i->b >> 3;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void Read64_16(Out *o, In *i, int Len)
{
	In *e = i + Len;
	while (i < e)
	{
		o->r = i->r >> 11;
		o->g = i->g >> 10;
		o->b = i->b >> 11;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void Read32_24(Out *o, In *i, int Len)
{
	In *e = i + Len;

	while (i < e)
	{
		o->r = i->r;
		o->g = i->g;
		o->b = i->b;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void Read64_24(Out *o, In *i, int Len)
{
	In *e = i + Len;
	while (i < e)
	{
		o->r = i->r >> 8;
		o->g = i->g >> 8;
		o->b = i->b >> 8;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void Read32_32(Out *o, In *i, int Len)
{
	In *e = i + Len;

	while (i < e)
	{
		o->r = i->r;
		o->g = i->g;
		o->b = i->b;
		o->a = 255;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void Read64_32(Out *o, In *i, int Len)
{
	In *e = i + Len;

	while (i < e)
	{
		o->r = i->r >> 8;
		o->g = i->g >> 8;
		o->b = i->b >> 8;
		o->a = 255;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void ReadAlpha32_16(Out *o, In *i, int Len)
{
	In *e = i + Len;

	while (i < e)
	{
		o->r = i->r >> 3;
		o->g = i->g >> 2;
		o->b = i->b >> 3;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void ReadAlpha64_16(Out *o, In *i, int Len)
{
	In *e = i + Len;
	while (i < e)
	{
		o->r = i->r >> 11;
		o->g = i->g >> 10;
		o->b = i->b >> 11;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void ReadAlpha32_24(Out *o, In *i, int Len)
{
	In *e = i + Len;

	while (i < e)
	{
		o->r = i->r;
		o->g = i->g;
		o->b = i->b;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void ReadAlpha64_24(Out *o, In *i, int Len)
{
	In *e = i + Len;
	while (i < e)
	{
		o->r = i->r >> 8;
		o->g = i->g >> 8;
		o->b = i->b >> 8;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void ReadAlpha32_32(Out *o, In *i, int Len)
{
	In *e = i + Len;

	while (i < e)
	{
		o->r = i->r;
		o->g = i->g;
		o->b = i->b;
		o->a = i->a;
		o++;
		i++;
	}
}

template<typename Out, typename In>
void ReadAlpha64_32(Out *o, In *i, int Len)
{
	In *e = i + Len;
	while (i < e)
	{
		o->r = i->r >> 8;
		o->g = i->g >> 8;
		o->b = i->b >> 8;
		o->a = i->a >> 8;
		o++;
		i++;
	}
}

GFilter::IoStatus GdcPng::ReadImage(GSurface *pDeviceContext, GStream *In)
{
	GFilter::IoStatus Status = IoError;

	Pos = 0;
	pDC = pDeviceContext;
	DeleteArray(PrevScanLine);

	GVariant v;
	if (Props &&
		Props->GetValue(LGI_FILTER_PARENT_WND, v) &&
		v.Type == GV_GVIEW)
	{
		Parent = (GView*)v.Value.Ptr;
	}

	if (!Lib->IsLoaded() && !Lib->Load(sLibrary))
	{
		if (Props)
			Props->SetValue(LGI_FILTER_ERROR, v = "Can't load libpng");
        return GFilter::IoComponentMissing;
	}

	if (setjmp(Here))
	{
		if (Props)
			Props->SetValue(LGI_FILTER_ERROR, v = "setjmp failed");
		return Status;
	}

	png_structp png_ptr = Lib->png_create_read_struct(	PNG_LIBPNG_VER_STRING,
													(void*)this,
													LibPngError,
													LibPngWarning);
	if (!png_ptr)
	{
		if (Props)
			Props->SetValue(LGI_FILTER_ERROR, v = "png_create_read_struct failed.");
	}
	else
	{
		png_infop info_ptr = Lib->png_create_info_struct(png_ptr);
		if (info_ptr)
		{
			Lib->png_set_read_fn(png_ptr, In, LibPngRead);

			#if 0 // What was this for again?
			int off = (char*)&png_ptr->io_ptr - (char*)png_ptr;
			if (!png_ptr->io_ptr)
			{
				printf("io_ptr offset = %i\n", off);
				LgiAssert(0);

				CurrentLibPng = 0;
				return false;
			}
			#endif

			Lib->png_read_png(png_ptr, info_ptr, 0, 0);
			png_bytepp Scan0 = Lib->png_get_rows(png_ptr, info_ptr);
			if (Scan0)
			{
			    int BitDepth = Lib->png_get_bit_depth(png_ptr, info_ptr);
				int FinalBits = BitDepth == 16 ? 8 : BitDepth;
				int ColourType = Lib->png_get_color_type(png_ptr, info_ptr);
				int Channels = Lib->png_get_channels(png_ptr, info_ptr);
				int RequestBits = FinalBits * Channels;
				
				if (!pDC->Create(	Lib->png_get_image_width(png_ptr, info_ptr),
									Lib->png_get_image_height(png_ptr, info_ptr),
									ColourType == PNG_COLOR_TYPE_GRAY_ALPHA ? 8 : max(RequestBits, 8)))
				{
					printf("%s:%i - GMemDC::Create(%i, %i, %i) failed.\n",
							_FL,
							Lib->png_get_image_width(png_ptr, info_ptr),
							Lib->png_get_image_height(png_ptr, info_ptr),
							RequestBits);
				}
				else
				{
					#if 1
					if (ColourType == PNG_COLOR_TYPE_GRAY_ALPHA)
					{
						pDC->HasAlpha(true); // Setup alpha channel
					}
					#endif
				
					// Copy in the scanlines
					int ActualBits = pDC->GetBits();
					int ScanLen = Lib->png_get_image_width(png_ptr, info_ptr) * ActualBits / 8;
					for (int y=0; y<pDC->Y(); y++)
					{
						uchar *Scan = (*pDC)[y];
						LgiAssert(Scan != NULL);

						GColourSpace OutCs = pDC->GetColourSpace();
						switch (RequestBits)
						{
							case 1:
							{
								uchar *o = Scan;
								uchar *e = Scan + pDC->X();
								uchar *i = Scan0[y];
								uchar Mask = 0x80;

								while (o < e)
								{
									*o++ = (*i & Mask) ? 1 : 0;
									Mask >>= 1;
									if (!Mask)
									{
										i++;
										Mask = 0x80;
									}
								}
								break;
							}
							case 16:
							{
								if (ColourType == PNG_COLOR_TYPE_GRAY_ALPHA)
								{
									uint8 *grey = Scan;
									uint8 *alpha = (*(pDC->AlphaDC()))[y];
									LgiAssert(grey && alpha);
									uint8 *end = grey + pDC->X();
									uint8 *in = Scan0[y];
									
									while (grey < end)
									{
										*grey++ = *in++;
										*alpha++ = *in++;
									}
								}
								else
								{
									memcpy(Scan, Scan0[y], ScanLen);
								}
								break;
							}
							case 24:
							{
							    switch (OutCs)
							    {
									#define Read24Case(name, bits) \
										case Cs##name: \
										{ \
											if (Lib->png_get_bit_depth(png_ptr, info_ptr) == 16) \
												Read64_##bits((G##name*)Scan, (Png48*)Scan0[y], pDC->X()); \
											else \
												Read32_##bits((G##name*)Scan, (Png24*)Scan0[y], pDC->X()); \
											break; \
										}

									Read24Case(Rgb16, 16);
									Read24Case(Bgr16, 16);

									Read24Case(Rgb24, 24);
									Read24Case(Bgr24, 24);
									Read24Case(Xrgb32, 24);
									Read24Case(Rgbx32, 24);
									Read24Case(Xbgr32, 24);
									Read24Case(Bgrx32, 24);

									Read24Case(Rgba32, 32);
									Read24Case(Bgra32, 32);
									Read24Case(Argb32, 32);
									Read24Case(Abgr32, 32);
									default:
										LgiTrace("%s:%i - Unsupported colour space: 0x%x (%s)\n",
												_FL,	
												pDC->GetColourSpace(),
												GColourSpaceToString(pDC->GetColourSpace()));
										LgiAssert(!"Not impl.");
										break;
								}
								break;
							}
							case 32:
							{
							    switch (pDC->GetColourSpace())
							    {
									#define Read32Case(name, bits) \
										case Cs##name: \
										{ \
											if (Lib->png_get_bit_depth(png_ptr, info_ptr) == 16) \
												ReadAlpha64_##bits((G##name*)Scan, (Png64*)Scan0[y], pDC->X()); \
											else \
												ReadAlpha32_##bits((G##name*)Scan, (Png32*)Scan0[y], pDC->X()); \
											break; \
										}

									Read32Case(Rgb16, 16);
									Read32Case(Bgr16, 16);

									Read32Case(Rgb24, 24);
									Read32Case(Bgr24, 24);
									Read32Case(Xrgb32, 24);
									Read32Case(Rgbx32, 24);
									Read32Case(Xbgr32, 24);
									Read32Case(Bgrx32, 24);

									Read32Case(Rgba32, 32);
									Read32Case(Bgra32, 32);
									Read32Case(Argb32, 32);
									Read32Case(Abgr32, 32);
									default:
										LgiTrace("%s:%i - Unsupported colour space: 0x%x (%s)\n",
												_FL,	
												pDC->GetColourSpace(),
												GColourSpaceToString(pDC->GetColourSpace()));
										LgiAssert(!"Not impl.");
										break;
								}
								break;
							}
							default:
							{
								memcpy(Scan, Scan0[y], ScanLen);
								break;
							}
						}
					}

					if (ActualBits <= 8)
					{
						// Copy in the palette
						png_colorp pal;
						int num_pal = 0;
						if (Lib->png_get_PLTE(png_ptr, info_ptr, &pal, &num_pal) == PNG_INFO_PLTE)
						{
						    GPalette *Pal = new GPalette(0, num_pal);
						    if (Pal)
						    {
							    for (int i=0; i<num_pal; i++)
							    {
								    GdcRGB *Rgb = (*Pal)[i];
								    if (Rgb)
								    {
									    Rgb->r = pal[i].red;
									    Rgb->g = pal[i].green;
									    Rgb->b = pal[i].blue;
								    }
							    }
							    pDC->Palette(Pal, true);
						    }
						}

						if (Lib->png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
						{
						    png_bytep trans_alpha;
						    png_color_16p trans_color;
						    int num_trans;
                            if (Lib->png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans, &trans_color))
							{
								pDC->HasAlpha(true);
								GSurface *Alpha = pDC->AlphaDC();
								if (Alpha)
								{
									for (int y=0; y<Alpha->Y(); y++)
									{
										uchar *a = (*Alpha)[y];
										uchar *p = (*pDC)[y];
										for (int x=0; x<Alpha->X(); x++)
										{
											if (p[x] < num_trans)
											{
												a[x] = trans_alpha[p[x]];
											}
											else
											{
												a[x] = 0xff;
											}
										}
									}
								}
								else
								{
									printf("%s:%i - No alpha channel.\n", __FILE__, __LINE__);
								}
							}
							else
							{
								printf("%s:%i - Bad trans ptr.\n", __FILE__, __LINE__);
							}
						}
					}

					Status = IoSuccess;
				}
			}
			else
			{
				printf("%s:%i - png_get_rows failed.\n", _FL);
			}

			Lib->png_destroy_info_struct(png_ptr, &info_ptr);
		}

		png_charp ProfName = 0;
		int CompressionType = 0;
		png_charp ColProf = 0;
		png_uint_32 ColProfLen = 0;
		if (Lib->png_get_iCCP(png_ptr, info_ptr, &ProfName, &CompressionType, &ColProf, &ColProfLen) && Props)
		{
			v.SetBinary(ColProfLen, ColProf);
			Props->SetValue(LGI_FILTER_COLOUR_PROF, v);
		}

		Lib->png_destroy_read_struct(&png_ptr, 0, 0);
	}

	return Status;
}

GFilter::IoStatus GdcPng::WriteImage(GStream *Out, GSurface *pDC)
{
	GFilter::IoStatus Status = IoError;
	
	GVariant Transparent;
	bool HasTransparency = false;
	COLOUR Back = 0;
	GVariant v;

	if (!pDC)
		return GFilter::IoError;
    if (!Lib->IsLoaded() && !Lib->Load(sLibrary))
        return GFilter::IoComponentMissing;	
	
	// Work out whether the image has transparency
	if (pDC->GetBits() == 32)
	{
		// Check alpha channel
		for (int y=0; y<pDC->Y() && !HasTransparency; y++)
		{
			System32BitPixel *p = (System32BitPixel*)(*pDC)[y];
			if (!p) break;
			System32BitPixel *e = p + pDC->X();
			while (p < e)
			{
				if (p->a < 255)
				{
					HasTransparency = true;
					break;
				}
				p++;
			}
		}
	}
	else if (pDC->AlphaDC())
	{
		GSurface *a = pDC->AlphaDC();
		if (a)
		{
			for (int y=0; y<a->Y() && !HasTransparency; y++)
			{
				uint8 *p = (*a)[y];
				if (!p) break;
				uint8 *e = p + a->X();
				while (p < e)
				{
					if (*p < 255)
					{
						HasTransparency = true;
						break;
					}
					p++;
				}
			}
		}
	}

	if (Props)
	{
		if (Props->GetValue(LGI_FILTER_PARENT_WND, v) &&
			v.Type == GV_GVIEW)
		{
			Parent = (GView*)v.Value.Ptr;
		}
		
		if (Props->GetValue(LGI_FILTER_BACKGROUND, v))
		{
			Back = v.CastInt32();
		}

		Props->GetValue(LGI_FILTER_TRANSPARENT, Transparent);
	}

	#ifdef FILTER_UI
	if (Parent && Transparent.IsNull())
	{
		// put up a dialog to ask about transparent colour
		GTransparentDlg Dlg(Parent, &Transparent);
		if (!Dlg.DoModal())
		{
			if (Props)
				Props->SetValue("Cancel", v = 1);
			
			return IoCancel;
		}
	}
	#endif

	if (setjmp(Here) == 0 && pDC && Out)
	{
		GVariant ColProfile;
		if (Props)
		{
			Props->GetValue(LGI_FILTER_COLOUR_PROF, ColProfile);
		}

		// setup
		png_structp png_ptr = Lib->png_create_write_struct(	PNG_LIBPNG_VER_STRING,
														(void*)this,
														LibPngError,
														LibPngWarning);
		if (png_ptr)
		{
			png_infop info_ptr = Lib->png_create_info_struct(png_ptr);
			if (info_ptr)
			{
				Out->SetSize(0);

				PngWriteInfo WriteInfo;
				WriteInfo.s = Out;
				WriteInfo.m = Meter;
				Lib->png_set_write_fn(png_ptr, &WriteInfo, LibPngWrite, 0);
				
				// png_set_write_status_fn(png_ptr, write_row_callback);

				bool KeyAlpha = false;
				bool ChannelAlpha = false;
				
				GMemDC *pTemp = 0;
				if (pDC->AlphaDC() && HasTransparency)
				{
					pTemp = new GMemDC(pDC->X(), pDC->Y(), System32BitColourSpace);
					if (pTemp)
					{
						pTemp->Colour(0);
						pTemp->Rectangle();
						
						pTemp->Op(GDC_ALPHA);
						pTemp->Blt(0, 0, pDC);
						pTemp->Op(GDC_SET);
						
						pDC = pTemp;
						ChannelAlpha = true;
					}
				}
				else
				{
					if (Transparent.CastInt32() &&
						Props &&
						Props->GetValue(LGI_FILTER_BACKGROUND, v))
					{
						KeyAlpha = true;
					}
				}

				int Ar = R32(Back);
				int Ag = G32(Back);
				int Ab = B32(Back);

				if (pDC->GetBits() == 32)
				{
					if (!ChannelAlpha && !KeyAlpha)
					{
						for (int y=0; y<pDC->Y(); y++)
						{
							System32BitPixel *s = (System32BitPixel*) (*pDC)[y];
							for (int x=0; x<pDC->X(); x++)
							{
								if (s[x].a < 0xff)
								{
									ChannelAlpha = true;
									y = pDC->Y();
									break;
								}
							}
						}
					}
				}

				bool ExtraAlphaChannel = ChannelAlpha || (pDC->GetBits() > 8 ? KeyAlpha : 0);

                int ColourType;
                if (pDC->GetBits() <= 8)
                {
                    if (pDC->Palette())
                        ColourType = PNG_COLOR_TYPE_PALETTE;
                    else
                        ColourType = PNG_COLOR_TYPE_GRAY;
                }
                else if (ExtraAlphaChannel)
                {
                    ColourType = PNG_COLOR_TYPE_RGB_ALPHA;
                }
                else
                {
                    ColourType = PNG_COLOR_TYPE_RGB;
                }

                Lib->png_set_IHDR(png_ptr,
                            info_ptr,
                            pDC->X(), pDC->Y(),
                            8,
                            ColourType,
                            PNG_INTERLACE_NONE,
                            PNG_COMPRESSION_TYPE_DEFAULT,
                            PNG_FILTER_TYPE_DEFAULT);
                            
				if (ColProfile.Type == GV_BINARY)
				{
					Lib->png_set_iCCP(png_ptr,
								info_ptr,
								(char*)"ColourProfile",
								NULL,
								(char*)ColProfile.Value.Binary.Data,
								ColProfile.Value.Binary.Length);
				}

				int TempLine = pDC->X() * ((pDC->GetBits() <= 8 ? 1 : 3) + (ExtraAlphaChannel ? 1 : 0));
				uchar *TempBits = new uchar[pDC->Y() * TempLine];

				if (Meter)
					Meter->SetLimits(0, pDC->Y());

				switch (pDC->GetBits())
				{
					case 8:
					{
						// Output the palette
						GPalette *Pal = pDC->Palette();
						if (Pal)
						{
							int Colours = Pal->GetSize();
							GAutoPtr<png_color> PngPal(new png_color[Colours]);
							if (PngPal)
							{
							    for (int i=0; i<Colours; i++)
							    {
									GdcRGB *Rgb = (*Pal)[i];
									if (Rgb)
									{
							            PngPal[i].red = Rgb->r;
							            PngPal[i].green = Rgb->g;
							            PngPal[i].blue = Rgb->b;
							        }
							    }
							    
                                Lib->png_set_PLTE(png_ptr,
                                            info_ptr,
                                            PngPal,
                                            Colours);
       						}
						}
						
						// Copy the pixels
						for (int y=0; y<pDC->Y(); y++)
						{
							uchar *s = (*pDC)[y];
							Png8 *d = (Png8*) (TempBits + (TempLine * y));
							for (int x=0; x<pDC->X(); x++)
							{
								*d++ = *s++;
							}								
						}
						
						// Setup the transparent palette entry
						if (KeyAlpha)
						{
							static png_byte Trans[256];
							for (uint n=0; n<CountOf(Trans); n++)
							{
								Trans[n] = Back == n ? 0 : 255;
							}
							
                            Lib->png_set_tRNS(png_ptr,
                                        info_ptr,
                                        Trans,
                                        CountOf(Trans),
                                        0);
						}
						break;
					}
					case 15:
					case 16:
					{
						//info_ptr->bit_depth = 8;
						//info_ptr->channels = 3 + (ExtraAlphaChannel ? 1 : 0);
						//info_ptr->color_type = PNG_COLOR_TYPE_RGB | (KeyAlpha ? PNG_COLOR_MASK_ALPHA : 0);
						
						for (int y=0; y<pDC->Y(); y++)
						{
							uint16 *s = (uint16*) (*pDC)[y];
							
							if (pDC->GetBits() == 15)
							{
								if (KeyAlpha)
								{
									Png32 *d = (Png32*) (TempBits + (TempLine * y));
									for (int x=0; x<pDC->X(); x++)
									{
										d->r = Rc15(*s);
										d->g = Gc15(*s);
										d->b = Bc15(*s);
										d->a = (d->r == Ar &&
												d->g == Ag &&
												d->b == Ab) ? 0 : 0xff;
										s++;
										d++;
									}
								}
								else
								{
									Png24 *d = (Png24*) (TempBits + (TempLine * y));
									for (int x=0; x<pDC->X(); x++)
									{
										d->r = Rc15(*s);
										d->g = Gc15(*s);
										d->b = Bc15(*s);
										s++;
										d++;
									}
								}
							}
							else
							{
								if (KeyAlpha)
								{
									Png32 *d = (Png32*) (TempBits + (TempLine * y));
									for (int x=0; x<pDC->X(); x++)
									{
										d->r = Rc16(*s);
										d->g = Gc16(*s);
										d->b = Bc16(*s);
										d->a = (d->r == Ar &&
												d->g == Ag &&
												d->b == Ab) ? 0 : 0xff;
										s++;
										d++;
									}
								}
								else
								{
									Png24 *d = (Png24*) (TempBits + (TempLine * y));
									for (int x=0; x<pDC->X(); x++)
									{
										d->r = Rc16(*s);
										d->g = Gc16(*s);
										d->b = Bc16(*s);
										s++;
										d++;
									}
								}

							}
						}
						break;
					}
					case 24:
					{
						//info_ptr->bit_depth = 8;
						//info_ptr->channels = 3 + (KeyAlpha ? 1 : 0);
						//info_ptr->color_type = PNG_COLOR_TYPE_RGB | (KeyAlpha ? PNG_COLOR_MASK_ALPHA : 0);

						for (int y=0; y<pDC->Y(); y++)
						{
							System24BitPixel *s = (System24BitPixel*) (*pDC)[y];
							if (KeyAlpha)
							{
								Png32 *d = (Png32*) (TempBits + (TempLine * y));
								for (int x=0; x<pDC->X(); x++)
								{
									d->r = s->r;
									d->g = s->g;
									d->b = s->b;
									d->a = (s->r == Ar &&
											s->g == Ag &&
											s->b == Ab) ? 0 : 0xff;
									s++;
									d++;
								}
							}
							else
							{
								Png24 *d = (Png24*) (TempBits + (TempLine * y));
								for (int x=0; x<pDC->X(); x++)
								{
									d->r = s->r;
									d->g = s->g;
									d->b = s->b;
									s++;
									d++;
								}
							}
						}
						break;
					}
					case 32:
					{
						//info_ptr->bit_depth = 8;
						//info_ptr->channels = 3 + (ExtraAlphaChannel ? 1 : 0);
						//info_ptr->color_type = PNG_COLOR_TYPE_RGB | (ExtraAlphaChannel ? PNG_COLOR_MASK_ALPHA : 0);

						for (int y=0; y<pDC->Y(); y++)
						{
							System32BitPixel *s = (System32BitPixel*) (*pDC)[y];
							if (ChannelAlpha)
							{
								Png32 *d = (Png32*) (TempBits + (TempLine * y));
								for (int x=0; x<pDC->X(); x++)
								{
									d->r = s->r;
									d->g = s->g;
									d->b = s->b;
									d->a = s->a;
									s++;
									d++;
								}
							}
							else if (KeyAlpha)
							{
								Png32 *d = (Png32*) (TempBits + (TempLine * y));
								Png32 *e = d + pDC->X();
								while (d < e)
								{
									if (s->a == 0 ||
										(s->r == Ar && s->g == Ag && s->b == Ab)
										)
									{
										d->r = 0;
										d->g = 0;
										d->b = 0;
										d->a = 0;
									}
									else
									{
										d->r = s->r;
										d->g = s->g;
										d->b = s->b;
										d->a = s->a;
									}

									s++;
									d++;
								}
							}
							else
							{
								Png24 *d = (Png24*) (TempBits + (TempLine * y));
								for (int x=0; x<pDC->X(); x++)
								{
									d->r = s->r;
									d->g = s->g;
									d->b = s->b;
									s++;
									d++;
								}
							}
						}
						break;
					}
					default:
					{
						goto CleanUp;
					}
				}
				
				png_bytep *row = new png_bytep[pDC->Y()];
				if (row)
				{
					for (int y=0; y<pDC->Y(); y++)
					{
						row[y] = TempBits + (TempLine * y);
					}
					
					Lib->png_set_rows(png_ptr, info_ptr, row);
					Lib->png_write_png(png_ptr, info_ptr, 0, 0);

					Status = IoSuccess;

					DeleteArray(row);
				}

				DeleteArray(TempBits);
				DeleteObj(pTemp);
				
				Lib->png_destroy_info_struct(png_ptr, &info_ptr);
			}

			CleanUp:
			Lib->png_destroy_write_struct(&png_ptr, NULL);
		}
	}

	return Status;
}

#endif // HAS_LIBPNG_ZLIB
