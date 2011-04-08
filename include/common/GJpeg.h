#if !defined(_GJPEG_H_) && HAS_LIBJPEG
#define _GJPEG_H_

#define XMD_H
#undef FAR

//
// 'jpeglib.h' comes from libjpeg available here:
// http://freshmeat.net/projects/libjpeg
//
// If you don't want to build with JPEG support then set
// the define HAS_LIBJPEG to '0' in Lgi.h
//
#include "jpeglib.h"
#include <setjmp.h>

// JPEG
class LibJpeg : public GLibrary
{
public:
	LibJpeg() :
		#if defined(WIN32) && defined(_DEBUG)
		GLibrary("libjpegd")
		#else
		GLibrary("libjpeg")
		#endif
	{
		#if 0
		char File[256];
		GetModuleFileName(Handle(), File, sizeof(File));
		LgiTrace("%s:%i - JPEG: %s\n", __FILE__, __LINE__, File);
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

class GdcJpeg : public GFilter, public LibJpeg
{
	friend class GJpegOptions;

	bool _Write(GStream *Out, GSurface *pDC, int Quality);

public:
	Format GetFormat() { return FmtJpeg; }
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	bool ReadImage(GSurface *pDC, GStream *In);
	bool WriteImage(GStream *Out, GSurface *pDC);

	bool GetVariant(const char *n, GVariant &v, char *a)
	{
		if (!stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Jpeg";
		}
		else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "JPG,JPEG";
		}
		else return false;

		return true;
	}
};

#endif