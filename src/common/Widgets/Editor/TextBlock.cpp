#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"

GRichTextPriv::TextBlock::TextBlock(GRichTextPriv *priv) : Block(priv)
{
	LayoutDirty = false;
	Len = 0;
	Pos.ZOff(-1, -1);
	Style = NULL;
	Fnt = NULL;
			
	Margin.ZOff(0, 0);
	Border.ZOff(0, 0);
	Padding.ZOff(0, 0);
}
		
GRichTextPriv::TextBlock::~TextBlock()
{
	LgiAssert(Cursors == 0);
	Txt.DeleteObjects();
}

void GRichTextPriv::TextBlock::Dump()
{
	LgiTrace("    margin=%s, border=%s, padding=%s\n",
				Margin.GetStr(),
				Border.GetStr(),
				Padding.GetStr());
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		GString s(t->Length() ? &t->First() : NULL);
		s = s.Strip();
				
		/*
		LgiTrace("    %p: style=%p/%s, txt(%i)=%s\n",
			t,
			t->GetStyle(),
			t->GetStyle() ? t->GetStyle()->Name.Get() : NULL,
			t->Length(),
			s.Get());
		*/
	}
}
		
GNamedStyle *GRichTextPriv::TextBlock::GetStyle()
{
	return Style;
}
		
void GRichTextPriv::TextBlock::SetStyle(GNamedStyle *s)
{
	if ((Style = s))
	{
		Fnt = d->GetFont(s);
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

bool GRichTextPriv::TextBlock::ToHtml(GStream &s)
{
	s.Print("<p>");
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		GNamedStyle *style = t->GetStyle();
		
		GString utf(t->At(0), t->Length());
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
		if (ElemName)
			s.Print(">");
		
		// Encode entities...
		GUtf8Ptr last(str);
		GUtf8Ptr cur(str);
		GUtf8Ptr end(str + utf.Length());
		while (cur < end)
		{
			char16 ch = cur;
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
	{
		LgiAssert(0);
		return false;
	}

	if (LayoutDirty)
	{
		Cursor->Pos.ZOff(-1, -1);
		LgiTrace("%s:%i - Can't get pos from index, layout is dirty...\n", _FL);
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
			int dsChars = ds->Length();
					
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
					Cursor->Pos.x1 = r.x1 + IntToFixed(FixX);
				}
				else if (CharOffset == dsChars)
				{
					// Last char
					Cursor->Pos.x1 = r.x1 + IntToFixed(FixX + ds->FX());
				}
				else
				{
					// In the middle somewhere...
					GDisplayString Tmp(ds->GetFont(), *ds, CharOffset);
					Cursor->Pos.x1 = r.x1 + IntToFixed(FixX + Tmp.FX());
				}

				Cursor->Pos.y1 = r.y1 + ds->OffsetY;
				Cursor->Pos.y2 = Cursor->Pos.y1 + ds->Y() - 1;
				Cursor->Pos.x2 = Cursor->Pos.x1 + 1;

				Cursor->Line.Set(Pos.x1, r.y1, Pos.x2, r.y2);
				return true;
			}					
					
			FixX += ds->FX();
			CharPos += ds->Length();
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
				int OffChar = ds->CharAt(OffPx);

				// d->DebugRects[0].Set(Pos.x1, r.y1, Pos.x1 + InputX+1, r.y2);
						
				htr.Blk = this;
				htr.Ds = ds;
				htr.Idx = CharPos + OffChar;
				htr.LineHint = i;
				return true;
			}
					
			FixX += ds->FX();

			CharPos += ds->Length();
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
			GFont *f = Ds->GetFont();
			ColourPair &Cols = Ds->Src->Colours;
			if (f != Fnt)
			{
				f->Transparent(false);
				Fnt = f;
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
				StyleText *CurStyle = GetTextAt(Ctx.Cursor->Offset);
				if (Ds->Src == CurStyle)
				{
					r.ZOff(Ds->X()-1, Ds->Y()-1);
					r.Offset(FixedToInt(FixX), FixedToInt(FixY));
				}
			}
			#endif

			if (CurEndPoint < EndPoints &&
				EndPoint[CurEndPoint] >= CharPos &&
				EndPoint[CurEndPoint] <= CharPos + Ds->Length())
			{
				// Process string into parts based on the selection boundaries
				const char16 *s = *(GDisplayString*)Ds;
				int Ch = EndPoint[CurEndPoint] - CharPos;
				GDisplayString ds1(f, s, Ch);
						
				// First part...
				f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
							Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
				ds1.FDraw(Ctx.pDC, FixX, FixY);
				FixX += ds1.FX();
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
					EndPoint[CurEndPoint] < CharPos + Ds->Length())
				{
					// Yes..
					int Ch2 = EndPoint[CurEndPoint] - CharPos;

					// Part 2
					GDisplayString ds2(f, s + Ch, Ch2 - Ch);
					f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
								Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
					ds2.FDraw(Ctx.pDC, FixX, FixY);
					FixX += ds2.FX();
					Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
					CurEndPoint++;

					// Part 3
					GDisplayString ds3(f, s + Ch2);
					f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
								Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
					ds3.FDraw(Ctx.pDC, FixX, FixY);
					FixX += ds3.FX();
				}
				else
				{
					// No... draw 2nd part
					GDisplayString ds2(f, s + Ch);
					f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
								Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
					ds2.FDraw(Ctx.pDC, FixX, FixY);
					FixX += ds2.FX();
				}
			}
			else
			{
				// No selection changes... draw the whole string
				f->Colour(	Ctx.Type == Unselected && Cols.Fore.IsValid() ? Cols.Fore : Ctx.Fore(),
							Ctx.Type == Unselected && Cols.Back.IsValid() ? Cols.Back : Ctx.Back());
						
				Ds->FDraw(Ctx.pDC, FixX, FixY);

				#if DEBUG_OUTLINE_CUR_DISPLAY_STR
				if (Ctx.Cursor->Blk == (Block*)this &&
					Ctx.Cursor->Offset >= CharPos &&
					Ctx.Cursor->Offset < CharPos + Ds->Length())
				{
					GRect r(0, 0, Ds->X()-1, Ds->Y()-1);
					r.Offset(FixedToInt(FixX), FixedToInt(FixY));
					Ctx.pDC->Colour(GColour::Red);
					Ctx.pDC->Box(&r);
				}
				#endif

				FixX += Ds->FX();
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

			CharPos += Ds->Length();
		}

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
		LgiAssert(AvailableX > 0);

		// Get the font for 't'
		GFont *f = flow.d->GetFont(t->GetStyle());
		if (!f)
			return flow.d->Error(_FL, "font creation failed.");
		
		char16 *sStart = &(*t)[0];
		char16 *sEnd = sStart + t->Length();
		for (unsigned Off = 0; Off < t->Length(); )
		{					
			// How much of 't' is on the same line?
			char16 *s = sStart + Off;
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
					CurLine->Strs.Add(new DisplayStr(t, f, L"", 0, flow.pDC));
				}
				continue;
			}

			char16 *e = s;
			while (*e != '\n' && e < sEnd)
				e++;
					
			// Add 't' to current line
			int Chars = min(1024, (int) (e - s));
			GAutoPtr<DisplayStr> Ds(new DisplayStr(t, f, s, Chars, flow.pDC));
			if (!Ds)
				return flow.d->Error(_FL, "display str creation failed.");

			if (FixedToInt(FixX) + Ds->X() > AvailableX)
			{
				// Wrap the string onto the line...
				int AvailablePx = AvailableX - FixedToInt(FixX);
				int FitChars = Ds->CharAt(AvailablePx);
				if (FitChars < 0)
				{
					flow.d->Error(_FL, "CharAt(%i) failed.", AvailablePx);
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
					if (ch > (FitChars >> 2))
						Chars = ch;
					else
						Chars = FitChars;
							
					// Create a new display string of the right size...
					if (!Ds.Reset(new DisplayStr(t, f, s, Chars, flow.pDC)))
						return flow.d->Error(_FL, "failed to create wrapped display str.");
					
					// Finish off line
					CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX + Ds->FX()) - 1;
					CurLine->Strs.Add(Ds.Release());

					CurLine->LayoutOffsets(CurLine->Strs.Last()->GetFont()->GetHeight());
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
		CurLine->LayoutOffsets(CurLine->Strs.Last()->GetFont()->GetHeight());
		Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
		Layout.Add(CurLine.Release());
	}
			
	flow.CurY = Pos.y2 + 1 + Margin.y2 + Border.y2 + Padding.y2;
	flow.Left -= Margin.x1 + Border.x1 + Padding.x1;
	flow.Right += Margin.x2 + Border.x2 + Padding.x2;
			
	return true;
}
		
