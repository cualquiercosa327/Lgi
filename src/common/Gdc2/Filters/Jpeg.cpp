/*hdr
**      FILE:           Jpeg.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Jpeg file filter
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <math.h>
#include "Lgi.h"
#include "GPalette.h"
#if HAS_LIBJPEG

#define XMD_H
#undef FAR

// If you don't want to build with JPEG support then set
// the define HAS_LIBJPEG to '0' in Lgi.h
//
// Linux:
// sudo apt-get install libjpeg-dev
#include "jpeglib.h"
#include <setjmp.h>
#include "GLibraryUtils.h"

#if LIBJPEG_SHARED
#define JPEGLIB d->
const char sLibrary[] =
	"libjpeg"
	#if defined(WINDOWS)
		_MSC_VER_STR
		#ifdef WIN64
		"x64"
		#else
		"x32"
		#endif
	#endif
	;
#else
#define JPEGLIB
#endif

// JPEG
#if LIBJPEG_SHARED
class LibJpeg : public GLibrary
{
public:
	LibJpeg() : GLibrary(sLibrary)
	{
		#if 0 // def _DEBUG
		char File[256];
		GetModuleFileNameA(Handle(), File, sizeof(File));
		LgiTrace("%s:%i - JPEG: %s\n", _FL, File);
		#endif
	}

	DynFunc1(boolean, jpeg_finish_decompress, j_decompress_ptr, cinfo);
	DynFunc3(JDIMENSION, jpeg_read_scanlines, j_decompress_ptr, cinfo, JSAMPARRAY, scanlines, JDIMENSION, max_lines);
	DynFunc1(boolean, jpeg_start_decompress, j_decompress_ptr, cinfo);
	DynFunc2(int, jpeg_read_header, j_decompress_ptr, cinfo, boolean, require_image);
	DynFunc2(int, jpeg_stdio_src, j_decompress_ptr, cinfo, FILE*, infile);
	DynFunc1(int, jpeg_destroy_decompress, j_decompress_ptr, cinfo);
	DynFunc1(int, jpeg_finish_compress, j_compress_ptr, cinfo);
	DynFunc1(int, jpeg_destroy_compress, j_compress_ptr, cinfo);
	DynFunc3(JDIMENSION, jpeg_write_scanlines, j_compress_ptr, cinfo, JSAMPARRAY, scanlines, JDIMENSION, num_lines);
	DynFunc2(int, jpeg_start_compress, j_compress_ptr, cinfo, boolean, write_all_tables);
	DynFunc1(int, jpeg_set_defaults, j_compress_ptr, cinfo);
	DynFunc1(struct jpeg_error_mgr *, jpeg_std_error, struct jpeg_error_mgr *, err);
	DynFunc2(int, jpeg_stdio_dest, j_compress_ptr, cinfo, FILE *, outfile);
	DynFunc3(int, jpeg_CreateCompress, j_compress_ptr, cinfo, int, version, size_t, structsize);
	DynFunc3(int, jpeg_CreateDecompress, j_decompress_ptr, cinfo, int, version, size_t, structsize);
	DynFunc3(int, jpeg_set_quality, j_compress_ptr, cinfo, int, quality, boolean, force_baseline);

	DynFunc3(int, jpeg_save_markers, j_decompress_ptr, cinfo, int, marker_code, unsigned int, length_limit);

};

GAutoPtr<LibJpeg> JpegLibrary;
#endif

#include <stdio.h>
#include <string.h>

#include "GLibraryUtils.h"
#include "GScrollBar.h"
#include "GVariant.h"
#include "GJpeg.h"

#define IJG_JPEG_ICC_MARKER             JPEG_APP0 + 2
#define IJG_JPEG_APP_ICC_PROFILE_LEN    0xFFFF
#define IJG_MAXIMUM_SEQUENCE_NUMBER     255
#define IJG_SEQUENCE_NUMBER_INDEX       12
#define IJG_NUMBER_OF_MARKERS_INDEX     13
#define IJG_JFIF_ICC_HEADER_LENGTH      14

/////////////////////////////////////////////////////////////////////
class GdcJpegFactory : public GFilterFactory
{
	bool CheckFile(const char *File, int Access, const uchar *Hint)
	{
		if (Hint)
		{
			if (Hint[6] == 'J' &&
				Hint[7] == 'F' &&
				Hint[8] == 'I' &&
				Hint[9] == 'F')
				return true;
			
			if (Hint[0] == 0xff &&
				Hint[1] == 0xd8 &&
				Hint[2] == 0xff &&
				Hint[3] == 0xe1)
				return true;
		}

		return (File) ? stristr(File, ".jpeg") != 0 ||
						stristr(File, ".jpg") != 0 : false;
	}

	GFilter *NewObject()
	{
		#if LIBJPEG_SHARED
		if (!JpegLibrary)
			JpegLibrary.Reset(new LibJpeg);
		#endif
		return new GdcJpeg;
	}

} JpegFactory;

/////////////////////////////////////////////////////////////////////
struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr *my_error_ptr;

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr) cinfo->err;
	// (*cinfo->err->output_message)(cinfo);

	char buf[256];
	(*cinfo->err->format_message)(cinfo, buf);
	
	longjmp(myerr->setjmp_buffer, 1);
}

bool IsJfifIccMarker(jpeg_saved_marker_ptr marker)
{
    return	marker->marker == IJG_JPEG_ICC_MARKER &&
			marker->data_length >= IJG_JFIF_ICC_HEADER_LENGTH &&
			GETJOCTET(marker->data[0]) == 0x49 &&
			GETJOCTET(marker->data[1]) == 0x43 &&
			GETJOCTET(marker->data[2]) == 0x43 &&
			GETJOCTET(marker->data[3]) == 0x5F &&
			GETJOCTET(marker->data[4]) == 0x50 &&
			GETJOCTET(marker->data[5]) == 0x52 &&
			GETJOCTET(marker->data[6]) == 0x4F &&
			GETJOCTET(marker->data[7]) == 0x46 &&
			GETJOCTET(marker->data[8]) == 0x49 &&
			GETJOCTET(marker->data[9]) == 0x4C &&
			GETJOCTET(marker->data[10]) == 0x45 &&
			GETJOCTET(marker->data[11]) == 0x0; 
}

bool GetIccProfile(j_decompress_ptr cinfo, uchar **icc_data, uint *icc_datalen)
{
	int num_markers = 0;
	jpeg_saved_marker_ptr markers = cinfo->marker_list;
	JOCTET *local_icc_data = NULL;
	int count = 0;
	uint total_length;
	bool marker_present[IJG_MAXIMUM_SEQUENCE_NUMBER];
	uint data_length[IJG_MAXIMUM_SEQUENCE_NUMBER];
	uint marker_sequence[IJG_MAXIMUM_SEQUENCE_NUMBER];

	if (icc_data == NULL || icc_datalen == NULL)
		return false;

	*icc_data = NULL;
	*icc_datalen = 0;

	ZeroObj(marker_present);
	for (markers = cinfo->marker_list; markers; markers = markers->next)
	{
		if (IsJfifIccMarker(markers))
		{
			int sequence_number = 0;
			if (num_markers == 0)
			{
				num_markers = GETJOCTET(markers->data[IJG_NUMBER_OF_MARKERS_INDEX]);
			}
			else if (num_markers != GETJOCTET(markers->data[IJG_NUMBER_OF_MARKERS_INDEX]))
			{
				return false;
			}

			sequence_number = GETJOCTET(markers->data[IJG_SEQUENCE_NUMBER_INDEX]) - 1;
			if (sequence_number < 0 || sequence_number > num_markers)
			{
				return false;
			}
			if (marker_present[sequence_number])
				return false;

			marker_present[sequence_number] = true;
			data_length[sequence_number] = 
				markers->data_length - IJG_JFIF_ICC_HEADER_LENGTH;
			marker_sequence[sequence_number] = count;
			count++;
	   }

	}

	if (num_markers == 0)
		return false;

	total_length = 0;
	markers = cinfo->marker_list;
	for (count = 0; count < num_markers; count++)
	{
		int	 previous_length = total_length;
		int	 index		   = 0;
		int	 i			   = 0;
		JOCTET  *src_pointer;
		JOCTET  *dst_pointer;

		index = marker_sequence[count];
		if (!(marker_present[count]))
			continue;
		total_length += data_length[count];

		if (local_icc_data != NULL)
			break;

		local_icc_data = (JOCTET *)malloc(total_length);
		if (local_icc_data == NULL)
			return false;

		for (i = 0; i < index; i++)
		{
			markers = markers->next;
			if (!markers)
			{
				free(local_icc_data);
				return false;
			}
		}

		dst_pointer = local_icc_data + previous_length;
		src_pointer = markers->data + IJG_JFIF_ICC_HEADER_LENGTH;
		while (data_length[count]--)
		{
			*dst_pointer++ = *src_pointer++;
		}
	}

	*icc_data = local_icc_data;
	*icc_datalen = total_length;

	return true;
}

struct JpegStream
{
    GStream *f;
    GArray<JOCTET> Buf;
};

boolean j_fill_input_buffer(j_decompress_ptr cinfo)
{
    JpegStream *s = (JpegStream*)cinfo->client_data;
    cinfo->src->next_input_byte = &s->Buf[0];
    cinfo->src->bytes_in_buffer = s->f->Read((void*)cinfo->src->next_input_byte, s->Buf.Length());
    return cinfo->src->bytes_in_buffer > 0;
}

void j_init_source(j_decompress_ptr cinfo)
{
    JpegStream *s = (JpegStream*)cinfo->client_data;
    s->Buf.Length(4 << 10);
    j_fill_input_buffer(cinfo);
}

void j_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
    // JpegStream *s = (JpegStream*)cinfo->client_data;
    
    while (num_bytes)
    {
        int Remain = min(cinfo->src->bytes_in_buffer, (size_t)num_bytes);
		if (!Remain)
			break;
        cinfo->src->next_input_byte += Remain;
        cinfo->src->bytes_in_buffer -= Remain;
        num_bytes -= Remain;
        if (num_bytes)
        {
            j_fill_input_buffer(cinfo);
        }
    }
}

boolean j_resync_to_restart(j_decompress_ptr cinfo, int desired)
{
    // JpegStream *s = (JpegStream*)cinfo->client_data;
    LgiAssert(0); // not impl
    return false;
}

void j_term_source(j_decompress_ptr cinfo)
{
    JpegStream *s = (JpegStream*)cinfo->client_data;
    s->Buf.Length(0);
}

GdcJpeg::GdcJpeg()
{
	#if LIBJPEG_SHARED
	d = JpegLibrary;
	#endif
}

typedef GRgb24 JpegRgb;
typedef GCmyk32 JpegCmyk;
struct JpegXyz
{
	uint8 x, y, z;
};

template<typename T>
void Convert16(T *d, int width)
{
	JpegRgb *s = (JpegRgb*) d;
	JpegRgb *e = s + width;
	
	while (s < e)
	{
		JpegRgb t = *s++;
		
		d->r = t.r >> 3;
		d->g = t.g >> 2;
		d->b = t.b >> 3;
		d++;
	}
}

template<typename T>
void Convert24(T *d, int width)
{
	JpegRgb *e = (JpegRgb*) d;
	JpegRgb *s = e;
	
	d += width - 1;
	s += width - 1;
	
	while (s >= e)
	{
		JpegRgb t = *s--;
		
		d->r = t.r;
		d->g = t.g;
		d->b = t.b;
		d--;
	}
}

template<typename T>
void Convert32(T *d, int width)
{
	JpegRgb *e = (JpegRgb*) d;
	JpegRgb *s = e;
	
	d += width - 1;
	s += width - 1;
	
	while (s >= e)
	{
		JpegRgb t = *s--;
		
		d->r = t.r;
		d->g = t.g;
		d->b = t.b;
		d->a = 255;
		d--;
	}
}

#define Rc 0
#define Gc (MaxRGB+1)
#define Bc (MaxRGB+1)*2
#define DownShift(x) ((int) ((x)+(1L << 15)) >> 16)
#define UpShifted(x) ((int) ((x)*(1L << 16)+0.5))
#define MaxRGB                          255

template<typename T>
void Ycc24(	T *d,
			int width,
			long *red,
			long *green,
			long *blue,
			uchar *range_table)
{
	JpegXyz *s = (JpegXyz*) d;
	T *e = d;
	uchar *range_limit = range_table + (MaxRGB + 1);
	
	d += width - 1;
	s += width - 1;

	while (d >= e)
	{
		/*	Y =	 0.299  * R	+ 0.587 * G	+ 0.114 * B 
			Cb = -0.169 * R	- 0.331 * G	+ 0.500 * B 
			Cr = 0.500  * R	- 0.419 * G	- 0.081 * B */		
		int x = s->x;
		int y = s->y;
		int z = s->z;
		
		d->r = range_limit[DownShift(red[x+Rc] + green[y+Rc] + blue[z+Rc])];
		d->g = range_limit[DownShift(red[x+Gc] + green[y+Gc] + blue[z+Gc])];
		d->b = range_limit[DownShift(red[x+Bc] + green[y+Bc] + blue[z+Bc])];
		d--;
		s--;
	}
}

