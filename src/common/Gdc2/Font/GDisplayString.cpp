//////////////////////////////////////////////////////////////////////
// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Lgi.h"
#include "GVariant.h"
#include "GFontSelect.h"
#include "GdiLeak.h"
#include "GUtf8.h"

#ifdef FontChange
#undef FontChange
#endif

//////////////////////////////////////////////////////////////////////
#define SubtractPtr(a, b)			(	(((NativeInt)(a))-((NativeInt)(b))) / sizeof(*a)	)
#define IsTabChar(c)				(c == '\t' || (c == 0x2192 && VisibleTab))

static OsChar GDisplayStringDots[] = {'.', '.', '.', 0};

class CharInfo
{
public:
	OsChar *Str;
	uint16 Len;
	uint16 X;
	uint8 FontId;
	int8 SizeDelta;

	CharInfo()
	{
		Str = 0;
		Len = 0;
		X = 0;
		FontId = 0;
		SizeDelta = 0;
	}
};

GDisplayString::GDisplayString(GFont *f, const char *s, int l, GSurface *pdc)
{
	pDC = pdc;
	Font = f;
	
	#if defined(MAC) || WIN32NATIVE
	Str = LgiNewUtf8To16(s, l);
	#else
	Str = NewStr(s, l);
	if (!LgiIsUtf8(Str))
		printf("%s:%i - Not valid utf\n", _FL);
	#endif
	
	Info = 0;
	Blocks = x = y = len = 0;
	TabOrigin = 0;
	LaidOut = 0;
	AppendDots = 0;
	VisibleTab = 0;
	
	#if defined MAC && !defined COCOA
	
	Hnd = 0;
	if (Font && Str)
	{
		len = l >= 0 ? l : StrlenW(Str);
		if (len > 0)
		{
			ATSUCreateTextLayout(&Hnd);
		}
	}
	
	#elif defined __GTK_H__
	
	Hnd = 0;
	if (Font && Str)
	{
		len = l >= 0 ? l : strlen(Str);
		if (len > 0)
		{
			Hnd = Gtk::pango_layout_new(GFontSystem::Inst()->GetContext());
		}
	}
	
	#endif
}

GDisplayString::GDisplayString(GFont *f, const char16 *s, int l, GSurface *pdc)
{
	pDC = pdc;
	Font = f;

    #if defined(MAC) || WIN32NATIVE	
	Str = NewStrW(s, len = l);
	#else
	Str = LgiNewUtf16To8(s, l < 0 ? -1 : l * sizeof(char16));
	len = Str ? strlen(Str) : 0;
	#endif	
	
	Info = 0;
	Blocks = x = y = 0;
	TabOrigin = 0;
	LaidOut = 0;
	AppendDots = 0;
	VisibleTab = 0;

	#if defined MAC && !defined COCOA
	
	Hnd = 0;
	if (Font && Str)
	{
		len = l >= 0 ? l : StrlenW(Str);
		if (len > 0)
		{
			OSStatus e = ATSUCreateTextLayout(&Hnd);
			if (e) printf("%s:%i - ATSUCreateTextLayout failed with %i.\n", __FILE__, __LINE__, (int)e);
		}
	}
	
	#elif defined __GTK_H__
	
	Hnd = 0;
	if (Font && Str && len > 0)
	{
		Hnd = Gtk::pango_layout_new(GFontSystem::Inst()->GetContext());
	}
	
	#endif
}

GDisplayString::~GDisplayString()
{
	#if defined MAC && !defined COCOA
	if (Hnd)
		ATSUDisposeTextLayout(Hnd);
	#elif defined __GTK_H__
	if (Hnd)
		g_object_unref(Hnd);
	#endif
	
	DeleteArray(Str);
	DeleteArray(Info);
}

