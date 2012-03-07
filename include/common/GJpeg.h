#if !defined(_GJPEG_H_) && HAS_LIBJPEG
#define _GJPEG_H_

class GdcJpeg : public GFilter
{
	friend class GJpegOptions;
	class GdcJpegPriv *d;

	GFilter::IoStatus _Write(GStream *Out, GSurface *pDC, int Quality);

public:
	GdcJpeg();
	~GdcJpeg();

    char *GetComponentName() { return "libjpeg"; }
	Format GetFormat() { return FmtJpeg; }
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	IoStatus ReadImage(GSurface *pDC, GStream *In);
	IoStatus WriteImage(GStream *Out, GSurface *pDC);

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