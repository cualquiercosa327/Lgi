#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"
#include "GScrollBar.h"
#include "GCssTools.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char *GRichEditElemContext::GetElement(GRichEditElem *obj)
{
	return obj->Tag;
}
	
const char *GRichEditElemContext::GetAttr(GRichEditElem *obj, const char *Attr)
{
	const char *a = NULL;
	obj->Get(Attr, a);
	return a;
}
	
bool GRichEditElemContext::GetClasses(GArray<const char *> &Classes, GRichEditElem *obj)
{
	const char *c;
	if (!obj->Get("class", c))
		return false;
		
	GString cls = c;
	GString::Array classes = cls.Split(" ");
	for (unsigned i=0; i<classes.Length(); i++)
		Classes.Add(NewStr(classes[i]));
	return true;
}

GRichEditElem *GRichEditElemContext::GetParent(GRichEditElem *obj)
{
	return dynamic_cast<GRichEditElem*>(obj->Parent);
}

GArray<GRichEditElem*> GRichEditElemContext::GetChildren(GRichEditElem *obj)
{
	GArray<GRichEditElem*> a;
	for (unsigned i=0; i<obj->Children.Length(); i++)
		a.Add(dynamic_cast<GRichEditElem*>(obj->Children[i]));
	return a;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GCssCache::GCssCache()
{
	Idx = 1;
}
	
GCssCache::~GCssCache()
{
	Styles.DeleteObjects();
}

bool GCssCache::OutputStyles(GStream &s, int TabDepth)
{
	char Tabs[64];
	memset(Tabs, '\t', TabDepth);
	Tabs[TabDepth] = 0;
		
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		GNamedStyle *ns = Styles[i];
		if (ns)
		{
			s.Print("%s.%s {\n", Tabs, ns->Name.Get());
				
			GAutoString a = ns->ToString();
			GString all = a.Get();
			GString::Array lines = all.Split("\n");
			for (unsigned n=0; n<lines.Length(); n++)
			{
				s.Print("%s%s\n", Tabs, lines[n].Get());
			}
				
			s.Print("%s}\n\n", Tabs);
		}
	}
		
	return true;
}

GNamedStyle *GCssCache::AddStyleToCache(GAutoPtr<GCss> &s)
{
	if (!s)
		return NULL;

	// Look through existing styles for a match...			
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		GNamedStyle *ns = Styles[i];
		if (*ns == *s)
			return ns;
	}
		
	// Not found... create new...
	GNamedStyle *ns = new GNamedStyle;
	if (ns)
	{
		ns->Name.Printf("style%i", Idx++);
		*(GCss*)ns = *s.Get();
		Styles.Add(ns);
	}
		
	return ns;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GRichTextPriv::GRichTextPriv(GRichTextEdit *view) :
	GHtmlParser(view),
	GFontCache(SysFont)
{
	View = view;
	WordSelectMode = false;
	Dirty = false;
	ScrollOffsetPx = 0;
	ShowTools = true;
	ScrollChange = false;
	DocumentExtent.x = 0;
	DocumentExtent.y = 0;

	for (unsigned i=0; i<CountOf(Areas); i++)
	{
		Areas[i].ZOff(-1, -1);
	}

	Values[GRichTextEdit::FontFamilyBtn] = "FontName";
	Values[GRichTextEdit::FontSizeBtn] = "14";

	Values[GRichTextEdit::BoldBtn] = true;
	Values[GRichTextEdit::ItalicBtn] = false;
	Values[GRichTextEdit::UnderlineBtn] = false;
		
	Values[GRichTextEdit::ForegroundColourBtn] = (int64)Rgb24To32(LC_TEXT);
	Values[GRichTextEdit::BackgroundColourBtn] = (int64)Rgb24To32(LC_WORKSPACE);

	Values[GRichTextEdit::MakeLinkBtn] = TEXT_LINK;
	Values[GRichTextEdit::CapabilityBtn] = TEXT_CAP_BTN;

	Padding(GCss::Len(GCss::LenPx, 4));

	EmptyDoc();
}
	
GRichTextPriv::~GRichTextPriv()
{
	Empty();
}
	
bool GRichTextPriv::Error(const char *file, int line, const char *fmt, ...)
{
	va_list Arg;
	va_start(Arg, fmt);
	GString s;
	LgiPrintf(s, fmt, Arg);
	va_end(Arg);
	LgiTrace("%s:%i - Error: %s\n", file, line, s.Get());
		
	LgiAssert(0);
	return false;
}

