#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"
#include "GDocView.h"

GRichTextPriv::HorzRuleBlock::HorzRuleBlock(GRichTextPriv *priv) : Block(priv)
{
	IsDeleted = false;
}

GRichTextPriv::HorzRuleBlock::HorzRuleBlock(const HorzRuleBlock *Copy) : Block(Copy->d)
{
	IsDeleted = Copy->IsDeleted;
}

GRichTextPriv::HorzRuleBlock::~HorzRuleBlock()
{
	LgiAssert(Cursors == 0);
}

bool GRichTextPriv::HorzRuleBlock::IsValid()
{
	return true;
}

int GRichTextPriv::HorzRuleBlock::GetLines()
{
	return 1;
}

bool GRichTextPriv::HorzRuleBlock::OffsetToLine(ssize_t Offset, int *ColX, GArray<int> *LineY)
{
	if (ColX)
		*ColX = Offset > 0;
	if (LineY)
		LineY->Add(0);
	return true;
}

int GRichTextPriv::HorzRuleBlock::LineToOffset(int Line)
{
	return 0;
}

void GRichTextPriv::HorzRuleBlock::Dump()
{
}

GNamedStyle *GRichTextPriv::HorzRuleBlock::GetStyle(ssize_t At)
{
	return NULL;
}

void GRichTextPriv::HorzRuleBlock::SetStyle(GNamedStyle *s)
{
}

ssize_t GRichTextPriv::HorzRuleBlock::Length()
{
	return IsDeleted ? 0 : 1;
}

bool GRichTextPriv::HorzRuleBlock::ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media)
{
	s.Print("<hr>\n");
	return true;
}

bool GRichTextPriv::HorzRuleBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor)
		return d->Error(_FL, "No cursor param.");

	Cursor->Pos = Pos;
	Cursor->Line = Pos;
	if (Cursor->Offset == 0)
		Cursor->Pos.x2 = Cursor->Pos.x1 + 1;
	else
		Cursor->Pos.x1 = Cursor->Pos.x2 - 1;

	return true;
}

bool GRichTextPriv::HorzRuleBlock::HitTest(HitTestResult &htr)
{
	if (htr.In.y < Pos.y1 || htr.In.y > Pos.y2)
		return false;

	htr.Near = false;
	htr.LineHint = 0;

	int Cx = Pos.x1 + (Pos.X() / 2);
	if (htr.In.x < Cx)
		htr.Idx = 0;
	else
		htr.Idx = 1;

	return true;
}

void GRichTextPriv::HorzRuleBlock::OnPaint(PaintContext &Ctx)
{
	bool HrSelected = Ctx.SelectBeforePaint(this);

	GColour Fore, Back = Ctx.Back();
	Fore = Ctx.Fore().Mix(Back, 0.75f);
	Ctx.pDC->Colour(Back);
	Ctx.pDC->Rectangle(&Pos);
	Ctx.pDC->Colour(Fore);

	int Cy = Pos.y1 + (Pos.Y() >> 1);
	Ctx.pDC->Rectangle(Pos.x1, Cy-1, Pos.x2, Cy);

	if (Ctx.Cursor != NULL &&
		Ctx.Cursor->Blk == (Block*)this &&
		Ctx.Cursor->Blink)
	{
		GRect &p = Ctx.Cursor->Pos;
		Ctx.pDC->Colour(Ctx.Fore());
		Ctx.pDC->Rectangle(&p);
	}

	Ctx.SelectAfterPaint(this);
}

bool GRichTextPriv::HorzRuleBlock::OnLayout(Flow &flow)
{
	Pos.x1 = flow.Left;
	Pos.y1 = flow.CurY;
	Pos.x2 = flow.Right;
	Pos.y2 = flow.CurY + 15; // Start with a 16px height.
	flow.CurY = Pos.y2 + 1;
	return true;
}

ssize_t GRichTextPriv::HorzRuleBlock::GetTextAt(ssize_t Offset, GArray<StyleText*> &t)
{
	return 0;
}

ssize_t GRichTextPriv::HorzRuleBlock::CopyAt(ssize_t Offset, ssize_t Chars, GArray<uint32> *Text)
{
	return 0;
}

bool GRichTextPriv::HorzRuleBlock::Seek(SeekType To, BlockCursor &Cursor)
{
	switch (To)
	{
		case SkLineStart:
		{
			Cursor.Offset = 0;
			Cursor.LineHint = 0;
			break;
		}
		case SkLineEnd:
		{
			Cursor.Offset = 1;
			Cursor.LineHint = 0;
			break;
		}
		case SkLeftChar:
		{
			if (Cursor.Offset != 1)
				return false;
			Cursor.Offset = 0;
			Cursor.LineHint = 0;
			break;
		}
		case SkRightChar:
		{
			if (Cursor.Offset != 0)
				return false;
			Cursor.Offset = 1;
			Cursor.LineHint = 0;
			break;
		}
		default:
		{
			return false;
			break;
		}
	}

	return true;
}

ssize_t GRichTextPriv::HorzRuleBlock::FindAt(ssize_t StartIdx, const uint32 *Str, GFindReplaceCommon *Params)
{
	return 0;
}

void GRichTextPriv::HorzRuleBlock::IncAllStyleRefs()
{
}

void GRichTextPriv::HorzRuleBlock::SetSpellingErrors(GArray<GSpellCheck::SpellingError> &Errors)
{
}

bool GRichTextPriv::HorzRuleBlock::DoContext(GSubMenu &s, GdcPt2 Doc, ssize_t Offset, bool Spelling)
{
	return false;
}

#ifdef _DEBUG
void GRichTextPriv::HorzRuleBlock::DumpNodes(GTreeItem *Ti)
{
	Ti->SetText("HorzRuleBlock");
}
#endif

GRichTextPriv::Block *GRichTextPriv::HorzRuleBlock::Clone()
{
	return new HorzRuleBlock(this);
}

GMessage::Result GRichTextPriv::HorzRuleBlock::OnEvent(GMessage *Msg)
{
	return 0;
}

bool GRichTextPriv::HorzRuleBlock::AddText(Transaction *Trans, ssize_t AtOffset, const uint32 *Str, ssize_t Chars, GNamedStyle *Style)
{
	return false;
}

bool GRichTextPriv::HorzRuleBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, GCss *Style, bool Add)
{
	return false;
}

ssize_t GRichTextPriv::HorzRuleBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, GArray<uint32> *DeletedText)
{
	IsDeleted = BlkOffset == 0;
	if (IsDeleted)
		return true;

	return false;
}

bool GRichTextPriv::HorzRuleBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
{
	return false;
}

GRichTextPriv::Block *GRichTextPriv::HorzRuleBlock::Split(Transaction *Trans, ssize_t AtOffset)
{
	return NULL;
}

