#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"
#include "GdcTools.h"

#define TIMEOUT_LOAD_PROGRESS		100 // ms

int ImgScales[] = { 15, 25, 50, 75, 100 };

class ImageLoader : public GEventTargetThread, public Progress
{
	GString File;
	GEventSinkI *Sink;
	GSurface *Img;
	GAutoPtr<GFilter> Filter;
	bool SurfaceSent;
	int64 Ts;
	GAutoPtr<GFile> In;

public:
	ImageLoader(GEventSinkI *s) : GEventTargetThread("ImageLoader")
	{
		Sink = s;
		Img = NULL;
		SurfaceSent = false;
		Ts = 0;
	}

	~ImageLoader()
	{
		Cancel(true);
	}

	void Value(int64 v)
	{
		Progress::Value(v);

		if (!SurfaceSent)
		{
			SurfaceSent = true;
			Sink->PostEvent(M_IMAGE_SET_SURFACE, (GMessage::Param)Img, (GMessage::Param)In.Release());
		}

		int64 Now = LgiCurrentTime();
		if (Now - Ts > TIMEOUT_LOAD_PROGRESS)
		{
			Ts = Now;
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

				if (!In.Reset(new GFile) ||
					!In->Open(File, O_READ))
					return Sink->PostEvent(M_IMAGE_ERROR);

				if (!(Img = new GMemDC))
					return Sink->PostEvent(M_IMAGE_ERROR);

				Filter->SetProgress(this);

				Ts = LgiCurrentTime();
				GFilter::IoStatus Status = Filter->ReadImage(Img, In);
				if (Status != GFilter::IoSuccess)
					return Sink->PostEvent(M_IMAGE_ERROR);

				if (!SurfaceSent)
					Sink->PostEvent(M_IMAGE_SET_SURFACE, (GMessage::Param)Img, (GMessage::Param)In.Release());

				Sink->PostEvent(M_IMAGE_FINISHED);
				break;
			}
			case M_IMAGE_LOAD_STREAM:
			{
				GAutoPtr<GStreamI> Stream((GStreamI*)Msg->A());
				GAutoPtr<GString> FileName((GString*)Msg->B());
				if (!Stream)
					break;
				
				GMemStream Mem(Stream, 0, -1);				
				if (!Filter.Reset(GFilterFactory::New(FileName ? *FileName : 0, O_READ, (const uchar*)Mem.GetBasePtr())))
					return Sink->PostEvent(M_IMAGE_ERROR);

				if (!(Img = new GMemDC))
					return Sink->PostEvent(M_IMAGE_ERROR);

				Filter->SetProgress(this);

				Ts = LgiCurrentTime();
				GFilter::IoStatus Status = Filter->ReadImage(Img, &Mem);
				if (Status != GFilter::IoSuccess)
					return Sink->PostEvent(M_IMAGE_ERROR);

				if (!SurfaceSent)
					Sink->PostEvent(M_IMAGE_SET_SURFACE, (GMessage::Param)Img, (GMessage::Param)In.Release());

				Sink->PostEvent(M_IMAGE_FINISHED);
				break;
			}
			case M_IMAGE_RESAMPLE:
			{
				GSurface *Dst = (GSurface*) Msg->A();
				GSurface *Src = (GSurface*) Msg->B();
				if (Src && Dst)
				{
					ResampleDC(Dst, Src, NULL, NULL);
					Sink->PostEvent(M_IMAGE_RESAMPLE);
				}
				break;
			}
			case M_IMAGE_COMPRESS:
			{
				GSurface *img = (GSurface*)Msg->A();
				GRichTextPriv::ImageBlock::ScaleInf *si = (GRichTextPriv::ImageBlock::ScaleInf*)Msg->B();
				if (!img || !si)
				{
					Sink->PostEvent(M_IMAGE_ERROR, (GMessage::Param) new GString("Invalid pointer."));
					break;
				}
				
				GAutoPtr<GFilter> f(GFilterFactory::New("a.jpg", O_READ, NULL));
				if (!f)
				{
					Sink->PostEvent(M_IMAGE_ERROR, (GMessage::Param) new GString("No JPEG filter available."));
					break;
				}

				GAutoPtr<GSurface> scaled;
				if (img->X() != si->Sz.x ||
					img->Y() != si->Sz.y)
				{
					if (!scaled.Reset(new GMemDC(si->Sz.x, si->Sz.y, img->GetColourSpace())))
						break;
					ResampleDC(scaled, img, NULL, NULL);
					img = scaled;
				}

				GXmlTag Props;
				f->Props = &Props;
				Props.SetAttr(LGI_FILTER_QUALITY, RICH_TEXT_RESIZED_JPEG_QUALITY);
				
				GAutoPtr<GMemStream> jpg(new GMemStream(1024));
				if (!f->WriteImage(jpg, img))
				{
					Sink->PostEvent(M_IMAGE_ERROR, (GMessage::Param) new GString("Image compression failed."));
					break;
				}

				Sink->PostEvent(M_IMAGE_COMPRESS, (GMessage::Param)jpg.Release(), (GMessage::Param)si);
				break;
			}
			case M_IMAGE_ROTATE:
			{
				GSurface *Img = (GSurface*)Msg->A();
				if (!Img)
					break;

				RotateDC(Img, Msg->B() == 1 ? 90 : 270);
				Sink->PostEvent(M_IMAGE_ROTATE);
				break;
			}
			case M_IMAGE_FLIP:
			{
				GSurface *Img = (GSurface*)Msg->A();
				if (!Img)
					break;

				if (Msg->B() == 1)
					FlipXDC(Img);
				else
					FlipYDC(Img);
				Sink->PostEvent(M_IMAGE_FLIP);
				break;
			}
			case M_CLOSE:
			{
				EndThread();
				break;
			}
		}

