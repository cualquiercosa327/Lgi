#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"
#include "GScrollBar.h"
#include "GCssTools.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Utf16to32(GArray<uint32> &Out, const uint16 *In, int Len)
{
	if (Len == 0)
	{
		Out.Length(0);
		return true;
	}

	// Count the length of utf32 chars...
	uint16 *Ptr = (uint16*)In;
	int Bytes = sizeof(*In) * Len;
	int Chars = 0;
	while (	Bytes >= sizeof(*In) &&
			LgiUtf16To32(Ptr, Bytes) > 0)
		Chars++;
	
	// Set the output buffer size..
	if (!Out.Length(Chars))
		return false;

	// Convert the string...
	Ptr = (uint16*)In;
	Bytes = sizeof(*In) * Len;
	uint32 *o = &Out[0];
	uint32 *e = o + Out.Length();
	while (	Bytes >= sizeof(*In))
	{
		*o++ = LgiUtf16To32(Ptr, Bytes);
	}
	
	LgiAssert(o == e);
	return true;
}

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

uint32 GCssCache::GetStyles()
{
	uint32 c = 0;
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		c += Styles[i]->RefCount != 0;
	}
	return c;
}

void GCssCache::ZeroRefCounts()
{
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		Styles[i]->RefCount = 0;
	}
}

