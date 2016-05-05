#ifndef _GDISPLAY_STRING_LAYOUT_H_
#define _GDISPLAY_STRING_LAYOUT_H_

#include "GUtf8.h"
#include "GDisplayString.h"

struct GDisplayStringLayout
{
	// Min and max bounds
	GdcPt2 Min, Max;
	int MinLines;
	
	// Wrap setting
	bool Wrap;

	// Array of words... NULL ptr for new line
	GArray<GDisplayString*> Strs;

	GDisplayStringLayout()
	{
		Min.x = Max.x = 0;
		Min.y = Max.y = 0;
		MinLines = 0;		
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

	// Pre-layout min/max calculation
	void DoPreLayout(GFont *f, char *t, int32 &Min, int32 &Max)
	{
		int MyMin = 0;
		int MyMax = 0;

		if (t && f)
		{
			GDisplayString Sp(f, " ");
			int X = 0;

			char White[] = " \t\r\n";
			for (char *s = t; *s; )
			{
				while (*s && strchr(White, *s))
				{
					if (*s == '\n')
						X = 0;
					s++;
				}

				char *e = s;
				while (*e)
				{
					uint32 c = NextChar(e);
					if (c == 0)
						break;
					if (e > s && LGI_BreakableChar(c))
						break;
					e = LgiSeekUtf8(e, 1);
				}

				GDisplayString d(f, s, (int) (e - s));
				MyMin = max(d.X(), MyMin);
				X += d.X() + (*e == ' ' ? Sp.X() : 0);
				MyMax = max(X, MyMax);

				s = *e && strchr(White, *e) ? e + 1 : e;
			}
		}

		Min = max(Min, MyMin);
		Max = max(Max, MyMax);
	}	

	// Create the lines from text
	bool DoLayout(GFont *f, char *s, int Width, bool Debug = false)
	{
		// Empty
		Min.x = Max.x = 0;
		Min.y = Max.y = 0;
		MinLines = 0;
		Strs.DeleteObjects();

		// Param validation
		if (!f || !s || !*s)
			return false;

		// Loop over input
		while (*s)
		{
			char *e = strchr(s, '\n');
			if (!e) e = s + strlen(s);
			size_t Len = e - s;
			MinLines++;

			// Create a display string for the entire line
			GDisplayString *n = new GDisplayString(f, Len ? s : (char*)"", Len ? (int)Len : 1);
			if (n)
			{
				// Do min / max size calculation
				Min.x = Min.x ? min(Min.x, n->X()) : n->X();
				Max.x = max(Max.x, n->X());
			
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
						n = new GDisplayString(f, s, (int) (e - s));
						MinLines--;
					}
				}

				Strs.Add(n);
			}
			
			s = *e == '\n' ? e + 1 : e;
		}

		Min.y = f->GetHeight() * MinLines;
		Max.y = f->GetHeight() * Strs.Length();
		
		if (Debug)
			LgiTrace("CreateTxtLayout(%i) min=%i,%i  max=%i,%i\n",
				Width,
				Min.x, Min.y,
				Max.x, Min.y);
		
		return true;
	}
	
	void Paint(	GSurface *pDC,
				GFont *f,
				GdcPt2 pt,
				GRect &rc,
				GColour &Fore,
				GColour &Back,
				bool Enabled)
	{
		GRegion Rgn(rc);
		
		if (!pDC || !f)
			return;
		
		if (Enabled)
		{
			f->Transparent(Back.IsTransparent());
			f->Colour(Fore, Back);
		}

		// Draw all the text
		int y = pt.y;
		for (unsigned i=0; i<Strs.Length(); i++)
		{
			GDisplayString *s = Strs[i];
			GRect r(rc.x1, y, rc.x2, y + s->Y() - 1);
			Rgn.Subtract(&r);

			if (Enabled)
			{
				s->Draw(pDC, rc.x1, y, &r);
			}
			else
			{
				f->Transparent(Back.IsTransparent());
				f->Colour(GColour(LC_LIGHT, 24), Back);
				s->Draw(pDC, pt.x+1, y+1, &r);
				
				f->Transparent(true);
				f->Colour(LC_LOW, LC_MED);
				s->Draw(pDC, pt.x, y, &r);
			}
			
			y += s->Y();
		}

		// Fill any remaining area with background...
		if (!Back.IsTransparent())
		{
			pDC->Colour(Back);
			for (GRect *r=Rgn.First(); r; r=Rgn.Next())
			{
				pDC->Rectangle(r);
			}
		}
	}
};

#endif
