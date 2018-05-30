#include "Lgi.h"
#include "LList.h"
#include "GCombo.h"
#include "GEdit.h"
#include "GPalette.h"
#include "GZoomView.h"
#include "GVariant.h"
#include "GButton.h"
#include "ImageComparison.h"
#include "GTabView.h"
#include "resdefs.h"
#include "LgiRes.h"

#define DIFF_LARGE_8BIT		(10)
#define DIFF_LARGE_16BIT	(DIFF_LARGE_8BIT << 8)
#define M_LOAD				(M_USER + 2000)

#define OPT_CompareLeft		"CmpLeft"
#define OPT_CompareRight	"CmpRight"

enum CmpCtrls
{
	IDC_A_NAME = 1000,
	IDC_B_NAME,
	IDC_C_NAME,

	IDC_A_VIEW,
	IDC_B_VIEW,
	IDC_C_VIEW,
	
	IDC_TAB_VIEW,
	IDC_TAB_PAGE,
};

#ifdef DIFF_CENTER
GArray<uint32> RDiff, GDiff, BDiff;
#endif

template<typename Px>
void CompareRgb(GSurface *A, GSurface *B, uint8 *c, GdcPt2 size, int threshold)
{
	if (!A || !B || !c)
		return;

	Px *a = (Px*) (*A)[size.y];
	Px *b = (Px*) (*B)[size.y];
	Px *e = a + size.x;
	int32 Diff, Mask, Value;
	
	while (a < e)
	{
		Value = (int32)a->r - b->r;
		#ifdef DIFF_CENTER
		RDiff[DIFF_CENTER+Value]++;
		#endif
		Mask = Value >> 31;
		Diff = (Value + Mask) ^ Mask;
		
		Value = (int32)a->g - b->g;
		#ifdef DIFF_CENTER
		GDiff[DIFF_CENTER+Value]++;
		#endif
		Mask = Value >> 31;
		Diff += (Value + Mask) ^ Mask;
		
		Value = (int32)a->b - b->b;
		#ifdef DIFF_CENTER
		BDiff[DIFF_CENTER+Value]++;
		#endif
		Mask = Value >> 31;
		Diff += (Value + Mask) ^ Mask;

		*c++ = (Diff > 0) + (Diff >= threshold);
		a++;
		b++;
	}
}

template<typename Px>
void CompareRgba(GSurface *A, GSurface *B, uint8 *c, GdcPt2 size, int threshold)
{
	Px *a = (Px*) (*A)[size.y];
	Px *b = (Px*) (*B)[size.y];
	Px *e = a + size.x;
	int32 Diff, Mask, Value;

	while (a < e)
	{
		Value = (int32)a->r - b->r;
		#ifdef DIFF_CENTER
		RDiff[DIFF_CENTER+Value]++;
		#endif
		Mask = Value >> 31;
		Diff = (Value + Mask) ^ Mask;
		
		Value = (int32)a->g - b->g;
		#ifdef DIFF_CENTER
		GDiff[DIFF_CENTER+Value]++;
		#endif
		Mask = Value >> 31;
		Diff += (Value + Mask) ^ Mask;
		
		Value = (int32)a->b - b->b;
		#ifdef DIFF_CENTER
		BDiff[DIFF_CENTER+Value]++;
		#endif
		Mask = Value >> 31;
		Diff += (Value + Mask) ^ Mask;

		Value = (int32)a->a - b->a;
		Mask = Value >> 31;
		Diff += (Value + Mask) ^ Mask;

		*c++ = (Diff > 0) + (Diff >= threshold);
		a++;
		b++;
	}
}

