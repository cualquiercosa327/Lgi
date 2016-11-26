#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"
#include "GForEach.h"

GRichTextPriv::TextBlock::TextBlock()
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

bool GRichTextPriv::TextBlock::ToHtml(GStream &s)
{
	s.Print("<p>");
	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		GNamedStyle *style = t->GetStyle();
		if (style)
		{
			s.Print("<span class='%s'>%.*S</span>", style->Name.Get(), t->Length(), &t[0]);
		}
		else
		{
			s.Print("%.*S", t->Length(), &t[0]);
		}
	}
	s.Print("</p>\n");
	return true;
}		

bool GRichTextPriv::TextBlock::GetPosFromIndex(GRect *CursorPos, GRect *LinePos, int Index)
{
	if (!CursorPos || !LinePos)
	{
		LgiAssert(0);
		return false;
	}
		
	int CharPos = 0;
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
					
			if (Index >= CharPos &&
				Index <= CharPos + dsChars)
			{
				int CharOffset = Index - CharPos;
				if (CharOffset == 0)
				{
					// First char
					CursorPos->x1 = r.x1 + IntToFixed(FixX);
				}
				else if (CharOffset == dsChars)
				{
					// Last char
					CursorPos->x1 = r.x1 + IntToFixed(FixX + ds->FX());
				}
				else
				{
					// In the middle somewhere...
					GDisplayString Tmp(ds->GetFont(), *ds, CharOffset);
					CursorPos->x1 = r.x1 + IntToFixed(FixX + Tmp.FX());
				}

				CursorPos->y1 = r.y1 + ds->OffsetY;
				CursorPos->y2 = CursorPos->y1 + ds->Y() - 1;
				CursorPos->x2 = CursorPos->x1 + 1;

				*LinePos = r;
				return true;
			}					
					
			FixX += ds->FX();
			CharPos += ds->Length();
		}
				
		if (tl->Strs.Length() == 0 && Index == CharPos)
		{
			// Cursor at the start of empty line.
			CursorPos->x1 = r.x1;
			CursorPos->x2 = CursorPos->x1 + 1;
			CursorPos->y1 = r.y1;
			CursorPos->y2 = r.y2;
			*LinePos = r;
			return true;
		}
				
		CharPos += tl->NewLine;
	}
			
	return false;
}
		
bool GRichTextPriv::TextBlock::HitTest(HitTestResult &htr)
{
	if (!Pos.Overlap(htr.In.x, htr.In.y))
		return false;

	int CharPos = 0;
	for (unsigned i=0; i<Layout.Length(); i++)
	{
		TextLine *tl = Layout[i];
		PtrCheckBreak(tl);

		GRect r = tl->PosOff;
		r.Offset(Pos.x1, Pos.y1);
		bool Over = r.Overlap(htr.In.x, htr.In.y);
		bool OnThisLine =	htr.In.y >= r.y1 &&
							htr.In.y <= r.y2;
		if (OnThisLine && htr.In.x < r.x1)
		{
			htr.Near = true;
			htr.Idx = CharPos;
			return true;
		}
				
		int FixX = 0;
		int InputX = IntToFixed(htr.In.x - Pos.x1);
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
						
				htr.Blk = this;
				htr.Ds = ds;
				htr.Idx = CharPos + OffChar;
				return true;
			}
					
			FixX += ds->FX();

			CharPos += ds->Length();
		}

		if (OnThisLine && htr.In.x > r.x2)
		{
			htr.Near = true;
			htr.Idx = CharPos;
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
		int CurY = LinePos.y1;
		GFont *Fnt = NULL;

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
				FixX += Ds->FX();
			}
					
			CharPos += Ds->Length();
		}
				
		CharPos += Line->NewLine;
	}

	if (Ctx.Cursor &&
		Ctx.Cursor->Blk == this)
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
		flow.CurY = Pos.y2 + 1;
		return true;
	}

	LayoutDirty = false;
	Layout.DeleteObjects();
			
	/*
	GString Str = flow.Describe();
	LgiTrace("%p::OnLayout Flow: %s Style: %s, %s, %s, %s\n",
		this,
		Str.Get(),
		Style?Style->Name.Get():0,
		Margin.GetStr(),
		Border.GetStr(),
		Padding.GetStr());
	*/
			
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

	/*
	Str = flow.Describe();
	LgiTrace("    Pos: %s Flow: %s\n", Pos.GetStr(), Str.Get());
	*/
			
	int FixX = 0; // Current x offset (fixed point) on the current line
	GAutoPtr<TextLine> CurLine(new TextLine(flow.Left - Pos.x1, flow.X(), flow.CurY - Pos.y1));
	if (!CurLine)
		return flow.d->Error(_FL, "alloc failed.");

	for (unsigned i=0; i<Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
				
		if (t->Length() == 0)
			continue;

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
				CurLine->PosOff.x2 = CurLine->PosOff.x1 + FixedToInt(FixX);
				FixX = 0;
				CurLine->LayoutOffsets(f->GetHeight());
				Pos.y2 = max(Pos.y2, Pos.y1 + CurLine->PosOff.y2);
				CurLine->NewLine = 1;
						
				// LgiTrace("        [%i] = %s\n", Layout.Length(), CurLine->PosOff.GetStr());
						
				Layout.Add(CurLine.Release());
				CurLine.Reset(new TextLine(flow.Left - Pos.x1, flow.X(), Pos.Y()));
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

			if (FixedToInt(FixX) + Ds->X() > Pos.X())
			{
				// Wrap the string onto the line...
				int AvailablePx = Pos.X() - FixedToInt(FixX);
				int FitChars = Ds->CharAt(AvailablePx);
				if (FitChars < 0)
					flow.d->Error(_FL, "CharAt(%i) failed.", AvailablePx);
				else
				{
					// Wind back to the last break opportunity
					int ch;
					for (ch = FitChars-1; ch > 0; ch--)
					{
						if ((*t)[ch] == ' ') // FIXME: use better breaking ops
							break;
					}
					if (ch > FitChars >> 2)
						Chars = FitChars;
							
					// Create a new display string of the right size...
					if (!Ds.Reset(new DisplayStr(t, f, s, Chars, flow.pDC)))
						return flow.d->Error(_FL, "failed to create wrapped display str.");
							
					FixX = Ds->FX();
				}
			}
			else
			{
				FixX += Ds->FX();
			}
					
			if (!Ds)
				break;
					
			CurLine->PosOff.x2 = FixedToInt(FixX);
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
			
	/*
	Str = flow.Describe();
	LgiTrace("    Pos: %s Flow: %s\n", Pos.GetStr(), Str.Get());
	*/

	return true;
}
		