bool GCssCache::OutputStyles(GStream &s, int TabDepth)
{
	char Tabs[64];
	memset(Tabs, '\t', TabDepth);
	Tabs[TabDepth] = 0;
		
	for (unsigned i=0; i<Styles.Length(); i++)
	{
		GNamedStyle *ns = Styles[i];
		if (ns && ns->RefCount > 0)
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
		char *p = Prefix;
		ns->Name.Printf("%sStyle%i", p?p:"", Idx++);
		*(GCss*)ns = *s.Get();
		Styles.Add(ns);

		#if 0 // _DEBUG
		GAutoString ss = ns->ToString();
		if (ss)
			LgiTrace("%s = %s\n", ns->Name.Get(), ss.Get());
		#endif
	}
		
	return ns;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BlockCursorState::BlockCursorState(bool cursor, GRichTextPriv::BlockCursor *c)
{
	Cursor = cursor;
	Offset = c->Offset;
	LineHint = c->LineHint;
	BlockUid = c->Blk->GetUid();
}

bool BlockCursorState::Apply(GRichTextPriv *Ctx, bool Forward)
{
	GAutoPtr<GRichTextPriv::BlockCursor> &Bc = Cursor ? Ctx->Cursor : Ctx->Selection;
	if (!Bc)
		return false;

	int o = Bc->Offset;
	int lh = Bc->LineHint;
	int uid = Bc->Blk->GetUid();

	int Index;
	GRichTextPriv::Block *b;
	if (!Ctx->GetBlockByUid(b, BlockUid, &Index))
		return false;

	if (b != Bc->Blk)
		Bc.Reset(new GRichTextPriv::BlockCursor(b, Offset, LineHint));
	else
	{
		Bc->Offset = Offset;
		Bc->LineHint = LineHint;
	}

	Offset = o;
	LineHint = lh;
	BlockUid = uid;
	return true;
}

/// This is the simplest form of undo, just save the entire block state, and restore it if needed
CompleteTextBlockState::CompleteTextBlockState(GRichTextPriv *Ctx, GRichTextPriv::TextBlock *Tb)
{
	if (Ctx->Cursor)
		Cur.Reset(new BlockCursorState(true, Ctx->Cursor));
	if (Ctx->Selection)
		Sel.Reset(new BlockCursorState(false, Ctx->Selection));
	if (Tb)
	{
		Uid = Tb->GetUid();
		Blk.Reset(new GRichTextPriv::TextBlock(Tb));
	}
}

bool CompleteTextBlockState::Apply(GRichTextPriv *Ctx, bool Forward)
{
	int Index;
	GRichTextPriv::TextBlock *b;
	if (!Ctx->GetBlockByUid(b, Uid, &Index))
		return false;

	// Swap the local state with the block in the ctx
	Ctx->Blocks[Index] = Blk.Release();
	Blk.Reset(b);

	// Update cursors
	if (Cur)
		Cur->Apply(Ctx, Forward);
	if (Sel)
		Sel->Apply(Ctx, Forward);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
GRichTextPriv::GRichTextPriv(GRichTextEdit *view, GRichTextPriv *&Ptr) :
	GHtmlParser(view),
	GFontCache(SysFont)
{
	Ptr = this;
	View = view;
	Log = &LogBuffer;
	NextUid = 1;
	UndoPos = 0;
	WordSelectMode = false;
	Dirty = false;
	ScrollOffsetPx = 0;
	ShowTools = true;
	ScrollChange = false;
	DocumentExtent.x = 0;
	DocumentExtent.y = 0;
	if (Font.Reset(new GFont))
		*Font = *SysFont;

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
	Values[GRichTextEdit::RemoveLinkBtn] = TEXT_REMOVE_LINK;
	Values[GRichTextEdit::RemoveStyleBtn] = TEXT_REMOVE_STYLE;
	Values[GRichTextEdit::CapabilityBtn] = TEXT_CAP_BTN;
	Values[GRichTextEdit::EmojiBtn] = TEXT_EMOJI;

	Padding(GCss::Len(GCss::LenPx, 4));

	EmptyDoc();
}
	
GRichTextPriv::~GRichTextPriv()
{
	Empty();
}
	
bool GRichTextPriv::DeleteSelection(Transaction *t, char16 **Cut)
{
	if (!Cursor || !Selection)
		return false;

	GArray<uint32> DeletedText;
	GArray<uint32> *DelTxt = Cut ? &DeletedText : NULL;

	bool Cf = CursorFirst();
	GRichTextPriv::BlockCursor *Start = Cf ? Cursor : Selection;
	GRichTextPriv::BlockCursor *End = Cf ? Selection : Cursor;
	if (Start->Blk == End->Blk)
	{
		// In the same block... just delete the text
		int Len = End->Offset - Start->Offset;
		Start->Blk->DeleteAt(NoTransaction, Start->Offset, Len, DelTxt);
	}
	else
	{
		// Multi-block delete...

		// 1) Delete all the content to the end of the first block
		int StartLen = Start->Blk->Length();
		if (Start->Offset < StartLen)
			Start->Blk->DeleteAt(NoTransaction, Start->Offset, StartLen - Start->Offset, DelTxt);

		// 2) Delete any blocks between 'Start' and 'End'
		int i = Blocks.IndexOf(Start->Blk);
		if (i >= 0)
		{
			for (++i; Blocks[i] != End->Blk && i < (int)Blocks.Length(); )
			{
				GRichTextPriv::Block *b = Blocks[i];
				b->CopyAt(0, -1, DelTxt);
				Blocks.DeleteAt(i, true);
				DeleteObj(b);
			}
		}
		else
		{
			LgiAssert(0);
			return false;
		}

		// 3) Delete any text up to the Cursor in the 'End' block
		End->Blk->DeleteAt(NoTransaction, 0, End->Offset, DelTxt);

		// Try and merge the start and end blocks
		Merge(Start->Blk, End->Blk);
	}

	// Set the cursor and update the screen
	Cursor->Set(Start->Blk, Start->Offset, Start->LineHint);
	Selection.Reset();
	View->Invalidate();

	if (Cut)
	{
		DelTxt->Add(0);
		// *Cut = DelTxt->Release();
		*Cut = (char16*)LgiNewConvertCp(LGI_WideCharset, &DelTxt->First(), "utf-32", DelTxt->Length()*sizeof(uint32));
	}

	return true;
}


GRichTextPriv::Block *GRichTextPriv::Next(Block *b)
{
	int Idx = Blocks.IndexOf(b);
	if (Idx < 0)
		return NULL;
	if (++Idx >= (int)Blocks.Length())
		return NULL;
	return Blocks[Idx];
}

GRichTextPriv::Block *GRichTextPriv::Prev(Block *b)
{
	int Idx = Blocks.IndexOf(b);
	if (Idx <= 0)
		return NULL;
	return Blocks[--Idx];
}

bool GRichTextPriv::AddTrans(GAutoPtr<Transaction> &t)
{
	// Delete any transaction history after 'UndoPos'
	for (unsigned i=UndoPos; i<UndoQue.Length(); i++)
	{
		delete UndoQue[i];
	}
	UndoQue.Length(UndoPos);

	// Now add the new transaction
	UndoQue.Add(t.Release());

	// And the position is now at the end of the que
	UndoPos = UndoQue.Length();

	return true;
}

bool GRichTextPriv::SetUndoPos(int Pos)
{
	Pos = limit(Pos, 0, UndoQue.Length());
	if (UndoQue.Length() == 0)
		return true;

	while (Pos != UndoPos)
	{
		if (Pos > UndoPos)
		{
			// Forward in que
			Transaction *t = UndoQue[UndoPos];
			if (!t->Apply(this, true))
				return false;

			UndoPos++;
		}
		else if (Pos < UndoPos)
		{
			Transaction *t = UndoQue[UndoPos-1];
			if (!t->Apply(this, false))
				return false;

			UndoPos--;
		}
		else break; // We are done
	}

	Dirty = true;
	InvalidateDoc(NULL);
	return true;
}

bool GRichTextPriv::Error(const char *file, int line, const char *fmt, ...)
{
	va_list Arg;
	va_start(Arg, fmt);
	GString s;
	LgiPrintf(s, fmt, Arg);
	va_end(Arg);

	GString Err;
	Err.Printf("%s:%i - Error: %s\n", file, line, s.Get());
	Log->Write(Err, Err.Length());

	Err = LogBuffer.NewGStr();
	LgiTrace("%.*s", Err.Length(), Err.Get());
		
	LgiAssert(0);
	return false;
}

void GRichTextPriv::UpdateStyleUI()
{
	if (!Cursor || !Cursor->Blk)
	{
		Error(_FL, "Not a valid cursor.");
		return;
	}

	TextBlock *b = dynamic_cast<TextBlock*>(Cursor->Blk);

	GArray<StyleText*> Styles;
	if (b)
		b->GetTextAt(Cursor->Offset, Styles);
	StyleText *st = Styles.Length() ? Styles.First() : NULL;

	GFont *f = st ? GetFont(st->GetStyle()) : View->GetFont();
	if (f)
	{
		Values[GRichTextEdit::FontFamilyBtn] = f->Face();
		Values[GRichTextEdit::FontSizeBtn] = f->PointSize();
		Values[GRichTextEdit::FontSizeBtn].CastString();
		Values[GRichTextEdit::BoldBtn] = f->Bold();
		Values[GRichTextEdit::ItalicBtn] = f->Italic();
		Values[GRichTextEdit::UnderlineBtn] = f->Underline();
	}
	else
	{
		Values[GRichTextEdit::FontFamilyBtn] = "(Unknown)";
	}
		
	Values[GRichTextEdit::ForegroundColourBtn] = (int64) (st && st->Colours.Fore.IsValid() ? st->Colours.Fore.c32() : TextColour.c32());
	Values[GRichTextEdit::BackgroundColourBtn] = (int64) (st && st->Colours.Back.IsValid() ? st->Colours.Back.c32() : 0);

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
		UpdateStyleUI();
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
		return Error(_FL, "Not a valid 'In' cursor, Blks=%i", Blocks.Length());
		
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
						return Error(_FL, "No block at %i", NewIdx);
						
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
						return Error(_FL, "No block at %i", NewIdx);
						
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
				GArray<int> Ln;
				c->Blk->OffsetToLine(c->Offset, NULL, &Ln);
				if (Ln.Length() == 2 &&
					c->LineHint == Ln.Last())
				{
					c->LineHint = Ln.First();
				}
				else
				{
					c->Offset--;
					if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln))
						c->LineHint = Ln.First();
				}

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

				if (!c.Reset(new BlockCursor(b, b->Length(), b->GetLines()-1)))
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
				GArray<uint32> a;
				c->Blk->CopyAt(0, c->Offset, &a);
					
				int i = c->Offset;
				while (i > 0 && IsWordBreakChar(a[i-1]))
					i--;
				while (i > 0 && !IsWordBreakChar(a[i-1]))
					i--;

				c->Offset = i;

				GArray<int> Ln;
				if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln))
					c->LineHint = Ln[0];

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
				GArray<int> Ln;
				if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln) &&
					Ln.Length() == 2 &&
					c->LineHint == Ln.First())
				{
					c->LineHint = Ln.Last();
				}
				else
				{
					c->Offset++;
					if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln))
						c->LineHint = Ln.First();
				}

				Status = true;
			}
			else // Seek to next block
			{
				SeekNextBlock:
				int Idx = Blocks.IndexOf(c->Blk);
				if (Idx < 0)
					return Error(_FL, "Block ptr index error.");

				if (Idx >= (int)Blocks.Length() - 1)
					break; // End of document
				
				Block *b = Blocks[++Idx];
				if (!b)
					return Error(_FL, "No block at %i.", Idx);

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
				GArray<uint32> a;
				int RemainingCh = c->Blk->Length() - c->Offset;
				c->Blk->CopyAt(c->Offset, RemainingCh, &a);
					
				int i = 0;
				while (i < RemainingCh && !IsWordBreakChar(a[i]))
					i++;
				while (i < RemainingCh && IsWordBreakChar(a[i]))
					i++;

				c->Offset += i;
				
				GArray<int> Ln;
				if (c->Blk->OffsetToLine(c->Offset, NULL, &Ln))
					c->LineHint = Ln.Last();
				else
					c->LineHint = -1;
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
			return Error(_FL, "Unknown seek type.");
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
		return Error(_FL, "Invalid cursor.");

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

	if (Cursor &&
		Selection &&
		*Cursor == *Selection)
		Selection.Reset();

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
	if (!c || !c->Blk)
	{
		Error(_FL, "Invalid cursor param.");
		return -1;
	}

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

bool GRichTextPriv::Merge(Block *a, Block *b)
{
	TextBlock *ta = dynamic_cast<TextBlock*>(a);
	TextBlock *tb = dynamic_cast<TextBlock*>(b);
	if (!ta || !tb)
		return Error(_FL, "Can't merge non-text block: %p, %p", ta, tb);

	ta->Txt.Add(tb->Txt);
	ta->LayoutDirty = true;
	ta->Len += tb->Len;
	tb->Txt.Length(0);

	Blocks.Delete(b, true);
	Dirty = true;

	return true;
}

GSurface *GRichTextPriv::GetEmojiImage()
{
	if (!EmojiImg)
	{
		GString p = LgiGetSystemPath(LSP_APP_INSTALL);
		if (!p)
		{
			Error(_FL, "No app install path.");
			return NULL;
		}

		char File[MAX_PATH] = "";
		LgiMakePath(File, sizeof(File), p, "..\\src\\common\\Text\\Emoji\\EmojiMap.png");
		GAutoString a;
		if (!FileExists(File))
			a.Reset(LgiFindFile("EmojiMap.png"));
		
		EmojiImg.Reset(GdcD->Load(a ? a : File, false));
		
	}

	return EmojiImg;
}

int GRichTextPriv::HitTest(int x, int y, int &LineHint)
{
	int CharPos = 0;
	HitTestResult r(x, y);

	if (Blocks.Length() == 0)
	{
		Error(_FL, "No blocks.");
		return -1;
	}

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
			Error(_FL, "Block failed to hit, i=%i, pos=%s, y=%i.", i, p.GetStr(), y);
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

GRichTextPriv::Block *GRichTextPriv::GetBlockByIndex(int Index, int *Offset, int *BlockIdx, int *LineCount)
{
	int CharPos = 0;
	int Lines = 0;
		
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		int Len = b->Length();
		int Ln = b->GetLines();

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
		Lines += Ln;
	}

	Block *b = Blocks.Last();
	if (Offset)
		*Offset = b->Length();
	if (BlockIdx)
		*BlockIdx = Blocks.Length() - 1;
	if (LineCount)
		*LineCount = Lines;

	return b;
}
	
bool GRichTextPriv::Layout(GScrollBar *&ScrollY)
{
	Flow f(this);
	
	ScrollLinePx = View->GetFont()->GetHeight();

	GRect Client = Areas[GRichTextEdit::ContentArea];
	Client.Offset(-Client.x1, -Client.y1);
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
		ScrollY->SetLimits(0, Lines);
	}
		
	if (Cursor)
	{
		LgiAssert(Cursor->Blk != NULL);
		if (Cursor->Blk)
			Cursor->Blk->GetPosFromIndex(Cursor);
	}

	return true;
}

