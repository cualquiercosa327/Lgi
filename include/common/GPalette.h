#ifndef _GPALETTE_H_
#define _GPALETTE_H_

#include "GColourSpace.h"
#include "GFile.h"

#ifdef WIN32
#pragma pack(push, before_pack)
#pragma pack(1)
#endif

/// RGB Colour
typedef GRgba32 GdcRGB;

#ifdef WIN32
#pragma pack(pop, before_pack)
#endif

/// Palette of colours
class LgiClass GPalette
{
protected:
	#if WINNATIVE
	HPALETTE	hPal;
	LOGPALETTE	*Data;
	#else
	int			Size;
	GRgba32		*Data;
	#endif
	uchar *Lut;

public:
	GPalette();
	virtual ~GPalette();

	#if WINNATIVE
	HPALETTE Handle() { return hPal; }
	#endif

	GPalette(GPalette *pPal);	
	GPalette(uchar *pPal, int s = 256);
	void Set(GPalette *pPal);
	void Set(uchar *pPal, int s = 256);
	void Set(int Index, int r, int g, int b);

	int GetSize();
	GRgba32 *operator [](int i);
	bool Update();
	bool SetSize(int s = 256);
	void SwapRAndB();
	int MatchRgb(uint32 Rgb);
	void CreateCube();
	void CreateGreyScale();
	bool Load(GFile &F);
	bool Save(GFile &F, int Format);
	uchar *MakeLut(int Bits = 16);

	bool operator ==(GPalette &p);
	bool operator !=(GPalette &p);
};

#endif