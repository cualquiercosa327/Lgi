#include <stdlib.h>
#include <stdio.h>

#include "Lgi.h"
#include "GAbout.h"
#include "GDocView.h"
#include "GBitmap.h"
#include "GButton.h"
#include "GCss.h"
#include "GTableLayout.h"

//////////////////////////////////////////////////////////////////////////////
enum Ctrls
{
	IDC_WEB = 100,
	IDC_TABLE,
	IDC_BMP,
	IDC_MESSAGE,
};

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

	GString n;
	n.Printf("About %s", AppName);
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

	GColour cBack(LC_MED, 24);
	GTableLayout *Tbl = new GTableLayout(IDC_TABLE);
	AddView(Tbl);
	Tbl->GetCss(true)->Padding("0.5em");
	int x = 0;

	if (AboutGraphic)
	{
		GAutoString FileName(LgiFindFile(AboutGraphic));
		if (FileName)
		{
			auto c = Tbl->GetCell(x++, 0, true);
			auto Img = new GBitmap(IDC_BMP, 0, 0, FileName, true);
			c->Add(Img);
			Img->GetCss(true)->BackgroundColor(cBack);
		}
	}

	GView *Ctrl = GViewFactory::Create("GTextView3");
	if (Ctrl)
	{
		auto c = Tbl->GetCell(x++, 0, true);
		c->Add(Ctrl);

		Ctrl->SetId(IDC_MESSAGE);
		Ctrl->Name(p.NewGStr());
		Ctrl->GetCss(true)->BackgroundColor(cBack);
		Ctrl->SetFont(SysFont);
	}

	auto c = Tbl->GetCell(0, 1, true, x);
	c->TextAlign(GCss::AlignRight);
	c->Add(new GButton(IDOK, 0, 0, -1, -1, "Ok"));

	GRect r(0, 0, 400, 260);
	SetPos(r);
	MoveSameScreen(parent);

	DoModal();
}

int GAbout::OnNotify(GViewI *Ctrl, int Flags)
{
	if (!Ctrl) return 0;
	
	switch (Ctrl->GetId())
	{
		case IDC_BMP:
		{
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

