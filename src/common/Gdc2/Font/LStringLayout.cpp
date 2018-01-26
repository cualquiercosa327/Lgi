#include "Lgi.h"
#include "LStringLayout.h"

////////////////////////////////////////////////////////////////////////////////////
void LLayoutString::SetColours(GCss *Style)
{
	GCss::ColorDef Fill = Style->Color();
	if (Fill.Type == GCss::ColorRgb)
		Fore.Set(Fill.Rgb32, 32);
	else if (Fill.Type != GCss::ColorTransparent)
		Fore.Set(LC_TEXT, 24);
			
	Fill = Style->BackgroundColor();
	if (Fill.Type == GCss::ColorRgb)
		Back.Set(Fill.Rgb32, 32);
}

////////////////////////////////////////////////////////////////////////////////////
LStringLayout::LStringLayout(GFontCache *fc)
{
	FontCache = fc;
	Wrap = false;
	AmpersandToUnderline = false;
	Empty();
}
	
LStringLayout::~LStringLayout()
{
	Empty();
}

void LStringLayout::Empty()
{
	Min.Zero();
	Max.Zero();
	MinLines = 0;
	Bounds.ZOff(-1, -1);
	Strs.DeleteObjects();
	Text.DeleteObjects();
}

bool LStringLayout::Add(const char *Str, GCss *Style)
{
	if (!Str)
		return false;

	if (AmpersandToUnderline)
	{
		for (const char *s = Str; *s; )
		{
			const char *e = s;
			// Find '&' or end of string
			while (*e && !(e[0] == '&' && e[1] != '&'))
				e++;
			if (e == s) break; // end of string

			// Add text before '&'
			LLayoutRun *r = new LLayoutRun(Style);
			r->Text.Set(s, e - s);
			Text.Add(r);

			if (!*e)
				break; // End of string

			// Add '&'ed char
			r = new LLayoutRun(Style);
			r->TextDecoration(GCss::TextDecorUnderline);
			s = e + 1; // Skip the '&' itself
			GUtf8Ptr p(s); // Find the end of the next unicode char
			p++;
			if ((const char*)p.GetPtr() == s)
				break; // No more text: exit
			r->Text.Set(s, (const char*)p.GetPtr()-s);
			Text.Add(r);
			s = (const char*) p.GetPtr();
		}
	}
	else // No parsing required
	{
		LLayoutRun *r = new LLayoutRun(Style);
		r->Text = Str;
		Text.Add(r);
	}

	return true;
}

uint32 LStringLayout::NextChar(char *s)
{
	ssize_t Len = 0;
	while (s[Len] && Len < 6) Len++;
	return LgiUtf8To32((uint8*&)s, Len);
}

uint32 LStringLayout::PrevChar(char *s)
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

GFont *LStringLayout::GetBaseFont()
{
	return FontCache && FontCache->GetDefaultFont() ? FontCache->GetDefaultFont() : SysFont;
}

void LStringLayout::SetBaseFont(GFont *f)
{
	if (FontCache)
		FontCache->SetDefaultFont(f);
}