void GRichTextPriv::OnStyleChange(GRichTextEdit::RectType t)
{
	GCss s;
	switch (t)
	{
		case GRichTextEdit::FontFamilyBtn:
		{
			GCss::StringsDef Fam(Values[t].Str());
			s.FontFamily(Fam);
			if (!ChangeSelectionStyle(&s, true))
				StyleDirty.Add(t);
			break;
		}
		case GRichTextEdit::FontSizeBtn:
		{
			double Pt = Values[t].CastDouble();
			s.FontSize(GCss::Len(GCss::LenPt, (float)Pt));
			if (!ChangeSelectionStyle(&s, true))
				StyleDirty.Add(t);
			break;
		}
		case GRichTextEdit::BoldBtn:
		{
			s.FontWeight(GCss::FontWeightBold);
			if (!ChangeSelectionStyle(&s, Values[t].CastBool()))
				StyleDirty.Add(t);
			break;
		}
		case GRichTextEdit::ItalicBtn:
		{
			s.FontStyle(GCss::FontStyleItalic);
			if (!ChangeSelectionStyle(&s, Values[t].CastBool()))
				StyleDirty.Add(t);
			break;
		}
		case GRichTextEdit::UnderlineBtn:
		{
			s.TextDecoration(GCss::TextDecorUnderline);
			if (ChangeSelectionStyle(&s, Values[t].CastBool()))
				StyleDirty.Add(t);
			break;
		}
		case GRichTextEdit::ForegroundColourBtn:
		{
			s.Color(GCss::ColorDef(GCss::ColorRgb, (uint32) Values[t].Value.Int64));
			if (!ChangeSelectionStyle(&s, true))
				StyleDirty.Add(t);
			break;
		}
		case GRichTextEdit::BackgroundColourBtn:
		{
			s.BackgroundColor(GCss::ColorDef(GCss::ColorRgb, (uint32) Values[t].Value.Int64));
			if (!ChangeSelectionStyle(&s, true))
				StyleDirty.Add(t);
			break;
		}
		default:
			break;
	}
}

