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
#include "GDisplayString.h"
#include "GPixelRops.h"

#ifdef FontChange
#undef FontChange
#endif

#ifdef LGI_SDL
#include "ftsynth.h"
#endif

//345678123456781234567812345678
	//	2nd

#define DEBUG_CHAR_AT				0

#if defined(__GTK_H__) || (defined(MAC) && !defined(COCOA) && !defined(LGI_SDL))
#define DISPLAY_STRING_FRACTIONAL_NATIVE	1
#else
#define DISPLAY_STRING_FRACTIONAL_NATIVE	0
#endif

template<typename Out, typename In>
bool StringConvert(Out *&out, uint32 *OutLen, const In *in, int InLen)
{
	char OutCs[8], InCs[8];
	sprintf_s(OutCs, sizeof(OutCs), "utf-%i", (int)sizeof(Out)<<3);
	sprintf_s(InCs, sizeof(InCs), "utf-%i", (int)sizeof(In)<<3);

	if (InLen < 0)
		InLen = StringLen(in);

	out = (Out*)LgiNewConvertCp
		(
			OutCs,
			in,
			InCs,
			InLen*sizeof(In)
		);
	
	if (OutLen)
		*OutLen = out ? StringLen(out) : 0;
	
	return out != NULL;
}

//////////////////////////////////////////////////////////////////////
#define SubtractPtr(a, b)			(	(((NativeInt)(a))-((NativeInt)(b))) / sizeof(*a)	)
#define IsTabChar(c)				(c == '\t' || (c == 0x2192 && VisibleTab))

static OsChar GDisplayStringDots[] = {'.', '.', '.', 0};

#if USE_CORETEXT
#include <CoreFoundation/CFString.h>

void GDisplayString::CreateAttrStr()
{
	if (!StrCache.Get())
		return;

	wchar_t *w = StrCache.Get();
	CFStringRef string = CFStringCreateWithBytes(kCFAllocatorDefault, (const uint8*)w, StrlenW(w) * sizeof(*w), kCFStringEncodingUTF32LE, false);
	if (string)
	{
		CFDictionaryRef attributes = Font->GetAttributes();
		if (attributes)
			AttrStr = CFAttributedStringCreate(kCFAllocatorDefault, string, attributes);
		else
			LgiAssert(0);
		
		CFRelease(string);
	}
}
#endif

GDisplayString::GDisplayString(GFont *f, const char *s, int l, GSurface *pdc)
{
	pDC = pdc;
	Font = f;
	
	#if LGI_DSP_STR_CACHE

		StrCache.Reset(LgiNewUtf8To16(s, l));

	#endif

	#if defined(MAC)
	
		StringConvert(Str, &len, s, l);
	
	#elif defined(WINNATIVE) || defined(LGI_SDL)

		StringConvert(Str, &len, s, l);

	#else

		Str = NewStr(s, l);
		len = Str ? strlen(Str) : 0;
		if (!LgiIsUtf8(Str))
			printf("%s:%i - Not valid utf\n", _FL);

	#endif
	
	x = y = 0;
	xf = 0;
	yf = 0;
	DrawOffsetF = 0;
	LaidOut = 0;
	AppendDots = 0;
	VisibleTab = 0;
	
	#if defined MAC && !defined COCOA && !defined(LGI_SDL)
	
		Hnd = 0;
		#if USE_CORETEXT
		AttrStr = NULL;
		#endif
		if (Font && Str)
		{
			len = l >= 0 ? l : StringLen(Str);
			if (len > 0)
			{
				#if USE_CORETEXT
					CreateAttrStr();
				#else
					ATSUCreateTextLayout(&Hnd);
				#endif
			}
		}
	
	#elif defined __GTK_H__
	
		Hnd = 0;
		LastTabOffset = -1;
		if (Font && Str)
		{
			len = l >= 0 ? l : strlen(Str);
			if (len > 0)
			{
				Gtk::GtkPrintContext *PrintCtx = pDC ? pDC->GetPrintContext() : NULL;
				if (PrintCtx)
					Hnd = Gtk::gtk_print_context_create_pango_layout(PrintCtx);
				else
					Hnd = Gtk::pango_layout_new(GFontSystem::Inst()->GetContext());
			}
		}
	
	#endif
}

GDisplayString::GDisplayString(GFont *f, const char16 *s, int l, GSurface *pdc)
{
	pDC = pdc;
	Font = f;

	#if LGI_DSP_STR_CACHE

		StrCache.Reset(NewStrW(s, l));

	#endif

    #if defined(MAC) || WINNATIVE || defined(LGI_SDL)

		StringConvert(Str, &len, s, l);

	#else

		Str = LgiNewUtf16To8(s, l < 0 ? -1 : l * sizeof(char16));
		len = Str ? strlen(Str) : 0;

	#endif
	
	x = y = 0;
	xf = 0;
	yf = 0;
	DrawOffsetF = 0;
	LaidOut = 0;
	AppendDots = 0;
	VisibleTab = 0;

	#if defined MAC && !defined COCOA && !defined(LGI_SDL)
	
		Hnd = NULL;
		#if USE_CORETEXT
		AttrStr = NULL;
		#endif
		if (Font && Str && len > 0)
		{
			#if USE_CORETEXT
				CreateAttrStr();
			#else
				OSStatus e = ATSUCreateTextLayout(&Hnd);
				if (e) printf("%s:%i - ATSUCreateTextLayout failed with %i.\n", _FL, (int)e);
			#endif
		}
	
	#elif defined __GTK_H__
	
		Hnd = 0;
		if (Font && Str && len > 0)
		{
			Gtk::GtkPrintContext *PrintCtx = pDC ? pDC->GetPrintContext() : NULL;
			if (PrintCtx)
				Hnd = Gtk::gtk_print_context_create_pango_layout(PrintCtx);
			else
				Hnd = Gtk::pango_layout_new(GFontSystem::Inst()->GetContext());
		}
	
	#endif
}

