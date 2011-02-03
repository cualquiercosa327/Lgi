/// \file
/// \author Matthew Allen
/// \created 3/2/2011
#ifndef _GCOLOUR_H_
#define _GCOLOUR_H_

/// A colour definition
class LgiClass GColour
{
public:
	enum Space
	{
		Col8,
		Col32,
	};

protected:
	class GPalette *pal;
	union {
		uint8  p8;
		uint32 p32;
	};
	Space space;

public:
	/// Transparent
	GColour()
	{
		c32(0);
	}

	/// Indexed colour
	GColour(uint8 idx8, GPalette *palette)
	{
		c8(idx8, palette);
	}

	/// True colour
	GColour(int r, int g, int b, int a = 255)
	{
		c32(Rgba32(r, g, b, a));
	}

	/// Conversion from COLOUR
	GColour(COLOUR c, int bits)
	{
		pal = 0;
		switch (bits)
		{
			case 8:
			{
				p8 = c;
				space = Col8;
				break;
			}
			case 15:
			{
				space = Col32;
				p32 = Rgb15To32(c);
				break;
			}
			case 16:
			{
				space = Col32;
				p32 = Rgb16To32(c);
				break;
			}
			case 24:
			{
				space = Col32;
				p32 = Rgb24To32(c);
				break;
			}
			case 32:
			{
				space = Col32;
				p32 = c;
				break;
			}
			default:
			{
				space = Col32;
				p32 = 0;
				LgiAssert(!"Not a known colour depth.");
			}
		}
	}

	bool Transparent()
	{
		if (space == Col32)
			return A32(p32) == 0;
		else if (space == Col8)
			return !pal || p8 < pal->GetSize();
		return true;
	}

	/// Sets the colour to a rgb(a) value
	void Rgb(int r, int g, int b, int a = 255)
	{
		c32(Rgba32(r, g, b, a));
	}

	/// Gets the red component (0-255)
	uint8 r()
	{
		return R32(c32());
	}

	/// Gets the green component (0-255)
	uint8 g()
	{
		return G32(c32());
	}
	
	/// Gets the blue component (0-255)
	uint8 b()
	{
		return B32(c32());
	}

	/// Gets the alpha component (0-255)
	uint8 a()
	{
		return A32(c32());
	}

	// Gets the indexed colour
	uint8 c8()
	{
		return p8;
	}

	// Sets indexed colour
	void c8(uint8 c, GPalette *p)
	{
		space = Col8;
		pal = p;
		p8 = c;
	}

	// Get as 24 bit colour
	uint32 c24()
	{
		if (space == Col32)
		{
			return Rgb32To24(p32);
		}
		else if (space == Col8)
		{
			if (pal)
			{
				// colour palette lookup
				if (p8 < pal->GetSize())
				{
					GdcRGB *c = (*pal)[p8];
					if (c)
					{
						return Rgb24(c->R, c->G, c->B);
					}
				}

				return 0;
			}
			
			return Rgb24(p8, p8, p8); // monochome
		}

		// Black...
		return 0;
	}

	/// Set 24 bit colour
	void c24(COLOUR c)
	{
		space = Col32;
		p32 = Rgb24To32(c);
		pal = 0;
	}

	/// Get 32 bit colour
	COLOUR c32()
	{
		if (space == Col32)
		{
			return p32;
		}
		else if (space == Col8)
		{
			if (pal)
			{
				// colour palette lookup
				if (p8 < pal->GetSize())
				{
					GdcRGB *c = (*pal)[p8];
					if (c)
					{
						return Rgb32(c->R, c->G, c->B);
					}
				}

				return 0;
			}
			
			return Rgb32(p8, p8, p8); // monochome
		}

		// Transparent?
		return 0;
	}

	/// Set 32 bit colour
	void c32(COLOUR c)
	{
		space = Col32;
		pal = 0;
		p32 = c;
	}
};

#endif