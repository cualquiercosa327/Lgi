#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"
#include "Emoji.h"

//////////////////////////////////////////////////////////////////////////////////////////////////
GRichTextPriv::StyleText::StyleText(const StyleText *St)
{
	Emoji = St->Emoji;
	Style = NULL;
	Element = St->Element;
	Param = St->Param;
	if (St->Style)
		SetStyle(St->Style);
	Add((uint32*)&St->ItemAt(0), St->Length());
}
		
GRichTextPriv::StyleText::StyleText(const uint32 *t, int Chars, GNamedStyle *style)
{
	Emoji = false;
	Style = NULL;
	Element = CONTENT;
	if (style)
		SetStyle(style);
	if (t)
	{
		if (Chars < 0)
			Chars = Strlen(t);
		Add((uint32*)t, Chars);
	}
}

uint32 *GRichTextPriv::StyleText::At(int i)
{
	if (i >= 0 && i < (int)Length())
		return &(*this)[i];
	LgiAssert(0);
	return NULL;
}
		
GNamedStyle *GRichTextPriv::StyleText::GetStyle()
{
	return Style;
}
				
void GRichTextPriv::StyleText::SetStyle(GNamedStyle *s)
{
	if (Style != s)
	{
		Style = s;
		Colours.Empty();
				
		if (Style)
		{			
			GCss::ColorDef c = Style->Color();
			if (c.Type == GCss::ColorRgb)
				Colours.Fore.Set(c.Rgb32, 32);
			c = Style->BackgroundColor();
			if (c.Type == GCss::ColorRgb)
				Colours.Back.Set(c.Rgb32, 32);
		}				
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
GRichTextPriv::EmojiDisplayStr::EmojiDisplayStr(StyleText *src, GSurface *img, GFont *f, const uint32 *s, int l) :
	DisplayStr(src, NULL, s, l)
{
	Img = img;
	#if defined(_MSC_VER)
	Utf16to32(Utf32, (const uint16*) StrCache.Get(), len);
	uint32 *u = &Utf32[0];
	#else
	LgiAssert(sizeof(char16) == 4);
	uint32 *u = (uint32*)StrCache.Get();
	Chars = Strlen(u);
	#endif

	for (int i=0; i<Chars; i++)
	{
		int Idx = EmojiToIconIndex(u + i, Chars - i);
		LgiAssert(Idx >= 0);
		if (Idx >= 0)
		{
			int x = Idx % EMOJI_GROUP_X;
			int y = Idx / EMOJI_GROUP_X;
			GRect &rc = SrcRect[i];
			rc.ZOff(EMOJI_CELL_SIZE-1, EMOJI_CELL_SIZE-1);
			rc.Offset(x * EMOJI_CELL_SIZE, y * EMOJI_CELL_SIZE);
		}
	}

	x = SrcRect.Length() * EMOJI_CELL_SIZE;
	y = EMOJI_CELL_SIZE;
	xf = IntToFixed(x);
	yf = IntToFixed(y);
}

GAutoPtr<GRichTextPriv::DisplayStr> GRichTextPriv::EmojiDisplayStr::Clone(int Start, int Len)
{
	if (Len < 0)
		Len = Chars - Start;
	#if defined(_MSC_VER)
	LgiAssert(	Start >= 0 &&
				Start < (int)Utf32.Length() &&
				Start + Len <= (int)Utf32.Length());
	#endif
	GAutoPtr<DisplayStr> s(new EmojiDisplayStr(Src, Img, NULL,
		#if defined(_MSC_VER)
		&Utf32[Start]
		#else
		(uint32*)(const char16*)(*this)
		#endif
		, Len));
	return s;
}

void GRichTextPriv::EmojiDisplayStr::Paint(GSurface *pDC, int &FixX, int FixY, GColour &Back)
{
	GRect f(0, 0, x-1, y-1);
	f.Offset(FixedToInt(FixX), FixedToInt(FixY));
	pDC->Colour(Back);
	pDC->Rectangle(&f);

	int Op = pDC->Op(GDC_ALPHA);
	for (unsigned i=0; i<SrcRect.Length(); i++)
	{
		pDC->Blt(f.x1, f.y1, Img, &SrcRect[i]);
		f.x1 += EMOJI_CELL_SIZE;
		FixX += IntToFixed(EMOJI_CELL_SIZE);
	}
	pDC->Op(Op);
}

double GRichTextPriv::EmojiDisplayStr::GetAscent()
{
	return EMOJI_CELL_SIZE * 0.8;
}

int GRichTextPriv::EmojiDisplayStr::PosToIndex(int XPos, bool Nearest)
{
	if (XPos >= (int)x)
		return Chars;
	if (XPos <= 0)
		return 0;
	return (XPos + (Nearest ? EMOJI_CELL_SIZE >> 1 : 0)) / EMOJI_CELL_SIZE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
GRichTextPriv::TextLine::TextLine(int XOffsetPx, int WidthPx, int YOffsetPx)
{
	NewLine = 0;
	PosOff.ZOff(0, 0);
	PosOff.Offset(XOffsetPx, YOffsetPx);
}

int GRichTextPriv::TextLine::Length()
{
	int Len = NewLine;
	for (unsigned i=0; i<Strs.Length(); i++)
		Len += Strs[i]->Chars;
	return Len;
}
		
/// This runs after the layout line has been filled with display strings.
/// It measures the line and works out the right offsets for each strings
/// so that their baselines all match up correctly.
void GRichTextPriv::TextLine::LayoutOffsets(int DefaultFontHt)
{
	double BaseLine = 0.0;
	int HtPx = 0;
			
	for (unsigned i=0; i<Strs.Length(); i++)
	{
		DisplayStr *ds = Strs[i];
		double Ascent = ds->GetAscent();
		BaseLine = max(BaseLine, Ascent);
		HtPx = max(HtPx, ds->Y());
	}
			
	if (Strs.Length() == 0)
		HtPx = DefaultFontHt;
	else
		LgiAssert(HtPx > 0);
			
	for (unsigned i=0; i<Strs.Length(); i++)
	{
		DisplayStr *ds = Strs[i];
		double Ascent = ds->GetAscent();
		if (Ascent > 0.0)
			ds->OffsetY = (int)(BaseLine - Ascent);
		LgiAssert(ds->OffsetY >= 0);
		HtPx = max(HtPx, ds->OffsetY+ds->Y());
	}
			
	PosOff.y2 = PosOff.y1 + HtPx - 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
GRichTextPriv::TextBlock::TextBlock(GRichTextPriv *priv) : Block(priv)
{
	LayoutDirty = false;
	Len = 0;
	Pos.ZOff(-1, -1);
	Style = NULL;
	Fnt = NULL;
	ClickErrIdx = -1;
			
	Margin.ZOff(0, 0);
	Border.ZOff(0, 0);
	Padding.ZOff(0, 0);
}

GRichTextPriv::TextBlock::TextBlock(const TextBlock *Copy) : Block(Copy)
{
	LayoutDirty = true;
	Len = Copy->Len;
	Pos = Copy->Pos;
	Style = Copy->Style;
	Fnt = Copy->Fnt;
			
	Margin = Copy->Margin;
	Border = Copy->Border;
	Padding = Copy->Padding;

	for (unsigned i=0; i<Copy->Txt.Length(); i++)
	{
		Txt.Add(new StyleText(Copy->Txt.ItemAt(i)));
	}
}
		
GRichTextPriv::TextBlock::~TextBlock()
{
	LgiAssert(Cursors == 0);
	Txt.DeleteObjects();
}

void GRichTextPriv::TextBlock::Dump()
{
	LgiTrace("    Txt.Len=%i, margin=%s, border=%s, padding=%s\n",
				Txt.Length(),
				Margin.GetStr(),
				Border.GetStr(),
				Padding.GetStr());
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		GString s(t->Length() ?
					#ifndef WINDOWS
					(char16*)
					#endif
					t->At(0) : NULL, t->Length());
		s = s.Strip();
				
		LgiTrace("    %p: style=%p/%s, len=%i\n",
			t,
			t->GetStyle(),
			t->GetStyle() ? t->GetStyle()->Name.Get() : NULL,
			t->Length());
	}
}
		
GNamedStyle *GRichTextPriv::TextBlock::GetStyle(int At)
{
	if (At >= 0)
	{
		GArray<StyleText*> t;
		if (GetTextAt(At, t))
			return t[0]->GetStyle();
	}

	return Style;
}
		
void GRichTextPriv::TextBlock::SetStyle(GNamedStyle *s)
{
	if ((Style = s))
	{
		Fnt = d->GetFont(s);
		LayoutDirty = true;
		LgiAssert(Fnt != NULL);

		Margin.x1 = Style->MarginLeft().ToPx(Pos.X(), Fnt);
		Margin.y1 = Style->MarginTop().ToPx(Pos.Y(), Fnt);
		Margin.x2 = Style->MarginRight().ToPx(Pos.X(), Fnt);
		Margin.y2 = Style->MarginBottom().ToPx(Pos.Y(), Fnt);

		Border.x1 = Style->BorderLeft().ToPx(Pos.X(), Fnt);
		Border.y1 = Style->BorderTop().ToPx(Pos.Y(), Fnt);
		Border.x2 = Style->BorderRight().ToPx(Pos.X(), Fnt);
		Border.y2 = Style->BorderBottom().ToPx(Pos.Y(), Fnt);

		Padding.x1 = Style->PaddingLeft().ToPx(Pos.X(), Fnt);
		Padding.y1 = Style->PaddingTop().ToPx(Pos.Y(), Fnt);
		Padding.x2 = Style->PaddingRight().ToPx(Pos.X(), Fnt);
		Padding.y2 = Style->PaddingBottom().ToPx(Pos.Y(), Fnt);
	}
}

int GRichTextPriv::TextBlock::Length()
{
	return Len;
}

HtmlTag IsDefaultStyle(HtmlTag Id, GCss *Css)
{
	if (!Css)
		return CONTENT;

	if (Css->Length() == 2)
	{
		GCss::ColorDef c = Css->Color();
		if ((GColour)c != GColour::Blue)
			return CONTENT;
				
		GCss::TextDecorType td = Css->TextDecoration();
		if (td != GCss::TextDecorUnderline)
			return CONTENT;

		return TAG_A;
	}
	else if (Css->Length() == 1)
	{
		GCss::FontWeightType fw = Css->FontWeight();
		if (fw == GCss::FontWeightBold ||
			fw == GCss::FontWeightBolder ||
			fw >= GCss::FontWeight700)
			return TAG_B;

		GCss::TextDecorType td = Css->TextDecoration();
		if (td == GCss::TextDecorUnderline)
			return TAG_U;

		GCss::FontStyleType fs = Css->FontStyle();
		if (fs == GCss::FontStyleItalic)
			return TAG_I;
	}

	return CONTENT;
}

bool GRichTextPriv::TextBlock::ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media)
{
	s.Print("<p>");
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		GNamedStyle *style = t->GetStyle();
		int tlen = t->Length();
		if (!tlen)
			continue;
		
		GString utf(
			#ifndef WINDOWS
			(char16*)
			#endif
			t->At(0), t->Length());
		char *str = utf;

		const char *ElemName = NULL;
		if (t->Element != CONTENT)
		{
			GHtmlElemInfo *e = d->Inst.Static->GetTagInfo(t->Element);
			if (!e)
				return false;
			ElemName = e->Tag;
			if (style)
			{
				HtmlTag tag = IsDefaultStyle(t->Element, style);
				if (tag == t->Element)
					style = NULL;
			}
		}
		else
		{
			HtmlTag tag = IsDefaultStyle(t->Element, style);
			if (tag != CONTENT)
			{
				GHtmlElemInfo *e = d->Inst.Static->GetTagInfo(tag);
				if (e)
				{
					ElemName = e->Tag;
					style = NULL;
				}
			}			
		}

		if (style && !ElemName)
			ElemName = "span";
		if (ElemName)
			s.Print("<%s", ElemName);
		if (style)
			s.Print(" class='%s'", style->Name.Get());
		if (t->Element == TAG_A && t->Param)
			s.Print(" href='%s'", t->Param.Get());
		if (ElemName)
			s.Print(">");
		
		// Encode entities...
		GUtf8Ptr last(str);
		GUtf8Ptr cur(str);
		GUtf8Ptr end(str + utf.Length());
		while (cur < end)
		{
			int32 ch = cur;
			switch (ch)
			{
				case '<':
					s.Print("%.*s&lt;", cur - last, last.GetPtr());
					last = ++cur;
					break;
				case '>':
					s.Print("%.*s&gt;", cur - last, last.GetPtr());
					last = ++cur;
					break;
				case '\n':
					s.Print("%.*s<br>\n", cur - last, last.GetPtr());
					last = ++cur;
					break;
				case '&':
					s.Print("%.*s&amp;", cur - last, last.GetPtr());
					last = ++cur;
					break;
				case 0xa0:
					s.Print("%.*s&nbsp;", cur - last, last.GetPtr());
					last = ++cur;
					break;
				default:
					cur++;
					break;
			}
		}
		s.Print("%.*s", cur - last, last.GetPtr());

		if (ElemName)
			s.Print("</%s>", ElemName);
	}
	s.Print("</p>\n");
	return true;
}		

bool GRichTextPriv::TextBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor)
		return d->Error(_FL, "No cursor param.");

	if (LayoutDirty)
	{
		Cursor->Pos.ZOff(-1, -1); // This is valid behaviour... need to 
								// wait for layout before getting cursor
								// position.
		return false;
	}
		
	int CharPos = 0;
	int LastY = 0;
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		PtrCheckBreak(tl);

		GRect r = tl->PosOff;
		r.Offset(Pos.x1, Pos.y1);
				
		int FixX = 0;
		for (unsigned n=0; n<tl->Strs.Length(); n++)
		{
			DisplayStr *ds = tl->Strs[n];
			int dsChars = ds->Chars;
					
			if
			(
				Cursor->Offset >= CharPos
				&&
				Cursor->Offset <= CharPos + dsChars
				&&
				(
					Cursor->LineHint < 0
					||
					Cursor->LineHint == i
				)
			)
			{
				int CharOffset = Cursor->Offset - CharPos;
				if (CharOffset == 0)
				{
					// First char
					Cursor->Pos.x1 = r.x1 + FixedToInt(FixX);
				}
				else if (CharOffset == dsChars)
				{
					// Last char
					Cursor->Pos.x1 = r.x1 + FixedToInt(FixX + ds->FX());
				}
				else
				{
					// In the middle somewhere...
					GAutoPtr<DisplayStr> Tmp = ds->Clone(0, CharOffset);
					// GDisplayString Tmp(ds->GetFont(), *ds, CharOffset);
					if (Tmp)
						Cursor->Pos.x1 = r.x1 + FixedToInt(FixX + Tmp->FX());
				}

				Cursor->Pos.y1 = r.y1 + ds->OffsetY;
				Cursor->Pos.y2 = Cursor->Pos.y1 + ds->Y() - 1;
				Cursor->Pos.x2 = Cursor->Pos.x1 + 1;

				Cursor->Line.Set(Pos.x1, r.y1, Pos.x2, r.y2);
				return true;
			}					
					
			FixX += ds->FX();
			CharPos += ds->Chars;
		}
				
		if
		(
			(
				tl->Strs.Length() == 0
				||
				i == Layout.Length() - 1
			)
			&&
			Cursor->Offset == CharPos
		)
		{
			// Cursor at the start of empty line.
			Cursor->Pos.x1 = r.x1;
			Cursor->Pos.x2 = Cursor->Pos.x1 + 1;
			Cursor->Pos.y1 = r.y1;
			Cursor->Pos.y2 = r.y2;

			Cursor->Line.Set(Pos.x1, r.y1, Pos.x2, r.y2);
			return true;
		}
				
		CharPos += tl->NewLine;
		LastY = tl->PosOff.y2;
	}
	
	if (Cursor->Offset == 0 &&
		Len == 0)
	{
		Cursor->Pos.x1 = Pos.x1;
		Cursor->Pos.x2 = Pos.x1 + 1;
		Cursor->Pos.y1 = Pos.y1;
		Cursor->Pos.y2 = Pos.y2;

		Cursor->Line = Pos;
		return true;
	}
			
	return false;
}
		
