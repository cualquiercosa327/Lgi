/**
	\file
	\author Matthew Allen
	\date 25/3/97
	\brief Graphics file filters
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GString.h"
#include "GVariant.h"
#include "GClipBoard.h"
#include "GToken.h"
#include "GPalette.h"

#ifndef WIN32
#define BI_RGB			0L
#define BI_RLE8			1L
#define BI_RLE4			2L
#define BI_BITFIELDS	3L
#else
#include "Lgi.h"
#include <objbase.h>
#ifdef _MSC_VER
#include <IImgCtx.h>
#endif
#endif

int FindHeader(int Offset, const char *Str, GStream *f)
{
	int i = 0;
	
	if (Offset >= 0)
	{
		f->SetPos(Offset);
	}
	
	while (true)
	{
		char c;
		
		if (!f->Read(&c, 1))
			break;

		if (Str[i] == c || Str[i] == '?')
		{
			i++;
			if (!Str[i])
			{
				return true;
			}
		}
		else
		{
			i = 0;
		}
	}

	return false;
}

/// Windows and OS/2 BMP file filter
class GdcBmp : public GFilter
{
	int ActualBits;
	int ScanSize;

public:
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	Format GetFormat() { return FmtBmp; }

	/// Reads a BMP file
	IoStatus ReadImage(GSurface *Out, GStream *In);
	
	/// Writes a Windows BMP file
	IoStatus WriteImage(GStream *Out, GSurface *In);

	bool GetVariant(const char *n, GVariant &v, char *a)
	{
		if (!stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Windows or OS/2 Bitmap";
		}
		else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "BMP";
		}
		else return false;

		return true;
	}
};

class GdcBmpFactory : public GFilterFactory
{
	bool CheckFile(const char *File, int Access, const uchar *Hint)
	{
		if (File)
		{
			const char *Ext = LgiGetExtension((char*)File);
			if (Ext && !_stricmp(Ext, "bmp"))
				return true;
		}
		
		if (Hint)
		{
			if (Hint[0] == 'B' &&
				Hint[1] == 'M')
				return true;
		}
		
		return false;
	}

	GFilter *NewObject()
	{
		return new GdcBmp;
	}

} BmpFactory;

#define BMP_ID			0x4D42

#ifdef WIN32
#pragma pack(push, before_pack)
#pragma pack(1)
#endif

class BMP_FILE {
public:
	char		Type[2];
	uint32		Size;
	uint16		Reserved1;
	uint16		Reserved2;
	uint32		OffsetToData;
};

class BMP_WININFO {
public:
	// Win2 hdr (12 bytes)
	int32		Size;
	int32		Sx;
	int32		Sy;
	uint16		Planes;
	uint16		Bits;
	
	// Win3 hdr (40 bytes)
	uint32		Compression;
	uint32		DataSize;
	int32		XPels;
	int32		YPels;
	uint32		ColoursUsed;
	uint32		ColourImportant;

	// Win4 hdr (108 bytes)
	uint32 RedMask;       /* Mask identifying bits of red component */
	uint32 GreenMask;     /* Mask identifying bits of green component */
	uint32 BlueMask;      /* Mask identifying bits of blue component */
	uint32 AlphaMask;     /* Mask identifying bits of alpha component */
	uint32 CSType;        /* Color space type */
	uint32 RedX;          /* X coordinate of red endpoint */
	uint32 RedY;          /* Y coordinate of red endpoint */
	uint32 RedZ;          /* Z coordinate of red endpoint */
	uint32 GreenX;        /* X coordinate of green endpoint */
	uint32 GreenY;        /* Y coordinate of green endpoint */
	uint32 GreenZ;        /* Z coordinate of green endpoint */
	uint32 BlueX;         /* X coordinate of blue endpoint */
	uint32 BlueY;         /* Y coordinate of blue endpoint */
	uint32 BlueZ;         /* Z coordinate of blue endpoint */
	uint32 GammaRed;      /* Gamma red coordinate scale value */
	uint32 GammaGreen;    /* Gamma green coordinate scale value */
	uint32 GammaBlue;     /* Gamma blue coordinate scale value */

	bool Read(GStream &f)
	{
		ZeroObj(*this);
		int64 Start = f.GetPos();
		
		#define Rd(var) if (f.Read(&var, sizeof(var)) != sizeof(var)) \
			{ LgiTrace("Bmp.Read(%i) failed\n", (int)sizeof(var)); return false; }
		Rd(Size); // 4
		Rd(Sx); // 4
		Rd(Sy); // 4
		Rd(Planes); // 2
		Rd(Bits); // 2
		// = 16
		
		if (Size >= 40)
		{
			Rd(Compression); // 4
			Rd(DataSize); // 4
			Rd(XPels); // 4
			Rd(YPels); // 4
			Rd(ColoursUsed); // 4
			Rd(ColourImportant); // 4
			// = 24 + 16 = 40
		}
		
		if (Size >= 52)
		{
			Rd(RedMask); // 4
			Rd(GreenMask); // 4
			Rd(BlueMask); // 4
			// 12 + 40 = 52
		}

		if (Size >= 56)
		{
			Rd(AlphaMask); // 4
			// = 4 + 52 = 56
		}
		
		if (Size >= 108)
		{
			Rd(CSType); // 4
			Rd(RedX); // 4
			Rd(RedY); // 4
			Rd(RedZ); // 4
			Rd(GreenX); // 4
			Rd(GreenY); // 4
			Rd(GreenZ); // 4
			Rd(BlueX); // 4
			Rd(BlueY); // 4
			Rd(BlueZ); // 4
			Rd(GammaRed); // 4
			Rd(GammaGreen); // 4
			Rd(GammaBlue); // 4
			// = 52 + 56 = 108
		}

		int64 End = f.GetPos();
		int64 Bytes = End - Start;
		return Bytes >= 12;
	}
};

#ifdef WIN32
#pragma pack(pop, before_pack)
#endif

static int CountSetBits(uint32 b)
{
	int Count = 0;
	for (int i=0; i<sizeof(b)<<3; i++)
	{
		if ((b & (1 << i)) != 0)
			Count++;
	}
	return Count;
}

struct MaskComp
{
	GComponentType Type;
	uint32 Mask;
	int Bits;
	
	void Set(GComponentType t, int mask)
	{
		Type = t;
		Mask = mask;
		Bits = CountSetBits(Mask);
	}
};

int UpSort(MaskComp *a, MaskComp *b)
{
	return b->Mask < a->Mask ? 1 : -1;
}

int DownSort(MaskComp *a, MaskComp *b)
{
	return b->Mask > a->Mask ? 1 : -1;
}