void GDisplayString::Layout()
{
    if (LaidOut)
        return;

    LaidOut = 1;
    
	#if defined __GTK_H__
	
	if (!Hnd)
		return;

	if (!LgiIsUtf8(Str))
		LgiTrace("%s:%i - Not utf8\n", _FL);

	Gtk::pango_context_set_font_description(GFontSystem::Inst()->GetContext(), Font->Handle());

	if (Font->TabSize())
	{
		int Len = 32;
		Gtk::PangoTabArray *t = Gtk::pango_tab_array_new(Len, true);
		if (t)
		{
			for (int i=0; i<Len; i++)
			{
				Gtk::pango_tab_array_set_tab(t, i, Gtk::PANGO_TAB_LEFT, i * Font->TabSize());
			}
			
			Gtk::pango_layout_set_tabs(Hnd, t);
			Gtk::pango_tab_array_free(t);
		}
	}

	Gtk::pango_layout_set_text(Hnd, Str, len);
	
	int px, py;
	Gtk::pango_layout_get_size(Hnd, &px, &py);
	
	x = (px + PANGO_SCALE - 1) / PANGO_SCALE;
	y = (py + PANGO_SCALE - 1) / PANGO_SCALE;
	
	// printf("Layout '%s' = %i,%i\n", Str, x, y);
	
	#elif defined MAC && !defined COCOA
	
	if (Hnd && Str)
	{
		len = StrlenW(Str);
		
		OSStatus e = ATSUSetTextPointerLocation(Hnd, Str, 0, len, len);
		if (e)
		{
			GAutoString a(LgiNewUtf16To8(Str));
			printf("%s:%i - ATSUSetTextPointerLocation failed with errorcode %i (%s)\n", _FL, (int)e, a.Get());
		}
		else
		{
			e = ATSUSetRunStyle(Hnd, Font->Handle(), 0, len);
			if (e)
			{
				GAutoString a(LgiNewUtf16To8(Str));
				printf("%s:%i - ATSUSetRunStyle failed with errorcode %i (%s)\n", _FL, (int)e, a.Get());
			}
			else
			{
				ATSUTextMeasurement fTextBefore;
				ATSUTextMeasurement fTextAfter;

				if (pDC)
				{
					OsPainter dc = pDC->Handle();
					ATSUAttributeTag        theTags[1] = {kATSUCGContextTag};
					ByteCount               theSizes[1] = {sizeof(CGContextRef), };
					ATSUAttributeValuePtr   theValues[1] = {&dc};

					e = ATSUSetLayoutControls(Hnd, 1, theTags, theSizes, theValues);
					if (e) printf("%s:%i - ATSUSetLayoutControls failed (e=%i)\n", __FILE__, __LINE__, (int)e);
				}

				#if 0
				
				OSStatus e;
				if (Font->d->fAscent == 0)
				{
					e = ATSUGetUnjustifiedBounds(Hnd,
												kATSUFromTextBeginning,
												kATSUToTextEnd,
												&fTextBefore,
												&fTextAfter,
												&Font->d->fAscent,
												&Font->d->fDescent);
					if (e) printf("ATSUGetUnjustifiedBounds failed with %i\n", e);
				}
				
				Rect r;
				e = ATSUMeasureTextImage(	Hnd,
											kATSUFromTextBeginning,
											kATSUToTextEnd, 
											0,
											0,
											&r);
				if (e) printf("ATSUMeasureTextImage failed with %i\n", e);
				else
				{
					x = r.right - r.left;
					y = (Font->d->fAscent + Font->d->fDescent) >> 16;
					
					fAscent = Font->d->fAscent;
					fDescent = Font->d->fDescent;
					
					if (!x)
					{
						char16 sp[2] = {' ', 0};
						if (StricmpW(Str, sp) == 0)
						{
							x = 5;
						}
					}
					
					// printf("r=%i,%i,%i,%i a=%i d=%i\n", r.left, r.top, r.right, r.bottom, fAscent, fDescent);
				}
				#elif 0
				{
					ATSTrapezoid t[1];
					ItemCount tlen = 0;
					e = ATSUGetGlyphBounds(	Hnd,
											0,
											0,
											kATSUFromTextBeginning,
											kATSUToTextEnd,
											kATSUseDeviceOrigins,
											1,
											t,
											&tlen);
					if (e)
					{
						printf("%s:%i - ATSUGetGlyphBounds failed with errorcode %i (%S)\n", __FILE__, __LINE__, e, Str);
					}
					else
					{
						x = (t[0].upperRight.x - t[0].upperLeft.x) >> 16;
						y = (t[0].lowerLeft.y - t[0].upperLeft.y) >> 16;
						
						fAscent = -(t[0].upperLeft.y);
						fDescent = t[0].lowerLeft.y;
						
						#if 0
						printf("str='%p' x=%i y=%i (%g,%g,%g,%g) %i,%i\n", Str, x, y,
							(double)t[0].upperRight.x/0x10000, (double)t[0].upperLeft.x/0x10000,
							(double)t[0].lowerLeft.y/0x10000, (double)t[0].upperLeft.y/0x10000,
							fAscent >> 16, fDescent >> 16);
						#endif
					}
				}
				#else
				e = ATSUGetUnjustifiedBounds(	Hnd,
												kATSUFromTextBeginning,
												kATSUToTextEnd,
												&fTextBefore,
												&fTextAfter,
												&fAscent,
												&fDescent);
				if (e)
				{
					GAutoString a(LgiNewUtf16To8(Str));
					printf("%s:%i - ATSUGetUnjustifiedBounds failed with errorcode %i (%s)\n", __FILE__, __LINE__, (int)e, a.Get());
				}
				else
				{
					x = (fTextAfter - fTextBefore + 0xffff) >> 16;
					y = (fAscent + fDescent) >> 16;
					
					#if 0
					printf("str='%p' x=%i y=%i (%g,%g,%g,%g) %i,%i\n", Str, x, y,
						(double)fTextBefore/0x10000, (double)fTextAfter/0x10000, (double)fAscent/0x10000, (double)fDescent/0x10000,
						fAscent >> 16, fDescent >> 16);
					#endif
				}
				#endif
			
			}
		}

		ATSUSetTransientFontMatching(Hnd, true);
	}
	
	#elif defined WIN32NATIVE
	
	int sx, sy, i = 0;
	if (!Font->Handle()) Font->Create();
	y = Font->GetHeight();

	GFontSystem *Sys = GFontSystem::Inst();
	if (Sys && Str)
	{
		GFont *f = Font;
		OsChar *s;
		bool GlyphSub = Font->SubGlyphs();

		Blocks = 1;
		bool WasTab = IsTabChar(*Str);
		for (s=Str; *s && *s != '\n'; NextOsChar(s))
		{
			GFont *n = GlyphSub ? Sys->GetGlyph(*s, Font) : Font;
			if (n != f || (IsTabChar(*s) ^ WasTab))
			{
				Blocks++;
				f = n;
				WasTab = IsTabChar(*s);
			}
		}
		len = s - Str;

		if (*s == '\n')
		{
			*s = 0;
		}

		Info = Blocks ? new CharInfo[Blocks] : 0;
		if (Info)
		{
			Info[i].Str = Str;

			int TabSize = Font->TabSize() ? Font->TabSize() : 32;

			WasTab = IsTabChar(*Str);
			f = GlyphSub ? Sys->GetGlyph(*Str, Font) : Font;
			if (f && f != Font)
			{
				f->PointSize(Font->PointSize());
				f->SetWeight(Font->GetWeight());
				if (!f->Handle())
					f->Create();
			}

			bool Debug = WasTab;
			
			for (s=Str; true; NextOsChar(s))
			{
				GFont *n = GlyphSub ? Sys->GetGlyph(*s, Font) : Font;
				bool Change = n != f || (IsTabChar(*s) ^ WasTab) || !*s;

				if (Change)
				{
					// End last segment
					if (n && n != Font)
					{
						n->PointSize(Font->PointSize());
						n->SetWeight(Font->GetWeight());
						if (!n->Handle())
							n->Create();
					}

					Info[i].Len = s - Info[i].Str;
					if (Info[i].Len)
					{
						LgiAssert(i < Blocks);

						if (WasTab)
						{
							// Handle tab(s)
							for (int t=0; t<Info[i].Len; t++)
							{
								int Dx = TabSize - ((Info[i].X + x + TabOrigin) % TabSize);
								Info[i].X += Dx;
							}
							x += Info[i].X;
						}
						else
						{
							GFont *m = f;

							#if 0
							// This code is causing email to display very slowly in Scribe...
							// I guess I should rewrite the glyph substitution code to be
							// better away of point size differences.
							if (f && (f->GetHeight() > Font->GetHeight()))
							{
								Info[i].SizeDelta = -1;
								f->PointSize(Font->PointSize() + Info[i].SizeDelta);
								f->Create();
							}
							#endif

							if (!f)
							{
								// no font, so ? out the chars... as they aren't available anyway
								// printf("Font Cache Miss, Len=%i\n\t", Info[i].Len);
								m = Font;
								for (int n=0; n<Info[i].Len; n++)
								{
									Info[i].Str[n] = '?';
								}
							}
							m->_Measure(sx, sy, Info[i].Str, Info[i].Len);
							x += Info[i].X = sx > 0xffff ? 0xffff : sx;
						}
						Info[i].FontId = !f || Font == f ? 0 : Sys->Lut[Info[i].Str[0]];

						i++;
					}

					f = n;

					// Start next segment
					WasTab = IsTabChar(*s);
					if (i < Blocks)
					{
						Info[i].Str = s;
					}
				}

				if (!*s) break;
			}

			Blocks = min(Blocks, i + 1);
			if (Blocks > 0 && Info[Blocks-1].Len == 0)
			{
				Blocks--;
			}
		}
	}
	
	#elif defined BEOS
	
	if (Font && Font->Handle())
	{
		const char *Strs[] = { Str };
		BRect Rc;
		escapement_delta Delta;
		Font->Handle()->GetBoundingBoxesForStrings(Strs, 1, B_SCREEN_METRIC, &Delta, &Rc);
		x = Rc.IntegerWidth();
		y = Rc.IntegerHeight();
		if (Rc.IntegerWidth() < 0)
		{
			font_height Ht;
			Font->Handle()->GetHeight(&Ht);
			x = 0;
			y = ceil(Ht.ascent + Ht.descent);
			
			int Len = strlen(Str);
			float *esc = new float[Len];

			Font->Handle()->GetEscapements(Str, Len, esc);
			for (int i=0; i<Len; i++)
			{
				x += ceil(esc[i] * y);
				/*
				printf("Glyph[%i/%c] = %f, %f, %f\n", i, Str[i],
					(double)g[i].space, (double)g[i].nonspace,
					(double)esc[i]);
				*/
			}

			delete [] esc;
		}
		else
		{
			Blocks = 1;
			Info = new CharInfo[Blocks];
			Info[0].Str = Str;
			Info[0].Len = strlen(Str);
			Info[0].X = x;
			Info[0].FontId = -1;
			Info[0].SizeDelta = 0;
		}

		// printf("Layout '%s' = %i,%i\n", Str, x, y);
	}
	else printf("%s:%i - No font or handle.\n", _FL);
	
	#endif
}

