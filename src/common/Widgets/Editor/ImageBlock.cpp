#include "Lgi.h"
#include "GRichTextEdit.h"
#include "GRichTextEditPriv.h"
#include "GdcTools.h"
#include "GToken.h"

#define LOADER_THREAD_LOGGING		1
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
	GAutoPtr<GStream> In;

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
		#if LOADER_THREAD_LOGGING
		LgiTrace("%s:%i - ~ImageLoader\n", _FL);
		#endif
	}

	void Value(int64 v)
	{
		Progress::Value(v);

		if (!SurfaceSent)
		{
			SurfaceSent = true;
			PostSink(M_IMAGE_SET_SURFACE, (GMessage::Param)Img, (GMessage::Param)In.Release());
		}

		int64 Now = LgiCurrentTime();
		if (Now - Ts > TIMEOUT_LOAD_PROGRESS)
		{
			Ts = Now;
			PostSink(M_IMAGE_PROGRESS, (GMessage::Param)v);
		}
	}

	bool PostSink(int Cmd, GMessage::Param a = 0, GMessage::Param b = 0)
	{
		for (int i=0; i<50; i++)
		{
			if (Sink->PostEvent(Cmd, a, b))
				return true;
			LgiSleep(1);
		}

		LgiAssert(!"PostSink failed.");
		return false;
	}

	GMessage::Result OnEvent(GMessage *Msg)
	{
		switch (Msg->Msg())
		{
			case M_IMAGE_LOAD_FILE:
			{
				GAutoPtr<GString> Str((GString*)Msg->A());
				File = *Str;

				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Receive(M_IMAGE_LOAD_FILE): '%s'\n", _FL, File.Get());
				#endif
				
				if (!Filter.Reset(GFilterFactory::New(File, O_READ, NULL)))
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): no filter\n", _FL);
					#endif
					return PostSink(M_IMAGE_ERROR);
				}

				if (!In.Reset(new GFile) ||
					!In->Open(File, O_READ))
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): can't read\n", _FL);
					#endif
					return PostSink(M_IMAGE_ERROR);
				}

				if (!(Img = new GMemDC))
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): alloc err\n", _FL);
					#endif
					return PostSink(M_IMAGE_ERROR);
				}

				Filter->SetProgress(this);

				Ts = LgiCurrentTime();
				GFilter::IoStatus Status = Filter->ReadImage(Img, In);
				if (Status != GFilter::IoSuccess)
				{
					if (Status == GFilter::IoComponentMissing)
					{
						GString *s = new GString(Filter->GetComponentName());
						#if LOADER_THREAD_LOGGING
						LgiTrace("%s:%i - Thread.Send(M_IMAGE_COMPONENT_MISSING)\n", _FL);
						#endif
						return PostSink(M_IMAGE_COMPONENT_MISSING, (GMessage::Param)s);
					}

					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): Filter::ReadImage err\n", _FL);
					#endif
					return PostSink(M_IMAGE_ERROR);
				}

				if (!SurfaceSent)
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_SET_SURFACE)\n", _FL);
					#endif
					PostSink(M_IMAGE_SET_SURFACE, (GMessage::Param)Img, (GMessage::Param)In.Release());
				}

				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Send(M_IMAGE_FINISHED)\n", _FL);
				#endif
				PostSink(M_IMAGE_FINISHED);
				break;
			}
			case M_IMAGE_LOAD_STREAM:
			{
				GAutoPtr<GStreamI> Stream((GStreamI*)Msg->A());
				GAutoPtr<GString> FileName((GString*)Msg->B());
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Receive(M_IMAGE_LOAD_STREAM)\n", _FL);
				#endif
				if (!Stream)
				{
					LgiAssert(!"No stream.");
					return PostSink(M_IMAGE_ERROR);
				}
				
				GMemStream *Mem = new GMemStream(Stream, 0, -1);
				In.Reset(Mem);
				if (!Filter.Reset(GFilterFactory::New(FileName ? *FileName : 0, O_READ, (const uchar*)Mem->GetBasePtr())))
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): no filter\n", _FL);
					#endif
					return PostSink(M_IMAGE_ERROR);
				}

				if (!(Img = new GMemDC))
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): alloc err\n", _FL);
					#endif
					return PostSink(M_IMAGE_ERROR);
				}

				Filter->SetProgress(this);

				Ts = LgiCurrentTime();
				GFilter::IoStatus Status = Filter->ReadImage(Img, Mem);
				if (Status != GFilter::IoSuccess)
				{
					if (Status == GFilter::IoComponentMissing)
					{
						GString *s = new GString(Filter->GetComponentName());
						#if LOADER_THREAD_LOGGING
						LgiTrace("%s:%i - Thread.Send(M_IMAGE_COMPONENT_MISSING)\n", _FL);
						#endif
						return PostSink(M_IMAGE_COMPONENT_MISSING, (GMessage::Param)s);
					}

					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): Filter::ReadImage err\n", _FL);
					#endif
					return PostSink(M_IMAGE_ERROR);
				}

				if (!SurfaceSent)
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_SET_SURFACE)\n", _FL);
					#endif
					PostSink(M_IMAGE_SET_SURFACE, (GMessage::Param)Img, (GMessage::Param)In.Release());
				}

				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Send(M_IMAGE_FINISHED)\n", _FL);
				#endif
				PostSink(M_IMAGE_FINISHED);
				break;
			}
			case M_IMAGE_RESAMPLE:
			{
				GSurface *Dst = (GSurface*) Msg->A();
				GSurface *Src = (GSurface*) Msg->B();
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Receive(M_IMAGE_RESAMPLE)\n", _FL);
				#endif
				if (Src && Dst)
				{
					ResampleDC(Dst, Src);
					if (PostSink(M_IMAGE_RESAMPLE))
					{
						#if LOADER_THREAD_LOGGING
						LgiTrace("%s:%i - Thread.Send(M_IMAGE_RESAMPLE)\n", _FL);
						#endif
					}
					else LgiTrace("%s:%i - Error sending re-sample msg.\n", _FL);
				}
				else
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): ptr err %p %p\n", _FL, Src, Dst);
					#endif
					return PostSink(M_IMAGE_ERROR);
				}
				break;
			}
			case M_IMAGE_COMPRESS:
			{
				GSurface *img = (GSurface*)Msg->A();
				GRichTextPriv::ImageBlock::ScaleInf *si = (GRichTextPriv::ImageBlock::ScaleInf*)Msg->B();
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Receive(M_IMAGE_COMPRESS)\n", _FL);
				#endif
				if (!img || !si)
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): invalid ptr\n", _FL);
					#endif
					PostSink(M_IMAGE_ERROR, (GMessage::Param) new GString("Invalid pointer."));
					break;
				}
				
				GAutoPtr<GFilter> f(GFilterFactory::New("a.jpg", O_READ, NULL));
				if (!f)
				{
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): No JPEG filter available\n", _FL);
					#endif
					PostSink(M_IMAGE_ERROR, (GMessage::Param) new GString("No JPEG filter available."));
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
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Thread.Send(M_IMAGE_ERROR): Image compression failed\n", _FL);
					#endif
					PostSink(M_IMAGE_ERROR, (GMessage::Param) new GString("Image compression failed."));
					break;
				}

				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Send(M_IMAGE_COMPRESS)\n", _FL);
				#endif
				PostSink(M_IMAGE_COMPRESS, (GMessage::Param)jpg.Release(), (GMessage::Param)si);
				break;
			}
			case M_IMAGE_ROTATE:
			{
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Receive(M_IMAGE_ROTATE)\n", _FL);
				#endif
				GSurface *Img = (GSurface*)Msg->A();
				if (!Img)
				{
					LgiAssert(!"No image.");
					break;
				}

				RotateDC(Img, Msg->B() == 1 ? 90 : 270);
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Send(M_IMAGE_ROTATE)\n", _FL);
				#endif
				PostSink(M_IMAGE_ROTATE);
				break;
			}
			case M_IMAGE_FLIP:
			{
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Receive(M_IMAGE_FLIP)\n", _FL);
				#endif
				GSurface *Img = (GSurface*)Msg->A();
				if (!Img)
				{
					LgiAssert(!"No image.");
					break;
				}

				if (Msg->B() == 1)
					FlipXDC(Img);
				else
					FlipYDC(Img);
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Send(M_IMAGE_FLIP)\n", _FL);
				#endif
				PostSink(M_IMAGE_FLIP);
				break;
			}
			case M_CLOSE:
			{
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Thread.Receive(M_CLOSE)\n", _FL);
				#endif
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
	IsDeleted = false;
	LayoutDirty = false;
	Pos.ZOff(-1, -1);
	Style = NULL;
	Size.x = 200;
	Size.y = 64;
	Scale = 1;
	SourceValid.ZOff(-1, -1);
	ResizeIdx = -1;
	ThreadBusy = 0;

	Margin.ZOff(0, 0);
	Border.ZOff(0, 0);
	Padding.ZOff(0, 0);
}

