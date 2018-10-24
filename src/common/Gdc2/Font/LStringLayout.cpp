#include "Lgi.h"
#include "LStringLayout.h"

#define DEBUG_PROFILE_LAYOUT		0

static char White[] = " \t\r\n";

////////////////////////////////////////////////////////////////////////////////////
void LLayoutString::Set(int LineIdx, int FixX, int YPx, LLayoutRun *Lr, ssize_t Start)
{
	Line = LineIdx;
	Src = Lr;
	Fx = FixX;
	y = YPx;
	Offset = Start;

	GCss::ColorDef Fill = Src->Color();
	if (Fill.Type == GCss::ColorRgb)
		Fore.Set(Fill.Rgb32, 32);
	else if (Fill.Type != GCss::ColorTransparent)
		Fore.Set(LC_TEXT, 24);
	
	Fill = Src->BackgroundColor();
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
		LLayoutRun *r;

		for (const char *s = Str; *s; )
		{
			const char *e = s;
			// Find '&' or end of string
			while (*e && !(e[0] == '&' && e[1] != '&'))
				e++;

			if (e > s)
			{
				// Add text before '&'
				r = new LLayoutRun(Style);
				r->Text.Set(s, e - s);
				Text.Add(r);
			}
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

typedef GArray<LLayoutString*> LayoutArray;

// Pre-layout min/max calculation
void LStringLayout::DoPreLayout(int32 &MinX, int32 &MaxX)
{		
	MinX = 0;
	MaxX = 0;
		
	GFont *f = GetBaseFont();
	if (!Text.Length() || !f)
		return;

	GArray<LayoutArray> Lines;
	int Line = 0;

	for (LLayoutRun **Run = NULL; Text.Iterate(Run); )
	{
		char *s = (*Run)->Text;
		GFont *f = FontCache ? FontCache->GetFont(*Run) : SysFont;
		LgiAssert(f != NULL);

		char *Start = s;
		while (*s)
		{
			if (*s == '\n')
			{
				Lines[Line++].Add(new LLayoutString(f, Start, s - Start));
				Start = s + 1;
			}
			s++;
		}
		if (s > Start)
			Lines[Line].Add(new LLayoutString(f, Start, s - Start));

		s = (*Run)->Text;
		while (*s)
		{
			while (*s && strchr(White, *s))
				s++;

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
				break;

			GDisplayString d(f, s, (int) (e - s));
			MinX = MAX(d.X(), MinX);

			s = e;
		}
	}

	for (unsigned i=0; i<Lines.Length(); i++)
	{
		int Fx = 0;
		LayoutArray &Ln = Lines[i];
		for (unsigned n=0; n<Ln.Length(); n++)
		{
			LLayoutString *s = Ln[n];
			Fx += s->FX();
		}

		int LineX = Fx >> GDisplayString::FShift;
		MaxX = MAX(MaxX, LineX);
	}
}	

struct Break
{
	LLayoutRun *Run;
	ssize_t Bytes;

	void Set(LLayoutRun *r, ssize_t b)
	{
		Run = r;
		Bytes = b;
	}
};

#define GotoNextLine()			\
	LineFX = 0;					\
	MinLines++;					\
	y += LineHeight;			\
	StartLine = Strs.Length();	\
	Breaks.Empty();

// Create the lines from text
bool LStringLayout::DoLayout(int Width, int MinYSize, bool Debug)
{
	#if DEBUG_PROFILE_LAYOUT
	GProfile Prof("LStringLayout::DoLayout");
	Prof.HideResultsIfBelow(100);
	#endif

	// Empty
	Min.x = Max.x = 0;
	Min.y = Max.y = 0;
	MinLines = 0;
	Strs.DeleteObjects();

	// Param validation
	GFont *f = GetBaseFont();
	if (!f || !Text.Length() || Width <= 0)
	{
		Min.y = Max.y = MAX((f ? f : SysFont)->GetHeight(), MinYSize);
		return false;
	}

	// Loop over input
	int y = 0;
	int LineFX = 0;
	int LineHeight = 0;
	int Shift = GDisplayString::FShift;

	// By definition these potential break points are all
	// on the same line. Clear the array on each new line.
	GArray<Break> Breaks;
	
	/// Index of the display string starting the line
	ssize_t StartLine = 0;

	/// There is alway one line of text... even if it's empty
	MinLines = 1;

	// LgiTrace("Text.Len=%i\n", Text.Length());
	for (LLayoutRun **Run = NULL; Text.Iterate(Run); )
	{
		char *Start = (*Run)->Text;
		GUtf8Ptr s(Start);

		// LgiTrace("    Run='%s' %p\n", s.GetPtr(), *Run);
		#if DEBUG_PROFILE_LAYOUT
		GString Pm;
		Pm.Printf("[%i] Run = '%.*s'\n", (int) (Run - Text.AddressOf()), max(20, (*Run)->Text.Length()), s.GetPtr());
		Prof.Add(Pm);
		#endif

		while (s)
		{
			GUtf8Ptr e(s);
			ssize_t StartOffset = (char*)s.GetPtr() - Start;
			for (uint32 Ch; (Ch = e); e++)
			{
				if (Ch == '\n')
					break;

				if ((char*)e.GetPtr() > Start &&
					LGI_BreakableChar(Ch))
					Breaks.New().Set(*Run, (char*)e.GetPtr() - Start);
			}

			ssize_t Bytes = e - s;

			GFont *Fnt = NULL;
			if (FontCache)
				Fnt = FontCache->GetFont(*Run);
			if (!Fnt)
				Fnt = f;

			#if DEBUG_PROFILE_LAYOUT
			Prof.Add("CreateStr");
			#endif

			// Create a display string for the segment
			LLayoutString *n = new LLayoutString(Fnt, Bytes ? (char*)s.GetPtr() : (char*)"", Bytes ? (int)Bytes : 1);
			if (n)
			{
				n->Set(MinLines - 1, LineFX, y, *Run, StartOffset);
				LineFX += n->FX();
				
				// Do min / max size calculation
				Min.x = Min.x ? MIN(Min.x, LineFX) : LineFX;
				Max.x = MAX(Max.x, LineFX);
			
				if (Wrap && (LineFX >> Shift) > Width)
				{
					// If wrapping, work out the split point and the text is too long
					ssize_t Chars = n->CharAt(Width - (n->Fx >> Shift));

					if (Breaks.Length() > 0)
					{
						#if DEBUG_PROFILE_LAYOUT
						Prof.Add("WrapWithBreaks");
						#endif

						// Break at previous word break
						Strs.Add(n);
							
						// Roll back till we get a break point that fits
						for (ssize_t i = Breaks.Length() - 1; i >= 0; i--)
						{
							Break &b = Breaks[i];

							// LgiTrace("        Testing Break %i: %p %i\n", i, b.Run, b.Bytes);

							// Calc line width from 'StartLine' to 'Break[i]'
							int FixX = 0;
							GAutoPtr<LLayoutString> Broken;
							size_t k;
							for (k=StartLine; k<Strs.Length(); k++)
							{
								LLayoutString *Ls = dynamic_cast<LLayoutString*>(Strs[k]);
								if (!Ls) return false;

								if (b.Run == Ls->Src &&
									b.Bytes >= Ls->Offset)
								{
									// Add just the part till the break
									if (Broken.Reset(new LLayoutString(Ls, b.Run->Text.Get() + Ls->Offset, b.Bytes - Ls->Offset)))
										FixX += Broken->X();
									break;
								}
								else
									// Add whole string
									FixX += Ls->FX();
							}

							// LgiTrace("        Len=%i of %i\n", FixX, Width);

							if ((FixX >> Shift) <= Width ||
								i == 0)
							{
								// Found a good fit...
								// LgiTrace("        Found a fit\n");

								// So we want to keep 'StartLine' to 'k-1' as is...
								while (Strs.Length() > k)
								{
									delete Strs.Last();
									Strs.PopLast();
								}

								if (Broken)
								{
									// Then change out from 'k' onwards for 'Broken'
									LineHeight = MAX(LineHeight, Broken->Y());
									Strs.Add(Broken.Release());
								}

								LineFX = FixX;
								s = b.Run->Text.Get() + b.Bytes;

								ssize_t Idx = Text.IndexOf(b.Run);
								if (Idx < 0)
								{
									LgiAssert(0);
									return false;
								}

								Run = Text.AddressOf(Idx);
								break;
							}
						}

						// Start pouring from the break pos...
						// s = BreakRun->Text.Get() + BreakBytes;
						while (s && strchr(White, s))
							s++;

						// Move to the next line...
						GotoNextLine();
						continue;
					}
					else
					{
						#if DEBUG_PROFILE_LAYOUT
						Prof.Add("WrapNoBreaks");
						#endif

						// Break at next word break
						e = s;
						e += Chars;
						for (uint32 Ch; (Ch = e); e++)
						{
							if (LGI_BreakableChar(Ch))
								break;
						}
					}

					LineFX -= n->FX();
					DeleteObj(n);
		
					n = new LLayoutString(Fnt, (char*)s.GetPtr(), e - s);
					n->Set(MinLines - 1, LineFX, y, *Run, (char*)s.GetPtr() - Start);
					LineHeight = MAX(LineHeight, n->Y());

					GotoNextLine();
				}

				#if DEBUG_PROFILE_LAYOUT
				Prof.Add("Bounds");
				#endif

				GRect Sr(0, 0, n->X()-1, n->Y()-1);
				Sr.Offset(n->Fx >> Shift, n->y);
				if (Strs.Length()) Bounds.Union(&Sr);
				else Bounds = Sr;
				LineHeight = MAX(LineHeight, Sr.Y());

				Strs.Add(n);
			}
			
			if (e == '\n')
			{
				s = ++e;
				#if DEBUG_PROFILE_LAYOUT
				Prof.Add("NewLine");
				#endif
				GotoNextLine();
			}
			else
				s = e;
		}
	}

	#if DEBUG_PROFILE_LAYOUT
	Prof.Add("Post");
	#endif

	Min.y = LineHeight * MinLines;
	if (Min.y < MinYSize)
		Min.y = MinYSize;
	Max.y = y + LineHeight;
	if (Max.y < MinYSize)
		Max.y = MinYSize;
	
	if (Debug)
		LgiTrace("CreateTxtLayout(%i) min=%i,%i  max=%i,%i\n",
			Width,
			Min.x, Min.y,
			Max.x, Min.y);

	return true;
}
	
void LStringLayout::Paint(	GSurface *pDC,
							GdcPt2 pt,
							GColour Back,
							GRect &rc,
							bool Enabled,
							bool Focused)
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
	
	GColour FocusFore(LC_FOCUS_SEL_FORE, 24);
	
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
			f->Colour(Focused ? FocusFore : s->Fore, Bk);
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