int GDisplayString::GetTabOrigin()
{
    return TabOrigin;
}

void GDisplayString::SetTabOrigin(int o)
{
    TabOrigin = o;
}

bool GDisplayString::ShowVisibleTab()
{
	return VisibleTab;
}

void GDisplayString::ShowVisibleTab(bool i)
{
	VisibleTab = i;
}

bool GDisplayString::IsTruncated()
{
	return AppendDots;
}

void GDisplayString::TruncateWithDots(int Width)
{
    Layout();
    
	#if defined WIN32NATIVE
	
	if (Width < X() + 8)
	{
		int c = CharAt(Width);
		if (c >= 0 && c < len)
		{
			if (c > 0) c--; // fudge room for dots
			if (c > 0) c--;
			
			AppendDots = 1;
			
			if (Info)
			{
				int Width = 0;
				int Pos = 0;
				for (int i=0; i<Blocks; i++)
				{
					if (c >= Pos &&
						c < Pos + Info[i].Len)
					{
						Info[i].Len = c - Pos;
						Info[i].Str[Info[i].Len] = 0;

						GFont *f = Font;
						if (Info[i].FontId)
						{
							GFontSystem *Sys = GFontSystem::Inst();
							f = Sys->Font[Info[i].FontId];
							f->PointSize(Font->PointSize() + Info[i].SizeDelta);
							if (!f->Handle())
							{
								f->Create();
							}
						}
						else
						{
							f = Font;
						}
						
						if (f)
						{
							int Sx, Sy;
							f->_Measure(Sx, Sy, Info[i].Str, Info[i].Len);
							Info[i].X = Sx;
							Width += Info[i].X;
						}
						
						Blocks = i + 1;
						
						break;
					}
					
					Pos += Info[i].Len;
					Width += Info[i].X;
				}
				
				int DotsX, DotsY;
				Font->_Measure(DotsX, DotsY, GDisplayStringDots, 3);
				x = Width + DotsX;
			}
		}
	}
	
	#endif
}