GDisplayString::~GDisplayString()
{
	#if defined(LGI_SDL)
	
		Img.Reset();
	
	#elif defined MAC && !defined COCOA

		#if USE_CORETEXT

			if (Hnd)
			{
				CFRelease(Hnd);
				Hnd = NULL;
			}
			if (AttrStr)
			{
				CFRelease(AttrStr);
				AttrStr = NULL;
			}

		#else

		if (Hnd)
			ATSUDisposeTextLayout(Hnd);

		#endif

	#elif defined __GTK_H__

		if (Hnd)
			g_object_unref(Hnd);

	#endif
	
	DeleteArray(Str);
}

#if defined __GTK_H__
void GDisplayString::UpdateTabs(int Offset, int Size, bool Debug)
{
	if (Hnd &&
		Font &&
		Font->TabSize())
	{
		int Len = 16;
		LastTabOffset = Offset;
		
		Gtk::PangoTabArray *t = Gtk::pango_tab_array_new(Len, true);
		if (t)
		{
			for (int i=0; i<Len; i++)
			{
				int Pos = (i * Size) + Offset;
				Gtk::pango_tab_array_set_tab(t,
											i,
											Gtk::PANGO_TAB_LEFT,
											Pos);
				if (Debug)
					printf("%i, ", Pos);
			}
			if (Debug)
				printf("\n");
			
			Gtk::pango_layout_set_tabs(Hnd, t);
			Gtk::pango_tab_array_free(t);
		}
	}
}
#endif