GAutoPtr<GMemDC> CreateDiff(GViewI *Parent, GSurface *A, GSurface *B)
{
	GAutoPtr<GMemDC> C;
	int Cx = MIN(A->X(), B->X()), Cy = MIN(A->Y(), B->Y());
	if (A->GetColourSpace() != B->GetColourSpace())
	{
		GStringPipe p;
		p.Print("The bit depths of the images are different: %i (left), %i (right).",
				A->GetBits(), B->GetBits());
		GAutoString a(p.NewStr());
		LgiMsg(Parent, "%s", "Image Compare", MB_OK, a.Get());
	}
	else if (C.Reset(new GMemDC(Cx, Cy, CsIndex8)) &&
			(*C)[0])
	{
		uchar Pal[] = {0, 0, 0, 0xc0, 0xc0, 0xc0, 0xff, 0, 0};
		C->Palette(new GPalette(Pal, 3));
			
		for (int y=0; y<Cy; y++)
		{
			int32 Diff, Mask, Value;
			uint8 *c = (*C)[y];
			
			switch (A->GetColourSpace())
			{
				case CsIndex8:
				{
					GPalette *apal = A->Palette();
					GPalette *bpal = B->Palette();
					if (apal && bpal)
					{
						GdcRGB *ap = (*apal)[0];
						GdcRGB *bp = (*bpal)[0];
						uint8 *a = (*A)[y];
						uint8 *b = (*B)[y];
						uint8 *e = a + Cx;
						while (a < e)
						{
							Value = (int32)ap[*a].r - bp[*b].r;
							Mask = Value >> 31;
							Diff = (Value + Mask) ^ Mask;
							Value = (int32)ap[*a].g - bp[*b].g;
							Mask = Value >> 31;
							Diff += (Value + Mask) ^ Mask;
							Value = (int32)ap[*a].b - bp[*b].b;
							Mask = Value >> 31;
							Diff += (Value + Mask) ^ Mask;

							*c++ = (Diff > 0) + (Diff >= DIFF_LARGE_8BIT);
							a++;
							b++;
						}
					}
					else LgiAssert(!"No palette?");
					break;
				}

				#define CompareCaseRgb(type, threshold) \
					case Cs##type: \
						CompareRgb<G##type>(A, B, c, GdcPt2(Cx, y), threshold); \
						break
				
				CompareCaseRgb(Rgb24, DIFF_LARGE_8BIT);
				CompareCaseRgb(Bgr24, DIFF_LARGE_8BIT);
				CompareCaseRgb(Rgbx32, DIFF_LARGE_8BIT);
				CompareCaseRgb(Bgrx32, DIFF_LARGE_8BIT);
				CompareCaseRgb(Xrgb32, DIFF_LARGE_8BIT);
				CompareCaseRgb(Xbgr32, DIFF_LARGE_8BIT);
				
				CompareCaseRgb(Rgb48, DIFF_LARGE_16BIT);
				CompareCaseRgb(Bgr48, DIFF_LARGE_16BIT);


				#define CompareCaseRgba(type, threshold) \
					case Cs##type: \
						CompareRgba<G##type>(A, B, c, GdcPt2(Cx, y), threshold); \
						break
				
				CompareCaseRgba(Rgba32, DIFF_LARGE_8BIT);
				CompareCaseRgba(Bgra32, DIFF_LARGE_8BIT);
				CompareCaseRgba(Argb32, DIFF_LARGE_8BIT);
				CompareCaseRgba(Abgr32, DIFF_LARGE_8BIT);
								
				CompareCaseRgba(Rgba64, DIFF_LARGE_16BIT);
				CompareCaseRgba(Bgra64, DIFF_LARGE_16BIT);

				default:
				{
					LgiAssert(!"Impl me.");
					break;
				}
			}
		}
	}
	
	#ifdef DIFF_CENTER
	int Len = max(RDiff.Length(), GDiff.Length());
	Len = max(Len, BDiff.Length());
	int Start = Len-1, End = 0;
	for (int i=0; i<Len; i++)
	{
		if (RDiff[i] ||
			GDiff[i] ||
			BDiff[i])
		{
			Start = min(Start, i);
			End = max(End, i);
		}
	}
	for (int i=Start; i<=End; i++)
	{
		int n = i - DIFF_CENTER;
		LgiTrace("%i,%i,%i,%i\n", n, RDiff[i], GDiff[i], BDiff[i]);
	}
	#endif
	
	return C;
}