GFilter::IoStatus GdcBmp::ReadImage(GSurface *pDC, GStream *In)
{
	if (!pDC || !In)
		return GFilter::IoError;

	ActualBits = 0;
	ScanSize = 0;
	
	BMP_FILE File;
	BMP_WININFO Info;
	GRect Cr;
	
	Read(In, &File.Type, sizeof(File.Type));
	Read(In, &File.Size, sizeof(File.Size));
	Read(In, &File.Reserved1, sizeof(File.Reserved1));
	Read(In, &File.Reserved2, sizeof(File.Reserved2));
	Read(In, &File.OffsetToData, sizeof(File.OffsetToData));

	if (!Info.Read(*In))
	{
		LgiTrace("%s:%i - BmpHdr read failed.\n", _FL);
		return GFilter::IoError;
	}

	if (File.Type[0] != 'B' || File.Type[1] != 'M')
	{
		LgiTrace("%s:%i - Bmp file id failed: '%.2s'.\n", _FL, File.Type);
		return GFilter::IoUnsupportedFormat;
	}

	ActualBits = Info.Bits;
	ScanSize = BMPWIDTH(Info.Sx * Info.Bits);
	int MemBits = max(Info.Bits, 8);
	if (!pDC->Create(Info.Sx, Info.Sy, GBitsToColourSpace(MemBits), ScanSize))
	{
		LgiTrace("%s:%i - MemDC(%i,%i,%i) failed.\n", _FL, Info.Sx, Info.Sy, max(Info.Bits, 8));
		return GFilter::IoError;
	}

	if (pDC->GetBits() <= 8)
	{
		int Colours = 1 << ActualBits;
		GPalette *Palette = new GPalette;
		if (Palette)
		{
			Palette->SetSize(Colours);
			GdcRGB *Start = (*Palette)[0];
			if (Start)
			{
				In->Read(Start, sizeof(GdcRGB)*Colours);
				Palette->SwapRAndB();
			}

			Palette->Update();
			pDC->Palette(Palette);
		}
	}

	GBmpMem *pMem = GetSurface(pDC);
	In->SetPos(File.OffsetToData);

	GFilter::IoStatus Status = IoSuccess;
	if (Info.Compression == BI_RLE8)
	{
		// 8 bit RLE compressed image
		int64 Remaining = In->GetSize() - In->GetPos();
		uchar *Data = new uchar[Remaining];
		if (Data)
		{
			if (In->Read(Data, (int)Remaining) == Remaining)
			{
				int x=0, y=pDC->Y()-1;
				uchar *p = Data;
				bool Done = false;

				while (!Done && p < Data + Remaining)
				{
					uchar Length = *p++;
					uchar Colour = *p++;

					if (Length == 0)
					{
						switch (Colour)
						{
							case 0:
							{
								x = 0;
								y--;
								break;
							}
							case 1:
							{
								Done = true;
								break;
							}
							case 2:
							{
								x += *p++;
								y -= *p++;
								break;
							}
							default:
							{
								// absolute mode
								uchar *Pixel = (*pDC)[y];
								if (Pixel && y >= 0 && y < pDC->Y())
								{
									int Len = min(Colour, pDC->X() - x);
									if (Len > 0)
									{
										memcpy(Pixel + x, p, Len);
										x += Colour;
										p += Colour;
									}
								}
								else
								{
									p += Colour;
								}
								if ((NativeInt) p & 1) p++;
								break;
							}
						}
					}
					else
					{
						// encoded mode
						uchar *Pixel = (*pDC)[y];
						if (Pixel && y >= 0 && y < pDC->Y())
						{
							int Len = min(Length, pDC->X() - x);
							if (Len > 0)
							{
								memset(Pixel + x, Colour, Len);
								x += Length;
							}
						}
					}
				}
			}
		}
	}
	else if (Info.Compression == BI_RLE4)
	{
		// 4 bit RLE compressed image

		// not implemented
		LgiTrace("%s:%i - BI_RLE4 not implemented.\n", _FL);
	}
	else
	{
		// Progress
		if (Meter)
		{
			Meter->SetDescription("scanlines");
			Meter->SetLimits(0, pMem->y-1);
		}
		
		GColourSpace SrcCs = CsNone;

		switch (ActualBits)
		{
			case 8:
				SrcCs = CsIndex8;
				break;
			case 15:
				SrcCs = CsRgb15;
				break;
			case 16:
				SrcCs = CsRgb16;
				break;
			case 24:
				SrcCs = CsBgr24;
				break;
			case 32:
				SrcCs = CsBgra32;
				break;
		}

		#if 1
		if (Info.Compression == BI_BITFIELDS)
		{
			// Should we try and create a colour space from these fields?
			GArray<MaskComp> Comps;
			Comps.New().Set(CtRed, Info.RedMask);
			Comps.New().Set(CtGreen, Info.GreenMask);
			Comps.New().Set(CtBlue, Info.BlueMask);
			
			if (Info.AlphaMask)
				Comps.New().Set(CtAlpha, Info.AlphaMask);
			Comps.Sort(ActualBits == 16 ? DownSort : UpSort);

			GColourSpaceBits Cs;
			Cs.All = 0;
			for (int i=0; i<Comps.Length(); i++)
			{
				MaskComp &c = Comps[i];
				Cs.All <<= 8;
				Cs.All |= (c.Type << 4) | c.Bits;
			}
			
			SrcCs = (GColourSpace) Cs.All;
		}
		#endif

		// straight
		switch (ActualBits)
		{
			case 1:
			{
				uchar *Buffer = new uchar[ScanSize];
				if (Buffer)
				{
					for (int y=pMem->y-1; y>=0; y--)
					{
						if (In->Read(Buffer, ScanSize) == ScanSize)
						{
							uchar Mask = 0x80;
							uchar *d = Buffer;
							for (int x=0; x<pDC->X(); x++)
							{
								pDC->Colour((*d & Mask) ? 1 : 0);
								pDC->Set(x, y);

								Mask >>= 1;
								if (!Mask)
								{
									Mask = 0x80;
									d++;
								}
							}
						}
						else
						{
						    Status = IoError;
							break;
						}

						if (Meter) Meter->Value(pMem->y-1-y);
					}

					DeleteArray(Buffer);
				}
				break;
			}
			case 4:
			{
				uchar *Buffer = new uchar[ScanSize];
				if (Buffer)
				{
					for (int y=pMem->y-1; y>=0; y--)
					{
						if (In->Read(Buffer, ScanSize) != ScanSize)
						{
						    Status = IoError;
						    break;
						}

						uchar *d = Buffer;
						for (int x=0; x<pDC->X(); x++)
						{
							if (x & 1)
							{
								pDC->Colour(*d & 0xf);
								d++;
							}
							else
							{
								pDC->Colour(*d >> 4);
							}
							pDC->Set(x, y);
						}

						if (Meter) Meter->Value(pMem->y-1-y);
					}

					DeleteArray(Buffer);
				}
				break;
			}
			default:
			{
				GColourSpace DstCs = pDC->GetColourSpace();
				for (int i=pMem->y-1; i>=0; i--)
				{
					uint8 *Ptr = pMem->Base + (pMem->Line * i);
					ssize_t r = In->Read(Ptr, ScanSize);
					if (r != ScanSize)
					{
						Status = IoError;
						LgiTrace("%s:%i - Bmp read err, wanted %i, got %i.\n", _FL, ScanSize, r);
						break;
					}
					
					if (DstCs != SrcCs)
					{
						if (!LgiRopRgb(Ptr, DstCs, Ptr, SrcCs, pMem->x, false))
						{
							Status = IoUnsupportedFormat;
							LgiTrace("%s:%i - Bmp had unsupported bit depth.\n", _FL);
							break;
						}
						
					}

					if (Meter) Meter->Value(pMem->y-1-i);
				}
				break;
			}
		}
	}

	Cr.ZOff(pDC->X()-1, pDC->Y()-1);
	pDC->ClipRgn(&Cr);
	pDC->Update(GDC_BITS_CHANGE|GDC_PAL_CHANGE);
	
	return Status;
}