bool GRichTextPriv::ChangeSelectionStyle(GCss *Style, bool Add)
{
	if (!Selection)
		return false;

	bool Cf = CursorFirst();
	GRichTextPriv::BlockCursor *Start = Cf ? Cursor : Selection;
	GRichTextPriv::BlockCursor *End = Cf ? Selection : Cursor;
	if (Start->Blk == End->Blk)
	{
		// Change style in the same block...
		int Len = End->Offset - Start->Offset;
		if (!Start->Blk->ChangeStyle(NoTransaction, Start->Offset, Len, Style, Add))
			return false;
	}
	else
	{
		// Multi-block style change...

		// 1) Change style on the content to the end of the first block
		Start->Blk->ChangeStyle(NoTransaction, Start->Offset, -1, Style, Add);

		// 2) Change style on blocks between 'Start' and 'End'
		int i = Blocks.IndexOf(Start->Blk);
		if (i >= 0)
		{
			for (++i; Blocks[i] != End->Blk && i < (int)Blocks.Length(); i++)
			{
				GRichTextPriv::Block *&b = Blocks[i];
				if (!b->ChangeStyle(NoTransaction, 0, -1, Style, Add))
					return false;
			}
		}
		else
		{
			return Error(_FL, "Start block has no index.");
		}

		// 3) Change style up to the Cursor in the 'End' block
		if (!End->Blk->ChangeStyle(NoTransaction, 0, End->Offset, Style, Add))
			return false;
	}

	Cursor->Pos.ZOff(-1, -1);
	InvalidateDoc(NULL);
	return true;
}