class ThreadLoader : public LThread
{
	GView *Owner;
	GAutoString File;
	
public:
	GAutoPtr<GSurface> Img;
	GMessage::Param Param;

	ThreadLoader(GView *owner, GAutoString file, GMessage::Param param) : LThread("ThreadLoader")
	{
		Owner = owner;
		File = file;
		Param = param;
		
		Run();
	}
	
	~ThreadLoader()
	{
		while (!IsExited())
			LgiSleep(10);
	}
	
	int Main()
	{
		// Load the file...
		Img.Reset(LoadDC(File));
		
		if (!Owner->Handle())
		{
			// Wait for the view to be created...
			uint64 Start = LgiCurrentTime();
			while (!Owner->Handle())
			{
				LgiSleep(100);
				if (LgiCurrentTime() - Start > 5000)
				{
					LgiAssert(0);
					return -1;
				}
			}
		}
		
		Owner->PostEvent(M_LOAD, 0, (GMessage::Param) this);
		return 0;
	}
};

class CompareView;
class CmpZoomView : public GZoomView
{
	CompareView *View;
	
public:
	CmpZoomView(GZoomViewCallback *callback, CompareView *view);
	void OnMouseClick(GMouse &m);
	void OnMouseMove(GMouse &m);
};

class CompareView : public GLayout
{
	GZoomViewCallback *Callback;
	GEdit *AName, *BName, *CName;
	GAutoPtr<GSurface> A, B, C;
	CmpZoomView *AView, *BView, *CView;
	GArray<ThreadLoader*> Threads;
	GStatusBar *Status;
	GStatusPane *Pane[3];
	bool DraggingView;	
	GPointF DocPos;
	
public:
	CompareView(GZoomViewCallback *callback, const char *FileA = NULL, const char *FileB = NULL)
	{
		Callback = callback;
		AName = BName = CName = NULL;
		DraggingView = false;
		SetPourLargest(true);
		
		AddView(Status = new GStatusBar);
		Status->AppendPane(Pane[0] = new GStatusPane);
		Status->AppendPane(Pane[1] = new GStatusPane);
		Status->AppendPane(Pane[2] = new GStatusPane);
		
		if (FileA)
		{
			GAutoString a(NewStr(FileA));
			Threads.Add(new ThreadLoader(this, a, 0));
		}
		if (FileB)
		{
			GAutoString a(NewStr(FileB));
			Threads.Add(new ThreadLoader(this, a, 1));
		}
		
		AddView(AName = new GEdit(IDC_A_NAME, 0, 0, 100, SysFont->GetHeight() + 8, FileA));
		AddView(BName = new GEdit(IDC_B_NAME, 0, 0, 100, SysFont->GetHeight() + 8, FileB));			
		AddView(CName = new GEdit(IDC_C_NAME, 0, 0, 100, SysFont->GetHeight() + 8, NULL));
		CName->Sunken(false);
		CName->Enabled(false);

		AddView(AView = new CmpZoomView(Callback, this));
		AView->SetId(IDC_A_VIEW);
		AView->Name("AView");
		AddView(BView = new CmpZoomView(Callback, this));
		BView->SetId(IDC_B_VIEW);
		BView->Name("BView");
		AddView(CView = new CmpZoomView(Callback, this));
		CView->SetId(IDC_C_VIEW);
		CView->Name("CView");

		#if 0
		AView->SetSampleMode(SuperView::SampleAverage);
		BView->SetSampleMode(SuperView::SampleAverage);
		// CView->SetSampleMode(SuperView::SampleMax);
		#endif
	}
	
	~CompareView()
	{
		Threads.DeleteObjects();
	}
	