void GDisplayString::Layout(bool Debug)
{
    if (LaidOut)
        return;

    LaidOut = 1;

	#if defined(LGI_SDL)
    
		FT_Face Fnt = Font->Handle();
		FT_Error error;
		
		if (!Fnt || !Str)
			return;
		
		// Create an array of glyph indexes
		GArray<uint32> Glyphs;
		for (OsChar *s = Str; *s; s++)
		{
			FT_UInt index = FT_Get_Char_Index(Fnt, *s);
			if (index)
				Glyphs.Add(index);
		}
		
		// Measure the string...
		GdcPt2 Sz;
		int FontHt = Font->GetHeight();
		int AscentF = (int) (Font->Ascent() * FScale);
		int LoadMode = FT_LOAD_FORCE_AUTOHINT;
		for (unsigned i=0; i<Glyphs.Length(); i++)
		{
			error = FT_Load_Glyph(Fnt, Glyphs[i], LoadMode);
			if (error == 0)
			{
				int PyF = AscentF - Fnt->glyph->metrics.horiBearingY;

				Sz.x += Fnt->glyph->metrics.horiAdvance;
				Sz.y = max(Sz.y, PyF + Fnt->glyph->metrics.height);
			}
		}
		
		// Create the memory context to draw into
		xf = Sz.x;
		x = ((Sz.x + FScale - 1) >> FShift) + 1;
		yf = FontHt << FShift;
		y = FontHt; // ((Sz.y + FScale - 1) >> FShift) + 1;
		
		if (Img.Reset(new GMemDC(x, y, CsIndex8)))
		{
			// Clear the context to black
			Img->Colour(0);
			Img->Rectangle();

			bool IsItalic = Font->Italic();
			
			int CurX = 0;
			int FBaseline = Fnt->size->metrics.ascender;
			for (unsigned i=0; i<Glyphs.Length(); i++)
			{
				error = FT_Load_Glyph(Fnt, Glyphs[i], LoadMode);
				if (error == 0)
				{
					if (IsItalic)
						FT_GlyphSlot_Oblique(Fnt->glyph);

					error = FT_Render_Glyph(Fnt->glyph, FT_RENDER_MODE_NORMAL);
					if (error == 0)
					{
						FT_Bitmap &bmp = Fnt->glyph->bitmap;
						if (bmp.buffer)
						{
							// Copy rendered glyph into our image memory
							int Px = (CurX + (FScale >> 1)) >> FShift;
							int PyF = AscentF - Fnt->glyph->metrics.horiBearingY;
							int Py = PyF >> FShift;
							int Skip = 0;
							
							if (Fnt->glyph->format == FT_GLYPH_FORMAT_BITMAP)
							{
								Px += Fnt->glyph->bitmap_left;
								Skip = Fnt->glyph->bitmap_left < 0 ? -Fnt->glyph->bitmap_left : 0;
								Py = (AscentF >> FShift) - Fnt->glyph->bitmap_top;
							}
							
							LgiAssert(Px + bmp.width <= Img->X());
							for (int y=0; y<bmp.rows; y++)
							{
								uint8 *in = bmp.buffer + (y * bmp.pitch);
								uint8 *out = (*Img)[Py+y];
								if (out)
								{
									LgiAssert(Px+Skip >= 0);
									out += Px+Skip;
									memcpy(out, in+Skip, bmp.width-Skip);
								}
								/*
								else
								{
									LgiAssert(!"No scanline?");
									break;
								}
								*/
							}
						}
						
						if (i < Glyphs.Length() - 1)
						{
							FT_Vector kerning;
							FT_Get_Kerning(Fnt, Glyphs[i], Glyphs[i+1], FT_KERNING_DEFAULT, &kerning);
							CurX += Fnt->glyph->metrics.horiAdvance + kerning.x;
						}
						else
						{
							CurX += Fnt->glyph->metrics.horiAdvance;
						}
					}
				}
			}
		}
		else LgiTrace("::Layout Create MemDC failed\n");
    
	#elif defined(__GTK_H__)
	
		y = Font->GetHeight();
		yf = y * PANGO_SCALE;
		if (!Hnd || !Font->Handle())
		{
			// LgiTrace("%s:%i - Missing handle: %p,%p\n", _FL, Hnd, Font->Handle());
			return;
		}

		if (!LgiIsUtf8(Str))
		{
			LgiTrace("%s:%i - Not utf8\n", _FL);
			return;
		}

		GFontSystem *FSys = GFontSystem::Inst();
		Gtk::pango_context_set_font_description(FSys->GetContext(), Font->Handle());

		int TabSizePx = Font->TabSize();
		int TabSizeF = TabSizePx * FScale;
		int TabOffsetF = DrawOffsetF % TabSizeF;
		int OffsetF = TabOffsetF ? TabSizeF - TabOffsetF : 0;
		/*
		if (Debug)
		{
			printf("'%s', TabSizeF=%i, TabOffsetF=%i, DrawOffsetF=%i, OffsetF=%i\n",
				Str,
				TabSizeF,
				TabOffsetF,
				DrawOffsetF,
				OffsetF);
		}
		*/
		UpdateTabs(OffsetF / FScale, Font->TabSize());


		if (Font->Underline())
		{
			Gtk::PangoAttrList *attrs = Gtk::pango_attr_list_new();
			Gtk::PangoAttribute *attr = Gtk::pango_attr_underline_new(Gtk::PANGO_UNDERLINE_SINGLE);
			Gtk::pango_attr_list_insert(attrs, attr);
			Gtk::pango_layout_set_attributes(Hnd, attrs);
			Gtk::pango_attr_list_unref(attrs);
		}
		
		Gtk::pango_layout_set_text(Hnd, Str, len);
		Gtk::pango_layout_get_size(Hnd, &xf, &yf);
		x = (xf + PANGO_SCALE - 1) / PANGO_SCALE;
		y = (yf + PANGO_SCALE - 1) / PANGO_SCALE;
	
	#elif defined MAC && !defined COCOA && !defined(LGI_SDL)
	
		#if USE_CORETEXT

			Hnd = CTLineCreateWithAttributedString(AttrStr);
			if (Hnd)
			{
				CGFloat ascent = 0.0;
				CGFloat descent = 0.0;
				CGFloat leading = 0.0;
				double width = CTLineGetTypographicBounds(Hnd, &ascent, &descent, &leading);
				int height = Font->GetHeight();
				x = ceil(width);
				y = height;
				xf = width * FScale;
				yf = height * FScale;
			}
	
		#else
	
			if (!Hnd || !Str)
				return;

			OSStatus e = ATSUSetTextPointerLocation(Hnd, Str, 0, len, len);
			if (e)
			{
				char *a = 0;
				StringConvert(a, NULL, Str, len);
				printf("%s:%i - ATSUSetTextPointerLocation failed with errorcode %i (%s)\n", _FL, (int)e, a);
				DeleteArray(a);
				return;
			}

			e = ATSUSetRunStyle(Hnd, Font->Handle(), 0, len);
			if (e)
			{
				char *a = 0;
				StringConvert(a, NULL, Str, len);
				printf("%s:%i - ATSUSetRunStyle failed with errorcode %i (%s)\n", _FL, (int)e, a);
				DeleteArray(a);
				return;
			}

			ATSUTextMeasurement fTextBefore;
			ATSUTextMeasurement fTextAfter;

			if (pDC)
			{
				OsPainter dc = pDC->Handle();
				ATSUAttributeTag        Tags[1] = {kATSUCGContextTag};
				ByteCount               Sizes[1] = {sizeof(CGContextRef)};
				ATSUAttributeValuePtr   Values[1] = {&dc};

				e = ATSUSetLayoutControls(Hnd, 1, Tags, Sizes, Values);
				if (e) printf("%s:%i - ATSUSetLayoutControls failed (e=%i)\n", _FL, (int)e);
			}
			
			ATSUTab Tabs[32];
			for (int i=0; i<CountOf(Tabs); i++)
			{
				Tabs[i].tabPosition = (i * Font->TabSize()) << 16;
				Tabs[i].tabType = kATSULeftTab;
			}
			e = ATSUSetTabArray(Hnd, Tabs, CountOf(Tabs));
			if (e) printf("%s:%i - ATSUSetTabArray failed (e=%i)\n", _FL, (int)e);

			e = ATSUGetUnjustifiedBounds(	Hnd,
											kATSUFromTextBeginning,
											kATSUToTextEnd,
											&fTextBefore,
											&fTextAfter,
											&fAscent,
											&fDescent);
			if (e)
			{
				char *a = 0;
				StringConvert(a, NULL, Str, len);
				printf("%s:%i - ATSUGetUnjustifiedBounds failed with errorcode %i (%s)\n", _FL, (int)e, a);
				DeleteArray(a);
				return;
			}

			xf = fTextAfter - fTextBefore;
			yf = fAscent + fDescent;
			x = (xf + 0xffff) >> 16;
			y = (yf + 0xffff) >> 16;
			ATSUSetTransientFontMatching(Hnd, true);

		#endif
	
	#elif defined WINNATIVE
	
		int sx, sy, i = 0;
		if (!Font)
			return;
		if (!Font->Handle())
			Font->Create();
		
		y = Font->GetHeight();

		GFontSystem *Sys = GFontSystem::Inst();
		if (Sys && Str)
		{
			GFont *f = Font;
			OsChar *s;
			bool GlyphSub = Font->SubGlyphs();

			Info[i].Str = Str;

			int TabSize = Font->TabSize() ? Font->TabSize() : 32;

			bool WasTab = IsTabChar(*Str);
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
				bool Change =	n != f ||					// The font changed
								(IsTabChar(*s) ^ WasTab) ||	// Entering/leaving a run of tabs
								!*s ||						// Hit a NULL character
								(s - Info[i].Str) >= 1000;	// This is to stop very long segments not rendering
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
						if (WasTab)
						{
							// Handle tab(s)
							for (int t=0; t<Info[i].Len; t++)
							{
								int Dx = TabSize - ((Info[i].X + x + GetDrawOffset()) % TabSize);
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
							// better aware of point size differences.
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
					Info[i].Str = s;
				}

				if (!*s) break;
			}

			if (Info.Length() > 0 && Info.Last().Len == 0)
			{
				Info.Length(Info.Length()-1);
			}
		}
		
		xf = x;
		yf = y;
	
	#elif defined BEOS
	
		if (Font && Font->Handle())
		{
			int TabSize = Font->TabSize() ? Font->TabSize() : 32;
			char *End = Str + len;
			GArray<CharInfo> a;
			char *s = Str;
			
			x = 0;
			while (s < End)
			{
				char *Start = s;
				while (s < End && *s && *s != '\t')
					s++;

				if (s > Start)
				{
					// Normal segment
					CharInfo &i = a.New();
					i.Str = Start;
					i.Len = s - Start;
					
					GdcPt2 size = Font->StringBounds(i.Str, i.Len);
					i.X = size.x;
					i.FontId = -1;
					i.SizeDelta = 0;
					
					x += size.x;
				}
				
				Start = s;
				while (s < End && *s && *s == '\t')
					s++;

				if (s > Start)
				{
					// Tabs segment
					CharInfo &i = a.New();
					i.Str = Start;
					i.Len = s - Start;
					
					i.X = 0;
					i.FontId = -1;
					i.SizeDelta = 0;
					
					for (int n=0; n<i.Len; n++)
					{
						int Dx = TabSize - ((x + TabOrigin) % TabSize);
						i.X += Dx;
						x += Dx;
					}
				}
			}
			
			LgiAssert(s == End);
			
			Blocks = a.Length();
			Info = a.Release();
			y = Font->GetHeight();

			#if 0
			printf("Layout '%s' = %i,%i\n", Str, x, y);
			for (int i=0; i<Blocks; i++)
			{
				CharInfo *ci = Info + i;
				printf("  [%i]=%s,%i x=%i\n", i, ci->Str, ci->Len, ci->X);
			}
			#endif		
		}
		else printf("%s:%i - No font or handle.\n", _FL);
	
	#endif
}