bool GRichTextPriv::TextBlock::HitTest(HitTestResult &htr)
{
	if (htr.In.y < Pos.y1 || htr.In.y > Pos.y2)
		return false;

	int CharPos = 0;
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		PtrCheckBreak(tl);
		int InitCharPos = CharPos;

		GRect r = tl->PosOff;
		r.Offset(Pos.x1, Pos.y1);
		bool Over = r.Overlap(htr.In.x, htr.In.y);
		bool OnThisLine =	htr.In.y >= r.y1 &&
							htr.In.y <= r.y2;
		if (OnThisLine && htr.In.x <= r.x1)
		{
			htr.Near = true;
			htr.Idx = CharPos;
			htr.LineHint = i;
			return true;
		}
		
		int FixX = 0;
		int InputX = IntToFixed(htr.In.x - Pos.x1 - tl->PosOff.x1);
		for (unsigned n=0; n<tl->Strs.Length(); n++)
		{
			DisplayStr *ds = tl->Strs[n];
			int dsFixX = ds->FX();
			
			if (Over &&
				InputX >= FixX &&
				InputX < FixX + dsFixX)
			{
				int OffFix = InputX - FixX;
				int OffPx = FixedToInt(OffFix);
				int OffChar = ds->PosToIndex(OffPx, true);

				// d->DebugRects[0].Set(Pos.x1, r.y1, Pos.x1 + InputX+1, r.y2);
						
				htr.Blk = this;
				htr.Ds = ds;
				htr.Idx = CharPos + OffChar;
				htr.LineHint = i;
				return true;
			}
					
			FixX += ds->FX();

			CharPos += ds->Chars;
		}

		if (OnThisLine)
		{
			htr.Near = true;
			htr.Idx = CharPos;
			htr.LineHint = i;
			return true;
		}
				
		CharPos += tl->NewLine;
	}

	return false;
}

