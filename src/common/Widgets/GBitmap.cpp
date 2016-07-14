#include "Lgi.h"
#include "GBitmap.h"
#include "GTableLayout.h"
#include "LgiRes.h"

//////////////////////////////////////////////////////////////////////////////////
#ifdef _MT
class GBitmapThread : public GThread
{
	GBitmap *Bmp;
	char *File;
	GThread **Owner;

public:
	GBitmapThread(GBitmap *bmp, char *file, GThread **owner) : GThread("GBitmapThread")
	{
		Bmp = bmp;
		File = NewStr(file);
		Owner = owner;
		if (Owner)
		{
			*Owner = this;
		}
		Run();
	}

	~GBitmapThread()
	{
		if (Owner)
		{
			*Owner = 0;
		}
		DeleteArray(File);
	}

	int Main()
	{
		if (Bmp)
		{
			GSurface *pDC = LoadDC(File);
			if (pDC)
			{
				Bmp->SetDC(pDC);

				while (!Bmp->Handle())
				{
					LgiSleep(10);
				}

				GRect r = Bmp->GetPos();
				r.Dimension(pDC->X()+4, pDC->Y()+4);
				Bmp->SetPos(r, true);
                Bmp->SendNotify();

				DeleteObj(pDC);
			}
		}

		return 0;
	}
};
#endif

GBitmap::GBitmap(int id, int x, int y, char *FileName, bool Async)
	: ResObject(Res_Bitmap)
{
	pDC = 0;
	pThread = 0;

	#if WINNATIVE
	SetStyle(WS_CHILD | WS_VISIBLE);
	#endif

	SetId(id);
	GRect r(0, 0, 4, 4);
	r.Offset(x, y);

	if (ValidStr(FileName))
	{
		#ifdef _MT
		if (!Async)
		{
		#endif
			pDC = LoadDC(FileName);
			if (pDC)
			{
				r.Dimension(pDC->X()+4, pDC->Y()+4);
			}
		#ifdef _MT
		}
		else
		{
			new GBitmapThread(this, FileName, &pThread);
		}
		#endif

	}

	SetPos(r);
	LgiResources::StyleElement(this);
}

GBitmap::~GBitmap()
{
	#ifdef _MT
	if (pThread)
	{
		pThread->Terminate();
		DeleteObj(pThread);
	}
	#endif
	DeleteObj(pDC);
}

void GBitmap::SetDC(GSurface *pNewDC)
{
	DeleteObj(pDC);
	if (pNewDC)
	{
		pDC = new GMemDC;
		if (pDC && pDC->Create(pNewDC->X(), pNewDC->Y(), GdcD->GetColourSpace()))
		{
			pDC->Colour(LC_WORKSPACE, 24);
			pDC->Rectangle();

			pDC->Op(GDC_ALPHA);
			pDC->Blt(0, 0, pNewDC);

			GRect r = GetPos();
			r.Dimension(pDC->X() + 4, pDC->Y() + 4);
			SetPos(r);
			Invalidate();

			for (GViewI *p = GetParent(); p; p = p->GetParent())
			{
				GTableLayout *Tl = dynamic_cast<GTableLayout*>(p);
				if (Tl)
				{
					Tl->InvalidateLayout();
					break;
				}
			}
			
			SendNotify(GNotifyTableLayout_LayoutChanged);
		}
	}
}

GSurface *GBitmap::GetSurface()
{
	return pDC;
}

GMessage::Result GBitmap::OnEvent(GMessage *Msg)
{
	return GView::OnEvent(Msg);
}

void GBitmap::OnPaint(GSurface *pScreen)
{
	GRect a = GetClient();
	if (pDC)
	{
		pScreen->Blt(0, 0, pDC);

		pScreen->Colour(LC_MED, 24);
		if (pDC->X() < a.X())
		{
			pScreen->Rectangle(pDC->X(), 0, a.x2, pDC->Y());
		}
		if (pDC->Y() < a.Y())
		{
			pScreen->Rectangle(0, pDC->Y(), a.x2, a.y2);
		}
	}
	else
	{
		pScreen->Colour(LC_MED, 24);
		pScreen->Rectangle(&a);
	}
}

void GBitmap::OnMouseClick(GMouse &m)
{
	if (!m.Down() && GetParent())
	{
		GDialog *Dlg = dynamic_cast<GDialog*>(GetParent());
		if (Dlg) Dlg->OnNotify(this, 0);
	}
}