	void OnCreate()
	{
		AttachChildren();
	}
	
	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_MED, 24);
		pDC->Rectangle();
	}
	
	GMessage::Param OnEvent(GMessage *Msg)
	{
		switch (MsgCode(Msg))
		{
			case M_LOAD:
			{
				ThreadLoader *t = (ThreadLoader*) MsgB(Msg);
				if (t)
				{
					if (t->Param == 0)
					{
						A = t->Img;
						AView->SetSurface(A, false);
					}
					else if (t->Param == 1)
					{
						B = t->Img;
						BView->SetSurface(B, false);
					}
					
					Threads.Delete(t);
					DeleteObj(t);
				}
				
				if (A &&
					B)
				{
					GAutoPtr<GMemDC> pDC = CreateDiff(this, A, B);
					if (pDC)
					{
						if (C.Reset(pDC.Release()))
						{
							CView->SetSurface(C, false);
						}
					}					
				}
				
				OnPosChange();
				break;
			}
		}
		
		return GLayout::OnEvent(Msg);
	}
	
	void OnPosChange()
	{
		GLayout::OnPosChange();
		
		if (AName && BName &&
			AView && BView && CView)
		{
			GRect c = GetClient();
			GRegion rgn;
			rgn = c;
			if (Status)
			{
				Status->Pour(rgn);
				rgn.Subtract(&Status->GetPos());
				c = rgn.Bound();
			}
				
			int width = (c.X() - 10) / 3;
			GRect ar = c;
			ar.x2 = ar.x1 + width - 1;
			GRect cr = c;
			cr.x1 = ar.x2 + 6;
			cr.x2 = cr.x1 + width - 1;
			GRect br = c;
			br.x1 = br.x2 - width;

			Pane[0]->SetWidth(cr.x1);
			Pane[1]->SetWidth(br.x1 - cr.x1);
			Pane[2]->SetWidth(cr.X());
			
			GRect name = ar;
			name.y2 = name.y1 + AName->Y() - 1;
			AName->SetPos(name);
			ar.y1 = name.y2 + 1;
			AView->SetPos(ar);
			
			name = br;
			name.y2 = name.y1 + BName->Y() - 1;
			BName->SetPos(name);
			br.y1 = name.y2 + 1;
			BView->SetPos(br);
			
			name = cr;
			name.y2 = name.y1 + CName->Y() - 1;
			CName->SetPos(name);
			cr.y1 = name.y2 + 1;
			CView->SetPos(cr);
			
			AView->Visible(true);
			BView->Visible(true);
			CView->Visible(true);
		}
	}
	
	void OnViewportChange()
	{
		if (CView && CName)
		{
			GZoomView::ViewportInfo i = CView->GetViewport();
			char s[256];
			int ch = sprintf_s(s, sizeof(s), "Scroll: %i,%i  ", i.Sx, i.Sy);
			if (i.Zoom < 0)
				ch += sprintf_s(s+ch, sizeof(s)-ch, "Zoom: 1/%i", 1 - i.Zoom);
			else
				ch += sprintf_s(s+ch, sizeof(s)-ch, "Zoom: %ix", i.Zoom + 1);
			CName->Name(s);
		}
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDC_A_VIEW:
			{
				GZoomView::ViewportInfo i = AView->GetViewport();
				BView->SetViewport(i);
				CView->SetViewport(i);
				OnViewportChange();
				break;
			}
			case IDC_B_VIEW:
			{
				GZoomView::ViewportInfo i = BView->GetViewport();
				AView->SetViewport(i);
				CView->SetViewport(i);
				OnViewportChange();
				break;
			}
			case IDC_C_VIEW:
			{
				GZoomView::ViewportInfo i = CView->GetViewport();
				AView->SetViewport(i);
				BView->SetViewport(i);
				OnViewportChange();
				break;
			}
		}
		
		return GLayout::OnNotify(Ctrl, Flags);
	}

    int ZoomToFactor(int Zoom)
    {
		return Zoom < 0 ? 1 - Zoom : Zoom + 1;
	}
	
	template<typename Px>
	void PixToStr15(char *s, Px *p, GRgba64 *Diff)
	{
		sprintf(s, "%i,%i,%i (%x,%x,%x)",
					G5bitTo8bit(p->r), G5bitTo8bit(p->g), G5bitTo8bit(p->b),
					p->r, p->g, p->b);
		Diff->r = p->r;
		Diff->g = p->g;
		Diff->b = p->b;
		Diff->a = -1;
	}

	template<typename Px>
	void PixToStr16(char *s, Px *p, GRgba64 *Diff)
	{
		sprintf(s, "%i,%i,%i (%x,%x,%x)",
					G5bitTo8bit(p->r), G6bitTo8bit(p->g), G5bitTo8bit(p->b),
					p->r, p->g, p->b);
		Diff->r = p->r;
		Diff->g = p->g;
		Diff->b = p->b;
		Diff->a = -1;
	}

	template<typename Px>
	void PixToStrRgb(char *s, Px *p, GRgba64 *Diff)
	{
		sprintf(s, "%i,%i,%i (%x,%x,%x)",
					p->r, p->g, p->b,
					p->r, p->g, p->b);
		Diff->r = p->r;
		Diff->g = p->g;
		Diff->b = p->b;
		Diff->a = -1;
	}

	template<typename Px>
	void PixToStrRgba(char *s, Px *p, GRgba64 *Diff)
	{
		sprintf(s, "%i,%i,%i,%i (%x,%x,%x,%x)",
					p->r, p->g, p->b, p->a,
					p->r, p->g, p->b, p->a);
		Diff->r = p->r;
		Diff->g = p->g;
		Diff->b = p->b;
		Diff->a = p->a;
	}

	GAutoString DescribePixel(GSurface *pDC, GdcPt2 Pos, GRgba64 *Diff)
	{
		char s[256] = "No Data";
		int ch = 0;
		
		switch (pDC->GetColourSpace())
		{
			case CsIndex8:
			{
				COLOUR c = pDC->Get(Pos.x, Pos.y);
				ch = sprintf_s(s, sizeof(s), "%i (%.2x)", c, c);
				break;
			}
			
			#define DescribeCase(type, method) \
				case Cs##type: \
				{ \
					G##type *p = (G##type*)((*pDC)[Pos.y]); \
					if (p) method(s, p + Pos.x, Diff); \
					break; \
				}
			
			DescribeCase(Rgb15, PixToStr15)
			DescribeCase(Bgr15, PixToStr15)
			
			DescribeCase(Rgb16, PixToStr16)
			DescribeCase(Bgr16, PixToStr16)
			
			DescribeCase(Rgb24, PixToStrRgb)
			DescribeCase(Bgr24, PixToStrRgb)
			DescribeCase(Rgbx32, PixToStrRgb)
			DescribeCase(Bgrx32, PixToStrRgb)
			DescribeCase(Xrgb32, PixToStrRgb)
			DescribeCase(Xbgr32, PixToStrRgb)
			DescribeCase(Rgb48, PixToStrRgb)
			DescribeCase(Bgr48, PixToStrRgb)
			
			DescribeCase(Rgba32, PixToStrRgba)
			DescribeCase(Bgra32, PixToStrRgba)
			DescribeCase(Argb32, PixToStrRgba)
			DescribeCase(Abgr32, PixToStrRgba)
			DescribeCase(Rgba64, PixToStrRgba)
			DescribeCase(Bgra64, PixToStrRgba)
			
			default:
				LgiAssert(0);
				break;
		}
		
		return GAutoString(NewStr(s));
	}
	
	/*
	#undef DefOption
	#define DefOption(_type, _name, _opt, _def) \
		_type _name() { GVariant v = _def; App->GetOptions()->GetValue(_opt, v); return v.CastInt32(); } \
		void _name(_type i) { GVariant v; App->GetOptions()->SetValue(_opt, v = i); }

	DefOption(int, DisplayGrid, OPT_DisplayGrid, true);
	DefOption(int, GridSize, OPT_GridSize, 8);
	DefOption(int, DisplayTile, OPT_DisplayTile, true);
	DefOption(int, TileType, OPT_TileType, 0);
	DefOption(int, TileX, OPT_TileX, 16);
	DefOption(int, TileY, OPT_TileY, 16);

	void UserKey(ImgKey &k)
	{
	}
	
	void UserMouseEnter(ImgMouse &m)
	{
	}
	
	void UserMouseExit(ImgMouse &m)
	{
	}

	*/
	
	void UserMouseClick(GMouse &m)
	{
		GZoomView *zv = dynamic_cast<GZoomView*>(m.Target);
		if (!zv)
		{
			LgiAssert(0);
			return;
		}
		
		zv->Capture(DraggingView = m.Down());
		if (m.Down())
		{
			GZoomView *zv = dynamic_cast<GZoomView*>(m.Target);
			if (!zv->Convert(DocPos, m.x, m.y))
				LgiAssert(0);

			zv->Focus(true);
		}
	}
	
	void UserMouseMove(GMouse &m)
	{
		if (DraggingView)
		{
			GZoomView::ViewportInfo vp = AView->GetViewport();
			int Factor = ZoomToFactor(vp.Zoom);
			if (vp.Zoom < 0)
			{
				// scaling down
				vp.Sx = (int) (DocPos.x - (Factor * m.x));
				vp.Sy = (int) (DocPos.y - (Factor * m.y));
				
			}
			else if (vp.Zoom > 0)
			{
				// scaling up
				vp.Sx = (int) (DocPos.x - (m.x / Factor));
				vp.Sy = (int) (DocPos.y - (m.y / Factor));
			}
			else
			{
				// 1:1
				vp.Sx = (int) (DocPos.x - m.x);
				vp.Sy = (int) (DocPos.y - m.y);
			}
			
			AView->SetViewport(vp);
			BView->SetViewport(vp);
			CView->SetViewport(vp);
			OnViewportChange();
		}
	
		if (AView && BView)
		{
			GSurface *a = AView->GetSurface();
			GSurface *b = BView->GetSurface();
			if (a && b)
			{
				GRgba64 ap, bp;
				ZeroObj(ap);
				ZeroObj(bp);
				
				GAutoString Apix = DescribePixel(a, GdcPt2(m.x, m.y), &ap);
				Pane[0]->Name(Apix);

				GAutoString Bpix = DescribePixel(b, GdcPt2(m.x, m.y), &bp);
				Pane[2]->Name(Bpix);

				int Channels = GColourSpaceChannels(a->GetColourSpace());
				int TileX = Callback->TileX();
				int TileY = Callback->TileY();
				int Tx = m.x / TileX;
				int Ty = m.y / TileY;
				int Tile = Ty * TileX + Tx;

				char s[256];
				int diffr = bp.r - ap.r;
				int diffg = bp.g - ap.g;
				int diffb = bp.b - ap.b;
				int diffa = bp.a - ap.a;
				#define PercentDiff(c) \
					diff##c, (ap.c ? (double)abs(diff##c) * 100 / ap.c : (diff##c ? 100.0 : 0.0))

				GZoomView *zv = dynamic_cast<GZoomView*>(m.Target);
				GPointF Doc;
				zv->Convert(Doc, m.x, m.y);
				
				int ch = sprintf_s(s, sizeof(s), "Mouse: %.1f, %.1f  Tile: %i (%i, %i)  Diff: %i(%.1f%%),%i(%.1f%%),%i(%.1f%%)",
					Doc.x, Doc.y,
					Tile, Tx, Ty,
					PercentDiff(r),
					PercentDiff(g),
					PercentDiff(b));
				if (Channels > 3)
					ch += sprintf_s(s+ch, sizeof(s)-ch, ",%i(%.1f%%)", PercentDiff(b));
				Pane[1]->Name(s);
			}
		}
	}
};

