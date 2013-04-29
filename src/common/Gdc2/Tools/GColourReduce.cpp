#include <math.h>
#include "Gdc2.h"

#define TABLE_COMPSIZE	6
#define TABLE_SIZE		(1 << (3 * TABLE_COMPSIZE))
#define DIMENSIONS		3

struct ImgColour
{
	uint8 c[DIMENSIONS];
	int count;
};

struct Range
{
	int Min, Max;
	
	int Len()
	{
		return Max - Min + 1;
	}
};

struct Edge
{
	int Dimension, Length;
	
	Edge(int d, int l)
	{
		Dimension = d;
		Length = l;
	}
};

int Cmp0(ImgColour **a, ImgColour **b)
{
	return (*a)->c[0] - (*b)->c[0];
}
int Cmp1(ImgColour **a, ImgColour **b)
{
	return (*a)->c[1] - (*b)->c[1];
}
int Cmp2(ImgColour **a, ImgColour **b)
{
	return (*a)->c[2] - (*b)->c[2];
}

struct Box
{
	GArray<ImgColour*> Cols;
	Range r[DIMENSIONS];
	int Pixels;
	
	Box(ImgColour *c, int len)
	{
		LgiAssert(c && len > 0);
		if (!c || len <= 0)
			return;
		
		Cols.Length(len);
		for (int i=0; i<len; i++)
		{
			Cols[i] = c + i;
		}
		
		FindLimits();
	}

	Box(GArray<ImgColour*> &c, int Start, int End)
	{
		LgiAssert(End >= Start);
		
		Cols.Length(End - Start + 1);
		for (int i=0; i<Cols.Length(); i++)
		{
			Cols[i] = c[Start + i];
		}
		// memcpy(&Cols[0], &c[Start], Cols.Length() * sizeof(ImgColour*));
		
		FindLimits();
	}
	
	void Average(int *avg)
	{
		uint64 f[DIMENSIONS];
		for (int d=0; d<DIMENSIONS; d++)
		{
			f[d] = 0;
		}

		for (int i=0; i<Cols.Length(); i++)
		{
			ImgColour *col = Cols[i];
			for (int d=0; d<DIMENSIONS; d++)
			{
				f[d] += col->c[d] * col->count;
			}
		}
		
		for (int d=0; d<DIMENSIONS; d++)
		{
			avg[d] = f[d] / Pixels;
		}
	}
	
	void FindLimits()
	{
		LgiAssert(Cols.Length() > 0);
		
		Pixels = 0;
		for (int i=0; i<DIMENSIONS; i++)
		{
			r[i].Min = r[i].Max = Cols[0]->c[i];
		}
		Pixels += Cols[0]->count;

		for (int n=0; n<Cols.Length(); n++)
		{
			ImgColour *c = Cols[n];
			LgiAssert(c);
			for (int i=0; i<DIMENSIONS; i++)
			{
				r[i].Min = min(r[i].Min, c->c[i]);
				r[i].Max = max(r[i].Max, c->c[i]);
			}
			Pixels += c->count;
		}
	}
	
	Edge LongestDimension()
	{
		int Max = -1;
		for (int i=0; i<DIMENSIONS; i++)
		{
			if (Max < 0 || r[Max].Len() < r[i].Len())
				Max = i;
		}
		
		return Edge(Max, r[Max].Len());
	}

	bool Sort(GArray<ImgColour*> &Index, int Dim)
	{
		Index = Cols;
		if (Dim == 0)
			Index.Sort(Cmp0);
		else if (Dim == 1)
			Index.Sort(Cmp1);
		else if (Dim == 2)
			Index.Sort(Cmp2);
		else
			return false;
		return true;
	}
};

int HueSort(GColour *a, GColour *b)
{
	GColour A = *a;
	GColour B = *b;
	return A.GetH() - b->GetH();
}

int LumaSort(GColour *a, GColour *b)
{
	GColour A = *a;
	GColour B = *b;
	return A.GetL() - b->GetL();
}

template<typename B>
uint32 ColourDistance(ImgColour *a, B *b)
{
	// calculate distance
	int dr = a->c[0] - b->r;
	int dg = a->c[1] - b->g;
	int db = a->c[2] - b->b;

	// final result
	return abs(dr) + abs(dg) + abs(db);
}

