/*hdr
**      FILE:           Base64.cpp
**      AUTHOR:         Matthew Allen
**      DATE:           1/6/98
**      DESCRIPTION:    Base64 encoding/decoding
**
**      Copyright (C) 1998, Matthew Allen
**              fret@memecode.com
*/

#include "GMem.h"
#include "LgiInc.h"
#include "Base64.h"

static bool HasBase64ToBinLut = false;
static uchar Base64ToBinLut[128];
static bool HasBinToBase64Lut = false;
static uchar BinToBase64Lut[64];

uchar Base64ToBin(char c)
{
	if (c >= 'A' AND c <= 'Z')
	{
		return c - 'A';
	}

	if (c >= 'a' AND c <= 'z')
	{
		return c - 'a' + 26;
	}

	if (c >= '0' AND c <= '9')
	{
		return c - '0' + 52;
	}

	if (c == '+')
	{
		return 62;
	}

	if (c == '/')
	{
		return 63;
	}

	return 0;
}

char BinToBase64(uchar c)
{
	if (c <= 25)
	{
		return c + 'A';
	}

	if (c >= 26 AND c <= 51)
	{
		return c - 26 + 'a';
	}

	if (c >= 52 AND c <= 61)
	{
		return c - 52 + '0';
	}

	if (c == 62)
	{
		return '+';
	}

	if (c == 63)
	{
		return '/';
	}

	return 0;
}

int ConvertBase64ToBinary(uchar *Binary, int OutBuf, char *Base64, int InBuf)
{
	int Status = 0;
	uchar *Start = Binary;

	if (!HasBase64ToBinLut)
	{
		for (int i=0; i<sizeof(Base64ToBinLut); i++)
		{
			Base64ToBinLut[i] = Base64ToBin(i);
		}
		HasBase64ToBinLut = true;
	}

	if (Binary AND OutBuf > 0 AND Base64 AND InBuf > 0)
	{
		int Temp[4];

		while (InBuf > 3 AND OutBuf > 2)
		{
			Temp[0] = Base64ToBinLut[Base64[0] & 0x7f];
			Temp[1] = Base64ToBinLut[Base64[1] & 0x7f];
			Temp[2] = Base64ToBinLut[Base64[2] & 0x7f];
			Temp[3] = Base64ToBinLut[Base64[3] & 0x7f];

			*Binary++ = (Temp[0] << 2) | (Temp[1] >> 4);
			OutBuf--;
			if (Base64[2] != '=')
			{
				*Binary++ = (Temp[1] << 4) | (Temp[2] >> 2);
				OutBuf--;
				if (Base64[3] != '=')
				{
					*Binary++ = (Temp[2] << 6) | (Temp[3]);
					OutBuf--;
				}
				else
					break;
			}
			else
				break;

			Base64 += 4;
			InBuf -= 4;
		}
	}

	return (int)Binary - (int)Start;
}

int ConvertBinaryToBase64(char *Base64, int OutBuf, uchar *Binary, int InBuf)
{
	char *Start = Base64;

	if (!HasBinToBase64Lut)
	{
		for (int i=0; i<sizeof(BinToBase64Lut); i++)
		{
			BinToBase64Lut[i] = BinToBase64(i);
		}
		HasBinToBase64Lut = true;
	}

	if (Binary AND OutBuf > 0 AND Base64 AND InBuf > 0)
	{
		int i = InBuf / 3;
		int o = OutBuf / 4;
		int Segments = min(i, o);

		for (int n=0; n<Segments; n++)
		{
			Base64[0] = BinToBase64Lut[Binary[0] >> 2];
			Base64[1] = BinToBase64Lut[((Binary[0] & 3) << 4) | (Binary[1] >> 4)];
			Base64[2] = BinToBase64Lut[((Binary[1] & 0xF) << 2) | (Binary[2] >> 6)];
			Base64[3] = BinToBase64Lut[Binary[2] & 0x3F];

			Base64 += 4;
			Binary += 3;
		}

		InBuf -= Segments * 3;
		OutBuf -= Segments << 2;

		if (OutBuf > 3 AND InBuf > 0)
		{
			*Base64++ = BinToBase64Lut[Binary[0] >> 2];
			InBuf--;

			if (InBuf > 0)
			{
				*Base64++ = BinToBase64Lut[((Binary[0] & 3) << 4) | (Binary[1] >> 4)];
				InBuf--;

				if (InBuf > 0)
				{
					*Base64++ = BinToBase64Lut[((Binary[1] & 0xF) << 2) | (Binary[2] >> 6)];
					*Base64++ = BinToBase64Lut[Binary[2] & 0x3F];
				}
				else
				{
					*Base64++ = BinToBase64Lut[(Binary[1] & 0xF) << 2];
					*Base64++ = '=';
				}
			}
			else
			{
				*Base64++ = BinToBase64Lut[(Binary[0] << 4) & 0x3F];
				*Base64++ = '=';
				*Base64++ = '=';
			}
		}
	}

	return (int)Base64 - (int)Start;
}

