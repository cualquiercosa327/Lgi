#include <malloc.h>

#include "xbitmapimage.h"

XBitmapImage::XBitmapImage()
{
	Img = 0;
}

XBitmapImage::~XBitmapImage()
{
	create(0, 0, 0);
}

bool XBitmapImage::create(int x, int y, int bits)
{
	bool Status = false;

	if (Img)
	{
		XDestroyImage(Img);
	}

	if (x > 0 AND
		y > 0 AND
		bits > 0)
	{
		int Pad = 4;
		Bits = bits;
		int Bytes = Bits == 24 ? 4 : (Bits + 7) / 8;

		int Line = (((x * Bytes) + Pad - 1) / Pad) * Pad;
		int Length = y * Line;
		char *Data = (char*)malloc(Length);
		if (Data)
		{
			#ifdef _DEBUG
			memset(Data, 0, Length);
			#endif
			
			Img = XCreateImage(XDisplay(), DefaultVisual(XDisplay(), 0), bits, ZPixmap, 0, Data, x, y+1, Pad << 3, Line); //XYPixmap
			if (Img)
			{
				Status = true;

				// printf("24: depth=%i bbp=%i\n", Img->depth, Img->bits_per_pixel);
			}
			else
			{
				printf("XBitmapImage::create(%i,%i,%i) XCreateImage failed, Line=%i Bits=%i\n", x, y, bits, Line, Bits);
			}
		}
	}

	return Status;
}

uchar *XBitmapImage::scanLine(int y)
{
	if (Img AND
		y >= 0 AND
		y < Img->height)
	{
		return (uchar*) (Img->data + (y * Img->bytes_per_line));
	}

	return 0;
}

int XBitmapImage::bytesPerLine()
{
	if (Img)
	{
		return Img->bytes_per_line;
	}
	return 0;
}

int XBitmapImage::getBits()
{
	return Bits;
}