void GRichTextPriv::UpdateStyleUI()
{
	if (!Cursor ||
		!Cursor->Blk)
	{
		LgiAssert(0);
		return;
	}

	TextBlock *b = dynamic_cast<TextBlock*>(Cursor->Blk);
	StyleText *st = b ? b->GetTextAt(Cursor->Offset) : NULL;
	if (st)
	{
		GFont *f = GetFont(st->GetStyle());
		if (f)
		{
			Values[GRichTextEdit::FontFamilyBtn] = f->Face();
			Values[GRichTextEdit::FontSizeBtn] = f->PointSize();
			Values[GRichTextEdit::FontSizeBtn].CastString();
			Values[GRichTextEdit::BoldBtn] = f->Bold();
			Values[GRichTextEdit::ItalicBtn] = f->Italic();
			Values[GRichTextEdit::UnderlineBtn] = f->Underline();
		}
		
		Values[GRichTextEdit::ForegroundColourBtn] = (int64) (st->Colours.Fore.IsValid() ? st->Colours.Fore.c32() : TextColour.c32());
		Values[GRichTextEdit::BackgroundColourBtn] = (int64) (st->Colours.Back.IsValid() ? st->Colours.Back.c32() : 0);
	}

	View->Invalidate(Areas + GRichTextEdit::ToolsArea);
}

void GRichTextPriv::ScrollTo(GRect r)
{
	GRect Content = Areas[GRichTextEdit::ContentArea];
	Content.Offset(-Content.x1, ScrollOffsetPx-Content.y1);

	if (r.y1 < Content.y1)
	{
		int OffsetPx = max(r.y1, 0);
		View->SetScrollPos(0, OffsetPx / ScrollLinePx);
		InvalidateDoc(NULL);
	}
	if (r.y2 > Content.y2)
	{
		int OffsetPx = r.y2 - Content.Y();
		View->SetScrollPos(0, (OffsetPx + ScrollLinePx - 1) / ScrollLinePx);
		InvalidateDoc(NULL);
	}
}

void GRichTextPriv::InvalidateDoc(GRect *r)
{
	// Transform the coordinates from doc to screen space
	GRect &c = Areas[GRichTextEdit::ContentArea];
	if (r)
	{
		GRect t = *r;
		t.Offset(c.x1, c.y1 - ScrollOffsetPx);
		View->Invalidate(&t);
	}
	else View->Invalidate(&c);
}

void GRichTextPriv::EmptyDoc()
{
	Block *Def = new TextBlock(this);
	if (Def)
	{			
		Blocks.Add(Def);
		Cursor.Reset(new BlockCursor(Def, 0, 0));
	}
}
	
void GRichTextPriv::Empty()
{
	// Delete cursors first to avoid hanging references
	Cursor.Reset();
	Selection.Reset();
		
	// Clear the block list..
	Blocks.DeleteObjects();
}