		return 0;
	}
};

GRichTextPriv::ImageBlock::ImageBlock(GRichTextPriv *priv) : Block(priv)
{
	ThreadHnd = 0;
	LayoutDirty = false;
	Pos.ZOff(-1, -1);
	Style = NULL;
	Size.x = 200;
	Size.y = 64;
	Scale = 1;
	SourceValid.ZOff(-1, -1);
	ResizeIdx = -1;

	Margin.ZOff(0, 0);
	Border.ZOff(0, 0);
	Padding.ZOff(0, 0);
}

GRichTextPriv::ImageBlock::ImageBlock(const ImageBlock *Copy) : Block(Copy->d)
{
	ThreadHnd = 0;
	LayoutDirty = true;
	SourceImg.Reset(new GMemDC(Copy->SourceImg));
	Size = Copy->Size;

	Margin = Copy->Margin;
	Border = Copy->Border;
	Padding = Copy->Padding;
}

GRichTextPriv::ImageBlock::~ImageBlock()
{
	if (ThreadHnd)
		PostThreadEvent(ThreadHnd, M_CLOSE);
	LgiAssert(Cursors == 0);
}

bool GRichTextPriv::ImageBlock::IsValid()
{
	return true;
}

bool GRichTextPriv::ImageBlock::SetImage(GAutoPtr<GSurface> Img)
{
	SourceImg = Img;
	if (!SourceImg)
		return false;

	Scales.Length(CountOf(ImgScales));
	for (int i=0; i<CountOf(ImgScales); i++)
	{
		ScaleInf &si = Scales[i];
		si.Sz.x = SourceImg->X() * ImgScales[i] / 100;
		si.Sz.y = SourceImg->Y() * ImgScales[i] / 100;
		si.Percent = ImgScales[i];
	
		if (si.Sz.x == SourceImg->X() &&
			si.Sz.y == SourceImg->Y())
		{
			ResizeIdx = i;
		}
	}

	LayoutDirty = true;
	UpdateDisplayImg();
	if (DisplayImg)
	{
		// Update the display image by scaling it from the source...
		PostThreadEvent(GetThreadHandle(),
						M_IMAGE_RESAMPLE,
						(GMessage::Param) DisplayImg.Get(),
						(GMessage::Param) SourceImg.Get());

	}
	else LayoutDirty = true;

	// Also create a JPG for the current scale (needed before 
	// we save to HTML).
	if (ResizeIdx >= 0 && ResizeIdx < (int)Scales.Length())
	{
		ScaleInf &si = Scales[ResizeIdx];
		PostThreadEvent(GetThreadHandle(), M_IMAGE_COMPRESS, (GMessage::Param)SourceImg.Get(), (GMessage::Param)&si);
	}
	
	return true;
}