GRichTextPriv::ImageBlock::ImageBlock(const ImageBlock *Copy) : Block(Copy->d)
{
	ThreadHnd = 0;
	ThreadBusy = 0;
	LayoutDirty = true;
	SourceImg.Reset(new GMemDC(Copy->SourceImg));
	Size = Copy->Size;
	IsDeleted = false;

	Margin = Copy->Margin;
	Border = Copy->Border;
	Padding = Copy->Padding;
}

GRichTextPriv::ImageBlock::~ImageBlock()
{
	LgiAssert(ThreadBusy == 0);
	if (ThreadHnd)
		PostThreadEvent(ThreadHnd, M_CLOSE);
	LgiAssert(Cursors == 0);
}

bool GRichTextPriv::ImageBlock::IsValid()
{
	return true;
}

bool GRichTextPriv::ImageBlock::IsBusy(bool Stop)
{
	return ThreadBusy != 0;
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
		if (PostThreadEvent(GetThreadHandle(),
							M_IMAGE_RESAMPLE,
							(GMessage::Param) DisplayImg.Get(),
							(GMessage::Param) SourceImg.Get()))
			UpdateThreadBusy(_FL, 1);

	}
	else LayoutDirty = true;

	// Also create a JPG for the current scale (needed before 
	// we save to HTML).
	if (ResizeIdx >= 0 && ResizeIdx < (int)Scales.Length())
	{
		ScaleInf &si = Scales[ResizeIdx];
		if (PostThreadEvent(GetThreadHandle(), M_IMAGE_COMPRESS, (GMessage::Param)SourceImg.Get(), (GMessage::Param)&si))
			UpdateThreadBusy(_FL, 1);
	}
	else LgiAssert(!"ResizeIdx should be valid.");
	
	return true;
}