bool GRichTextPriv::Seek(BlockCursor *In, SeekType Dir, bool Select)
{
	if (!In || !In->Blk || Blocks.Length() == 0)
		return false;
		
	GAutoPtr<BlockCursor> c;

	bool Status = false;
	switch (Dir)
	{
		case SkLineEnd:
		case SkLineStart:
		case SkUpLine:
		case SkDownLine:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			Block *b = c->Blk;
			Status = b->Seek(Dir, *c);
			if (Status)
				break;

			if (Dir == SkUpLine)
			{
				// No more lines in the current block...
				// Move to the next block.
				int CurIdx = Blocks.IndexOf(b);
				int NewIdx = CurIdx - 1;
				if (NewIdx >= 0)
				{
					Block *b = Blocks[NewIdx];
					if (!b)
						return false;
						
					c.Reset(new BlockCursor(b, b->Length(), b->GetLines() - 1));
					LgiAssert(c->Offset >= 0);
					Status = true;							
				}
			}
			else if (Dir == SkDownLine)
			{
				// No more lines in the current block...
				// Move to the next block.
				int CurIdx = Blocks.IndexOf(b);
				int NewIdx = CurIdx + 1;
				if ((unsigned)NewIdx < Blocks.Length())
				{
					Block *b = Blocks[NewIdx];
					if (!b)
						return false;
						
					c.Reset(new BlockCursor(b, 0, 0));
					LgiAssert(c->Offset >= 0);
					Status = true;							
				}
			}
			break;
		}
		case SkDocStart:
		{
			if (!c.Reset(new BlockCursor(Blocks[0], 0, 0)))
				break;

			Status = true;
			break;
		}
		case SkDocEnd:
		{
			if (Blocks.Length() == 0)
				break;

			Block *l = Blocks.Last();
			if (!c.Reset(new BlockCursor(l, l->Length(), -1)))
				break;

			Status = true;
			break;
		}
		case SkLeftChar:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			if (c->Offset > 0)
			{
				c->Offset--;
				Status = true;
			}
			else // Seek to previous block
			{
				SeekPrevBlock:
				int Idx = Blocks.IndexOf(c->Blk);
				if (Idx < 0)
				{
					LgiAssert(0);
					break;
				}

				if (Idx == 0)
					break; // Beginning of document
				
				Block *b = Blocks[--Idx];
				if (!b)
				{
					LgiAssert(0);
					break;
				}

				if (!c.Reset(new BlockCursor(b, b->Length(), -1)))
					break;

				Status = true;
			}
			break;
		}
		case SkLeftWord:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			if (c->Offset > 0)
			{
				GArray<char16> a;
				c->Blk->CopyAt(0, c->Offset, &a);
					
				int i = c->Offset;
				while (i > 0 && IsWordBreakChar(a[i-1]))
					i--;
				while (i > 0 && !IsWordBreakChar(a[i-1]))
					i--;

				c->Offset = i;
				Status = true;
			}
			else // Seek into previous block?
			{
				goto SeekPrevBlock;
			}
			break;
		}
		case SkRightChar:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			if (c->Offset < c->Blk->Length())
			{
				c->Offset++;
				Status = true;
			}
			else // Seek to next block
			{
				SeekNextBlock:
				int Idx = Blocks.IndexOf(c->Blk);
				if (Idx < 0)
				{
					LgiAssert(0);
					break;
				}

				if (Idx >= Blocks.Length() - 1)
					break; // End of document
				
				Block *b = Blocks[++Idx];
				if (!b)
				{
					LgiAssert(0);
					break;
				}

				if (!c.Reset(new BlockCursor(b, 0, 0)))
					break;

				Status = true;
			}
			break;
		}
		case SkRightWord:
		{
			if (!c.Reset(new BlockCursor(*In)))
				break;

			if (c->Offset < c->Blk->Length())
			{
				GArray<char16> a;
				int RemainingCh = c->Blk->Length() - c->Offset;
				c->Blk->CopyAt(c->Offset, RemainingCh, &a);
					
				int i = 0;
				while (i < RemainingCh && !IsWordBreakChar(a[i]))
					i++;
				while (i < RemainingCh && IsWordBreakChar(a[i]))
					i++;

				c->Offset += i;
				Status = true;
			}
			else // Seek into next block?
			{
				goto SeekNextBlock;
			}
			break;
		}
		case SkUpPage:
		{
			GRect &Content = Areas[GRichTextEdit::ContentArea];
			int LineHint = -1;
			int TargetY = In->Pos.y1 - Content.Y();
			int Idx = HitTest(In->Pos.x1, max(TargetY, 0), LineHint);
			if (Idx >= 0)
			{
				int Offset = -1;
				Block *b = GetBlockByIndex(Idx, &Offset);
				if (b)
				{
					if (!c.Reset(new BlockCursor(b, Offset, LineHint)))
						break;

					Status = true;
				}
			}
			break;
		}
		case SkDownPage:
		{
			GRect &Content = Areas[GRichTextEdit::ContentArea];
			int LineHint = -1;
			int TargetY = In->Pos.y1 + Content.Y();
			int Idx = HitTest(In->Pos.x1, min(TargetY, DocumentExtent.y-1), LineHint);
			if (Idx >= 0)
			{
				int Offset = -1, BlkIdx = -1;
				int CursorBlkIdx = Blocks.IndexOf(Cursor->Blk);
				Block *b = GetBlockByIndex(Idx, &Offset, &BlkIdx);

				if (!b ||
					BlkIdx < CursorBlkIdx ||
					(BlkIdx == CursorBlkIdx && Offset < Cursor->Offset))
				{
					LgiAssert(!"GetBlockByIndex failed.\n");
					LgiTrace("%s:%i - GetBlockByIndex failed.\n", _FL);
				}

				if (b)
				{
					if (!c.Reset(new BlockCursor(b, Offset, LineHint)))
						break;

					Status = true;
				}
			}
			break;
		}
		default:
		{
			LgiAssert(!"Unknown seek type.");
			return false;
		}
	}
		
	if (Status)
		SetCursor(c, Select);
		
	return Status;
}
	
bool GRichTextPriv::CursorFirst()
{
	if (!Cursor || !Selection)
		return true;
		
	int CIdx = Blocks.IndexOf(Cursor->Blk);
	int SIdx = Blocks.IndexOf(Selection->Blk);
	if (CIdx != SIdx)
		return CIdx < SIdx;
		
	return Cursor->Offset < Selection->Offset;
}
	
