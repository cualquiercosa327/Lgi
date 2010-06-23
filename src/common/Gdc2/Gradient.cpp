#include <math.h>
#include "Gdc2.h"

class GGradient : public GApplicator
{
protected:
	int Sx, Sy;
	uchar *Ptr;
	int Bytes;
	COLOUR Background;
	int Angle;
	GRect Rect;

public:
	bool SetSurface(GBmpMem *d, GPalette *p = 0, GBmpMem *a = 0)
	{
		Ptr = 0;
		Bytes = 0;
		Background = 0;
		Angle = 0;
		Rect.ZOff(0, 0);
		Sx = Sy = 0;

		if (d)
		{
			Dest = d;
			Pal = p;
			Alpha = 0;

			Ptr = d->Base;
			Bytes = d->Bits / 8;
			return true;
		}

		return false;
	}

	void SetPtr(int x, int y)
	{
		Sx = x;
		Sy = y;
		Ptr = Dest->Base + (y * Dest->Line) + (x * Bytes);
	}

	void IncX()
	{
		Ptr += Bytes;
	}

	void IncY()
	{
		Ptr += Dest->Line;
	}

	void IncPtr(int X, int Y)
	{
		Ptr += (Y * Dest->Line) + (X * Bytes);
	}

	void Set()
	{
	}

	COLOUR Get()
	{
		switch (Bytes)
		{
			case 2:
			{
				ushort *p = (ushort*)Ptr;
				return *p;
				break;
			}
			case 3:
			{
				uchar *p = (uchar*)Ptr;
				return Rgb24(p[2], p[1], p[0]);
				break;
			}
			case 4:
			{
				ulong *p = (ulong*)Ptr;
				return *p;
				break;
			}
		}

		return -1;
	}

	void VLine(int height)
	{
	}

	bool Blt(GBmpMem *Src, GPalette *SPal, GBmpMem *SrcAlpha = 0)
	{
		return false;
	}

	int GetVar(int Var)
	{
		switch (Var)
		{
			case GAPP_ANGLE:
			{
				return Angle;
			}
			case GAPP_BOUNDS:
			{
				return (int)&Rect;
			}
			case GAPP_BACKGROUND:
			{
				return Background;
			}
		}

		return -1;
	}

	int SetVar(int Var, int Value)
	{
		int o = GetVar(Var);

		switch (Var)
		{
			case GAPP_ANGLE:
			{
				Angle = Value;
				break;
			}
			case GAPP_BOUNDS:
			{
				if (Value)
				{
					Rect = *((GRect*)Value);
				}
				break;
			}
			case GAPP_BACKGROUND:
			{
				Background = Value;
				break;
			}
		}

		return o;
	}
};

class GLinearGradient : public GGradient
{
public:
	void Rectangle(int X, int Y)
	{
		COLOUR Fore = CBit(24, c, Dest->Bits);
		int Fr = R24(Fore);
		int Fg = G24(Fore);
		int Fb = B24(Fore);

		COLOUR Back = CBit(24, Background, Dest->Bits);
		int Br = R24(Back);
		int Bg = G24(Back);
		int Bb = B24(Back);

		double Ang = LGI_DegToRad(Angle);
		while (Ang > LGI_PI) Ang -= LGI_PI;
		while (Ang < 0) Ang += LGI_PI;
		uchar *Div255 = Div255Lut;
		int a, oma, r, g, b;

		int Cx = (Rect.X() / 2);
		int Cy = (Rect.Y() / 2);
		#define Rx(x, y)		((cos(Ang)*(x-Cx)) - (sin(Ang)*(y-Cy)))
		#define Ry(x, y)		((sin(Ang)*(x-Cx)) + (cos(Ang)*(y-Cy)))
		GdcPt2 p[4] =
		{
			GdcPt2(0, 0),
			GdcPt2(Rect.X()-1, 0),
			GdcPt2(0, Rect.Y()-1),
			GdcPt2(Rect.X()-1, Rect.Y()-1)
		};

		int MinX = 100000, MaxX = -100000;
		int MinY = 100000, MaxY = -100000;
		for (int i=0; i<4; i++)
		{
			p[i].x = Rx(p[i].x, p[i].y);
			p[i].y = Ry(p[i].x, p[i].y);

			MinX = min(MinX, p[i].x);
			MaxX = max(MaxX, p[i].x);
			MinY = min(MinY, p[i].y);
			MaxY = max(MaxY, p[i].y);
		}

		int Scale = MaxX - MinX;
		int Offset = -MinX;
		if (Scale == 0)
		{
			return;
		}

		for (int y=0; y<Y; y++)
		{
			int Nx;
			uchar *p = Ptr;
			int Kx = Cx - (Sx - Rect.x1);
			int Ky = Cy - (Sy - Rect.y1);
			double CosA = cos(Ang);
			int CompY = sin(Ang) * (y - Ky);

			switch (Dest->Bits)
			{
				case 16:
				{
					for (int x=0; x<X; x++, p+=Bytes)
					{
						Nx = (CosA*(x-Kx)) - CompY;
						a = ((Nx + Offset) * 0xff / Scale);
						oma = 0xff - a;
						r = Div255[(Fr*a) + (Br*oma)];
						g = Div255[(Fg*a) + (Bg*oma)];
						b = Div255[(Fb*a) + (Bb*oma)];
						*((uint32*)p) = Rgb16(r, g, b);
					}
					break;
				}
				case 24:
				{
					for (int x=0; x<X; x++)
					{
						Nx = (CosA*(x-Kx)) - CompY;
						a = ((Nx + Offset) * 0xff / Scale);
						oma = 0xff - a;
						*p++ = Div255[(Fb*a) + (Bb*oma)];
						*p++ = Div255[(Fg*a) + (Bg*oma)];
						*p++ = Div255[(Fr*a) + (Br*oma)];
					}
					break;
				}
				case 32:
				{
					for (int x=0; x<X; x++, p+=Bytes)
					{
						Nx = (CosA*(x-Kx)) - CompY;
						a = ((Nx + Offset) * 0xff / Scale);
						oma = 0xff - a;
						r = Div255[(Fr*a) + (Br*oma)];
						g = Div255[(Fg*a) + (Bg*oma)];
						b = Div255[(Fb*a) + (Bb*oma)];
						*((uint32*)p) = Rgb32(r, g, b);
					}
					break;
				}
			}

			Ptr += Dest->Line;
		}
	}
};

