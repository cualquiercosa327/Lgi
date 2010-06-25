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

#ifndef WIN32
#define BI_RLE8       1L
#define BI_RLE4       2L
#else
#include "Lgi.h"
#include <objbase.h>
#include <IImgCtx.h>
#endif

int FindHeader(int Offset, char *Str, GStream *f)
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

	bool SetSize(GStream *s, long *Info, GSurface *pDC);

public:
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	Format GetFormat() { return FmtBmp; }

	/// Reads a BMP file
	bool ReadImage(GSurface *Out, GStream *In);
	
	/// Writes a Windows BMP file
	bool WriteImage(GStream *Out, GSurface *In);

	bool GetVariant(char *n, GVariant &v, char *a)
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
	bool CheckFile(char *File, int Access, uchar *Hint)
	{
		return (File) ? stristr(File, ".bmp") != 0 : false;
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
	uint16		Type;
	uint32		Size;
	uint16		Reserved1;
	uint16		Reserved2;
	uint32		OffsetToData;
};

class BMP_WININFO {
public:
	int32		Size;
	int32		Sx;
	int32		Sy;
	uint16		Planes;
	uint16		Bits;
	uint32		Compression;
	uint32		DataSize;
	int32		XPels;
	int32		YPels;
	uint32		ColoursUsed;
	uint32		ColourImportant;
};

class BMP_OS2INFO {
public:
	int32		Size;
	uint32		Sx;
	uint32		Sy;
	uint32		Planes;
	uint32		Bits;
};

#ifdef WIN32
#pragma pack(pop, before_pack)
#endif

bool GdcBmp::SetSize(GStream *s, long *Info, GSurface *pDC)
{
	bool Status = false;

	switch (*Info)
	{
		case sizeof(BMP_WININFO):
		{
			BMP_WININFO *i = (BMP_WININFO*) Info;
			ActualBits = i->Bits;
			ScanSize = BMPWIDTH(i->Sx * i->Bits);
			Status = pDC->Create(i->Sx, i->Sy, max(i->Bits, 8), ScanSize);
			break;
		}

		case sizeof(BMP_OS2INFO):
		{
			BMP_OS2INFO *i = (BMP_OS2INFO*) Info;
			ActualBits = i->Bits;
			ScanSize = BMPWIDTH(i->Sx * i->Bits);
			Status = pDC->Create(i->Sx, i->Sy, max(i->Bits, 8), ScanSize);
			s->SetPos(s->GetPos() + (sizeof(BMP_OS2INFO)-sizeof(BMP_WININFO)));
			break;
		}
	}

	return Status;
}