bool GRichTextPriv::SetCursor(GAutoPtr<BlockCursor> c, bool Select)
{
	GRect InvalidRc(0, 0, -1, -1);

	if (!c || !c->Blk)
	{
		LgiAssert(0);
		return false;
	}

	if (Select && !Selection)
	{
		// Selection starting... save cursor as selection end point
		if (Cursor)
			InvalidRc = Cursor->Line;
		Selection = Cursor;
	}
	else if (!Select && Selection)
	{
		// Selection ending... invalidate selection region and delete 
		// selection end point
		GRect r = SelectionRect();
		InvalidateDoc(&r);
		Selection.Reset();

		// LgiTrace("Ending selection delete(sel) Idx=%i\n", i);
	}
	else if (Select && Cursor)
	{
		// Changing selection...
		InvalidRc = Cursor->Line;

		// LgiTrace("Changing selection region: %i\n", i);
	}

	if (Cursor && !Select)
	{
		// Just moving cursor
		InvalidateDoc(&Cursor->Pos);
	}

	if (!Cursor)
		Cursor.Reset(new BlockCursor(*c));
	else
		Cursor = c;

	Cursor->Blk->GetPosFromIndex(Cursor);
	UpdateStyleUI();
	
	#if DEBUG_OUTLINE_CUR_DISPLAY_STR || DEBUG_OUTLINE_CUR_STYLE_TEXT
	InvalidateDoc(NULL);
	#else
	if (Select)
		InvalidRc.Union(&Cursor->Line);
	else
		InvalidateDoc(&Cursor->Pos);
		
	if (InvalidRc.Valid())
	{
		// Update the screen
		InvalidateDoc(&InvalidRc);
	}
	#endif

	// Check the cursor is on the visible part of the document.
	if (Cursor->Pos.Valid())
		ScrollTo(Cursor->Pos);
	else
		LgiTrace("%s:%i - Invalid cursor position.\n", _FL);

	return true;
}

GRect GRichTextPriv::SelectionRect()
{
	GRect SelRc;
	if (Cursor)
	{
		SelRc = Cursor->Line;
		if (Selection)
			SelRc.Union(&Selection->Line);
	}
	else if (Selection)
	{
		SelRc = Selection->Line;
	}
	return SelRc;
}

int GRichTextPriv::IndexOfCursor(BlockCursor *c)
{
	if (!c)
		return -1;

	int CharPos = 0;
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		if (c->Blk == b)
			return CharPos + c->Offset;			
		CharPos += b->Length();
	}
		
	LgiAssert(0);
	return -1;
}

GdcPt2 GRichTextPriv::ScreenToDoc(int x, int y)
{
	GRect &Content = Areas[GRichTextEdit::ContentArea];
	return GdcPt2(x - Content.x1, y - Content.y1 + ScrollOffsetPx);
}

GdcPt2 GRichTextPriv::DocToScreen(int x, int y)
{
	GRect &Content = Areas[GRichTextEdit::ContentArea];
	return GdcPt2(x + Content.x1, y + Content.y1 - ScrollOffsetPx);
}

int GRichTextPriv::HitTest(int x, int y, int &LineHint)
{
	int CharPos = 0;
	HitTestResult r(x, y);

	if (Blocks.Length() == 0)
		return -1;

	GRect rc = Blocks.First()->GetPos();
	if (y < rc.y1)
		return 0;

	Block *b;
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		b = Blocks[i];
		GRect p = b->GetPos();
		bool Over = y >= p.y1 && y <= p.y2;
		if (b->HitTest(r))
		{
			LineHint = r.LineHint;
			return CharPos + r.Idx;
		}
		else if (Over)
		{
			LgiAssert(!"Block failed to hit.");
		}
			
		CharPos += b->Length();
	}

	b = Blocks.Last();
	rc = b->GetPos();
	if (y > rc.y2)
		return CharPos + b->Length();
		
	return -1;
}
	
bool GRichTextPriv::CursorFromPos(int x, int y, GAutoPtr<BlockCursor> *Cursor, int *GlobalIdx)
{
	int CharPos = 0;
	HitTestResult r(x, y);

	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		if (b->HitTest(r))
		{
			if (Cursor)
				Cursor->Reset(new BlockCursor(b, r.Idx, r.LineHint));
			if (GlobalIdx)
				*GlobalIdx = CharPos + r.Idx;

			return true;
		}
			
		CharPos += b->Length();
	}
		
	return false;
}

GRichTextPriv::Block *GRichTextPriv::GetBlockByIndex(int Index, int *Offset, int *BlockIdx)
{
	int CharPos = 0;
		
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		int Len = b->Length();

		if (Index >= CharPos &&
			Index <= CharPos + Len)
		{
			if (BlockIdx)
				*BlockIdx = i;
			if (Offset)
				*Offset = Index - CharPos;
			return b;
		}
			
		CharPos += b->Length();
	}

	Block *b = Blocks.Last();
	if (Offset)
		*Offset = b->Length();
	if (BlockIdx)
		*BlockIdx = Blocks.Length() - 1;

	return b;
}
	
