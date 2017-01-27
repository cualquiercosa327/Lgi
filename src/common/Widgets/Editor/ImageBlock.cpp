#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"

GRichTextPriv::ImageBlock::ImageBlock(GRichTextPriv *priv) : Block(priv)
{
	LayoutDirty = false;
	Pos.ZOff(-1, -1);
	Style = NULL;

	Margin.ZOff(0, 0);
	Border.ZOff(0, 0);
	Padding.ZOff(0, 0);
}

GRichTextPriv::ImageBlock::ImageBlock(const ImageBlock *Copy) : Block(Copy->d)
{
	LayoutDirty = true;
	SourceImg.Reset(new GMemDC(Copy->SourceImg));

	Margin = Copy->Margin;
	Border = Copy->Border;
	Padding = Copy->Padding;
}

GRichTextPriv::ImageBlock::~ImageBlock()
{
	LgiAssert(Cursors == 0);
}

bool GRichTextPriv::ImageBlock::IsValid()
{
	return true;
}

int GRichTextPriv::ImageBlock::GetLines()
{
	return 1;
}

bool GRichTextPriv::ImageBlock::OffsetToLine(int Offset, int *ColX, GArray<int> *LineY)
{
	if (ColX)
		*ColX = Offset > 0;
	if (LineY)
		LineY->Add(0);
	return true;
}

int GRichTextPriv::ImageBlock::LineToOffset(int Line)
{
	return 0;
}

void GRichTextPriv::ImageBlock::Dump()
{
}

GNamedStyle *GRichTextPriv::ImageBlock::GetStyle(int At)
{
	return Style;
}

void GRichTextPriv::ImageBlock::SetStyle(GNamedStyle *s)
{
	Style = s;
	LayoutDirty = true;
}

int GRichTextPriv::ImageBlock::Length()
{
	return 1;
}

bool GRichTextPriv::ImageBlock::ToHtml(GStream &s)
{
	return false;
}

bool GRichTextPriv::ImageBlock::GetPosFromIndex(BlockCursor *Cursor)
{
	if (!Cursor)
		return d->Error(_FL, "No cursor param.");

	if (LayoutDirty)
	{
		Cursor->Pos.ZOff(-1, -1);	// This is valid behaviour... need to 
									// wait for layout before getting cursor
									// position.
		return false;
	}

	Cursor->Pos = Pos;
	Cursor->Line = Pos;
	if (Cursor->Offset == 0)
	{
		Cursor->Pos.x2 = Cursor->Pos.x1 + 1;
	}
	else if (Cursor->Offset == 1)
	{
		Cursor->Pos.x1 = Cursor->Pos.x2;
		Cursor->Pos.x2++;
	}

	return true;
}

bool GRichTextPriv::ImageBlock::HitTest(HitTestResult &htr)
{
	if (htr.In.y < Pos.y1 || htr.In.y > Pos.y2)
		return false;

	htr.Near = false;
	htr.LineHint = 0;
	if (htr.In.x < Pos.X() / 2)
		htr.Idx = 0;
	else
		htr.Idx = 1;

	return true;
}

void GRichTextPriv::ImageBlock::OnPaint(PaintContext &Ctx)
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

	GSurface *Src = SourceImg ? SourceImg : DisplayImg;
	if (Src)
	{
		Ctx.pDC->Blt(r.x1, r.y1, Src, &r);
	}
	else
	{
		Ctx.pDC->Colour(Ctx.Colours[Unselected].Back);
		Ctx.pDC->Rectangle(&r);

		Ctx.pDC->Colour(LC_MED, 24);
		uint Ls = Ctx.pDC->LineStyle(GSurface::LineAlternate);
		Ctx.pDC->Box(&r);
		Ctx.pDC->LineStyle(Ls);

		int Cx = r.x1 + (r.X() >> 1);
		int Cy = r.y1 + (r.Y() >> 1);
		Ctx.pDC->Colour(GColour::Red);
		int Sz = 5;
		Ctx.pDC->Line(Cx - Sz, Cy - Sz, Cx + Sz, Cy + Sz);
		Ctx.pDC->Line(Cx + Sz, Cy - Sz, Cx - Sz, Cy + Sz);
	}
}

bool GRichTextPriv::ImageBlock::OnLayout(Flow &flow)
{
	LayoutDirty = false;

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

	GSurface *Src = SourceImg ? SourceImg : DisplayImg;
	if (Src)
		Pos.y2 = Pos.y1 + Src->Y() - 1;
	else
		Pos.y2 = Pos.y1 + 39;

	flow.CurY = Pos.y2 + 1 + Margin.y2 + Border.y2 + Padding.y2;
	flow.Left -= Margin.x1 + Border.x1 + Padding.x1;
	flow.Right += Margin.x2 + Border.x2 + Padding.x2;

	return true;
}

int GRichTextPriv::ImageBlock::GetTextAt(uint32 Offset, GArray<StyleText*> &t)
{
	// No text to get
	return 0;
}

int GRichTextPriv::ImageBlock::CopyAt(int Offset, int Chars, GArray<uint32> *Text)
{
	// No text to copy
	return 0;
}

bool GRichTextPriv::ImageBlock::Seek(SeekType To, BlockCursor &Cursor)
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

int GRichTextPriv::ImageBlock::FindAt(int StartIdx, const uint32 *Str, GFindReplaceCommon *Params)
{
	// No text to find in
	return -1;
}

void GRichTextPriv::ImageBlock::IncAllStyleRefs()
{
	if (Style)
		Style->RefCount++;
}

bool GRichTextPriv::ImageBlock::AddText(Transaction *Trans, int AtOffset, const uint32 *Str, int Chars, GNamedStyle *Style)
{
	// Can't add text to image block
	return false;
}

bool GRichTextPriv::ImageBlock::ChangeStyle(Transaction *Trans, int Offset, int Chars, GCss *Style, bool Add)
{
	// No styles to change...
	return false;
}

int GRichTextPriv::ImageBlock::DeleteAt(Transaction *Trans, int BlkOffset, int Chars, GArray<uint32> *DeletedText)
{
	// No text to delete...
	return false;
}

bool GRichTextPriv::ImageBlock::DoCase(Transaction *Trans, int StartIdx, int Chars, bool Upper)
{
	// No text to change case...
	return false;
}

#ifdef _DEBUG
void GRichTextPriv::ImageBlock::DumpNodes(GTreeItem *Ti)
{
	GString s;
	s.Printf("ImageBlock style=%s", Style?Style->Name.Get():NULL);
	Ti->SetText(s);
}
#endif

