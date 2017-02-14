#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"
#include "GdcTools.h"

class ImageLoader : public GEventTargetThread, public Progress
{
	GString File;
	GEventSinkI *Sink;
	GSurface *Img;
	GAutoPtr<GFilter> Filter;
	int64 PrevY;
	bool SurfaceSent;

public:
	ImageLoader(GEventSinkI *s) : GEventTargetThread("ImageLoader")
	{
		Sink = s;
		PrevY = 0;
		Img = NULL;
		SurfaceSent = false;
	}

	void Value(int64 v)
	{
		Progress::Value(v);

		if (!SurfaceSent)
		{
			SurfaceSent = true;
			Sink->PostEvent(M_IMAGE_SET_SURFACE, (GMessage::Param)Img);
		}

		if (v - PrevY > 16)
		{
			PrevY = v;
			Sink->PostEvent(M_IMAGE_PROGRESS, (GMessage::Param)v);
		}
	}

	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_IMAGE_LOAD_FILE:
			{
				GAutoPtr<GString> Str((GString*)Msg->A());
				File = *Str;
				
				if (!Filter.Reset(GFilterFactory::New(File, O_READ, NULL)))
					return Sink->PostEvent(M_IMAGE_ERROR);

				GFile In;
				if (!In.Open(File, O_READ))
					return Sink->PostEvent(M_IMAGE_ERROR);

				if (!(Img = new GMemDC))
					return Sink->PostEvent(M_IMAGE_ERROR);

				Filter->SetProgress(this);

				GFilter::IoStatus Status = Filter->ReadImage(Img, &In);
				if (Status != GFilter::IoSuccess)
					return Sink->PostEvent(M_IMAGE_ERROR);

				if (!SurfaceSent)
					Sink->PostEvent(M_IMAGE_SET_SURFACE, (GMessage::Param)Img);

				Sink->PostEvent(M_IMAGE_FINISHED);
				break;
			}			
		}

		return 0;
	}
};

GRichTextPriv::ImageBlock::ImageBlock(GRichTextPriv *priv) : Block(priv)
{
	LayoutDirty = false;
	Pos.ZOff(-1, -1);
	Style = NULL;
	Size.x = 200;
	Size.y = 64;
	Scale = 1;

	Margin.ZOff(0, 0);
	Border.ZOff(0, 0);
	Padding.ZOff(0, 0);
}