int GDisplayString::X()
{
    Layout();
	return x;
}

int GDisplayString::Y()
{
    Layout();
	return y;
}

int GDisplayString::Length()
{
	return len;
}

void GDisplayString::Length(int New)
{
    Layout();

	#if defined WIN32NATIVE
	
	if (New < len)
	{
		GFontSystem *Sys = GFontSystem::Inst();
		
		int CurX = 0;
		int CurLen = 0;
		for (int i=0; i<Blocks; i++)
		{
			// Is the cut point in this block?
			if (New >= CurLen && New < CurLen + Info[i].Len )
			{
				// In this block
				int Offset = New - CurLen;
				Info[i].Len = Offset;
				Info[i].Str[Info[i].Len] = 0;

				// Get the font for this block of characters
				GFont *f = 0;
				if (Info[i].FontId)
				{
					f = Sys->Font[Info[i].FontId];
					f->PointSize(Font->PointSize());
					f->Transparent(Font->Transparent());
					if (!f->Handle())
					{
						f->Create();
					}
				}
				else
				{
					f = Font;
				}

				// Chop the current block and re-measure
				int ChoppedX, Unused;
				f->_Measure(ChoppedX, Unused, Info[i].Str, Info[i].Len);
				Info[i].X = ChoppedX;
				x = CurX + Info[i].X;
				Blocks = i+1;
				
				// Leave the loop
				break;
			}
			
			CurX += Info[i].X;
			CurLen += Info[i].Len;
		}
	}
	else
	{
		printf("%s:%i - New>=Len (%i>=%i)\n", __FILE__, __LINE__, New, len);
	}

	#endif
}

