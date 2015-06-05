#ifndef _GDISPLAY_STRING_LAYOUT_H_
#define _GDISPLAY_STRING_LAYOUT_H_

#include "GUtf8.h"
#include "GDisplayString.h"

struct GDisplayStringLayout
{
	// Min and max bounds
	GdcPt2 Min, Max;
	
	// Wrap setting
	bool Wrap;

	// Array of words... NULL ptr for new line
	GArray<GDisplayString*> Strs;

	GDisplayStringLayout()
	{
		Min.x = Max.x = 0;
		Min.y = Max.y = 0;
		
		Wrap = false;
	}
	
	~GDisplayStringLayout()
	{
		Strs.DeleteObjects();
	}

	uint32 NextChar(char *s)
	{
		int Len = 0;
		while (s[Len] && Len < 6) Len++;
		return LgiUtf8To32((uint8*&)s, Len);
	}

	uint32 PrevChar(char *s)
	{
		if (IsUtf8_Lead(*s) || IsUtf8_1Byte(*s))
		{
			s--;
			int Len = 1;
			while (IsUtf8_Trail(*s) && Len < 6) { s--; Len++; }

			return LgiUtf8To32((uint8*&)s, Len);
		}

		return 0;
	}

	// Create the lines from text
	bool Create(GFont *f, char *s, int Width)
	{
		// Empty
		Min.x = Max.x = 0;
		Min.y = Max.y = 0;
		Strs.DeleteObjects();

		// Param validation
		if (!f || !s || !*s)
			return false;

		// Loop over input
		while (*s)
		{
			char *e = strchr(s, '\n');
			if (!e) e = s + strlen(s);
			int Len = e - s;

			// Create a display string for the entire line
			GDisplayString *n = new GDisplayString(f, Len ? s : (char*)"", Len ? Len : 1);
			if (n)
			{
				// Do min / max size calculation
				Min.x = Min.x ? min(Min.x, n->X()) : n->X();
				Min.y += n->Y();
				
				Max.x = max(Max.x, n->X());
				Max.y += n->Y();
			
				if (Wrap && n->X() > Width)
				{
					// If wrapping, work out the split point and the text is too long
					int Ch = n->CharAt(Width);
					if (Ch > 0)
					{
						// Break string into chunks
						e = LgiSeekUtf8(s, Ch);
						while (e > s)
						{
							uint32 n = PrevChar(e);
							if (LGI_BreakableChar(n))
								break;
							e = LgiSeekUtf8(e, -1, s);
						}
						if (e == s)
						{
							e = LgiSeekUtf8(s, Ch);
							while (*e)
							{
								uint32 n = NextChar(e);
								if (LGI_BreakableChar(n))
									break;
								e = LgiSeekUtf8(e, 1);
							}
						}

						DeleteObj(n);
						n = new GDisplayString(f, s, e - s);
					}
				}

				Strs.Add(n);
			}
			
			s = *e == '\n' ? e + 1 : e;
		}
		
		return true;
	}
	
	void Paint(GSurface *pDC, GFont *f, GRect &rc, GColour &Fore, GColour &Back, bool Enabled)
	{
		f->Transparent(Back.Transparent());
		f->Colour(Fore, Back);

		int y = 0;
		for (unsigned i=0; i<Strs.Length(); i++)
		{
			GDisplayString *s = Strs[i];
			if (Enabled)
			{
				GRect r(0, y, rc.X()-1, y + s->Y() - 1);
				s->Draw(pDC, 0, y, &r);
			}
			else
			{
				GRect r(0, y, rc.X()-1, y + s->Y() - 1);
				f->Transparent(Back.Transparent());
				f->Colour(GColour(LC_LIGHT, 24), Back);
				s->Draw(pDC, 1, y+1, &r);
				
				f->Transparent(true);
				f->Colour(LC_LOW, LC_MED);
				s->Draw(pDC, 0, y, &r);
			}
			
			y += s->Y();
		}

		if (y < rc.Y())
		{
			pDC->Colour(Back);
			pDC->Rectangle(0, y, rc.X()-1, rc.Y()-1);
		}
	}
};

#endif