GRichTextPriv::StyleText *GRichTextPriv::TextBlock::GetTextAt(uint32 Offset)
{
	if (Txt.Length() == 0)
		return NULL;

	StyleText **t = &Txt[0];
	StyleText **e = t + Txt.Length();

	int TotalLen = 0;

	while (Offset >= 0 && t < e)
	{
		uint32 Len = (*t)->Length();
		if (Offset < Len)
			return *t;
		Offset -= Len;
		TotalLen += Len;

		t++;
	}

	LgiAssert(TotalLen == Len);
	return NULL;
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
	LgiAssert(Len == TxtLen);

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

	if (Offset <= 0)
	{
		if (ColX) *ColX = 0;
		if (LineY) *LineY = 0;
		return true;
	}

	bool Found = false;
	int Pos = 0;

	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		int Len = tl->Length();

		if (Offset >= Pos && Offset <= Pos + Len)
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

int GRichTextPriv::TextBlock::DeleteAt(int BlkOffset, int Chars, GArray<char16> *DeletedText)
{
	int Pos = 0;
	int Deleted = 0;

	#if 0
	for (unsigned i=0; i<Txt.Length(); i++)
		LgiTrace("%p/%i: '%.*S'\n", Txt[i], i, Txt[i]->Length(), &(*Txt[i])[0]);
	#endif
	
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
				DeletedText->Add(&(*t)[TxtOffset], Remove);
			}
			if (Remaining > 0)
			{
				// Copy down
				memmove(&(*t)[TxtOffset], &(*t)[TxtOffset + Remove], Remaining * sizeof(char16));
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
		LayoutDirty = true;

	IsValid();

	return Deleted;
}
		
bool GRichTextPriv::TextBlock::AddText(int AtOffset, const char16 *Str, int Chars, GNamedStyle *Style)
{
	if (!Str)
		return false;
	if (Chars < 0)
		Chars = StrlenW(Str);
	
	if (AtOffset >= 0)
	{
		for (unsigned i=0; i<Txt.Length(); i++)
		{
			StyleText *t = Txt[i];
			if (AtOffset <= (int)t->Length())
			{
				if (!Style)
				{
					// Add to existing text run
					int After = t->Length() - AtOffset;
					int NewSz = t->Length() + Chars;
					t->Length(NewSz);
					char16 *c = &t->First();
					if (After > 0)
						memmove(c + AtOffset + Chars, c + AtOffset, After * sizeof(*c));
					memcpy(c + AtOffset, Str, Chars * sizeof(*c));
				}
				else
				{
					// Break into 2 runs, with the new text in the middle...

					// Insert the new text+style
					StyleText *Run = new StyleText(Str, Chars, Style);
					if (!Run) return false;
					Txt.AddAt(++i, Run);

					if (AtOffset < (int)t->Length())
					{
						// Insert the 2nd part of the string
						Run = new StyleText(&(*t)[AtOffset], t->Length() - AtOffset, t->GetStyle());
						if (!Run) return false;
						Txt.AddAt(++i, Run);

						// Now truncate the existing text..
						t->Length(AtOffset);
					}
				}

				Str = NULL;
				break;
			}

			AtOffset -= t->Length();
		}
	}

	if (Str)
	{
		StyleText *Run = new StyleText(Str, Chars, Style);
		if (!Run) return false;
		Txt.Add(Run);
	}

	Len += Chars;
	LayoutDirty = true;
	
	IsValid();
	
	return true;
}

int GRichTextPriv::TextBlock::CopyAt(int Offset, int Chars, GArray<char16> *Text)
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

int GRichTextPriv::TextBlock::FindAt(int StartIdx, const char16 *Str, GFindReplaceCommon *Params)
{
	if (!Str || !Params)
		return -1;

	int InLen = Strlen(Str);
	bool Match;
	int CharPos = 0;
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		char16 *s = &t->First();
		char16 *e = s + t->Length();
		if (Params->MatchCase)
		{
			for (char16 *c = s; c < e; c++)
			{
				if (*c == *Str)
				{
					if (c + InLen <= e)
						Match = !Strncmp(c, Str, InLen);
					else
					{
						GArray<char16> tmp;
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
			char16 l = ToLower(*Str);
			for (char16 *c = s; c < e; c++)
			{
				if (ToLower(*c) == l)
				{
					if (c + InLen <= e)
						Match = !Strnicmp(c, Str, InLen);
					else
					{
						GArray<char16> tmp;
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

bool GRichTextPriv::TextBlock::ChangeStyle(int Offset, int Chars, GCss *Style, bool Add)
{
	if (!Style)
	{
		LgiAssert(0);
		return false;
	}

	if (Offset < 0 || Offset >= Len)
		return true;
	if (Chars < 0)
		Chars = Len;

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
				
				memmove(t->At(0), t->At(Inside), After*sizeof(char16));
				t->Length(After);
				LayoutDirty = true;
			}
			else if (Inside)
			{
				// Re-style the whole run
				t->SetStyle(CacheStyle);
				LayoutDirty = true;

				if (t->Element != CONTENT)
				{
					t->Element = CONTENT;
					t->Param.Empty();
				}
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
	
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *Line = Layout[i];
		PtrCheckBreak(Line);
		int Len = Line->Length();				
				
		LineOffset[i] = CharPos;
		LineLen[i] = Len;
				
		if (Cur.Offset >= CharPos &&
			Cur.Offset <= CharPos + Len)
		{
			if (Cur.LineHint < 0 || i == Cur.LineHint)
				CurLine = i;
		}				
				
		CharPos += Len;
	}
			
	if (CurLine < 0)
	{
		LgiAssert(!"Index not in layout lines.");
		return false;
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
				return false;
			break;
		}				
		case SkDownLine:
		{
			// Get next line...
			if (CurLine >= (int)Layout.Length() - 1)
				return false;
			Line = Layout[++CurLine];
			if (!Line)
				return false;
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
					int Char = Ds->CharAt(Px);
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
			GTreeItem *TxtElem = PrintNode(TxtRoot, "[%i] range=%i-%i, len=%i, style=%s, '%.20S'",
				i,
				Pos, Pos + Len - 1,
				Len,
				St->GetStyle() ? St->GetStyle()->Name.Get() : NULL,
				St->At(0));
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
				PrintNode(Elem, "[%i] style=%s len=%i txt='%.20S'",
					n,
					Style ? Style->Name.Get() : NULL,
					Ds->Length(),
					(const char16*) (*Ds));
			}
		}
	}
}
#endif
