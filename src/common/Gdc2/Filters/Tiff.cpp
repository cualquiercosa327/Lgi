/*hdr
**      FILE:           Tiff.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           9/10/98
**      DESCRIPTION:    Tiff filter
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#ifdef WIN32
#include <conio.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "Lgi.h"
#include "Lzw.h"
#include "GVariant.h"

// TIFF
#define TIFF_USE_LZW

class TiffIo : public GFilter
{
public:
	GStream *s;

	TiffIo()
	{
		s = 0;
	}

	Format GetFormat()
	{
		return FmtTiff;
	}

	bool GetSwap()
	{
		return false;
	}

	bool Read(void *p, int len)
	{
		return GFilter::Read(s, p, len);
	}

	bool Write(const void *p, int len)
	{
		return GFilter::Write(s, p, len);
	}
};

class IFD
{
	void *Data;

public:
	ushort	Tag;
	ushort	Type;
	ulong	Count;
	ulong	Offset;

	IFD();
	~IFD();
	void *GetData();
	char *Str();
	int ElementSize();
	int Value();
	int ArrayValue(int i);
	bool Read(TiffIo &f);
	bool Write(TiffIo &f);
};

class GdcTiff : public TiffIo
{
	List<IFD> Blocks;
	IFD *FindTag(ushort n);
	int ScanLength;

	#ifdef TIFF_USE_LZW
	Lzw Lib;
	#endif

	bool ProcessRead(GSurface *pDC);
	bool ProcessWrite(GSurface *pDC);

public:
	GView *Parent;
	OsView Handle();

	GdcTiff();
	~GdcTiff();

	int GetCapabilites() { return FILTER_CAP_READ; }
	bool ReadImage(GSurface *pDC, GStream *In);
	bool WriteImage(GStream *Out, GSurface *pDC);

	bool GetVariant(const char *n, GVariant &v, char *a)
	{
		if (!stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Tiff";
		}
		else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "TIF,TIFF";
		}
		else return false;

		return true;
	}
};

// Object Factory
class GdcTiffFactory : public GFilterFactory
{
	bool CheckFile(char *File, int Access, uchar *Hint)
	{
		if (Access == FILTER_CAP_READ)
		{
			return (File) ? stristr(File, ".tiff") != 0 OR
							stristr(File, ".tif") != 0 : false;
		}

		return false;
	}

	GFilter *NewObject()
	{
		return new GdcTiff;
	}

} TiffFactory;

// Tiff tag numbers
#define TAG_ImageX						0x100
#define TAG_ImageY						0x101
#define TAG_Bits						0x102
#define TAG_Palette						0x140

#define TAG_Compression					0x103
#define TAG_RowsPerStrip				0x116
#define TAG_StripOffsets				0x111
#define TAG_StripByteOffsets			0x117
#define TAG_PhotometricInterpretation	0x106 // PHOTOMETRIC_????
#define TAG_Predictor					0x13D

#define PHOTOMETRIC_MINISWHITE			0
#define PHOTOMETRIC_MINISBLACK			1
#define PHOTOMETRIC_RGB					2
#define PHOTOMETRIC_PALETTE				3
#define PHOTOMETRIC_MASK				4
#define PHOTOMETRIC_CMYK				5
#define PHOTOMETRIC_YCBCR				6
#define PHOTOMETRIC_CIELAB				8
#define PHOTOMETRIC_ICCLAB				9
#define PHOTOMETRIC_ITULAB				10
#define PHOTOMETRIC_LOGL				32844
#define PHOTOMETRIC_LOGLUV				32845

// Tiff tag data types
#define TYPE_UBYTE						1
#define TYPE_USHORT						3
#define TYPE_ULONG						4

#define TYPE_SBYTE						6
#define TYPE_SSHORT						8
#define TYPE_SLONG						9

#define TYPE_ASCII						2
#define TYPE_URATIONAL					5
#define TYPE_UNDEFINED					7
#define TYPE_SRATIONAL					10
#define TYPE_FLOAT						11
#define TYPE_DOUBLE						12

////////////////////////////////////////////////////////////////////////////////////////////
IFD::IFD()
{
	Data = 0;
}

IFD::~IFD()
{
	DeleteArray((uchar*&)Data);
}

void *IFD::GetData()
{
	return (Data) ? Data : &Offset;
}

int IFD::ElementSize()
{
	switch (Type)
	{
		case TYPE_UBYTE:
		case TYPE_SBYTE:
		case TYPE_ASCII:
		case TYPE_UNDEFINED:
		{
			return 1;
		}
		case TYPE_USHORT:
		case TYPE_SSHORT:
		{
			return 2;
		}
		case TYPE_ULONG:
		case TYPE_SLONG:
		case TYPE_FLOAT:
		{
			return 4;
		}
		case TYPE_URATIONAL:
		case TYPE_SRATIONAL:
		case TYPE_DOUBLE:
		{
			return 8;
		}
	}

	return 0;
}

char *IFD::Str()
{
	if (Data)
	{
		return (char*) Data;
	}
	return (char*) &Offset;
}

int IFD::Value()
{
	if (Count == 1)
	{
		switch (Type)
		{
			case TYPE_UBYTE:
			{
				return (int) ((uchar) (Offset & 0xFF));
			}
			case TYPE_USHORT:
			{
				return (int) ((ushort) (Offset & 0xFFFF));
			}
			case TYPE_ULONG:
			{
				return (int) ((ulong) Offset);
			}
			case TYPE_SBYTE:
			{
				return (int) ((signed char) (Offset & 0xFF));
			}
			case TYPE_SSHORT:
			{
				return (int) ((short) (Offset & 0xFFFF));
			}
			case TYPE_SLONG:
			{
				return (int) ((long) Offset);
			}
			case TYPE_ASCII:
			{
				char *s = Str();
				return (s) ? atoi(s) : 0;
			}
			case TYPE_URATIONAL:
			case TYPE_UNDEFINED:
			case TYPE_SRATIONAL:
			case TYPE_FLOAT:
			case TYPE_DOUBLE:
			{
				break;
			}
		}
	}
	return 0;
}

int IFD::ArrayValue(int i)
{
	void *p = GetData();
	
	if (i < Count AND p)
	{
		switch (Type)
		{
			case TYPE_ASCII:
			case TYPE_UBYTE:
			{
				return (int) ((uchar*)p)[i];
			}
			case TYPE_USHORT:
			{
				return (int) ((ushort*)p)[i];
			}
			case TYPE_ULONG:
			{
				return (int) ((ulong*)p)[i];
			}
			case TYPE_SBYTE:
			{
				return (int) ((signed char*)p)[i];
			}
			case TYPE_SSHORT:
			{
				return (int) ((short*)p)[i];
			}
			case TYPE_SLONG:
			{
				return (int) ((long*)p)[i];
			}
		}
	}
	else
	{
		printf("%s:%i - IFD::ArrayValue(%i) error, Count=%i p=%p\n",
			__FILE__, __LINE__,
			i, Count, p);
	}
	
	return 0;
}

bool IFD::Read(TiffIo &f)
{
	f.Read(&Tag, sizeof(Tag));
	f.Read(&Type, sizeof(Type));
	f.Read(&Count, sizeof(Count));
	f.Read(&Offset, sizeof(Offset));

	int Size = Count * ElementSize();
	if (Size > 4)
	{
		Data = new uchar[Size];
		if (Data)
		{
			int Pos = f.s->GetPos();
			f.s->SetPos(Offset);

			switch (Type)
			{
				case TYPE_USHORT:
				{
					uint16 *p = (uint16*)Data;
					for (int i=0; i<Count; i++)
					{
						f.Read(p+i, sizeof(p[i]));
					}
					break;
				}
				case TYPE_ULONG:
				{
					uint32 *p = (uint32*)Data;
					for (int i=0; i<Count; i++)
					{
						f.Read(p+i, sizeof(p[i]));
					}
					break;
				}
				case TYPE_SSHORT:
				{
					int16 *p = (int16*)Data;
					for (int i=0; i<Count; i++)
					{
						f.Read(p+i, sizeof(p[i]));
					}
					break;
				}
				case TYPE_SLONG:
				{
					int32 *p = (int32*)Data;
					for (int i=0; i<Count; i++)
					{
						f.Read(p+i, sizeof(p[i]));
					}
					break;
				}
				default:
				{
					f.s->Read(Data, Size);
					break;
				}
			}
			f.s->SetPos(Pos);
		}
	}
	else if (f.GetSwap())
	{
		switch (Type)
		{
			case TYPE_UBYTE:
			case TYPE_SBYTE:
			{
				Offset >>= 24;
				break;
			}
			case TYPE_USHORT:
			case TYPE_SSHORT:
			{
				Offset >>= 16;
				break;
			}
		}
	}

	return true;
}

bool IFD::Write(TiffIo &f)
{
	f.Write(&Tag, sizeof(Tag));
	f.Write(&Type, sizeof(Type));
	f.Write(&Count, sizeof(Count));
	f.Write(&Offset, sizeof(Offset));
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
GdcTiff::GdcTiff()
{
	Parent = 0;
}

GdcTiff::~GdcTiff()
{
}

OsView GdcTiff::Handle()
{
	return (Parent) ? Parent->Handle() : 0;
}

IFD *GdcTiff::FindTag(ushort n)
{
	for (IFD *i = Blocks.First(); i; i = Blocks.Next())
	{
		if (i->Tag == n)
		{
			return i;
		}
	}

	return NULL;
}

class TiffPipe : public GBytePipe
{
	Progress *p;
	int Scansize;
	int Last;
	int64 Size;

public:
	TiffPipe(Progress *prog, int scansize, int size) : GBytePipe(size)
	{
		p = prog;
		Last = 0;
		Size = 0;
		Scansize = scansize;
	}

	int Write(const void *buf, int size, int flags)
	{
		int Status = GBytePipe::Write(buf, size, flags);
		Size += size;

		if (p)
		{
			int y = Size / Scansize;
			if (y > Last + 64)
			{
				p->Value(y);
				Last = y;

				LgiTrace("y=%i\n", y);
			}
		}

		return Status;
	}
};

bool GdcTiff::ProcessRead(GSurface *pDC)
{
	bool Status = false;
	bool Error = false;
	IFD *x = FindTag(TAG_ImageX);
	IFD *y = FindTag(TAG_ImageY);
	IFD *BitTag = FindTag(TAG_Bits);
	int Bits = 1;
	int Pos = s->GetPos();

	if (BitTag)
	{
		switch (BitTag->Count)
		{
			case 1:
			{
				Bits = BitTag->Value();
				break;
			}
			case 3:
			{
				ushort *Depth = (ushort*) BitTag->GetData();
				if (Depth AND
					BitTag->Type == TYPE_USHORT)
				{
					// Baseline RGB image
					Bits = Depth[0] + Depth[1] + Depth[2];
				}
				break;
			}
			case 4:
			{
				// Maybe a 32-bit image?
				ushort *Depth = (ushort*) BitTag->GetData();
				if (Depth AND
					BitTag->Type == TYPE_USHORT)
				{
					// CMYK image?
					Bits = Depth[0] + Depth[1] + Depth[2] + Depth[3];
				}
				break;
			}
		}
	}

	int X = x ? x->Value() : 0;
	int Y = y ? y->Value() : 0;
	int B = 0;
	bool DownSam = false;
	
	if (Bits > 32)
	{
		B = 32;
		DownSam = true;
	}
	else
	{
		B = Bits >= 8 ? Bits : 8;
	}
	
	#ifdef MAC
	if (B == 24)
		B = 32;
	#endif

	if (pDC AND
		pDC->Create(X, Y, B))
	{
		if (Meter)
		{
			// Set the meter block to the size of a scanline
			Lib.MeterBlock = (X * Bits) >> (DownSam ? 2 : 3);
			Lib.Meter = Meter;
			Meter->SetDescription("scanlines");
			Meter->SetLimits(0, Y-1);
		}

		pDC->Colour(0);
		pDC->Rectangle();

		// Read Palette
		IFD *Palette = FindTag(TAG_Palette);
		if (Palette)
		{
			ushort *s = (ushort*) Palette->GetData();
			if (s AND Palette->Type == TYPE_USHORT)
			{
				int Colours = Palette->Count / 3;
				GPalette *Pal = new GPalette(0, Colours);
				if (Pal)
				{
					for (int i=0; i<Colours; i++)
					{
						GdcRGB *d = (*Pal)[i];
						if (d)
						{
							d->R = s[(0*Colours)+i] >> 8;
							d->G = s[(1*Colours)+i] >> 8;
							d->B = s[(2*Colours)+i] >> 8;
						}
					}

					pDC->Palette(Pal);
				}
			}
		}

		// Read Image
		IFD *Compression = FindTag(TAG_Compression);
		IFD *RowsPerStrip = FindTag(TAG_RowsPerStrip);
		IFD *StripOffsets = FindTag(TAG_StripOffsets);
		IFD *StripByteCounts = FindTag(TAG_StripByteOffsets);
		IFD *PhotometricInterpretation = FindTag(TAG_PhotometricInterpretation);
		if (Compression AND
			StripOffsets AND
			StripByteCounts)
		{
			int Comp = Compression->Value();
			int Rows = RowsPerStrip ? RowsPerStrip->Value() : Y;
			int Interpretation = PhotometricInterpretation->Value();
			int Cy = 0;

			switch (Bits)
			{
				case 1:
				case 8:
				case 24:
				case 32:
				case 64:
				{
					int Strip = 0;
					ScanLength = ((pDC->X() * Bits) + 7) / 8;
					
					for (	;
							!Error AND
							Cy<pDC->Y() AND
							Strip < StripOffsets->Count;
							Strip++)
					{
						int Offset = StripOffsets->ArrayValue(Strip);
						if (Offset)
						{
							Lib.SetBufSize(Offset);
						}
						else
						{
							Error = true;
							break;
						}
						
						int Limit = min(pDC->Y() - Cy, Rows);
						s->SetPos(Offset);
						
						switch (Comp)
						{
							case 1: // uncompressed
							{
								for (int i=0; i<Limit; i++, Cy++)
								{
									uchar *d = (*pDC)[Cy];
									if (d)
									{
										s->Read(d, ScanLength);
									}
								}
								break;
							}
							case 32773: // Pack-bits RLE
							{
								for (int i=0; i<Limit; i++, Cy++)
								{
									uchar n;
									uchar c;
									uchar *d = (*pDC)[Cy];
									if (d)
									{
										// unpack data
										for (uchar *e=d+ScanLength; d<e; )
										{
											Read(&n, sizeof(n));
											if (n & 0x80)
											{
												// copy run
												Read(&c, sizeof(c));
												n = -n+1;
												memset(d, c, n);
											}
											else
											{
												// copy data
												n++;
												s->Read(d, n);
											}
											d += n;
										}
									}
								}
								break;
							}
							case 2: // modified huffman
							{
								if (Props)
								{
									GVariant v;
									Props->SetValue(LGI_FILTER_ERROR, v = "This image uses a modified huffman compression,\n"
																		"Which is not supported yet.\n");
								}
								Error = true;
								break;
							}
							case 5: // LZW compression
							{
								#ifdef TIFF_USE_LZW
								
								TiffPipe Out(Meter, ScanLength, 512 << 10);
								if (Lib.Decompress(&Out, s))
								{
									// int Bytes = Out.GetSize();
									uchar *FullRez = (DownSam) ? new uchar[ScanLength] : 0;

									for (; Out.GetSize() >= ScanLength; Cy++)
									{
										uchar *o = (*pDC)[Cy];
										if (o)
										{
											if (DownSam)
											{
												if (FullRez)
												{
													Out.Read(FullRez, ScanLength);
													uint16 *s = (uint16*) FullRez;
													uint16 *e = s + (pDC->X() * 4);
													uchar *d = o;
													while (s < e)
													{
														*d++ = *s++ >> 8;
													}
												}
											}
											else
											{
												Out.Read(o, ScanLength);
											}
										}
									}
									
									DeleteArray(FullRez);
								}
								else
								{
									printf("%s:%i - LZW decompress failed.\n", __FILE__, __LINE__);
								}
								
								#else
								Props.Set(OptErrMsg,
										"This image uses LZW, which is unsupported due to\r\n"
										"UniSys requiring a royalty for it's patented algorithm.\r\n"
										"\r\n"
										"Use PNG instead.");
								Error = true;
								#endif
								break;
							}
							default:
							{
								if (Props)
								{
									char Msg[256];
									sprintf(Msg, "This image uses an unsupported TIFF compression method: %i", Comp);
									GVariant v = Msg;
									Props->SetValue(LGI_FILTER_ERROR, v);
								}
								Error = true;
								
								printf("%s:%i - Unknown compression '%i'\n", __FILE__, __LINE__, Comp);
								break;
							}
						}
					}

					if (Bits == 1)
					{
						// Unpack bits into 8 bit pixels
						for (int y=0; y<Cy; y++)
						{
							uchar *i = (*pDC)[y];
							for (int x = pDC->X()-1; x >= 0; x--)
							{
								i[x] =	(
											i[x >> 3]
											&
											(1 << (7 - (x & 7)))
										)
										?
										0
										:
										0xff;
							}
						}
					}
					else if (B == 32 && Bits == 24)
					{
						// Upconvert Rgb24 to Rgb32
						for (int y=0; y<Cy; y++)
						{
							Pixel32 *d = (Pixel32*)(*pDC)[y];
							Pixel24 *s = (Pixel24*)(*pDC)[y];
							Pixel32 *e = d;
							d += pDC->X() - 1;
							s += pDC->X() - 1;
							while (d >= e)
							{
								d->r = s->r;
								d->g = s->g;
								d->b = s->b;
								d->a = 255;
								s--;
								d--;
							}
						}
					}

					if (Interpretation == PHOTOMETRIC_RGB AND
						pDC->GetBits() >= 24)
					{
						if (pDC->GetBits() == 24)
						{
							class Tiff24
							{
							public:
								uint8 r, g, b;
							};
							
							// swap R and B
							for (int y=0; y<Cy; y++)
							{
								Pixel24 *e = (Pixel24*) (*pDC)[y];
								Pixel24 *d = e;
								Tiff24 *s = (Tiff24*) (*pDC)[y];
								
								((uchar*&)d) += Pixel24::Size * (pDC->X()-1);
								s += pDC->X() - 1;
								
								while (d >= e)
								{
									Tiff24 t = *s;

									d->r = t.r;
									d->g = t.g;
									d->b = t.b;
									
									s--;
									((uchar*&)d) -= Pixel24::Size;
								}
							}
						}
						else if (pDC->GetBits() == 32)
						{
							class Tiff32
							{
							public:
								uint8 r, g, b, a;
							};
							
							// swap R and B
							for (int y=0; y<Cy; y++)
							{
								Pixel32 *e = (Pixel32*) (*pDC)[y];
								Pixel32 *d = e;
								Tiff32 *s = (Tiff32*) (*pDC)[y];
								
								((uchar*&)d) += Pixel32::Size * (pDC->X()-1);
								s += pDC->X() - 1;
								
								while (d >= e)
								{
									Tiff32 t = *s;

									d->r = t.r;
									d->g = t.g;
									d->b = t.b;
									
									s--;
									((uchar*&)d) -= Pixel32::Size;
								}
							}
						}
					}
					
					if (Interpretation == PHOTOMETRIC_CMYK AND
						pDC->GetBits() == 32)
					{
						class TiffCmyk
						{
						public:
							uint8 c, m, y, k;
						};

						// 32 bit CMYK -> RGBA conversion
						uchar *Div = Div255Lut;
						for (int y=0; y<Cy; y++)
						{
							Pixel32 *d = (Pixel32*) (*pDC)[y];
							Pixel32 *e = d + pDC->X();
							TiffCmyk *s = (TiffCmyk*) (*pDC)[y];
							
							while (d < e)
							{
								int C = Div[s->c * (255 - s->k)] + s->k;
								int M = Div[s->m * (255 - s->k)] + s->k;
								int Y = Div[s->y * (255 - s->k)] + s->k;

								d->r = 255 - C;
								d->g = 255 - M;
								d->b = 255 - Y;
								d->a = 255;

								s++;
								d++;
							}
						}
					}

					IFD *Predictor = FindTag(TAG_Predictor);
					if (Predictor AND Predictor->Value() == 2)
					{
						switch (Bits)
						{
							case 8:
							{
								for (int y=0; y<Cy; y++)
								{
									uchar *d = (*pDC)[y];
									for (int x=1; x<pDC->X(); x++, d++)
									{
										d[0] += d[-1];
									}
								}
								break;
							}
							case 24:
							{
								for (int y=0; y<Cy; y++)
								{
									uchar *d = (*pDC)[y] + 3;
									for (int x=1; x<pDC->X(); x++, d+=3)
									{
										d[0] += d[-3];
										d[1] += d[-2];
										d[2] += d[-1];
									}
								}
								break;
							}
						}
					}
					break;
				}
				default:
				{
					if (Props)
					{
						char Msg[256];
						sprintf(Msg, "Image currently doesn't support %i bit TIFF files", Bits);
						GVariant v = Msg;
						Props->SetValue(LGI_FILTER_ERROR, v);
					}
					Error = true;
					break;
				}
			}

			Status = !Error;
		}
		else
		{
			printf("%s:%i - Tag missing Compression=%p RowsPerStrip=%p StripOffsets=%p StripByteCounts=%p.\n",
				__FILE__, __LINE__,
				Compression,
				RowsPerStrip,
				StripOffsets,
				StripByteCounts);
		}
	}
	else
	{
		if (Props)
		{
			char Msg[256];
			sprintf(Msg, "Couldn't create bitmap of size %ix%i @ %i bpp.", X, Y, B);
			GVariant v = Msg;
			Props->SetValue(LGI_FILTER_ERROR, v);
		}
		Error = true;
	}

	s->SetPos(Pos);

	return Status;
}

bool GdcTiff::ReadImage(GSurface *pDC, GStream *In)
{
	bool Status = false;
	ushort n16;

	if (!pDC || !In)
		return false;

	s = In;
	In->Read(&n16, sizeof(n16));
	if (n16 == 0x4D4D)
	{
		// file is big endian so we swap
		// Intel is little endian (i think)
		// TODO_PPC: change this
		LgiAssert(0);
	}

	Read(&n16, sizeof(n16));
	if (n16 == 42) // check magic number
	{
		bool Done = false;
		bool Error = false;

		ulong Offset;
		Read(&Offset, sizeof(Offset));

		// loop through all the IFD's
		while (	!Done AND
				!Error)
		{
			if (Offset == 0)
			{
				Done = true;
				break;
			}
			else
			{
				In->SetPos(Offset);

				ushort IFDEntries;
				Read(&IFDEntries, sizeof(IFDEntries));

				for (int i=0; i<IFDEntries; i++)
				{
					IFD *Ifd = new IFD;
					if (Ifd)
					{
						Blocks.Insert(Ifd);
						Ifd->Read(*this);
					}
					else
					{
						Error = true;
					}
				}
			}

			if (!Error)
			{
				// process sub file
				Status = ProcessRead(pDC);
				Error = !Status;
			}
			else
			{
				printf("%s:%i - error reading the IFD's.\n", __FILE__, __LINE__);
			}

			Read(&Offset, sizeof(Offset));
		}
	}
	else
	{
		printf("%s:%i - magic number check failed.\n", __FILE__, __LINE__);
	}

	return Status;
}

bool GdcTiff::ProcessWrite(GSurface *pDC)
{
	return false;
}

bool GdcTiff::WriteImage(GStream *Out, GSurface *pDC)
{
	return false;
}