void DrawDecor(GSurface *pDC, GRichTextPriv::DisplayStr *Ds, int Fx, int Fy, int Start, int Len)
{
	GColour Old = pDC->Colour(GColour::Red);
	GDisplayString ds1(Ds->GetFont(), (const char16*)(*Ds), Start);
	GDisplayString ds2(Ds->GetFont(), (const char16*)(*Ds), Start+Len);

	int x = (Fx >> GDisplayString::FShift);
	int y = (Fy >> GDisplayString::FShift) + (int)Ds->GetAscent() + 1;
	int End = x + ds2.X();
	x += ds1.X();
	while (x < End)
	{
		pDC->Set(x, y+(x%2));
		x++;
	}
}

bool Overlap(GSpellCheck::SpellingError *e, int start, int len)
{
	if (!e)
		return false;
	if (start+len <= e->Start)
		return false;
	if (start >= e->End())
		return false;
	return true;
}

void GRichTextPriv::TextBlock::DrawDisplayString(GSurface *pDC, DisplayStr *Ds, int &FixX, int FixY, GColour &Bk, int &Pos)
{
	int OldX = FixX;

	// Paint the string itself...
	Ds->Paint(pDC, FixX, FixY, Bk);

	// Does the a spelling error overlap this string?
	int DsEnd = Pos + Ds->Chars;
	while (Overlap(SpErr, Pos, Ds->Chars))
	{
		// Yes, work out the region of characters and paint the decor
		int Start = max(SpErr->Start, Pos);
		int Len = min(SpErr->End(), Pos + Ds->Chars) - Start;
		
		// Draw the decor for the error
		DrawDecor(pDC, Ds, OldX, FixY, Start - Pos, Len);
		
		if (SpErr->End() < DsEnd)
		{
			// Are there more errors?
			SpErr = SpellingErrors.AddressOf(++PaintErrIdx);
		}
		else break;
	}

	Pos += Ds->Chars;
}


