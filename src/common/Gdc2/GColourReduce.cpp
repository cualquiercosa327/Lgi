#include <math.h>
#include "Gdc2.h"

#define TABLE_SIZE	(32<<10)
class ImgColour
{
public:
	uint r, g, b, pixels;
	ushort ar, ag, ab;

	void Calc()
	{
		// calculate average RGB
		ar = (r + (pixels>>1)) / pixels;
		ag = (g + (pixels>>1)) / pixels;
		ab = (b + (pixels>>1)) / pixels;
	}

	void Merge(ImgColour *c)
	{
		r += c->r;
		g += c->g;
		b += c->b;
		pixels += c->pixels;
		Calc();

		c->r = c->g = c->b = 0;
		c->ar = c->ag = c->ab = 0;
		c->pixels = 0;
	}
};

class ImgCell
{
public:
	List<ImgColour> Colours;
};

uint32 ColourDistance(ImgColour *a, ImgColour *b)
{
	// calculate distance
	int dr = a->ar - b->ar;
	int dg = a->ag - b->ag;
	int db = a->ab - b->ab;

	// final result
	return (uint32)sqrt((double) ((dr*dr) + (dg*dg) + (db*db)));
}

bool CreatePalette(GPalette *Out, GSurface *In, int DestSize)
{
	if (Out AND In AND In->GetBits() > 8)
	{
		ImgColour *Col = new ImgColour[TABLE_SIZE];
		if (Col)
		{
			// scan image for colours
			memset(Col, 0, sizeof(ImgColour) * TABLE_SIZE);
			for (int y=0; y<In->Y(); y++)
			{
				for (int x=0; x<In->X(); x++)
				{
					COLOUR c = CBit(24, In->Get(x, y), In->GetBits());
					int i = Rgb24To15(c) & 0x7FFF;
					Col[i].r += R24(c);
					Col[i].g += G24(c);
					Col[i].b += B24(c);
					Col[i].pixels++;
				}
			}

			// remove unused colours
			int Cols = 0, i;
			for (i=0; i<TABLE_SIZE; i++)
			{
				if (Col[i].pixels != 0)
				{
					// used entry
					Col[Cols] = Col[i];
					Col[Cols++].Calc();
				}
			}

			// reduce colourset
			int Sides = 4;
			int TableSize = 1 << (Sides*3);
			ImgCell *Cell = new ImgCell[TableSize];
			if (Cell)
			{
				int Div = 255/Sides;
				int ActiveCells = 0;
				for (i=0; i<Cols; i++)
				{
					int Index = ((Col[i].ar/Div)<<(2*Sides)) + 
								((Col[i].ag/Div)<<(Sides)) +
								(Col[i].ab/Div);
					
					if (Cell[Index].Colours.Length() == 0) ActiveCells++;
					Cell[Index].Colours.Insert(Col+i);
				}

				// work out how many colours per cell we should have...
				// roughly
				int DSize = DestSize;
				int SSize = Cols;
				double Scale = (double) DSize / SSize;
				int Result = 0;

				for (i=0; i<TableSize; i++)
				{
					int Items = Cell[i].Colours.Length();
					if (Items > 0)
					{
						// active cell

						// we want about this many colours
						int Aim = (int) ((double)Items * Scale);
						if (Aim < 1) Aim = 1;			// obviously we need at least 1 colour
														// to represent this space
						DSize -= Aim;
						SSize -= Items;
						Scale = (double) DSize / SSize; // recalc scale so we
														// don't drift off target to much

						// now reduce the list of colours to our aim
						while (Cell[i].Colours.Length() > Aim)
						{
							// find least number of pixels
							ImgColour *Least = 0;
							ImgColour *c;
							for (c = Cell[i].Colours.First(); c; c = Cell[i].Colours.Next())
							{
								LgiAssert(c->pixels);

								if (!Least) Least = c;
								else if (c->pixels < Least->pixels)
								{
									Least = c;
								}
							}

							LgiAssert(Least);
							if (Least)
							{
								// remove colour from list so we can find the nearest
								Cell[i].Colours.Delete(Least);

								// find closest neighbour
								ImgColour *Closest = 0;
								uint Dist = ~0L;
								for (c = Cell[i].Colours.First(); c; c = Cell[i].Colours.Next())
								{
									LgiAssert(c->pixels);

									uint d = ColourDistance(c, Least);
									if (d < Dist)
									{
										Dist = d;
										Closest = c;
									}
								}

								LgiAssert(Closest);
								if (Closest)
								{
									Closest->Merge(Least);
								}
							}
						}

						// calculate our result set
						Result += Cell[i].Colours.Length();
					}
				}

				// write output
				int Index = 0;
				for (i=0; i<TableSize AND Index<DestSize; i++)
				{
					for (ImgColour *c = Cell[i].Colours.First(); c; c = Cell[i].Colours.Next())
					{
						LgiAssert(c->pixels);

						GdcRGB *rgb = (*Out)[Index++];
						if (rgb)
						{
							rgb->R = c->ar;
							rgb->G = c->ag;
							rgb->B = c->ab;
						}
					}
				}
			}

			DeleteArray(Cell);
			DeleteArray(Col);

			return true;
		}
	}

	return false;
}

bool GReduceBitDepth(GSurface *pDC, int Bits, GPalette *Pal, GReduceOptions *Reduce)
{
	bool Status = false;

	GSurface *pTemp = new GMemDC;
	if (pDC AND
		pTemp AND
		pTemp->Create(pDC->X(), pDC->Y(), Bits))
	{
		if (Bits <= 8 AND Pal)
		{
			pTemp->Palette(new GPalette(Pal));
		}

		if ((Bits <= 8) AND (pDC->GetBits() > 8) AND Reduce)
		{
			// dithering

			// palette
			if (!pTemp->Palette())
			{
				// currently has no palette
				switch (Reduce->PalType)
				{
					case CR_PAL_CUBE:
					{
						// RGB cube palette
						pTemp->Palette(Pal = new GPalette);
						if (Pal)
						{
							Pal->CreateCube();
						}
						break;
					}
					case CR_PAL_OPT:
					{
						// optimal palette
						pTemp->Palette(Pal = new GPalette(0, Reduce->Colours));
						if (Pal)
						{
							CreatePalette(Pal, pDC, Reduce->Colours);
						}
						break;
					}
				}
			}

			// colour conversion
			int ReduceType = REDUCE_NEAREST;
			switch (Reduce->MatchType)
			{
				case CR_MATCH_HALFTONE:
				{
					Pal->CreateCube();
					ReduceType = REDUCE_HALFTONE;
					break;
				}
				case CR_MATCH_ERROR:
				{
					ReduceType = REDUCE_ERROR_DIFFUSION;
					break;
				}
			}

			int OldType = GdcD->GetOption(GDC_REDUCE_TYPE);
			GdcD->SetOption(GDC_REDUCE_TYPE, ReduceType);
			pTemp->Blt(0, 0, pDC);
			GdcD->SetOption(GDC_REDUCE_TYPE, OldType);
		}
		else
		{
			// direct convert
			pTemp->Blt(0, 0, pDC);
		}

		if (pDC->Create(pTemp->X(), pTemp->Y(), pTemp->GetBits()))
		{
			if (pTemp->Palette())
			{
				pDC->Palette(new GPalette(pTemp->Palette()));
			}
			pDC->Blt(0, 0, pTemp);
			Status = true;
		}
	}

	DeleteObj(pTemp);
	return Status;
}