GRichTextPriv::StyleText *GRichTextPriv::TextBlock::GetTextAt(uint32 Offset)
{
	StyleText **t = &Txt[0];
	StyleText **e = t + Txt.Length();
	while (Offset > 0 && t < e)
	{
		if (Offset < (*t)->Length())
			return *t;
		Offset -= (*t)->Length();
	}
	return NULL;
}

bool GRichTextPriv::TextBlock::IsValid()
{
	for (unsigned i = 0; i < Txt.Length(); i++)
	{
		StyleText *t = Txt[i];
		for (unsigned n = 0; n < t->Length(); n++)
		{
			if ((*t)[n] == 0)
			{
				LgiAssert(0);
				return false;
			}
		}
	}

	return true;
}

int GRichTextPriv::TextBlock::DeleteAt(int BlkOffset, int Chars, GArray<char16> *DeletedText)
{
	int Pos = 0;
	
	for (unsigned i=0; i<Txt.Length(); i++)
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
			t->Length(NewLen);
			LayoutDirty = true;
			return Remove;
		}
		Pos += t->Length();
	}

	IsValid();

	return 0;
}
		
bool GRichTextPriv::TextBlock::AddText(int AtOffset, const char16 *Str, int Chars, GNamedStyle *Style)
{
	if (!Str)
		return false;
	if (Chars < 0)
		Chars = StrlenW(Str);
			
	bool StyleDiff = (Style != NULL) ^ ((Txt.Length() ? Txt.Last()->GetStyle() : NULL) != NULL);
	StyleText *t = !StyleDiff && Txt.Length() ? Txt.Last() : NULL;
	if (t)
	{
		t->Add((char16*)Str, Chars);
		Len += Chars;
	}
	else if ((t = new StyleText(Str, Chars)))
	{
		Len += t->Length();
		Txt.Add(t);
	}
	else return false;

	LayoutDirty = true;
	t->SetStyle(Style);
	
	IsValid();
	
	return true;
}

int GRichTextPriv::TextBlock::CopyAt(int Offset, int Chars, GArray<char16> *Text)
{
	if (!Text)
		return 0;
	if (Chars < 0)
		Chars = Length();

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

	return 0;
}

int GRichTextPriv::TextBlock::Seek(SeekType To, int Offset, int XPos)
{
	int XOffset = XPos - Pos.x1;
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
				
		if (Offset >= CharPos &&
			Offset <= CharPos + Len) // - Line->NewLine
		{
			CurLine = i;
		}				
				
		CharPos += Len;
	}
			
	if (CurLine < 0)
	{
		LgiAssert(!"Index not in layout lines.");
		return -1;
	}
				
	TextLine *Line = NULL;
	switch (To)
	{
		case SkLineStart:
		{
			return LineOffset[CurLine];
		}
		case SkLineEnd:
		{
			return	LineOffset[CurLine] +
					LineLen[CurLine] -
					Layout[CurLine]->NewLine;
		}
		case SkUpLine:
		{
			// Get previous line...
			if (CurLine == 0)
				return -1;
			Line = Layout[--CurLine];
			if (!Line)
				return -1;
			break;
		}				
		case SkDownLine:
		{
			// Get next line...
			if (CurLine >= (int)Layout.Length() - 1)
				return -1;
			Line = Layout[++CurLine];
			if (!Line)
				return -1;
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
					int Px = XOffset - FixedToInt(FixX);
					int Char = Ds->CharAt(Px);
					if (Char >= 0)
					{
						return	LineOffset[CurLine] +	// Character offset of line
								CharOffset +			// Character offset of current string
								Char;					// Offset into current string for 'XOffset'
					}
				}
						
				FixX += Ds->FX();
				CharOffset += Ds->Length();
			}
					
			// Cursor is nearest the end of the string...?
			return LineOffset[CurLine] + Line->Length() - 1;
		}
		else if (Line->NewLine)
		{
			return LineOffset[CurLine];
		}
	}
			
	return -1;
}
	