void GRichTextPriv::TextBlock::OnPaint(PaintContext &Ctx)
{
	int CharPos = 0;
	int EndPoints = 0;
	int EndPoint[2] = {-1, -1};
	int CurEndPoint = 0;

	if (Cursors > 0 && Ctx.Select)
	{
		// Selection end point checks...
		if (Ctx.Cursor && Ctx.Cursor->Blk == this)
			EndPoint[EndPoints++] = Ctx.Cursor->Offset;
		if (Ctx.Select && Ctx.Select->Blk == this)
			EndPoint[EndPoints++] = Ctx.Select->Offset;
				
		// Sort the end points
		if (EndPoints > 1 &&
			EndPoint[0] > EndPoint[1])
		{
			int ep = EndPoint[0];
			EndPoint[0] = EndPoint[1];
			EndPoint[1] = ep;
		}
	}
	
	// Paint margins, borders and padding...
	GRect r = Pos;
	r.x1 -= Margin.x1;
	r.y1 -= Margin.y1;
	r.x2 -= Margin.x2;
	r.y2 -= Margin.y2;
	GCss::ColorDef BorderStyle;
	if (Style)
		BorderStyle = Style->BorderLeft().Color;
	GColour BorderCol(222, 222, 222);
	if (BorderStyle.Type == GCss::ColorRgb)
		BorderCol.Set(BorderStyle.Rgb32, 32);

	Ctx.DrawBox(r, Margin, Ctx.Colours[Unselected].Back);
	Ctx.DrawBox(r, Border, BorderCol);
	Ctx.DrawBox(r, Padding, Ctx.Colours[Unselected].Back);
	
	int CurY = Pos.y1;
	PaintErrIdx = 0;
	SpErr = SpellingErrors.AddressOf(PaintErrIdx);

	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *Line = Layout[i];

		GRect LinePos = Line->PosOff;
		LinePos.Offset(Pos.x1, Pos.y1);
		if (Line->PosOff.X() < Pos.X())
		{
			Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
			Ctx.pDC->Rectangle(LinePos.x2, LinePos.y1, Pos.x2, LinePos.y2);
		}

		int FixX = IntToFixed(LinePos.x1);
		
		if (CurY < LinePos.y1)
		{
			// Fill padded area...
			Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
			Ctx.pDC->Rectangle(Pos.x1, CurY, Pos.x2, LinePos.y1 - 1);
		}

		CurY = LinePos.y1;
		GFont *Fnt = NULL;

		#if DEBUG_NUMBERED_LAYOUTS
		GString s;
		s.Printf("%i", Ctx.Index);
		Ctx.Index++;
		#endif

		for (unsigned n=0; n<Line->Strs.Length(); n++)
		{
			DisplayStr *Ds = Line->Strs[n];
			GFont *DsFnt = Ds->GetFont();
			ColourPair &Cols = Ds->Src->Colours;
			if (DsFnt && DsFnt != Fnt)
			{
				Fnt = DsFnt;
				Fnt->Transparent(false);
			}

			// If the current text part doesn't cover the full line height we have to
			// fill in the rest here...
			if (Ds->Y() < Line->PosOff.Y())
			{
				Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
				int CurX = FixedToInt(FixX);
				if (Ds->OffsetY > 0)
					Ctx.pDC->Rectangle(CurX, CurY, CurX+Ds->X(), CurY+Ds->OffsetY-1);

				int DsY2 = Ds->OffsetY + Ds->Y();
				if (DsY2 < Pos.Y())
					Ctx.pDC->Rectangle(CurX, CurY+DsY2, CurX+Ds->X(), Pos.y2);
			}

			// Check for selection changes...
			int FixY = IntToFixed(CurY + Ds->OffsetY);

			#if DEBUG_OUTLINE_CUR_STYLE_TEXT
			GRect r(0, 0, -1, -1);
			if (Ctx.Cursor->Blk == (Block*)this)
			{
				GArray<StyleText*> CurStyle;
				if (GetTextAt(Ctx.Cursor->Offset, CurStyle) && Ds->Src == CurStyle.First())
				{
					r.ZOff(Ds->X()-1, Ds->Y()-1);
					r.Offset(FixedToInt(FixX), FixedToInt(FixY));
				}
			}
			#endif

			if (CurEndPoint < EndPoints &&
				EndPoint[CurEndPoint] >= CharPos &&
				EndPoint[CurEndPoint] <= CharPos + Ds->Chars)
			{
				// Process string into parts based on the selection boundaries
				int Ch = EndPoint[CurEndPoint] - CharPos;
				int TmpPos = CharPos;
				GAutoPtr<DisplayStr> ds1 = Ds->Clone(0, Ch);
						
				// First part...
				GColour Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
				if (DsFnt)
					DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
				if (ds1)
					DrawDisplayString(Ctx.pDC, ds1, FixX, FixY, Bk, TmpPos);


				Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
				CurEndPoint++;
						
				// Is there 3 parts?
				//
				// This happens when the selection starts and end in the one string.
				//
				// The alternative is that it starts or ends in the strings but the other
				// end point is in a different string. In which case there is only 2 strings
				// to draw.
				if (CurEndPoint < EndPoints &&
					EndPoint[CurEndPoint] >= CharPos &&
					EndPoint[CurEndPoint] <= CharPos + Ds->Chars)
				{
					// Yes..
					int Ch2 = EndPoint[CurEndPoint] - CharPos;

					// Part 2
					GAutoPtr<DisplayStr> ds2 = Ds->Clone(Ch, Ch2 - Ch);
					GColour Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
					if (DsFnt)
						DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
					if (ds2)
						DrawDisplayString(Ctx.pDC, ds2, FixX, FixY, Bk, TmpPos);
					Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
					CurEndPoint++;

					// Part 3
					GAutoPtr<DisplayStr> ds3 = Ds->Clone(Ch2);
					Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
					if (DsFnt)
						DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
					if (ds3)
						DrawDisplayString(Ctx.pDC, ds3, FixX, FixY, Bk, TmpPos);
				}
				else if (Ch < Ds->Chars)
				{
					// No... draw 2nd part
					GAutoPtr<DisplayStr> ds2 = Ds->Clone(Ch);
					GColour Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
					if (DsFnt)
						DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
					if (ds2)	
						DrawDisplayString(Ctx.pDC, ds2, FixX, FixY, Bk, TmpPos);
				}
			}
			else
			{
				// No selection changes... draw the whole string
				GColour Bk = Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back();
				if (DsFnt)
					DsFnt->Colour(Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(), Bk);
						
				#if DEBUG_OUTLINE_CUR_DISPLAY_STR
				int OldFixX = FixX;
				#endif

				int TmpPos = CharPos;
				DrawDisplayString(Ctx.pDC, Ds, FixX, FixY, Bk, TmpPos);

				#if DEBUG_OUTLINE_CUR_DISPLAY_STR
				if (Ctx.Cursor->Blk == (Block*)this &&
					Ctx.Cursor->Offset >= CharPos &&
					Ctx.Cursor->Offset < CharPos + Ds->Chars)
				{
					GRect r(0, 0, Ds->X()-1, Ds->Y()-1);
					r.Offset(FixedToInt(OldFixX), FixedToInt(FixY));
					Ctx.pDC->Colour(GColour::Red);
					Ctx.pDC->Box(&r);
				}
				#endif
			}

			#if DEBUG_OUTLINE_CUR_STYLE_TEXT
			if (r.Valid())
			{
				Ctx.pDC->Colour(GColour(192, 192, 192));
				Ctx.pDC->LineStyle(GSurface::LineDot);
				Ctx.pDC->Box(&r);
				Ctx.pDC->LineStyle(GSurface::LineSolid);
			}
			#endif

			CharPos += Ds->Chars;
		}
		if (Line->Strs.Length() == 0)
		{
			if (CurEndPoint < EndPoints &&
				EndPoint[CurEndPoint] == CharPos)
			{
				Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
				CurEndPoint++;
			}
		}
		if (Ctx.Type == Selected)
		{
			// Draw new line
			int x1 = FixedToInt(FixX);
			FixX += IntToFixed(5);
			int x2 = FixedToInt(FixX);
			Ctx.pDC->Colour(Ctx.Colours[Selected].Back);
			Ctx.pDC->Rectangle(x1, LinePos.y1, x2, LinePos.y2);
		}
		Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
		Ctx.pDC->Rectangle(FixedToInt(FixX), LinePos.y1, Pos.x2, LinePos.y2);
		

		#if DEBUG_NUMBERED_LAYOUTS
		GDisplayString Ds(SysFont, s);
		SysFont->Colour(GColour::Green, GColour::White);
		SysFont->Transparent(false);
		Ds.Draw(Ctx.pDC, LinePos.x1, LinePos.y1);
		/*
		Ctx.pDC->Colour(GColour::Blue);
		Ctx.pDC->Line(LinePos.x1, LinePos.y1,LinePos.x2,LinePos.y2);
		*/
		#endif
				
		CurY = LinePos.y2 + 1;
		CharPos += Line->NewLine;
	}

	if (CurY < Pos.y2)
	{
		// Fill padded area...
		Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
		Ctx.pDC->Rectangle(Pos.x1, CurY, Pos.x2, Pos.y2);
	}

	if (Ctx.Cursor &&
		Ctx.Cursor->Blk == this &&
		Ctx.Cursor->Blink &&
		d->View->Focus())
	{
		Ctx.pDC->Colour(CursorColour);
		if (Ctx.Cursor->Pos.Valid())
			Ctx.pDC->Rectangle(&Ctx.Cursor->Pos);
		else
			Ctx.pDC->Rectangle(Pos.x1, Pos.y1, Pos.x1, Pos.y2);
	}
	#if 0 // def _DEBUG
	if (Ctx.Select &&
		Ctx.Select->Blk == this)
	{
		Ctx.pDC->Colour(GColour(255, 0, 0));
		Ctx.pDC->Rectangle(&Ctx.Select->Pos);
	}
	#endif
}
		