GRichTextPriv::ImageBlock::ImageBlock(const ImageBlock *Copy) : Block(Copy->d)
{
	LayoutDirty = true;
	SourceImg.Reset(new GMemDC(Copy->SourceImg));
	Size = Copy->Size;

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

bool GRichTextPriv::ImageBlock::Load(const char *Src)
{
	if (Src)
		Source = Src;
	if (!Thread.Reset(new GEventSinkPtr(new ImageLoader(this), true)))
		return false;

	return Thread->PostEvent(M_IMAGE_LOAD_FILE, (GMessage::Param)new GString(Src));
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
	if ((Style = s))
	{
		GFont *Fnt = d->GetFont(s);
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

int GRichTextPriv::ImageBlock::Length()
{
	return 1;
}

bool GRichTextPriv::ImageBlock::ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media)
{
	if (Media)
	{
		LgiAssert(!"Impl me.");
	}
	else
	{
		s.Print("<img src='%s'>\n", Source.Get());
	}
	
	return true;
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

	Cursor->Pos = ImgPos;
	Cursor->Line = Pos;
	if (Cursor->Offset == 0)
	{
		Cursor->Pos.x2 = Cursor->Pos.x1 + 1;
	}
	else if (Cursor->Offset == 1)
	{
		Cursor->Pos.x1 = Cursor->Pos.x2 - 1;
	}

	return true;
}

bool GRichTextPriv::ImageBlock::HitTest(HitTestResult &htr)
{
	if (htr.In.y < Pos.y1 || htr.In.y > Pos.y2)
		return false;

	htr.Near = false;
	htr.LineHint = 0;

	int Cx = ImgPos.x1 + (ImgPos.X() / 2);
	if (htr.In.x < Cx)
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

	// After image selection end point
	if (CurEndPoint < EndPoints &&
		EndPoint[CurEndPoint] == 0)
	{
		Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
		CurEndPoint++;
	}

	bool ImgSelected = Ctx.Type == Selected;

	GSurface *Src = DisplayImg ? DisplayImg : SourceImg;
	if (Src)
	{
		Ctx.pDC->Blt(r.x1, r.y1, Src);
	}
	else
	{
		// Drag missing image...
		r = ImgPos;
		GColour cBack(245, 245, 245);
		Ctx.pDC->Colour(ImgSelected ? cBack.Mix(Ctx.Colours[Selected].Back) : cBack);
		Ctx.pDC->Rectangle(&r);

		Ctx.pDC->Colour(LC_LOW, 24);
		uint Ls = Ctx.pDC->LineStyle(GSurface::LineAlternate);
		Ctx.pDC->Box(&r);
		Ctx.pDC->LineStyle(Ls);

		int Cx = r.x1 + (r.X() >> 1);
		int Cy = r.y1 + (r.Y() >> 1);
		Ctx.pDC->Colour(GColour::Red);
		int Sz = 5;
		Ctx.pDC->Line(Cx - Sz, Cy - Sz, Cx + Sz, Cy + Sz);
		Ctx.pDC->Line(Cx - Sz, Cy - Sz + 1, Cx + Sz - 1, Cy + Sz);
		Ctx.pDC->Line(Cx - Sz + 1, Cy - Sz, Cx + Sz, Cy + Sz - 1);

		Ctx.pDC->Line(Cx + Sz, Cy - Sz, Cx - Sz, Cy + Sz);
		Ctx.pDC->Line(Cx + Sz - 1, Cy - Sz, Cx - Sz, Cy + Sz - 1);
		Ctx.pDC->Line(Cx + Sz, Cy - Sz + 1, Cx - Sz + 1, Cy + Sz);
	}

	// After image selection end point
	if (CurEndPoint < EndPoints &&
		EndPoint[CurEndPoint] == 1)
	{
		Ctx.Type = Ctx.Type == Selected ? Unselected : Selected;
		CurEndPoint++;
	}

	if (Ctx.Type == Selected)
	{
		Ctx.pDC->Colour(Ctx.Colours[Selected].Back);
		Ctx.pDC->Rectangle(ImgPos.x2 + 1, ImgPos.y1, ImgPos.x2 + 7, ImgPos.y2);
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

	ImgPos.x1 = Pos.x1 + Padding.x1;
	ImgPos.y1 = Pos.y1 + Padding.y1;	

	ImgPos.x2 = ImgPos.x1 + Size.x - 1;
	ImgPos.y2 = ImgPos.y1 + Size.y - 1;

	int Px2 = ImgPos.x2 + Padding.x2;
	if (Px2 < Pos.x2)
		Pos.x2 = ImgPos.x2 + Padding.x2;
	Pos.y2 = ImgPos.y2 + Padding.y2;

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

GMessage::Result GRichTextPriv::ImageBlock::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_IMAGE_ERROR:
		{
			Thread.Reset();
			break;
		}
		case M_IMAGE_SET_SURFACE:
		{
			if (SourceImg.Reset((GSurface*)Msg->A()))
			{
				int ViewX = d->Areas[GRichTextEdit::ContentArea].X();
				int MaxX = (int) (ViewX * 0.9);
				if (SourceImg->X() > MaxX)
				{
					double Ratio = (double)SourceImg->X() / MaxX;
					Scale = (int)ceil(Ratio);

					Size.x = (int)ceil((double)SourceImg->X() / Scale);
					Size.y = (int)ceil((double)SourceImg->Y() / Scale);
					if (DisplayImg.Reset(new GMemDC(Size.x, Size.y, SourceImg->GetColourSpace())))
					{
						DisplayImg->Colour(0, 32);
						DisplayImg->Rectangle();
					}
				}
			}
			break;
		}
		case M_IMAGE_PROGRESS:
		{
			break;
		}
		case M_IMAGE_FINISHED:
		{
			LayoutDirty = true;
			d->InvalidateDoc(NULL);
			Thread.Reset();

			if (DisplayImg)
				ResampleDC(DisplayImg, SourceImg, NULL, NULL);
			break;
		}
	}

	return 0;
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