bool GRichTextPriv::ImageBlock::Load(const char *Src)
{
	if (Src)
		Source = Src;

	GAutoPtr<GStreamI> Stream;
	
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
			StreamMimeType = j->MimeType;
			ContentId = j->ContentId.Strip("<>");
			FileName = j->Filename;

			if (j->Stream)
			{
				Stream = j->Stream;
			}
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
		FileMimeType = LgiApp->GetFileMimeType(Source);
	}
	else
		return false;

	if (!FileName && !Stream)
		return false;

	if (Stream)
	{
		#if LOADER_THREAD_LOGGING
		LgiTrace("%s:%i - Posting M_IMAGE_LOAD_STREAM\n", _FL);
		#endif
		if (PostThreadEvent(GetThreadHandle(), M_IMAGE_LOAD_STREAM, (GMessage::Param)Stream.Release(), (GMessage::Param) (FileName ? new GString(FileName) : NULL)))
		{
			UpdateThreadBusy(_FL, 1);
			return true;
		}
	}
	
	if (FileName)
	{
		#if LOADER_THREAD_LOGGING
		LgiTrace("%s:%i - Posting M_IMAGE_LOAD_FILE\n", _FL);
		#endif
		if (PostThreadEvent(GetThreadHandle(), M_IMAGE_LOAD_FILE, (GMessage::Param)new GString(FileName)))
		{
			UpdateThreadBusy(_FL, 1);
			return true;
		}
	}
	
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
	return IsDeleted ? 0 : 1;
}