void GRichTextPriv::PaintBtn(GSurface *pDC, GRichTextEdit::RectType t)
{
	GRect r = Areas[t];
	GVariant &v = Values[t];
	bool Down = v.Type == GV_BOOL && v.Value.Bool;

	SysFont->Colour(LC_TEXT, LC_MED);
	SysFont->Transparent(false);

	GColour Low(96, 96, 96);
	pDC->Colour(Down ? GColour::White : Low);
	pDC->Line(r.x1, r.y2, r.x2, r.y2);
	pDC->Line(r.x2, r.y1, r.x2, r.y2);
	pDC->Colour(Down ? Low : GColour::White);
	pDC->Line(r.x1, r.y1, r.x2, r.y1);
	pDC->Line(r.x1, r.y1, r.x1, r.y2);
	r.Size(1, 1);
	// LgiThinBorder(pDC, r, Down ? EdgeXpSunken : EdgeXpRaised);
	
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
				default:
					break;
			}
			if (!Label) break;
			GDisplayString Ds(SysFont, Label);
			Ds.Draw(pDC, r.x1 + ((r.X()-Ds.X())>>1) + Down, r.y1 + ((r.Y()-Ds.Y())>>1) + Down, &r);
			break;
		}
		default:
			break;
	}
}

bool GRichTextPriv::ClickBtn(GMouse &m, GRichTextEdit::RectType t)
{
	switch (t)
	{
		case GRichTextEdit::FontFamilyBtn:
		{
			List<const char> Fonts;
			if (!GFontSystem::Inst()->EnumerateFonts(Fonts))
				return Error(_FL, "EnumerateFonts failed.");

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
			break;
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
			if (!Cursor || !Cursor->Blk)
				break;

			TextBlock *tb = dynamic_cast<TextBlock*>(Cursor->Blk);
			if (!tb)
				break;

			GArray<StyleText*> st;
			if (!tb->GetTextAt(Cursor->Offset, st))
				break;

			StyleText *a = st.Length() > 1 && st[1]->Element == TAG_A ? st[1] : st.First()->Element == TAG_A ? st[0] : NULL;
			if (a)
			{
				// Edit the existing link...
				GInput i(View, a->Param, "Edit link:", "Link");
				if (i.DoModal())
				{
					a->Param = i.Str;
				}
			}
			else if (Selection)
			{
				// Turn current selection into link
				GInput i(View, NULL, "Edit link:", "Link");
				if (i.DoModal())
				{
					BlockCursor *Start = CursorFirst() ? Cursor : Selection;
					BlockCursor *End = CursorFirst() ? Selection : Cursor;
					if (!Start || !End) return false;
					if (Start->Blk != End->Blk)
					{
						LgiMsg(View, "Selection too large.", "Error");
						return false;
					}
					int Off = Start->Offset;
					int Len = End->Offset - Start->Offset;
					GAutoPtr<GNamedStyle> ns(new GNamedStyle);
					if (ns)
					{
						if (st.Last()->GetStyle())
							*ns = *st.Last()->GetStyle();
						ns->TextDecoration(GCss::TextDecorUnderline);
						ns->Color(GCss::ColorDef(GCss::ColorRgb, GColour::Blue.c32()));
						tb->ChangeStyle(NoTransaction, Off, Len, ns, true);

						if (tb->GetTextAt(Off+1, st))
						{
							st.First()->Element = TAG_A;
							st.First()->Param = i.Str;
						}
					}
				}
			}
			break;
		}
		case GRichTextEdit::RemoveLinkBtn:
		{
			if (!Cursor || !Cursor->Blk)
				break;

			TextBlock *tb = dynamic_cast<TextBlock*>(Cursor->Blk);
			if (!tb)
				break;

			GArray<StyleText*> st;
			if (!tb->GetTextAt(Cursor->Offset, st))
				break;

			StyleText *a = st.Length() > 1 && st[1]->Element == TAG_A ? st[1] : st.First()->Element == TAG_A ? st[0] : NULL;
			if (a)
			{
				// Remove the existing link...
				a->Element = CONTENT;
				a->Param.Empty();

				GAutoPtr<GCss> s(new GCss);
				GNamedStyle *Ns = a->GetStyle();
				if (Ns)
					*s = *Ns;
				if (s->TextDecoration() == GCss::TextDecorUnderline)
					s->DeleteProp(GCss::PropTextDecoration);
				if ((GColour)s->Color() == GColour::Blue)
					s->DeleteProp(GCss::PropColor);

				Ns = AddStyleToCache(s);
				a->SetStyle(Ns);

				tb->LayoutDirty = true;
				InvalidateDoc(NULL);
			}
			break;			
		}
		case GRichTextEdit::RemoveStyleBtn:
		{
			GCss s;
			ChangeSelectionStyle(&s, false);
			break;
		}
		case GRichTextEdit::CapabilityBtn:
		{
			View->OnCloseInstaller();
			break;
		}
		case GRichTextEdit::EmojiBtn:
		{
			GdcPt2 p(Areas[t].x1, Areas[t].y2 + 1);
			View->PointToScreen(p);
			new EmojiMenu(this, p);
			break;
		}
		default:
			return false;
	}

	return true;
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
		#ifdef WIN32
		GDoubleBuffer Buf(pDC, &t);
		#endif
		pDC->Colour(GColour(180, 180, 206));
		pDC->Rectangle(&t);

		GRect r = t;
		r.Size(3, 3);
		#define AllocPx(sz, border) \
			GRect(r.x1, r.y1, r.x1 + (int)(sz) - 1, r.y2); r.x1 += (int)(sz) + border

		Areas[GRichTextEdit::FontFamilyBtn] = AllocPx(100, 6);
		Areas[GRichTextEdit::FontSizeBtn] = AllocPx(40, 6);

		Areas[GRichTextEdit::BoldBtn] = AllocPx(r.Y(), 0);
		Areas[GRichTextEdit::ItalicBtn] = AllocPx(r.Y(), 0);
		Areas[GRichTextEdit::UnderlineBtn] = AllocPx(r.Y(), 6);

		Areas[GRichTextEdit::ForegroundColourBtn] = AllocPx(r.Y()*1.5, 0);
		Areas[GRichTextEdit::BackgroundColourBtn] = AllocPx(r.Y()*1.5, 6);

		{
			GDisplayString Ds(SysFont, TEXT_LINK);
			Areas[GRichTextEdit::MakeLinkBtn] = AllocPx(Ds.X() + 12, 0);
		}
		{
			GDisplayString Ds(SysFont, TEXT_REMOVE_LINK);
			Areas[GRichTextEdit::RemoveLinkBtn] = AllocPx(Ds.X() + 12, 6);
		}
		{
			GDisplayString Ds(SysFont, TEXT_REMOVE_STYLE);
			Areas[GRichTextEdit::RemoveStyleBtn] = AllocPx(Ds.X() + 12, 6);
		}
		{
			GDisplayString Ds(SysFont, TEXT_EMOJI);
			Areas[GRichTextEdit::EmojiBtn] = AllocPx(Ds.X() + 12, 6);
		}

		for (unsigned i = GRichTextEdit::FontFamilyBtn; i < GRichTextEdit::MaxArea; i++)
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

	ScrollOffsetPx = ScrollY ? (int)(ScrollY->Value() * ScrollLinePx) : 0;
	pDC->SetOrigin(-r.x1, -r.y1+ScrollOffsetPx);

	int DrawPx = ScrollOffsetPx + Areas[GRichTextEdit::ContentArea].Y();
	int ExtraPx = DrawPx > DocumentExtent.y ? DrawPx - DocumentExtent.y : 0;

	r.Set(0, 0, DocumentExtent.x-1, DocumentExtent.y-1);

	// Fill the padding...
	GCssTools ct(this, Font);
	r = ct.PaintPadding(pDC, r);

	// Fill the background...
	#if DEBUG_COVERAGE_CHECK
	pDC->Colour(GColour(255, 0, 255));
	#else
	GCss::ColorDef cBack = BackgroundColor();
	// pDC->Colour(cBack.IsValid() ? (GColour)cBack : GColour(LC_WORKSPACE, 24));
	#endif
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

	#if 0 // Outline the line the cursor is on
	if (Cursor)
	{
		pDC->Colour(GColour::Blue);
		pDC->LineStyle(GSurface::LineDot);
		pDC->Box(&Cursor->Line);
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
			"\t<meta name=\"charset\" content=\"utf-8\">\n");
	
	ZeroRefCounts();
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Blocks[i]->IncAllStyleRefs();
	}
	if (GetStyles())
	{
		p.Print("\t<style>\n");
		OutputStyles(p, 1);		
		p.Print("\t</style>\n");
	}

	p.Print("</head>\n"
			"<body>\n");
		
	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		Block *b = Blocks[i];
		b->ToHtml(p);
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
	char Sp[48];
	int SpLen = min(Depth << 1, sizeof(Sp) - 1);
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
				if (ValidStrW(Style))
					LgiAssert(!"Impl me.");
				continue;
				break;
			}
			case TAG_B:
			{
				if (!Style)
					Style.Reset(new GCss);
				if (Style)
					Style->FontWeight(GCss::FontWeightBold);
				break;
			}
			case TAG_A:
			{
				if (!Style)
					Style.Reset(new GCss);
				if (Style)
				{
					Style->TextDecoration(GCss::TextDecorUnderline);
					Style->Color(GCss::ColorDef(GCss::ColorRgb, GColour::Blue.c32()));
				}
				break;
			}
			case TAG_IMG:
			{
				IsBlock = true;
				break;
			}
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
								Style->Parse(s);
						}
					}
				}
			}
		}

		GNamedStyle *CachedStyle = AddStyleToCache(Style);			
		if
		(
			(
				IsBlock
				&&
				ctx.LastChar != '\n'
			)
			||
			c->TagId == TAG_BR
		)
		{
			if (!ctx.Tb)
			{
				Blocks.Add(ctx.Tb = new TextBlock(this));
				if (CachedStyle && ctx.Tb)
					ctx.Tb->SetStyle(CachedStyle);
			}
			if (ctx.Tb)
			{
				const uint32 Nl[] = {'\n', 0};
				ctx.Tb->AddText(NoTransaction, -1, Nl, 1, NULL/*CachedStyle*/);
				ctx.LastChar = '\n';
				ctx.StartOfLine = true;
			}
		}

		bool EndStyleChange = false;

		if (c->TagId == TAG_A)
		{
			ctx.StartOfLine |= ctx.AddText(CachedStyle, c->GetText());
			
			if (ctx.Tb->Txt.Length())
			{
				StyleText *st = ctx.Tb->Txt.Last();

				st->Element = TAG_A;
			
				const char *Link;
				if (c->Get("href", Link))
					st->Param = Link;
			}
		}
		else
		{
			if (IsBlock && ctx.Tb != NULL)
			{
				if (CachedStyle != ctx.Tb->GetStyle())
				{
					// Start a new block because the styles are different...
					EndStyleChange = true;
					Blocks.Add(ctx.Tb = new TextBlock(this));					
					if (CachedStyle)
						ctx.Tb->SetStyle(CachedStyle);
				}
			}

			char16 *Txt = c->GetText();
			if
			(
				Txt
				&&
				(
					!ctx.StartOfLine
					|| 
					ValidStrW(Txt)
				)
			)
			{
				if (!ctx.Tb)
				{
					Blocks.Add(ctx.Tb = new TextBlock(this));
					ctx.Tb->SetStyle(CachedStyle);
				}
			
				ctx.AddText(CachedStyle, Txt);
				ctx.StartOfLine = false;
			}
		}
			
		if (!FromHtml(c, ctx, Style, Depth + 1))
			return false;
			
		if (EndStyleChange)
			ctx.Tb = NULL;
		if (IsBlock)
			ctx.StartOfLine = true;
	}

	return true;
}

