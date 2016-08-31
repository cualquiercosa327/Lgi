#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GAbout.h"
#include "GDocView.h"
#include "GBitmap.h"
#include "GButton.h"
#include "GCss.h"

//////////////////////////////////////////////////////////////////////////////
#define IDM_WEB				100
#define IDM_BMP				101
#define IDM_MESSAGE			102

GAbout::GAbout(	GView *parent,
				const char *AppName,
				const char *Ver,
				const char *Text,
				const char *AboutGraphic,
				const char *Url,
				const char *Email)
{
	SetParent(parent);
	if (!AppName) AppName = "Application";

	char n[256];
	sprintf(n, "About %s", AppName);
	Name(n);

	#ifdef _DEBUG
	const char *Build = "Debug";
	#else
	const char *Build = "Release";
	#endif

	GStringPipe p;
	const char *OsName = LgiGetOsName();
	#if defined(_WIN64)
	OsName = "Win64";
	#elif defined(WIN32)
	OsName = "Win32";
	#endif	
	
	p.Print("%s v%s (%s %s)\n", AppName, Ver, OsName, Build);
	p.Print("Build: %s, %s\n\n", __DATE__, __TIME__);
	if (Url) p.Print("Homepage:\n\t%s\n", Url);
	if (Email) p.Print("Email:\n\t%s\n", Email);
	if (Text) p.Write((char*)Text, strlen(Text));

	GView *Img = 0;
	int x = 10;
	if (AboutGraphic)
	{
		char *FileName = LgiFindFile(AboutGraphic);
		if (FileName)
		{
			Children.Insert(Img = new GBitmap(IDM_BMP, 4, 4, FileName, true));
			DeleteArray(FileName);
			x += 10 + Img->X();
			Img->Sunken(true);
			Img->SetNotify(this);
		}
	}

	GRect rc(x, 10, x+190, 145);
	GView *Ctrl = GViewFactory::Create("GTextView3");
	if (Ctrl)
	{
		AddView(Ctrl);

		GRect r(rc.x1, rc.y1, 1000, 500);

		Ctrl->SetId(IDM_MESSAGE);
		Ctrl->SetPos(r);
		Ctrl->SetNotify(this);
		char *Str = p.NewStr();
		Ctrl->Name(Str);

		GFontType Type;
		Type.GetSystemFont("System");
		GFont *f = Type.Create();
		if (f)
		{
			Ctrl->SetFont(f);
			DeleteObj(f);
		}

		GDocView *View = dynamic_cast<GDocView*>(Ctrl);
		if (View)
		{
			View->SetEnv(this);
			View->GetCss(true)->BackgroundColor(GCss::ColorDef(LC_MED));
			View->SetReadOnly(true);
			View->SetCursor(Str ? strlen(Str) : 0, false);

			int x = 0, y = 0;
			View->GetTextExtent(x, y);
			printf("x=%i, y=%i\n", x, y);
			x += 4;
			y += 4;
			x = max(x, 100);
			if (Img) y = max(y, Img->Y() - 30);
			
			rc.Dimension(x-1, y-1);
			Ctrl->SetPos(rc);
		}
		else LgiAssert(!"You need to fix RTTI for your build of Lgi");

		DeleteArray(Str);
	}

	GRect BtnPos(rc.x2 - 60, rc.y2 + 10, rc.x2, rc.y2 + 30);
	Children.Insert(new GButton(IDOK, BtnPos.x1, BtnPos.y1, BtnPos.X(), BtnPos.Y(), "Ok"));

	rc.ZOff(BtnPos.x2 + 20, BtnPos.y2 + 40);
	SetPos(rc);
	MoveToCenter();

	DoModal();
}

int GAbout::OnNotify(GViewI *Ctrl, int Flags)
{
	if (!Ctrl) return 0;
	
	switch (Ctrl->GetId())
	{
		case IDM_BMP:
		{
			GViewI *Bmp, *Text, *Btn;
			if (GetViewById(IDM_BMP, Bmp) &&
				GetViewById(IDM_MESSAGE, Text) &&
				GetViewById(IDOK, Btn))
			{
				GRect b = Bmp->GetPos();
				GRect t = Text->GetPos();
				GRect c = Btn->GetPos();
				
				t.Offset(b.x2 + 10 - t.x1, 0);
				GRect p = GetPos();
				p.Dimension(t.x2 + 20, max(b.y2, t.y2+30) + 40);
				c.Offset(t.x2 - c.X() - c.x1, p.Y() - 60 - c.y1);

				Text->SetPos(t, true);
				Btn->SetPos(c, true);
				SetPos(p, true);
			}
			break;
		}
		case IDOK:
		{
			EndModal(0);
			break;
		}
	}
	return 0;
}

