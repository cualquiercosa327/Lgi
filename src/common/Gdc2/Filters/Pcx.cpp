/*hdr
**      FILE:           Pcx.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           8/9/1998
**      DESCRIPTION:    Pcx file filter
**
**      Copyright (C) 1997-8, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "Gdc2.h"
#include "GString.h"
#include "GVariant.h"

class GdcPcx : public GFilter {
public:
	Format GetFormat() { return FmtPcx; }
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	bool ReadImage(GSurface *pDC, GStream *In);
	bool WriteImage(GStream *Out, GSurface *pDC);

	bool GetVariant(char *n, GVariant &v, char *a)
	{
		if (!stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Pcx";
		}
		else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "PCX";
		}
		else return false;

		return true;
	}
};

// Object Factory
class GdcPcxFactory : public GFilterFactory
{
	bool CheckFile(char *File, int Access, uchar *Hint)
	{
		return (File) ? stristr(File, ".pcx") != 0 : false;
	}

	GFilter *NewObject()
	{
		return new GdcPcx;
	}
} PcxFactory;

// PCX
#define swapb(i)		((i>>8) | ((i&0x00FF)<<8))

typedef struct {
	uchar		Magic;
	uchar		Type;
	uchar		Compression;
	uchar		Bits;

	ushort		x1;
	ushort		y1;
	ushort		x2;
	ushort		y2;

	ushort		DevX;
	ushort		DevY;

	uchar		Palette[48];

	uchar		Unknown; // 0

	uchar		Planes;
	ushort		Line; // Bytes per plane
	ushort		PalType; // 1

} PCX_HEADER;

bool GdcPcx::ReadImage(GSurface *pDC, GStream *In)
{
	bool Status = false;

	if (pDC && In)
	{
		PCX_HEADER Header;

		In->Read(&Header, sizeof(Header));
		if (Header.Magic == 0x0A)
		{
			int Bits = Header.Bits * Header.Planes;
			int Sx = Header.x2 - Header.x1 + 1;
			int Sy = Header.y2 - Header.y1  + 1;
			// int Len = (((Sx * Bits + 15) / 16) * 2);
			GPalette *Pal = 0;
			uchar PalData[768];

			memcpy(PalData, Header.Palette, 48);
			switch (Bits)
			{
				case 1:
				{
					Pal = new GPalette(PalData, 2);
					break;
				}
				case 4:
				{
					Pal = new GPalette(PalData, 16);
					break;
				}
				case 8:
				{
					In->SetPos(In->GetSize()-768);
					In->Read(PalData, 768);
					Pal = new GPalette(PalData);
					break;
				}
			}

			if (Pal)
			{
				pDC->Palette(Pal);
			}

			In->SetPos(128);

			if (pDC->Create(Sx, Sy, max(8, Bits)))
			{
				int Line = GetLineLength(pDC);
				uchar *buf = new uchar[Line];
				uchar *pbuf = new uchar[Header.Line];
				uchar *dest = 0;

				if (buf AND pbuf)
				{
					uint update = 10,x,y = 0;
					uchar a,b,c,p;

					while (y < Sy AND update)
					{
						memset(buf, 0, Line);

						for (p=0; p<Header.Planes; p++)
						{
							dest = pbuf;
							x = 0;

							while (x < Header.Line)
							{
								In->Read(&a, 1);
								if (a > 192)
								{
									In->Read(&b, 1);
									c = a - 192;
									memset(dest, b, c);
									x += c;
									dest += c;
								}
								else
								{
									*dest = a;
									x++;
									dest++;
								}
							}

							switch (Bits)
							{
								case 1:
								{
									uchar *d = buf,*s = pbuf;
									uchar Mask = 0x80;

									for (int x=0; x<Sx; x++)
									{
										*d++ = (*s & Mask) ? 1 : 0;
										Mask >>= 1;
										if (!Mask)
										{
											Mask = 0x80;
											s++;
										}
									}
									break;
								}

								case 4:
								{
									uchar *d = buf,*s = pbuf;
									uchar mask = 0x80;
									uchar bp = 1 << p;

									for (int i=0; i<Sx; i++)
									{
										if (*s & mask)
										{
											*d |= bp;
										}

										d++;
										mask >>= 1;
										if (!mask)
										{
											mask = 0x80;
											s++;
										}
									}

									break;
								}

								case 8:
								{
									memcpy(buf, pbuf, Line);
									break;
								}
								
								case 24:
								{
									int Offset = 2 - p;
									uchar *d = buf,*s = pbuf;

									for (int x=0; x<Sx; x++)
									{
										d[Offset] = *s++;
										d += 3;
									}
									
									break;
								}
							}
						}

						memcpy((*pDC)[y], buf, Line);
						dest = buf;

						y++;
						x = 0;

						update--;
						if (update == 1)
						{
							update = 10;
						}
					}

					Status = true;

					DeleteObj(buf);
					DeleteObj(pbuf);

					if (y != Sy)
					{
						return false;
					}
				}
			}
		}
	}

	return Status;
}

bool GdcPcx::WriteImage(GStream *Out, GSurface *pDC)
{
	bool Status = false;

	if (pDC && Out)
	{
		Out->SetSize(0);

		GPalette *Pal = pDC->Palette();
		PCX_HEADER Header;
		int Bits = pDC->GetBits();
		char c;

		// Fill out header struct and save it
		Header.Magic = 10;
		Header.Type = 5;
		Header.Compression = 1;

		switch (pDC->GetBits())
		{
			case 8:
			{
				Header.Bits = 8;
				Header.Planes = 1;

				if (Pal)
				{
					switch (Pal->GetSize())
					{
						case 2:
						{
							Header.Bits = 1;
							Header.Planes = 1;
							Bits = 1;
							break;
						}
						case 16:
						{
							Header.Bits = 1;
							Header.Planes = 4;
							Bits = 4;
							break;
						}
					}
				}

				break;
			}

			default:
			{
				Header.Bits = 8;
				Header.Planes = 3;
				break;
			}
		}

		Header.x1 = 0;
		Header.y1 = 0;
		Header.x2 = pDC->X() - 1;
		Header.y2 = pDC->Y() - 1;
		Header.DevX = GdcD->X();
		Header.DevY = GdcD->Y();

		memset(Header.Palette, 0, sizeof(Header.Palette));
		if (Pal)
		{
			for (int i=0; i<min(Pal->GetSize(), 16); i++)
			{
				uchar *p = Header.Palette + (i * 3);
				
				p[0] = (*Pal)[i]->R;
				p[1] = (*Pal)[i]->G;
				p[2] = (*Pal)[i]->B;
			}
		}

		Header.Unknown = 0;
		Header.Line = ((pDC->X() * Header.Bits) + 7) / 8;
		Header.Line += Header.Line % 2;
		Header.PalType = 1;

		Out->Write(&Header, sizeof(Header));
		
		// pad header to 128 bytes in length with zeros
		c = 0;
		int i;
		for (i=0; i<128-sizeof(Header); i++)
		{
			Out->Write(&c, 1);
		}

		// Write image data, plane by plane
		uchar *Buf = new uchar[Header.Line];
		if (Buf)
		{
			for (int y=0; y<pDC->Y(); y++)
			{
				// Extract the data for this plane
				for (int Plane=0; Plane<Header.Planes; Plane++)
				{
					memset(Buf, 0, Header.Line);

					switch (Bits)
					{
						case 1:
						{
							uchar *d = Buf;
							uchar Mask = 0x80;

							for (int x=0; x<pDC->X(); x++)
							{
								if (pDC->Get(x, y) & 1)
								{
									*d |= Mask;
								}

								Mask >>= 1;
								if (!Mask)
								{
									Mask = 0x80;
									d++;
								}
							}
							break;
						}
						case 4:
						{
							uchar *d = Buf;
							uchar Mask = 0x80;
							uchar PBit = 1 << Plane;

							for (int x=0; x<pDC->X(); x++)
							{
								if (pDC->Get(x, y) & PBit)
								{
									*d |= Mask;
								}

								Mask >>= 1;
								if (!Mask)
								{
									Mask = 0x80;
									d++;
								}
							}
							break;
						}
						case 8:
						{
							uchar *d = Buf;
							for (int x=0; x<pDC->X(); x++)
							{
								*d++ = pDC->Get(x, y);
							}
							break;
						}
						case 16:
						{
							uchar *d = Buf;
							if (Plane == 0)
							{
								for (int x=0; x<pDC->X(); x++)
								{
									*d++ = R16(pDC->Get(x, y)) << 3;
								}
							}
							if (Plane == 1)
							{
								for (int x=0; x<pDC->X(); x++)
								{
									*d++ = G16(pDC->Get(x, y)) << 2;
								}
							}
							if (Plane == 2)
							{
								for (int x=0; x<pDC->X(); x++)
								{
									*d++ = B16(pDC->Get(x, y)) << 3;
								}
							}
							break;
						}
						case 24:
						{
							uchar *d = Buf;
							uchar *s = (*pDC)[y];
							for (int x=0; x<pDC->X(); x++)
							{
								*d++ = s[2-Plane];
								s += 3;
							}
							break;
						}
						case 32:
						{
							uchar *d = Buf;
							uchar *s = (*pDC)[y];
							for (int x=0; x<pDC->X(); x++)
							{
								*d++ = s[1+Plane];
								s += 4;
							}
							break;
						}
					}

					// Compress the data and write it to the file
					for (int i=0; i<Header.Line;)
					{
						uchar Len = 0;
						uchar c = Buf[i];
	
						// search forward to see how many bytes we can archive
						for (; i+Len < Header.Line AND Buf[i+Len] == c AND Len < 63; Len++);
	
						if (Len > 1)
						{
							// archived section
							// Size: Len - 192
							i += Len;
							Len |= 0xC0;
							Out->Write(&Len, 1);
							Out->Write(&c, 1);
						}
						else
						{
							if (c & 0xC0)
							{
								// this colour can get confused with an archive byte
								// so we store it as a archive of length 1
								// this sux big time because every byte takes up
								// 2 bytes in the file :(
	
								Len = 0xC1;
								Out->Write(&Len, 1);
								Out->Write(&c, 1);
							}
							else
							{
								// write the colour to the file
								Out->Write(&c, 1);
							}
							
							i++;
						}
					}
				}
			}

			DeleteArray(Buf);
		}
		// Write out any 256 colour palette
		if (Pal AND Pal->GetSize() > 16)
		{
			c = 12;
			Out->Write(&c, 1);

			for (i=0; i<Pal->GetSize(); i++)
			{
				Out->Write(&(*Pal)[i]->R, 1);
				Out->Write(&(*Pal)[i]->G, 1);
				Out->Write(&(*Pal)[i]->B, 1);
			}
		}

		Status = true;
	}

	return Status;
}