void GDisplayString::Size(int *sx, int *sy)
{
    Layout();
	if (sx) *sx = x;
	if (sy) *sy = y;
}

int GDisplayString::CharAt(int Px)
{
	int Status = -1;

    Layout();
	if (Px < 0)
		return 0;
	else if (Px >= x)
	{
		#if defined __GTK_H__
		if (Str)
		{
			GUtf8Str u(Str);
			return u.GetChars();
		}
		return 0;
		#else
		return len;
		#endif
	}

	#if defined MAC && !defined COCOA

	if (Hnd && Str)
	{
		UniCharArrayOffset Off = 0, Off2 = 0;
		Boolean IsLeading;
		OSStatus e = ATSUPositionToOffset(Hnd, FloatToFixed(Px), FloatToFixed(y / 2), &Off, &IsLeading, &Off2);
		if (e) printf("%s:%i - ATSUPositionToOffset failed with %i, CharAt(%i) x=%i len=%i\n", __FILE__, __LINE__, (int)e, Px, x, len);
		else
		{
			// printf("CharAt(%i) off=%i,%i Leading=%i x=%i len=%i\n", Px, Off, Off2, IsLeading, x, len);
			Status = Off;
		}
	}
	
	#elif defined __GTK_H__
	
	if (Hnd)
	{
		int Index = 0, Trailing = 0;
		if (Gtk::pango_layout_xy_to_index(Hnd, Px * PANGO_SCALE, 0, &Index, &Trailing))
		{
			// printf("Index = %i, Trailing = %i\n", Index, Trailing);
			GUtf8Str u(Str);
			Status = 0;
			while ((OsChar*)u.GetPtr() < Str + Index)
			{
				u++;
				Status++;
			}
		}
		else if (Trailing)
		{
		    GUtf8Str u(Str + Index);
		    if (u)
		        u++;		    
		    Status = (OsChar*)u.GetPtr() - Str;
		}
		else Status = 0;
	}
	
	#elif defined WIN32NATIVE
	
	GFontSystem *Sys = GFontSystem::Inst();

	if (Info && Font && Sys)
	{
		int TabSize = Font->TabSize() ? Font->TabSize() : 32;
		int Cx = 0;
		int Char = 0;

		for (int i=0; i<Blocks && Status < 0; i++)
		{
			if (Px < Cx)
			{
				Status = Char;
				break;
			}

			if (Px >= Cx && Px < Cx + Info[i].X)
			{
				// The position is in this block of characters
				if (IsTabChar(Info[i].Str[0]))
				{
					// Search through tab block
					for (int t=0; t<Info[i].Len; t++)
					{
						int TabX = TabSize - (Cx % TabSize);
						if (Px >= Cx && Px < Cx + TabX)
						{
							Status = Char;
							break;
						}
						Cx += TabX;
						Char++;
					}
				}
				else
				{
					// Get the blocks font
					GFont *f = Font;
					if (Info[i].FontId)
					{
						f = Sys->Font[Info[i].FontId];
						f->Colour(Font->Fore(), Font->Back());
						f->PointSize(Font->PointSize());
						if (!f->Handle())
						{
							f->Create();
						}
					}

					// Find the pos
					int Fit = f->_CharAt(Px - Cx, Info[i].Str, Info[i].Len);
					if (Fit >= 0)
					{
						Status = Char + Fit;
					}
					else
					{
						Status = -1;
						goto CharAtError;
					}
					break;
				}
			}
			else
			{
				// Not in this block, skip the whole lot
				Cx += Info[i].X;
				Char += Info[i].Len;
			}
		}

		if (Status < 0)
		{
			Status = Char;
		}
	}

CharAtError:
	#endif

	return Status;
}

