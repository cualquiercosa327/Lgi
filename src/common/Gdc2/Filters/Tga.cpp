/*hdr
**      FILE:           Tga.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           16/8/2000
**      DESCRIPTION:    Tga file filter
**
**      Copyright (C) 2000, Matthew Allen
**              fret@memecode.com
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Lgi.h"
#include "GVariant.h"

class GdcTga : public GFilter
{
public:
	int GetCapabilites() { return FILTER_CAP_READ; }
	Format GetFormat() { return FmtTga; }

	bool GetVariant(const char *n, GVariant &v, char *a)
	{
		if (!stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Targa";
		}
		else if (!stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "TGA";
		}
		else return false;

		return true;
	}

	IoStatus WriteImage(GStream *Out, GSurface *pDC)
	{
		return IoUnsupportedFormat;
	}

	IoStatus ReadImage(GSurface *pDC, GStream *In)
	{
		IoStatus Status = IoError;
		
		if (pDC && In)
		{
			uchar IdFieldSize, ImgType, CMapType, CMapEntrySize, Bits, Descriptor;
			ushort CMapOrigin, CMapLength;
			ushort OrgX, OrgY, X, Y;
			
			Read(In, &IdFieldSize, sizeof(IdFieldSize));
			Read(In, &CMapType, sizeof(CMapType));
			Read(In, &ImgType, sizeof(ImgType));
			Read(In, &CMapOrigin, sizeof(CMapOrigin));
			Read(In, &CMapLength, sizeof(CMapLength));
			Read(In, &CMapEntrySize, sizeof(CMapEntrySize));
			Read(In, &OrgX, sizeof(OrgX));
			Read(In, &OrgY, sizeof(OrgY));
			Read(In, &X, sizeof(X));
			Read(In, &Y, sizeof(Y));
			Read(In, &Bits, sizeof(Bits));
			Read(In, &Descriptor, sizeof(Descriptor));

			In->SetPos(18 + IdFieldSize);

			// load colour map
			if (CMapType)
			{
				In->SetPos(In->GetPos() + (CMapEntrySize * CMapLength));
			}

			// load pixels
			if (pDC->Create(X, Y, GBitsToColourSpace(Bits)))
			{
				switch (ImgType)
				{
					case 2:
					{
						Status = IoSuccess;
						switch (Bits)
						{
							case 16:
							case 24:
							case 32:
							{
								int PixSize = Bits / 8;
								int Line = PixSize * X;

								if (Meter)
								{
									Meter->SetDescription("Scanlines");
									Meter->SetLimits(0, Y-1);
								}

								for (int y=0; y<Y; y++)
								{
									uchar *p = (*pDC)[y];
									if (p)
									{
										In->Read(p, Line);
									}

									if (y % 10 == 0 &&
										Meter)
									{
										Meter->Value(y);
									}
								}
								break;
							}
							default:
							{
								Status = IoError;
								break;
							}
						}
						break;
					}
				}
			}
		}

		return Status;
	}
};

class GdcTgaFactory : public GFilterFactory
{
	bool CheckFile(const char *File, int Access, const uchar *Hint)
	{
		return (Access == FILTER_CAP_READ && File) ? stristr(File, ".tga") != 0 : false;
	}

	GFilter *NewObject()
	{
		return new GdcTga;
	}

} TgaFactory;