CmpZoomView::CmpZoomView(GZoomViewCallback *callback, CompareView *view) : GZoomView(callback)
{
	View = view;
}

void CmpZoomView::OnMouseClick(GMouse &m)
{
	LgiAssert(m.Target == this);
	GZoomView::OnMouseClick(m);
	View->UserMouseClick(m);
}

void CmpZoomView::OnMouseMove(GMouse &m)
{
	LgiAssert(m.Target == this);
	GZoomView::OnMouseMove(m);
	View->UserMouseMove(m);
}

struct CompareThread : public LThread
{
	LList *lst;
	bool loop;
	
	CompareThread(LList *l) : LThread("CompareThread")
	{
		lst = l;
		loop = true;
		Run();
	}
	
	~CompareThread()
	{
		loop = false;
		while (!IsExited())
			LgiSleep(1);
	}
	
	int Main()
	{
		List<LListItem> items;
		lst->GetAll(items);
		List<LListItem>::I it = items.begin();
		
		while (loop)
		{
			LListItem *i = *it;

			if (i)
			{
				char *left = i->GetText(0);
				char *right = i->GetText(1);
				GAutoPtr<GSurface> left_img(LoadDC(left));
				GAutoPtr<GSurface> right_img(LoadDC(right));
				if (left_img && right_img)
				{
					if (left_img->X() == right_img->X() &&
						left_img->Y() == right_img->Y() &&
						left_img->GetColourSpace() == right_img->GetColourSpace())
					{
						bool diff = false;
						int bytes = (left_img->X() * left_img->GetBits()) / 8;
						for (int y=0; y<left_img->Y(); y++)
						{
							uint8 *left_scan = (*left_img)[y];
							uint8 *right_scan = (*right_img)[y];
							if (memcmp(left_scan, right_scan, bytes))
							{
								diff = true;
								break;
							}
						}
						
						i->SetText(diff ? "Different" : "Same", 2);
					}
					else
					{
						i->SetText("SizeDiff", 2);
					}
				}
				else
				{
					i->SetText("Failed", 2);
				}
				i->Update();
			}
			else break;
						
			it++;
		}

		return 0;
	}
};