bool GRichTextPriv::Layout(GScrollBar *&ScrollY)
{
	Flow f(this);
	
	ScrollLinePx = View->GetFont()->GetHeight();

	GRect Client = Areas[GRichTextEdit::ContentArea];
	DocumentExtent.x = Client.X();

	GCssTools Ct(this, Font);
	GRect Content = Ct.ApplyPadding(Client);
	f.Left = Content.x1;
	f.Right = Content.x2;
	f.Top = f.CurY = Content.y1;

	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		b->OnLayout(f);

		if ((f.CurY > Client.Y()) && ScrollY==NULL && !ScrollChange)
		{
			// We need to add a scroll bar
			View->SetScrollBars(false, true);
			View->Invalidate();
			ScrollChange = true;
			return false;
		}
	}
	DocumentExtent.y = f.CurY + (Client.y2 - Content.y2);

	if (ScrollY)
	{
		int Lines = (f.CurY + ScrollLinePx - 1) / ScrollLinePx;
		int PageLines = (Client.Y() + ScrollLinePx - 1) / ScrollLinePx;
		ScrollY->SetPage(PageLines);
		ScrollY->SetLimits(0, Lines-1);
	}
		
	if (Cursor)
	{
		LgiAssert(Cursor->Blk != NULL);
		if (Cursor->Blk)
		{
			Cursor->Blk->GetPosFromIndex(Cursor);
			// LgiTrace("%s:%i - Cursor->Pos=%s\n", _FL, Cursor->Pos.GetStr());
		}
	}

	return true;
}

void GRichTextPriv::OnStyleChange(GRichTextEdit::RectType t)
{
}

void GRichTextPriv::PaintBtn(GSurface *pDC, GRichTextEdit::RectType t)
{
	GRect r = Areas[t];
	GVariant &v = Values[t];
	bool Down = v.Type == GV_BOOL && v.Value.Bool;

	SysFont->Colour(LC_TEXT, LC_MED);
	SysFont->Transparent(false);

	LgiThinBorder(pDC, r, Down ? EdgeXpSunken : EdgeXpRaised);
	switch (v.Type)
	{
		case GV_STRING:
		{
			GDisplayString Ds(SysFont, v.Str());
			Ds.Draw(pDC, r.x1 + ((r.X()-Ds.X())>>1), r.y1 + ((r.Y()-Ds.Y())>>1), &r);
			break;
		}
		case GV_INT64:
		{
			if (v.Value.Int64)
			{
				pDC->Colour((uint32)v.Value.Int64, 32);
				pDC->Rectangle(&r);
			}
			else
			{
				// Transparent
				int g[2] = { 128, 192 };
				pDC->ClipRgn(&r);
				for (int y=0; y<r.Y(); y+=2)
				{
					for (int x=0; x<r.X(); x+=2)
					{
						int i = ((y>>1)%2) ^ ((x>>1)%2);
						pDC->Colour(GColour(g[i],g[i],g[i]));
						pDC->Rectangle(r.x1+x, r.y1+y, r.x1+x+1, r.y1+y+1);
					}
				}
				pDC->ClipRgn(NULL);
			}
			break;
		}
		case GV_BOOL:
		{
			const char *Label = NULL;
			switch (t)
			{
				case GRichTextEdit::BoldBtn: Label = "B"; break;
				case GRichTextEdit::ItalicBtn: Label = "I"; break;
				case GRichTextEdit::UnderlineBtn: Label = "U"; break;
			}
			if (!Label) break;
			GDisplayString Ds(SysFont, Label);
			Ds.Draw(pDC, r.x1 + ((r.X()-Ds.X())>>1) + Down, r.y1 + ((r.Y()-Ds.Y())>>1) + Down, &r);
			break;
		}
	}
}

