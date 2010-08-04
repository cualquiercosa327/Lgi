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
#if HAS_LIBJPEG

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
	bool CheckFile(char *File, int Access, uchar *Hint)
	{
		if (Hint)
		{
			if (Hint[6] == 'J' &&
				Hint[7] == 'F' &&
				Hint[8] == 'I' &&
				Hint[9] == 'F')
			{
				return true;
			}
		}

		return (File) ? stristr(File, ".jpeg") != 0 ||
						stristr(File, ".jpg") != 0 : false;
	}

	GFilter *NewObject()
	{
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
	(*cinfo->err->output_message) (cinfo);
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
			return false;
		total_length += data_length[count];

		assert(local_icc_data == 0);
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
    JpegStream *s = (JpegStream*)cinfo->client_data;
    
    while (num_bytes)
    {
        int Remain = min(cinfo->src->bytes_in_buffer, num_bytes);
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
    JpegStream *s = (JpegStream*)cinfo->client_data;
    LgiAssert(0); // not impl
    return false;
}

void j_term_source(j_decompress_ptr cinfo)
{
    JpegStream *s = (JpegStream*)cinfo->client_data;
    s->Buf.Length(0);
}

bool GdcJpeg::ReadImage(GSurface *pDC, GStream *In)
{
	bool Status = false;
	GVariant v;
	if (!IsLoaded())
	{
		if (Props)
			Props->SetValue(LGI_FILTER_ERROR, v = "libjpeg library isn't installed or wasn't usable.");
		return false;
	}

	if (!pDC || !In)
	{
		return false;
	}

    int row_stride;
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	JpegStream s;
	s.f = In;

    /*
	char *filename = NewStr(GetName());
	if (filename)
	{
		if (IsOpen())
		{
			Close();
		}

		FILE *infile;

		char16 *WideFile = LgiNewUtf8To16(filename);

		#ifdef WIN32
		infile = _wfopen(WideFile, L"rb");
		#else
		infile = fopen(filename, "rb");
		#endif

		DeleteArray(WideFile);
		if (infile)
		{
	*/
	
	ZeroObj(cinfo);
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	cinfo.client_data = &s;

	if (setjmp(jerr.setjmp_buffer))
	{
		/*
		char File[MAX_PATH];
		GetModuleFileName(Handle(), File, sizeof(File));
		*/
		jpeg_destroy_decompress(&cinfo);
		return 0;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_source_mgr Source;
	ZeroObj(Source);
	cinfo.src = &Source;
	cinfo.src->init_source = j_init_source;
	cinfo.src->fill_input_buffer = j_fill_input_buffer;
	cinfo.src->skip_input_data = j_skip_input_data;
    cinfo.src->resync_to_restart = j_resync_to_restart;
    cinfo.src->term_source = j_term_source;

	jpeg_save_markers
	(
		&cinfo,
		IJG_JPEG_ICC_MARKER,
		IJG_JPEG_APP_ICC_PROFILE_LEN
	);
	jpeg_read_header(&cinfo, true);
	
	uchar *icc_data = 0;
	uint icc_len = 0;
	if (Props && GetIccProfile(&cinfo, &icc_data, &icc_len))
	{
		v.SetBinary(icc_len, icc_data);
		Props->SetValue(LGI_FILTER_COLOUR_PROF, v);
		free(icc_data);
		icc_data = 0;
	}

	int Bits = cinfo.num_components * 8;
	#ifdef MAC
	if (Bits == 24)
		Bits = 32;
	#endif
	if (pDC->Create(cinfo.image_width, cinfo.image_height, Bits))
	{
		// zero out bitmap
		pDC->Colour(0, 24);
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
						p->R = i;
						p->G = i;
						p->B = i;
					}
				}
				pDC->Palette(Pal);
			}
		}

		// setup decompress
		jpeg_start_decompress(&cinfo);
		row_stride = cinfo.output_width * cinfo.output_components;

		#define Rc 0
		#define Gc (MaxRGB+1)
		#define Bc (MaxRGB+1)*2
		#define DownShift(x) ((int) ((x)+(1L << 15)) >> 16)
		#define UpShifted(x) ((int) ((x)*(1L << 16)+0.5))
		#define MaxRGB                          255

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
			uchar *range_limit=range_table+(MaxRGB+1);

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

			class JpegRgb
			{
			public:
				uchar r, g, b;
			};
			
			class JpegXyz
			{
			public:
				uchar x, y, z;
			};
			
			class JpegCmyk
			{
			public:
				uchar c, m, y, k;
			};					
			
			// loop through scanlines
			while (cinfo.output_scanline < cinfo.output_height)
			{
				uchar *Ptr = (*pDC)[cinfo.output_scanline];
				if (Ptr)
				{
					if (jpeg_read_scanlines(&cinfo, &Ptr, 1))
					{
						switch (cinfo.jpeg_color_space)
						{
							case JCS_GRAYSCALE:
							{
								// do nothing
								break;
							}
							case JCS_CMYK:
							{
								/*
								RGB to CMY                         
								Cyan    = 1-Red                    
								Magenta = 1-Green                 
								Yellow  = 1-Blue                  

								CMY to RGB                   
								Red   = 1-Cyan
								Green = 1-Magenta 
								Blue  = 1-Yellow

								CMY to CMYK                             
								Black   = minimum (Cyan,Magenta,Yellow) 
								Cyan    = (Cyan-Black)/(1-Black)                
								Magenta = (Magenta-Black)/(1-Black)     
								Yellow  = (Yellow-Black)/(1-Black)      

								CMYK to CMY
								Cyan    = minimum(1,Cyan*(1-Black)+Black)
								Magenta = minimum(1,Magenta*(1-Black)+Black)
								Yellow  = minimum(1,Yellow*(1-Black)+Black)
								*/

								if (cinfo.num_components == 4)
								{
									Pixel32 *d = (Pixel32*) Ptr;
									
									for (int x=0; x<pDC->X(); x++)
									{
										JpegCmyk *s = (JpegCmyk*) d;
										
										double C = (double) s->c / 255;
										double M = (double) s->m / 255;
										double Y = (double) s->y / 255;
										double K = (double) s->k / 255;

										d->r = (1.0 - min(1, C * (1.0 - K) + K)) * 255;
										d->g = (1.0 - min(1, M * (1.0 - K) + K)) * 255;
										d->b = (1.0 - min(1, Y * (1.0 - K) + K)) * 255;
										d->a = 255;

										d++;
									}
								}
								break;
							}
							case JCS_YCCK: // YCbCrK
							case 10000: // YCbCr
							{
								if (cinfo.num_components == 3)
								{
									JpegXyz *s = (JpegXyz*) Ptr;
									#ifdef MAC
									Pixel32 *d = (Pixel32*) Ptr;
									#else
									Pixel24 *d = (Pixel24*) Ptr;
									#endif
									
									for (int n=pDC->X()-1; n>=0; n--)
									{
										/*
										Y =	0.299 * R	+ 0.587 * G	+ 0.114 * B 
										Cb =	-0.169 * R	- 0.331 * G	+ 0.500 * B 
										Cr =	0.500 * R	- 0.419 * G	- 0.081 * B
										*/
										
										int x = s->x;
										int y = s->y;
										int z = s->z;
										
										d->r = range_limit[DownShift(red[x+Rc] + green[y+Rc] + blue[z+Rc])];
										d->g = range_limit[DownShift(red[x+Gc] + green[y+Gc] + blue[z+Gc])];
										d->b = range_limit[DownShift(red[x+Bc] + green[y+Bc] + blue[z+Bc])];
										#ifdef MAC
										d->a = 255;
										#endif
										d = d->Next();
										s++;
									}
								}
								break;
							}
							case JCS_YCbCr:
							case JCS_RGB:
							{
								if (cinfo.num_components == 3)
								{
									#ifdef MAC
									Pixel32 *d = (Pixel32*) Ptr;
									#else
									Pixel24 *d = (Pixel24*) Ptr;
									#endif
									JpegRgb *e = (JpegRgb*) Ptr;
									JpegRgb *s = e;
									
									#ifdef MAC
									d += pDC->X()-1;
									#else
									((char*&)d) += Pixel24::Size * (pDC->X()-1);
									#endif
									s += pDC->X() - 1;
									
									while (s >= e)
									{
										JpegRgb t = *s--;
										
										d->r = t.r;
										d->g = t.g;
										d->b = t.b;
										#ifdef MAC
										d->a = 255;
										d--;
										#else
										((uchar*&)d) -= Pixel24::Size;
										#endif
										
									}
								}
								break;
							}
						}
					}
					else break;
				}

				if (Meter) Meter->Value(cinfo.output_scanline);
			}
		}

		DeleteArray(red);
		DeleteArray(green);
		DeleteArray(blue);
		DeleteArray(range_table);

		jpeg_finish_decompress(&cinfo);

		Status = true;
	}

	jpeg_destroy_decompress(&cinfo);

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

	int w = Stream->f->Write(&Stream->Buf[0], Stream->Buf.Length());
	cinfo->dest->next_output_byte = &Stream->Buf[0];
	cinfo->dest->free_in_buffer = Stream->Buf.Length();

	return true;
}