template<typename T>
void Ycc32(	T *d,
			int width,
			long *red,
			long *green,
			long *blue,
			uchar *range_table)
{
	JpegXyz *s = (JpegXyz*) d;
	T *e = d;
	uchar *range_limit = range_table + (MaxRGB + 1);
	
	d += width - 1;
	s += width - 1;

	while (d >= e)
	{
		/*	Y =	 0.299  * R	+ 0.587 * G	+ 0.114 * B 
			Cb = -0.169 * R	- 0.331 * G	+ 0.500 * B 
			Cr = 0.500  * R	- 0.419 * G	- 0.081 * B */		
		int x = s->x;
		int y = s->y;
		int z = s->z;
		
		d->r = range_limit[DownShift(red[x+Rc] + green[y+Rc] + blue[z+Rc])];
		d->g = range_limit[DownShift(red[x+Gc] + green[y+Gc] + blue[z+Gc])];
		d->b = range_limit[DownShift(red[x+Bc] + green[y+Bc] + blue[z+Bc])];
		d->a = 255;
		d--;
		s--;
	}
}

template<typename D, typename S>
void CmykToRgb24(D *d, S *s, int width)
{
	D *end = d + width;
	while (d < end)
	{
		int k = s->k;

		d->r = s->c * k / 255;
		d->g = s->m * k / 255;
		d->b = s->y * k / 255;
		
		d++;
		s++;
	}
}

