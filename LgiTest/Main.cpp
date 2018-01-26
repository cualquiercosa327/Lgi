#include "Lgi.h"
#include "GEdit.h"
#include "GButton.h"
#include "GDisplayString.h"
#include "GTextLabel.h"
#include "GCss.h"

const char *AppName = "Lgi Test App";

enum Ctrls
{
	IDC_EDIT1 = 100,
	IDC_EDIT2,
	IDC_BLT_TEST,
	IDC_TXT,
};

void GStringTest()
{
	GString a("This<TD>is<TD>a<TD>test");
	int idx = a.RFind("<TD>");
	GString f = a(8,10);
	GString end = a(-5, -1);
	
	GString sep(", ");
	GString::Array parts = GString("This is a test").Split(" ");
	GString joined = sep.Join(parts);
	
	GString src("  asdwer   ");
	GString left = src.LStrip();
	GString right = src.RStrip();
	GString both = src.Strip();
	
}

GColourSpace RgbColourSpaces[] =
{
	/*
	CsIndex1,
	CsIndex4,
	CsIndex8,
	CsAlpha8,
	*/
	CsRgb15,
	CsBgr15,
	CsRgb16,
	CsBgr16,
	CsRgb24,
	CsBgr24,
	CsRgbx32,
	CsBgrx32,
	CsXrgb32,
	CsXbgr32,
	CsRgba32,
	CsBgra32,
	CsArgb32,
	CsAbgr32,
	CsBgr48,
	CsRgb48,
	CsBgra64,
	CsRgba64,
	CsAbgr64,
	CsArgb64,
	/*
	CsHls32,
	CsCmyk32,
	*/
	CsNone,
};

class BltTest : public GWindow
{
	struct Test
	{
		GColourSpace Src, Dst;
		GMemDC Result;
		
		void Create()
		{
			if (!Result.Create(16, 16, Dst))
				return;
			for (int y=0; y<Result.Y(); y++)
			{
				Result.Colour(Rgb24(y * 16, 0, 0), 24);
				for (int x=0; x<Result.X(); x++)
				{
					Result.Set(x, y);
				}
			}
			
			GMemDC SrcDC;
			if (!SrcDC.Create(16, 16, Src))
				return;
			
			// Result.Blt(0, 0, &SrcDC);
		}
	};
	
	GArray<Test*> a;

public:
	BltTest()
	{
		Name("BltTest");
		GRect r(0, 0, 1200, 1000);
		SetPos(r);
		MoveToCenter();
		
		if (Attach(0))
		{
			Visible(true);
			
			for (int Si=0; RgbColourSpaces[Si]; Si++)
			{
				for (int Di=0; RgbColourSpaces[Di]; Di++)
				{
					Test *t = new Test;
					t->Src = RgbColourSpaces[Si];
					t->Dst = RgbColourSpaces[Di];
					t->Create();
					a.Add(t);
				}
			}
		}
	}
	
	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_WORKSPACE, 24);
		pDC->Rectangle();
		
		int x = 10;
		int y = 10;
		for (unsigned i=0; i<a.Length(); i++)
		{
			char s[256];
			Test *t = a[i];
			sprintf_s(s, sizeof(s), "%s->%s", GColourSpaceToString(t->Src), GColourSpaceToString(t->Dst));
			GDisplayString ds(SysFont, s);
			ds.Draw(pDC, x, y);
			y += ds.Y();
			if (t->Result.Y())
			{
				pDC->Blt(x, y, &t->Result);
				y += t->Result.Y();
			}
			y += 10;
			if (Y() - y < 100)
			{
				y = 10;
				x += 150;
			}
		}
	}
};

class App : public GWindow
{
	GEdit *e;
	GEdit *e2;
	GText *Txt;

public:
	App()
	{
		e = 0;
		e2 = 0;
		Txt = 0;

		GRect r(0, 0, 1000, 800);
		SetPos(r);
		Name(AppName);
		MoveToCenter();
		SetQuitOnClose(true);
		
		if (Attach(0))
		{
			#if 1

			AddView(Txt = new GText(IDC_TXT, 0, 0, -1, -1, "This is a test string. &For like\ntesting and stuff. It has multiple\nlines to test wrapping."));
			Txt->SetWrap(true);
			//Txt->GetCss(true)->Color(GCss::ColorDef(GColour::Red));
			// Txt->GetCss(true)->FontWeight(GCss::FontWeightBold);
			// Txt->GetCss(true)->FontStyle(GCss::FontStyleItalic);
			Txt->GetCss(true)->FontSize(GCss::Len("22pt"));
			Txt->OnStyleChange();

			#else

			AddView(e = new GEdit(IDC_EDIT1, 10, 10, 200, 22));
			AddView(e2 = new GEdit(IDC_EDIT1, 10, 50, 200, 22));
			AddView(new GButton(IDC_BLT_TEST, 10, 200, -1, -1, "Blt Test"));
			// e->Focus(true);
			e->Password(true);
			e->SetEmptyText("(this is a test)");

			#endif
			
			AttachChildren();
			Visible(true);
		}
	}

	void OnPosChange()
	{
		if (Txt)
		{
			GRect c = GetClient();
			c.Size(10, 10);
			Txt->SetPos(c);
		}
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(LC_LOW, 24);
		pDC->Rectangle();
	}
	
	int OnNotify(GViewI *Ctrl, int Flags)
	{
		switch (Ctrl->GetId())
		{
			case IDC_BLT_TEST:
				new BltTest();
				break;
		}
		
		return 0;
	}
};

int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, "Lgi Test");
	if (a.IsOk())
	{
		GStringTest();
		a.AppWnd = new App;
		a.Run();
	}
	return 0;
}