bool GRichTextPriv::ImageBlock::ToHtml(GStream &s, GArray<GDocView::ContentMedia> *Media, GRange *Rng)
{
	if (Media)
	{
		bool ValidSourceFile = FileExists(Source);
		GDocView::ContentMedia &Cm = Media->New();
		
		int Idx = LgiRand() % 10000;
		if (!ContentId)
			ContentId.Printf("%u@memecode.com", Idx);
		Cm.Id = ContentId;

		GString Style;
		ScaleInf *Si = ResizeIdx >= 0 && ResizeIdx < (int)Scales.Length() ? &Scales[ResizeIdx] : NULL;
		if (Si && Si->Compressed)
		{
			// Attach a copy of the resized JPEG...
			Si->Compressed->SetPos(0);
			Cm.Stream.Reset(new GMemStream(Si->Compressed, 0, -1));
			Cm.MimeType = Si->MimeType;

			if (FileName)
				Cm.FileName = FileName;
			else if (Cm.MimeType.Equals("image/jpeg"))
				Cm.FileName.Printf("img%u.jpg", Idx);
			else if (Cm.MimeType.Equals("image/png"))
				Cm.FileName.Printf("img%u.png", Idx);
			else if (Cm.MimeType.Equals("image/tiff"))
				Cm.FileName.Printf("img%u.tiff", Idx);
			else if (Cm.MimeType.Equals("image/gif"))
				Cm.FileName.Printf("img%u.gif", Idx);
			else if (Cm.MimeType.Equals("image/bmp"))
				Cm.FileName.Printf("img%u.bmp", Idx);
			else
			{
				LgiAssert(!"Unknown image mime type?");
				Cm.FileName.Printf("img%u", Idx);
			}
		}
		else if (ValidSourceFile)
		{
			// Attach the original file...
			GAutoString mt = LgiApp->GetFileMimeType(Source);
			Cm.MimeType = mt.Get();
			Cm.FileName = LgiGetLeaf(Source);

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
			LgiAssert(!"No source file or compressed image.");
			return false;
		}

		LgiAssert(Cm.MimeType != NULL);

		if (DisplayImg &&
			SourceImg &&
			DisplayImg->X() != SourceImg->X())
		{
			int Dx = DisplayImg->X();
			Style.Printf(" style=\"width:%ipx\"", Dx);
		}
		
		if (Cm.Stream)
		{
			s.Print("<img%s src=\"", Style ? Style.Get() : "");
			if (d->HtmlLinkAsCid)
				s.Print("cid:%s", Cm.Id.Get());
			else
				s.Print("%s", Cm.FileName.Get());
			s.Print("\">\n");

			LgiAssert(Cm.Valid());
			return true;
		}
	}

	s.Print("<img src=\"%s\">\n", Source.Get());
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
	bool ImgSelected = Ctx.SelectBeforePaint(this);
			
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

	ImgSelected = Ctx.SelectAfterPaint(this);

	if (ImgSelected)
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
				if (si.Compressed)
				{
					char Sz[128];
					LgiFormatSize(Sz, sizeof(Sz), si.Compressed->GetSize());
					GString s;
					s.Printf(" (%s)", Sz);
					m += s;
				}
				
				GMenuItem *mi = c->AppendItem(m, IDM_SCALE_IMAGE+i, !IsBusy());
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

void GRichTextPriv::ImageBlock::OnComponentInstall(GString Name)
{
	if (Source && !SourceImg)
	{
		// Retry the load?
		Load(Source);
	}
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
	}

	LayoutDirty = true;
	this->d->InvalidateDoc(NULL);
}