bool GRichTextPriv::TextBlock::OnLayout(Flow &flow)
{
	if (Pos.X() == flow.X() && !LayoutDirty)
	{
		// Adjust position to match the flow, even if we are not dirty
		Pos.Offset(0, flow.CurY - Pos.y1);
		flow.CurY = Pos.y2 + 1;
		return true;
	}

	LayoutDirty = false;
	Layout.DeleteObjects();
			
	flow.Left += Margin.x1;
	flow.Right -= Margin.x2;
	flow.CurY += Margin.y1;
			
	Pos.x1 = flow.Left;
	Pos.y1 = flow.CurY;
	Pos.x2 = flow.Right;
	Pos.y2 = flow.CurY-1; // Start with a 0px height.
			
	flow.Left += Border.x1 + Padding.x1;
	flow.Right -= Border.x2 + Padding.x2;
	flow.CurY += Border.y1 + Padding.y1;

	int FixX = 0; // Current x offset (fixed point) on the current line
	GAutoPtr<TextLine> CurLine(new TextLine(flow.Left - Pos.x1, flow.X(), flow.CurY - Pos.y1));
	if (!CurLine)
		return flow.d->Error(_FL, "alloc failed.");

	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
				
		if (t->Length() == 0)
			continue;

		int AvailableX = Pos.X() - CurLine->PosOff.x1;
		if (AvailableX < 0)
			AvailableX = 1;

		// Get the font for 't'
		GFont *f = flow.d->GetFont(t->GetStyle());
		if (!f)
			return flow.d->Error(_FL, "font creation failed.");
		
		uint32 *sStart = t->At(0);
		uint32 *sEnd = sStart + t->Length();
		for (unsigned Off = 0; Off < t->Length(); )
		{					
			// How much of 't' is on the same line?
			uint32 *s = sStart + Off;
			if (*s == '\n')
			{
				// New line handling...
				Off++;
				CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX) - 1;
				FixX = 0;
				CurLine->LayoutOffsets(f->GetHeight());
				Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
				CurLine->NewLine = 1;
						
				// LgiTrace("        [%i] = %s\n", Layout.Length(), CurLine->PosOff.GetStr());
						
				Layout.Add(CurLine.Release());
				CurLine.Reset(new TextLine(flow.Left - Pos.x1, flow.X(), Pos.Y()));

				if (Off == t->Length())
				{
					// Empty line at the end of the StyleText
					const uint32 Empty[] = {0};
					CurLine->Strs.Add(new DisplayStr(t, f, Empty, 0, flow.pDC));
				}
				continue;
			}

			uint32 *e = s;
			while (*e != '\n' && e < sEnd)
				e++;
					
			// Add 't' to current line
			int Chars = min(1024, (int) (e - s));
			GAutoPtr<DisplayStr> Ds
			(
				t->Emoji
				?
				new EmojiDisplayStr(t, d->GetEmojiImage(), f, s, Chars)
				:
				new DisplayStr(t, f, s, Chars, flow.pDC)
			);
			if (!Ds)
				return flow.d->Error(_FL, "display str creation failed.");

			if (FixedToInt(FixX) + Ds->X() > AvailableX)
			{
				// Wrap the string onto the line...
				int AvailablePx = AvailableX - FixedToInt(FixX);
				int FitChars = Ds->PosToIndex(AvailablePx, false);
				if (FitChars < 0)
				{
					flow.d->Error(_FL, "PosToIndex(%i) failed.", AvailablePx);
					LgiAssert(0);
				}
				else
				{
					// Wind back to the last break opportunity
					int ch = 0;
					for (ch = FitChars; ch > 0; ch--)
					{
						if (IsWordBreakChar(s[ch-1]))
							break;
					}
					if (ch == 0)
					{
						// One word minimum per line
						for (ch = 1; ch < Chars; ch++)
						{
							if (IsWordBreakChar(s[ch]))
								break;
						}
						Chars = ch;
					}						
					else if (ch > (FitChars >> 2))
						Chars = ch;
					else
						Chars = FitChars;
							
					// Create a new display string of the right size...
					if
					(
						!
						Ds.Reset
						(
							t->Emoji
							?
							new EmojiDisplayStr(t, d->GetEmojiImage(), f, s, Chars)
							:
							new DisplayStr(t, f, s, Chars, flow.pDC)
						)
					)
						return flow.d->Error(_FL, "failed to create wrapped display str.");
					
					// Finish off line
					CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX + Ds->FX()) - 1;
					CurLine->Strs.Add(Ds.Release());

					CurLine->LayoutOffsets(d->Font->GetHeight());
					Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
					Layout.Add(CurLine.Release());
					
					// New line...
					CurLine.Reset(new TextLine(flow.Left - Pos.x1, flow.X(), Pos.Y()));
					FixX = 0;
					Off += Chars;
					continue;
				}
			}
			else
			{
				FixX += Ds->FX();
			}
					
			if (!Ds)
				break;
					
			CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX) - 1;
			CurLine->Strs.Add(Ds.Release());
			Off += Chars;
		}
	}
	if (Txt.Length() == 0)
	{
		// Empty node case
		int y = Pos.y1 + flow.d->View->GetFont()->GetHeight() - 1;
		Pos.y2 = max(Pos.y2, y);
	}
			
	if (CurLine && CurLine->Strs.Length() > 0)
	{
		CurLine->LayoutOffsets(d->View->GetFont()->GetHeight());
		Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
		Layout.Add(CurLine.Release());
	}
			
	flow.CurY = Pos.y2 + 1 + Margin.y2 + Border.y2 + Padding.y2;
	flow.Left -= Margin.x1 + Border.x1 + Padding.x1;
	flow.Right += Margin.x2 + Border.x2 + Padding.x2;
			
	return true;
}
		
int GRichTextPriv::TextBlock::GetTextAt(uint32 Offset, GArray<StyleText*> &Out)
{
	if (Txt.Length() == 0)
		return 0;

	StyleText **t = &Txt[0];
	StyleText **e = t + Txt.Length();
	Out.Length(0);

	uint32 Pos = 0;
	while (t < e)
	{
		uint32 Len = (*t)->Length();
		if (Offset >= Pos && Offset <= Pos + Len)
			Out.Add(*t);

		t++;
		Pos += Len;
	}

	LgiAssert(Pos == Len);
	return Out.Length();
}

bool GRichTextPriv::TextBlock::IsValid()
{
	int TxtLen = 0;
	for (unsigned i = 0; i < Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		TxtLen += t->Length();
		for (unsigned n = 0; n < t->Length(); n++)
		{
			if ((*t)[n] == 0)
			{
				LgiAssert(0);
				return false;
			}
		}
	}
	
	if (Len != TxtLen)
		return d->Error(_FL, "Txt.Len vs Len mismatch: %i, %i.", TxtLen, Len);

	return true;
}

int GRichTextPriv::TextBlock::GetLines()
{
	return Layout.Length();
}

bool GRichTextPriv::TextBlock::OffsetToLine(int Offset, int *ColX, GArray<int> *LineY)
{
	if (LayoutDirty)
		return false;

	if (LineY)
		LineY->Length(0);
	if (Offset <= 0)
	{
		if (ColX) *ColX = 0;
		if (LineY) LineY->Add(0);
		return true;
	}

	bool Found = false;
	int Pos = 0;

	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		int Len = tl->Length();

		if (Offset >= Pos && Offset <= Pos + Len - tl->NewLine)
		{
			if (ColX) *ColX = Offset - Pos;
			if (LineY) LineY->Add(i);
			Found = true;
		}
		
		Pos += Len;
	}

	return Found;
}

int GRichTextPriv::TextBlock::LineToOffset(int Line)
{
	if (LayoutDirty)
		return -1;
	
	if (Line <= 0)
		return 0;

	int Pos = 0;
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		int Len = tl->Length();
		if (i == Line)
			return Pos;
		Pos = Len;
	}

	return Length();
}


bool GRichTextPriv::TextBlock::PreEdit(Transaction *Trans)
{
	if (Trans)
	{
		bool HasThisBlock = false;
		for (unsigned i=0; i<Trans->Changes.Length(); i++)
		{
			CompleteTextBlockState *c = dynamic_cast<CompleteTextBlockState*>(Trans->Changes[i]);
			if (c)
			{
				if (c->Uid == BlockUid)
				{
					HasThisBlock = true;
					break;
				}
			}
		}

		if (!HasThisBlock)
			Trans->Add(new CompleteTextBlockState(d, this));
	}

	return true;
}