bool GdcBmp::ReadImage(GSurface *pDC, GStream *In)
{
	if (!pDC || !In)
		return false;

	bool Status = false;
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

	Read(In, &Info.Size, sizeof(Info.Size));
	Read(In, &Info.Sx, sizeof(Info.Sx));
	Read(In, &Info.Sy, sizeof(Info.Sy));
	Read(In, &Info.Planes, sizeof(Info.Planes));
	Read(In, &Info.Bits, sizeof(Info.Bits));
	Read(In, &Info.Compression, sizeof(Info.Compression));
	Read(In, &Info.DataSize, sizeof(Info.DataSize));
	Read(In, &Info.XPels, sizeof(Info.XPels));
	Read(In, &Info.YPels, sizeof(Info.YPels));
	Read(In, &Info.ColoursUsed, sizeof(Info.ColoursUsed));
	Read(In, &Info.ColourImportant, sizeof(Info.ColourImportant));

	if (File.Type == BMP_ID)
	{
		if (SetSize(In, (long*) &Info, pDC))
		{
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

			Status = true;

			if (Info.Compression == BI_RLE8)
			{
				// 8 bit RLE compressed image
				int Remaining = In->GetSize() - In->GetPos();
				uchar *Data = new uchar[Remaining];
				if (Data)
				{
					if (In->Read(Data, Remaining) == Remaining)
					{
						int x=0, y=pDC->Y()-1;
						uchar *p = Data;
						bool Done = false;

						while (!Done AND p < Data + Remaining)
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
										if (Pixel AND y >= 0 AND y < pDC->Y())
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
										if ((int) p & 1) p++;
										break;
									}
								}
							}
							else
							{
								// encoded mode
								uchar *Pixel = (*pDC)[y];
								if (Pixel AND y >= 0 AND y < pDC->Y())
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
				printf("%s:%i - BI_RLE4 not implemented.\n", __FILE__, __LINE__);
			}
			else
			{
				// Progress
				if (Meter)
				{
					Meter->SetDescription("scanlines");
					Meter->SetLimits(0, pMem->y-1);
				}

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
								Status &= In->Read(Buffer, ScanSize) == ScanSize;
								if (Status)
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
								Status &= In->Read(Buffer, ScanSize) == ScanSize;

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
					case 24:
					#ifdef XWIN
					{
						for (int i=pMem->y-1; i>=0; i--)
						{
							uchar *Ptr = pMem->Base + (pMem->Line * i);
							Status &= In->Read(Ptr, ScanSize) == ScanSize;

							ulong *Out = ((ulong*)Ptr) + (pDC->X()-1);
							uchar *In = Ptr + ((pDC->X()-1) * 3);
							for (int x=0; x<pDC->X(); x++)
							{
								*Out-- = Rgb32(In[2], In[1], In[0]);
								In -= 3;
							}
							
							if (Meter) Meter->Value(pMem->y-1-i);
						}
						break;
					}
					#endif
					case 8:
					case 16:
					case 32:
					{
						for (int i=pMem->y-1; i>=0; i--)
						{
						    // int64 Pos = In->GetPos();
						    int r = In->Read(pMem->Base + (pMem->Line * i), ScanSize);
						    // LgiTrace("Read scan %i = %i @ %I64i\n", i, r, Pos);
						    if (r != ScanSize)
						    {
							    Status = false;
							    break;
							}
							
							if (Meter) Meter->Value(pMem->y-1-i);
						}
						
						break;
					}
					default:
					{
						Status = false;
						printf("%s:%i - Bmp had unsupported bit depth.\n", __FILE__, __LINE__);
						break;
					}
				}
			}

			Cr.ZOff(pDC->X()-1, pDC->Y()-1);
			pDC->Update(GDC_BITS_CHANGE|GDC_PAL_CHANGE);
		}
		else
		{
			Cr.ZOff(-1, -1);
			printf("%s:%i - SetSize failed.\n", __FILE__, __LINE__);
		}
	}
	else
	{
		Cr.ZOff(-1, -1);
		printf("%s:%i - Bmp type check failed '%.2s'.\n", __FILE__, __LINE__, &File.Type);
	}

	pDC->ClipRgn(&Cr);
	
	return Status;
}