// Pre-layout min/max calculation
void LStringLayout::DoPreLayout(int32 &MinX, int32 &MaxX)
{		
	MinX = 0;
	MaxX = 0;
		
	GFont *f = GetBaseFont();
	if (!Text.Length() || !f)
		return;

	char White[] = " \t\r\n";
	char *LineStart = NULL;
	for (LLayoutRun **Run = NULL; Text.Iterate(Run); )
	{
		char *s = (*Run)->Text;
		while (*s)
		{
			while (*s && strchr(White, *s))
			{
				if (*s == '\n')
				{
					GDisplayString Line(f, LineStart, s - LineStart);
					MaxX = max(MaxX, Line.X());
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
			MinX = max(d.X(), MinX);

			s = *e && strchr(White, *e) ? e + 1 : e;
		}
		
		if (s > LineStart)
		{
			GDisplayString Line(f, LineStart, s - LineStart);
			MaxX = max(MaxX, Line.X());
		}
	}
}	

// Create the lines from text
bool LStringLayout::DoLayout(int Width, bool Debug)
{
	// Empty
	Min.x = Max.x = 0;
	Min.y = Max.y = 0;
	MinLines = 0;
	Strs.DeleteObjects();

	// Param validation
	GFont *f = GetBaseFont();
	if (!f || !Text.Length())
		return false;

	// Loop over input
	int /*Fx = 0,*/ y = 0;
	int LineFX = 0;
	int LineHeight = 0;
	int Shift = GDisplayString::FShift;
	MinLines = 1;

	for (LLayoutRun **Run = NULL; Text.Iterate(Run); )
	{
		char *s = (*Run)->Text;
		while (*s)
		{
			char *e = s;
			while (*e && *e != '\n')
				e++;
			size_t Len = e - s;

			GFont *Fnt;
			if (FontCache)
				Fnt = FontCache->GetFont(*Run);
			if (!Fnt)
				Fnt = f;

			// Create a display string for the segment
			LLayoutString *n = new LLayoutString(Fnt, Len ? s : (char*)"", Len ? (int)Len : 1);
			if (n)
			{
				n->Fx = LineFX;
				n->y = y;
				n->SetColours(*Run);
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
		
						n = new LLayoutString(Fnt, s, (int) (e - s));
						n->Fx = LineFX;
						n->y = y;
						n->SetColours(*Run);
						LineHeight = max(LineHeight, n->Y());

						LineFX = 0;
						MinLines++;
						y += LineHeight;
					}
				}

				GRect Sr(0, 0, n->X()-1, n->Y()-1);
				Sr.Offset(n->Fx >> Shift, n->y);
				if (Strs.Length()) Bounds.Union(&Sr);
				else Bounds = Sr;
				LineHeight = max(LineHeight, Sr.Y());

				Strs.Add(n);
			}
			
			if (*e == '\n')
			{
				MinLines++;
				y += LineHeight;
				s = e + 1;
				LineFX = 0;
			}
			else
				s = e;
		}

		Min.y = LineHeight * MinLines;
		Max.y = y + LineHeight;
		
		if (Debug)
			LgiTrace("CreateTxtLayout(%i) min=%i,%i  max=%i,%i\n",
				Width,
				Min.x, Min.y,
				Max.x, Min.y);
	}

	return true;
}
	
void LStringLayout::Paint(	GSurface *pDC,
			GdcPt2 pt,
			GColour Back,
			GRect &rc,
			bool Enabled)
{
	if (!pDC)
		return;

	#ifdef WINDOWS
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
		LLayoutString *s = dynamic_cast<LLayoutString*>(*ds);
		GFont *f = s->GetFont();
		GColour Bk = s->Back.IsTransparent() ? Back : s->Back;

		#ifdef WINDOWS
		int y = pt.y + s->y;
		GRect r(pt.x + s->Fx, y,
				pt.x + s->Fx + s->FX() - 1, y + s->Y() - 1);
		Rgn.Subtract(&r);
		f->Transparent(Bk.IsTransparent());
		#else
		f->Transparent(true);
		#endif

		// LgiTrace("'%S' @ %i,%i\n", (const char16*)(**ds), r.x1, r.y1);

		if (Enabled)
		{
			f->Colour(s->Fore, Bk);
			#ifdef WINDOWS
			s->Draw(pDC, r.x1, r.y1, &r);
			#else
			GdcPt2 k((pt.x << Shift) + s->Fx, (pt.y + s->y) << Shift);
			s->FDraw(pDC, k.x, k.y);
			#endif
		}
		else
		{
			f->Transparent(Bk.IsTransparent());
			f->Colour(GColour(LC_LIGHT, 24), Bk);
			#ifdef WINDOWS
			s->Draw(pDC, r.x1+1, r.y1+1, &r);
			#else
			GdcPt2 k(((pt.x+1) << Shift) + s->Fx, (pt.y + 1 + s->y) << Shift);
			s->FDraw(pDC, k.x, k.y);
			#endif
				
			f->Transparent(true);
			f->Colour(LC_LOW, LC_MED);
			#ifdef WINDOWS
			s->Draw(pDC, r.x1, r.y1, &r);
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