struct ImageCompareDlgPriv : public GZoomViewCallback
{
	GCombo *l, *r;
	LList *lst;
	GTabView *tabs;
	GAutoPtr<CompareThread> Thread;

	ImageCompareDlgPriv()
	{
		l = r = NULL;
		tabs = NULL;
	}

	void DrawBackground(GZoomView *View, GSurface *Dst, GdcPt2 Offset, GRect *Where)
	{
		Dst->Colour(LC_WORKSPACE, 24);
		Dst->Rectangle(Where);
	}
	
	void DrawForeground(GZoomView *View, GSurface *Dst, GdcPt2 Offset, GRect *Where)
	{
	}

	void SetStatusText(const char *Msg, int Pane = 0)
	{
	}
};

ImageCompareDlg::ImageCompareDlg(GView *p, const char *OutPath)
{
	d = new ImageCompareDlgPriv();
	SetParent(p);
	GRect r(0, 0, 1200, 900);
	SetPos(r);
	MoveToCenter();
	Name("Image Compare");

	if (!Attach(0))
	{
		delete this;
		return;
	}

	GFile::Path ResFile(__FILE__);
	ResFile--;
	ResFile += "ImageComparison.lr8";
	if (!ResFile.Exists())
	{
		GAutoString r(LgiFindFile("ImageComparison.lr8"));
		if (r)
			ResFile = r.Get();
	}
	LgiAssert(ResFile.GetFull());
	if (ResFile.GetFull())
	{
		AddView(d->tabs = new GTabView(IDC_TAB_VIEW));
		d->tabs->SetPourLargest(true);
		GTabPage *First = d->tabs->Append("Select");
		
		LgiResources *Res = LgiGetResObj(false, ResFile.GetFull());
		LgiAssert(Res);
		if (Res && Res->LoadDialog(IDD_COMPARE, First))
		{
			MoveToCenter();
			
			GButton *b;
			if (GetViewById(IDC_COMPARE, b))
			{
				b->Default(true);
			}
			
			if (GetViewById(IDC_LEFT, d->l) &&
				GetViewById(IDC_RIGHT, d->r) &&
				GetViewById(IDC_LIST, d->lst))
			{			
				GDirectory dir;
				for (bool b = dir.First(OutPath); b; b = dir.Next())
				{
					if (dir.IsDir())
					{
						char p[MAX_PATH];
						if (dir.Path(p, sizeof(p)))
						{
							d->l->Insert(p);
							d->r->Insert(p);
						}
					}
				}
				
				d->l->Value(0);
				d->r->Value(1);
			}
		}
	}

	AttachChildren();
	Visible(true);
}