int GRichTextPriv::TextBlock::DeleteAt(Transaction *Trans, int BlkOffset, int Chars, GArray<uint32> *DeletedText)
{
	int Pos = 0;
	int Deleted = 0;

	PreEdit(Trans);

	for (unsigned i=0; i<Txt.Length() && Chars > 0; i++)
	{
		StyleText *t = Txt[i];
		int TxtOffset = BlkOffset - Pos;
		if (TxtOffset >= 0 && TxtOffset < (int)t->Length())
		{
			int MaxChars = t->Length() - TxtOffset;
			int Remove = min(Chars, MaxChars);
			if (Remove <= 0)
				return 0;
			int Remaining = MaxChars - Remove;
			int NewLen = t->Length() - Remove;
			
			if (DeletedText)
			{
				DeletedText->Add(t->At(TxtOffset), Remove);
			}
			if (Remaining > 0)
			{
				// Copy down
				memmove(&(*t)[TxtOffset], &(*t)[TxtOffset + Remove], Remaining * sizeof(uint32));
				(*t)[NewLen] = 0;
			}

			// Change length
			if (NewLen == 0)
			{
				// Remove run completely
				// LgiTrace("DelRun %p/%i '%.*S'\n", t, i, t->Length(), &(*t)[0]);
				Txt.DeleteAt(i--, true);
				DeleteObj(t);
			}
			else
			{
				// Shorten run
				t->Length(NewLen);
				// LgiTrace("ShortenRun %p/%i '%.*S'\n", t, i, t->Length(), &(*t)[0]);
			}

			LayoutDirty = true;
			Chars -= Remove;
			Len -= Remove;
			Deleted += Remove;
		}

		if (t)
			Pos += t->Length();
	}

	if (Deleted > 0)
	{
		LayoutDirty = true;
		UpdateSpelling();
	}

	IsValid();

	return Deleted;
}
		
GMessage::Result GRichTextPriv::TextBlock::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_COMMAND:
		{
			GSpellCheck::SpellingError *e = SpellingErrors.AddressOf(ClickErrIdx);
			if (e)
			{
				// Replacing text with spell check suggestion:
				int i = Msg->A() - SPELLING_BASE;
				if (i >= 0 && i < (int)e->Suggestions.Length())
				{
					GString s = e->Suggestions[i];
					AutoTrans t(new GRichTextPriv::Transaction);
					
					// Delete the old text...
					DeleteAt(t, e->Start, e->Len);

					// Insert the new text....
					GAutoPtr<uint32,true> u((uint32*)LgiNewConvertCp("utf-32", s, "utf-8"));
					AddText(t, e->Start, u.Get(), Strlen(u.Get()));
					
					d->AddTrans(t);
				}
			}
			break;
		}
	}

	return 0;
}

bool GRichTextPriv::TextBlock::AddText(Transaction *Trans, int AtOffset, const uint32 *InStr, int InChars, GNamedStyle *Style)
{
	if (!InStr)
		return d->Error(_FL, "No input text.");
	if (InChars < 0)
		InChars = Strlen(InStr);

	PreEdit(Trans);
	
	GArray<int> EmojiIdx;
	EmojiIdx.Length(InChars);
	for (int i=0; i<InChars; i++)
	{
		EmojiIdx[i] = EmojiToIconIndex(InStr + i, InChars - i);
	}

	// const uint32 *End = InStr + InChars;
	int Chars = 0; // Length of run to insert
	int Pos = 0; // Current position in this block
	uint32 TxtIdx = 0; // Index into Txt array
	
	for (int i = 0; i < InChars; i += Chars)
	{
		// Work out the run of chars that are either
		// emoji or not emoji...
		bool IsEmoji = EmojiIdx[i] >= 0;
		Chars = 1;
		for (int n = i + 1; n < InChars; n++)
		{
			if ( IsEmoji ^ (EmojiIdx[n] >= 0) )
				break;
			Chars++;
		}
		
		// Now process 'Char' chars
		const uint32 *Str = InStr + i;

		if (AtOffset >= 0 && Txt.Length() > 0)
		{
			// Seek further into block?
			while (	Pos < AtOffset &&
					TxtIdx < Txt.Length())
			{
				StyleText *t = Txt[TxtIdx];
				int Len = t->Length();
				if (AtOffset <= Pos + Len)
					break;
				Pos += Len;
				TxtIdx++;
			}
			
			StyleText *t = TxtIdx >= Txt.Length() ? Txt.Last() : Txt[TxtIdx];
			int TxtLen = t->Length();			
			if (AtOffset >= Pos && AtOffset <= Pos + TxtLen)
			{
				int StyleOffset = AtOffset - Pos;
				if (!Style && IsEmoji == t->Emoji)
				{
					// Insert/append to existing text run
					int After = t->Length() - StyleOffset;
					int NewSz = t->Length() + Chars;
					t->Length(NewSz);
					uint32 *c = &t->First();

					LOG_FN("TextBlock(%i)::Add(%i,%i,%s)::Append StyleOffset=%i, After=%i\n", GetUid(), AtOffset, InChars, Style?Style->Name.Get():NULL, StyleOffset, After);

					// Do we need to move characters up to make space?
					if (After > 0)
						memmove(c + StyleOffset + Chars, c + StyleOffset, After * sizeof(*c));

					// Insert the new string...
					memcpy(c + StyleOffset, Str, Chars * sizeof(*c));
					Len += Chars;
					AtOffset += Chars;
				}
				else
				{
					// Break into 2 runs, with the new text in the middle...

					// Insert the new text+style
					StyleText *Run = new StyleText(Str, Chars, Style);
					if (!Run)
						return false;
					Run->Emoji = IsEmoji;
					Pos += StyleOffset; // We are skipping over the run at 'TxtIdx', update pos
					Txt.AddAt(++TxtIdx, Run);

					LOG_FN("TextBlock(%i)::Add(%i,%i,%s)::Insert StyleOffset=%i\n", GetUid(), AtOffset, InChars, Style?Style->Name.Get():NULL, StyleOffset);

					if (StyleOffset < TxtLen)
					{
						// Insert the 2nd part of the string
						Run = new StyleText(t->At(StyleOffset), TxtLen - StyleOffset, t->GetStyle());
						if (!Run) return false;
						Pos += Chars;
						Txt.AddAt(++TxtIdx, Run);

						// Now truncate the existing text..
						t->Length(StyleOffset);
					}
					
					Len += Chars;
					AtOffset += Chars;
				}

				Str = NULL;
			}
		}

		if (Str)
		{
			// At the end
			StyleText *Last = Txt.Length() > 0 ? Txt.Last() : NULL;
			if (Last &&
				Last->GetStyle() == Style &&
				IsEmoji == Last->Emoji)
			{
				if (Last->Add((uint32*)Str, Chars))
				{
					Len += Chars;
					if (AtOffset >= 0)
						AtOffset += Chars;
				}
			}
			else
			{			
				StyleText *Run = new StyleText(Str, Chars, Style);
				if (!Run)
					return false;
				Run->Emoji = IsEmoji;
				Txt.Add(Run);
				Len += Chars;
				if (AtOffset >= 0)
					AtOffset += Chars;
			}
		}
	}

	LayoutDirty = true;
	
	IsValid();

	UpdateSpelling();
	
	return true;
}

void GRichTextPriv::TextBlock::UpdateSpelling()
{
	if (d->SpellCheck &&
		d->SpellDictionaryLoaded)
	{
		GArray<uint32> Text;
		if (CopyAt(0, Length(), &Text))
		{
			GString s(&Text[0], Text.Length());
			d->SpellCheck->Check(d->View->AddDispatch(), s, 0, (GRichTextPriv::Block*)this);
		}
	}
}