int GRichTextPriv::ImageBlock::GetThreadHandle()
{
	if (ThreadHnd == 0)
	{
		ImageLoader *il = new ImageLoader(this);
		if (il != NULL)
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

void GRichTextPriv::ImageBlock::UpdateThreadBusy(const char *File, int Line, int Off)
{
	if (ThreadBusy + Off >= 0)
	{
		ThreadBusy += Off;
		#if LOADER_THREAD_LOGGING
		LgiTrace("%s:%i - ThreadBusy=%i\n", File, Line, ThreadBusy);
		#endif
	}
	else
	{
		#if LOADER_THREAD_LOGGING
		LgiTrace("%s:%i - Error: ThreadBusy=%i\n", File, Line, ThreadBusy, ThreadBusy + Off);
		#endif
		LgiAssert(0);
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

					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Posting M_IMAGE_COMPRESS\n", _FL);
					#endif
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_COMPRESS, (GMessage::Param)SourceImg.Get(), (GMessage::Param)&si))
						UpdateThreadBusy(_FL, 1);
					else
						LgiAssert(!"PostThreadEvent failed.");
				}
			}
			else switch (Msg->A())
			{
				case IDM_CLOCKWISE:
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Posting M_IMAGE_ROTATE\n", _FL);
					#endif
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_ROTATE, (GMessage::Param) SourceImg.Get(), 1))
						UpdateThreadBusy(_FL, 1);
					break;
				case IDM_ANTI_CLOCKWISE:
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Posting M_IMAGE_ROTATE\n", _FL);
					#endif
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_ROTATE, (GMessage::Param) SourceImg.Get(), -1))
						UpdateThreadBusy(_FL, 1);
					break;
				case IDM_X_FLIP:
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Posting M_IMAGE_FLIP\n", _FL);
					#endif
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_FLIP, (GMessage::Param) SourceImg.Get(), 1))
						UpdateThreadBusy(_FL, 1);
					break;
				case IDM_Y_FLIP:
					#if LOADER_THREAD_LOGGING
					LgiTrace("%s:%i - Posting M_IMAGE_FLIP\n", _FL);
					#endif
					if (PostThreadEvent(GetThreadHandle(), M_IMAGE_FLIP, (GMessage::Param) SourceImg.Get(), 0))
						UpdateThreadBusy(_FL, 1);
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
				#if LOADER_THREAD_LOGGING
				LgiTrace("%s:%i - Error: M_IMAGE_COMPRESS bad arg\n", _FL);
				#endif
				break;
			}

			#if LOADER_THREAD_LOGGING
			LgiTrace("%s:%i - Received M_IMAGE_COMPRESS\n", _FL);
			#endif
			Si->Compressed.Reset(Jpg.Release());
			Si->MimeType = "image/jpeg";
			UpdateThreadBusy(_FL, -1);
			break;
		}
		case M_IMAGE_ERROR:
		{
			GAutoPtr<GString> ErrMsg((GString*) Msg->A());
			#if LOADER_THREAD_LOGGING
			LgiTrace("%s:%i - Received M_IMAGE_ERROR, posting M_CLOSE\n", _FL);
			#endif
			UpdateThreadBusy(_FL, -1);
			break;
		}
		case M_IMAGE_COMPONENT_MISSING:
		{
			GAutoPtr<GString> Component((GString*) Msg->A());
			#if LOADER_THREAD_LOGGING
			LgiTrace("%s:%i - Received M_IMAGE_COMPONENT_MISSING, posting M_CLOSE\n", _FL);
			#endif
			UpdateThreadBusy(_FL, -1);

			if (Component)
			{
				GToken t(*Component, ",");
				for (int i=0; i<t.Length(); i++)
					d->View->NeedsCapability(t[i]);
			}
			else LgiAssert(!"Missing component name.");
			break;
		}
		case M_IMAGE_SET_SURFACE:
		{
			GAutoPtr<GStream> File((GStream*)Msg->B());

			#if LOADER_THREAD_LOGGING
			LgiTrace("%s:%i - Received M_IMAGE_SET_SURFACE\n", _FL);
			#endif

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
						si.Compressed.Reset(File.Release());

						if (StreamMimeType)
						{
							si.MimeType = StreamMimeType;
						}
						else if (FileMimeType)
						{
							si.MimeType = FileMimeType.Get();
							FileMimeType.Reset();
						}
					}
				}

				UpdateDisplayImg();
			}
			break;
		}
		case M_IMAGE_PROGRESS:
		{
			#if LOADER_THREAD_LOGGING
			LgiTrace("%s:%i - Received M_IMAGE_PROGRESS\n", _FL);
			#endif

			UpdateDisplay((int)Msg->A());
			break;
		}
		case M_IMAGE_FINISHED:
		{
			#if LOADER_THREAD_LOGGING
			LgiTrace("%s:%i - Received M_IMAGE_FINISHED\n", _FL);
			#endif

			UpdateDisplay(SourceImg->Y()-1);
			UpdateThreadBusy(_FL, -1);

			if (DisplayImg != NULL &&
				PostThreadEvent(GetThreadHandle(),
								M_IMAGE_RESAMPLE,
								(GMessage::Param)DisplayImg.Get(),
								(GMessage::Param)SourceImg.Get()))
				UpdateThreadBusy(_FL, 1);
			break;
		}
		case M_IMAGE_RESAMPLE:
		{
			#if LOADER_THREAD_LOGGING
			LgiTrace("%s:%i - Received M_IMAGE_RESAMPLE\n", _FL);
			#endif

			LayoutDirty = true;
			UpdateThreadBusy(_FL, -1);
			d->InvalidateDoc(NULL);
			SourceValid.ZOff(-1, -1);
			break;
		}
		case M_IMAGE_ROTATE:
		case M_IMAGE_FLIP:
		{
			#if LOADER_THREAD_LOGGING
			LgiTrace("%s:%i - Received %s\n", _FL, Msg->Msg()==M_IMAGE_ROTATE?"M_IMAGE_ROTATE":"M_IMAGE_FLIP");
			#endif

			GAutoPtr<GSurface> Img = SourceImg;
			UpdateThreadBusy(_FL, -1);
			SetImage(Img);
			break;
		}
		default:
			return false;
	}

	return true;
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
	IsDeleted = BlkOffset == 0;
	if (IsDeleted)
		return true;

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

