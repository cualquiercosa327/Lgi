#ifndef _GDISPLAY_STRING_LAYOUT_H_
#define _GDISPLAY_STRING_LAYOUT_H_

#include "GUtf8.h"
#include "GDisplayString.h"

#define AMP_TO_UNDERLINE	1

struct LayoutString : public GDisplayString
{
	int Fx, y;
	
	LayoutString(GFont *f,
				const char *s,
				ssize_t l = -1,
				GSurface *pdc = 0) :
		GDisplayString(f, s, l, pdc)
	{
		Fx = y = 0;
	}
};

struct GDisplayStringLayout
{
	// Min and max bounds
	GdcPt2 Min, Max;
	int MinLines;
	
	// Wrap setting
	bool Wrap;

	// Array of display strings...
	GArray<GDisplayString*> Strs;
	GRect StrBounds;
	
	#if AMP_TO_UNDERLINE
	GFont *PrevFont;
	GAutoPtr<GFont> Under;

	void RemoveAmp(GString &s)
	{
		char *i = s.Get(), *o = s.Get();
		while (*i)
		{
			if (*i == '&' && i[1] != '&')
				;
			else
				*o++ = *i;
			
			i++;
		}
		s.Length(o - s.Get());
	}
	#endif

	GDisplayStringLayout()
	{
		Min.x = Max.x = 0;
		Min.y = Max.y = 0;
		MinLines = 0;		
		Wrap = false;

		#if AMP_TO_UNDERLINE
		PrevFont = NULL;
		#endif
	}
	
	~GDisplayStringLayout()
	{
		Strs.DeleteObjects();
	}

	uint32 NextChar(char *s)
	{
		ssize_t Len = 0;
		while (s[Len] && Len < 6) Len++;
		return LgiUtf8To32((uint8*&)s, Len);
	}

	uint32 PrevChar(char *s)
	{
		if (IsUtf8_Lead(*s) || IsUtf8_1Byte(*s))
		{
			s--;
			ssize_t Len = 1;
			while (IsUtf8_Trail(*s) && Len < 6) { s--; Len++; }

			return LgiUtf8To32((uint8*&)s, Len);
		}

		return 0;
	}

	GFont *GetUnderlineFont(GFont *f)
	{
		#if AMP_TO_UNDERLINE
		if (PrevFont != f || !Under)
		{
			PrevFont = f;
			if (Under.Reset(new GFont))
			{
				*Under = *f;
				Under->Underline(true);
				Under->Create();
			}
		}
		return Under;
		#else
		return NULL;
		#endif
	}

	// Pre-layout min/max calculation
	void DoPreLayout(GFont *f, char *t, int32 &Min, int32 &Max)
	{		
		Min = 0;
		Max = 0;
		
		if (!t || !f)
			return;

		#if AMP_TO_UNDERLINE
		GString Str = t;
		RemoveAmp(Str);
		t = Str.Get();
		#endif
		
		char *LineStart = t;
		char White[] = " \t\r\n";
		char *s = t;
		while (*s)
		{
			while (*s && strchr(White, *s))
			{
				if (*s == '\n')
				{
					GDisplayString Line(f, LineStart, s - LineStart);
					Max = max(Max, Line.X());
					LineStart = s + 1;
				}
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
				
				char *cur = e;
				e = LgiSeekUtf8(e, 1);
				if (e == cur) // sanity check...
				{
					LgiAssert(!"LgiSeekUtf8 broke.");
					break;
				}
			}
			
			if (e == s)
			{
				LgiAssert(0);
				break;
			}

			GDisplayString d(f, s, (int) (e - s));
			Min = max(d.X(), Min);

			s = *e && strchr(White, *e) ? e + 1 : e;
		}
		
		if (s > LineStart)
		{
			GDisplayString Line(f, LineStart, s - LineStart);
			Max = max(Max, Line.X());
		}
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
		int /*Fx = 0,*/ y = 0;
		int LineFX = 0;
		int Shift = GDisplayString::FShift;
		MinLines = 1;

		while (*s)
		{
			char *e = s;
			GFont *Fnt = f;
			#if AMP_TO_UNDERLINE
			bool IsUnderline = *s == '&' && s[1] != '&';
			if (IsUnderline)
			{
				s++;
				e = LgiSeekUtf8(s, 1);
				Fnt = GetUnderlineFont(f);
			}
			else
			#endif
			{
				for (; *e; e++)
				{
					if
					(
						#if AMP_TO_UNDERLINE
						(*e == '&' && e[1] != '&')
						||
						#endif
						(*e == '\n')
					)					
						break;
				}			
			}
			size_t Len = e - s;
			
			// Create a display string for the segment
			LayoutString *n = new LayoutString(Fnt, Len ? s : (char*)"", Len ? (int)Len : 1);
			if (n)
			{
				n->Fx = LineFX;
				n->y = y;
				LineFX += n->FX();
				
				// Do min / max size calculation
				Min.x = Min.x ? min(Min.x, LineFX) : LineFX;
				Max.x = max(Max.x, LineFX);
			
				if (Wrap && (LineFX >> Shift) > Width)
				{
					// If wrapping, work out the split point and the text is too long
					ssize_t Ch = n->CharAt(Width - (n->Fx >> Shift));
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

						LineFX -= n->FX();
						DeleteObj(n);
						n = new LayoutString(f, s, (int) (e - s));
						
						LineFX = 0;
						MinLines++;
						y += f->GetHeight();
					}
				}

				GRect Sr(0, 0, n->X()-1, n->Y()-1);
				Sr.Offset(n->Fx >> Shift, n->y);
				if (Strs.Length()) StrBounds.Union(&Sr);
				else StrBounds = Sr;

				Strs.Add(n);
			}
			
			if (*e == '\n')
			{
				MinLines++;
				y += f->GetHeight();
				s = e + 1;
			}
			else
				s = e;
		}