int ErrSort(GSpellCheck::SpellingError *a, GSpellCheck::SpellingError *b)
{
	return a->Start - b->Start;
}

bool GRichTextPriv::TextBlock::DoContext(GSubMenu &s, GdcPt2 Doc, int Offset, bool Spelling)
{
	if (Spelling)
	{
		// Is there a spelling error at 'Offset'?		
		for (unsigned i=0; i<SpellingErrors.Length(); i++)
		{
			GSpellCheck::SpellingError &e = SpellingErrors[i];
			if (Offset >= e.Start && Offset < e.End())
			{
				ClickErrIdx = i;
				if (e.Suggestions.Length())
				{
					GSubMenu *Sp = s.AppendSub("Spelling");
					if (Sp)
					{
						s.AppendSeparator();
						for (unsigned n=0; n<e.Suggestions.Length(); n++)
							Sp->AppendItem(e.Suggestions[n], SPELLING_BASE+n);
					}
				}
				break;
			}
		}
	}

	return true;
}

void GRichTextPriv::TextBlock::SetSpellingErrors(GArray<GSpellCheck::SpellingError> &Errors)
{
	SpellingErrors = Errors;	
	SpellingErrors.Sort(ErrSort);
}

int GRichTextPriv::TextBlock::CopyAt(int Offset, int Chars, GArray<uint32> *Text)
{
	if (!Text)
		return 0;
	if (Chars < 0)
		Chars = Length() - Offset;

	int Pos = 0;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		if (Offset >= Pos && Offset < Pos + (int)t->Length())
		{
			int Skip = Offset - Pos;
			int Remain = t->Length() - Skip;
			int Cp = min(Chars, Remain);
			Text->Add(&(*t)[Skip], Cp);
			Chars -= Cp;
			Offset += Cp;
		}

		Pos += t->Length();
	}

	return Text->Length();
}

int GRichTextPriv::TextBlock::FindAt(int StartIdx, const uint32 *Str, GFindReplaceCommon *Params)
{
	if (!Str || !Params)
		return -1;

	int InLen = Strlen(Str);
	bool Match;
	int CharPos = 0;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		uint32 *s = &t->First();
		uint32 *e = s + t->Length();
		if (Params->MatchCase)
		{
			for (uint32 *c = s; c < e; c++)
			{
				if (*c == *Str)
				{
					if (c + InLen <= e)
						Match = !Strncmp(c, Str, InLen);
					else
					{
						GArray<uint32> tmp;
						if (CopyAt(CharPos + (c - s), InLen, &tmp) &&
							tmp.Length() == InLen)
							Match = !Strncmp(&tmp[0], Str, InLen);
						else
							Match = false;
					}
					if (Match)
						return CharPos + (c - s);
				}
			}
		}
		else
		{
			uint32 l = ToLower(*Str);
			for (uint32 *c = s; c < e; c++)
			{
				if (ToLower(*c) == l)
				{
					if (c + InLen <= e)
						Match = !Strnicmp(c, Str, InLen);
					else
					{
						GArray<uint32> tmp;
						if (CopyAt(CharPos + (c - s), InLen, &tmp) &&
							tmp.Length() == InLen)
							Match = !Strnicmp(&tmp[0], Str, InLen);
						else
							Match = false;
					}
					if (Match)
						return CharPos + (c - s);
				}
			}
		}

		CharPos += t->Length();
	}

	return -1;
}

bool GRichTextPriv::TextBlock::DoCase(Transaction *Trans, int StartIdx, int Chars, bool Upper)
{
	Range Blk(0, Len);
	Range Inp(StartIdx, Chars < 0 ? Len - StartIdx : Chars);
	Range Change = Blk.Overlap(Inp);

	PreEdit(Trans);

	Range Run(0, 0);
	bool Changed = false;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *st = Txt[i];
		Run.Len = st->Length();
		Range Edit = Run.Overlap(Change);
		if (Edit.Len > 0)
		{
			uint32 *s = st->At(Edit.Start - Run.Start);
			for (int n=0; n<Edit.Len; n++)
			{
				if (Upper)
				{
					if (s[n] >= 'a' && s[n] <= 'z')
						s[n] = s[n] - 'a' + 'A';
				}
				else
				{
					if (s[n] >= 'A' && s[n] <= 'Z')
						s[n] = s[n] - 'A' + 'a';
				}
			}
			Changed = true;
		}

		Run.Start += Run.Len;
	}

	LayoutDirty |= Changed;

	return Changed;
}

GRichTextPriv::Block *GRichTextPriv::TextBlock::Split(Transaction *Trans, int AtOffset)
{
	if (AtOffset < 0 || 
		AtOffset >= Len)
		return NULL;

	GRichTextPriv::TextBlock *After = new GRichTextPriv::TextBlock(d);
	if (!After)
	{
		d->Error(_FL, "Alloc Err");
		return NULL;
	}

	After->SetStyle(GetStyle());
	
	int Pos = 0;
	unsigned i;
	for (i=0; i<Txt.Length(); i++)
	{
		StyleText *St = Txt[i];
		int StLen = St->Length();
		if (AtOffset >= Pos && AtOffset < Pos + StLen)
		{
			int StOff = AtOffset - Pos;
			if (StOff > 0)
			{
				// Split the text into 2 blocks...
				uint32 *t = St->At(StOff);
				int remaining = St->Length() - StOff;
				StyleText *AfterText = new StyleText(t, remaining, St->GetStyle());
				if (!AfterText)
				{
					d->Error(_FL, "Alloc Err");
					return NULL;
				}
				St->Length(StOff);
				i++;
				Len = Pos + StOff;

				After->Txt.Add(AfterText);
				After->Len += AfterText->Length();
			}
			break;
		}

		Pos += StLen;
	}

	while (i < Txt.Length())
	{
		StyleText *St = Txt[i];
		Txt.DeleteAt(i, true);
		After->Txt.Add(St);
		After->Len += St->Length();
	}

	LayoutDirty = true;
	After->LayoutDirty = true;

	return After;
}

void GRichTextPriv::TextBlock::IncAllStyleRefs()
{
	if (Style)
		Style->RefCount++;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		GNamedStyle *s = Txt[i]->GetStyle();
		if (s) s->RefCount++;
	}
}

bool GRichTextPriv::TextBlock::ChangeStyle(Transaction *Trans, int Offset, int Chars, GCss *Style, bool Add)
{
	if (!Style)
		return d->Error(_FL, "No style.");

	if (Offset < 0 || Offset >= Len)
		return true;
	if (Chars < 0)
		Chars = Len;

	if (Trans)
		Trans->Add(new CompleteTextBlockState(d, this));

	int CharPos = 0;
	int RestyleEnd = Offset + Chars;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		int Len = t->Length();
		int End = CharPos + Len;

		if (End <= Offset || CharPos > RestyleEnd)
			;
		else
		{
			int Before = Offset >= CharPos ? Offset - CharPos : 0;
			LgiAssert(Before >= 0);
			int After = RestyleEnd < End ? End - RestyleEnd : 0; 
			LgiAssert(After >= 0);
			int Inside = Len - Before - After;
			LgiAssert(Inside >= 0);


			GAutoPtr<GCss> TmpStyle(new GCss);
			if (Add)
			{
				if (t->GetStyle())
					*TmpStyle = *t->GetStyle();
				*TmpStyle += *Style;
			}
			else if (Style->Length() != 0)
			{
				if (t->GetStyle())
					*TmpStyle = *t->GetStyle();
				*TmpStyle -= *Style;
			}

			GNamedStyle *CacheStyle = TmpStyle && TmpStyle->Length() ? d->AddStyleToCache(TmpStyle) : NULL;


			if (Before && After)
			{
				// Split into 3 parts:
				// |---before----|###restyled###|---after---|
				StyleText *st = new StyleText(t->At(Before), Inside, CacheStyle);
				if (st)
					Txt.AddAt(++i, st);
				
				st = new StyleText(t->At(Before + Inside), After, t->GetStyle());
				if (st)
					Txt.AddAt(++i, st);
				
				t->Length(Before);
				LayoutDirty = true;
				return true;
			}
			else if (Before)
			{
				// Split into 2 parts:
				// |---before----|###restyled###|
				StyleText *st = new StyleText(t->At(Before), Inside, CacheStyle);
				if (st)
					Txt.AddAt(++i, st);
				
				t->Length(Before);
				LayoutDirty = true;
			}
			else if (After)
			{
				// Split into 2 parts:
				// |###restyled###|---after---|
				StyleText *st = new StyleText(t->At(0), Inside, CacheStyle);
				if (st)
					Txt.AddAt(i, st);
				
				memmove(t->At(0), t->At(Inside), After*sizeof(uint32));
				t->Length(After);
				LayoutDirty = true;
			}
			else if (Inside)
			{
				// Re-style the whole run
				t->SetStyle(CacheStyle);
				LayoutDirty = true;
			}
		}

		CharPos += Len;
	}

	return true;
}

