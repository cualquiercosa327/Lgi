/**
	\file
	\author Matthew Allen
	\date 1/4/97
	\brief Graphics file filters
*/

#ifndef FILTER2_H
#define FILTER2_H

#include <string.h>
#include "GContainers.h"
// #include "GProperties.h"

// These are properties defined for use in the GFilter property list
#define LGI_FILTER_ERROR		"ErrorMsg"
#define LGI_FILTER_COLOUR_PROF	"ColourProfile"
#define LGI_FILTER_PARENT_WND	"Parent"
#define LGI_FILTER_FOREGROUND	"Fore"
#define LGI_FILTER_BACKGROUND	"Back"
#define LGI_FILTER_TRANSPARENT	"Transparent"
#define LGI_FILTER_QUALITY		"Quality"
#define LGI_FILTER_SUBSAMPLE	"SubSample"
#define LGI_FILTER_INFO			"Info"
#define LGI_FILTER_DPI_X		"DpiX"
#define LGI_FILTER_DPI_Y		"DpiY"

// These must be returned by a GFilter's GetVariant method
/// A descriptive name for a GFilter
#define LGI_FILTER_TYPE			"Type"
/// A comma separated list of extensions the GFilter can handle
#define LGI_FILTER_EXTENSIONS	"Extension"

LgiFunc int FindHeader(int Offset, const char *Str, GStream *f);

/// Filter can read an image
#define FILTER_CAP_READ			0x0001
/// Filter can write an image
#define FILTER_CAP_WRITE		0x0002
/// Filter can get information about an image
#define FILTER_CAP_INFO			0x0004

/// General base class for image filters. If you are creating a new filter you will
/// need to also create a factory for it by inheriting another singleton class from
/// GFilterFactory.
class LgiClass GFilter : public GDom
{
	GArray<uint8> Buf;

protected:
	GBmpMem *GetSurface(GSurface *pDC) { return pDC->pMem; }
	GRect *GetClip(GSurface *pDC) { return &pDC->Clip; }
	int GetLineLength(GSurface *pDC) { return (pDC->pMem) ? pDC->pMem->Line : 0; }

	Progress *Meter;

	inline void Swap(uint8 *out, const void *in, int len)
	{
		#ifdef __BIG_ENDIAN__
		// Swap the bytes...
		uint8 *o = out + len - 1;
		const uint8 *i = in;
		while (i < in + len)
		{
			*o++ = *i++;
		}
		#else
		// Copy the byte
		memcpy(out, in, len);
		#endif
	}

	bool Read(GStream *s, void *p, int len)
	{
		if (Buf.Length() < (uint32)len)
			Buf.Length(len);

		int r = s->Read(&Buf[0], len);
		if (r != len)
			return false;

		Swap((uint8*)p, &Buf[0], len);
		return true;
	}

	bool Write(GStream *s, const void *p, int len)
	{
		if (Buf.Length() < (uint32)len)
			Buf.Length(len);
		Swap(&Buf[0], p, len);
		int w = s->Write(&Buf[0], len);
		return w == len;
	}

public:
	GDom *Props;

	enum Format
	{
		FmtJpeg,
		FmtPng,
		FmtBmp,
		FmtIco,
		FmtTiff,
		FmtGif,
		FmtPcx,
		FmtSpi,
		FmtTga,
	};
	
	enum IoStatus
	{
	    IoError,
	    IoSuccess,
	    IoComponentMissing,
	    IoUnsupportedFormat,
	    IoCancel
	};

	GFilter()
	{
		Meter = 0;
		Props = 0;
	}
	
	virtual ~GFilter() { }
	virtual Format GetFormat() = 0;

	/// Get the progress meter
	Progress *GetProgress() { return Meter; }
	/// Set the progress meter
	void SetProgress(Progress *Prg) { Meter = Prg; }

	/// Override this to return the capabilities of your filter.
	/// \returns some combination of #FILTER_CAP_READ, #FILTER_CAP_WRITE and #FILTER_CAP_INFO.
	virtual int GetCapabilites() { return 0; }
	/// \returns the number of images in the file
	virtual int GetImages() { return 1; }
	/// Reads an image into the specified surface. Override to implement reading an image.
	/// Also you need to return FILTER_CAP_READ from GetCapabilites if implemented.
	virtual IoStatus ReadImage(GSurface *Out, GStream *In) = 0;
	/// Writes an image from the specified surface. Override to implement writing an image.
	/// Also you need to return FILTER_CAP_WRITE from GetCapabilites if implemented.
	virtual IoStatus WriteImage(GStream *Out, GSurface *In) = 0;
    /// Returns the name of the external component needed to read/write images.
    virtual char *GetComponentName() { return NULL; }
};

#define GDC_RLE_COLOUR				0x0001
#define GDC_RLE_MONO				0x0002
#define GDC_RLE_READONLY			0x0004
#define GDC_RLE_KEY					0x0008		// set if valid

class LgiClass GdcRleDC : public GMemDC
{
protected:
	COLOUR	Key;

	int	Flags;
	int	Length;
	int	Alloc;
	uchar	*Data;
	uchar	**ScanLine;

	bool SetLength(int Len);
	bool FindScanLines();
	void Empty();

public:
	GdcRleDC();
	virtual ~GdcRleDC();

	void Move(GdcRleDC *pDC);

	bool Create(int x, int y, int Bits, int LineLen = 0);
	bool CreateInfo(int x, int y, GColourSpace cs);
	void ReadOnly(bool Read);
	bool ReadOnly();
	void Mono(bool Mono);
	bool Mono();

	void Update(int Flags);
	void Draw(GSurface *Dest, int x, int y);

	int SizeOf();
	bool Read(GFile &F);
	bool Write(GFile &F);
};


////////////////////////////////////////////////////////////////

/// Factory class for creating filter objects. You should create a static
/// instance of a class inheriting from GFilterFactory in your filter code
/// that can create instances of your filter.
class LgiClass GFilterFactory
{
	static class GFilterFactory *First;
	class GFilterFactory *Next;

	/// Override this to detect whether you can handle openning a file.
	/// \returns true if this filter can handle a given file.
	virtual bool CheckFile
	(
		/// The filename, useful for extension checking.
		/// \sa LgiGetExtension.
		char *File,
		/// The access mode that is desired, either #FILTER_CAP_READ or #FILTER_CAP_WRITE.
		int Access,
		/// The first 16 bytes of the file, or NULL if not available. Useful for checking
		/// magic numbers etc.
		uchar *Hint
	) = 0;
	/// Override to return an new (heap alloced) instance of your filter.
	virtual GFilter *NewObject() = 0;

public:
	GFilterFactory();
	virtual ~GFilterFactory();

	static GFilter *New(char *File, int Access, uchar *Hint);
	static GFilter *NewAt(int i);
	static int GetItems();
};

#endif