template<typename D, typename S>
void CmykToRgb32(D *d, S *s, int width)
{
	D *end = d + width;
	while (d < end)
	{
		int k = s->k;
		int c = s->c * k;
		int m = s->m * k;
		int y = s->y * k;

		d->r = c / 255;
		d->g = m / 255;
		d->b = y / 255;
		d->a = 255;
		
		d++;
		s++;
	}
}

GFilter::IoStatus GdcJpeg::ReadImage(GSurface *pDC, GStream *In)
{
	GFilter::IoStatus Status = IoError;
	GVariant v;
	
	#if LIBJPEG_SHARED
	if (!d->IsLoaded() && !d->Load(sLibrary))
	{
		if (Props)
		{
			GString s;
			s.Printf("libjpeg is missing (%s.%s)", sLibrary, LGI_LIBRARY_EXT);
			Props->SetValue(LGI_FILTER_ERROR, v = s);
		}
		
		static bool Warn = true;
		if (Warn)
		{
		    LgiTrace("%s:%i - Unable to load libjpg (%s.%s).\n", _FL, sLibrary, LGI_LIBRARY_EXT);
		    Warn = false;
		}
		
		return GFilter::IoComponentMissing;
	}
	#endif

	if (!pDC || !In)
	{
		return Status;
	}

    int row_stride;
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	JpegStream s;
	s.f = In;
	
	ZeroObj(cinfo);
	cinfo.err = JPEGLIB jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	cinfo.client_data = &s;

	if (setjmp(jerr.setjmp_buffer))
	{
		const char *msg = cinfo.err->jpeg_message_table[cinfo.err->msg_code];
		if (Props)
		{
			Props->SetValue(LGI_FILTER_ERROR, v = msg);
		}
	
		JPEGLIB jpeg_destroy_decompress(&cinfo);
		return Status;
	}

	JPEGLIB jpeg_create_decompress(&cinfo);
	jpeg_source_mgr Source;
	cinfo.mem->max_memory_to_use = 512 << 20;

	ZeroObj(Source);
	cinfo.src = &Source;
	cinfo.src->init_source = j_init_source;
	cinfo.src->fill_input_buffer = j_fill_input_buffer;
	cinfo.src->skip_input_data = j_skip_input_data;
    cinfo.src->resync_to_restart = j_resync_to_restart;
    cinfo.src->term_source = j_term_source;

	JPEGLIB jpeg_save_markers
	(
		&cinfo,
		IJG_JPEG_ICC_MARKER,
		IJG_JPEG_APP_ICC_PROFILE_LEN
	);
	int result = JPEGLIB jpeg_read_header(&cinfo, true);
	if (result != JPEG_HEADER_OK)
	{
		return IoError;
	}
	
	uchar *icc_data = 0;
	uint icc_len = 0;
	if (Props && GetIccProfile(&cinfo, &icc_data, &icc_len))
	{
		v.SetBinary(icc_len, icc_data);
		Props->SetValue(LGI_FILTER_COLOUR_PROF, v);
		free(icc_data);
		icc_data = 0;
	}
	
	if (Props)
	{
		GStringPipe s(256);
		s.Print("Sub-sampling: ");
		for (int i=0; i<cinfo.comps_in_scan; i++)
			s.Print("%s%ix%i", i ? "_" : "", cinfo.comp_info[i].h_samp_factor, cinfo.comp_info[i].v_samp_factor);
		GAutoString ss(s.NewStr());
		Props->SetValue(LGI_FILTER_INFO, v = ss.Get());
	}

	int Bits = cinfo.num_components * 8;
	if (pDC->Create(cinfo.image_width, cinfo.image_height, GBitsToColourSpace(Bits)))
	{
		// zero out bitmap
		pDC->Colour(0, Bits);
		pDC->Rectangle();

		// Progress
		if (Meter)
		{
			Meter->SetDescription("scanlines");
			Meter->SetLimits(0, cinfo.image_height-1);
		}

		// Read
		if (cinfo.num_components == 1)
		{
			// greyscale
			GPalette *Pal = new GPalette(0, 256);
			if (Pal)
			{
				GdcRGB *p = (*Pal)[0];
				if (p)
				{
					for (int i=0; i<256; i++, p++)
					{
						p->r = i;
						p->g = i;
						p->b = i;
					}
				}
				pDC->Palette(Pal);
			}
		}

		// setup decompress
		JPEGLIB jpeg_start_decompress(&cinfo);
		row_stride = cinfo.output_width * cinfo.output_components;

		long  *red   = new long[3*(MaxRGB+1)];
		long  *green = new long[3*(MaxRGB+1)];
		long  *blue  = new long[3*(MaxRGB+1)];
		uchar *range_table = new uchar[3*(MaxRGB+1)];

		if (red &&
			green &&
			blue &&
			range_table)
		{
			int i;
			for (i=0; i<=MaxRGB; i++)
			{
				range_table[i]=0;
				range_table[i+(MaxRGB+1)]=(uchar) i;
				range_table[i+(MaxRGB+1)*2]=MaxRGB;
			}
			// uchar *range_limit=range_table+(MaxRGB+1);

			for (i=0; i<=MaxRGB; i++)
			{
				red[i+Rc]   = UpShifted(1.00000)*i;
				green[i+Rc] = 0;
				blue[i+Rc]  = UpShifted(1.40200*0.5) * ((i << 1)-MaxRGB);

				red[i+Gc]   = UpShifted(1.00000)*i;
				green[i+Gc] = (-UpShifted(0.34414*0.5)) * ((i << 1)-MaxRGB);
				blue[i+Gc]  = (-UpShifted(0.71414*0.5)) * ((i << 1)-MaxRGB);

				red[i+Bc]   = UpShifted(1.00000)*i;
				green[i+Bc] = UpShifted(1.77200*0.5) * ((i << 1)-MaxRGB);
				blue[i+Bc]  = 0;
			}

			// loop through scanlines
			Status = IoSuccess;
			while (	cinfo.output_scanline < cinfo.output_height &&
					Status == IoSuccess)
			{
				uchar *Ptr = (*pDC)[cinfo.output_scanline];
				if (Ptr)
				{
					if (JPEGLIB jpeg_read_scanlines(&cinfo, &Ptr, 1))
					{
						switch (cinfo.jpeg_color_space)
						{
							default:
								break;
							case JCS_GRAYSCALE:
							{
								// do nothing
								break;
							}
							case JCS_CMYK:
							{
								if (cinfo.num_components != 4)
								{
									LgiAssert(!"Weird number of components for CMYK JPEG.");
									break;
								}

								LgiAssert(pDC->GetBits() == 32);
								
								switch (pDC->GetColourSpace())
								{
									#define CmykCase(name, bits) \
										case Cs##name: CmykToRgb##bits((G##name*)Ptr, (GCmyk32*)Ptr, pDC->X()); break

									CmykCase(Rgb24, 24);
									CmykCase(Bgr24, 24);
									CmykCase(Rgbx32, 24);
									CmykCase(Bgrx32, 24);
									CmykCase(Xrgb32, 24);
									CmykCase(Xbgr32, 24);
									CmykCase(Rgba32, 32);
									CmykCase(Bgra32, 32);
									CmykCase(Argb32, 32);
									CmykCase(Abgr32, 32);
									
									#undef CmykCase

									default:
										LgiAssert(!"impl me.");
										Status = IoUnsupportedFormat;
										break;
								}
								break;
							}
							case JCS_YCCK: // YCbCrK
							case 10000: // YCbCr
							{
								if (cinfo.num_components == 3)
								{
									switch (pDC->GetColourSpace())
									{
										#define YccCase(name, bits) \
											case Cs##name: Ycc##bits((G##name*)Ptr, pDC->X(), red, green, blue, range_table); break;
										
										YccCase(Rgb24, 24);
										YccCase(Bgr24, 24);
										YccCase(Xrgb32, 24);
										YccCase(Xbgr32, 24);
										YccCase(Rgbx32, 24);
										YccCase(Bgrx32, 24);

										YccCase(Argb32, 32);
										YccCase(Abgr32, 32);
										YccCase(Rgba32, 32);
										YccCase(Bgra32, 32);
										
										default:
											LgiAssert(!"Unsupported colour space.");
											Status = IoUnsupportedFormat;
											break;
									}
								}
								break;
							}
							case JCS_RGB:
							case JCS_YCbCr:
							{
								if (cinfo.num_components == 3)
								{
									int Width = pDC->X();
									switch (pDC->GetColourSpace())
									{
										#define JpegCase(name, bits) \
											case Cs##name: Convert##bits((G##name*)Ptr, Width); break;

										JpegCase(Rgb16, 16);
										JpegCase(Bgr16, 16);
										
										JpegCase(Rgb24, 24);
										JpegCase(Bgr24, 24);
										JpegCase(Xrgb32, 24);
										JpegCase(Xbgr32, 24);
										JpegCase(Rgbx32, 24);
										JpegCase(Bgrx32, 24);

										JpegCase(Argb32, 32);
										JpegCase(Abgr32, 32);
										JpegCase(Rgba32, 32);
										JpegCase(Bgra32, 32);

										default:
											LgiAssert(!"Unsupported colour space.");
											Status = IoUnsupportedFormat;
											break;
									}
								}
								break;
							}
						}
					}
					else break;
				}

				if (Meter)
				{
					Meter->Value(cinfo.output_scanline);
					if (Meter->IsCancelled())
						Status = IoCancel;
				}
			}
		}

		DeleteArray(red);
		DeleteArray(green);
		DeleteArray(blue);
		DeleteArray(range_table);

		JPEGLIB jpeg_finish_decompress(&cinfo);
	}

	JPEGLIB jpeg_destroy_decompress(&cinfo);

	return Status;
}