bool CreatePalette(GPalette *Out, GSurface *In, int DestSize)
{
	if (!Out || !In || In->GetBits() <= 8)
		return false;

	int TableSize = TABLE_SIZE;
	int TableMemSize = sizeof(ImgColour) * TableSize;
	ImgColour *Col = new ImgColour[TableSize];
	if (!Col)
		return false;

	// scan image for colours
	memset(Col, 0, TableMemSize);
	
	int ColUsed = 0;
	for (int y=0; y<In->Y(); y++)
	{
		bool fuzzy = ColUsed > DestSize;

		switch (In->GetColourSpace())
		{
			case CsRgb24:
			{
				GPixelPtr in, end;
				in.u8 = (*In)[y];
				end.rgb24 = in.rgb24 + In->X();
				
				while (in.rgb24 < end.rgb24)
				{
					int hash =	(((int)in.rgb24->r & 0xfc) << 10) |
								(((int)in.rgb24->g & 0xfc) << 4) |
								(((int)in.rgb24->b & 0xfc) >> 2);
					
					int h;
					for (h = 0; h < TABLE_SIZE; h++)
					{
						int idx = (hash + h) % TABLE_SIZE;
						ImgColour *col = Col + idx;
						if (col->count == 0)
						{
							// Create entry for this RGB
							col->c[0] = in.rgb24->r;
							col->c[1] = in.rgb24->g;
							col->c[2] = in.rgb24->b;
							col->count++;
							ColUsed++;
							break;
						}
						else
						{
							int dist = ColourDistance(col, in.rgb24);
							if (dist == 0 || (fuzzy && dist < 3))
							{
								// Reuse the existing entry
								col->count++;
								break;
							}
						}
					}
					
					LgiAssert(h < TABLE_SIZE);
					
					in.rgb24++;
				}
				break;
			}
			case CsArgb32:
			{
				GPixelPtr in, end;
				in.u8 = (*In)[y];
				end.rgba32 = in.rgba32 + In->X();
				
				while (in.rgba32 < end.rgba32)
				{
					int hash =	(((int)in.rgba32->r & 0xfc) << 10) |
								(((int)in.rgba32->g & 0xfc) << 4) |
								(((int)in.rgba32->b & 0xfc) >> 2);
					
					int h;
					for (h = 0; h < TABLE_SIZE; h++)
					{
						int idx = (hash + h) % TABLE_SIZE;
						ImgColour *col = Col + idx;
						if (col->count == 0)
						{
							// Create entry for this RGB
							col->c[0] = in.rgba32->r;
							col->c[1] = in.rgba32->g;
							col->c[2] = in.rgba32->b;
							col->count++;
							ColUsed++;
							break;
						}
						else
						{
							int dist = ColourDistance(col, in.rgba32);
							if (dist == 0 || (fuzzy && dist < 3))
							{
								// Reuse the existing entry
								col->count++;
								break;
							}
						}
					}
					LgiAssert(h < TABLE_SIZE);
					
					in.rgba32++;
				}
				break;
			}
			default:
			{
				LgiAssert(!"Not impl.");
				break;
			}
		}
	}

	// remove unused colours
	int Cols = 0, i;
	for (i=0; i<TABLE_SIZE; i++)
	{
		if (Col[i].count != 0)
		{
			// used entry
			Col[Cols++] = Col[i];
		}
	}

	if (Cols <= DestSize)
	{
		// We have enough palette entries to colour all the input colours
		for (i=0; i<Cols; i++)
		{
			GdcRGB *p = (*Out)[i];
			p->R = Col[i].c[0];
			p->G = Col[i].c[1];
			p->B = Col[i].c[2];
		}
	}
	else
	{
		// Reduce the size of the colour set to fit
		GArray<Box*> Boxes;
		Box *b = new Box(Col, Cols);
		Boxes.Add(b);
		
		while (Boxes.Length() < DestSize)
		{
			// Find largest box...
			int MaxBox = -1;
			for (int i=0; i<Boxes.Length(); i++)
			{
				if
				(
					(
						MaxBox < 0
						||
						(Boxes[i]->Pixels > Boxes[MaxBox]->Pixels)
					)
					&&
					Boxes[i]->Cols.Length() > 1 // otherwise we can't split the colours
				)
					MaxBox = i;
			}

			Edge MaxEdge = Boxes[MaxBox]->LongestDimension();
			
			// Split that box along the median
			Box *b = Boxes[MaxBox];
			GArray<ImgColour*> Index;
			if (b->Sort(Index, MaxEdge.Dimension))
			{
				// Find the median by picking an arbitrary mid-point
				int Pos = Index.Length() >> 1;
				if (Index.Length() > 2)
				{
					int LowerSum = 0;
					int UpperSum = 0;
					for (int i=0; i<Index.Length(); i++)
					{
						if (i < Pos)
							LowerSum += Index[i]->count;
						else
							UpperSum += Index[i]->count;
					}
					// And then refining it's position iteratively
					while (LowerSum != UpperSum)
					{
						ImgColour *c;
						int Offset;
						if (LowerSum < UpperSum)
						{
							// Move pos up
							c = Index[Pos];
							Offset = 1;
						}
						else
						{
							// Move pos down
							c = Index[Pos-1];
							Offset = -1;
						}
						
						int NewLower = LowerSum + (Offset * c->count);
						int NewUpper = UpperSum + (-Offset * c->count);
						if
						(
							(LowerSum < UpperSum)
							^
							(NewLower < NewUpper) 
						)
						{
							break;
						}
						
						LowerSum = NewLower;
						UpperSum = NewUpper;
						Pos += Offset;					
					}
				}
				
				// Create new boxes out of the 2 parts
				LgiAssert(Pos > 0);
				Boxes.Add(new Box(Index, 0, Pos - 1));
				Boxes.Add(new Box(Index, Pos, Index.Length() - 1));
				
				// Delete the old 'big' box
				Boxes.Delete(b);
				DeleteObj(b);
			}
			else return false;
		}
		
		GArray<GColour> Colours;
		for (int i=0; i<Boxes.Length(); i++)
		{
			Box *in = Boxes[i];
			int avg[DIMENSIONS];
			in->Average(avg);
			
			GColour p(avg[0], avg[1], avg[2]);
			Colours.Add(p);
		}
		
		Colours.Sort(LumaSort);
		
		Out->SetSize(DestSize);
		for (int i=0; i<Colours.Length(); i++)
		{
			GColour &in = Colours[i];
			GdcRGB *out = (*Out)[i];
			if (out)
			{
				out->R = in.r();
				out->G = in.g();
				out->B = in.b();
			}
			else LgiAssert(0);
		}
	}

	DeleteArray(Col);

	return true;
}

bool GReduceBitDepth(GSurface *pDC, int Bits, GPalette *Pal, GReduceOptions *Reduce)
{
	bool Status = false;

	GSurface *pTemp = new GMemDC;
	if (pDC &&
		pTemp &&
		pTemp->Create(pDC->X(), pDC->Y(), Bits))
	{
		if (Bits <= 8 && Pal)
		{
			pTemp->Palette(new GPalette(Pal));
		}

		if ((Bits <= 8) && (pDC->GetBits() > 8) && Reduce)
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

