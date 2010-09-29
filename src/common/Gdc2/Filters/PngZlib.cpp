/*hdr
**      FILE:           Png.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Png file filter
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GLibrary.h"
#include "zlib.h"
#include "GFilterUtils.h"

class ZLib : public GLibrary
{
public:
	ZLib() : GLibrary("zlib") {}

	DynFunc1(int, inflateEnd, z_streamp, strm);
	DynFunc2(int, inflate, z_streamp, strm, int, flush);
	DynFunc3(int, inflateInit_, z_streamp, strm, const char *, version, int, stream_size);
	DynFunc1(int, deflateEnd, z_streamp, strm);
	DynFunc2(int, deflate, z_streamp, strm, int, flush);
	DynFunc4(int, deflateInit_, z_streamp, strm, int, level, const char *, version, int, stream_size);
};

class GdcPng : public GFilter, public ZLib
{
	static bool CrcTableComputed;
	static ulong CrcTable[256];

	void MakeCrcTable();
	ulong UpdateCrc(ulong crc, uchar *buf, int len);
	ulong Crc(uchar *buf, int len);

	struct IHDR_Obj {

		uchar Raw[13];

		int x;
		int y;
		uchar Bits;
		uchar ColourType;
		uchar Compression;
		uchar Filter;
		uchar Interlace;

		int TotalBits;

		void Read();
		void Write();

	} Header;

	int Pos;
	uchar *PrevScanLine;
	GSurface *pDC;
	GBytePipe DataPipe;

	bool ProcessData();
	uchar PaethPredictor(uchar a, uchar b, uchar c);
	bool WriteObject(ulong ChunkId, uchar *Ptr, int Len);

public:
	GdcPng();
	~GdcPng();

	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	bool ReadImage(GSurface *pDC);
	bool WriteImage(GSurface *pDC);
};

// Object Factory
class GdcPngFactory : public GFilterFactory
{
	bool CheckFile(char *File, int Access)
	{
		return (File) ? stristr(File, ".png") != 0 : false;
	}

	GFilter *NewObject()
	{
		return new GdcPng;
	}
} PngFactory;

#define IHDR				0x49484452
#define PLTE				0x504c5445
#define IDAT				0x49444154
#define IEND				0x49454e44

bool GdcPng::CrcTableComputed = FALSE;
ulong GdcPng::CrcTable[256];

void GdcPng::MakeCrcTable()
{
	ulong c;

	for (int n = 0; n < 256; n++)
	{
		c = (ulong) n;
		for (int k = 0; k < 8; k++)
		{
			if (c & 1)
			{
				c = 0xedb88320L ^ (c >> 1);
			}
			else
			{
				c = c >> 1;
			}
		}

		CrcTable[n] = c;
	}

	CrcTableComputed = TRUE;
}

ulong GdcPng::UpdateCrc(ulong crc, uchar *buf, int len)
{
	ulong c = crc;

	if (!CrcTableComputed)
	{
		MakeCrcTable();
	}

	for (int n = 0; n < len; n++)
	{
		c = CrcTable[(c ^ buf[n]) & 0xff] ^ (c >> 8);
	}

	return c;
}

ulong GdcPng::Crc(uchar *buf, int len)
{
	return UpdateCrc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

uchar *Write4(uchar *Ptr, int i)
{
	*Ptr++ = i >> 24;
	*Ptr++ = i >> 16;
	*Ptr++ = i >> 8;
	*Ptr++ = i;
	return Ptr;
}

uchar *Read4(uchar *Ptr, int &i)
{
	i = ((int) Ptr[0] << 24) | ((int) Ptr[1] << 16) | ((int) Ptr[2] << 8) | (Ptr[3]);
	return Ptr + 4;
}

void GdcPng::IHDR_Obj::Read()
{
	uchar *Ptr = Raw;

	Ptr = Read4(Ptr, x);
	Ptr = Read4(Ptr, y);
	Bits = *Ptr++;
	ColourType = *Ptr++;
	Compression = *Ptr++;
	Filter = *Ptr++;
	Interlace = *Ptr++;
}

void GdcPng::IHDR_Obj::Write()
{
	uchar *Ptr = Raw;

	Ptr = Write4(Ptr, x);
	Ptr = Write4(Ptr, y);

	*Ptr++ = Bits;
	*Ptr++ = ColourType;
	*Ptr++ = Compression;
	*Ptr++ = Filter;
	*Ptr++ = Interlace;
}

uchar GdcPng::PaethPredictor(uchar a, uchar b, uchar c)
{
	// a = left, b = above, c = upper left
	int p = (int) a + b - c;	// initial estimate
	int pa = abs(p - a);		// distances to a, b, c
	int pb = abs(p - b);
	int pc = abs(p - c);
	
	// return nearest of a,b,c,
	// breaking ties in order a,b,c.
	if (pa <= pb AND pa <= pc)
	{
		return a;
	}
	else if (pb <= pc)
	{
		return b;
	}
	
	return c;
}

bool GdcPng::ProcessData()
{
	int Line = ((pDC->X() * Header.TotalBits) + 7) / 8;
	uchar *Buf = new uchar[1+Line];

	for (; DataPipe.Pop(Buf, 1+Line); )
	{
		uchar *Data = Buf;
		uchar Code = *Data++;
		int bbp = max(Header.TotalBits / 8, 1);

		switch (Code)
		{
			case 1:		// Sub
			{
				for (int x=bbp; x<Line; x++)
				{
					 Data[x] += Data[x-bbp];
				}
				break;
			}
			case 2:		// Up
			{
				if (PrevScanLine)
				{
					for (int x=0; x<Line; x++)
					{
						Data[x] += PrevScanLine[x];
					}
				}
				else
				{
					return FALSE;
				}
				break;
			}
			case 3:		// Average
			{
				if (PrevScanLine)
				{
					for (int x=0; x<Line; x++)
					{
						int Last = (x >= bbp) ? Data[x-bbp] : 0;
						Data[x] += (Last + PrevScanLine[x]) / 2;
					}
				}
				else
				{
					return FALSE;
				}
				break;
			}
			case 4:		// Paeth
			{
				if (PrevScanLine)
				{
					int x;

					for (x=0; x<bbp; x++)
					{
						Data[x] += PaethPredictor(0, PrevScanLine[x], 0);
					}

					for (; x<Line; x++)
					{
						Data[x] += PaethPredictor(Data[x-bbp], PrevScanLine[x], PrevScanLine[x-bbp]);
					}
				}
				else
				{
					return FALSE;
				}
				break;
			}
   		}

		if (NOT PrevScanLine)
		{
			PrevScanLine = new uchar[Line];
		}

		if (PrevScanLine)
		{
			memcpy(PrevScanLine, Data, Line);
		}

		uchar *d = (Pos >= 0 AND Pos < pDC->Y()) ? (*pDC)[Pos] : 0;
		if (d)
		{
			switch (Header.TotalBits)
			{
				case 1:
				{
					int x;
					uchar Mask = 0x80;

					for (x=0; x<pDC->X(); x++)
					{
						pDC->Colour((*Data & Mask) ? 1 : 0);
						pDC->Set(x, Pos);

						Mask >>= 1;
						if (NOT Mask)
						{
							Data++;
							Mask = 0x80;
						}
					}
					break;
				}
				case 4:
				{
					int x;
					for (x=0; x<pDC->X(); x++)
					{
						if (x & 1)
						{
							pDC->Colour((*Data) & 0xF);
							Data++;
						}
						else
						{
							pDC->Colour((*Data) >> 4);
						}

						pDC->Set(x, Pos);
					}
					break;
				}
				case 8:
				{
					memcpy(d, Data, Line);
					break;
				}
				case 24:
				{
					for (int x=0; x<pDC->X(); x++)
					{
						d[0] = Data[2];
						d[1] = Data[1];
						d[2] = Data[0];
						d += 3;
						Data += 3;
					}
					break;
				}
			}
		}

		Pos++;
	}

	DeleteObj(Buf);

	return TRUE;
}

bool GdcPng::ReadImage(GSurface *pDeviceContext)
{
	char PngSig[] = {137, 80, 78, 71, 13, 10, 26, 10, 0};
	z_stream Stream;

	ZeroObj(Stream);
	int Err = inflateInit(&Stream);

	Pos = 0;
	pDC = pDeviceContext;
	DeleteArray(PrevScanLine);

	if (FindHeader(0, PngSig, *this))
	{
		bool Done = FALSE;
		bool Error = FALSE;

		SetSwap(TRUE);

		while (NOT Eof() AND NOT Done AND NOT Error)
		{
			int Type;
			int Length;
			int CRC;

			*this >> Length;
			*this >> Type;

			switch (Type)
			{
				case IHDR:
				{
					if (Read(Header.Raw, 13))
					{
						Header.Read();
						Header.TotalBits = Header.Bits;
						switch (Header.ColourType)
						{
							case 2: Header.TotalBits *= 3; break;
							case 6: Header.TotalBits *= 3; break;
						}
						
						if (pDC->Create(Header.x, Header.y, max(Header.TotalBits, 8)))
						{
							pDC->Colour(-1);
							pDC->Rectangle();
						}
						else
						{
							Error = Done = TRUE;
						}
					}
					break;
				}
				case PLTE:
				{
					int Colours = Length / 3;
					GPalette *Pal = new GPalette;
					if (Pal AND Pal->SetSize(Colours))
					{
						for (int i=0; i<Colours; i++)
						{
							GdcRGB *p = (*Pal)[i];
							if (p)
							{
								*this >> p->R;
								*this >> p->G;
								*this >> p->B;
							}
						}

						pDC->Palette(Pal);
					}
					break;
				}
				case IDAT:
				{
					int TotalOutput = 0;
					if (Header.Compression == 0)
					{
						int InSize = Length;
						int OutSize = 65536;
						uchar *InBuf = new uchar[InSize];
						uchar *OutBuf = new uchar[OutSize];

						if (InBuf AND
							OutBuf AND
							(Read(InBuf, InSize) == InSize))
						{
							// ZLIB stuff to decompress the stream
							if (Err == Z_OK)
							{
								Stream.next_in = InBuf;
								Stream.avail_in = InSize;

								while (Stream.avail_in > 0 AND Err == Z_OK)
								{
									Stream.next_out  = OutBuf;
									Stream.avail_out = OutSize;

									Err = inflate(&Stream, Z_SYNC_FLUSH);
									if (Err == Z_OK OR Err == Z_STREAM_END)
									{
										TotalOutput += (int) Stream.next_out - (int) OutBuf;
										DataPipe.Push(OutBuf, (int) Stream.next_out - (int) OutBuf);
										Error = NOT ProcessData();
									}
								}
							}
							else
							{
								Error = TRUE;
							}
						}

						/*
						Err = inflate(&Stream, Z_SYNC_FLUSH);
						if (Err == Z_OK OR Err == Z_STREAM_END)
						{
							TotalOutput += (int) Stream.next_out - (int) OutBuf;
							DataPipe.Push(OutBuf, (int) Stream.next_out - (int) OutBuf);
							Error = NOT ProcessData();
						}
						*/

						DeleteArray(InBuf);
						DeleteArray(OutBuf);
					}
					else
					{
						// skip unknown compression type
						Seek(Length, SEEK_CUR);
					}
					break;
				}
				case IEND:
				{
					Done = TRUE;
					Status = TRUE;
					break;
				}
				default:
				{
					Seek(Length, SEEK_CUR);
					break;
				}
			}

			*this >> CRC;
		}
	}

	inflateEnd(&Stream);

	return Status;
}