		Min.y = f->GetHeight() * MinLines;
		Max.y = y + f->GetHeight();
		
		if (Debug)
			LgiTrace("CreateTxtLayout(%i) min=%i,%i  max=%i,%i\n",
				Width,
				Min.x, Min.y,
				Max.x, Min.y);
		
		return true;
	}
	
	void Paint(	GSurface *pDC,
				GdcPt2 pt,
				GRect &rc,
				GColour &Fore,
				GColour &Back,
				bool Enabled)
	{
		if (!pDC)
			return;

		#ifdef WINDOWS
		int y = pt.y;
		GRegion Rgn(rc);
		#else
		// Fill the background...
		if (!Back.IsTransparent())
		{
			pDC->Colour(Back);
			pDC->Rectangle(&rc);
		}
		int Shift = GDisplayString::FShift;
		#endif		
		
		// Draw all the text
		for (GDisplayString **ds = NULL; Strs.Iterate(ds); )
		{
			LayoutString *s = dynamic_cast<LayoutString*>(*ds);
			GFont *f = s->GetFont();
			#ifdef WINDOWS
			GRect r(pt.x + s->Fx, y,
					pt.x + s->Fx + s->FX() - 1, y + s->Y() - 1);
			Rgn.Subtract(&r);
			f->Transparent(Back.IsTransparent());
			#else
			f->Transparent(true);
			#endif

			if (Enabled)
			{
				f->Colour(Fore, Back);
				#ifdef WINDOWS
				s->Draw(pDC, pt.x + s->Fx, pt.y, &r);
				#else
				GdcPt2 k((pt.x << Shift) + s->Fx, (pt.y + s->y) << Shift);
				s->FDraw(pDC, k.x, k.y);
				#endif
			}
			else
			{
				f->Transparent(Back.IsTransparent());
				f->Colour(GColour(LC_LIGHT, 24), Back);
				#ifdef WINDOWS
				s->Draw(pDC, pt.x+1, y+1, &r);
				#else
				GdcPt2 k(((pt.x+1) << Shift) + s->Fx, (pt.y + 1 + s->y) << Shift);
				s->FDraw(pDC, k.x, k.y);
				#endif
				
				f->Transparent(true);
				f->Colour(LC_LOW, LC_MED);
				#ifdef WINDOWS
				s->Draw(pDC, pt.x, y, &r);
				#else
				s->FDraw(pDC, (pt.x << Shift) + s->Fx, (pt.y + s->y) << Shift);
				#endif
			}
		}

		#ifdef WINDOWS
		// Fill any remaining area with background...
		if (!Back.IsTransparent())
		{
			pDC->Colour(Back);
			for (GRect *r=Rgn.First(); r; r=Rgn.Next())
				pDC->Rectangle(r);
		}
		#endif
	}
};

#endif