bool GRichTextPriv::GetSelection(GArray<char16> &Text)
{
	GArray<uint32> Utf32;

	bool Cf = CursorFirst();
	GRichTextPriv::BlockCursor *Start = Cf ? Cursor : Selection;
	GRichTextPriv::BlockCursor *End = Cf ? Selection : Cursor;
	if (Start->Blk == End->Blk)
	{
		// In the same block... just copy
		int Len = End->Offset - Start->Offset;
		Start->Blk->CopyAt(Start->Offset, Len, &Utf32);
	}
	else
	{
		// Multi-block delete...

		// 1) Copy the content to the end of the first block
		Start->Blk->CopyAt(Start->Offset, -1, &Utf32);

		// 2) Copy any blocks between 'Start' and 'End'
		int i = Blocks.IndexOf(Start->Blk);
		int EndIdx = Blocks.IndexOf(End->Blk);
		if (i >= 0 && EndIdx >= i)
		{
			for (++i; Blocks[i] != End->Blk && i < (int)Blocks.Length(); i++)
			{
				GRichTextPriv::Block *&b = Blocks[i];
				b->CopyAt(0, -1, &Utf32);
			}
		}
		else return Error(_FL, "Blocks missing index: %i, %i.", i, EndIdx);

		// 3) Delete any text up to the Cursor in the 'End' block
		End->Blk->CopyAt(0, End->Offset, &Utf32);
	}

	char16 *w = (char16*)LgiNewConvertCp(LGI_WideCharset, &Utf32[0], "utf-32", Utf32.Length() * sizeof(uint32));
	if (!w)
		return Error(_FL, "Failed to convert %i utf32 to wide.", Utf32.Length());

	Text.Add(w, Strlen(w));
	Text.Add(0);
	return true;
}