void j_init_destination(j_compress_ptr cinfo)
{
	JpegStream *Stream = (JpegStream*)cinfo->client_data;
	Stream->Buf.Length(4 << 10);
	cinfo->dest->free_in_buffer = Stream->Buf.Length();
	cinfo->dest->next_output_byte = &Stream->Buf[0];
}

boolean j_empty_output_buffer(j_compress_ptr cinfo)
{
	JpegStream *Stream = (JpegStream*)cinfo->client_data;

	Stream->f->Write(&Stream->Buf[0], Stream->Buf.Length());
	cinfo->dest->next_output_byte = &Stream->Buf[0];
	cinfo->dest->free_in_buffer = Stream->Buf.Length();

	return true;
}

void j_term_destination(j_compress_ptr cinfo)
{
	JpegStream *Stream = (JpegStream*)cinfo->client_data;
	int Bytes = Stream->Buf.Length() - cinfo->dest->free_in_buffer;
	if (Stream->f->Write(&Stream->Buf[0], Bytes) != Bytes)
		LgiAssert(!"Write failed.");
}

GFilter::IoStatus GdcJpeg::WriteImage(GStream *Out, GSurface *pDC)
{
	GVariant v;
	#if LIBJPEG_SHARED
	if (!d->IsLoaded() && !d->Load(sLibrary))
	{
		if (Props)
			Props->SetValue(LGI_FILTER_ERROR, v = "libjpeg library isn't installed or wasn't usable.");

		static bool Warn = true;
		if (Warn)
		{
		    LgiTrace("%s:%i - Unabled to load libjpeg.\n", _FL);
		    Warn = false;
		}
		
		return GFilter::IoComponentMissing;
	}
	#endif

	// bool Ok = true;

	// Setup quality setting
	GVariant Quality(80), SubSample(Sample_1x1_1x1_1x1), DpiX, DpiY;
	GdcPt2 Dpi;
	if (Props)
	{
		Props->GetValue(LGI_FILTER_QUALITY, Quality);
		Props->GetValue(LGI_FILTER_SUBSAMPLE, SubSample);
		Props->GetValue(LGI_FILTER_DPI_X, DpiX);
		Props->GetValue(LGI_FILTER_DPI_Y, DpiY);
		
		Dpi.x = DpiX.CastInt32();
		Dpi.y = DpiY.CastInt32();
	}

	if (!Dpi.x)
		Dpi.x = 300;
	if (!Dpi.y)
		Dpi.y = 300;

	return _Write(Out, pDC, Quality.CastInt32(), (SubSampleMode)SubSample.CastInt32(), Dpi);
}