bool GRichTextPriv::TextBlock::Seek(SeekType To, BlockCursor &Cur)
{
	int XOffset = Cur.Pos.x1 - Pos.x1;
	int CharPos = 0;
	GArray<int> LineOffset;
	GArray<int> LineLen;
	int CurLine = -1;
	int CurLineScore = 0;
	
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *Line = Layout[i];
		PtrCheckBreak(Line);
		int Len = Line->Length();				
				
		LineOffset[i] = CharPos;
		LineLen[i] = Len;
				
		if (Cur.Offset >= CharPos &&
			Cur.Offset <= CharPos + Len - Line->NewLine)	// Minus 'NewLine' is because the cursor can't be
															// after the '\n' on a line. It's actually on the 
															// next line.
		{
			int Score = 1;
			if (Cur.LineHint >= 0 && i == Cur.LineHint)
				Score++;
			if (Score > CurLineScore)
			{
				CurLine = i;
				CurLineScore = Score;
			}
		}				
				
		CharPos += Len;
	}
			
	if (CurLine < 0)
	{
		CharPos = 0;
		d->Log->Print("TextBlock(%i)::Seek, lines=%i\n", GetUid(), Layout.Length());
		for (unsigned i=0; i<Layout.Length(); i++)
		{
			TextLine *Line = Layout[i];
			if (Line) { d->Log->Print("\tLine[%i] @ %i+%i=%i\n", i, CharPos, Line->Length(), CharPos + Line->Length()); CharPos += Line->Length(); }
			else      { d->Log->Print("\tLine[%i] @ %i, is NULL\n", i, CharPos); break; }
		}
		return d->Error(_FL, "Index '%i' not in layout lines.", Cur.Offset);
	}
				
	TextLine *Line = NULL;
	switch (To)
	{
		case SkLineStart:
		{
			Cur.Offset = LineOffset[CurLine];
			Cur.LineHint = CurLine;
			return true;
		}
		case SkLineEnd:
		{
			Cur.Offset = LineOffset[CurLine] +
						LineLen[CurLine] -
						Layout[CurLine]->NewLine;
			Cur.LineHint = CurLine;
			return true;
		}
		case SkUpLine:
		{
			// Get previous line...
			if (CurLine == 0)
				return false;
			Line = Layout[--CurLine];
			if (!Line)
				return d->Error(_FL, "No line at %i.", CurLine);
			break;
		}				
		case SkDownLine:
		{
			// Get next line...
			if (CurLine >= (int)Layout.Length() - 1)
				return false;
			Line = Layout[++CurLine];
			if (!Line)
				return d->Error(_FL, "No line at %i.", CurLine);
			break;
		}
		default:
		{
			return false;
			break;
		}
	}
			
	if (Line)
	{
		// Work out where the cursor should be based on the 'XOffset'
		if (Line->Strs.Length() > 0)
		{
			int FixX = 0;
			int CharOffset = 0;
			for (unsigned i=0; i<Line->Strs.Length(); i++)
			{
				DisplayStr *Ds = Line->Strs[i];
				PtrCheckBreak(Ds);
						
				if (XOffset >= FixedToInt(FixX) &&
					XOffset <= FixedToInt(FixX + Ds->FX()))
				{
					// This is the matching string...
					int Px = XOffset - FixedToInt(FixX) - Line->PosOff.x1;
					int Char = Ds->PosToIndex(Px, true);
					if (Char >= 0)
					{
						Cur.Offset = LineOffset[CurLine] +	// Character offset of line
									CharOffset +			// Character offset of current string
									Char;					// Offset into current string for 'XOffset'
						Cur.LineHint = CurLine;
						return true;
					}
				}
						
				FixX += Ds->FX();
				CharOffset += Ds->Length();
			}
					
			// Cursor is nearest the end of the string...?
			Cur.Offset = LineOffset[CurLine] + Line->Length() - Line->NewLine;
			Cur.LineHint = CurLine;
			return true;
		}
		else if (Line->NewLine)
		{
			Cur.Offset = LineOffset[CurLine];
			Cur.LineHint = CurLine;
			return true;
		}
	}
			
	return false;
}
	
#ifdef _DEBUG
void GRichTextPriv::TextBlock::DumpNodes(GTreeItem *Ti)
{
	GString s;
	s.Printf("TextBlock style=%s", Style?Style->Name.Get():NULL);
	Ti->SetText(s);

	GTreeItem *TxtRoot = PrintNode(Ti, "Txt(%i)", Txt.Length());
	if (TxtRoot)
	{
		int Pos = 0;
		for (unsigned i=0; i<Txt.Length(); i++)
		{
			StyleText *St = Txt[i];
			int Len = St->Length();
			GString u;
			if (Len)
			{
				GStringPipe p(256);
				uint32 *Str = St->At(0);
				p.Write("\'", 1);
				for (int k=0; k<min(Len, 30); k++)
				{
					if (Str[k] == '\n')
						p.Write("\\n", 2);
					else if (Str[k] >= 0x10000)
						p.Print("&#%i;", Str[k]);
					else
					{
						uint8 utf8[6], *n = utf8;
						int utf8len = sizeof(utf8);
						if (LgiUtf32To8(Str[k], n, utf8len))
							p.Write(utf8, sizeof(utf8)-utf8len);
					}
				}
				p.Write("\'", 1);
				u = p.NewGStr();
			}
			else u = "(Empty)";

			PrintNode(	TxtRoot, "[%i] range=%i-%i, len=%i, style=%s, %s",
						i,
						Pos, Pos + Len - 1,
						Len,
						St->GetStyle() ? St->GetStyle()->Name.Get() : NULL,
						u.Get());
			Pos += Len;
		}
	}

	GTreeItem *LayoutRoot = PrintNode(Ti, "Layout(%i)", Layout.Length());
	if (LayoutRoot)
	{
		for (unsigned i=0; i<Layout.Length(); i++)
		{
			TextLine *Tl = Layout[i];
			GTreeItem *Elem = PrintNode(LayoutRoot, "[%i] len=%i", i, Tl->Length());
			for (unsigned n=0; n<Tl->Strs.Length(); n++)
			{
				DisplayStr *Ds = Tl->Strs[n];
				GNamedStyle *Style = Ds->Src ? Ds->Src->GetStyle() : NULL;
				PrintNode(	Elem, "[%i] style=%s len=%i txt='%.20S'",
							n,
							Style ? Style->Name.Get() : NULL,
							Ds->Length(),
							(const char16*) (*Ds));
			}
		}
	}
}
#endif


