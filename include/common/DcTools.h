/// \file Graphics tools
#ifndef _DC_TOOLS_H_
#define _DC_TOOLS_H_

/// Various RGB colour space types
enum RgbLutCs
{
	RgbLutSRGB,
	RgbLutLinear
};

/// Build an RGB colour space conversion lookup table from 0 to max(T) 
// and of length 'Len'
template <typename T, int Len = 256>
class RgbLut
{
public:
	T Lut[Len];

	/// Construct the LUT
	RgbLut(RgbLutCs To, RgbLutCs From)
	{
		T Mul;
		memset(&Mul, 0xff, sizeof(Mul));
		double Div = Len - 1;

		double in, out;
		if (From == RgbLutLinear && To == RgbLutSRGB)
		{
			for (int i=0; i<Len; i++)
			{
				in = (double)i / Div;
				if (in <= 0.0031308)
					out = in * 12.92;
				else
					out = pow(in, 1.0 / 2.4) * 1.055 - 0.055;

				Lut[i] = out * Mul + 0.0000001;
			}
		}
		else if (From == RgbLutSRGB && To == RgbLutLinear)
		{
			for (int i=0; i<Len; i++)
			{
				in = (double)i / Div;
				if (in <= 0.04045)
					out = in / 12.92;
				else
					out = pow((in + 0.055) / 1.055, 2.4);

				Lut[i] = out * Mul;
			}
		}
	}
};

// remap dc to new palette
extern bool RemapDC(GSurface *pDC, GPalette *DestPal);

// flip dc
#define		FLIP_X					1
#define		FLIP_Y					2

extern bool FlipDC(GSurface *pDC, int Dir);

// return true if dc is greyscale
extern bool IsGreyScale(GSurface *pDC);

// convert dc to greyscale
extern bool GreyScaleDC(GSurface *pDest, GSurface *pSrc);

// invert the dc
extern bool InvertDC(GSurface *pDC);

// rotate dc
extern bool RotateDC(GSurface *pDC, double Angle);

// flip dc
extern bool FlipXDC(GSurface *pDC);
extern bool FlipYDC(GSurface *pDC);

// resample the dc
extern bool ResampleDC(GSurface *pTo, GSurface *pFrom, GRect *FromRgn = 0, Progress *Prog = 0);

#endif