template<typename T>
int SwapWrite(GStream *Out, T v)
{
	#if 0 // __ORDER_BIG_ENDIAN__
	uint8 *s = (uint8*)&v;
	uint8 *e = s + sizeof(v) - 1;
	while (s < e)
	{
		uint8 tmp = *s;
		*s++ = *e;
		*e-- = tmp;
	}
	#endif
	return Out->Write(&v, sizeof(v));
}

GFilter::IoStatus GdcBmp::WriteImage(GStream *Out, GSurface *pDC)
{
    GFilter::IoStatus Status = IoError;
	
	if (!pDC || !Out)
		return GFilter::IoError;

	BMP_FILE File;
	BMP_WININFO Info;
	GBmpMem *pMem = GetSurface(pDC);
	int Colours = (pMem->Cs == CsIndex8) ? 1 << 8 : 0;
	int UsedBits = GColourSpaceToBits(pMem->Cs);

	GPalette *Palette = pDC->Palette();
	if (pMem->Cs == CsIndex8 && Palette)
	{
		int Size = Palette->GetSize();
		int Bits = 8;
		for (int c=256; c; c>>=1, Bits--)
		{
			if (c & Size)
			{
				Colours = c;
				UsedBits = Bits;
				break;
			}
		}
	}

	if (!pMem ||
		!pMem->x ||
		!pMem->y)
	{
		return GFilter::IoError;
	}

	Out->SetSize(0);

	#define Wr(var) SwapWrite(Out, var)

	Info.Compression = UsedBits == 16 || UsedBits == 32 ? BI_BITFIELDS : BI_RGB;
	int BitFieldSize = Info.Compression == BI_BITFIELDS ? 16 : 0;
	Info.Size = 40 + BitFieldSize;

	File.Type[0] = 'B';
	File.Type[1] = 'M';
	File.OffsetToData = 14 + Info.Size;
	File.Size = File.OffsetToData + (abs(pMem->Line) * pMem->y);
	File.Reserved1 = 0;
	File.Reserved2 = 0;

	Info.Sx = pMem->x;
	Info.Sy = pMem->y;
	Info.Planes = 1;
	Info.Bits = UsedBits;
	Info.DataSize = 0;
	Info.XPels = 3000;
	Info.YPels = 3000;
	Info.ColoursUsed = Colours;
	Info.ColourImportant = Colours;

	bool Written =	Out->Write(File.Type, 2) &&
					Wr(File.Size) &&
					Wr(File.Reserved1) &&
					Wr(File.Reserved2) &&
					Wr(File.OffsetToData);

	Written =		Wr(Info.Size) &&
					Wr(Info.Sx) &&
					Wr(Info.Sy) &&
					Wr(Info.Planes) &&
					Wr(Info.Bits) &&
					Wr(Info.Compression) &&
					Wr(Info.DataSize) &&
					Wr(Info.XPels) &&
					Wr(Info.YPels) &&
					Wr(Info.ColoursUsed) &&
					Wr(Info.ColourImportant);

	if (Written)
	{
		if (pMem->Cs == CsIndex8)
		{
			int Done = 0;

			if (Palette)
			{
				GdcRGB *Start = (*Palette)[0];
				if (Start)
				{
					Palette->SwapRAndB();
					Out->Write(Start, sizeof(GdcRGB)*Palette->GetSize());
					Palette->SwapRAndB();
					Done += Palette->GetSize();
				}
			}

			char Grey[4];
			Grey[3] = 0;

			while (Done < Colours)
			{
				int C = Done * 256 / Colours;

				Grey[0] = C;
				Grey[1] = C;
				Grey[2] = C;

				Out->Write(Grey, 4);
				Done++;
			}
		}

		// Progress
		if (Meter)
		{
			Meter->SetDescription("scanlines");
			Meter->SetLimits(0, pMem->y-1);
		}

		int Bytes = BMPWIDTH(pMem->x * UsedBits);
		Status = GFilter::IoSuccess;
		switch (UsedBits)
		{
			case 1:
			{
				uchar *Buffer = new uchar[Bytes];
				if (Buffer)
				{
					for (int i=pMem->y-1; i>=0; i--)
					{
						uchar *Src = pMem->Base + (i * pMem->Line);
						
						// assemble the bits
						memset(Buffer, 0, Bytes);
						for (int x=0; x<pMem->x; x++)
						{
							Buffer[x>>3] |= (Src[x] & 1) << (x & 3);
						}

						// write the bits
						if (Out->Write(Buffer, Bytes) != Bytes)
						{
						    Status = IoError;
						    break;
						}
						
						// update status
						if (Meter) Meter->Value(pMem->y-1-i);
					}

					DeleteArray(Buffer);
				}
				break;
			}
			case 4:
			{
				uchar *Buffer = new uchar[Bytes];
				if (Buffer)
				{
					for (int i=pMem->y-1; i>=0; i--)
					{
						uchar *Src = pMem->Base + (i * pMem->Line);

						// assemble the nibbles
						for (int x=0; x<pMem->x; x+=2)
						{
							Buffer[x>>1] = (Src[x]<<4) | (Src[x+1]&0x0f);
						}

						// write the line
						if (Out->Write(Buffer, Bytes) != Bytes)
						{
						    Status = IoError;
						    break;
						}
						
						// update status
						if (Meter) Meter->Value(pMem->y-1-i);
					}

					DeleteArray(Buffer);
				}
				break;
			}
			default:
			{
				if (UsedBits == 16)
				{
					System16BitPixel px[8];
					ZeroObj(px);
					px[0].r = 0x1f;
					px[2].g = 0x3f;
					px[4].b = 0x1f;
					Out->Write(px, sizeof(px));
				}
				else if (UsedBits == 32)
				{
					System32BitPixel px[4];
					ZeroObj(px);
					px[0].r = 0xff;
					px[1].g = 0xff;
					px[2].b = 0xff;
					px[3].a = 0xff;
					Out->Write(px, sizeof(px));
				}

				for (int i=pMem->y-1; i>=0; i--)
				{
					ssize_t w = Out->Write(pMem->Base + (i * pMem->Line), Bytes);
					if (w != Bytes)
					{
						Status = IoError;
						break;
					}

					if (Meter) Meter->Value(pMem->y-1-i);
				}
				break;
			}
		}
	}

	Out->Close();
	
	return Status;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// ICO file format
class GdcIco : public GFilter
{
	int TruncSize(int i)
	{
		if (i >= 64) return 64;
		if (i >= 32) return 32;
		return 16;
	}

public:
	GdcIco();

	Format GetFormat() { return FmtIco; }
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	int GetImages() { return 1; }
	IoStatus ReadImage(GSurface *pDC, GStream *In);
	IoStatus WriteImage(GStream *Out, GSurface *pDC);
	bool GetVariant(const char *n, GVariant &v, char *a);
};

class GdcIcoFactory : public GFilterFactory
{
	bool CheckFile(const char *File, int Access, const uchar *Hint)
	{
		return (File) ? stristr(File, ".ico") != 0 : false;
	}

	GFilter *NewObject()
	{
		return new GdcIco;
	}

} IcoFactory;

GdcIco::GdcIco()
{
}

bool GdcIco::GetVariant(const char *n, GVariant &v, char *a)
{
	if (!stricmp(n, LGI_FILTER_TYPE))
	{
		v = "Icon file";
	}
	else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
	{
		v = "ICO";
	}
	else return false;

	return true;
}

GFilter::IoStatus GdcIco::ReadImage(GSurface *pDC, GStream *In)
{
	GFilter::IoStatus Status = IoError;
	int MyBits = 0;

	int16 Reserved_1;
	int16 Type;
	int16 Count;

	Read(In, &Reserved_1, sizeof(Reserved_1));
	Read(In, &Type, sizeof(Type));
	Read(In, &Count, sizeof(Count));

	for (int Image = 0; Image < Count; Image++)
	{
		int8	Width;
		int8	Height;
		int8	ColorCount;
		int8	Reserved_2;
		int16	Planes;
		int16	BitCount;
		int32	BytesInRes;
		int32	ImageOffset;
		
		Read(In, &Width, sizeof(Width));
		Read(In, &Height, sizeof(Height));
		Read(In, &ColorCount, sizeof(ColorCount));
		Read(In, &Reserved_2, sizeof(Reserved_2));
		Read(In, &Planes, sizeof(Planes));
		Read(In, &BitCount, sizeof(BitCount));
		Read(In, &BytesInRes, sizeof(BytesInRes));
		Read(In, &ImageOffset, sizeof(ImageOffset));
		
		int BytesLeft = BytesInRes;

		int64 CurrentPos = In->GetPos();
		In->SetPos(ImageOffset);

		BMP_WININFO	Header;
		GdcRGB *Colours;
		uchar *XorBytes = 0;
		uchar *AndBytes = 0;

		int64 StartHdrPos = In->GetPos();
		if (!Header.Read(*In))
			return GFilter::IoError;
		BytesLeft -= In->GetPos() - StartHdrPos;

		if (!Header.Sx) Header.Sx = Width;
		if (!Header.Sy) Header.Sy = Height;
		if (!Header.Bits)
		{
			if (BitCount) Header.Bits = BitCount;
			else if (ColorCount)
			{
				for (int i=1; i<=8; i++)
				{
					if (1 << i >= ColorCount)
					{
						BitCount = Header.Bits = i;
						break;
					}
				}
			}
		}

		Colours = new GdcRGB[ColorCount];
		if (Colours)
		{
			In->Read(Colours, sizeof(GdcRGB) * ColorCount);
			BytesLeft -= sizeof(GdcRGB) * ColorCount;
		}

		Header.Sy >>= 1;
		
		int XorSize = BMPWIDTH(Header.Sx * Header.Bits) * Height;
		int AndSize = BMPWIDTH(Header.Sx) * Height;
		
		if (BytesLeft >= XorSize)
		{
			XorBytes = new uchar[XorSize];
			if (XorBytes)
			{
				In->Read(XorBytes, XorSize);
				BytesLeft -= XorSize;
			}
		}
		
		if (BytesLeft >= AndSize)
		{
			AndBytes = new uchar[AndSize];
			if (AndBytes)
			{
				In->Read(AndBytes, AndSize);
				BytesLeft -= AndSize;
			}
		}

		/*
		LgiTrace("BytesInRes=%i, Xor=%i, And=%i, Pal=%i, BytesLeft=%i\n",
			BytesInRes,
			XorSize,
			AndSize,
			sizeof(GdcRGB) * Cols,
			BytesLeft);
		*/

		if (Colours &&
			XorBytes &&
			(Header.Bits > MyBits || Width > pDC->X() || Height > pDC->Y()) &&
			pDC->Create(Width, Height, GBitsToColourSpace(max(8, Header.Bits)) ))
		{
			MyBits = Header.Bits;
			pDC->Colour(0, 24);
			pDC->Rectangle();

			GPalette *Pal = new GPalette;
			if (Pal)
			{
				Pal->SetSize(ColorCount);
				memcpy((*Pal)[0], Colours, sizeof(GdcRGB) * ColorCount);
				Pal->SwapRAndB();
				pDC->Palette(Pal, true);
			}
			
			if (AndBytes)
			{
				pDC->HasAlpha(true);
			}			

			GSurface *pAlpha = pDC->AlphaDC();
			int XorLine = XorSize / Height;
			int AndLine = AndSize / Height;
			for (int y=0; y<Height; y++)
			{
				uchar *Dest = (*pDC)[y];
				uchar *Alpha = pAlpha ? (*pAlpha)[y] : 0;
				uchar *Src = XorBytes + ((Height-y-1) * XorLine);
				uchar *Mask = AndBytes ? AndBytes + ((Height-y-1) * AndLine) : 0;

				switch (Header.Bits)
				{
					case 1:
					{
						uchar m = 0x80;
						for (int x=0; x<Width; x++)
						{
							if (Alpha)
							{
								Alpha[x] = (Mask[x>>3] & (0x80 >> (x&7))) ? 0 : 0xff;
							}

							if (Src[x/8] & m)
							{
								Dest[x] = 1;
							}
							else
							{
								Dest[x] = 0;
							}
							m >>= 1;
							if (!m) m = 0x80;
						}
						Status = IoSuccess;
						break;
					}
					case 4:
					{
						for (int x=0; x<Width; x++)
						{
							if (Alpha)
							{
								Alpha[x] = (Mask[x>>3] & (0x80 >> (x&7))) ? 0 : 0xff;
							}

							if (x & 1)
							{
								Dest[x] = Src[x>>1] & 0xf;
							}
							else
							{
								Dest[x] = Src[x>>1] >> 4;
							}
						}
						Status = IoSuccess;
						break;
					}
					case 8:
					{
						for (int x=0; x<Width; x++)
						{
							if (Alpha)
							{
								Alpha[x] = (Mask[x>>3] & (0x80 >> (x&7))) ? 0 : 0xff;
							}

							Dest[x] = Src[x];
						}

						Status = IoSuccess;
						break;
					}
				}
			}
		}
		else
		{
			LgiTrace("%s:%i - Header size error: %i != %i + %i, Img: %ix%i @ %i bits\n",
				_FL,
				Header.DataSize, XorSize, AndSize,
				Header.Sx, Header.Sy, Header.Bits);
		}

		DeleteArray(Colours);
		DeleteArray(XorBytes);
		DeleteArray(AndBytes);
		In->SetPos(CurrentPos);
	}

	return Status;
}

GFilter::IoStatus GdcIco::WriteImage(GStream *Out, GSurface *pDC)
{
	GFilter::IoStatus Status = IoError;

	if (!pDC || pDC->GetBits() > 8 || !Out)
		return GFilter::IoError;

	Out->SetSize(0);

	int ActualBits = pDC->GetBits();
	GPalette *Pal = pDC->Palette();
	if (Pal)
	{
		if (Pal->GetSize() <= 2)
		{
			ActualBits = 1;
		}
		else if (Pal->GetSize() <= 16)
		{
			ActualBits = 4;
		}
	}

	// write file header
	int Colours = 1 << ActualBits;
	int16 Reserved_1 = 0;
	int16 Type = 1;
	int16 Count = 1;

	Write(Out, &Reserved_1, sizeof(Reserved_1));
	Write(Out, &Type, sizeof(Type));
	Write(Out, &Count, sizeof(Count));

	// write directory list
	int8	Width = TruncSize(pDC->X());
	int8	Height = TruncSize(pDC->Y());
	int8	ColorCount = Colours;
	int8	Reserved_2 = 0;
	int16	Planes = 0;
	int16	BitCount = 0;

	int Line = (ActualBits * Width) / 8;
	int MaskLine = BMPWIDTH(Width/8);
	int32	BytesInRes = sizeof(BMP_WININFO) +
						(sizeof(GdcRGB) * Colours) +
						(Line * Height) +
						(MaskLine * Height);
	
	Write(Out, &Width, sizeof(Width));
	Write(Out, &Height, sizeof(Height));
	Write(Out, &ColorCount, sizeof(ColorCount));
	Write(Out, &Reserved_2, sizeof(Reserved_2));
	Write(Out, &Planes, sizeof(Planes));
	Write(Out, &BitCount, sizeof(BitCount));
	Write(Out, &BytesInRes, sizeof(BytesInRes));

	int32	ImageOffset = (int32)(Out->GetPos() + sizeof(ImageOffset));
	Write(Out, &ImageOffset, sizeof(ImageOffset));

	// Write icon itself
	BMP_WININFO	Header;
	LgiAssert(sizeof(Header) == 40);
	Header.Size = sizeof(Header);
	Header.Sx = Width;
	Header.Sy = Height * 2;
	Header.Planes = 1;
	Header.Bits = ActualBits;
	Header.Compression = 0; // BI_RGB;
	Header.DataSize = (Line * Height) + (MaskLine * Height);
	Header.XPels = 0;
	Header.YPels = 0;
	Header.ColoursUsed = 0;
	Header.ColourImportant = 0;
	Out->Write(&Header, sizeof(Header));

	// Write palette
	if (Pal)
	{
		Pal->SwapRAndB();
		Out->Write((*Pal)[0], sizeof(GdcRGB) * (1 << ActualBits));
		Pal->SwapRAndB();
	}

	// Get background colour
	COLOUR Back = 0xffffffff;
	if (Props)
	{
		GVariant v;
		if (Props->GetValue(LGI_FILTER_BACKGROUND, v))
			Back = v.CastInt32();
	}

	// Write "Colour" bits
	int y;
	for (y=Height-1; y>=0; y--)
	{
		uchar *Src = (*pDC)[y];

		// xor bits (colour)
		switch (ActualBits)
		{
			case 4:
			{
				uchar Dest = 0;
				for (int x=0; x<Width; x++)
				{
					if (x&1)
					{
						Dest |=	(Back == Src[x]) ? 0 : Src[x] & 0xf;
						Write(Out, &Dest, sizeof(Dest));
						Dest = 0;
					}
					else
					{
						Dest |=	(Back == Src[x]) ? 0 : (Src[x] & 0xf) << 4;
					}
				}

				Status = IoSuccess;
				break;
			}
			case 8:
			{
				for (int x=0; x<Width; x++)
				{
					uchar Dest = (Back == Src[x]) ? 0 : Src[x];
					Write(Out, &Dest, sizeof(Dest));
				}
				Status = IoSuccess;
				break;
			}
		}
	}

	// Write "And" bits (mask)
	for (y=Height-1; y>=0; y--)
	{
		uchar *Src = (*pDC)[y];
		uchar Dest = 0;
		uchar Mask = 0x80;
		
		int x;
		for (x = 0; x < Width; x++)
		{
			if (Src[x] == Back) Dest |= Mask;
			Mask >>= 1;
			if (!Mask)
			{
				Write(Out, &Dest, sizeof(Dest));
				Dest = 0;
				Mask = 0x80;
			}
		}

		x = Width / 8;
		while (x < MaskLine)
		{
			uchar c = 0;
			Write(Out, &c, sizeof(c));
			x++;
		}

		Status = IoSuccess;
	}

	return Status;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// Run length encoded DC
GdcRleDC::GdcRleDC()
{
	Flags = GDC_RLE_COLOUR | GDC_RLE_READONLY;
	Length = 0;
	Alloc = 0;
	Data = 0;
	ScanLine = 0;
	Key = 0;

	pMem = new GBmpMem;
	if (pMem)
	{
		pMem->Base = 0;
		pMem->x = 0;
		pMem->y = 0;
		pMem->Cs = CsNone;
		pMem->Line = 0;
		pMem->Flags = 0;
	}
}

GdcRleDC::~GdcRleDC()
{
	Empty();
}

void GdcRleDC::Empty()
{
	Length = 0;
	Alloc = 0;
	DeleteArray(Data);
	DeleteArray(ScanLine);
}

bool GdcRleDC::Create(int x, int y, GColourSpace cs, int flags)
{
	bool Status = GMemDC::Create(x, y, cs, flags);
	if (Status)
	{
		Flags &= ~GDC_RLE_READONLY;
	}
	return Status;
}

bool GdcRleDC::CreateInfo(int x, int y, GColourSpace cs)
{
	bool Status = false;

	DeleteObj(pMem);
	pMem = new GBmpMem;
	if (pMem)
	{
		pMem->x = x;
		pMem->y = y;
		pMem->Cs = cs;
		pMem->Base = 0;
		pMem->Line = 0;
		pMem->Flags = 0;

		Flags |= GDC_RLE_READONLY;

		if (cs == CsIndex8)
		{
			Flags |= GDC_RLE_MONO;
		}
	}

	return Status;
}

void GdcRleDC::ReadOnly(bool Read)
{
	if (Read)
	{
		Flags |= GDC_RLE_READONLY;
	}
	else
	{
		if (pMem)
		{
			pMem->Base = new uchar[pMem->Line * pMem->y];
		}
		else
		{
			// just in case... shouldn't ever happen
			// am I paranoid?
			// 'course :)
			Create(1, 1, System24BitColourSpace);
		}

		Flags &= ~GDC_RLE_READONLY;
	}
}

bool GdcRleDC::ReadOnly()
{
	return (Flags & GDC_RLE_READONLY) != 0;
}

void GdcRleDC::Mono(bool Mono)
{
	if (Mono)
	{
		Flags |= GDC_RLE_MONO;
		Flags &= ~GDC_RLE_COLOUR;
	}
	else
	{
		Flags &= ~GDC_RLE_MONO;
		Flags |= GDC_RLE_COLOUR;
	}
}

bool GdcRleDC::Mono()
{
	return (Flags & GDC_RLE_MONO) != 0;
}

bool GdcRleDC::SetLength(int Len)
{
	bool Status = true;

	if (Len + 1 > Alloc)
	{
		int NewAlloc = (Len + 0x4000) & (~0x3FFF);
		uchar *Temp = new uchar[NewAlloc];
		if (Temp)
		{
			if (NewAlloc > Length)
			{
				memset(Temp + Length, 0, NewAlloc - Length);
			}

			memcpy(Temp, Data, Length);
			DeleteArray(Data);
			DeleteArray(ScanLine);
			Data = Temp;
			Length = Len;
			Alloc = NewAlloc;
		}
		else
		{
			Status = false;
		}
	}
	else
	{
		// TODO: resize Data to be smaller if theres a
		// considerable size change
		Length = Len;
	}

	return Status;
}

void GdcRleDC::Update(int UpdateFlags)
{
	bool Error = false;

	if ( (UpdateFlags & GDC_BITS_CHANGE) &&
		!(Flags & GDC_RLE_READONLY))
	{
		Empty();

		COLOUR Key = Get(0, 0);
		ulong Pos = 0;
		ulong PixelSize = GetBits() / 8;
		
		for (ulong y=0; !Error && y<Y(); y++)
		{
			for (ulong x=0; x<X(); )
			{
				ulong Skip = 0;
				ulong Pixels = 0;
				uchar *Bits = 0;

				while (Get(x, y) == Key &&
						x < X())
				{
					Skip++;
					x++;
				}

				Bits = (*this)[y] + (x * PixelSize);
				while (Get(x, y) != Key && x < X())
				{
					Pixels++;
					x++;
				}

				if (Flags & GDC_RLE_COLOUR)
				{
					if (SetLength(	Length +
							(2 * sizeof(ulong)) +
							(Pixels * PixelSize)))
					{
						*((ulong*)(Data+Pos)) = Skip;
						Pos += sizeof(Skip);
						*((ulong*)(Data+Pos)) = Pixels;
						Pos += sizeof(Pixels);
						
						int l = Pixels * PixelSize;
						memcpy(Data+Pos, Bits, l);
						Pos += l;
					}
					else
					{
						Error = true;
						break;
					}
				}
				else if (Flags & GDC_RLE_MONO)
				{
					if (SetLength(	Length + (2 * sizeof(ulong))))
					{
						*((ulong*)(Data+Pos)) = Skip;
						Pos += sizeof(Skip);
						*((ulong*)(Data+Pos)) = Pixels;
						Pos += sizeof(Pixels);
					}
					else
					{
						Error = true;
						break;
					}
				}
			}
		}
	}

	if (Error)
	{
		Empty();
	}
	else
	{
		FindScanLines();
	}
}

bool GdcRleDC::FindScanLines()
{
	bool Status = false;
	bool Error = false;
	DeleteArray(ScanLine);

	if (Data)
	{
		ScanLine = new uchar*[Y()];
		if (ScanLine)
		{
			ulong Pos = 0;
			ulong PixelSize = GetBits() / 8;
			
			for (ulong y=0; !Error && y<Y(); y++)
			{
				ScanLine[y] = Data + Pos;
				ulong x;
				for (x=0; x<X();)
				{
					ulong Skip = 0;
					ulong Pixels = 0;

					Skip = *((ulong*)(Data + Pos));
					Pos += sizeof(Skip);
					Pixels = *((ulong*)(Data + Pos));
					Pos += sizeof(Pixels);

					if (Flags & GDC_RLE_COLOUR)
					{
						Pos += Pixels * PixelSize;
					}

					x += Pixels + Skip;
				}

				if (x != X() || Pos > Length)
				{
					Error = true;
				}
			}
		}

		if (Error)
		{
			DeleteArray(ScanLine);
		}
		else
		{
			Status = true;
		}
	}

	return Status;
}

void GdcRleDC::Draw(GSurface *Dest, int Ox, int Oy)
{
	int OriX, OriY;
	Dest->GetOrigin(OriX, OriY);
	Ox += OriX;
	Oy += OriY;

	if (Dest && Data)
	{
		bool ReBuildScanInfo = false;
		
		if (!ScanLine) ReBuildScanInfo = true;
		if (ScanLine && Data && ScanLine[0] != Data) ReBuildScanInfo = true;

		if (ReBuildScanInfo)
		{
			// need to rebuild scan line info
			if (!FindScanLines())
			{
				// couldn't create scan line info so quit
				return;
			}
		}

		GRect S;
		S.ZOff(X()-1, Y()-1);
		GRect D;
		D = S;
		D.Offset(Ox, Oy);
		GRect DClip = D;
		GRect Temp = Dest->ClipRgn();
		D.Bound(&Temp);

		if (DClip == D && (*Dest)[0])
		{
			// no clipping needed
			int PixLen = Dest->GetBits() / 8;

			if (Flags & GDC_RLE_COLOUR)
			{
				if (Dest->GetBits() == GetBits())
				{
					for (int y=0; y<Y(); y++)
					{
						uchar *s = ScanLine[y];
						uchar *d = (*Dest)[Oy+y];
						for (int x=0; x<X(); )
						{
							x += *((ulong*)s); s+=4;
							int Pixels = *((ulong*)s); s+=4;
							int Len = Pixels*PixLen;
							memcpy(d + ((Ox+x)*PixLen), s, Len);
							x += Pixels;
							s += Len;
						}
					}
				}
			}
			else if (Flags & GDC_RLE_MONO)
			{
				GApplicator *pDApp = Dest->Applicator();

				for (int y=0; y<Y(); y++)
				{
					uchar *s = ScanLine[y];
					for (int x=0; x<X(); )
					{
						x += *((ulong*)s); s+=4;
						int Pixels = *((ulong*)s); s+=4;
						pDApp->SetPtr(Ox+x, Oy+y);
						pDApp->Rectangle(Pixels, 1);
						x += Pixels;
					}
				}
			}
		}
		else if (DClip.Valid())
		{
			// clip
			int PixLen = Dest->GetBits() / 8;

			if (Flags & GDC_RLE_COLOUR)
			{
				if (Dest->GetBits() == GetBits())
				{
					for (int y=0; y<Y(); y++)
					{
						int Fy = Oy+y;
						if (Fy >= Temp.y1 && Fy < Temp.y2) // clip y
						{
							uchar *s = ScanLine[y];
							uchar *d = (*Dest)[Fy];
							for (int x=0; x<X(); )
							{
								x += *((ulong*)s); s+=4;
								int Pixels = *((ulong*)s); s+=4;

								int Fx = Ox+x;
								int PreClipPixels = max(0, Temp.x1 - Fx);
								int PostClipPixels = max(0, (Pixels + Fx) - Temp.x2 - 1);
								int PixelsLeft = Pixels - (PreClipPixels + PostClipPixels);
								if (PixelsLeft > 0) // clip x
								{
									memcpy(	d + ((Fx - PreClipPixels) * PixLen),
										s + (PreClipPixels * PixLen),
										PixelsLeft * PixLen);
								}

								x += Pixels;
								s += Pixels * PixLen;
							}
						}
					}
				}
			}
			/*
			else if (Flags & GDC_RLE_MONO)
			{
				GApplicator *pDApp = Dest->Applicator();

				for (int y=0; y<Y(); y++)
				{
					uchar *s = ScanLine[y];
					for (int x=0; x<X(); )
					{
						x += *((ulong*)s); s+=4;
						int Pixels = *((ulong*)s); s+=4;
						pDApp->SetPtr(Ox+x, Oy+y);
						pDApp->Rectangle(Pixels, 1);
						x += Pixels;
					}
				}
			}
			*/
		}
	}
}

int GdcRleDC::SizeOf()
{
	return (sizeof(int) * 5) + Length;
}

bool GdcRleDC::Read(GFile &F)
{
	Empty();
	
	int32 Len = 0, x = 0, y = 0, bits = 0;
	int8 Monochrome;
	F >> Len;
	if (SetLength(Len))
	{
		F >> Key;
		F >> x;
		F >> y;
		F >> bits;
		F >> Monochrome;
		Mono(Monochrome);
		CreateInfo(x, y, GBitsToColourSpace(bits));
		F.Read(Data, Len);
		return !F.GetStatus();
	}

	return false;
}

bool GdcRleDC::Write(GFile &F)
{
	if (Data)
	{
		char Monochrome = Mono();
		
		F << Length;
		F << Key;
		F << X();
		F << Y();
		F << GetBits();
		F << Monochrome;

		F.Write(Data, Length);

		return !F.GetStatus();
	}

	return false;
}

void GdcRleDC::Move(GdcRleDC *pDC)
{
	if (pDC)
	{
		Key = pDC->Key;
		Flags = pDC->Flags;
		Length = pDC->Length;
		Alloc = pDC->Alloc;
		Data = pDC->Data;
		ScanLine = pDC->ScanLine;
		pMem = pDC->pMem;
		pPalette = pDC->pPalette;

		pDC->pPalette = 0;
		pDC->pMem = 0;
		pDC->Length = 0;
		pDC->Alloc = 0;
		pDC->Data = 0;
		pDC->ScanLine = 0;
	}
}

////////////////////////////////////////////////////////////////////
GFilterFactory *GFilterFactory::First = 0;
GFilterFactory::GFilterFactory()
{
	// add this filter to the global list
	Next = First;
	First = this;
}

GFilterFactory::~GFilterFactory()
{
	// delete from the global list
	if (First == this)
	{
		First = First->Next;
	}
	else
	{
		GFilterFactory *i = First;
		while (i->Next && i->Next != this)
		{
			i = i->Next;
		}
		if (i->Next == this)
		{
			i->Next = Next;
		}
		else
		{
			// we aren't in the list
			// thats bad
			LgiAssert(0);
		}
	}
}

GFilter *GFilterFactory::New(const char *File, int Access, const uchar *Hint)
{
	GFilterFactory *i = First;
	while (i)
	{
		if (i->CheckFile(File, Access, Hint))
		{
			return i->NewObject();
		}
		i = i->Next;
	}
	return 0;
}

GFilter *GFilterFactory::NewAt(int n)
{
	int Status = 0;
	GFilterFactory *i = First;
	while (i)
	{
		if (Status++ == n)
		{
			return i->NewObject();
		}

		i = i->Next;
	}

	return 0;
}

int GFilterFactory::GetItems()
{
	int Status = 0;
	GFilterFactory *i = First;
	while (i)
	{
		Status++;
		i = i->Next;
	}
	return Status;
}

//////////////////////////////////////////////////////////////////////////

/// Legacy wrapper that calls the new method
GSurface *LoadDC(const char *Name, bool UseOSLoader)
{
    return GdcD->Load(Name, UseOSLoader);
}



GSurface *GdcDevice::Load(const char *Name, bool UseOSLoader)
{
	if (!FileExists(Name))
	{
		// Loading non-file resource...
		#if WINNATIVE
		GAutoWString WName(Utf8ToWide(Name));
		// a resource... lock and load gentlemen
		HRSRC hRsrc = FindResource(NULL, WName, RT_BITMAP);
		if (hRsrc)
		{
			int Size = SizeofResource(NULL, hRsrc);
			HGLOBAL hMem = LoadResource(NULL, hRsrc);
			if (hMem)
			{
				uchar *Ptr = (uchar*) LockResource(hMem);
				if (Ptr)
				{
					GClipBoard Clip(NULL);
					GSurface *pDC = Clip.ConvertFromPtr(Ptr);
					GlobalUnlock(hMem);
					return pDC;
				}
			}
		}
		#endif
		return NULL;
	}

	GFile File;
	if (!File.Open(Name, O_READ))
	{
		LgiTrace("%s:%i - Couldn't open '%s' for reading.\n", _FL, Name);
		return NULL;
	}

	return Load(&File, Name, UseOSLoader);
}

GSurface *GdcDevice::Load(GStream *In, const char *Name, bool UseOSLoader)
{
	if (!In)
		return NULL;

	GFilter::IoStatus FilterStatus = GFilter::IoError;

	int64 Size = In->GetSize();
	if (Size <= 0)
	{
		return NULL;
	}

	GAutoPtr<uchar, true> Hint(new uchar[16]);
	memset(Hint, 0, 16);
	if (In->Read(Hint, 16) == 0)
	{
		Hint.Reset(0);
	}

	int64 SeekResult = In->SetPos(0);
	if (SeekResult != 0)
	{
		LgiTrace("%s:%i - Seek failed after reading hint.\n", _FL);
		return NULL;
	}

	GAutoPtr<GFilter> Filter(GFilterFactory::New(Name, FILTER_CAP_READ, Hint));
	GAutoPtr<GSurface> pDC;
	if (Filter &&
		pDC.Reset(new GMemDC))
	{
		FilterStatus = Filter->ReadImage(pDC, In);
		if (FilterStatus != GFilter::IoSuccess)
		{
			pDC.Reset();
			LgiTrace("%s:%i - Filter couldn't cope with '%s'.\n", _FL, Name);
		}
	}

	if (UseOSLoader && !pDC)
	{
		#if LGI_SDL
		
		
		#elif defined MAC && !defined COCOA
		
		if (Name)
		{
			CFURLRef FileUrl = CFURLCreateFromFileSystemRepresentation(0, (const UInt8*)Name, strlen(Name), false);
			if (!FileUrl)
				LgiTrace("%s:%i - CFURLCreateFromFileSystemRepresentation failed.\n", _FL);
			else
			{
				CGImageSourceRef Src = CGImageSourceCreateWithURL(FileUrl, 0);
				if (!Src)
					LgiTrace("%s:%i - CGImageSourceCreateWithURL failed.\n", _FL);
				else
				{
					CGImageRef Img = CGImageSourceCreateImageAtIndex(Src, 0, 0);
					if (!Img)
						LgiTrace("%s:%i - CGImageSourceCreateImageAtIndex failed.\n", _FL);
					else
					{
						size_t x = CGImageGetWidth(Img);
						size_t y = CGImageGetHeight(Img);
						size_t bits = CGImageGetBitsPerPixel(Img);
						
						if (pDC.Reset(new GMemDC) &&
							pDC->Create(x, y, System32BitColourSpace))
						{
							pDC->Colour(0);
							pDC->Rectangle();
							
							CGRect r = {{0, 0}, {x, y}};
							CGContextDrawImage(pDC->Handle(), r, Img);
						}
						else
						{
							LgiTrace("%s:%i - pMemDC->Create failed.\n", _FL);
							pDC.Reset();
						}
						
						CGImageRelease(Img);
					}
					
					CFRelease(Src);
				}

				CFRelease(FileUrl);
			}
		}

		#elif WINNATIVE && defined(_MSC_VER)
		
		/*
		char *Ext = LgiGetExtension((char*)Name);
		if (Ext && stricmp(Ext, "gif") && stricmp(Ext, "png"))
		*/
		{
			IImgCtx *Ctx = 0;
			HRESULT hr = CoCreateInstance(CLSID_IImgCtx, NULL, CLSCTX_INPROC_SERVER, IID_IImgCtx, (LPVOID*)&Ctx);
			if (SUCCEEDED(hr))
			{
				GVariant Fn = Name;
				hr = Ctx->Load(Fn.WStr(), 0);
				if (SUCCEEDED(hr))
				{
					SIZE  Size = { -1, -1 };
					ULONG State = 0;
					int64 Start = LgiCurrentTime();
					while (LgiCurrentTime() - Start < 3000) // just in case it gets stuck....
					{
						hr = Ctx->GetStateInfo(&State, &Size, false);
						if (SUCCEEDED(hr))
						{
							if ((State & IMGLOAD_COMPLETE) || (State & IMGLOAD_STOPPED) || (State & IMGLOAD_ERROR))
							{
								break;
							}
							else
							{
								LgiSleep(10);
							}
						}
						else break;
					}

					if (Size.cx > 0 &&
						Size.cy > 0 &&
						pDC.Reset(new GMemDC))
					{
						if (pDC->Create(Size.cx, Size.cy, System24BitColourSpace))
						{
							HDC hDC = pDC->StartDC();
							if (hDC)
							{
								RECT rc = { 0, 0, pDC->X(), pDC->Y() };
								Ctx->Draw(hDC, &rc);
								pDC->EndDC();
							}

							if (pDC->GetBits() == 32)
							{
								// Make opaque
								int Op = pDC->Op(GDC_OR);
								pDC->Colour(Rgba32(0, 0, 0, 255), 32);
								pDC->Rectangle();
								pDC->Op(GDC_SET);
							}
						}
						else pDC.Reset();
					}
				}

				Ctx->Release();
			}
		}

		#endif

		if (pDC)
			return pDC.Release();
	}

	if (FilterStatus == GFilter::IoComponentMissing)
	{
		const char *c = Filter->GetComponentName();
		LgiAssert(c);
		if (c)
		{
			GToken t(c, ",");
			for (int i=0; i<t.Length(); i++)
				NeedsCapability(t[i]);
		}
	}

	#ifndef WIN32
	if (pDC)
	{
		int PromoteTo = GdcD->GetOption(GDC_PROMOTE_ON_LOAD);
		
		if (PromoteTo > 0 &&
			PromoteTo != pDC->GetBits())
		{
			GAutoPtr<GSurface> pOld;;
			GPalette *pPal = pDC->Palette();

			pOld = pDC;
			pDC.Reset(new GMemDC);
			if (pOld &&
				pDC &&
				pDC->Create(pOld->X(), pOld->Y(), GBitsToColourSpace(PromoteTo)))
			{
				pDC->Blt(0, 0, pOld);
				pOld.Reset();
			}
		}
		#ifdef BEOS
		else if (pDC->GetBits() == 8)
		{
			// Create remap table
			color_map *Map = (color_map*) system_colors();
			GPalette *Palette = pDC->Palette();
			if (Map && Palette)
			{
				char ReMap[256];
				for (int i=0; i<256; i++)
				{
					GdcRGB *p = (*Palette)[i];
					if (p)
					{
						COLOUR c = Rgb15(p->r, p->g, p->b);
						ReMap[i] = Map->index_map[c];

						p->r = Map->color_list[i].red;
						p->g = Map->color_list[i].green;
						p->b = Map->color_list[i].blue;
					}
					else
					{
						ReMap[i] = 0;
					}
				}

				// Remap colours to BeOS palette
				for (int y=0; y<pDC->Y(); y++)
				{
					uchar *d = (*pDC)[y];
					if (d)
					{
						for (int x=0; x<pDC->X(); x++)
						{
							d[x] = ReMap[d[x]];
						}
					}
				}
			}
		}
		#endif
	}
	#endif

	return pDC.Release();
}

bool WriteDC(const char *Name, GSurface *pDC)
{
    return GdcD->Save(Name, pDC);
}

bool GdcDevice::Save(const char *Name, GSurface *pDC)
{
	bool Status = false;

	if (Name && pDC)
	{
		GAutoPtr<GFilter> F(GFilterFactory::New(Name, FILTER_CAP_WRITE, 0));
		if (F)
		{
			GFile File;
			if (File.Open(Name, O_WRITE))
			{
				Status = F->WriteImage(&File, pDC);
			}
		}
		else
		{
			LgiTrace("No filter to write '%s'\n", Name);
		}
	}

	return Status;
}
