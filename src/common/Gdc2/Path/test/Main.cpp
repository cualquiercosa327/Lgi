#include "Lgi.h"
#include "resdefs.h"
#include "GPath.h"

//////////////////////////////////////////////////////////////////
const char *AppName = "GPathTest";

class App : public GWindow
{
	GPath p;
	GMemDC img;

public:
    App()
    {
        Name(AppName);
        GRect r(0, 0, 1000, 800);
        SetPos(r);
        MoveToCenter();
        SetQuitOnClose(true);

        if (Attach(0))
        {
			Test();
            Visible(true);
        }
    }

	void Test()
	{
		double r = 7.0;
		int x = (int)(r * 2.0);
		img.Create(x, x, System32BitColourSpace);
		img.Colour(0, 32);
		img.Rectangle();

		p.Circle(r, r, r);
		p.SetFillRule(FILLRULE_ODDEVEN);
		GSolidBrush s(GColour::White);
		p.Fill(&img, s);
		p.Empty();

		p.Circle(r, r, r);
		p.Circle(r, r, r - 1.0);
		p.SetFillRule(FILLRULE_ODDEVEN);
		GSolidBrush s2(GColour(0xcb, 0xcb, 0xcb));
		p.Fill(&img, s2);

		img.ConvertPreMulAlpha(true);
	}

	void OnPaint(GSurface *pDC)
	{
		pDC->Colour(GColour(LC_MED, 24));
		pDC->Rectangle();

		pDC->Op(GDC_ALPHA);
		pDC->Blt(0, 0, &img);
	}
};

//////////////////////////////////////////////////////////////////
int LgiMain(OsAppArguments &AppArgs)
{
	GApp a(AppArgs, AppName);
	if (a.IsOk())
	{
		a.AppWnd = new App;
		a.Run();
	}

	return 0;
}