#define CRCCOMPL(c) ((c)^0xffffffff)
#define CRCINIT (CRCCOMPL(0))

bool GdcPng::WriteObject(ulong ChunkId, uchar *Ptr, int Len)
{
	ulong CrcVal = CRCINIT;
	if (Ptr)
	{
		uchar Chunk[5] = "    ";

		Chunk[0] = (ChunkId >> 24) & 0xFF;
		Chunk[1] = (ChunkId >> 16) & 0xFF;
		Chunk[2] = (ChunkId >> 8) & 0xFF;
		Chunk[3] = ChunkId & 0xFF;

		*this << Len;
		Write(Chunk, 4);
		Write(Ptr, Len);
		
		CrcVal = UpdateCrc(CrcVal, Chunk, 4);
		CrcVal = UpdateCrc(CrcVal, Ptr, Len);

		ulong Temp = CRCCOMPL(CrcVal);
		*this << Temp;

		return TRUE;
	}

	return FALSE;
}

bool GdcPng::WriteImage(GSurface *pDC)
{
	bool Status = FALSE;
	bool Error = FALSE;
	z_stream Stream;
	ZeroObj(Stream);
	int Err = deflateInit(&Stream, Z_DEFAULT_COMPRESSION);

	if (pDC AND Open())
	{
		char PngSig[] = {137, 80, 78, 71, 13, 10, 26, 10, 0};
		GPalette *Pal = pDC->Palette();

		// setup
		SetSize(0);
		SetSwap(true);

		// write sig
		Write(PngSig, 8);

		// write IHDR
		Header.x = pDC->X();
		Header.y = pDC->Y();
		Header.Compression = 0;
		Header.Filter = 0;
		Header.Interlace = FALSE;

		switch (pDC->GetBits())
		{
			case 8:
			{
				Header.Bits = 8;
				if (Pal)
				{
					if (Pal->GetSize() <= 2)
					{
						Header.Bits = 1;
					}
					else if (Pal->GetSize() <= 16)
					{
						Header.Bits = 4;
					}

					Header.ColourType = 3; // palette
				}
				else
				{
					Header.ColourType = 0; // greyscale
				}
				Header.TotalBits = Header.Bits;
				break;
			}
			case 16:
			case 24:
			case 32:
			{
				Header.Bits = 8; // 24bit rgb
				Header.ColourType = 2; // 24bit rgb
				Header.TotalBits = 24;
				break;
			}
		}

		Header.Write();
		WriteObject(IHDR, Header.Raw, 13);
		
		// write PLTE
		if (Pal)
		{
			int Size = Pal->GetSize()*3;
			uchar *Data = new uchar[Size];
			if (Data)
			{
				uchar *d = Data;
				for (int i=0; i<Size; i++)
				{
					GdcRGB *s = (*Pal)[i];
					if (s)
					{
						*d++ = s->R;
						*d++ = s->G;
						*d++ = s->B;
					}
				}

				WriteObject(PLTE, Data, Size);
				DeleteArray(Data);
			}
		}

		// write IDAT's
		int Line = ((pDC->X() * Header.TotalBits) + 7) / 8;
		uchar *Data = new uchar[Line+1];
		if (Data)
		{
			for (int y=0; y<pDC->Y(); y++)
			{
				// pack data
				*Data = 0;
				uchar *d = Data + 1;
				memset(d, 0, Line);
				switch (pDC->GetBits())
				{
					case 8:
					{
						switch (Header.Bits)
						{
							case 1:
							{
								uchar Mask = 0x80;

								for (int x=0; x<pDC->X(); x++)
								{
									*d |= (pDC->Get(x, y)) ? Mask : 0;
									Mask >>= 1;
									if (NOT Mask)
									{
										Mask = 0x80;
										d++;
									}
								}
								break;
							}
							case 4:
							{
								for (int x=0; x<pDC->X(); x++)
								{
									if (x & 1)
									{
										*d |= pDC->Get(x, y) & 0xF;;
										d++;
									}
									else
									{
										*d |= pDC->Get(x, y) << 4;
									}
								}
								break;
							}
							case 8:
							{
								memcpy(d, (*pDC)[y], Line);
								break;
							}
						}
						break;
					}
					case 16:
					{
						for (int x=0; x<pDC->X(); x++)
						{
							COLOUR c = pDC->Get(x, y);
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
							COLOUR c = pDC->Get(x, y);
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
							COLOUR c = pDC->Get(x, y);
							*d++ = R32(c);
							*d++ = G32(c);
							*d++ = B32(c);
						}
						break;
					}
				}

				// add to bitstream
				DataPipe.Push(Data, Line+1);
			}

			DeleteArray(Data);

			// compress image
			int Size = 32 << 10;
			uchar *InBuf = new uchar[Size];
			uchar *OutBuf = new uchar[Size];
			if (InBuf AND OutBuf)
			{
				// ZLIB stuff to compress the stream
				if (Err == Z_OK)
				{
					int TotalInput = DataPipe.Sizeof();

					bool Done = FALSE;
					while (NOT Done)
					{
						if (Stream.avail_in == 0)
						{
							int Len = min(DataPipe.Sizeof(), Size);
							if (Len > 0)
							{
								DataPipe.Pop(InBuf, Len);
								Stream.next_in  = (Bytef*) InBuf;
								Stream.avail_in = Len;
							}
							else
							{
								Done = TRUE;
							}
						}

						if (Stream.avail_out == 0)
						{
							int CompLen = (int) Stream.next_out - (int) OutBuf;
							if (CompLen > 0)
							{
								WriteObject(IDAT, OutBuf, CompLen);
							}

							Stream.next_out = (Bytef*) OutBuf;
							Stream.avail_out = Size;
						}

						if (Stream.avail_in > 0 AND Stream.avail_out > 0)
						{
							int Err = deflate(&Stream, Z_FULL_FLUSH);
							if (Err != Z_OK)
							{
								Done = TRUE;
								Error = TRUE;

								Props.Set(LGI_FILTER_ERROR, "Compressor return an error.");
							}
						}
					}

					deflate(&Stream, Z_FINISH);

					int CompLen = (int) Stream.next_out - (int) OutBuf;
					if (CompLen > 0)
					{
						WriteObject(IDAT, OutBuf, CompLen);
					}

					Status = NOT Error;
				}
				else
				{
					Error = TRUE;
					Props.Set(LGI_FILTER_ERROR, "Compressor failed to initialize.");
				}
			}
			else
			{
				Error = TRUE;
				Props.Set(LGI_FILTER_ERROR, "Buffer memory allocation error.");
			}

			DeleteArray(InBuf);
			DeleteArray(OutBuf);
		}
		else
		{
			Error = FALSE;
		}

		// write IEND
		WriteObject(IEND, (uchar*) "", 0);
	}

	deflateEnd(&Stream);

	return Status;
}

GdcPng::GdcPng()
{
	Pos = 0;
	PrevScanLine = 0;
	Props.Set("Type", "Portable Network Graphic");
	Props.Set("Extension", "PNG");
}

GdcPng::~GdcPng()
{
	DeleteArray(PrevScanLine);
}