int GDisplayString::GetDrawOffset()
{
    return DrawOffsetF >> FShift;
}

void GDisplayString::SetDrawOffset(int Px)
{
	if (LaidOut)
		LgiAssert(!"No point setting TabOrigin after string is laid out.\n");
    DrawOffsetF = Px << FShift;
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
    
	#if defined __GTK_H__
	    
	if (Hnd)
	{
		Gtk::pango_layout_set_ellipsize(Hnd, Gtk::PANGO_ELLIPSIZE_END);
		Gtk::pango_layout_set_width(Hnd, Width * PANGO_SCALE);
	}
    
	#elif WINNATIVE
	
	if (Width < X() + 8)
	{
		int c = CharAt(Width);
		if (c >= 0 && c < len)
		{
			if (c > 0) c--; // fudge room for dots
			if (c > 0) c--;
			
			AppendDots = 1;
			
			if (Info.Length())
			{
				int Width = 0;
				int Pos = 0;
				for (int i=0; i<Info.Length(); i++)
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
						
						Info.Length(i + 1);						
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
	
	#elif  defined(LGI_SDL)
	
	#else
	
		#if USE_CORETEXT

		if (Hnd)
		{
			CFAttributedStringRef truncationString = CFAttributedStringCreate(NULL, CFSTR("\u2026"), Font->GetAttributes());
			if (truncationString)
			{
				CTLineRef truncationToken = CTLineCreateWithAttributedString(truncationString);
				CFRelease(truncationString);
				if (truncationToken)
				{
					CTLineRef TruncatedLine = CTLineCreateTruncatedLine(Hnd, Width, kCTLineTruncationEnd, truncationToken);
					if (TruncatedLine)
					{
						CFRelease(Hnd);
						Hnd = TruncatedLine;
					}
				}
			}
		}
	
		#else
	
			if (!Str)
				return;

			ATSULineTruncation truc = kATSUTruncateEnd;
			ATSUTextMeasurement width = Width << FShift;

			ATSUAttributeTag        tags[] = {kATSULineWidthTag, kATSULineTruncationTag};
			ByteCount               sizes[] = {sizeof(width), sizeof(truc)};
			ATSUAttributeValuePtr   values[] = {&width, &truc};

			OSStatus e = ATSUSetLayoutControls(	Hnd,
												CountOf(tags),
												tags,
												sizes,
												values);
			if (e)
				printf("%s:%i - ATSUSetLayoutControls failed (e=%i)\n", _FL, (int)e);
	
		#endif
	
	#endif
}

int GDisplayString::CharAt(int Px)
{
	int Status = -1;

    Layout();
	if (Px < 0)
	{
		// printf("CharAt(%i) <0\n", Px);
		return 0;
	}
	else if (Px >= x)
	{
		// printf("CharAt(%i) >x=%i len=%i\n", Px, x, len);
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

	#if defined MAC && !defined COCOA && !defined(LGI_SDL)

	if (Hnd && Str)
	{
		UniCharArrayOffset Off = 0, Off2 = 0;
		Boolean IsLeading;
		
		#if USE_CORETEXT

			#if 1
		
			CGPoint pos = { Px, 1.0 };
			CFIndex idx = CTLineGetStringIndexForPosition(Hnd, pos);
			return idx;
		
			#else
		
			CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString(AttrStr);
			if (typesetter)
			{
				CFIndex count = CTTypesetterSuggestLineBreak(typesetter, 0, Px);
				CFRelease(typesetter);
				// printf("CharAt(%i) = %i\n", Px, (int)count);
				return count;
			}
		
			#endif

		#else

			OSStatus e = ATSUPositionToOffset(Hnd, FloatToFixed(Px), FloatToFixed(y / 2), &Off, &IsLeading, &Off2);
			if (e) printf("%s:%i - ATSUPositionToOffset failed with %i, CharAt(%i) x=%i len=%i\n", _FL, (int)e, Px, x, len);
			else

		#endif
		{
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
	
	#elif defined(LGI_SDL)
	
	#else // This case is for Win32 and Haiku.
	
	#if defined(BEOS)
	if (Font && Font->Handle())
	#elif defined(WINNATIVE)
	GFontSystem *Sys = GFontSystem::Inst();
	if (Info.Length() && Font && Sys)
	#endif
	{
		int TabSize = Font->TabSize() ? Font->TabSize() : 32;
		int Cx = 0;
		int Char = 0;

		#if DEBUG_CHAR_AT
		printf("CharAt(%i) Str='%s'\n", Px, Str);
		#endif

		for (int i=0; i<Info.Length() && Status < 0; i++)
		{
			if (Px < Cx)
			{
				Status = Char;
				#if DEBUG_CHAR_AT
				printf("\tPx<Cx %i<%i\n", Px, Cx);
				#endif
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
							#if DEBUG_CHAR_AT
							printf("\tIn tab block %i\n", i);
							#endif
							break;
						}
						Cx += TabX;
						Char++;
					}
				}
				else
				{
					// Find the pos in this block
					GFont *f = Font;

					#if defined(BEOS)
					BString s(Info[i].Str, Info[i].Len);
					Font->Handle()->TruncateString(&s, B_TRUNCATE_END, Px - Cx);
					int Fit = s.CountChars();
					#elif defined(WIN32)
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

					int Fit = f->_CharAt(Px - Cx, Info[i].Str, Info[i].Len);
					#endif

					#if DEBUG_CHAR_AT
					printf("\tNon tab block %i, Fit=%i, Px-Cx=%i-%i=%i, Str='%.5s'\n",
						i, Fit, Px, Cx, Px-Cx, Info[i].Str);
					#endif
					if (Fit >= 0)
					{
						Status = Char + Fit;
					}
					else
					{
						Status = -1;
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
	
	#endif

	return Status;
}

int GDisplayString::Length()
{
	return len;
}

void GDisplayString::Length(int New)
{
    Layout();

	#if WINNATIVE
	
	if (New < len)
	{
		GFontSystem *Sys = GFontSystem::Inst();
		
		int CurX = 0;
		int CurLen = 0;
		for (int i=0; i<Info.Length(); i++)
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
				Info.Length(i + 1);
				
				// Leave the loop
				break;
			}
			
			CurX += Info[i].X;
			CurLen += Info[i].Len;
		}
	}
	else
	{
		printf("%s:%i - New>=Len (%i>=%i)\n", _FL, New, len);
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

GdcPt2 GDisplayString::Size()
{
    Layout();
	return GdcPt2(x, y);
}

#if defined LGI_SDL

template<typename OutPx>
bool CompositeText8Alpha(GSurface *Out, GSurface *In, GFont *Font, int px, int py)
{
	OutPx map[256];

	if (!Out || !In || !Font)
		return false;

	// FIXME, do blt clipping here...

	// Create colour map of the foreground/background colours		
	register uint8 *Div255 = Div255Lut;
	GColour fore = Font->Fore();
	GRgb24 fore_px;
	fore_px.r = fore.r();
	fore_px.g = fore.g();
	fore_px.b = fore.b();
	if (Font->Transparent())
	{
		for (int a=0; a<256; a++)
		{
			map[a].r = Div255[a * fore_px.r];
			map[a].g = Div255[a * fore_px.g];
			map[a].b = Div255[a * fore_px.b];
			map[a].a = a;
		}
	}
	else
	{
		GColour back = Font->Back();
		GRgb24 back_px;
		back_px.r = back.r();
		back_px.g = back.g();
		back_px.b = back.b();

		for (int a=0; a<256; a++)
		{
			int oma = 255 - a;
			map[a].r = (oma * back_px.r) + (a * fore_px.r) / 255;
			map[a].g = (oma * back_px.g) + (a * fore_px.g) / 255;
			map[a].b = (oma * back_px.b) + (a * fore_px.b) / 255;
			map[a].a = 255;
		}
	}

	uint8 *StartOfBuffer = (*Out)[0];
	uint8 *EndOfBuffer = StartOfBuffer + (Out->GetRowStep() * Out->Y());

	for (unsigned y=0; y<In->Y(); y++)
	{
		register OutPx *d = ((OutPx*) (*Out)[py + y]) + px;
		register uint8 *i = (*In)[y];
		if (!i) return false;
		register uint8 *e = i + In->X();

		LgiAssert((uint8*)d >= StartOfBuffer);
		
		if (Font->Transparent())
		{
			register uint8 a, o;
			register OutPx *s;
			
			while (i < e)
			{
				// Alpha blend map and output pixel
				a = *i++;
				switch (a)
				{
					case 0:
						break;
					case 255:
						// Copy
						*d = map[a];
						break;
					default:
						// Blend
						o = 255 - a;
						s = map + a;
						#define NonPreMulOver32NoAlpha(c)		d->c = ((s->c * a) + (Div255[d->c * 255] * o)) / 255
						NonPreMulOver32NoAlpha(r);
						NonPreMulOver32NoAlpha(g);
						NonPreMulOver32NoAlpha(b);
						/*
						dst->r = Div255[(oma * dst->r) + (a * src->r)];
						dst->g = Div255[(oma * dst->g) + (a * src->g)];
						dst->b = Div255[(oma * dst->b) + (a * src->b)];
						dst->a = (dst->a + src->a) + Div255[dst->a * src->a];
						*/
						break;
				}
				d++;
			}
		}
		else
		{
			while (i < e)
			{
				// Copy rop
				*d++ = map[*i++];
			}
		}

		LgiAssert((uint8*)d <= EndOfBuffer);
	}
	
	return true;
}

template<typename OutPx>
bool CompositeText8NoAlpha(GSurface *Out, GSurface *In, GFont *Font, int px, int py)
{
	GRgba32 map[256];

	if (!Out || !In || !Font)
		return false;

	// FIXME, do blt clipping here...

	// Create colour map of the foreground/background colours		
	register uint8 *DivLut = Div255Lut;
	GColour fore = Font->Fore();
	GRgb24 fore_px;
	fore_px.r = fore.r();
	fore_px.g = fore.g();
	fore_px.b = fore.b();
	if (Font->Transparent())
	{
		for (int a=0; a<256; a++)
		{
			map[a].r = DivLut[a * fore_px.r];
			map[a].g = DivLut[a * fore_px.g];
			map[a].b = DivLut[a * fore_px.b];
			map[a].a = a;
		}
	}
	else
	{
		GColour back = Font->Back();
		GRgb24 back_px;
		back_px.r = back.r();
		back_px.g = back.g();
		back_px.b = back.b();

		for (int a=0; a<256; a++)
		{
			int oma = 255 - a;
			map[a].r = DivLut[(oma * back_px.r) + (a * fore_px.r)];
			map[a].g = DivLut[(oma * back_px.g) + (a * fore_px.g)];
			map[a].b = DivLut[(oma * back_px.b) + (a * fore_px.b)];
			map[a].a = 255;
		}
	}

	uint8 *StartOfBuffer = (*Out)[0];
	uint8 *EndOfBuffer = StartOfBuffer + (Out->GetRowStep() * Out->Y());

	for (unsigned y=0; y<In->Y(); y++)
	{
		register OutPx *dst = ((OutPx*) (*Out)[py + y]) + px;
		register uint8 *i = (*In)[y];
		if (!i) return false;
		register uint8 *e = i + In->X();
		register GRgba32 *src;

		LgiAssert((uint8*)dst >= StartOfBuffer);
		
		if (Font->Transparent())
		{
			register uint8 a, oma;
			
			while (i < e)
			{
				// Alpha blend map and output pixel
				a = *i++;
				src = map + a;
				switch (a)
				{
					case 0:
						break;
					case 255:
						// Copy
						dst->r = src->r;
						dst->g = src->g;
						dst->b = src->b;
						break;
					default:
						// Blend
						OverPm32toPm24(src, dst);
						break;
				}
				dst++;
			}
		}
		else
		{
			while (i < e)
			{
				// Copy rop
				src = map + *i++;
				dst->r = src->r;
				dst->g = src->g;
				dst->b = src->b;
				dst++;
			}
		}

		LgiAssert((uint8*)dst <= EndOfBuffer);
	}
	
	return true;
}

template<typename OutPx>
bool CompositeText5NoAlpha(GSurface *Out, GSurface *In, GFont *Font, int px, int py)
{
	OutPx map[256];

	if (!Out || !In || !Font)
		return false;

	// FIXME, do blt clipping here...
	#define MASK_5BIT 0x1f
	#define MASK_6BIT 0x3f

	// Create colour map of the foreground/background colours		
	register uint8 *Div255 = Div255Lut;
	GColour fore = Font->Fore();
	GRgb24 fore_px;
	fore_px.r = fore.r();
	fore_px.g = fore.g();
	fore_px.b = fore.b();
	if (Font->Transparent())
	{
		for (int a=0; a<256; a++)
		{
			map[a].r = ((a * fore_px.r) / 255) >> 3;
			map[a].g = ((a * fore_px.g) / 255) >> 2;
			map[a].b = ((a * fore_px.b) / 255) >> 3;
		}
	}
	else
	{
		GColour back = Font->Back();
		GRgb24 back_px;
		back_px.r = back.r();
		back_px.g = back.g();
		back_px.b = back.b();

		for (int a=0; a<256; a++)
		{
			int oma = 255 - a;
			map[a].r = Div255[(oma * back_px.r) + (a * fore_px.r)] >> 3;
			map[a].g = Div255[(oma * back_px.g) + (a * fore_px.g)] >> 2;
			map[a].b = Div255[(oma * back_px.b) + (a * fore_px.b)] >> 3;
		}
	}

	uint8 *StartOfBuffer = (*Out)[0];
	uint8 *EndOfBuffer = StartOfBuffer + (Out->GetRowStep() * Out->Y());

	for (unsigned y=0; y<In->Y(); y++)
	{
		register OutPx *dst = ((OutPx*) (*Out)[py + y]);
		if (!dst)
			continue;
		dst += px;		
		register uint8 *i = (*In)[y];
		if (!i) return false;
		register uint8 *e = i + In->X();

		LgiAssert((uint8*)dst >= StartOfBuffer);
		
		if (Font->Transparent())
		{
			register uint8 a;
			register OutPx *src;
			
			while (i < e)
			{
				// Alpha blend map and output pixel
				a = *i++;
				switch (a)
				{
					case 0:
						break;
					case 255:
						// Copy
						*dst = map[a];
						break;
					default:
					{
						// Blend
						register uint8 a5 = a >> 3;
						register uint8 a6 = a >> 2;
						register uint8 oma5 = MASK_5BIT - a5;
						register uint8 oma6 = MASK_6BIT - a6;
						src = map + a;
						dst->r = ((oma5 * dst->r) + (a5 * src->r)) / MASK_5BIT;
						dst->g = ((oma6 * dst->g) + (a6 * src->g)) / MASK_6BIT;
						dst->b = ((oma5 * dst->b) + (a5 * src->b)) / MASK_5BIT;
						break;
					}
				}
				dst++;
			}
		}
		else
		{
			while (i < e)
			{
				// Copy rop
				*dst++ = map[*i++];
			}
		}
		
		LgiAssert((uint8*)dst <= EndOfBuffer);
	}
	
	return true;
}

#endif

void GDisplayString::Draw(GSurface *pDC, int px, int py, GRect *r)
{
    Layout();

	#if DISPLAY_STRING_FRACTIONAL_NATIVE

	// GTK / Mac use fractional pixels, so call the fractional version:
	GRect rc;
	if (r)
	{
		rc = *r;
		rc.x1 <<= FShift;
		rc.y1 <<= FShift;
		#ifdef MAC
		rc.x2 <<= FShift;
		rc.y2 <<= FShift;
		#else
		rc.x2 = (rc.x2 + 1) << FShift;
		rc.y2 = (rc.y2 + 1) << FShift;
		#endif
	}
	
	FDraw(pDC, px << FShift, py << FShift, r ? &rc : NULL);
	
	#elif defined LGI_SDL
	
	if (Img && pDC && pDC->Y() > 0 && (*pDC)[0])
	{
		int Ox = 0, Oy = 0;
		pDC->GetOrigin(Ox, Oy);

		/*
		if (!Font->Transparent())
		{
			GRect Fill;
			if (r)
				Fill = *r;
			else
			{
				Fill.ZOff(x-1, y-1);
				Fill.Offset(px, py);
			}
			Fill.Offset(-Ox, -Oy);		
		}
		*/

		GColourSpace DstCs = pDC->GetColourSpace();
		switch (DstCs)
		{
			#define DspStrCase(px_fmt, comp)											\
				case Cs##px_fmt:														\
					CompositeText##comp<G##px_fmt>(pDC, Img, Font, px-Ox, py-Oy);	\
					break;
			
			DspStrCase(Rgb16, 5NoAlpha)
			DspStrCase(Bgr16, 5NoAlpha)

			DspStrCase(Rgb24, 8NoAlpha)
			DspStrCase(Bgr24, 8NoAlpha)
			DspStrCase(Rgbx32, 8NoAlpha)
			DspStrCase(Bgrx32, 8NoAlpha)
			DspStrCase(Xrgb32, 8NoAlpha)
			DspStrCase(Xbgr32, 8NoAlpha)
			DspStrCase(Rgba32, 8Alpha)
			DspStrCase(Bgra32, 8Alpha)
			DspStrCase(Argb32, 8Alpha)
			DspStrCase(Abgr32, 8Alpha)
			default:
				LgiTrace("%s:%i - GDisplayString::Draw Unsupported colour space.\n", _FL);
				// LgiAssert(!"Unsupported colour space.");
				break;
			
			#undef DspStrCase
		}
	}
	else
		LgiTrace("::Draw argument error.\n");

	#elif defined WINNATIVE
	
	if (Info.Length() && pDC && Font)
	{
		GFontSystem *Sys = GFontSystem::Inst();
		COLOUR Old = pDC->Colour();
		int TabSize = Font->TabSize() ? Font->TabSize() : 32;
		int Ox = px;

		for (int i=0; i<Info.Length(); i++)
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
					b.x2 = i < Info.Length() - 1 ? px + Info[i].X - 1 : r->x2;
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
								int Dx = TabSize - ((X - Ox + GetDrawOffset()) % TabSize);
								GRect Char(X, b.y1, X + Dx - 1, b.y2);
								GColour WsCol = f->WhitespaceColour();
								LgiAssert(WsCol.IsValid());
								f->_Draw(pDC, X, py, Info[i].Str, 1, &Char, WsCol);
								X += Dx;
							}
						}
					}
					else
					{
						// Draw the character(s)
						GColour Fg = f->Fore();
						LgiAssert(Fg.IsValid());
						f->_Draw(pDC, px, py, Info[i].Str, Info[i].Len, &b, Fg);
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

			GColour Fg = Font->Fore();
			Font->_Draw(pDC, px, py, GDisplayStringDots, 3, &b, Fg);
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
	
	#elif defined(BEOS)

	if (pDC && Font)
	{
		rgb_color Fg, Bk;

		// Create colours
		GColour c = Font->Fore();
		Fg.red = c.r();
		Fg.green = c.g();
		Fg.blue = c.b();

		c = Font->Back();
		Bk.red = c.r();
		Bk.green = c.g();
		Bk.blue = c.b();

		// Paint text
		BView *Hnd = pDC->Handle();
		if (Hnd)
		{
			GLocker Locker(Hnd, _FL);
			Locker.Lock();

			// Draw background if required.		
			if (!Font->Transparent())
			{
				Hnd->SetHighColor(Bk);
				if (r)
				{
					BRect rc(r->x1, r->y1, r->x2, r->y2);
					Hnd->FillRect(rc);
				}
				else
				{
					GRect b;
					b.ZOff(x-1, y-1);
					b.Offset(px, py);
					BRect rc(b.x1, b.y1, b.x2, b.y2);
					Hnd->FillRect(rc);				
				}
			}

			// Draw foreground text segments		
			Hnd->SetHighColor(Fg);
			Hnd->SetLowColor(Bk);			
			Hnd->SetFont(Font->Handle());

			int CurX = 0;
			for (int i=0; i<Blocks; i++)
			{
				CharInfo *ci = Info + i;
				BPoint pos(px + CurX, py + Font->Ascent());
				if (ci->Str[0] != '\t')
					Hnd->DrawString(ci->Str, ci->Len, pos);
				#if 0 // useful to debug where strings are drawn
				{
					GRect r;
					r.ZOff(ci->X-1, Font->GetHeight()-1);
					r.Offset(px + CurX, py);
					pDC->Box(&r);
				}
				#endif
				CurX += ci->X;
			}
			
			if (!pDC->IsScreen())
				Hnd->Sync();
		}
		else printf("%s:%i - Error: no BView to draw on.\n", _FL);
	}
	else printf("%s:%i - Error: no DC or Font.\n", _FL);

	#endif
}

int GDisplayString::GetDrawOffsetF()
{
    return DrawOffsetF;
}

void GDisplayString::SetDrawOffsetF(int Fpx)
{
	if (LaidOut)
		LgiAssert(!"No point setting TabOrigin after string is laid out.\n");
    DrawOffsetF = Fpx;
}

int GDisplayString::FX()
{
    Layout();
	return xf;
}

int GDisplayString::FY()
{
    Layout();
	return y;
}

GdcPt2 GDisplayString::FSize()
{
    Layout();
	return GdcPt2(xf, yf);
}

void GDisplayString::FDraw(GSurface *pDC, int fx, int fy, GRect *frc, bool Debug)
{
    Layout(Debug);

	#if !DISPLAY_STRING_FRACTIONAL_NATIVE

	// BeOS / Windows don't use fractional pixels, so call the integer version:
	GRect rc;
	if (frc)
	{
		rc = *frc;
		rc.x1 >>= FShift;
		rc.y1 >>= FShift;
		rc.x2 >>= FShift;
		rc.y2 >>= FShift;
	}
	
	Draw(pDC, fx >> FShift, fy >> FShift, frc ? &rc : NULL);

	#elif defined __GTK_H__
	
	Gtk::pango_context_set_font_description(GFontSystem::Inst()->GetContext(), Font->Handle());
	Gtk::cairo_t *cr = pDC->Handle();
	if (!cr)
	{
		LgiAssert(!"Can't get cairo.");
		return;
	}

	int Ox = 0, Oy = 0;
	pDC->GetOrigin(Ox, Oy);
	
	Gtk::cairo_save(cr);
	
	GRect Client;
	if (pDC->GetClient(&Client) && Client.Valid())
	{
		Gtk::cairo_rectangle(cr, Client.x1, Client.y1, Client.X(), Client.Y());
		Gtk::cairo_clip(cr);
		Gtk::cairo_new_path(cr);
	}
	else
	{
		Client.ZOff(-1, -1);
	}
	
	GColour b = Font->Back();
	
	Gtk::cairo_set_source_rgb(cr,
								(double)b.r()/255.0,
								(double)b.g()/255.0,
								(double)b.b()/255.0);

	if (!Font->Transparent() && frc)
	{
		Gtk::cairo_new_path(cr);
		Gtk::cairo_rectangle
		(
			cr,
			((double)frc->x1 / FScale) - Ox,
			((double)frc->y1 / FScale) - Oy,
			(double)frc->X() / FScale,
			(double)frc->Y() / FScale
		);
		Gtk::cairo_fill(cr);
	}

	double Dx = ((double)fx / FScale) - Ox;
	double Dy = ((double)fy / FScale) - Oy;

	cairo_translate(cr, Dx, Dy);	
	
	if (!Font->Transparent() && !frc)
	{
		Gtk::cairo_new_path(cr);
		Gtk::cairo_rectangle(cr, 0, 0, x, y);
		Gtk::cairo_fill(cr);
	}
	if (Hnd)
	{
	    GColour f = Font->Fore();
	    Gtk::cairo_set_source_rgb(	cr,
								    (double)f.r()/255.0,
								    (double)f.g()/255.0,
								    (double)f.b()/255.0);
	    Gtk::pango_cairo_show_layout(cr, Hnd);
	}
	
	Gtk::cairo_restore(cr);
	
	#elif defined MAC && !defined COCOA && !defined(LGI_SDL)

	int Ox = 0, Oy = 0;
	int px = fx >> FShift;
	int py = fy >> FShift;
	GRect rc;
	if (frc)
		rc.Set(	frc->x1 >> FShift,
				frc->y1 >> FShift,
				frc->x2 >> FShift,
				frc->y2 >> FShift);

	if (pDC && !pDC->IsScreen())
		pDC->GetOrigin(Ox, Oy);

	if (pDC && !Font->Transparent())
	{
		GColour Old = pDC->Colour(Font->Back());
		if (frc)
		{
			pDC->Rectangle(&rc);
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
		OsPainter				dc = pDC->Handle();

		#if USE_CORETEXT

			int y = (pDC->Y() - py + Oy);

			CGContextSaveGState(dc);
			pDC->Colour(Font->Fore());
		
			if (pDC->IsScreen())
			{
				if (frc)
				{
					CGRect rect = rc;
					rect.size.width += 1.0;
					rect.size.height += 1.0;
					CGContextClipToRect(dc, rect);
				}
				CGContextTranslateCTM(dc, 0, pDC->Y()-1);
				CGContextScaleCTM(dc, 1.0, -1.0);
			}
			else
			{
				if (frc)
				{
					CGContextSaveGState(dc);
					
					CGRect rect = rc;
					rect.origin.x -= Ox;
					rect.origin.y = pDC->Y() - rect.origin.y + Oy - rect.size.height;
					rect.size.width += 1.0;
					rect.size.height += 1.0;
					CGContextClipToRect(dc, rect);
				}
			}

			CGFloat Tx = (CGFloat)fx / FScale - Ox;
			CGFloat Ty = (CGFloat)y - Font->Ascent();
			CGContextSetTextPosition(dc, Tx, Ty);

			CTLineDraw(Hnd, dc);

			CGContextRestoreGState(dc);

		#else

			ATSUAttributeTag        Tags[1] = {kATSUCGContextTag};
			ByteCount               Sizes[1] = {sizeof(CGContextRef)};
			ATSUAttributeValuePtr   Values[1] = {&dc};

			e = ATSUSetLayoutControls(Hnd, 1, Tags, Sizes, Values);
			if (e)
			{
				printf("%s:%i - ATSUSetLayoutControls failed (e=%i)\n", _FL, (int)e);
			}
			else
			{
				// Set style attr
				ATSURGBAlphaColor c;
				GColour Fore = Font->Fore();
				c.red   = (double) Fore.r() / 255.0;
				c.green = (double) Fore.g() / 255.0;
				c.blue  = (double) Fore.b() / 255.0;
				c.alpha = 1.0;
				
				ATSUAttributeTag Tags[]			= {kATSURGBAlphaColorTag};
				ATSUAttributeValuePtr Values[]	= {&c};
				ByteCount Lengths[]				= {sizeof(c)};
				
				e = ATSUSetAttributes(  Font->Handle(),
										CountOf(Tags),
										Tags,
										Lengths,
										Values);
				if (e)
				{
					printf("%s:%i - Error setting font attr (e=%i)\n", _FL, (int)e);
				}
				else
				{
					int y = (pDC->Y() - py + Oy);

					if (pDC->IsScreen())
					{
						CGContextSaveGState(dc);

						if (frc)
						{
							CGRect rect = rc;
							rect.size.width += 1.0;
							rect.size.height += 1.0;
							CGContextClipToRect(dc, rect);
						}
						CGContextTranslateCTM(dc, 0, pDC->Y()-1);
						CGContextScaleCTM(dc, 1.0, -1.0);

						e = ATSUDrawText(Hnd, kATSUFromTextBeginning, kATSUToTextEnd, fx - Long2Fix(Ox), Long2Fix(y) - fAscent);

						CGContextRestoreGState(dc);
					}
					else
					{
						if (frc)
						{
							CGContextSaveGState(dc);
							
							CGRect rect = rc;
							rect.origin.x -= Ox;
							rect.origin.y = pDC->Y() - rect.origin.y + Oy - rect.size.height;
							rect.size.width += 1.0;
							rect.size.height += 1.0;
							CGContextClipToRect(dc, rect);
						}
						
						e = ATSUDrawText(Hnd, kATSUFromTextBeginning, kATSUToTextEnd, Long2Fix(px - Ox), Long2Fix(y) - fAscent);
						
						if (frc)
							CGContextRestoreGState(dc);
					}
					if (e)
					{
						char *a = 0;
						StringConvert(a, NULL, Str, len);
						printf("%s:%i - ATSUDrawText failed with %i, len=%i, str=%.20s\n", _FL, (int)e, len, a);
						DeleteArray(a);
					}
				}
			}
		#endif
	}
	
	#endif
}