#ifdef _DEBUG
void GRichTextPriv::DumpNodes(GTree *Root)
{
	if (Cursor)
	{
		GTreeItem *ti = new GTreeItem;
		ti->SetText("Cursor");
		PrintNode(ti, "Offset=%i", Cursor->Offset);
		PrintNode(ti, "Pos=%s", Cursor->Pos.GetStr());
		PrintNode(ti, "LineHint=%i", Cursor->LineHint);
		PrintNode(ti, "Blk=%i", Cursor->Blk ? Blocks.IndexOf(Cursor->Blk) : -2);
		Root->Insert(ti);
	}
	if (Selection)
	{
		GTreeItem *ti = new GTreeItem;
		ti->SetText("Selection");
		PrintNode(ti, "Offset=%i", Selection->Offset);
		PrintNode(ti, "Pos=%s", Selection->Pos.GetStr());
		PrintNode(ti, "LineHint=%i", Selection->LineHint);
		PrintNode(ti, "Blk=%i", Selection->Blk ? Blocks.IndexOf(Selection->Blk) : -2);
		Root->Insert(ti);
	}

	for (unsigned i=0; i<Blocks.Length(); i++)
	{
		GTreeItem *ti = new GTreeItem;
		Block *b = Blocks[i];
		b->DumpNodes(ti);

		GString s;
		s.Printf("[%i] %s", i, ti->GetText());
		ti->SetText(s);

		Root->Insert(ti);
	}
}

GTreeItem *PrintNode(GTreeItem *Parent, const char *Fmt, ...)
{
	GTreeItem *i = new GTreeItem;
	GString s;

	va_list Arg;
	va_start(Arg, Fmt);
	int Ch = s.Printf(Arg, Fmt);
	va_end(Arg);
	s = s.Replace("\n", "\\n");

	i->SetText(s);
	Parent->Insert(i);
	return i;
}

#endif