ImageCompareDlg::~ImageCompareDlg()
{
	DeleteObj(d);
}

int ImageCompareDlg::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case IDC_LIST:
		{
			if (Flags == GNotifyItem_DoubleClick)
			{
				LListItem *s = d->lst->GetSelected();
				if (s)
				{
					char *left = s->GetText(0);
					char *right = s->GetText(1);
					
					#if 0
					char p[MAX_PATH];
					LgiGetSystemPath(LSP_APP_INSTALL, p, sizeof(p));
					LgiMakePath(p, sizeof(p), p, "../../../../i.Mage/trunk/Win32Debug/image.exe");
					if (FileExists(p))
					{
						char args[MAX_PATH];
						sprintf_s(args, sizeof(args), "\"%s\" \"%s\"", left, right);
						LgiExecute(p, args);
					}
					#else
					char *Leaf = strrchr(left, DIR_CHAR);
					int Len = d->tabs->GetTabs();
					GTabPage *t = d->tabs->Append(Leaf ? Leaf + 1 : left);
					if (t)
					{
						t->HasButton(true);
						t->SetId(IDC_TAB_PAGE);
						
						CompareView *cv = new CompareView(d, left, right);
						t->Append(cv);
						d->tabs->Value(Len);
					}
					#endif
				}
			}
			break;
		}
		case IDCANCEL:
		{
			Quit();
			break;
		}
		case IDC_COMPARE:
		{
			GHashTbl<char*, char*> Left;
			GDirectory LDir, RDir;
			char p[MAX_PATH];				
			for (bool b=LDir.First(d->l->Name()); b; b=LDir.Next())
			{
				if (LDir.IsDir())
					continue;
				LDir.Path(p, sizeof(p));
				Left.Add(LDir.GetName(), NewStr(p));
			}
			for (bool b=RDir.First(d->r->Name()); b; b=RDir.Next())
			{
				char *LeftFile = Left.Find(RDir.GetName());
				if (LeftFile)
				{
					RDir.Path(p, sizeof(p));
					LListItem *l = new LListItem;
					l->SetText(LeftFile, 0);
					l->SetText(p, 1);
					l->SetText("Processing...", 2);
					d->lst->Insert(l);
				}
			}
			Left.DeleteArrays();
			d->lst->ResizeColumnsToContent();
			d->Thread.Reset(new CompareThread(d->lst));
			break;
		}
		case IDC_TAB_PAGE:
		{
			if (Flags == GNotifyTabPage_ButtonClick)
			{
				GTabPage *p = dynamic_cast<GTabPage*>(Ctrl);
				LgiAssert(p);
				if (p)
				{
					GTabView *v = p->GetTabControl();
					LgiAssert(v);
					if (v)
					{
						v->Delete(p);
					}
				}
			}
			break;
		}
	}
	
	return 0;
}