GFilter::IoStatus GdcJpeg::_Write(GStream *Out, GSurface *pDC, int Quality, SubSampleMode SubSample, GdcPt2 Dpi)
{
	struct jpeg_compress_struct cinfo;
	struct my_error_mgr jerr;

	int row_stride;
	cinfo.err = JPEGLIB jpeg_std_error(&jerr.pub);
	JPEGLIB jpeg_create_compress(&cinfo);
	jerr.pub.error_exit = my_error_exit;

	if (setjmp(jerr.setjmp_buffer))
	{
		JPEGLIB jpeg_destroy_compress(&cinfo);
		return GFilter::IoError;
	}

	JpegStream Stream;
	Stream.f = Out;
	cinfo.client_data = &Stream;

	jpeg_destination_mgr Dest;
	ZeroObj(Dest);
	Dest.init_destination = j_init_destination;
	Dest.empty_output_buffer = j_empty_output_buffer;
	Dest.term_destination = j_term_destination;
	cinfo.dest = &Dest;

	bool GreyScale = FALSE;
	GPalette *Pal = pDC->Palette();
	if (pDC->GetBits() == 8)
	{
		if (Pal)
		{
			GreyScale = true;
			for (int i=0; i<Pal->GetSize(); i++)
			{
				GdcRGB *p = (*Pal)[i];
				if
				(
					p &&
					(
						p->r != i ||
						p->g != i ||
						p->b != i
					)
				)
				{
					GreyScale = FALSE;
					break;
				}
			}
		}
		else if (!Pal)
		{
			GreyScale = true;
		}
	}

	cinfo.image_width = pDC->X();
	cinfo.image_height = pDC->Y();
	if (GreyScale)
	{
		cinfo.input_components = 1;
		cinfo.in_color_space = JCS_GRAYSCALE;
	}
	else
	{
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;
	}
	
	JPEGLIB jpeg_set_defaults(&cinfo);
	
	switch (SubSample)
	{
	    default:
	    case Sample_1x1_1x1_1x1:
	        cinfo.comp_info[0].h_samp_factor = 1;
	        cinfo.comp_info[0].v_samp_factor = 1;
	        break;
	    case Sample_2x2_1x1_1x1:
	        cinfo.comp_info[0].h_samp_factor = 2;
	        cinfo.comp_info[0].v_samp_factor = 2;
	        break;
	    case Sample_2x1_1x1_1x1:
	        cinfo.comp_info[0].h_samp_factor = 2;
	        cinfo.comp_info[0].v_samp_factor = 1;
	        break;
	    case Sample_1x2_1x1_1x1:
	        cinfo.comp_info[0].h_samp_factor = 1;
	        cinfo.comp_info[0].v_samp_factor = 2;
	        break;
	}
    cinfo.comp_info[1].h_samp_factor = 1;
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1;
    cinfo.comp_info[2].v_samp_factor = 1;

	if (Quality >= 0)
	{
		JPEGLIB jpeg_set_quality(&cinfo, Quality, true);
	}

	cinfo.X_density = Dpi.x;
	cinfo.Y_density = Dpi.y;
	cinfo.density_unit = Dpi.x && Dpi.y ? 1 : 0;

	JPEGLIB jpeg_start_compress(&cinfo, true);

	row_stride = pDC->X() * cinfo.input_components;
	uchar *Buffer = new uchar[row_stride];
	if (Buffer)
	{
		// Progress
		if (Meter)
		{
			Meter->SetDescription("scanlines");
			Meter->SetLimits(0, cinfo.image_height-1);
		}

		// Write
		while (cinfo.next_scanline < cinfo.image_height)
		{
			uchar *dst = Buffer;

			switch (pDC->GetColourSpace())
			{
				case CsIndex8:
				{
					if (GreyScale)
					{
						memcpy(Buffer, (*pDC)[cinfo.next_scanline], pDC->X());
					}
					else if (Pal)
					{
						GdcRGB *p = (*Pal)[0];
						if (p)
						{
						    uint8 *c = (*pDC)[cinfo.next_scanline];
						    uint8 *end = c + pDC->X();
							while (c < end)
							{
							    GdcRGB &rgb = p[*c++];
								dst[0] = rgb.r;
								dst[1] = rgb.g;
								dst[2] = rgb.b;
								dst += 3;
							}
						}
					}
					break;
				}
				case CsRgb16:
				{
				    GRgb16 *p = (GRgb16*)(*pDC)[cinfo.next_scanline];
				    GRgb16 *end = p + pDC->X();
					while (p < end)
					{
						dst[0] = G5bitTo8bit(p->r);
						dst[1] = G6bitTo8bit(p->g);
						dst[2] = G5bitTo8bit(p->b);
						dst += 3;
						p++;
					}
					break;
				}
				case CsBgr16:
				{
				    GBgr16 *p = (GBgr16*)(*pDC)[cinfo.next_scanline];
				    GBgr16 *end = p + pDC->X();
					while (p < end)
					{
						dst[0] = (p->r << 3) | (p->r >> 5);
						dst[1] = (p->g << 2) | (p->g >> 6);
						dst[2] = (p->b << 3) | (p->b >> 5);
						dst += 3;
						p++;
					}
					break;
				}
				case System24BitColourSpace:
				{
				    System24BitPixel *p, *end;
				    p = (System24BitPixel*) (*pDC)[cinfo.next_scanline];
                    end = p + pDC->X();
					while (p < end)
					{
						*dst++ = p->r;
						*dst++ = p->g;
						*dst++ = p->b;
                        p++;
					}
					break;
				}
				case System32BitColourSpace:
				{
				    System32BitPixel *p = (System32BitPixel*) (*pDC)[cinfo.next_scanline];
				    System32BitPixel *end = p + pDC->X();
					while (p < end)
					{
						*dst++ = p->r;
						*dst++ = p->g;
						*dst++ = p->b;
						p++;
					}
					break;
				}
				default:
					LgiAssert(0);
					break;
			}

			JPEGLIB jpeg_write_scanlines(&cinfo, &Buffer, 1);
			if (Meter) Meter->Value(cinfo.next_scanline);
		}

		DeleteArray(Buffer);
	}

	JPEGLIB jpeg_finish_compress(&cinfo);
	JPEGLIB jpeg_destroy_compress(&cinfo);

	return GFilter::IoSuccess;
}

#endif