void GRichTextPriv::ClickBtn(GMouse &m, GRichTextEdit::RectType t)
{
	switch (t)
	{
		case GRichTextEdit::FontFamilyBtn:
		{
			List<const char> Fonts;
			if (!GFontSystem::Inst()->EnumerateFonts(Fonts))
			{
				LgiTrace("%s:%i - EnumerateFonts failed.\n", _FL);
				break;
			}

			bool UseSub = (SysFont->GetHeight() * Fonts.Length()) > (GdcD->Y() * 0.8);

			GSubMenu s;
			GSubMenu *Cur = NULL;
			int Idx = 1;
			char Last = 0;
			for (const char *f = Fonts.First(); f; )
			{
				if (*f == '@')
				{
					Fonts.Delete(f);
					DeleteArray(f);
					f = Fonts.Current();
				}
				else if (UseSub)
				{
					if (*f != Last || Cur == NULL)
					{
						GString str;
						str.Printf("%c...", Last = *f);
						Cur = s.AppendSub(str);
					}
					if (Cur)
						Cur->AppendItem(f, Idx++);
					else
						break;
					f = Fonts.Next();
				}
				else
				{
					s.AppendItem(f, Idx++);
					f = Fonts.Next();
				}
			}

			GdcPt2 p(Areas[t].x1, Areas[t].y2 + 1);
			View->PointToScreen(p);
			int Result = s.Float(View, p.x, p.y, true);
			if (Result)
			{
				Values[t] = Fonts[Result-1];
				View->Invalidate(Areas+t);
				OnStyleChange(t);
			}
			break;
		}
		case GRichTextEdit::FontSizeBtn:
		{
			static const char *Sizes[] = { "6", "7", "8", "9", "10", "11", "12", "14", "16", "18", "20", "24",
											"28", "32", "40", "50", "60", "80", "100", "120", 0 };
			GSubMenu s;
			for (int Idx = 0; Sizes[Idx]; Idx++)
				s.AppendItem(Sizes[Idx], Idx+1);

			GdcPt2 p(Areas[t].x1, Areas[t].y2 + 1);
			View->PointToScreen(p);
			int Result = s.Float(View, p.x, p.y, true);
			if (Result)
			{
				Values[t] = Sizes[Result-1];
				View->Invalidate(Areas+t);
				OnStyleChange(t);
			}
			break;
		}
		case GRichTextEdit::BoldBtn:
		case GRichTextEdit::ItalicBtn:
		case GRichTextEdit::UnderlineBtn:
		{
			Values[t] = !Values[t].CastBool();
			View->Invalidate(Areas+t);
			OnStyleChange(t);
		}
		case GRichTextEdit::ForegroundColourBtn:
		case GRichTextEdit::BackgroundColourBtn:
		{
			GdcPt2 p(Areas[t].x1, Areas[t].y2 + 1);
			View->PointToScreen(p);
			new SelectColour(this, p, t);
			break;
		}
		case GRichTextEdit::MakeLinkBtn:
		{
			LgiAssert(!"Impl link dialog.");
			break;
		}
		case GRichTextEdit::CapabilityBtn:
		{
			View->OnCloseInstaller();
			break;
		}
	}
}
	
