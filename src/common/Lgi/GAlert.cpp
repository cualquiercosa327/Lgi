// Lgi.cpp
#include "Lgi.h"
#include "GTextLabel.h"
#include "GButton.h"
#include "GDisplayString.h"
#include "GTableLayout.h"

//////////////////////////////////////////////////////////////////////////////
#define CMD_BASE		100
#ifdef LGI_SDL
#define BTN_SCALE		2.0f
#else
#define BTN_SCALE		1.0f
#endif

GAlert::GAlert(	GViewI *parent,
				const char *Title,
				const char *Text,
				const char *Btn1,
				const char *Btn2,
				const char *Btn3)
{
	GArray<const char*> Names;
	if (Btn1) Names.Add(Btn1);
	if (Btn2) Names.Add(Btn2);
	if (Btn3) Names.Add(Btn3);

	// Setup dialog
	SetParent(parent);
	Name((char*)Title);

	GTableLayout *Tbl = new GTableLayout(100);
	AddView(Tbl);

	GLayoutCell *c = Tbl->GetCell(0, 0, true, Names.Length());
	c->Add(new GTextLabel(-1, 8, 8, -1, -1, Text));
	c->PaddingBottom(GCss::Len("6px"));

	for (unsigned i=0; i<Names.Length(); i++)
	{
		c = Tbl->GetCell(i, 1, true);
		c->TextAlign(GCss::Len(GCss::AlignCenter));
		c->Add(new GButton(CMD_BASE + i,
							0, 0, -1, -1,
							Names[i]));
	}
		
	GRect r(0, 0, 1000, 1000);
	Tbl->SetPos(r);
	Tbl->GetCss(true)->Padding(GCss::Len("6px"));
	r = Tbl->GetUsedArea();
	int x = LgiApp->GetMetric(LGI_MET_DECOR_X) + 12;
	int y = LgiApp->GetMetric(LGI_MET_DECOR_Y) +
			LgiApp->GetMetric(LGI_MET_DECOR_CAPTION) +
			12;

	r.x2 += x;
	r.y2 += y;

	SetPos(r);
	if (parent)
		MoveSameScreen(parent);
	else
		MoveToCenter();
}

void GAlert::SetAppModal()
{
    #if WINNATIVE
    SetExStyle(GetExStyle() | WS_EX_TOPMOST);
	#elif defined(MAC) && !defined(LGI_SDL)
	if (Handle())
	{
		OSStatus e = HIWindowChangeClass(WindowHandle(), kMovableModalWindowClass);
		if (e)
			printf("%s:%i - Error: HIWindowChangeClass=%i\n", _FL, (int)e);
	}
    #elif !defined(_MSC_VER)
    #warning "Impl me."
    #endif
}

int GAlert::OnNotify(GViewI *Ctrl, int Flags)
{
	switch (Ctrl->GetId())
	{
		case CMD_BASE+0:
		case CMD_BASE+1:
		case CMD_BASE+2:
		{
			if (Flags != GNotifyTableLayout_LayoutChanged)
			{
				EndModal(Ctrl->GetId() - CMD_BASE + 1);
			}
			break;
		}
	}

	return 0;
}
