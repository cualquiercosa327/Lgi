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
#include "GRange.h"

static bool HasBase64ToBinLut = false;
static uchar Base64ToBinLut[128];
static bool HasBinToBase64Lut = false;
static uchar BinToBase64Lut[64];

uchar Base64ToBin(char c)
{
	if (c >= 'A' && c <= 'Z')
	{
		return c - 'A';
	}

	if (c >= 'a' && c <= 'z')
	{
		return c - 'a' + 26;
	}

	if (c >= '0' && c <= '9')
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

	if (c >= 26 && c <= 51)
	{
		return c - 26 + 'a';
	}

	if (c >= 52 && c <= 61)
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

ssize_t ConvertBase64ToBinary(uchar *Binary, ssize_t OutBuf, char *Base64, ssize_t InBuf)
{
	uchar *Start = Binary;

	if (!HasBase64ToBinLut)
	{
		for (int i=0; i<sizeof(Base64ToBinLut); i++)
		{
			Base64ToBinLut[i] = Base64ToBin(i);
		}
		HasBase64ToBinLut = true;
	}

	if (Binary && OutBuf > 0 && Base64 && InBuf > 0)
	{
		int Temp[4];

		while (InBuf > 3 && OutBuf > 0)
		{
			Temp[0] = Base64ToBinLut[Base64[0] & 0x7f];
			Temp[1] = Base64ToBinLut[Base64[1] & 0x7f];
			Temp[2] = Base64ToBinLut[Base64[2] & 0x7f];
			Temp[3] = Base64ToBinLut[Base64[3] & 0x7f];

			*Binary++ = (Temp[0] << 2) | (Temp[1] >> 4);
			OutBuf--;
			if (Base64[2] != '=')
			{
				LgiAssert(OutBuf > 0);
				*Binary++ = (Temp[1] << 4) | (Temp[2] >> 2);
				OutBuf--;
				if (Base64[3] != '=')
				{
					LgiAssert(OutBuf > 0);
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

	return (int) (Binary - Start);
}

ssize_t ConvertBinaryToBase64(char *Base64, ssize_t OutBuf, uchar *Binary, ssize_t InBuf)
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

	if (Binary && OutBuf > 0 && Base64 && InBuf > 0)
	{
		ssize_t i = InBuf / 3;
		ssize_t o = OutBuf / 4;
		ssize_t Segments = MIN(i, o);

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

		if (OutBuf > 3 && InBuf > 0)
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

	return (int) (Base64 - Start);
}