void GRichTextPriv::Paint(GSurface *pDC, GScrollBar *&ScrollY)
{
	if (Areas[GRichTextEdit::CapabilityArea].Valid())
	{
		GRect &t = Areas[GRichTextEdit::CapabilityArea];
		pDC->Colour(GColour::Red);
		pDC->Rectangle(&t);
		int y = t.y1 + 4;
		for (unsigned i=0; i<NeedsCap.Length(); i++)
		{
			CtrlCap &cc = NeedsCap[i];
			GDisplayString Ds(SysFont, cc.Param);
			SysFont->Transparent(true);
			SysFont->Colour(GColour::White, GColour::Red);
			Ds.Draw(pDC, t.x1 + 4, y);
			y += Ds.Y() + 4;
		}

		PaintBtn(pDC, GRichTextEdit::CapabilityBtn);
	}

	if (Areas[GRichTextEdit::ToolsArea].Valid())
	{
		// Draw tools area...
		GRect &t = Areas[GRichTextEdit::ToolsArea];
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle(&t);

		GRect r = t;
		r.Size(2, 2);
		#define AllocPx(sz, border) \
			GRect(r.x1, r.y1, r.x1 + (int)(sz) - 1, r.y2); r.x1 += (int)(sz) + border

		Areas[GRichTextEdit::FontFamilyBtn] = AllocPx(100, 6);
		Areas[GRichTextEdit::FontSizeBtn] = AllocPx(40, 6);

		Areas[GRichTextEdit::BoldBtn] = AllocPx(r.Y(), 0);
		Areas[GRichTextEdit::ItalicBtn] = AllocPx(r.Y(), 0);
		Areas[GRichTextEdit::UnderlineBtn] = AllocPx(r.Y(), 6);

		Areas[GRichTextEdit::ForegroundColourBtn] = AllocPx(r.Y()*1.5, 0);
		Areas[GRichTextEdit::BackgroundColourBtn] = AllocPx(r.Y()*1.5, 6);

		GDisplayString Ds(SysFont, TEXT_LINK);
		Areas[GRichTextEdit::MakeLinkBtn] = AllocPx(Ds.X() + 12, 6);

		for (unsigned i = GRichTextEdit::FontFamilyBtn; i <= GRichTextEdit::MakeLinkBtn; i++)
		{
			PaintBtn(pDC, (GRichTextEdit::RectType) i);
		}
	}

	GRect r = Areas[GRichTextEdit::ContentArea];
	#if defined(WINDOWS) && !DEBUG_NO_DOUBLE_BUF
	GMemDC Mem(r.X(), r.Y(), pDC->GetColourSpace());
	GSurface *pScreen = pDC;
	pDC = &Mem;
	r.Offset(-r.x1, -r.y1);
	#else
	pDC->ClipRgn(&r);
	#endif

	ScrollOffsetPx = ScrollY ? ScrollY->Value() * ScrollLinePx : 0;
	pDC->SetOrigin(-r.x1, -r.y1+ScrollOffsetPx);

	int DrawPx = ScrollOffsetPx + Areas[GRichTextEdit::ContentArea].Y();
	int ExtraPx = DrawPx > DocumentExtent.y ? DrawPx - DocumentExtent.y : 0;

	r.Set(0, 0, DocumentExtent.x-1, DocumentExtent.y-1);

	// Fill the padding...
	GCssTools ct(this, Font);
	r = ct.PaintPadding(pDC, r);

	// Fill the background...
	GCss::ColorDef cBack = BackgroundColor();
	pDC->Colour(cBack.IsValid() ? cBack : GColour(LC_WORKSPACE, 24));
	pDC->Rectangle(&r);
	if (ExtraPx)
		pDC->Rectangle(0, DocumentExtent.y, DocumentExtent.x-1, DocumentExtent.y+ExtraPx);

	PaintContext Ctx;
	Ctx.pDC = pDC;
	Ctx.Cursor = Cursor;
	Ctx.Select = Selection;
	Ctx.Colours[Unselected].Fore.Set(LC_TEXT, 24);
	Ctx.Colours[Unselected].Back.Set(LC_WORKSPACE, 24);		
		
	if (View->Focus())
	{
		Ctx.Colours[Selected].Fore.Set(LC_FOCUS_SEL_FORE, 24);
		Ctx.Colours[Selected].Back.Set(LC_FOCUS_SEL_BACK, 24);
	}
	else
	{
		Ctx.Colours[Selected].Fore.Set(LC_NON_FOCUS_SEL_FORE, 24);
		Ctx.Colours[Selected].Back.Set(LC_NON_FOCUS_SEL_BACK, 24);
	}
		
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		if (b)
		{
			b->OnPaint(Ctx);
			#if DEBUG_OUTLINE_BLOCKS
			pDC->Colour(GColour(192, 192, 192));
			pDC->LineStyle(GSurface::LineDot);
			pDC->Box(&b->GetPos());
			pDC->LineStyle(GSurface::LineSolid);
			#endif
		}
	}

	#ifdef _DEBUG
	pDC->Colour(GColour::Green);
	for (unsigned i=0; i<DebugRects.Length(); i++)
	{
		pDC->Box(&DebugRects[i]);
	}
	#endif

	#if defined(WINDOWS) && !DEBUG_NO_DOUBLE_BUF
	Mem.SetOrigin(0, 0);
	pScreen->Blt(Areas[GRichTextEdit::ContentArea].x1, Areas[GRichTextEdit::ContentArea].y1, &Mem);
	#endif
}
	
GHtmlElement *GRichTextPriv::CreateElement(GHtmlElement *Parent)
{
	return new GRichEditElem(Parent);
}

bool GRichTextPriv::ToHtml()		
{
	GStringPipe p(256);
		
	p.Print("<html>\n"
			"<head>\n"
			"\t<style>\n");		
	OutputStyles(p, 1);		
	p.Print("\t</style>\n"
			"</head>\n"
			"<body>\n");
		
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Blocks[i]->ToHtml(p);
	}
		
	p.Print("</body>\n");
	return UtfNameCache.Reset(p.NewStr());
}
	
void GRichTextPriv::DumpBlocks()
{
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		LgiTrace("%p: style=%p/%s {\n",
			b,
			b->GetStyle(),
			b->GetStyle() ? b->GetStyle()->Name.Get() : NULL);
		b->Dump();
		LgiTrace("}\n");
	}
}
	