void GDisplayString::Draw(GSurface *pDC, int px, int py, GRect *r)
{
    Layout();

	#if defined __GTK_H__
	
	Gtk::cairo_t *cr = pDC->GetCairo();
	if (!cr)
	{
		LgiAssert(!"Can't get cairo.");
		return;
	}

	int Ox = 0, Oy = 0;
	pDC->GetOrigin(Ox, Oy);
	GColour b = Font->Back();
	
	Gtk::cairo_set_source_rgb(cr,
								(double)b.r()/255.0,
								(double)b.g()/255.0,
								(double)b.b()/255.0);

	if (!Font->Transparent() && r)
	{
		Gtk::cairo_new_path(cr);
		Gtk::cairo_rectangle(cr, r->x1 - Ox, r->y1 - Oy, r->X(), r->Y());
		Gtk::cairo_fill(cr);
	}

	cairo_translate(cr, px-Ox, py-Oy);
	
	if (!Font->Transparent() && !r)
	{
		Gtk::cairo_new_path(cr);
		Gtk::cairo_rectangle(cr, 0, 0, x, y);
		Gtk::cairo_fill(cr);
	}
	if (Hnd)
	{
	    GColour f = Font->Fore();
	    Gtk::cairo_set_source_rgb(cr,
								    (double)f.r()/255.0,
								    (double)f.g()/255.0,
								    (double)f.b()/255.0);
	    Gtk::pango_cairo_show_layout(cr, Hnd);
	}
	
	cairo_identity_matrix(cr);
	
	#elif defined(BEOS)

	// FIXME
	if (pDC && Font)
	{
		BView *Hnd = pDC->Handle();
		if (Hnd)
		{
			/*
			BPoint o = Hnd->Origin();
			int Ox = 0, Oy = 0;
			pDC->GetOrigin(Ox, Oy);
			*/
			
			GColour c = Font->Fore();
			rgb_color Fg;
			Fg.red = c.r();
			Fg.green = c.g();
			Fg.blue = c.b();
			Hnd->SetHighColor(Fg);

			c = Font->Back();
			rgb_color Bk;
			Bk.red = c.r();
			Bk.green = c.g();
			Bk.blue = c.b();
			Hnd->SetLowColor(Bk);			
			
			int Len = strlen(Str);
			Hnd->SetFont(Font->Handle());
			Hnd->DrawString(Str, Len, BPoint(px, py + Font->Ascent()));
			printf("Drawing String '%s' %i at %i,%i\n", Str, Len, px, py);
		}
		else printf("%s:%i - Error: no BView to draw on.", _FL);
	}
	else printf("%s:%i - Error: no DC or Font.\n", _FL);

	#elif defined MAC && !defined COCOA

	int Ox = 0, Oy = 0;
	if (pDC && !pDC->IsScreen())
		pDC->GetOrigin(Ox, Oy);

	if (!Font->Transparent())
	{
		GColour Old = pDC->Colour(Font->Back());
		if (r)
		{
			GRect a = r;
			pDC->Rectangle(&a);
		}
		else
		{
			GRect a(px, py, px + x - 1, py + y - 1);
			pDC->Rectangle(&a);
		}
		pDC->Colour(Old);
	}
	
	if (Hnd && pDC && len > 0)
	{
		OSStatus e;
		OsPainter dc = pDC->Handle();
		ATSUAttributeTag        theTags[1] = {kATSUCGContextTag};
		ByteCount               theSizes[1] = {sizeof(CGContextRef)};
		ATSUAttributeValuePtr   theValues[1] = {&dc};

		e = ATSUSetLayoutControls(Hnd, 1, theTags, theSizes, theValues);
		if (e)
		{
			printf("%s:%i - ATSUSetLayoutControls failed (e=%i)\n", __FILE__, __LINE__, (int)e);
		}
		else
		{
			// Set style attr
			ATSURGBAlphaColor c;
			c.red = (double) Font->Fore().r() / 255.0;
			c.green = (double) Font->Fore().g() / 255.0;
			c.blue = (double) Font->Fore().b() / 255.0;
			c.alpha = 1.0;
			
			ATSUAttributeTag Tags[]			= {kATSURGBAlphaColorTag};
			ATSUAttributeValuePtr Values[]	= {&c};
			ByteCount Lengths[]				= {sizeof(c)};
			
		
			if (!(e = ATSUSetAttributes(Font->Handle(),
										CountOf(Tags),
										Tags,
										Lengths,
										Values)))
			{
				int y = (pDC->Y() - py + Oy);

				if (pDC->IsScreen())
				{
					CGContextSaveGState(pDC->Handle());

					if (r)
					{
						CGRect rect = *r;
						rect.origin.y -= rect.size.height;
						rect.size.width += 1.0;
						rect.size.height += 1.0;
						#if 1
						CGContextClipToRect(pDC->Handle(), rect);
						#else
						CGContextSetLineWidth(pDC->Handle(), 1.0);
						CGContextStrokeRect(pDC->Handle(), rect);
						#endif
					}
					CGContextTranslateCTM(pDC->Handle(), 0, pDC->Y()-1);
					CGContextScaleCTM(pDC->Handle(), 1.0, -1.0);

					e = ATSUDrawText(Hnd, kATSUFromTextBeginning, kATSUToTextEnd, (px - Ox) << 16, (y << 16) - fAscent);

					CGContextRestoreGState(pDC->Handle());
				}
				else
				{
					if (r)
					{
						CGContextSaveGState(pDC->Handle());
						
						CGRect rect = *r;
						rect.origin.x -= Ox;
						rect.origin.y = pDC->Y() - rect.origin.y + Oy;
						rect.size.width += 1.0;
						rect.size.height += 1.0;
						#if 1
						CGContextClipToRect(pDC->Handle(), rect);
						#else
						CGContextSetLineWidth(pDC->Handle(), 1.0);
						CGContextStrokeRect(pDC->Handle(), rect);
						#endif
					}
					
					e = ATSUDrawText(Hnd, kATSUFromTextBeginning, kATSUToTextEnd, (px - Ox) << 16, (y << 16) - fAscent);
					
					if (r)
						CGContextRestoreGState(pDC->Handle());
				}
				if (e)
				{
					GAutoString a(LgiNewUtf16To8(Str));
					printf("%s:%i - ATSUDrawText failed with %i, len=%i, str=%.20s\n", __FILE__, __LINE__, (int)e, len, a.Get());
				}
				else
				{
					/*
					_draw_layout++;
					int64 Now = LgiCurrentTime();
					if (_last == 0)
						_last = Now;
					else if (Now > _last + 1000)
					{
						int ms = Now - _last;
						printf("ms=%i create=%i layout=%i draw=%i\n",
							ms, _create_style, _create_layout, _draw_layout);
						_create_style = 0;
						_create_layout = 0;
						_draw_layout = 0;
						_last = Now;
					}
					*/
				}
			}
			else
			{
				printf("%s:%i - Error setting font attr (e=%i)\n", __FILE__, __LINE__, (int)e);
			}
		}
	}
	
	#elif defined WIN32NATIVE
	
	if (Info && pDC && Font)
	{
		GFontSystem *Sys = GFontSystem::Inst();
		COLOUR Old = pDC->Colour();
		int TabSize = Font->TabSize() ? Font->TabSize() : 32;
		int Ox = px;

		for (int i=0; i<Blocks; i++)
		{
			GFont *f = 0;

			// Get the font for this block of characters
			if (Info[i].FontId)
			{
				f = Sys->Font[Info[i].FontId];

				#if 1 // true == proper colour
				f->Colour(Font->Fore(), Font->Back());
				#else
				f->Colour(Rgb24(255, 0, 0), Font->Back());
				#endif

				f->PointSize(Font->PointSize() + Info[i].SizeDelta);
				f->Transparent(Font->Transparent());
				f->Underline(Font->Underline());
				if (!f->Handle())
				{
					f->Create();
				}
			}
			else
			{
				f = Font;
			}

			if (f)
			{
				GRect b;
				if (r)
				{
					b.x1 = i ? px : r->x1;
					b.y1 = r->y1;
					b.x2 = i < Blocks - 1 ? px + Info[i].X - 1 : r->x2;
					b.y2 = r->y2;
				}
				else
				{
					b.x1 = px;
					b.y1 = py;
					b.x2 = px + Info[i].X - 1;
					b.y2 = py + Y() - 1;
				}
				
				if (b.Valid())
				{
					if (IsTabChar(*Info[i].Str))
					{
						if (Info[i].Str[0] == '\t')
						{
							// Invisible tab... draw blank space
							if (!Font->Transparent())
							{
								pDC->Colour(Font->Back());
								pDC->Rectangle(&b);
							}
						}
						else
						{
							// Visible tab...
							int X = px;
							for (int n=0; n<Info[i].Len; n++)
							{
								int Dx = TabSize - ((X - Ox + TabOrigin) % TabSize);
								GRect Char(X, b.y1, X + Dx - 1, b.y2);
								f->_Draw(pDC, X, py, Info[i].Str, 1, &Char, f->WhitespaceColour());
								X += Dx;
							}
						}
					}
					else
					{
						// Draw the character(s)
						f->_Draw(pDC, px, py, Info[i].Str, Info[i].Len, &b, f->Fore());
					}
				}
			}

			// Inc my position
			px += Info[i].X;
		}
		
		if (AppendDots)
		{
			int Sx, Sy;
			Font->_Measure(Sx, Sy, GDisplayStringDots, 3);

			GRect b;
			if (r)
			{
				b.x1 = px;
				b.y1 = r->y1;
				b.x2 = min(px + Sx - 1, r->x2);
				b.y2 = r->y2;
			}
			else
			{
				b.x1 = px;
				b.y1 = py;
				b.x2 = px + Sx - 1;
				b.y2 = py + Y() - 1;
			}

			Font->_Draw(pDC, px, py, GDisplayStringDots, 3, &b, Font->Fore());
		}

		pDC->Colour(Old);
	}
	else if (r &&
			Font &&
			!Font->Transparent())
	{
		pDC->Colour(Font->Back());
		pDC->Rectangle(r);
	}
	
	#endif
}