bool GRichTextPriv::ImageBlock::Load(const char *Src)
{
	if (Src)
		Source = Src;

	GAutoPtr<GStreamI> Stream;
	GString FileName;
	
	GString::Array a = Source.Strip().Split(":", 1);
	if (a.Length() > 1 &&
		a[0].Equals("cid"))
	{
		GDocumentEnv *Env = d->View->GetEnv();
		if (!Env)
			return false;
		
		GDocumentEnv::LoadJob *j = Env->NewJob();
		if (!j)
			return false;
		
		j->Uri.Reset(NewStr(Source));
		j->Env = Env;
		j->Pref = GDocumentEnv::LoadJob::FmtStream;
		j->UserUid = d->View->GetDocumentUid();

		GDocumentEnv::LoadType Result = Env->GetContent(j);
		if (Result == GDocumentEnv::LoadImmediate)
		{
			if (j->Stream)
				Stream = j->Stream;
			else if (j->Filename)
				FileName = j->Filename;
			else if (j->pDC)
			{
				SourceImg = j->pDC;
				return true;
			}
		}
		else if (Result == GDocumentEnv::LoadDeferred)
		{
			LgiAssert(!"Impl me?");
		}
		
		DeleteObj(j);
	}
	else if (FileExists(Source))
	{
		FileName = Source;
	}
	else
		return false;

	if (!FileName && !Stream)
		return false;

	ImageLoader *il = new ImageLoader(this);
	if (!il)
		return false;
	ThreadHnd = il->GetHandle();
	LgiAssert(ThreadHnd > 0);

	if (Stream)
		return PostThreadEvent(ThreadHnd, M_IMAGE_LOAD_STREAM, (GMessage::Param)Stream.Release(), (GMessage::Param) (FileName ? new GString(FileName) : NULL));
	
	if (FileName)
		return PostThreadEvent(ThreadHnd, M_IMAGE_LOAD_FILE, (GMessage::Param)new GString(FileName));
	
	return false;
}

int GRichTextPriv::ImageBlock::GetLines()
{
	return 1;
}

bool GRichTextPriv::ImageBlock::OffsetToLine(ssize_t Offset, int *ColX, GArray<int> *LineY)
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

GNamedStyle *GRichTextPriv::ImageBlock::GetStyle(ssize_t At)
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

ssize_t GRichTextPriv::ImageBlock::Length()
{
	return 1;
}