bool GdcBmp::WriteImage(GStream *Out, GSurface *pDC)
{
	bool Status = false;
	
	if (!pDC || !Out)
		return false;

	BMP_FILE File;
	BMP_WININFO Info;
	GBmpMem *pMem = GetSurface(pDC);
	int Colours = (pMem->Bits <= 8) ? 1 << pMem->Bits : 0;
	int UsedBits = pMem->Bits;

	GPalette *Palette = pDC->Palette();
	if (pMem->Bits <= 8 AND
		Palette)
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

	if (!pMem OR
		!pMem->x OR
		!pMem->y)
	{
		return false;
	}

	Out->SetSize(0);

	File.Type = BMP_ID;
	File.OffsetToData = sizeof(BMP_FILE) + sizeof(BMP_WININFO) + (Colours * 4);
	File.Size = File.OffsetToData + (pMem->Line * pMem->y);
	File.Reserved1 = 0;
	File.Reserved2 = 0;

	Info.Size = sizeof(BMP_WININFO);
	Info.Sx = pMem->x;
	Info.Sy = pMem->y;
	Info.Planes = 1;
	Info.Bits = UsedBits;
	Info.Compression = 0;
	Info.DataSize = 0;
	Info.XPels = 3000;
	Info.YPels = 3000;
	Info.ColoursUsed = Colours;
	Info.ColourImportant = Colours;

	if (Out->Write(&File, sizeof(File)) &&
		Out->Write(&Info, sizeof(Info)))
	{
		if (pMem->Bits <= 8)
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
		Status = true;
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
						Status &= Out->Write(Buffer, Bytes) == Bytes;
						
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
						Status &= Out->Write(Buffer, Bytes) == Bytes;
						
						// update status
						if (Meter) Meter->Value(pMem->y-1-i);
					}

					DeleteArray(Buffer);
				}
				break;
			}
			default:
			{
				for (int i=pMem->y-1; i>=0; i--)
				{
					// int64 Pos = Out->GetPos();
					int w = Out->Write(pMem->Base + (i * pMem->Line), Bytes);
					// LgiTrace("Write scan %i = %i @ %I64i\n", i, w, Pos);
					if (w != Bytes)
					{
						Status = false;
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
	bool ReadImage(GSurface *pDC, GStream *In);
	bool WriteImage(GStream *Out, GSurface *pDC);
	bool GetVariant(char *n, GVariant &v, char *a);
};

class GdcIcoFactory : public GFilterFactory
{
	bool CheckFile(char *File, int Access, uchar *Hint)
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

bool GdcIco::GetVariant(char *n, GVariant &v, char *a)
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

bool GdcIco::ReadImage(GSurface *pDC, GStream *In)
{
	bool Status = false;
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
		In->SetPos(In->GetPos() + ImageOffset);

		BMP_WININFO	Header;
		GdcRGB *Colours;
		uchar *XorBytes = 0;
		uchar *AndBytes = 0;

		LgiAssert(sizeof(Header) == 40);
		In->Read(&Header, sizeof(Header));
		BytesLeft -= sizeof(Header);

		int Cols = 1 << (Header.Bits);
		Colours = new GdcRGB[Cols];
		if (Colours)
		{
			In->Read(Colours, sizeof(GdcRGB) * Cols);
			BytesLeft -= sizeof(GdcRGB) * Cols;
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
		printf("BytesInRes=%i, Xor=%i, And=%i, Pal=%i, BytesLeft=%i\n",
			BytesInRes,
			XorSize,
			AndSize,
			sizeof(GdcRGB) * Cols,
			BytesLeft);
		*/

		if (Colours AND
			XorBytes AND
			(Header.Bits > MyBits OR Width > pDC->X() OR Height > pDC->Y()) AND
			pDC->Create(Width, Height, max(8, Header.Bits) ))
		{
			MyBits = Header.Bits;
			pDC->Colour(0, 24);
			pDC->Rectangle();

			GPalette *Pal = new GPalette;
			if (Pal)
			{
				Pal->SetSize(Cols);
				memcpy((*Pal)[0], Colours, sizeof(GdcRGB) * Cols);
				Pal->SwapRAndB();
				pDC->Palette(Pal, true);
			}
			
			if (AndBytes)
			{
				pDC->IsAlpha(true);
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
						Status = true;
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
						Status = true;
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

						Status = true;
						break;
					}
				}
			}
		}
		else
		{
			printf("%s:%i - Header size error: %i != %i + %i, Img: %ix%i @ %i bits\n",
				__FILE__, __LINE__,
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

bool GdcIco::WriteImage(GStream *Out, GSurface *pDC)
{
	bool Status = false;

	if (!pDC || pDC->GetBits() > 8 || !Out)
		return false;

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

	int32	ImageOffset = Out->GetPos() + sizeof(ImageOffset);
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

				Status = true;
				break;
			}
			case 8:
			{
				for (int x=0; x<Width; x++)
				{
					uchar Dest = (Back == Src[x]) ? 0 : Src[x];
					Write(Out, &Dest, sizeof(Dest));
				}
				Status = true;
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

		Status = true;
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
		pMem->Bits = 0;
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

bool GdcRleDC::Create(int x, int y, int bits, int LineLen)
{
	bool Status = GMemDC::Create(x, y, bits, LineLen);
	if (Status)
	{
		Flags &= ~GDC_RLE_READONLY;
	}
	return Status;
}

bool GdcRleDC::CreateInfo(int x, int y, int bits)
{
	bool Status = false;

	DeleteObj(pMem);
	pMem = new GBmpMem;
	if (pMem)
	{
		pMem->x = x;
		pMem->y = y;
		pMem->Bits = bits;
		pMem->Base = 0;
		pMem->Line = 0;
		pMem->Flags = 0;

		Flags |= GDC_RLE_READONLY;

		if (bits == 8)
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
			Create(1, 1, 24);
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

	if (	(UpdateFlags & GDC_BITS_CHANGE) AND
		!(Flags & GDC_RLE_READONLY))
	{
		Empty();

		COLOUR Key = Get(0, 0);
		ulong Pos = 0;
		ulong PixelSize = GetBits() / 8;
		
		for (ulong y=0; !Error AND y<Y(); y++)
		{
			for (ulong x=0; x<X(); )
			{
				ulong Skip = 0;
				ulong Pixels = 0;
				uchar *Bits = 0;

				while (Get(x, y) == Key AND x < X())
				{
					Skip++;
					x++;
				}

				Bits = (*this)[y] + (x * PixelSize);
				while (Get(x, y) != Key AND x < X())
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
			
			for (ulong y=0; !Error AND y<Y(); y++)
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

				if (x != X() OR Pos > Length)
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

	if (Dest AND Data)
	{
		bool ReBuildScanInfo = false;
		
		if (!ScanLine) ReBuildScanInfo = true;
		if (ScanLine AND Data AND ScanLine[0] != Data) ReBuildScanInfo = true;

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

		if (DClip == D AND (*Dest)[0])
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
						if (Fy >= Temp.y1 AND Fy < Temp.y2) // clip y
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
		CreateInfo(x, y, bits);
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
		while (i->Next AND i->Next != this)
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

GFilter *GFilterFactory::New(char *File, int Access, uchar *Hint)
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
GSurface *LoadDC(char *Name, bool UseOSLoader)
{
	GAutoPtr<GSurface> pDC;

	if (UseOSLoader)
	{
		#if defined MAC
		
		CFURLRef FileUrl = CFURLCreateFromFileSystemRepresentation(0, (const UInt8*)Name, strlen(Name), false);
		if (!FileUrl)
			printf("%s:%i - CFURLCreateFromFileSystemRepresentation failed.\n", _FL);
		else
		{
			CGImageSourceRef Src = CGImageSourceCreateWithURL(FileUrl, 0);
			if (!Src)
				printf("%s:%i - CGImageSourceCreateWithURL failed.\n", _FL);
			else
			{
				CGImageRef Img = CGImageSourceCreateImageAtIndex(Src, 0, 0);
				if (!Img)
					printf("%s:%i - CGImageSourceCreateImageAtIndex failed.\n", _FL);
				else
				{
					size_t x = CGImageGetWidth(Img);
					size_t y = CGImageGetHeight(Img);
					size_t bits = CGImageGetBitsPerPixel(Img);
					
					if (pDC.Reset(new GMemDC) &&
						pDC->Create(x, y, 32))
					{
						pDC->Colour(0);
						pDC->Rectangle();
						
						CGRect r = {{0, 0}, {x, y}};
						CGContextDrawImage(pDC->Handle(), r, Img);
					}
					else
					{
						printf("%s:%i - pMemDC->Create failed.\n", _FL);
						pDC.Reset();
					}
					
					CGImageRelease(Img);
				}
				
				CFRelease(Src);
			}

			CFRelease(FileUrl);
		}

		#elif WIN32NATIVE

		char *Ext = LgiGetExtension(Name);
		if (Ext && stricmp(Ext, "gif") && stricmp(Ext, "png"))
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
						if (pDC->Create(Size.cx, Size.cy, 24))
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

	GFile File;
	if (File.Open(Name, O_READ))
	{
		GAutoPtr<uchar, true> Hint(new uchar[16]);
		memset(Hint, 0, 16);
		if (File.Read(Hint, 16) == 0)
		{
			Hint.Reset(0);
		}

		File.SetPos(0);

		GAutoPtr<GFilter> Filter(GFilterFactory::New(Name, FILTER_CAP_READ, Hint));
		if (Filter)
		{
			if (pDC.Reset(new GMemDC) && !Filter->ReadImage(pDC, &File))
			{
				pDC.Reset();
				LgiTrace("%s:%i - Filter couldn't cope with '%s'.\n", _FL, Name);
			}
		}
	}
	else
	{
		LgiTrace("%s:%i - Couldn't open '%s' for reading.\n", _FL, Name);
	}
	
	#if WIN32NATIVE
	if (!pDC)
	{
		// a resource... lock and load gentlemen
		HRSRC hRsrc = FindResource(NULL, Name, RT_BITMAP);
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
					pDC.Reset(Clip.ConvertFromPtr(Ptr));
					GlobalUnlock(hMem);
				}
			}
		}
	}
	#else
	if (pDC)
	{
		int PromoteTo = GdcD->GetOption(GDC_PROMOTE_ON_LOAD);
		
		if (PromoteTo > 0 AND
			PromoteTo != pDC->GetBits())
		{
			GAutoPtr<GSurface> pOld;;
			GPalette *pPal = pDC->Palette();

			pOld = pDC;
			pDC.Reset(new GMemDC);
			if (pOld &&
				pDC &&
				pDC->Create(pOld->X(), pOld->Y(), PromoteTo))
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
			if (Map AND Palette)
			{
				char ReMap[256];
				for (int i=0; i<256; i++)
				{
					GdcRGB *p = (*Palette)[i];
					if (p)
					{
						COLOUR c = Rgb15(p->R, p->G, p->B);
						ReMap[i] = Map->index_map[c];

						p->R = Map->color_list[i].red;
						p->G = Map->color_list[i].green;
						p->B = Map->color_list[i].blue;
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

bool WriteDC(char *Name, GSurface *pDC)
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
	}

	return Status;
}