class GRadialGradient : public GGradient
{
public:
	void Rectangle(int X, int Y)
	{
		COLOUR Fore = CBit(24, c, Dest->Bits);
		int Fr = R24(Fore);
		int Fg = G24(Fore);
		int Fb = B24(Fore);

		COLOUR Back = CBit(24, Background, Dest->Bits);
		int Br = R24(Back);
		int Bg = G24(Back);
		int Bb = B24(Back);

		uchar *Div255 = Div255Lut;
		int a, oma, r, g, b;

		double Cx = Rect.X() / 2;
		double Cy = Rect.Y() / 2;
		double Radius = sqrt(Cx*Cx + Cy*Cy);
		for (int y=0; y<Y; y++)
		{
			int Kx = Cx - (Sx - Rect.x1);
			uchar *p = Ptr;
			double DySq = Cy - y - (Sy - Rect.y1);
			DySq *= DySq;
			double Dx;

			switch (Dest->Bits)
			{
				case 16:
				{
					for (int x=0; x<X; x++, p += Bytes)
					{
						Dx = Kx-x;
						a = sqrt(Dx*Dx + DySq) / Radius * 0xff;
						oma = 0xff - a;
						r = Div255[(Fr*a) + (Br*oma)];
						g = Div255[(Fg*a) + (Bg*oma)];
						b = Div255[(Fb*a) + (Bb*oma)];
						*((uint32*)p) = Rgb16(r, g, b);
					}
					break;
				}
				case 24:
				{
					for (int x=0; x<X; x++)
					{
						Dx = Kx-x;
						a = sqrt(Dx*Dx + DySq) / Radius * 0xff;
						oma = 0xff - a;
						*p++ = Div255[(Fb*a) + (Bb*oma)];
						*p++ = Div255[(Fg*a) + (Bg*oma)];
						*p++ = Div255[(Fr*a) + (Br*oma)];
					}
					break;
				}
				case 32:
				{
					for (int x=0; x<X; x++, p += Bytes)
					{
						Dx = Kx-x;
						a = sqrt(Dx*Dx + DySq) / Radius * 0xff;
						oma = 0xff - a;
						r = Div255[(Fr*a) + (Br*oma)];
						g = Div255[(Fg*a) + (Bg*oma)];
						b = Div255[(Fb*a) + (Bb*oma)];
						*((uint32*)p) = Rgb32(r, g, b);
					}
					break;
				}
			}

			Ptr += Dest->Line;
		}
	}
};

class GGradientFactory : public GApplicatorFactory
{
public:
	GApplicator *Create(int Bits, int Op)
	{
		if (Op == 5)
		{
			return new GLinearGradient;
		}
		else if (Op == 6)
		{
			return new GRadialGradient;
		}

		return 0;
	}

} GradientFactory;