void j_term_destination(j_compress_ptr cinfo)
{
	JpegStream *Stream = (JpegStream*)cinfo->client_data;
	int Bytes = Stream->Buf.Length() - cinfo->dest->free_in_buffer;
	int w = Stream->f->Write(&Stream->Buf[0], Bytes);
}

bool GdcJpeg::WriteImage(GStream *Out, GSurface *pDC)
{
	bool Status = FALSE;
	GVariant v;
	if (!IsLoaded())
	{
		if (Props)
			Props->SetValue(LGI_FILTER_ERROR, v = "libjpeg library isn't installed or wasn't usable.");
		return false;
	}

	bool Ok = true;

	// Setup quality setting
	GVariant Quality;
	if (Props)
		Props->GetValue(LGI_FILTER_QUALITY, Quality);

	return _Write(Out, pDC, Quality.CastInt32());
}

bool GdcJpeg::_Write(GStream *Out, GSurface *pDC, int Quality)
{
	struct jpeg_compress_struct cinfo;
	struct my_error_mgr jerr;

	int row_stride;
	cinfo.err = jpeg_std_error(&jerr.pub);
	jpeg_create_compress(&cinfo);
	jerr.pub.error_exit = my_error_exit;

	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_compress(&cinfo);
		return 0;
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
				if (	p &&
					   (p->R != i ||
					p->G != i ||
					p->B != i))
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

	jpeg_set_defaults(&cinfo);
	
	if (Quality >= 0)
	{
		jpeg_set_quality(&cinfo, Quality, true);
	}

	jpeg_start_compress(&cinfo, true);

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
			uchar *d = Buffer;

			switch (pDC->GetBits())
			{
				case 8:
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
							for (int x=0; x<pDC->X(); x++)
							{
								COLOUR c = pDC->Get(x, cinfo.next_scanline);
								*d++ = p[c].R;
								*d++ = p[c].G;
								*d++ = p[c].B;
							}
						}
					}
					break;
				}
				case 16:
				{
					for (int x=0; x<pDC->X(); x++)
					{
						COLOUR c = pDC->Get(x, cinfo.next_scanline);
						*d++ = (R16(c) << 3) | (R16(c) >> 5);
						*d++ = (G16(c) << 2) | (G16(c) >> 6);
						*d++ = (B16(c) << 3) | (B16(c) >> 5);
					}
					break;
				}
				case 24:
				{
					for (int x=0; x<pDC->X(); x++)
					{
						COLOUR c = pDC->Get(x, cinfo.next_scanline);
						*d++ = R24(c);
						*d++ = G24(c);
						*d++ = B24(c);
					}
					break;
				}
				case 32:
				{
					for (int x=0; x<pDC->X(); x++)
					{
						COLOUR c = pDC->Get(x, cinfo.next_scanline);
						*d++ = R32(c);
						*d++ = G32(c);
						*d++ = B32(c);
					}
					break;
				}
			}

			jpeg_write_scanlines(&cinfo, &Buffer, 1);
			if (Meter) Meter->Value(cinfo.next_scanline);
		}

		DeleteArray(Buffer);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	return true;
}

#endif