bool GRichTextPriv::ImageBlock::ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media)
{
	if (Media)
	{
		bool ValidSourceFile = FileExists(Source);
		GDocView::ContentMedia &Cm = Media->New();
		
		int Idx = LgiRand() % 10000;
		Cm.Id.Printf("%u@memecode.com", Idx);

		if (ValidSourceFile)
		{
			Cm.FileName = LgiGetLeaf(Source);
			GAutoString mt = LgiApp->GetFileMimeType(Source);
			Cm.MimeType = mt.Get();
		}
		else
		{
			Cm.FileName.Printf("img%u.jpg", Idx);
			Cm.MimeType = "image/jpeg";
		}
		
		GString Style;
		
		LgiAssert(Cm.MimeType != NULL);
		
		ScaleInf *Si = ResizeIdx >= 0 && ResizeIdx < (int)Scales.Length() ? &Scales[ResizeIdx] : NULL;
		if (Si && Si->Jpg)
		{
			// Attach a copy of the resized jpeg...
			Si->Jpg->SetPos(0);
			Cm.Stream.Reset(new GMemStream(Si->Jpg, 0, -1));
		}
		else if (ValidSourceFile)
		{
			// Attach the original file...
			GFile *f = new GFile;
			if (f)
			{
				if (f->Open(Source, O_READ))
				{
					Cm.Stream.Reset(f);
				}
				else
				{
					delete f;
					LgiTrace("%s:%i - Failed to open link image '%s'.\n", _FL, Source.Get());
				}
			}
		}
		else
		{
			LgiTrace("%s:%i - No source or JPEG for saving image to HTML.\n", _FL);
			return false;
		}

		if (DisplayImg &&
			SourceImg &&
			DisplayImg->X() != SourceImg->X())
		{
			int Dx = DisplayImg->X();
			int Sx = SourceImg->X();
			Style.Printf(" style=\"width:%.0f%%\"", (double)Dx * 100 / Sx);
		}
		
		if (Cm.Stream)
		{
			s.Print("<img%s src='", Style ? Style.Get() : "");
			if (d->HtmlLinkAsCid)
				s.Print("cid:%s", Cm.Id.Get());
			else
				s.Print("%s", Cm.FileName.Get());
			s.Print("'>\n");
			return true;
		}
	}

	s.Print("<img src='%s'>\n", Source.Get());
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
	// int CharPos = 0;
	int EndPoints = 0;
	ssize_t EndPoint[2] = {-1, -1};
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
			ssize_t ep = EndPoint[0];
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

	if (!DisplayImg &&
		SourceImg &&
		SourceImg->X() > r.X())
	{
		UpdateDisplayImg();
	}

	GSurface *Src = DisplayImg ? DisplayImg : SourceImg;
	if (Src)
	{
		if (SourceValid.Valid())
		{
			GRect Bounds(0, 0, Size.x-1, Size.y-1);
			Bounds.Offset(r.x1, r.y1);

			Ctx.pDC->Colour(LC_MED, 24);
			Ctx.pDC->Box(&Bounds);
			Bounds.Size(1, 1);
			Ctx.pDC->Colour(LC_WORKSPACE, 24);
			Ctx.pDC->Rectangle(&Bounds);

			GRect rr(0, 0, Src->X()-1, SourceValid.y2 / Scale);
			Ctx.pDC->Blt(r.x1, r.y1, Src, &rr);
		}
		else
		{
			if (Ctx.Type == GRichTextPriv::Selected)
			{
				if (!SelectImg &&
					SelectImg.Reset(new GMemDC(Src->X(), Src->Y(), System32BitColourSpace)))
				{
					SelectImg->Blt(0, 0, Src);

					int Op = SelectImg->Op(GDC_ALPHA);
					GColour c = Ctx.Colours[GRichTextPriv::Selected].Back;
					c.Rgb(c.r(), c.g(), c.b(), 0xa0);
					SelectImg->Colour(c);
					SelectImg->Rectangle();
					SelectImg->Op(Op);
				}

				Ctx.pDC->Blt(r.x1, r.y1, SelectImg);
			}
			else
			{
				Ctx.pDC->Blt(r.x1, r.y1, Src);
			}
		}
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

ssize_t GRichTextPriv::ImageBlock::GetTextAt(ssize_t Offset, GArray<StyleText*> &t)
{
	// No text to get
	return 0;
}

ssize_t GRichTextPriv::ImageBlock::CopyAt(ssize_t Offset, ssize_t Chars, GArray<uint32> *Text)
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

ssize_t GRichTextPriv::ImageBlock::FindAt(ssize_t StartIdx, const uint32 *Str, GFindReplaceCommon *Params)
{
	// No text to find in
	return -1;
}

void GRichTextPriv::ImageBlock::IncAllStyleRefs()
{
	if (Style)
		Style->RefCount++;
}

bool GRichTextPriv::ImageBlock::DoContext(GSubMenu &s, GdcPt2 Doc, ssize_t Offset, bool Spelling)
{
	if (SourceImg && !Spelling)
	{
		s.AppendSeparator();
		
		GSubMenu *c = s.AppendSub("Transform Image");
		if (c)
		{
			c->AppendItem("Rotate Clockwise", IDM_CLOCKWISE); 
			c->AppendItem("Rotate Anti-clockwise", IDM_ANTI_CLOCKWISE); 
			c->AppendItem("Horizontal Flip", IDM_X_FLIP); 
			c->AppendItem("Vertical Flip", IDM_Y_FLIP); 
		}

		c = s.AppendSub("Scale Image");
		if (c)
		{
			for (unsigned i=0; i<Scales.Length(); i++)
			{
				ScaleInf &si = Scales[i];
				
				GString m;
				si.Sz.x = SourceImg->X() * ImgScales[i] / 100;
				si.Sz.y = SourceImg->Y() * ImgScales[i] / 100;
				si.Percent = ImgScales[i];
				
				m.Printf("%i x %i, %i%% ", si.Sz.x, si.Sz.y, ImgScales[i]);
				if (si.Jpg)
				{
					char Sz[128];
					LgiFormatSize(Sz, sizeof(Sz), si.Jpg->GetSize());
					GString s;
					s.Printf(" (%s)", Sz);
					m += s;
				}
				
				GMenuItem *mi = c->AppendItem(m, IDM_SCALE_IMAGE+i);
				if (mi && ResizeIdx == i)
				{
					mi->Checked(true);
				}
			}
		}

		return true;
	}

	return false;
}

GRichTextPriv::Block *GRichTextPriv::ImageBlock::Clone()
{
	return new ImageBlock(this);
}

void GRichTextPriv::ImageBlock::UpdateDisplay(int yy)
{
	GRect s;
	if (DisplayImg && !SourceValid.Valid())
	{
		SourceValid = SourceImg->Bounds();
		SourceValid.y2 = yy;
		s = SourceValid;
	}
	else
	{
		s = SourceValid;
		s.y1 = s.y2 + 1;
		s.y2 = SourceValid.y2 = yy;
	}

	if (DisplayImg)
	{
		GRect d(0, s.y1 / Scale, DisplayImg->X()-1, s.y2 / Scale);

		// Do a quick and dirty nearest neighbor scale to 
		// show the user some feed back.
		GSurface *Src = SourceImg;
		GSurface *Dst = DisplayImg;
		for (int y=d.y1; y<=d.y2; y++)
		{
			int sy = y * Scale;
			int sx = d.x1 * Scale;
			for (int x=d.x1; x<=d.x2; x++, sx+=Scale)
			{
				COLOUR c = Src->Get(sx, sy);
				Dst->Colour(c);
				Dst->Set(x, y);
			}
		}
					
		LayoutDirty = true;
		this->d->InvalidateDoc(NULL);
	}
}

int GRichTextPriv::ImageBlock::GetThreadHandle()
{
	if (ThreadHnd == 0)
	{
		ImageLoader *il = new ImageLoader(this);
		if (il > 0)
			ThreadHnd = il->GetHandle();
	}

	return ThreadHnd;
}

void GRichTextPriv::ImageBlock::UpdateDisplayImg()
{
	if (!SourceImg)
		return;

	Size.x = SourceImg->X();
	Size.y = SourceImg->Y();

	int ViewX = d->Areas[GRichTextEdit::ContentArea].X();
	if (ViewX > 0)
	{
		int MaxX = (int) (ViewX * 0.9);
		if (SourceImg->X() > MaxX)
		{
			double Ratio = (double)SourceImg->X() / MAX(1, MaxX);
			Scale = (int)ceil(Ratio);

			Size.x = (int)ceil((double)SourceImg->X() / Scale);
			Size.y = (int)ceil((double)SourceImg->Y() / Scale);
			if (DisplayImg.Reset(new GMemDC(Size.x, Size.y, SourceImg->GetColourSpace())))
			{
				DisplayImg->Colour(LC_MED, 24);
				DisplayImg->Rectangle();
			}
		}
	}
}

GMessage::Result GRichTextPriv::ImageBlock::OnEvent(GMessage *Msg)
{
	switch (Msg->Msg())
	{
		case M_COMMAND:
		{
			if (!SourceImg)
				break;
			if (Msg->A() >= IDM_SCALE_IMAGE &&
				Msg->A() < IDM_SCALE_IMAGE + CountOf(ImgScales))
			{
				int i = (int)Msg->A() - IDM_SCALE_IMAGE;
				if (i >= 0 && i < (int)Scales.Length())
				{
					ScaleInf &si = Scales[i];
					ResizeIdx = i;

					PostThreadEvent(GetThreadHandle(), M_IMAGE_COMPRESS, (GMessage::Param)SourceImg.Get(), (GMessage::Param)&si);
				}
			}
			else switch (Msg->A())
			{
				case IDM_CLOCKWISE:
					PostThreadEvent(GetThreadHandle(), M_IMAGE_ROTATE, (GMessage::Param) SourceImg.Get(), 1);
					break;
				case IDM_ANTI_CLOCKWISE:
					PostThreadEvent(GetThreadHandle(), M_IMAGE_ROTATE, (GMessage::Param) SourceImg.Get(), -1);
					break;
				case IDM_X_FLIP:
					PostThreadEvent(GetThreadHandle(), M_IMAGE_FLIP, (GMessage::Param) SourceImg.Get(), 1);
					break;
				case IDM_Y_FLIP:
					PostThreadEvent(GetThreadHandle(), M_IMAGE_FLIP, (GMessage::Param) SourceImg.Get(), 0);
					break;
			}
			break;
		}
		case M_IMAGE_COMPRESS:
		{
			GAutoPtr<GMemStream> Jpg((GMemStream*)Msg->A());
			ScaleInf *Si = (ScaleInf*)Msg->B();
			if (!Jpg || !Si)
			{
				LgiAssert(0);
				break;
			}

			Si->Jpg.Reset(Jpg.Release());
			break;
		}
		case M_IMAGE_ERROR:
		{
			GAutoPtr<GString> ErrMsg((GString*) Msg->A());
			PostThreadEvent(ThreadHnd, M_CLOSE);
			ThreadHnd = 0;
			break;
		}
		case M_IMAGE_SET_SURFACE:
		{
			GAutoPtr<GFile> Jpeg((GFile*)Msg->B());

			if (SourceImg.Reset((GSurface*)Msg->A()))
			{
				Scales.Length(CountOf(ImgScales));
				for (int i=0; i<CountOf(ImgScales); i++)
				{
					ScaleInf &si = Scales[i];
					si.Sz.x = SourceImg->X() * ImgScales[i] / 100;
					si.Sz.y = SourceImg->Y() * ImgScales[i] / 100;
					si.Percent = ImgScales[i];
				
					if (si.Sz.x == SourceImg->X() &&
						si.Sz.y == SourceImg->Y())
					{
						ResizeIdx = i;
						si.Jpg.Reset(Jpeg.Release());
					}
				}

				UpdateDisplayImg();
			}
			break;
		}
		case M_IMAGE_PROGRESS:
		{
			UpdateDisplay((int)Msg->A());
			break;
		}
		case M_IMAGE_FINISHED:
		{
			UpdateDisplay(SourceImg->Y()-1);
			PostThreadEvent(GetThreadHandle(),
							M_IMAGE_RESAMPLE,
							(GMessage::Param)DisplayImg.Get(),
							(GMessage::Param)SourceImg.Get());
			break;
		}
		case M_IMAGE_RESAMPLE:
		{
			LayoutDirty = true;
			d->InvalidateDoc(NULL);
			PostThreadEvent(ThreadHnd, M_CLOSE);
			ThreadHnd = 0;
			SourceValid.ZOff(-1, -1);
			break;
		}
		case M_IMAGE_ROTATE:
		case M_IMAGE_FLIP:
		{
			GAutoPtr<GSurface> Img = SourceImg;
			SetImage(Img);
			break;
		}
	}

	return 0;
}

bool GRichTextPriv::ImageBlock::AddText(Transaction *Trans, ssize_t AtOffset, const uint32 *Str, ssize_t Chars, GNamedStyle *Style)
{
	// Can't add text to image block
	return false;
}

bool GRichTextPriv::ImageBlock::ChangeStyle(Transaction *Trans, ssize_t Offset, ssize_t Chars, GCss *Style, bool Add)
{
	// No styles to change...
	return false;
}

ssize_t GRichTextPriv::ImageBlock::DeleteAt(Transaction *Trans, ssize_t BlkOffset, ssize_t Chars, GArray<uint32> *DeletedText)
{
	// The image is one "character"
	if (BlkOffset == 0)
	{
		
	}

	return false;
}

bool GRichTextPriv::ImageBlock::DoCase(Transaction *Trans, ssize_t StartIdx, ssize_t Chars, bool Upper)
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