bool GRichTextPriv::FromHtml(GHtmlElement *e, CreateContext &ctx, GCss *ParentStyle, int Depth)
{
	char Sp[256];
	int SpLen = Depth << 1;
	memset(Sp, ' ', SpLen);
	Sp[SpLen] = 0;
		
	for (unsigned i = 0; i < e->Children.Length(); i++)
	{
		GHtmlElement *c = e->Children[i];
		GAutoPtr<GCss> Style;
		if (ParentStyle)
			Style.Reset(new GCss(*ParentStyle));
			
		// Check to see if the element is block level and end the previous
		// paragraph if so.
		c->Info = c->Tag ? GHtmlStatic::Inst->GetTagInfo(c->Tag) : NULL;
		bool IsBlock =	c->Info != NULL && c->Info->Block();

		switch (c->TagId)
		{
			case TAG_STYLE:
			{
				char16 *Style = e->GetText();
				if (Style)
					LgiAssert(!"Impl me.");
				continue;
				break;
			}
			case TAG_B:
			{
				if (Style.Reset(new GCss))
					Style->FontWeight(GCss::FontWeightBold);
				break;
			}
			/*
			case TAG_IMG:
			{
				ctx.Tb = NULL;
					
				const char *Src = NULL;
				if (e->Get("src", Src))
				{
					ImageBlock *Ib = new ImageBlock;
					if (Ib)
					{
						Ib->Src = Src;
						Blocks.Add(Ib);
					}
				}
				break;
			}
			*/
			default:
			{
				break;
			}
		}
			
		const char *Css, *Class;
		if (c->Get("style", Css))
		{
			if (!Style)
				Style.Reset(new GCss);
			if (Style)
				Style->Parse(Css, ParseRelaxed);
		}
		if (c->Get("class", Class))
		{
			GCss::SelArray Selectors;
			GRichEditElemContext StyleCtx;
			if (ctx.StyleStore.Match(Selectors, &StyleCtx, dynamic_cast<GRichEditElem*>(c)))
			{
				for (unsigned n=0; n<Selectors.Length(); n++)
				{
					GCss::Selector *sel = Selectors[n];
					if (sel)
					{
						const char *s = sel->Style;
						if (s)
						{
							if (!Style)
								Style.Reset(new GCss);
							if (Style)
							{
								LgiTrace("class style: %s\n", s);
								Style->Parse(s);
							}
						}
					}
				}
			}
		}

		GNamedStyle *CachedStyle = AddStyleToCache(Style);
		//LgiTrace("%s%s IsBlock=%i CachedStyle=%p\n", Sp, c->Tag.Get(), IsBlock, CachedStyle);
			
		if ((IsBlock && ctx.LastChar != '\n') || c->TagId == TAG_BR)
		{
			if (!ctx.Tb)
				Blocks.Add(ctx.Tb = new TextBlock(this));
			if (ctx.Tb)
			{
				ctx.Tb->AddText(-1, L"\n", 1, CachedStyle);
				ctx.LastChar = '\n';
			}
		}

		bool EndStyleChange = false;
		if (IsBlock && ctx.Tb != NULL)
		{
			if (CachedStyle != ctx.Tb->GetStyle())
			{
				// Start a new block because the styles are different...
				EndStyleChange = true;
				Blocks.Add(ctx.Tb = new TextBlock(this));
					
				if (CachedStyle)
				{
					ctx.Tb->Fnt = ctx.FontCache->GetFont(CachedStyle);
					ctx.Tb->SetStyle(CachedStyle);
				}
			}
		}

		if (c->GetText())
		{
			if (!ctx.Tb)
				Blocks.Add(ctx.Tb = new TextBlock(this));
			
			ctx.AddText(CachedStyle, c->GetText());
		}
			
		if (!FromHtml(c, ctx, Style, Depth + 1))
			return false;
			
		if (EndStyleChange)
			ctx.Tb = NULL;
	}

	return true;
}

bool GRichTextPriv::GetSelection(GArray<char16> &Text)
{
	bool Cf = CursorFirst();
	GRichTextPriv::BlockCursor *Start = Cf ? Cursor : Selection;
	GRichTextPriv::BlockCursor *End = Cf ? Selection : Cursor;
	if (Start->Blk == End->Blk)
	{
		// In the same block... just copy
		int Len = End->Offset - Start->Offset;
		Start->Blk->CopyAt(Start->Offset, Len, &Text);
	}
	else
	{
		// Multi-block delete...

		// 1) Copy the content to the end of the first block
		Start->Blk->CopyAt(Start->Offset, -1, &Text);

		// 2) Copy any blocks between 'Start' and 'End'
		int i = Blocks.IndexOf(Start->Blk);
		int EndIdx = Blocks.IndexOf(End->Blk);
		if (i >= 0 && EndIdx >= i)
		{
			for (++i; Blocks[i] != End->Blk && i < (int)Blocks.Length(); i++)
			{
				GRichTextPriv::Block *&b = Blocks[i];
				b->CopyAt(0, -1, &Text);
			}
		}
		else
		{
			LgiAssert(0);
			return false;
		}

		// 3) Delete any text up to the Cursor in the 'End' block
		End->Blk->CopyAt(0, End->Offset, &Text);
	}

	// Null terminate
	Text.Add(0);

	return true;
}