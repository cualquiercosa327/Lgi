
#ifndef __XBITMAP_IMAGE_H__
#define __XBITMAP_IMAGE_H__

#include "LgiLinux.h"

class XBitmapImage : public XObject
{
	friend class XPainter;
	
	XImage *Img;
	int Bits;

public:
	enum BlitOp
	{
		ColorOnly
	};

	XBitmapImage();
	~XBitmapImage();
	
	XImage *GetImage() { return Img; }
	
	bool create(int x, int y, int bits);
	uchar *scanLine(int y);
	int bytesPerLine();
	int getBits();
};

typedef XBitmapImage *OsBitmap;


#endif
